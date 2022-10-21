#pragma once
#include <tuple>
#include <fmt/format.h>
#include <SQLite/sqlite3.h>

#include "nl_types.h"
#include "visitor.h"
#include "tuple_t_operations.h"
#include "nl_time.h"
#include "nl_uuid.h"
namespace nl
{

	template<size_t I, typename row_t>
	constexpr inline const auto& row_value(const row_t& tuple) noexcept
	{
		return std::get<I>(tuple);
	}
	template<size_t I, typename row_t>
	constexpr inline auto& row_value(row_t& tuple) noexcept
	{
		return std::get<I>(tuple);
	}
	
	namespace detail
	{
		//sqlite only wants 5 types: integral, floating_point, string, blob and null
		//in nitrolite blob is std::vector<uint8_t>
		//date_time_t time is a chrono::time_point that is store as a int64 in sqlite,
		//the standard says system clock's duration representation can be at least 45 bits
		//storing as int64 should be sufficient 
		//this type trait ensures we are only operating in those types

		template<typename T>
		class is_database_type
		{
			using special_types = std::tuple<std::string, blob_t, nullptr_t, date_time_t, uuid>;
		public:
			enum {value = (std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_enum_v<T> || index_of<special_types, T>::value >= 0) };
		};

		template<typename T>
		std::uint64_t func1(std::reference_wrapper<T>* t);
		std::uint8_t func1(...);

		template<typename T>
		struct is_std_ref_type :
			public std::conditional_t<
			(sizeof(func1((T*)nullptr)) == sizeof(std::uint64_t)), std::true_type, std::false_type>
		{};

		
		//for SQL with ? instead of a parameter
		template<size_t count, typename database_stmt, typename tuple_t>
		inline bool handle_bind(database_stmt statement, const tuple_t& tuple)
		{
			//col_id is the value in the tuple
			//sqlite wants positional parameters to start from 1 not 0 so add on to col_id to get its pos
			constexpr size_t col_id = (std::tuple_size_v<tuple_t> - (count + 1));
			constexpr size_t position = col_id + 1;
			using arg_type = std::decay_t<std::tuple_element_t<col_id, tuple_t>>;
			static_assert(is_database_type<arg_type>::value, "Tuple type is not a valid database type");	
			if constexpr (std::is_integral_v<arg_type>)
			{
				//64-bit int
				if constexpr (sizeof(arg_type) == sizeof(std::uint64_t))
				{
					return (SQLITE_OK == sqlite3_bind_int64(statement, position, std::get<col_id>(tuple)));
				}
				return (SQLITE_OK == sqlite3_bind_int(statement, position, std::get<col_id>(tuple)));
			}
			else if constexpr (std::is_floating_point_v<arg_type>)
			{
				if constexpr (std::is_same_v<arg_type,float>)
				{
					return (SQLITE_OK == sqlite3_bind_double(statement, position, static_cast<double>(std::get<col_id>(tuple))));
				}
				return (SQLITE_OK == sqlite3_bind_double(statement, position, std::get<col_id>(tuple)));
			}
			//need to add support for other string formats
			else if constexpr (std::is_same_v<arg_type, std::string>)
			{
				const std::string value = std::get<col_id>(tuple);
				if (!value.empty())
				{
					return (SQLITE_OK == sqlite3_bind_text(statement, position, value.c_str(), value.size(), SQLITE_TRANSIENT));
				}
			}
			else if constexpr (std::is_same_v<arg_type, blob_t>)
			{
				auto vec = std::get<col_id>(tuple);
				if (vec.empty())
				{
					//write null??? or just leave it so that, we would just bind null on empty vector
					return (SQLITE_OK == sqlite3_bind_null(statement, position));
				}
				return (SQLITE_OK == sqlite3_bind_blob(statement, position, vec.data(), vec.size(), SQLITE_TRANSIENT));
			}
			else if constexpr (std::is_null_pointer<arg_type>::value)
			{
				return (SQLITE_OK == sqlite3_bind_null(statement, position));
			}
			else if constexpr (std::is_same_v<arg_type, date_time_t>)
			{
				auto rep = nl::to_representation(std::get<col_id>(tuple));
				return (SQLITE_OK == sqlite3_bind_int64(statement, position, rep));
			}
			else if constexpr (std::is_same_v<arg_type, uuid>)
			{
				auto blob = std::get<col_id>(tuple).to_blob();
				return (SQLITE_OK == sqlite3_bind_blob(statement, position, blob.data(), blob.size(), SQLITE_TRANSIENT));
			}
			else if constexpr (std::is_enum_v<arg_type>) {
				auto en = static_cast<std::uint32_t>(std::get<col_id>(tuple));
				return (SQLITE_OK == sqlite3_bind_int(statement, position, en));
			}
			else
			{
				return false;
			}

		}

		template<size_t count,typename database_stmt_t, typename tuple_t, typename para_array_t>
		inline bool handle_bind_para(database_stmt_t statement, const tuple_t& tuple, const para_array_t& array)
		{
			constexpr size_t col_id = std::tuple_size_v<tuple_t> - (count + 1);
			using arg_type = typename std::decay_t<std::tuple_element_t<col_id, tuple_t>>;
			using array_value_type = typename std::decay_t<typename para_array_t::value_type>;
			static_assert(is_std_ref_type<array_value_type>::value, "Parameters must be ref wrapped");
			static_assert(is_database_type<arg_type>::value, "Tuple type is not a valid database type");
			size_t position = -1;
			
			if constexpr (std::is_convertible_v<typename array_value_type::type, std::string>)
			{
				const std::string p_name = fmt::format(":{}", array[col_id].get());
				position = sqlite3_bind_parameter_index(statement, p_name.c_str());
				if (position == 0) return false;
			}
			else if constexpr (std::is_convertible_v<typename array_value_type::type, size_t>)
			{
				position = array[col_id].get();
			}

			if constexpr (std::is_integral_v<arg_type>)
			{
				//64-bit integers have speical functions to upload in sqlite
				if constexpr (sizeof(arg_type) == sizeof(std::uint64_t))
				{
					return (SQLITE_OK == sqlite3_bind_int64(statement, position, std::get<col_id>(tuple)));
				}
				return (SQLITE_OK == sqlite3_bind_int(statement, position, std::get<col_id>(tuple)));
			}
			else if constexpr (std::is_floating_point_v<arg_type>)
			{
				if constexpr (std::is_same_v<arg_type, float>)
				{
					return (SQLITE_OK == sqlite3_bind_double(statement, position, static_cast<double>(std::get<col_id>(tuple))));
				}
				return (SQLITE_OK == sqlite3_bind_double(statement, position, std::get<col_id>(tuple)));
			}
			//need to add support for other string formats
			else if constexpr (std::is_same_v<arg_type, std::string>)
			{
				const std::string value = std::get<col_id>(tuple);
				if (!value.empty())
				{
					return (SQLITE_OK == sqlite3_bind_text(statement, position, value.c_str(), value.size(), SQLITE_TRANSIENT));
				}
				return (SQLITE_OK == sqlite3_bind_null(statement, position));
			}
			else if constexpr (std::is_same_v<arg_type, blob_t>)
			{
				auto vec = std::get<col_id>(tuple);
				if (vec.empty())
				{
					//write null??? or just leave it so that, we would just bind null on empty vector
					return (SQLITE_OK == sqlite3_bind_null(statement, position));
				}
				return (SQLITE_OK == sqlite3_bind_blob(statement, position, vec.data(), vec.size(), SQLITE_TRANSIENT));
			}
			else if constexpr (std::is_null_pointer<arg_type>::value)
			{
				return (SQLITE_OK == sqlite3_bind_null(statement, position));
			}
			else if constexpr (std::is_same_v<arg_type, date_time_t>)
			{
				auto rep = nl::to_representation(std::get<col_id>(tuple));
				return (SQLITE_OK == sqlite3_bind_int64(statement, position, rep));
			}
			else if constexpr (std::is_same_v<arg_type, uuid>)
			{
				auto uuid = std::get<col_id>(tuple);
				return (SQLITE_OK == sqlite3_bind_blob(statement, position, &uuid, uuid.size(), SQLITE_TRANSIENT));
			}
			else if constexpr (std::is_enum_v<arg_type>) {
				auto en = static_cast<std::uint32_t>(std::get<col_id>(tuple));
				return (SQLITE_OK == sqlite3_bind_int(statement, position, en));
			}
			else
			{
				return false;
			}

		}

		//auto was having problems deducing the return type, hence the need for this ugliness 
		template<typename Arg_type, size_t count, typename database_stmt>
		inline std::tuple<Arg_type> handle_retrive(database_stmt statement)
		{
			const size_t col = (size_t)sqlite3_column_count(statement) - (count + 1);
			using arg_t = typename std::decay_t<Arg_type>;


			static_assert(is_database_type<arg_t>::value, "Type in tuple is not a valid database type");
			if constexpr (std::is_integral_v<arg_t>)
			{
				//64 bit intergers has a special download function in sqlite
				if constexpr (sizeof(arg_t) == sizeof(std::uint64_t))
				{
					return std::make_tuple(sqlite3_column_int64(statement, col));
				}
				return std::make_tuple(sqlite3_column_int(statement, col));
			}else if constexpr (std::is_floating_point_v<arg_t>)
			{
				if constexpr (std::is_same_v<arg_t, float>)
				{
					//sqlite uses REAL type as double, need to safely cast, removes annoying warnings
					return std::make_tuple(static_cast<arg_t>(sqlite3_column_double(statement, col)));
				}
				return std::make_tuple(sqlite3_column_double(statement, col));
			}
			else if constexpr (std::is_same_v<arg_t, blob_t>)
			{
				const blob_t::value_type* val_ptr = static_cast<const blob_t::value_type*>(sqlite3_column_blob(statement, col));
				if (val_ptr)
				{
					const size_t size = sqlite3_column_bytes(statement, col);
					blob_t vec(size);
					std::copy(val_ptr, val_ptr + size, vec.begin());
					return std::make_tuple(std::move(vec));
				}
				return std::make_tuple(blob_t{});
			}
			//dealing with text and all the diffrenent forms lol
			//only handling char8 for now
			else if constexpr (std::is_same_v<arg_t, std::string>)
			{
				const char* txt = (const char*)(sqlite3_column_text(statement, col));
				if (txt)
				{
					return std::make_tuple(std::string(txt));
				}
				return std::make_tuple(std::string{});
			}
			else if constexpr (std::is_same_v<arg_t, date_time_t>)
			{
				auto rep = sqlite3_column_int64(statement, col);
				return std::make_tuple(from_representation((clock::duration::rep)rep));
			}
			else if constexpr (std::is_same_v<arg_t, uuid>)
			{
				const blob_t::value_type* val_ptr = static_cast<const blob_t::value_type*>(sqlite3_column_blob(statement, col));
				if (val_ptr)
				{
					const size_t size = sqlite3_column_bytes(statement, col);
					if (size == 16) //128 bit ids
					{
						nl::uuid id;
						std::copy(val_ptr, val_ptr + size, id.begin());
						return std::make_tuple(std::move(id));
					}
				}
				return std::make_tuple(nl::uuid(boost::uuids::nil_uuid()));
			}
			else if constexpr (std::is_enum_v<arg_t>) {
				auto en = static_cast<arg_t>(sqlite3_column_int(statement, col));
				return std::make_tuple(en);
			}

			else if constexpr (std::is_same_v<arg_t, std::basic_string<wchar_t>>)
			{assert(false && "no support for wide characters");}
			else if constexpr (std::is_same_v<arg_t, std::basic_string<char16_t>>)
			{assert(false && "no support for UTF-16");}
			else if constexpr (std::is_same_v<arg_t, std::basic_string<char32_t>>)
			{assert(false && "no support for UTF-32");}
			else { return std::tuple<arg_t>{}; /*should not reach here, lol*/ }

		}

		template<size_t count>
		class loop
		{
		public:
			template<typename relation_t, typename buffer_t>
			static void do_buffer_write(buffer_t& buffer, typename relation_t::tuple_t& tuple)
			{
				loop<count - 1>::template do_buffer_write<relation_t, buffer_t>(buffer, tuple);
				buffer.write(std::get<count>(tuple));
			}

			template<typename relation_t, typename buffer_t>
			static auto do_buffer_read(buffer_t& buffer)
			{
				auto t0 = loop<count - 1>::template do_buffer_read<relation_t, buffer_t>(buffer);
				using arg_type = typename std::tuple_element_t<count, typename relation_t::tuple_t>;
				arg_type object;
				buffer.read(object);
				return std::tuple_cat(t0, std::make_tuple(object));
			}

			template<typename tuple_t, template<typename> typename tbuffer_t>
			static void do_tbuffer_write(tuple_t& tuple, tbuffer_t<tuple_t>& buffer) {
				loop<count - 1>::template do_tbuffer_write<tuple_t, tbuffer_t>(tuple, buffer);
				if (buffer.get_state()[count]) {
					buffer.write(std::get<count>(tuple));
				}
			}

			template<typename tuple_t, template<typename> typename tbuffer_t>
			static void do_tbuffer_read(tuple_t& tuple, tbuffer_t<tuple_t>& buffer) {
				loop<count - 1>::template do_tbuffer_read<tuple_t, tbuffer_t>(tuple, buffer);
				if (buffer.get_state()[count]) {
					using arg_type = typename std::tuple_element_t<count, tuple_t>;
					arg_type object;
					buffer.read(object);
					std::get<count>(tuple) = std::move(object);
				}
			}

			template<typename database_stmt, typename tuple_t>
			static bool do_bind(database_stmt statement, const tuple_t& tuple)
			{
				bool b = handle_bind<count>(statement, tuple);
				bool b2 = loop<count - 1>::do_bind(statement, tuple);
				return (b2 && b);
			}

			template<typename database_stmt_t, typename tuple_t, typename parameter_array_t>
			static bool do_bind_para(database_stmt_t statement, const tuple_t& tuple, const parameter_array_t& parray)
			{
				bool b = handle_bind_para<count>(statement, tuple, parray);
				bool b2 = loop<count - 1>::do_bind_para(statement, tuple, parray);
				return (b && b2);
			}


			//tuple <col0, col1, col2>
			//database <col0, col1, col2>
			//loop starts from the tuple size - 1, inorder to get to 0,
			//col therefore needs to be 0 on first iteration, so subract tuple_size from loop count + 1
			// so that cat would be col0 ,col1 col2

			template<typename tuple_t, typename database_stmt_t>
			static auto do_retrive(database_stmt_t statement)
			{
				constexpr size_t col = (std::tuple_size_v<tuple_t> - (count + 1));
				using arg_type = std::tuple_element_t<col, tuple_t>;

				auto t = handle_retrive<arg_type, count>(statement);
				auto t2 = loop<count - 1>::template do_retrive<tuple_t>(statement);
				return std::tuple_cat(std::move(t), std::move(t2));
			}

			//runs a function for each element in the tuple 
			//accept a visitor
			template<typename tuple_t>
			static void accept(base_visitor& guest, tuple_t& tuple)
			{
				constexpr size_t col = (std::tuple_size_v<tuple_t> - (count + 1));
				using arg_type = std::tuple_element_t<col, tuple_t>;
				if (nl::visitor<arg_type, void> * visitor_ptr = dynamic_cast<nl::visitor<arg_type, void>*>(&guest))
				{
					visitor_ptr->visit(std::get<col>(tuple), col);
				}
				loop<count - 1>::template accept(guest, tuple);
			}

			template<typename tuple_t, typename T>
			static void get_in(const tuple_t& tuple, T& put_in, size_t column)
			{
				assert((column < std::tuple_size_v<tuple_t>) && "Invalid \'column\' in get_in");
				constexpr size_t col = (std::tuple_size_v<tuple_t> -(count + 1));
				using arg_type = std::tuple_element_t<col, tuple_t>;
				using put_type = std::decay_t<T>;
				if (col == column)
				{
					static_assert(std::is_convertible_v<arg_type, put_type>,
					"Value type is not convertible to the type stored in the tuple");
					put_in = std::get<col>(tuple);
					return;
				}
				loop<count - 1>::get_in(tuple, put_in, column);
			}

			template<typename tuple_t>
			static auto get_as_string(const tuple_t& tuple, size_t column) -> std::string 
			{
				assert((column < std::tuple_size_v<tuple_t>) && "Invalid \'column\' in get_as_string");
				constexpr size_t col = (std::tuple_size_v<tuple_t> -(count + 1));
				using arg_type = std::decay_t<std::tuple_element_t<col, tuple_t>>;
				if (col == column)
				{
					if constexpr (std::is_same_v<date_time_t, arg_type>)
					{
						return fmt::format("{}", nl::to_string_date(std::get<col>(tuple)));
					}else if constexpr (std::is_integral_v<arg_type>){
						return fmt::format("{:d}", std::get<col>(tuple));
					}
					else if constexpr (std::is_floating_point_v<arg_type>){
						return fmt::format("{:.4f}", std::get<col>(tuple));
					}
					else if constexpr (std::is_same_v<nl::uuid, arg_type>) {
						return fmt::format("{:q}", std::get<col>(tuple));
					}
					else if constexpr(fmt::is_formattable<arg_type>::value) {
						return fmt::format("{}", std::get<col>(tuple));
					}
					else{
						//null type lol 
						return std::string();
					}
				}
				return loop<count - 1>::get_as_string(tuple, column);
			}

			template<typename tuple_t>
			static auto get_as_string_ref(const tuple_t& tuple, size_t column) -> std::string
			{
				assert((column < std::tuple_size_v<tuple_t>) && "Invalid \'column\' in get_as_string");
				constexpr size_t col = (std::tuple_size_v<tuple_t> -(count + 1));
				using arg_type = std::decay_t<typename std::tuple_element_t<col, tuple_t>::type>;

				if (col == column)
				{
					if constexpr (std::is_same_v<date_time_t, arg_type>)
					{
						return fmt::format("{}", nl::to_string_date(std::get<col>(tuple)));
					}
					else if constexpr (std::is_integral_v<arg_type>) {
						return fmt::format("{:d}", std::get<col>(tuple).get());
					}
					else if constexpr (std::is_floating_point_v<arg_type>) {
						return fmt::format("{:.4f}", std::get<col>(tuple).get());
					}
					else if constexpr (std::is_same_v<nl::uuid, arg_type>) {
						return fmt::format("{:q}", std::get<col>(tuple).get());
					}
					else if constexpr (fmt::is_formattable<arg_type>::value) {
						return fmt::format("{}", std::get<col>(tuple).get());
					}
					else {
						//null type lol 
						return std::string();
					}
				}
				return loop<count - 1>::get_as_string_ref(tuple, column);
			}



			template<typename tuple_t, typename T>
			static void put_value_in(tuple_t& tuple, const  T& value, size_t column)
			{
				assert((column < std::tuple_size_v<tuple_t>) && "Invalid \'column\' in put_value_in");
				constexpr size_t col = (std::tuple_size_v<tuple_t> -(count + 1));
				using arg_type = std::decay_t<std::tuple_element_t<col, tuple_t>>;
				if (col == column)
				{
					//might work? static_cast might be a problem
					if constexpr (std::is_convertible_v<T, arg_type>){
						std::get<col>(tuple) = static_cast<const arg_type&>(value);
						return;
					}
				}
				return loop<count - 1>::put_value_in(tuple, value, column);
			}

			template<typename tuple_t, typename variant_t>
			static void set_from_variant(tuple_t& tuple, const variant_t& variant, size_t column) 
			{
				assert((column < std::tuple_size_v<tuple_t>) && "Invalid \'column\' in set_from_varient");
				constexpr size_t col = (std::tuple_size_v<tuple_t> -(count + 1));
				using arg_type = std::decay_t<std::tuple_element_t<col, tuple_t>>;
				if (col == column){
					//if the type is what the variants is holding 
					if (std::holds_alternative<arg_type>(variant)) {
						nl::row_value<col>(tuple) = std::get<arg_type>(variant);
						return;
					}else return; //error type mismatch in the variant, should throw an exception
				}
				return loop<count - 1>::set_from_variant(tuple, variant, column);
			}


			//true assending, false desending
			template<typename relation_t, bool order = true>
			static void quick_sort_column(relation_t & relation, size_t column)
			{
				assert((column < std::tuple_size_v<typename relation_t::tuple_t>) && "Invalid \'column\' in quick_sort_column");
				constexpr size_t col = (std::tuple_size_v<typename relation_t::tuple_t> -(count + 1));
				using arg_type = std::decay_t<std::tuple_element_t<col, typename relation_t::tuple_t>>;
				if constexpr (order)
				{
					if (col == column){
						relation.template quick_sort<col, nl::order_asc<arg_type>>();
						return;
					}
					return loop<count - 1>::template quick_sort_column<relation_t, true>(relation, column);
				}
				else{
					if (col == column){
						relation.template quick_sort<col, nl::order_dec<arg_type>>();
						return;
					}
					return loop<count - 1>::template quick_sort_column<relation_t, false>(relation, column);
				}
			}
		};

		template<>
		class loop<0>
		{
		public:
			template<typename relation_t, typename buffer_t>
			static void do_buffer_write(buffer_t& buffer, typename relation_t::tuple_t& tuple)
			{
				buffer.write(std::get<0>(tuple));
			}
			//TODO: const correct the buffer
			template<typename relation_t, typename buffer_t>
			static auto do_buffer_read(buffer_t& buffer)
			{
				using arg_type = typename std::tuple_element_t<0, typename relation_t::tuple_t>;
				arg_type object;
				buffer.read(object);
				return std::make_tuple(object);
			}

			template<typename tuple_t, template<typename> typename tbuffer_t>
			static void do_tbuffer_write(tuple_t& tuple, tbuffer_t<tuple_t>& buffer) {
				if (buffer.get_state()[0]) {
					buffer.write(std::get<0>(tuple));
				}
			}

			template<typename tuple_t, template<typename> typename tbuffer_t>
			static void do_tbuffer_read(tuple_t& tuple, tbuffer_t<tuple_t>& buffer) {
				if (buffer.get_state()[0]) {
					using arg_type = typename std::tuple_element_t<0, tuple_t>;
					arg_type object;
					buffer.read(object);
					std::get<0>(tuple) = std::move(object);
				}
			}


			template<typename database_stmt, typename tuple_t>
			static bool do_bind(database_stmt statement, const tuple_t& tuple)
			{
				return handle_bind<0>(statement, tuple);
			}

			template<typename tuple_t, typename database_stmt_t>
			static auto do_retrive(database_stmt_t statement)
			{

				constexpr size_t col = (std::tuple_size_v<tuple_t> - 1);
				using arg_type = std::tuple_element_t<col, tuple_t>;

				return handle_retrive<arg_type, 0>(statement);
			}

			template<typename database_stmt_t, typename tuple_t, typename parameter_array_t>
			static bool do_bind_para(database_stmt_t statement, const tuple_t& tuple, const parameter_array_t& parray)
			{
				return  handle_bind_para<0>(statement, tuple, parray);
			
			}

			template<typename tuple_t>
			static void accept(base_visitor& guest, tuple_t& tuple)
			{
				constexpr size_t col = (std::tuple_size_v<tuple_t> - 1);
				using arg_type = std::tuple_element_t<col, tuple_t>;
				if (nl::visitor<arg_type, void> * visitor_ptr = dynamic_cast<nl::visitor<arg_type, void>*>(&guest))
				{
					visitor_ptr->visit(std::get<col>(tuple), col);
				}
			}

			template<typename tuple_t, typename T>
			static void get_in(const tuple_t& tuple, T& put_in, size_t column)
			{
				assert((column < std::tuple_size_v<tuple_t>) && "Invalid \'column\' in get_in");
				constexpr size_t col = (std::tuple_size_v<tuple_t> - 1);
				using arg_type = std::tuple_element_t<col, tuple_t>;
				if (col == column)
				{
					put_in = std::get<col>(tuple);
					return;
				}
			}


			template<typename tuple_t>
			static auto get_as_string(const tuple_t& tuple, size_t column) -> std::string
			{
				assert((column < std::tuple_size_v<tuple_t>) && "Invalid \'column\' in get_as_string");
				constexpr size_t col = (std::tuple_size_v<tuple_t> - 1);
				using arg_type = std::decay_t<std::tuple_element_t<col, tuple_t>>;
				if (col == column)
				{
					if constexpr (std::is_same_v<date_time_t, arg_type>)
					{
						return fmt::format("{}:{}", nl::to_string_date(std::get<col>(tuple)),
							nl::to_string_time(std::get<col>(tuple)));
					}
					else if constexpr (std::is_integral_v<arg_type>) {
						return fmt::format("{:d}", std::get<col>(tuple));
					}
					else if constexpr (std::is_floating_point_v<arg_type>) {
						return fmt::format("{:.4f}", std::get<col>(tuple));
					}
					else if constexpr (std::is_same_v<nl::uuid, arg_type>) {
						return fmt::format("{:q}", std::get<col>(tuple));
					}
					else if constexpr (fmt::is_formattable<arg_type>::value) {
						return fmt::format("{}", std::get<col>(tuple));
					}
					else {
						//null type lol 
						return std::string();
					}
				}
				return std::string();
			}

			template<typename tuple_t>
			static auto get_as_string_ref(const tuple_t& tuple, size_t column) -> std::string
			{
				assert((column < std::tuple_size_v<tuple_t>) && "Invalid \'column\' in get_as_string");
				constexpr size_t col = (std::tuple_size_v<tuple_t> -1);
				using arg_type = std::decay_t<typename std::tuple_element_t<col, tuple_t>::type>;
				if (col == column)
				{
					if constexpr (std::is_same_v<date_time_t, arg_type>)
					{
						return fmt::format("{}:{}", nl::to_string_date(std::get<col>(tuple)),
							nl::to_string_time(std::get<col>(tuple)).get());
					}
					else if constexpr (std::is_integral_v<arg_type>) {
						return fmt::format("{:d}", std::get<col>(tuple).get());
					}
					else if constexpr (std::is_floating_point_v<arg_type>) {
						return fmt::format("{:.4f}", std::get<col>(tuple).get());
					}
					else if constexpr (std::is_same_v<nl::uuid, arg_type>) {
						return fmt::format("{:q}", std::get<col>(tuple).get());
					}
					else if constexpr (fmt::is_formattable<arg_type>::value) {
						return fmt::format("{}", std::get<col>(tuple).get());
					}
					else {
						//null type lol 
						return std::string();
					}
				}
				return std::string();
			}

			template<typename tuple_t, typename T>
			static void put_value_in(tuple_t& tuple, const  T& value, size_t column)
			{
				assert((column < std::tuple_size_v<tuple_t>) && "Invalid \'column\' in put_value_in");
				constexpr size_t col = (std::tuple_size_v<tuple_t> - 1);
				using arg_type = std::decay_t<std::tuple_element_t<col, tuple_t>>;
				if (col == column)
				{
					//might work? static_cast might be a problem
					if constexpr (std::is_convertible_v<T, arg_type>) {
						std::get<col>(tuple) = static_cast<const arg_type&>(value);
						return;
					}
				}
			}

			template<typename tuple_t, typename variant_t>
			static void set_from_variant(tuple_t& tuple, const variant_t& variant, size_t column)
			{
				assert((column < std::tuple_size_v<tuple_t>) && "Invalid \'column\' in set_from_varient");
				constexpr size_t col = (std::tuple_size_v<tuple_t> - 1);
				using arg_type = std::decay_t<std::tuple_element_t<col, tuple_t>>;
				if (col == column) {
					//if the type is what the variants is holding 
					if (std::holds_alternative<arg_type>(variant)) {
						nl::row_value<col>(tuple) = std::get<arg_type>(variant);
						return;
					}
				}
			}


			template<typename relation_t, bool order = true>
			static void quick_sort_column(relation_t& relation, size_t column)
			{
				assert((column < std::tuple_size_v<typename relation_t::tuple_t>) && "Invalid \'column\' in quick_sort_column");
				constexpr size_t col = (std::tuple_size_v<typename relation_t::tuple_t> - 1);
				using arg_type = std::decay_t<std::tuple_element_t<col, typename relation_t::tuple_t>>;
				if constexpr (order)
				{
					if (col == column) {
						relation.template quick_sort<col, nl::order_asc<arg_type>>();
						return;
					}
				}
				else {
					if (col == column) {
						relation.template quick_sort<col, nl::order_dec<arg_type>>();
						return;
					}
				}
			}

		};

		template<size_t I, typename T, typename S>
		struct comp_tuple_with_value
		{
			bool operator()(const T& t, const S& s) const { return std::get<I>(t) < s; }
			bool operator()(const S& s, const T& t) const { return s < std::get<I>(t); }
		};

		template<typename rel_type, typename Compare, typename execution_policy = std::execution::sequenced_policy, std::enable_if_t<std::is_same_v<typename rel_type::container_t, std::list<typename rel_type::tuple_t>>, int> = 0>
		void sort_par(rel_type & rel, Compare comp, execution_policy policy = std::execution::seq)
		{
			(void)policy;
			rel.sort(comp);
		}

		template<typename rel_type, typename Compare, typename execution_policy = std::execution::sequenced_policy,  std::enable_if_t<std::is_same_v<typename rel_type::container_t, std::vector<typename rel_type::tuple_t>>, int> = 0>
		void sort_par(rel_type & rel, Compare comp, execution_policy policy = std::execution::seq)
		{
			std::sort(policy, rel.begin(), rel.end(), comp);
		}
		
		template<typename rel_type, typename Compare, std::enable_if_t<std::is_same_v<typename rel_type::container_t, std::list<typename rel_type::tuple_t>>, int> = 0>
		void sort(rel_type & rel, Compare comp){
			rel.sort(comp);
		}

		template<typename rel_type, typename Compare, std::enable_if_t<std::is_same_v<typename rel_type::container_t, std::vector<typename rel_type::tuple_t>>, int> = 0>
		void sort(rel_type & rel, Compare comp){
			std::sort(rel.begin(), rel.end(), comp);
		}

		//R is the super class
		//T is the subclass

		//type traits, TODO: Move all these to the the nl_type_traits file
		

		template<class R, class T>
		class is_converatable_to_relation
		{
			typedef char small_t;
			class big_t { char dummy[2]; };
			static small_t test(const T&);
			static big_t test(...);
			static R make_t();
		public:
			enum {exisit = sizeof(test(make_t())) == sizeof(small_t), same_type = false};
		};

		template<class T>
		class is_converatable_to_relation<T,T>{
		public:
			enum {exisit = 1, same_type = 1};
		};

		template<typename R, typename T>
		struct _derived_from_relation{
			enum
			{
				value = (is_converatable_to_relation<const std::decay_t<T>*, const std::decay_t<R>*>::exisit &&
				!is_converatable_to_relation<const std::decay_t<R>*, void*>::same_type)
			};
		};
		template<typename T> class has_base_relation { 
			public: 
				enum { value = _derived_from_relation<base_relation, T>::value}; 
		};


		//figure out a better way to do this.. lol i relaised that this is way better, this "is_convertible_v" was not here before
		//i just learnt it, but i am so proud of the other ones i wrote up with andrei's loki libary implementation that i dont want to remove it
		template <typename T> class _is_relation { public:enum { value = std::is_convertible_v<T, base_relation> }; };
		template<typename T, typename relation>
		class _is_relation_row { 
		public: 
			enum { value = std::conjunction_v<_is_relation<relation>, std::is_same<T, typename relation::row_t>> };
		};


		template<typename T>
		using is_relation = _is_relation<std::decay_t<T>>;

		template<typename T, typename rel>
		using is_relation_row = _is_relation_row<std::decay_t<T>, std::decay_t<rel>>;

		template<typename T>
		constexpr bool is_relation_v = is_relation<T>::value;
		template<typename T, typename relation>
		constexpr bool is_relation_row_v = is_relation_row<T, relation>::value;


		//redundant?? yes but i am keeping it
		template<typename...T> class _is_relation<vector_relation<T...>> { public:enum { value = true }; };
		template<typename...T> class _is_relation<list_relation<T...>> { public:enum { value = true }; };
		template<typename...T> class _is_relation<set_relation<T...>> { public:enum { value = true }; };


		template<typename T>
		struct is_linear_relation{
			enum {
				value = std::is_same_v<typename T::container_tag, linear_relation_tag>
			};
		};

		template<typename T>
		struct is_map_relation{
			enum {
				value = std::is_same_v<typename T::container_tag, map_relation_tag>
			};
		};

		template<typename T>
		struct is_hash_relation
		{
			enum {
				value = std::is_same_v<typename T::container_tag, hash_relation_tag>
			};
		};

		template<typename T>
		struct is_set_relation
		{
			enum {
				value = std::is_same_v<typename T::container_tag, set_relation_tag>
			};
		};

		//get relation column type as string
		namespace helper
		{
			static constexpr std::uint32_t FRONT_SIZE = sizeof("nl::detail::helper::get_type_name_helper<") - 1u;
			static constexpr std::uint32_t BACK_SIZE = sizeof(">::get_type_name") - 1u;

			template<typename T>
			struct get_type_name_helper
			{
				static const char* get_type_name() {
					static constexpr size_t size = sizeof(__FUNCTION__) - FRONT_SIZE - BACK_SIZE;
					static char type_name[size] = {};
					std::memcpy(type_name, __FUNCTION__ + FRONT_SIZE, size - 1u);
					return type_name;
				}
			};
		}
	}

	template<typename T>
	inline const char* get_type_name()
	{
		return detail::helper::get_type_name_helper<T>::get_type_name();
	}

	template<typename relation1, typename relation2>
	class join_relation
	{
		typedef typename relation1::tuple_t rel_tuple1;
		typedef typename relation2::tuple_t rel_tuple2;
		typedef typename detail::append<rel_tuple1, rel_tuple2>::type new_relation_tuple;
	public:
		typedef nl::relation<typename relation1::template container_t<new_relation_tuple, nl::alloc_t<new_relation_tuple>>> type;
	};

}
