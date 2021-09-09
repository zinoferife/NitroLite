#pragma once
#include "../pch.h"
#include "tuple_loop.h"
#include "relation_buffer.h"
	/////////////////////////////////////////////////////////////
	// relation represents a frame of data from the database
	// reads and writes to the database a done via relations
	// operations can be done via relational functions or functions in classes derived from relations
	// relations can be serialized into buffers and maybe transfered across a network
	// this froms the base object of all _TBL objects in the application 
	// over engineering by ewomagram !!!!!
	//exceptions are thrown by the std algorithms, exceptions are not handled by the class, i think it would better to catch the excpetions at application level not libary level, have more handling capabilites plus can inform the user

#include <cstdint>
#define BEGIN_COL_NAME(name)static constexpr char table_name[] = name; static constexpr const char* col_names[] =  {
#define COL_NAME(name) name,
#define END_COL_NAME() "null"};
#define IMPLEMENT_GET_COL_NAME() \
		template<size_t i> \
		constexpr const char* get_col_name() { \
		static_assert(i >= 0 && i < sizeof(col_names), "invalid index in get_col_name()"); return col_names[i]; }
#define DEFINE_UNIQUE_ID() enum {unique_id = 0 };

namespace nl {
	template<class container> class relation;

	class base_relation
	{
	public:
		virtual ~base_relation() {}
	};


	template<template<class, class>typename container, 
		typename... val>
	class relation<container<std::tuple<val...>, alloc_t<std::tuple<val...> > > > : 
		public container<std::tuple<val...>, alloc_t<std::tuple<val...>>>, public base_relation
	{
	public:
		enum rel_constants
		{
			unique_id = 0,
			col_id_start
		};

		using container_t = container<std::tuple<val...>, alloc_t<std::tuple<val...>>>;
		using tuple_t = typename container_t::value_type;
		using row_t = typename container_t::value_type;
		using container_tag = linear_relation_tag;
		using relation_t = relation;

		template<size_t I>
		using elem_t = std::tuple_element_t<I, tuple_t>;
		constexpr static size_t column_count = std::tuple_size_v<tuple_t>;
		static size_t row_id;

		relation() = default;
		explicit relation(size_t size) : container_t{ size } {}
		relation(const relation& val) : container_t(val) {}
		relation(const relation&& val) noexcept : container_t(std::move(val)) {};
		relation& operator=(const relation& rhs)
		{
			container_t::operator=(rhs);
			return (*this);
		}
		relation& operator=(const relation&& rhs) noexcept
		{
			container_t::operator=(std::move(rhs));
			return (*this);
		}

		virtual ~relation() {}



		auto insert(tuple_t& tuple)
		{
			return container_t::push_back(tuple);
		}

		template<size_t I>
		auto average()
		{
			double sum = 0.0;
			for (auto& tuple : *this)
			{
				sum += std::get<I>(tuple);
			}
			return (sum / static_cast<double>(container_t::size()));
		}

		inline void add(const val&... args)
		{
			static_assert(std::tuple_size_v<tuple_t> == sizeof...(args), "Incomplete argument in add");
			container_t::emplace_back(args...);
		}

		void add(const row_t& row)
		{
			container_t::emplace_back(std::move(row));
		}
		template<size_t col>
		inline const typename std::tuple_element_t<col, tuple_t>& get(size_t row) const
		{
			return std::get<col>(tuple_at(row));
		}

		template<size_t col>
		inline void set(size_t row, const typename std::tuple_element_t<col, tuple_t>& value)
		{
			std::get<col>(tuple_at(row)) = std::forward<decltype(value)>(value);
		}

		template<size_t...I>
		inline void set(size_t row, std::index_sequence<I...>, const typename std::tuple_element_t<I, tuple_t>& ... args)
		{
			std::forward_as_tuple(std::get<I>(tuple_at(row))...) = std::tie(args...);
		}

		template<size_t...I>
		inline void update_row(size_t row, const elem_t<I>& ... args)
		{
			((std::get<I>(container_t::operator[](row)) = args),...);
		}


		template<size_t... I>
		inline std::tuple<std::tuple_element_t<I, tuple_t>...>  get(size_t row, std::index_sequence<I...>) const
		{
			return std::make_tuple(std::get<I>(tuple_at(row))...);
		}

		template<size_t col>
		bool find_on(const typename std::tuple_element_t<col, tuple_t>& value, tuple_t& found_row) const
		{
			auto it = std::find_if(container_t::begin(), container_t::end(), [&](const tuple_t& tuple) {
				return(value == std::get<col>(tuple));
				});
			if (it == container_t::end())
			{
				return false;
			}
			found_row = *it;
			return true;
		}

		template<size_t I>
		bool binary_find(const typename std::tuple_element_t<I, tuple_t>& value, tuple_t& found_row)
		{
			detail::comp_tuple_with_value<I, tuple_t, std::tuple_element_t<I, tuple_t>> comp{};
			auto it = std::lower_bound(container_t::begin(), container_t::end(), value, comp);
			if (it == container_t::end()){
				return false;
			}
			found_row = *it;
			return true;
		}

		template<size_t I>
		std::vector<relation_t> group_by(){
			order_by<I>();
			std::vector<relation_t> ret_vec;
			for (auto iter = container_t::begin(); iter != container_t::end();){
				detail::comp_tuple_with_value<I, tuple_t, std::tuple_element_t<I, tuple_t>> comp{};
				auto upper_iter = std::upper_bound(iter, container_t::end(), std::get<I>(*iter), comp);

				relation_t new_relation(std::distance(iter, upper_iter));
				std::copy(iter, upper_iter, new_relation.begin());

				iter = upper_iter;
				ret_vec.push_back(std::move(new_relation));
			}
			return ret_vec;
		}

		template<size_t I>
		auto map_group_by()
		{
			std::unordered_map<elem_t<I>, relation_t> group_map;
			for (auto iter = container_t::begin(); iter != container_t::end(); iter++){
				group_map[std::get<I>(*iter)].push_back(*iter);
			}
			return std::move(group_map);
		}

		//O(M+n) complexity in time
		//O(n) complexity in memory
		//assumes that both col I1 and I2 have unique elements and are both the same type or is 

		template<size_t I1, size_t I2, typename rel_t >
		auto join_on(rel_t& rel)
		{
			static_assert(std::is_same_v<typename std::tuple_element_t<I1, tuple_t>, typename std::tuple_element_t<I2, typename rel_t::tuple_t>> 
						 || std::is_convertible_v<elem_t<I1>, typename rel_t::template elem_t<I2>>, "Cannot join on column that are not same type or the types are not convertible");
			using type = typename detail::join_tuple_type<tuple_t, typename rel_t::tuple_t>::type;
			relation<container<type, alloc_t<type>>> new_relation;
			std::unordered_map<std::tuple_element_t<I2, typename rel_t::tuple_t>, typename rel_t::iterator> find_map;
			for (auto rel_iter = rel.begin(); rel_iter != rel.end(); rel_iter++)	{
				find_map.insert(std::make_pair(std::get<I2>(*rel_iter), rel_iter));
			}
			for (auto this_iter = container_t::begin(); this_iter != container_t::end(); this_iter++){
				auto find_iter = find_map.find(std::get<I1>(*this_iter));
				if (find_iter != find_map.end()){
					new_relation.push_back(std::forward<type>(std::tuple_cat(*this_iter, *(find_iter->second))));;
				}
			}
			return std::move(new_relation);
		}

		//removes all consequtive adjacent dublicates, sort first if you want to remove all dublicate 
		//O(N) for the whole relation
		template<size_t I>
		void unique(){
			auto start = container_t::begin();
			auto result = start;
			while (++start != container_t::end()){
				if (!(std::get<I>(*result) == std::get<I>(*start)) && ((++result) != (start))){
					*result = std::move(*start);
				}
			}
			auto end = ++result;
			container_t::erase(end, container_t::end());
		}

		template<size_t...I>
		auto select(){
			using T = std::tuple<std::tuple_element_t<I, tuple_t>...>;
			relation<container<T, alloc_t<T>>> new_relation;
			for (auto iter = container_t::begin(); iter != container_t::end(); iter++)
			{
				new_relation.push_back(std::forward_as_tuple(std::get<I>(*iter)...));
			}
			return std::move(new_relation);
		}


		template<size_t...I>
		auto select(std::index_sequence<I...>)
		{
			using T = std::tuple<std::tuple_element_t<I, tuple_t>...>;
			relation<container<T, alloc_t<T>>> new_relation;
			for (auto iter = container_t::begin(); iter != container_t::end(); iter++)
			{
				new_relation.push_back(std::forward_as_tuple(std::get<I>(*iter)...));
			}
			return std::move(new_relation);
		}

		

		template<size_t I, typename order_by = order_asc<typename std::tuple_element_t<I, tuple_t>>>
		void order_by() {
			detail::sort(*this, [&](tuple_t& l, tuple_t& r) {
					return (order_by{}(std::get<I>(l), std::get<I>(r)));
			});
		}

		void unpack_row_in(size_t row, val& ...args){
			if (container_t::empty()) return;
			std::tie(args...) = tuple_at(row);
		}

		template<size_t I>
		void add_in_order(const val&... args)
		{
			static_assert(std::tuple_size_v<tuple_t> == sizeof...(args), "Incomplete argument in add_in_order");
			tuple_t tuple = std::tie(args...);
			detail::comp_tuple_with_value<I, tuple_t, std::tuple_element_t<I, tuple_t>> comp{};
			auto iter = std::lower_bound(container_t::begin(), container_t::end(),std::get<I>(tuple), comp);
			if (iter != container_t::end()){
				container_t::insert(iter, std::move(tuple));
			}
			else {
				//greater than the last element
				container_t::push_back(std::move(tuple));
			}
		}

		template<size_t I>
		void add_in_order(const row_t& row)
		{
			detail::comp_tuple_with_value<I, tuple_t, std::tuple_element_t<I, tuple_t>> comp{};
			auto iter = std::lower_bound(container_t::begin(), container_t::end(), std::get<I>(row), comp);
			if (iter != container_t::end()) {
				container_t::insert(iter, row);
			}
			else {
				//greater than the last element
				container_t::push_back(row);
			}
		}

		static inline auto make_row(const val&... values){
			return std::move(std::make_tuple(values...));
		}

		static inline void set_default_row(const val& ... values)
		{
			default_row = std::make_tuple(values...);
		}
		template<size_t I>
		void add_default_in_order(){
			detail::comp_tuple_with_value<I, tuple_t, std::tuple_element_t<I, tuple_t>> comp{};
			auto iter = std::lower_bound(container_t::begin(), container_t::end(), std::get<I>(default_row), comp);
			if (iter != container_t::end()) {
				container_t::insert(iter, default_row);
			}
			else {
				//greater than the last element
				container_t::push_back(default_row);
			}
		}

		void add_default(){
			container_t::push_back(default_row);
		}


		//assumes that both are it is sorted
		void merge(relation_t& rel)
		{
				relation_t ret(container_t::size() + rel.size());
				std::merge(container_t::begin(), container_t::end(), rel.begin(), rel.end(), ret.begin());
				(*this) = std::move(ret);
		}

		template<size_t I>
		void merge_on(relation_t& rel)
		{
				relation_t ret(container_t::size() + rel.size());
				std::merge(container_t::begin(), container_t::end(), rel.begin(), rel.end(), ret.begin(), [&](const tuple_t& val1, const tuple_t& val2) {
						return (std::get<I>(val1) < std::get<I>(val2));
					});
				(*this) = std::move(ret);
		}

		bool is_sub_relation(relation_t& rel)
		{
			return std::includes(container_t::begin(), container_t::end(), rel.begin(), rel.end());
		}
		
		template<size_t I>
		auto min_max_on()
		{
			return std::minmax_element(container_t::begin(), container_t::end(), [&](tuple_t& lhs, tuple_t& rhs) {
				return std::get<I>(lhs) < std::get<I>(rhs);
				});
		}

		template<size_t I, typename Pred>
		void remove_on_if(Pred pred)
		{
			auto it = std::remove_if(container_t::begin(), container_t::end(), [&](const tuple_t& tuple)-> bool{
				return pred(std::get<I>(tuple));
			});
			container_t::erase(it, container_t::end());
		}

		template<size_t I, typename Pred>
		auto where(Pred pred)
		{
			relation_t ret_rel;
			std::copy_if(container_t::begin(), container_t::end(), std::back_inserter<relation_t>(ret_rel), [&](const tuple_t& value) {
				return pred(std::get<I>(value));
				});
		
			return std::move(ret_rel);
		}

		//applies that function to every value in the column I
		template<size_t I, typename Func>
		void transfrom(Func func)
		{
			std::transform(container_t::begin(), container_t::end(), container_t::begin(), [&](tuple_t& tuple) -> tuple_t {
				std::get<I>(tuple) = func(std::get<I>(tuple));
				return tuple;
			});
		}

		template<typename Func>
		void transfrom_row(Func func)
		{
			std::transform(container_t::begin(), container_t::end(), container_t::begin(), [&](tuple_t& tuple) -> tuple_t {
				return std::move(func(tuple));
			});
		}




	//parallel algorithms
	public:
		template<size_t I, typename Func, typename execution_policy = std::execution::parallel_policy>
		void transfrom_par(Func func, execution_policy policy = std::execution::par)
		{
			std::transform(policy, container_t::begin(), container_t::end(), container_t::begin(), [&](tuple_t& tuple) -> tuple_t {
				get<I>(tuple) = func(get<I>(tuple));
				return tuple;
			});
		}

		template<typename Func, typename execution_policy = std::execution::parallel_policy >
		void transfrom_row_par(Func func, execution_policy policy = std::execution::par)
		{
			std::transform(policy,container_t::begin(), container_t::end(), container_t::begin(), [&](tuple_t& tuple) -> tuple_t {
				return std::move(func(tuple));
			});
		}

		template<size_t I, typename Pred, typename execution_policy = std::execution::parallel_policy>
		auto where_par(Pred pred, execution_policy policy = std::execution::par)
		{
			relation_t ret_rel;
			std::copy_if(policy, container_t::begin(), container_t::end(), std::back_inserter<relation_t>(ret_rel), [&](const tuple_t& value) {
				return pred(std::get<I>(value));
				});

			return std::move(ret_rel);
		}

		template<size_t I, typename Pred, typename execution_policy = std::execution::parallel_policy>
		void remove_on_if_par(Pred pred, execution_policy policy = std::execution::par)
		{
			auto it = std::remove_if(policy, container_t::begin(), container_t::end(), [&](const tuple_t& tuple)-> bool {
				return pred(std::get<I>(tuple));
				});
			container_t::erase(it, container_t::end());
		}

		template<size_t I, typename execution_policy = std::execution::parallel_policy>
		auto min_max_on_par(execution_policy policy = std::execution::par)
		{
			return std::minmax_element(policy, container_t::begin(), container_t::end(), [&](tuple_t& lhs, tuple_t& rhs) {
				return std::get<I>(lhs) < std::get<I>(rhs);
				});
		}

		template<typename execution_policy = std::execution::parallel_policy>
		bool is_sub_relation_par(relation_t & rel, execution_policy policy = std::execution::par)
		{
			return std::includes(policy, container_t::begin(), container_t::end(), rel.begin(), rel.end());
		}


		template<size_t I, typename execution_policy = std::execution::parallel_policy>
		void merge_on_par(relation_t & rel, execution_policy  policy = std::execution::par)
		{
			relation_t ret(container_t::size() + rel.size());
			std::merge(policy, container_t::begin(), container_t::end(), rel.begin(), rel.end(), ret.begin(), [&](const tuple_t& val1, const tuple_t& val2) {
				return (std::get<I>(val1) < std::get<I>(val2));
				});
			(*this) = std::move(ret);
		}

		template<typename execution_policy = std::execution::parallel_policy>
		void merge_par(relation_t & rel, execution_policy policy = std::execution::par)
		{
			relation_t ret(container_t::size() + rel.size());
			std::merge(policy, container_t::begin(), container_t::end(), rel.begin(), rel.end(), ret.begin());
			(*this) = std::move(ret);
		}

		template<size_t I, typename order_by = order_asc<typename std::tuple_element_t<I, tuple_t>>, typename execution_policy = std::execution::parallel_policy>
		void order_by_par(execution_policy policy = std::execution::par) {
			detail::sort_par(*this, [&](tuple_t& l, tuple_t& r) {
				return (order_by{}(std::get<I>(l), std::get<I>(r)));
				}, policy);
		}

		template<size_t...I>
		auto select_par()
		{
			using T = std::tuple<std::tuple_element_t<I, tuple_t>...>;
			relation<container<T, alloc_t<T>>> new_relation(container_t::size());
			
			//atomic_counter ?? might be better
			std::mutex m;
			volatile size_t x{ 0 };
			std::for_each(std::execution::par, container_t::begin(), container_t::end(), [&](const tuple_t& value) {
				std::unique_lock<std::mutex> lock(m);
				thread_local size_t index = x++;
				lock.unlock();
				new_relation[index] = std::make_tuple(std::get<I>(value)...);
			});
			return std::move(new_relation);
		}
		

		template<size_t I, typename execution_policy = std::execution::parallel_policy>
		void unique_par(execution_policy policy = std::execution::par)
		{
			auto it = std::unique(policy, container_t::begin(), container_t::end(), [&](const tuple_t& val1, const tuple_t& val2) {
				return std::get<I>(val1) == std::get<I>(val2);
				});
			container_t::erase(it, container_t::end());
		}

		template<size_t I1, size_t I2, typename rel_t, typename execution_policy = std::execution::parallel_policy >
		auto join_on_par(rel_t& rel, execution_policy policy = std::execution::par)
		{
			static_assert(std::is_same_v<typename std::tuple_element_t<I1, tuple_t>, typename std::tuple_element_t<I2, typename rel_t::tuple_t>>
				|| std::is_convertible_v<elem_t<I1>, typename rel_t::template elem_t<I2> >, "Cannot join on column that are not same type or the types are not convertible");
			using type = typename detail::join_tuple_type<tuple_t, typename relation_t::tuple_t>::type;
			
			relation<container<type, alloc_t<type>>> new_relation;
			std::unordered_map<std::tuple_element_t<I2, typename relation_t::tuple_t>, typename relation_t::tuple_t*> find_map;

			std::mutex m1, m2;
			std::for_each(policy, rel.begin(), rel.end(), [&](const typename rel_t::tuple_t& value) {
				std::lock_guard<std::mutex> lock(m1);
				find_map.insert(std::make_pair(std::get<I2>(value), &value));
			});

			std::for_each(policy, container_t::begin(), container_t::end(), [&](const tuple_t& value) {
				std::lock_guard<std::mutex> lock(m2);
				auto find_iter = find_map.find(std::get<I1>(value));
				if (find_iter != find_map.end()){
					new_relation.push_back(std::forward<type>(std::tuple_cat(value, *(find_iter->second))));
				}
			});
			return std::move(new_relation);
		}

		template<size_t col, typename execution_policy = std::execution::parallel_policy >
		bool find_on_par(const typename std::tuple_element_t<col, tuple_t>& value, tuple_t& found_row, execution_policy policy = std::execution::par) const
		{
			auto it = std::find_if(policy, container_t::begin(), container_t::end(), [&](const tuple_t& tuple) {
				return(value == std::get<col>(tuple));
				});
			if (it == container_t::end())
			{
				return false;
			}
			found_row = *it;
			return true;
		}
		template<size_t I, typename execution_policy = std::execution::parallel_policy>
		auto map_group_by_par(execution_policy policy = std::execution::par) {
			std::unordered_map<elem_t<I>, relation_t> group_map;
			std::mutex m;
			std::for_each(policy, container_t::begin(), container_t::end(), [&](const tuple_t& value) {
				std::lock_guard<std::mutex> lock(m);
				group_map[std::get<I>(value)].push_back(value);
			});
			return group_map;
		}

	protected:
		static row_t default_row;
		inline const tuple_t& tuple_at(size_t row) const{
			return *(std::next(container_t::begin(), row));
		}

	};

	//static data, so ugly
	template<template<class, class> class container, typename... val>
	typename relation<container<std::tuple<val...>, alloc_t<std::tuple<val...>>>>::row_t  relation<container<std::tuple<val...>, alloc_t<std::tuple<val...>>>>::default_row{};
	
	template<template<class, class> class container, typename... val>
	size_t relation<container<std::tuple<val...>, alloc_t<std::tuple<val...>>>>::row_id{ -1 };


	//set relation: This is turning out to be a dead end
	//might just have to give seq containers the ability to maintain a sorted list
	//and ablity to search the list
	template<template<class, class, class>typename container, typename... val>
	class relation<container<std::tuple<val...>, key_comp_set_t<std::tuple<val...>>, alloc_t<std::tuple<val...>> >> : public container<std::tuple<val...>, key_comp_set_t<std::tuple<val...>>, alloc_t<std::tuple<val...>>>
	{

	public:
		

		using container_t = container<std::tuple<val...>, key_comp_set_t<std::tuple<val...>>, alloc_t<std::tuple<val...>>>;
		using tuple_t = typename container_t::value_type;
		using container_tag = map_relation_tag;
		using compare_t = typename container_t::key_compare;
		using relation_t = relation;



		template<size_t I>
		using elem_t = std::tuple_element_t<I, tuple_t>;

		relation() = default;
		explicit relation(size_t size) : container_t{ size } {}
		relation(const relation& val) : container_t(val) {}
		relation(const relation&& val) noexcept : container_t(std::move(val)) {};
		relation& operator=(const relation& rhs)
		{
			container_t::operator=(rhs);
			return (*this);
		}
		relation& operator=(const relation&& rhs) noexcept
		{
			container_t::operator=(std::move(rhs));
			return (*this);
		}

		virtual ~relation() {}


		auto insert(const tuple_t& tuple)
		{
			return container_t::insert(std::move(tuple));
		}

		template<typename...T>
		void add(const T&& ... args)
		{
			static_assert(std::tuple_size_v<tuple_t> == sizeof...(args), "Incomplete argument in add");
			container_t::insert(tuple_t((args)...));
		}


		template<size_t I>
		auto average()
		{
			double sum = 0.0;
			for (auto& tuple : *this)
			{
				sum += std::get<I>(tuple);
			}
			return (sum / static_cast<double>(container_t::size()));
		}

		static tuple_t make_rel_element(val... values)
		{
			return std::forward_as_tuple(values...);
		}

		template<size_t I>
		auto group_by() -> std::vector<relation_t>
		{
				std::unordered_map<elem_t<I>, relation_t> sort_map{};
				for (auto iter = container_t::begin(); iter != container_t::end(); iter++)
				{
					sort_map[std::get<I>(*iter)].insert(*iter);
				}
				std::vector<relation_t> rel;
				for (auto iter_map = sort_map.begin(); iter_map != sort_map.end(); iter_map++)
				{
					rel.push_back(std::move(iter_map->second));
				}
				return std::move(rel);
		}

		template<size_t I, size_t J, typename rel>
		auto join_on(rel& set_rel)
		{
			using type = typename detail::join_tuple_type<tuple_t, typename rel::tuple_t>::type;
			relation<container<type, key_comp_set_t<type>, alloc_t<type>>> new_relation;

		}


		void merge(relation_t& rel)
		{
			relation_t result(container_t::size() + rel.size());
			std::set_union(container_t::begin(), container_t::end(), rel.begin(), rel.end(), result.begin());
			(*this) = std::move(result);
		}


		bool is_sub_relation(relation_t& rel)
		{
			return std::includes(container_t::begin(), container_t::end(), rel.begin(), rel.end());
		}

	private:
		typename container_t::iterator tuple_at(const tuple_t& key)
		{
			return container_t::find(key);
		}


	};
};

