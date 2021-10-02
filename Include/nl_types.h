#pragma once
#include <vector>
#include <variant>
#include "tuple_t_operations.h"
namespace nl
{
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
	struct set_relation_tag;
	struct hash_relation_tag;
	struct map_relation_tag;


	template<typename...T>
	using vector_relation = relation<std::vector<std::tuple<T...>>>;
	template<typename...T>
	using list_relation = relation<std::list<std::tuple<T...>>>;
	template<typename...T>
	using set_relation = relation<std::set<std::tuple<T...>>>;

	template<size_t offset, typename tuple_t>
	constexpr size_t j_ = std::tuple_size_v<tuple_t> +offset;

	using blob_t = std::vector<std::uint8_t>;

	template<typename tuple> struct variant_no_duplicate;

	template<typename ... val>
	struct variant_no_duplicate<std::tuple<val...>>
	{
		using type = std::variant<val...>;
	};

	template<typename tuple>
	struct variant
	{
	private:
		using no_duplicate = nl::detail::remove_duplicate_t<tuple>;
		using tuple_t = std::conditional_t<std::is_void_v<no_duplicate>, tuple, no_duplicate>;
	public:
		using type = typename variant_no_duplicate<tuple_t>::type;
	};
}