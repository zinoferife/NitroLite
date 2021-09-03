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

	template<template<class, class>typename container, typename... val>
	class relation<container<std::tuple<val...>, alloc_t<std::tuple<val...> > > > : public container<std::tuple<val...>, alloc_t<std::tuple<val...>>>
	{
	public:

		enum rel_constants
		{
			unique_id = 0,
			col_id_start
		};

		static size_t ROWID;
		using container_t = container<std::tuple<val...>, alloc_t<std::tuple<val...>>>;
		using tuple_t = typename container_t::value_type;
		using container_tag = linear_relation_tag;
		using relation_t = relation;


		relation() = default;
		explicit relation(size_t size) : container_t{ size } {}
		relation(const relation& val) : container_t(val) {}
		relation(const relation&& val) noexcept : container_t(std::move(val)) {};
		relation& operator=(const relation& rhs)
		{
			container_t::operator=(rhs);
		}
		relation& operator=(const relation&& rhs) noexcept
		{
			container_t::operator=(std::move(rhs));
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

		template<typename...T>
		void add_to_relation(T&& ... args)
		{
			static_assert(std::tuple_size_v<tuple_t> == sizeof...(args), "Incomplete argument in add_to_relation");
			container_t::emplace_back(tuple_t(std::forward<T>(args)...));
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


		template<size_t... I>
		inline std::tuple<std::tuple_element_t<I, tuple_t>...>  get(size_t row, std::index_sequence<I...>) const
		{
			return std::make_tuple(std::get<I>(tuple_at(row))...);
		}

		template<size_t col>
		tuple_t& find_on(const typename std::tuple_element_t<col, tuple_t>& value) const
		{
			auto it = std::find_if(container_t::begin(), container_t::end(), [&](const tuple_t& tuple) {
				return(value == std::get<col>(tuple));
				});
			return(*it);
		}
		template<size_t I>
		std::vector<relation_t> group_by()
		{
			order_by<I>();
			std::vector<relation_t> ret_vec;
			for (auto iter = container_t::begin(); iter != container_t::end();)
			{
				detail::comp_tuple_with_value<I, tuple_t, std::tuple_element_t<I, tuple_t>> comp{};
				auto upper_iter = std::upper_bound(iter, container_t::end(), std::get<I>(*iter), comp);
				relation_t new_relation(std::distance(iter, upper_iter));
				std::copy(iter, upper_iter, new_relation.begin());
				iter = upper_iter;
				ret_vec.push_back(std::move(new_relation));
			}
			return ret_vec;
		}

		//O(M+n) complexity in time
		//O(n) complexity in memory
		//assumes that both col I1 and I2 have unique elements

		template<size_t I1, size_t I2, typename relation_t >
		auto join_on(relation_t& rel)
		{
			static_assert(std::is_same_v<typename std::tuple_element_t<I1, tuple_t>, typename std::tuple_element_t<I2, typename relation_t::tuple_t>>, "Cannot join on column that are not same type");
			using type = typename detail::join_tuple_type<tuple_t, typename relation_t::tuple_t>::type;
			relation<container<type, alloc_t<type>>> new_relation;
			std::unordered_map<std::tuple_element_t<I2, typename relation_t::tuple_t>, typename relation_t::iterator> find_map;
			for (auto rel_iter = rel.begin(); rel_iter != rel.end(); rel_iter++)
			{
				find_map.insert(std::make_pair(std::get<I2>(*rel_iter), rel_iter));
			}
			for (auto this_iter = container_t::begin(); this_iter != container_t::end(); this_iter++)
			{
				auto find_iter = find_map.find(std::get<I1>(*this_iter));
				if (find_iter != find_map.end())
				{
					new_relation.push_back(std::forward<type>(std::tuple_cat(*this_iter, *(find_iter->second))));;
				}
			}
			return std::move(new_relation);
		}

		//removes all consequtive adjacent dublicates, sort first if you want to remove all dublicate 
		//O(N) for the whole relation
		template<size_t I>
		void unique()
		{
			auto start = container_t::begin();
			auto result = start;
			while (++start != container_t::end())
			{
				if (!(std::get<I>(*result) == std::get<I>(*start)) && (std::get<I>(*(++result)) != std::get<I>(*start)))
				{
					*result = std::move(*start);
				}
			}
			auto end = ++result;
			container_t::erase(end, container_t::end());
		}


		template<size_t...I>
		auto select()
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
		void order_by()
		{
			detail::sort(*this, [&](tuple_t& l, tuple_t& r) {
					return (order_by{}(std::get<I>(l), std::get<I>(r)));
				});
		}

		template<typename...T>
		void unpack_row_in(size_t row, T& ...args)
		{
			if (container_t::empty()) return;
			std::tie(args...) = tuple_at(row);
		}


		static inline tuple_t make_rel_element(const val&... values)
		{
			return std::make_tuple(values...);
		}

	protected:
		inline const tuple_t& tuple_at(size_t row) const
		{
			return *(std::next(container_t::begin(), row));
		}
	};

	template<template<class, class, class>typename container, typename... val>
	class relation<container<std::tuple<val...>, key_comp_set_t<std::tuple<val...>>, alloc_t<std::tuple<val...>> >> : public container<std::tuple<val...>, key_comp_set_t<std::tuple<val...>>, alloc_t<std::tuple<val...>>>
	{

	public:

		using container_t = container<std::tuple<val...>, key_comp_set_t<std::tuple<val...>>, alloc_t<std::tuple<val...>>>;
		using tuple_t = typename container_t::value_type;
		using container_tag = map_relation_tag;

		relation() = default;
		relation(relation&) = default;
		relation(relation&&) = default;
		relation& operator=(relation&) = default;
		relation& operator=(relation&&) = default;
		virtual ~relation() {}

		auto insert(tuple_t tuple)
		{
			return container_t::insert(tuple);
		}

		template<typename...T>
		void add_to_relation(T&& ... args)
		{
			static_assert(std::tuple_size_v<tuple_t> == sizeof...(args), "Incomplete argument in add_to_relation");
			container_t::insert(tuple_t(std::forward<T>(args)...));
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
	};
};

