#pragma once
#include <algorithm>
#include <string_view>
#include <string>
#include <type_traits>
#include <fmt/format.h>
#include <vector>
#include <sstream>
using namespace std::literals::string_literals;

#include "relation.h"

namespace nl
{
	struct not_null {
		enum { value = 0 };
	};
	struct unique {
		enum { value = 1 };
	};
	struct collate_no_case {
		enum { value = 2 };
	};
	struct collate_case {
		enum { value = 3 };
	};
	struct primary_key {
		enum { value = 4 };
	};
	struct no_op {
		enum { value = -1 };
	};

	namespace detail
	{

		//over
		namespace query {
			template<typename _policy, std::enable_if_t<std::is_same_v<_policy, collate_no_case>, int> = 0>
			inline void add_policy(std::stringstream & mQuery)
			{
				mQuery << "COLLATE NOCASE ";
			}

			template<typename _policy, std::enable_if_t<std::is_same_v<_policy, collate_case>, int> = 0>
			inline void add_policy(std::stringstream & mQuery)
			{
				mQuery << "COLLATE CASE ";
			}

			template<typename _policy, std::enable_if_t<std::is_same_v<_policy, primary_key>, int> = 0>
			inline void add_policy(std::stringstream & mQuery)
			{
				mQuery << "PRIMARY KEY ";
			}

			template<typename _policy, std::enable_if_t<std::is_same_v<_policy, unique>, int> = 0>
			inline void add_policy(std::stringstream & mQuery)
			{
				mQuery << "UNIQUE ";
			}

			template<typename _policy, std::enable_if_t<std::is_same_v<_policy, not_null>, int> = 0>
			inline void add_policy(std::stringstream & mQuery)
			{
				mQuery << "NOT NULL ";
			}

			template<typename _policy, std::enable_if_t<std::is_same_v<_policy, no_op>, int> = 0>
			inline void add_policy(std::stringstream & mQuery)
			{
				return;
			}

			template<size_t count>
			class loop
			{
			public:
				template<typename tuple_t>
				static void do_policy(std::stringstream& stream)
				{
					using policy_type = typename std::tuple_element_t<count, tuple_t>;
					loop<count - 1>::template do_policy<tuple_t>(stream);
					add_policy<policy_type>(stream);
				}
			};

			template<>
			class loop<0>
			{
			public:
				template<typename tuple_t>
				static void do_policy(std::stringstream& stream)
				{
					add_policy<std::tuple_element_t<0, tuple_t> >(stream);
				}

			};
		};
	};


	class query
	{
	public:


		query();
		query(const std::string& query);

		query& del(const std::string_view& table);
		query& where(const std::string_view& list);
		query& update(const std::string_view& table);
		query& insert(const std::string_view& table);
		query& create_table(const std::string_view& table);
		query& create_view(const std::string_view& view);
		query& create_index(const std::string_view& index, const std::string_view& table, const std::initializer_list<std::string_view>& colList = {});
		query& create_table_temp(const std::string_view& tempTable);
		query& create_view_temp(const std::string_view& tempView);
		query& create_index_temp(const std::string_view& TempIndex, const std::string_view& table, const std::initializer_list<std::string_view>& colList = {});
		query& inner_join(const std::initializer_list<std::string_view>& list);
		query& on(const std::initializer_list<std::string_view>& list);
		query& drop_table(const std::string_view& table);
		query& drop_view(const std::string_view& view);
		query& drop_trigger(const std::string_view& trigger);
		query& and_(const std::string_view& condition);
		query& or_(const std::string_view& condition);
		query& not_(const std::string_view& condition);

		query& create_trigger(const std::string_view& trig_name, const std::string_view& table);

		inline query& clear() { mQuery.str(std::string()); return (*this); }

		template<typename...T>
		query& select(const T&...args)
		{
			return do_arguements("SELECT ", "{},", "{}", args...);
		}
		template<typename...T>
		query& from(const T& ... args)
		{
			return do_arguements("FROM ", "{},", "{}", args...);
		}

		template<typename...T>
		query& set(const T&...args)
		{
			return do_arguements("SET "s, "{} = :{},", "{}=:{}", args...);
		}

		template<typename...T>
		query& values(T...args)
		{
			return do_arguements("VALUES (", ":{},", ":{})", args...);
		}

		template<typename def_t>
		query& default_(const def_t& def)
		{
			static_assert(nl::detail::is_database_type<def_t>::value, "Default type is not a database type");
			static_assert(fmt::is_formattable<def_t>::value, "Default type is not an formatable type");
			if constexpr (std::is_floating_point_v<def_t>) mQuery << fmt::format("DEFAULT {:.4f} ", def);
			else {
				mQuery << fmt::format("DEFAULT {} ", def);
			}
			return (*this);
		}

		/**
		col_poilcy... policy in the column defination
		maybe cp::no_case since col_policy is long to write
		put in a tuple, since they are enums we can only use the type at complie time
		//write a bunch of these functions to add the different policies
		template<col_policy _policy, std::enable_if<_policy == col_policy::no_case, int> = 0>
		inline void add_policy(std::stringstream& mQuery, col_policy policy)
		{ mQuery << "NO CASE ";}


		*/

		template<typename T, typename... policies>
		query& column(const std::string_view& name)
		{
			using tuple_t = std::tuple<policies...>;

			if (first_col) {
				first_col = false;
				mQuery << "( ";
			}
			else
			{
				mQuery << ", ";
			}
			if constexpr (std::is_same_v<T, std::string>)
			{
				mQuery << fmt::format(" {} TEXT ", name);
				detail::query::loop<sizeof...(policies) - 1>::template do_policy<tuple_t>(mQuery);
			}
			else if constexpr (std::is_integral_v<T>)
			{
				mQuery << fmt::format("{} INTEGER ", name);
				detail::query::loop<sizeof...(policies) - 1 >::template do_policy<tuple_t>(mQuery);
			}
			else if constexpr (std::is_floating_point_v<T>)
			{
				mQuery << fmt::format("{} REAL ", name);
				detail::query::loop<sizeof...(policies) - 1 >::template do_policy<tuple_t>(mQuery);

			}
			else if constexpr (std::is_same_v < T, nl::blob_t>)
			{
				mQuery << fmt::format("{} BLOB ", name);
				detail::query::loop<sizeof...(policies) - 1>::template do_policy<tuple_t>(mQuery);

			}
			return (*this);
		}

		query& end_col();
		query& end();
		query& begin();
		query& begin_immediate();
		query& roll_back();


		//appends a ";" at the end
		const std::string get_query() const {
			/*
			test this first
			std::string_view sv = mQuery.str();
			if (*(sv.end() - 1) == (char)" ")
			{
				mQuery.seekp(1, std::ios::end);
			}
			*/
			return std::string(mQuery.str() + ";");
		};
	private:
		bool first_col{ false };
		std::stringstream mQuery{};


		template<typename S, typename...T>
		query& do_arguements(const std::string& func, 
			const std::string& in_format, 
			const std::string& out_format, 
			const S& start, 
			const T& ... args)
		{
			std::array<S, sizeof...(T) + 1> parameters{ start, args... };
			std::string ft(func);
			for (int i = 0; i < parameters.size() - 1; i++)
			{
				ft += fmt::format(in_format, parameters[i], parameters[i]);
			}
			ft += fmt::format(out_format, parameters[parameters.size() - 1], parameters[parameters.size() - 1]);
			mQuery << ft;
			return (*this);
		}

		template<typename S>
		inline query& do_arguements(const std::string& func, 
				const std::string&/*not used needed for the overload*/,
				const std::string& format, 
				const S& start)
		{
			mQuery << fmt::format("{} {}", func, fmt::format(format, start));
			return (*this);
		}
		
	};



};