#pragma once
#include <tuple>
#include <type_traits>
#include <utility>
namespace nl
{
	template<size_t...I>
	using select = std::index_sequence<I...>;
	template<typename relation>
	using select_all = std::make_index_sequence<relation::column_count>;

	namespace detail
	{
		template<bool flag, typename T, typename U>
		struct select_type
		{
			typedef T type;
		};

		template<typename T, typename U>
		struct select_type<false, T, U>
		{
			typedef U type;
		};

		template<typename T>
		struct type_to_type
		{
			typedef T type;
		};


		template<typename T, typename S>
		struct _join_tuple_size
		{
			enum { value = std::tuple_size_v<T> +std::tuple_size_v<S> };
		};

		template<typename T, typename S>
		struct join_tuple_type;

		template<typename... Ts, typename... Ss>
		struct join_tuple_type<std::tuple<Ts...>, std::tuple<Ss...>>
		{
			using type = std::tuple<Ts..., Ss...>;
		};

		template<typename T>
		struct join_tuple_type<T, void>
		{
			using type = T;
		};

		template<typename tuple_t, class T> struct index_of;

		template<typename U, typename T>
		struct index_of<std::tuple<U>, T> { enum { value = -1 }; };

		template<typename T>
		struct index_of<std::tuple<T>, T> { enum { value = 0 }; };

		template<typename T, typename...U>
		struct index_of<std::tuple<T, U...>, T> { enum { value = 0 }; };

		template<typename T, typename U, typename ...S>
		struct index_of<std::tuple<U, S...>, T>
		{
		private:
			enum { temp = index_of<std::tuple<S...>, T>::value };
		public:
			enum { value = temp == -1 ? -1 : 1 + temp };
		};

		template<typename tuple_t, typename T> struct erase_type;

		template<typename U, typename T>
		struct erase_type<std::tuple<U>, T>
		{
			typedef std::tuple<U> type;
		};

		template<typename T>
		struct erase_type<std::tuple<T>, T>
		{
			typedef void type;
		};

		template<typename T, typename... U>
		struct erase_type<std::tuple<T, U...>, T>
		{
			typedef std::tuple<U...> type;
		};

		template<typename T, typename U, typename...S>
		struct erase_type<std::tuple<U, S...>, T>
		{
			typedef typename join_tuple_type<std::tuple<U>, typename erase_type<std::tuple<S...>, T>::type >::type type;
		};

		template<typename tuple_t, typename T> struct erase_all_type;

		template<typename U, typename T>
		struct erase_all_type<std::tuple<U>, T>
		{
			typedef std::tuple<U> type;
		};

		template<typename T>
		struct erase_all_type<std::tuple<T>, T>
		{
			typedef void type;
		};

		template<typename T, typename ... S>
		struct erase_all_type<std::tuple<T, S...>, T>
		{
			typedef typename erase_all_type<std::tuple<S...>, T>::type type;
		};

		template<typename T, typename U, typename...S>
		struct erase_all_type<std::tuple<U, S...>, T>
		{
			typedef typename join_tuple_type<std::tuple<U>, typename erase_all_type<std::tuple<S...>, T>::type >::type type;
		};

		template<typename tuple_t> struct remove_duplicate;

		template<typename U>
		struct remove_duplicate<std::tuple<U>>
		{
			typedef void type;
		};

		template<typename T, typename... S>
		struct remove_duplicate<std::tuple<T, S...>>
		{
		private:
			typedef typename remove_duplicate<std::tuple<S...>>::type L1;
			typedef typename erase_type<typename select_type<std::is_void_v<L1>, std::tuple<S...>, L1>::type, T>::type L2;
		public:
			typedef typename join_tuple_type<std::tuple<T>, L2>::type  type;
		};

		template<typename tuple_t>
		using remove_duplicate_t = typename remove_duplicate<tuple_t>::type;

		template<typename tuple, typename T> class append;

		template<typename T, typename...U>
		class append<std::tuple<U...>, T>
		{
		public:
			typedef std::tuple<U..., T> type;
		};

		template<typename... T, typename... U>
		class append<std::tuple<T...>, std::tuple<U...>>
		{
		public:
			typedef std::tuple<T..., U...> type;
		};

		template<typename T, typename U>
		using append_t = typename append<T, U>::type;

		template<typename tuple> class reverse;

		template<typename T>
		class reverse<std::tuple<T>>
		{
		public:
			typedef std::tuple<T> type;
		};

		template<typename T, typename... S>
		class reverse<std::tuple<T, S...>>
		{
		public:
			typedef append_t< typename reverse<std::tuple<S...>>::type, T> type;
		};

		template<typename tuple>
		using reverse_t = typename reverse<tuple>::type;

		template<typename T, typename... List>
		struct first_type_from_list {
			using type = T;
		};

		template<typename... List>
		using first_type_from_list_t = typename first_type_from_list<List...>::type;
	}
}