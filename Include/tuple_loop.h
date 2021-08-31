#pragma once
#include <tuple>
#include <SQLite/sqlite3.h>
namespace nl
{
	template<size_t...I>
	using select = std::index_sequence<I...>;

	template<typename T>
	using alloc_t = std::allocator<T>;

	template<typename ty>
	using hash_t = std::hash<ty>;

	template<typename ty>
	using key_comp_map_t = std::equal_to<ty>;

	template<typename ty>
	using key_comp_set_t = std::less<ty>;

	struct linear_relation_tag {};
	struct map_relation_tag {};


	template<typename T>
	using order_asc = std::less<T>;

	template<typename T>
	using order_dec = std::greater<T>;


	template< typename container>
	class relation;
	struct linear_relation_tag;
	struct map_relation_tag;

	template<typename...T>
	using vector_relation = relation<std::vector<std::tuple<T...>>>;
	template<typename...T>
	using list_relation = relation<std::list<std::tuple<T...>>>;
	template<typename...T>
	using set_relation = relation<std::set<std::tuple<T...>>>;

	template<size_t offset, typename tuple_t>
	constexpr size_t j_ = std::tuple_size_v<tuple_t> + offset;

	namespace detail
	{
		template<size_t count>
		class loop
		{
		public:
			template<typename relation_t, template<typename> typename buffer_t>
			static void do_buffer_write(buffer_t<relation_t>& buffer, typename relation_t::tuple_t& tuple)
			{
				loop<count - 1>::template do_buffer_write<relation_t, buffer_t>(buffer, tuple);
				buffer.write(std::get<count>(tuple));
			}

			template<typename relation_t, template<typename> typename buffer_t>
			static auto do_buffer_read(buffer_t<relation_t>& buffer)
			{
				auto t0 = loop<count - 1>::template do_buffer_read<relation_t, buffer_t>(buffer);
				using arg_type = typename std::tuple_element_t<count, typename relation_t::tuple_t>;
				arg_type object;
				buffer.read(object);
				return std::tuple_cat(t0, std::make_tuple(object));
			}
		};

		template<>
		class loop<0>
		{
		public:
			template<typename relation_t, template<typename> typename buffer_t>
			static void do_buffer_write(buffer_t<relation_t>& buffer, typename relation_t::tuple_t& tuple)
			{
				buffer.write(std::get<0>(tuple));
			}

			template<typename relation_t, template<typename> typename buffer_t>
			static auto do_buffer_read(buffer_t<relation_t>& buffer)
			{
				using arg_type = typename std::tuple_element_t<0, typename relation_t::tuple_t>;
				arg_type object;
				buffer.read(object);
				return std::make_tuple(object);
			}
		};




		template<size_t I, typename T, typename S>
		struct comp_tuple_with_value
		{
			bool operator()(const T& t, S s) const { return std::get<I>(t) < s; }
			bool operator()(S s, const T& t) const { return s < std::get<I>(t); }
		};

		template<typename T, typename S>
		struct _join_tuple_size
		{
			enum {value = std::tuple_size_v<T> + std::tuple_size_v<S>};
		};

		template<typename T, typename S>
		struct join_tuple_type
		{
			using type = decltype(std::tuple_cat(T{}, S{}));
		};
		
		template<typename rel, typename Compare, std::enable_if_t<std::is_same_v<typename rel::container_t, std::list<typename rel::tuple_t>>, int> = 0>
		void sort(rel & rel, Compare comp)
		{
			rel.sort(comp);
		}

		template<typename rel, typename Compare, std::enable_if_t<std::is_same_v<typename rel::container_t, std::vector<typename rel::tuple_t>>, int> = 0>
		void sort(rel & rel, Compare comp)
		{
			std::sort(rel.begin(), rel.end(), comp);
		}
		
		//R is the super class
		//T is the subclass

		template<class R, class T>
		class is_converatable_to_relation
		{
			typedef char small_t;
			class big_t { char dummy[2]; };
			static small_t test(const T&);
			static big_t test(...);
			static R make_t();
		public:
			enum {exisit = sizeof(test(make_t())) == sizeof(small), same_type = false};
		};

		template<class T>
		class is_converatable_to_relation<T,T>
		{
		public:
			enum {exisit = 1, same_type = 1};
		};

		template<typename R, typename T>
		struct _derived_from_relation
		{
			enum
			{
				value = (is_converatable_to_relation<const std::decay_t<T>*, const std::decay_t<R>*>::exisit && !is_converatable_to_relation<const std::decay_t<R>*, void*>::same_type)
			};
		};


		//figure out a better way to do this
		template <typename T> class _is_relation { public:enum { value = false }; };
		template<typename...T> class _is_relation<vector_relation<T...>> { public:enum { value = true }; };
		template<typename...T> class _is_relation<list_relation<T...>> { public:enum { value = true }; };
		template<typename...T> class _is_relation<set_relation<T...>> { public:enum { value = true }; };

	
		template<typename T, typename relation> class has_base_relation { 
			public: 
				enum { value = _derived_from_relation<relation, T>::value}; 
		};
		

		template<typename T>
		using is_relation = _is_relation<std::decay_t<T>>;

		template<typename T>
		constexpr bool is_relation_v = is_relation<T>::value;

		template<typename T>
		struct is_linear_relation
		{
			enum {
				value = std::is_same_v<typename T::container_tag, linear_relation_tag>
			};
		};

		template<typename T>
		struct is_map_relation
		{
			enum {
				value = std::is_same_v<typename T::container_tag, map_relation_tag>
			};
		};


	}
}
