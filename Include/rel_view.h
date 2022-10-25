#pragma once
//////////////////////////////////////////////////////////////////////
// this class represents a view of a relation
// it does not derive from a container, it holds the contianer as a member
// it holds references to constant relations
// the values of the relations cannot be changed by the view, the offer only a "view" to the relations
// a change of the relations that the view represents makes the view invalidated and would need to be reconstructed
// 

//TODO: adapt most of the relation functions, esp query functions to work on views 
#include "relation.h"
#include "nl_types.h"
#include <functional>
#include <vector>
#include <utility>
#include <regex>
namespace nl {

	template<typename comp_func = std::less<>>
	struct compare_variant
	{
		template<typename T>
		bool operator()(const std::reference_wrapper<const T>& a, const std::reference_wrapper<const T>& b)
		{
			return comp_func{}(a.get(), b.get());
		}

		template<typename T>
		bool operator()(const std::reference_wrapper<T>& a, const std::reference_wrapper<T>& b)
		{
			return comp_func{}(a.get(), b.get());
		}

		bool operator()(...) {
			return false;
		}

	};

	
	template<typename... Args>
	class rel_view {
	public:
		using container_t = std::vector<std::tuple<std::reference_wrapper<const Args>...>>;
		using row_t = typename container_t::value_type;
		using iterator = typename container_t::iterator;
		using const_iterator = typename container_t::const_iterator;
		using value_type = typename container_t::value_type;
		using variant_t = typename nl::variant<row_t>::type;
		static std::array<std::string_view, sizeof...(Args)> col_type_names;

		template<size_t I>
		using elem_t = typename std::tuple_element_t<I, row_t>::type;

		static constexpr bool is_view = std::true_type::value;
		static constexpr size_t column_count = sizeof...(Args);

		rel_view()
		{
			
		}

		rel_view(size_t size)
		{
			m_data.reserve(size);
		}

		void reserve(size_t size) { m_data.reserve(size); }
		void resize(size_t size) { m_data.resize(size); }

		//cannot copy or assign a view
		rel_view(const rel_view& view) = delete; 
		rel_view& operator=(const rel_view& view) = delete;

		rel_view(rel_view&& view)
		{
			m_data = std::move(view.m_data);
		}
		
		rel_view& operator=(rel_view&& view)
		{
			m_data = std::move(view.m_data);
			return *(this);
		}

		//destroctor is not virtual 
		//a view is not to be derived from 

		~rel_view() {}


		//this is a static function that creates a view from a range and a set of columns
		template<typename relation, size_t... I>
		static auto make_view(typename relation::const_iterator begin, 
				typename relation::const_iterator end, std::index_sequence<I...> req) -> rel_view<Args...>
		{
			rel_view<Args...> view(std::distance(begin, end));
			for (auto iter = begin; iter != end; iter++) {
				view.m_data.emplace_back(std::ref(nl::row_value<I>(*iter))...);
			}
			return view;
		}

		//this is a static function that creates a view from a range and a set of columns
		template<typename relation, size_t... I>
		static auto make_view(const relation& rel, std::vector<size_t> idx,
			std::index_sequence<I...> req) -> rel_view<Args...>
		{
			rel_view<Args...> view(idx.size());
			for (auto i : idx) {
				view.m_data.emplace_back(std::ref(nl::row_value<I>(rel[i]))...);
			}
			return view;
		}

		void  emplace_back(const Args&... value) {
			m_data.emplace_back(value...);
		}

		template<size_t I>
		inline auto like_index(const std::regex&& expression)
		{
			if constexpr (!std::is_convertible_v<elem_t<I>, std::string>) {
				return std::vector<size_t>{};
			}
			std::vector<size_t> ret;
			ret.reserve(m_data.size());
			for (size_t i = 0; i < m_data.size(); i++) {
				if (std::regex_match(std::get<I>(m_data.at(i)).get(), expression)) {
					ret.emplace_back(i);
				}
			}
			ret.shrink_to_fit();
			return ret;
		}

		inline std::string get_as_string(size_t row, size_t column) const noexcept
		{
			return nl::detail::loop<column_count - 1>::get_as_string_ref(m_data.at(row), column);
		}


		constexpr size_t get_column_count() const { return sizeof...(Args); }

		template<size_t col>
		inline const typename std::tuple_element_t<col, row_t>& get(size_t row) const
		{
			return std::get<col>(m_data.at(row));
		}

		//error code needed , might throw an exception on type mismatch with varient
		inline void get_in_variant(size_t row, size_t col, variant_t& variant) const 
		{
			nl::detail::loop<column_count - 1>::get_in(m_data.at(row), variant, col);

		}

		
		//using only vector relation, other types of relations are simply terrible
		template<typename relation, typename indices = std::make_index_sequence<relation::column_count>>
		void from_relation(const relation& rel) {
			from_relation_impl(rel, indices{});
		}
		
		//get column type as string
		inline const char* get_column_type_name(size_t index) const
		{
			assert(index < column_count && "Index is invalid");
			return col_type_names[index].data();
		}

		//takes a range
		template<typename relation, typename indices = std::make_index_sequence<relation::column_count>>
		void from_relation(typename relation::const_iterator begin, typename relation::const_iterator end){
			from_relation_impl<relation>(begin, end, indices{});
		}

		template<typename relation, typename indices = std::make_index_sequence<relation::column_count>>
		void from_relation(const relation& rel, std::vector<size_t> idx){
			from_relation_impl<relation>(rel, idx, indices{});
		}


		template<size_t I>
		inline std::vector<std::reference_wrapper<elem_t<I>>> isolate_column()
		{
			std::vector<std::reference_wrapper<elem_t<I>>> isolated_column;
			isolated_column.reserve(container_t::size());
			for (auto& item : m_data)
			{
				isolated_column.emplace_back(std::get<I>(item));
			}
			return isolated_column;
		}


		inline const_iterator begin() const {
			return m_data.cbegin();
		}

		
		inline const_iterator end() const {
			return m_data.cend();
		}

		inline size_t size() const {
			return m_data.size();
		}

		const value_type operator[](size_t i) const {
			assert(i < size());
			return m_data.at(i);
		}

		const container_t& get_view_data() const {
			return m_data;
		}

	private:
		template<typename... As, size_t... I>
		void from_relation_impl(const nl::vector_relation<As...>& relation, std::index_sequence<I...> seq)
		{
			//holds a view of the whole relation
			static_assert(sizeof...(Args) == sizeof...(As), "The types of the view must match the type of the relation");
			static_assert(std::is_same_v<std::tuple<Args...>, std::remove_const_t<typename decltype(relation)::row_t>>, "The relation type order must match the order of the view");
			m_data.clear();
			for (const auto& v : relation) {
				m_data.emplace_back(std::ref(nl::row_value<I>(v))...);
			}
		}

		template<typename relation, size_t... I>
		void from_relation_impl(typename relation::const_iterator begin, typename relation::const_iterator end,
			std::index_sequence<I...> seq)
		{
			m_data.clear();
			for (auto iter = begin; iter != end; iter++){
				m_data.emplace_back(std::ref(nl::row_value<I>(*iter))...);
			}

		}

		template<typename relation, size_t... I>
		void from_relation_impl(const relation& rel, std::vector<size_t> idx, std::index_sequence<I...> seq)
		{
			m_data.clear();
			for (auto i : idx) {
				m_data.emplace_back(std::ref(nl::row_value<I>(rel[i]))...);
			}

		}

		container_t m_data;

	};

	//create memory for the static, 
	//tbh i dont still understand memory allocation for template statics
	// this file would be included in multiple translation units 
	//would each translastion unit create a file static varible or would there all reference the first..
	//translation unit that creates the static?? questions o questions
	template<typename... args>
	std::array<std::string_view, sizeof...(args)> rel_view<args...>::col_type_names {(nl::get_type_name<args>())... };

	//good?? or terrible code, 
	namespace detail {
		template<typename... Args>
		std::uint64_t func2(const rel_view<Args...>* t);
		std::uint8_t func2(...);

		template<typename T>
		struct is_rel_view :
			public std::conditional_t<(sizeof(func2((T*)nullptr)) == sizeof(std::uint64_t)), std::true_type, std::false_type>
		{};
		template<typename T>
		constexpr bool is_rel_view_v = is_rel_view<T>::value;
	}
	
}