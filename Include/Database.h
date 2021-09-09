#pragma once
#include <string>
#include <string_view>
#include <utility>
#include <cassert>
#include <SQLite/sqlite3.h>

#include "relation.h"
#include "query.h"
/*
	NitroLite uses sqlite for database connectivity
	this class represents a single connection to a database
	it exposes functions to prepare a query, retrive, insert and update data as relations
	also to delete data based on a col in the relation, delete<col_id>(index, relation) and delete<col_id>(index, row, relation)
	both deletes all the data in the database with that column value 
*/


namespace nl
{
	namespace detail
	{
		template<size_t count>
		class loop;
	};

	struct sql_extension_func_aggregate
	{
		typedef void (*FFunction)(sqlite3_context*, int, sqlite3_value**);
		typedef void(*FStep)(sqlite3_context*, int, sqlite3_value**);
		typedef void(*FFinal)(sqlite3_context*);
		static std::int32_t sTextEncoding;

		std::string fName{};
		std::int32_t fArgCount{ 0 };
		void* fUserData{ nullptr };
		FFunction fFunc{ nullptr };
		FStep fStep{ nullptr };
		FFinal fFinal{ nullptr };

		sql_extension_func_aggregate() = default;
	};

	class database_connection
	{
	//callbacks for sqlite
	public:
		typedef std::vector<sqlite3_stmt*> statements;
		typedef statements::size_type statement_index;
		typedef int(*exeu_callback)(void* arg, int col, char** rol_val, char** col_names);
		typedef int(*commit_callback)(void* arg);
		typedef int(*rollback_callback)(void* arg);
		typedef int(*trace_callback)(std::uint32_t traceType, void* UserData, void* statement, void* traceData);
		typedef int(*busy_callback)(void* arg, int i);
		typedef int(*progress_callback)(void* arg);
		typedef int(*auth)(void* arg, int eventCode, const char* evt_1, const char* evt_2, const char* database_name, const char* tig_view_name);
		bool roll_back{false};
		
		//pre-defined statement
		enum stmt : size_t {
			begin = 0,
			begin_immediate,
			end,
			rollback
		};


	public:
		enum
		{
			BADSTMT = size_t(-1)
		};

		database_connection();
		database_connection(const std::string_view& database_file);
		database_connection(const database_connection&& connection) noexcept;
		database_connection& operator=(const database_connection&&) noexcept;

		~database_connection();

		statement_index prepare_query(const std::string& query);
		statement_index prepare_query(const nl::query& query);
		
		inline const statements& get_statements() const { return m_statements; }
		inline const std::string& get_error_msg() const { return m_error_msg; }
		void remove_statement(nl::database_connection::statement_index index);
		bool is_open() const { return (m_database_conn != nullptr); }
		bool connect(const std::string_view& file);
		
	public:
		bool set_commit_handler(commit_callback callback, void* UserData);
		bool set_trace_handler(trace_callback callback, std::uint32_t mask, void* UserData);
		bool set_busy_handler(busy_callback callback, void* UserData);
		bool set_auth_handler(auth callback, void* UserData);
		void set_progress_handler(progress_callback callback, void* UserData, int frq);
		bool register_extension(const sql_extension_func_aggregate& ext);
	
	//template functions cover
	public:	
		template<typename relation>
		relation retrive(statement_index index)
		{
			return do_query_retrive<relation>(index);
		}

		template<typename relation>
		bool insert(statement_index index, const relation& rel)
		{
			return do_query_insert(index, rel);
		}

		template<typename relation, typename...T>
		bool insert_params(statement_index index, const relation& rel ,T... args)
		{
			return do_query_insert_para(index, rel, args...);
		}

		bool exec_once(statement_index index);
		
		template<typename relation_t, typename...T>
		bool upate(statement_index index, typename relation_t::tuple_t& row, T... args)
		{
			return do_update_para(index, row, args...);
		}



	//template functions
	private:
		template<typename relation>
		relation do_query_retrive(statement_index index)
		{
			assert(index < m_statements.size() && "Invalid statement index");
			static_assert(nl::detail::has_base_relation<relation>::value, "relation is not a valid relation type");
			statements::reference statement = m_statements[index];
			if (sqlite3_step(m_statements[stmt::begin]) == SQLITE_DONE)
			{
				if (statement)
				{
					relation rel;
					std::insert_iterator<typename relation::container_t> insert(rel, rel.begin());
					constexpr size_t size = std::tuple_size_v<typename relation::tuple_t> -1;
					while ((sqlite3_step(statement) == SQLITE_ROW))
					{
						if constexpr (nl::detail::is_map_relation<relation>::value)
						{
							insert = nl::detail::loop<size>::template do_retrive< typename relation::tuple_t>(statement);
						}
						else if constexpr (nl::detail::is_linear_relation<relation>::value)
						{
							std::back_insert_iterator<typename relation::container_t> back_insert(rel);
							back_insert = nl::detail::loop<size>::template do_retrive<typename relation::tuple_t>(statement);
						}
					}
					if (sqlite3_errcode(m_database_conn) == SQLITE_DONE)
					{
						if (sqlite3_reset(statement) == SQLITE_SCHEMA) {
							m_error_msg = sqlite3_errmsg(m_database_conn);
							return relation{};
						}
						if (sqlite3_step(m_statements[roll_back ? stmt::rollback : stmt::end]) == SQLITE_DONE)
						{
							sqlite3_reset(m_statements[begin]);
							sqlite3_reset(m_statements[roll_back ? stmt::rollback : stmt::end]);
							return std::move(rel);
						}
					}
				}
			}
			m_error_msg = sqlite3_errmsg(m_database_conn);
			sqlite3_reset(statement);
			return relation{};
		}

		template<typename relation>
		bool do_query_insert(statement_index index, const relation& rel)
		{
			static_assert(nl::detail::has_base_relation<relation>::value, "relation is not a valid relation type");
			assert(index < m_statements.size() && "Invalid statement index");
			statements::reference statement = m_statements[index];
			constexpr size_t size = std::tuple_size_v<typename relation::tuple_t> - 1;
			
			//loop::do_insert should actually be called do_bind, it binds values to insert statements 
			if (sqlite3_step(m_statements[begin_immediate]) == SQLITE_DONE){
				for (auto& tuple : rel) {
					if (!nl::detail::loop<size>::do_bind(statement, tuple)) {
						m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
						return false;
					}
					if (sqlite3_step(statement) == SQLITE_DONE) {
							if(sqlite3_reset(statement) == SQLITE_SCHEMA) {
								m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
								return false;
							}
					}
					else{
						m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
						return false;
					}
				}
				if (sqlite3_step(m_statements[roll_back ? stmt::rollback : stmt::end]) == SQLITE_DONE) {
					sqlite3_reset(m_statements[roll_back ? stmt::rollback : stmt::end]);
					sqlite3_reset(m_statements[begin_immediate]);
					return true;
				}
			}
			m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
			return false;
		}

		template<typename relation, typename S,  typename... T>
		bool do_query_insert_para(statement_index index, const relation& rel,const S& start , const T&...args)
		{
			static_assert(nl::detail::has_base_relation<relation>::value, "relation is not a valid relation type");
			assert(index < m_statements.size() && "Invalid statement index");
			const std::array<const S, sizeof...(T) + 1> parameters{start, args...};
			constexpr size_t size = std::tuple_size_v<typename relation::tuple_t> - 1;
			statements::reference statement = m_statements[index];

			if (sqlite3_step(m_statements[stmt::begin_immediate]) == SQLITE_DONE)
			{
				for (auto& tuple : rel){
					//loop::do_insert_para should acctually be called do_bind_para it binds not insert
					if (!nl::detail::loop<size>::do_bind_para(statement, tuple, parameters)){
						m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
						return false;
					}
					//execute the insert statement
					if (sqlite3_step(statement) == SQLITE_DONE){
						if (sqlite3_reset(statement) == SQLITE_SCHEMA)
						{
							m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
							return false;
						}
					}else{
						m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
						return false;
					}
				}
			}
			if (sqlite3_step(m_statements[roll_back ? stmt::rollback : stmt::end]) == SQLITE_DONE){
				sqlite3_reset(m_statements[roll_back ? stmt::rollback : stmt::end]);
				sqlite3_reset(m_statements[stmt::begin_immediate]);
				return true;
			}

			//error that is not caught, hopefully lool i dont know, but i hope sqlite would set an error
			if (sqlite3_errcode(m_database_conn) == SQLITE_ERROR)
			{
				m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
				return false;
			}
			return true;
		}

		template<typename relation_t, typename S, typename... T>
		bool do_update_para(statement_index index, typename relation_t::tuple_t& row, const S& start, const T&... args)
		{
			static_assert(nl::detail::has_base_relation<relation_t>::value, "relation is not a valid relation type");
			assert(index < m_statements.size() && "Invalid statement index");
			const std::array<const S, sizeof...(T) + 1> parameters{ start, args... };
			constexpr size_t size = std::tuple_size_v<typename relation::tuple_t> -1;
			statements::reference statement = m_statements[index];
			
			if(nl::detail::loop<size>::template do_bind_para(statement, row, parameters))
			{
				if (sqlite3_step(m_statements[stmt::begin_immediate]) == SQLITE_DONE)
				{
					if (sqlite3_step(statement) == SQLITE_DONE)
					{
						if (sqlite3_reset(statement) == SQLITE_SCHEMA)
						{
							m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
							return false;
						}
					}else{
						m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
						return false;
					}
				}
				if (sqlite3_step(m_statements[roll_back ? stmt::rollback : stmt::end]) == SQLITE_DONE) {
					sqlite3_reset(m_statements[roll_back ? stmt::rollback : stmt::end]);
					sqlite3_reset(m_statements[stmt::begin_immediate]);
					return true;
				}
				//error that is not caught, hopefully lool i dont know, but i hope sqlite would set an error
				if (sqlite3_errcode(m_database_conn) == SQLITE_ERROR)
				{
					m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
					return false;
				}
				return true;
			}
			m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
			return false;
		}

	
		
	
	
	private:
		//cannot copy or assign a database connection, every created on is a new connection, 
		//a database connection is a resource that is usually only one in a thread
		database_connection(const database_connection&) = delete;
		database_connection& operator=(const database_connection&) = delete;
		


		sqlite3* m_database_conn{nullptr};
		statements m_statements{};
		std::string m_error_msg{};
	};


};
