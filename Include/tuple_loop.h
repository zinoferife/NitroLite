#pragma once
#include <tuple>
#include <fmt/format.h>
#include <SQLite/sqlite3.h>
namespace nl
{
	template<size_t...I>
	using select = std::index_sequence<I...>;
	template<typename relation>
	using select_all = std::make_index_sequence<relation::column_count>;

	template<size_t I, typename row_t>
	inline auto row_value(const row_t& tuple)
	{
		return std::get<I>(tuple);
	}
	template<size_t I, typename row_t>
	inline auto& row_value(row_t& tuple)
	{
		return std::get<I>(tuple);
	}
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
	constexpr size_t j_ = std::tuple_size_v<tuple_t> + offset;

	using blob_t = std::vector<std::uint8_t>;


	namespace detail
	{
		//sqlite only wants 5 types: integral, floating_point, string, blob and null
		//in nitrolite blob is std::vector<uint8_t> 
		//this type trait ensures we are only operating in those types
		template<typename tuple_t, class T> struct index_of;

		template<typename U, typename T>
		struct index_of<std::tuple<U>, T>{enum {value = -1};};

		template<typename T>
		struct index_of<std::tuple<T>, T>{enum { value = 0 };};

		template<typename T, typename...U>
		struct index_of<std::tuple<T, U...>, T>{enum {value = 0};};

		template<typename T, typename U, typename ...S>
		struct index_of<std::tuple<U, S...>, T>
		{
		private:
			enum {temp = index_of<std::tuple<S...>, T>::value};
		public:
			enum {value = temp == -1 ? -1 : 1 + temp };
		};

		template<typename T>
		class is_database_type
		{
			using special_types = std::tuple<std::string, blob_t, nullptr_t>;
		public:
			enum {value = (std::is_integral_v<T> || std::is_floating_point_v<T> || index_of<special_types, T>::value >= 0) };
		};


		template<size_t count, typename database_stmt, typename tuple_t>
		inline bool handle(database_stmt statement, const tuple_t& tuple)
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
			else
			{
				return false;
			}

		}

		template<size_t count,typename database_stmt_t, typename tuple_t, typename para_array_t>
		inline bool handle_para(database_stmt_t statement, const tuple_t& tuple, const para_array_t& array)
		{
			constexpr size_t col_id = std::tuple_size_v<tuple_t> - (count + 1);
			using arg_type = typename std::decay_t<std::tuple_element_t<col_id, tuple_t>>;
			using array_value_type = typename std::decay_t<typename para_array_t::value_type>;
			static_assert(is_database_type<arg_type>::value, "Tuple type is not a valid database type");
			size_t position = -1;
			
			if constexpr (std::is_same_v<array_value_type, std::string> || std::is_convertible_v<array_value_type, std::string>)
			{
				const std::string p_name = fmt::format(":{}", array[col_id]);
				position = sqlite3_bind_parameter_index(statement, p_name.c_str());
			}
			else if constexpr (std::is_same_v<array_value_type, size_t> || std::is_convertible_v<array_value_type, size_t>)
			{
				position = array[col_id];
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
			else
			{
				return false;
			}

		}

		//auto was having problems deducing the return type, hence the need for this ugliness 
		template<typename Arg_type, size_t count, typename database_stmt>
		inline std::tuple<Arg_type> handle_col(database_stmt statement)
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
				blob_t::value_type* val_ptr = static_cast<blob_t::value_type*>(sqlite3_column_blob(statement, col));
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

			template<typename database_stmt, typename tuple_t>
			static bool do_insert(database_stmt statement, const tuple_t& tuple)
			{
				bool b = handle<count>(statement, tuple);
				bool b2 = loop<count - 1>::do_insert(statement, tuple);
				return (b2 && b);
			}

			template<typename database_stmt_t, typename tuple_t, typename parameter_array_t>
			static bool do_insert_para(database_stmt_t statement, const tuple_t& tuple, const parameter_array_t& parray)
			{
				bool b = handle_para<count>(statement, tuple, parray);
				bool b2 = loop<count - 1>::do_insert_para(statement, tuple, parray);
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

				auto t = handle_col<arg_type, count>(statement);
				auto t2 = loop<count - 1>::template do_retrive<tuple_t>(statement);
				return std::tuple_cat(std::move(t), std::move(t2));
			}


			//runs a function for each element in the tuple 
			template<typename tuple_t, template<typename> typename Func, typename... Args>
			static void visit(tuple_t& tuple, Func<> function, Args... extra_args)
			{
				constexpr size_t col = (std::tuple_size_v<tuple_t> - (count + 1));
				using arg_type = std::tuple_element_t<col, tuple_t>;
				auto visit_ = std::bind(function<arg_type>, std::placeholders::_1, std::placeholders::_2, extra_args...);
				visit_(std::get<col>(tuple), col);
				loop<count - 1>::visit(tuple, function);
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
			//TODO: const correct the buffer
			template<typename relation_t, template<typename> typename buffer_t>
			static auto do_buffer_read(buffer_t<relation_t>& buffer)
			{
				using arg_type = typename std::tuple_element_t<0, typename relation_t::tuple_t>;
				arg_type object;
				buffer.read(object);
				return std::make_tuple(object);
			}
			template<typename database_stmt, typename tuple_t>
			static bool do_insert(database_stmt statement, const tuple_t& tuple)
			{
				return handle<0>(statement, tuple);
			}

			template<typename tuple_t, typename database_stmt_t>
			static auto do_retrive(database_stmt_t statement)
			{

				constexpr size_t col = (std::tuple_size_v<tuple_t> - 1);
				using arg_type = std::tuple_element_t<col, tuple_t>;

				return handle_col<arg_type, 0>(statement);
			}

			template<typename database_stmt_t, typename tuple_t, typename parameter_array_t>
			static bool do_insert_para(database_stmt_t statement, const tuple_t& tuple, const parameter_array_t& parray)
			{
				return  handle_para<0>(statement, tuple, parray);
			
			}

			template<typename tuple_t, typename Func>
			static void visit(tuple_t& tuple, Func function)
			{
				constexpr size_t col = (std::tuple_size_v<tuple_t> - 1 );
				function(std::get<col>(tuple), col);
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
		
		template<typename rel, typename Compare, typename execution_policy = std::execution::sequenced_policy, std::enable_if_t<std::is_same_v<typename rel::container_t, std::list<typename rel::tuple_t>>, int> = 0>
		void sort_par(rel & rel, Compare comp, execution_policy policy = std::execution::seq)
		{
			(void)policy;
			rel.sort(comp);
		}

		template<typename rel, typename Compare, typename execution_policy = std::execution::sequenced_policy,  std::enable_if_t<std::is_same_v<typename rel::container_t, std::vector<typename rel::tuple_t>>, int> = 0>
		void sort_par(rel & rel, Compare comp, execution_policy policy = std::execution::seq)
		{
			std::sort(policy, rel.begin(), rel.end(), comp);
		}
		
		template<typename rel, typename Compare, std::enable_if_t<std::is_same_v<typename rel::container_t, std::list<typename rel::tuple_t>>, int> = 0>
		void sort(rel & rel, Compare comp){
			rel.sort(comp);
		}

		template<typename rel, typename Compare, std::enable_if_t<std::is_same_v<typename rel::container_t, std::vector<typename rel::tuple_t>>, int> = 0>
		void sort(rel & rel, Compare comp){
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
		class is_converatable_to_relation<T,T>{
		public:
			enum {exisit = 1, same_type = 1};
		};

		template<typename R, typename T>
		struct _derived_from_relation{
			enum
			{
				value = (is_converatable_to_relation<const std::decay_t<T>*, const std::decay_t<R>*>::exisit && !is_converatable_to_relation<const std::decay_t<R>*, void*>::same_type)
			};
		};
		template<typename T> class has_base_relation { 
			public: 
				enum { value = _derived_from_relation<base_relation, T>::value}; 
		};


		//figure out a better way to do this.. lol i relaised that this is way better, this "is_convertible_v" was not here before
		//i just learnt it, but i am so proud of the other ones i wrote up with andrei's loki libary implementation that i dont want to remove it
		template <typename T> class _is_relation { public:enum { value = std::is_convertible_v<T, base_relation> }; };
		template<typename T>
		using is_relation = _is_relation<std::decay_t<T>>;
		template<typename T>
		constexpr bool is_relation_v = is_relation<T>::value;


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
	}
}
