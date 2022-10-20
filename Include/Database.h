#pragma once
#include <string>
#include <string_view>
#include <utility>
#include <cassert>
#include <SQLite/sqlite3.h>
#include <unordered_map>
#include <filesystem>
#include <functional>

#include "relation.h"
#include "query.h"
#include "nl_database_exception.h"

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

	//database events
	using namespace std::literals::string_literals;
	using namespace std::literals::string_view_literals;
	enum database_evt
	{
		D_INSERT = SQLITE_INSERT,
		D_UPDATE = SQLITE_UPDATE,
		D_DEL = SQLITE_DELETE,
		D_CREATE_INDEX = SQLITE_CREATE_INDEX,
		D_CREATE_TABLE = SQLITE_CREATE_TABLE,
		D_CREATE_VIEW = SQLITE_CREATE_VIEW,
		D_CREATE_TRIGGER = SQLITE_CREATE_TRIGGER,
		D_CREATE_TEMP_INDEX = SQLITE_CREATE_TEMP_INDEX,
		D_CREATE_TEMP_TABLE = SQLITE_CREATE_TEMP_TABLE,
		D_CREATE_TEMP_TRIGGER = SQLITE_CREATE_TEMP_TRIGGER,
		D_CREATE_TEMP_VIEW = SQLITE_CREATE_TEMP_VIEW,
		D_DROP_INDEX = SQLITE_DROP_INDEX,
		D_DROP_TABLE = SQLITE_DROP_TABLE,
		D_DROP_VIEW = SQLITE_DROP_VIEW,
		D_DROP_TRIGGER = SQLITE_DROP_TRIGGER,
		D_DROP_TEMP_INDEX = SQLITE_DROP_INDEX,
		D_DROP_TEMP_TABLE = SQLITE_DROP_TEMP_TABLE,
		D_DROP_TEMP_VIEW = SQLITE_DROP_TEMP_VIEW,
		D_DROP_TEMP_TRIGGER = SQLITE_DROP_TEMP_TRIGGER,
		D_PRAGMA = SQLITE_PRAGMA,
		D_READ = SQLITE_READ,
		D_SELECT = SQLITE_SELECT,
		D_TRANSACTION = SQLITE_TRANSACTION,
		D_ATTACH = SQLITE_ATTACH,
		D_DETACH = SQLITE_DETACH,
		D_IGNORE = SQLITE_IGNORE,
		D_SCHEMA = SQLITE_SCHEMA,
		D_OK = SQLITE_OK,
		D_ERROR = SQLITE_ERROR,
		D_BUSY = SQLITE_BUSY
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

	class database
	{
	//callbacks for sqlite
	public:
		typedef std::unordered_map<size_t,sqlite3_stmt*> statements;
		typedef statements::size_type statement_index;
		typedef statements::value_type::second_type statement_type;
		typedef int(*exeu_callback)(void* arg, int col, char** rol_val, char** col_names);
		typedef int(*commit_callback)(void* arg);
		typedef void(*rollback_callback)(void* arg);
		typedef void(*update_callback)(void* arg, int evt, char const* database_name, char const* table_name, sqlite_int64 rowid);
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

		static size_t index_counter;
		enum
		{
			BADSTMT = size_t(-1)
		};

		
		database();
		explicit database(const std::string_view& database_file);
		explicit database(const std::filesystem::path& database_file);
		database(database&& connection) noexcept;
		database& operator=(database&&) noexcept;
		void open(const std::filesystem::path& database_file);
		~database();

		statement_index prepare_query(const std::string& query);
		statement_index prepare_query(const nl::query& query);
		
		constexpr inline const statements& get_statements() const { return m_statements; }
		constexpr inline const std::string& get_error_msg() const { return m_error_msg; }
		inline const int get_error_code() const { return sqlite3_errcode(m_database_conn); }
		void remove_statement(nl::database::statement_index index);
		bool is_open() const { return (m_database_conn != nullptr); }
		void exec_once(statement_index index);
		void exec_rollback();
		void connect(const std::string_view& file);
		void cancel();
		
	public:
		void set_commit_handler(commit_callback callback, void* UserData);
		bool set_trace_handler(trace_callback callback, std::uint32_t mask, void* UserData);
		bool set_busy_handler(busy_callback callback, void* UserData);
		void set_rowback_handler(rollback_callback callback, void* UserData);
		void set_update_handler(update_callback callback, void* UserData);
		bool set_auth_handler(auth callback, void* UserData);
		void set_progress_handler(progress_callback callback, void* UserData, int frq);
		bool register_extension(const sql_extension_func_aggregate& ext);
		void flush_database();
	
	//template functions cover
	public:	
		template<typename relation>
		relation retrive(statement_index index)
		{
			return do_query_retrive<relation>(index);
		}

		template<typename row_t>
		row_t retrive_row(statement_index index){
			return do_query_retrive_row<row_t>(index);
		}

		template<typename relation>
		bool insert(statement_index index, const relation& rel)
		{
			return do_query_insert(index, rel);
		}

		template<typename relation, typename row_t>
		std::enable_if_t<nl::detail::is_relation_row_v<row_t, relation>, bool> 
			insert(statement_index index, const row_t& row)
		{
			return do_query_insert_row(index, row);
		}

		template<typename relation, typename...T>
		std::enable_if_t<nl::detail::is_relation_v<relation>, bool> 
			insert(statement_index index, const relation& rel ,const T&... args)
		{
			return do_query_insert_para(index, rel, args...);
		}

		template<typename relation, typename row_t, typename... T>
		std::enable_if_t<nl::detail::is_relation_row_v<row_t, relation>, bool>
			insert(statement_index index, const row_t& row, const T&... args)
		{
			return do_query_insert_para_row(index, row, args...);
		}

		
		template<typename... Args>
		bool bind(statement_index index, Args&&... args)
		{
			auto iter = m_statements.find(index);
			if (iter == m_statements.end()) {
				throw nl::database_exception("INVALID STATMENT INDEX", nl::database_exception::DB_STMT_INVALID);
			}
			auto statement = iter->second;
			constexpr size_t size = sizeof...(args);
			return nl::detail::loop<size - 1>::do_bind(statement, std::forward_as_tuple<Args...>(args...));
		}

		template<typename... Args, typename... Para>
		bool bind_para(statement_index index, std::tuple<Args...>&& data, const Para& ... paras) {
			using first_t = nl::detail::first_type_from_list_t<Para...>;
			static_assert(std::disjunction_v<std::is_convertible<first_t, std::string>,std::is_integral<first_t>>, "Parameters must be either an interger or a string");
			static_assert(std::conjunction_v<std::is_convertible<Para, std::conditional_t<std::is_integral_v<first_t>, first_t, std::string>>...>, "All parameters must be the same type");
			constexpr const size_t size = sizeof...(Args) - 1;
			const std::array<std::reference_wrapper<const first_t>, sizeof...(Para)> parameters{ std::ref(paras)... };

			auto iter = m_statements.find(index);
			if (iter == m_statements.end()) {
				throw nl::database_exception("INVALID STATMENT INDEX", nl::database_exception::DB_STMT_INVALID);
			} 
			auto statement = iter->second;
			return nl::detail::loop<size>::do_bind_para(statement, data, parameters);
		}


		template<typename row_t, typename...T>
		bool update(statement_index index, typename row_t& row, const T&... args)
		{
			return do_update_para(index, row, args...);
		}



	//template functions to do the dirty work
	private:
		//TODO: retrive_row
		template<typename relation_t>
		relation_t do_query_retrive(statement_index index)
		{
			static_assert(nl::detail::is_relation_v<relation_t>, "relation is not a valid relation type");

			auto iter = m_statements.find(index);
			if (iter == m_statements.end()){
				throw nl::database_exception("INVALID STATMENT INDEX", nl::database_exception::DB_STMT_INVALID);
			}
			auto statement = iter->second;
			int ret = 0;
			if ((ret = sqlite3_step(m_statements[stmt::begin])) == SQLITE_DONE)
			{
				
				relation_t rel;
				std::insert_iterator<typename relation_t::container_t> insert(rel, rel.begin());
				constexpr size_t size = std::tuple_size_v<typename relation_t::tuple_t> -1;
				while ((ret = sqlite3_step(statement)) == SQLITE_ROW)
				{
					if constexpr (nl::detail::is_map_relation<relation_t>::value)
					{
						insert = nl::detail::loop<size>::template do_retrive< typename relation_t::tuple_t>(statement);
					}
					else if constexpr (nl::detail::is_linear_relation<relation_t>::value)
					{
						std::back_insert_iterator<typename relation_t::container_t> back_insert(rel);
						back_insert = nl::detail::loop<size>::template do_retrive<typename relation_t::tuple_t>(statement);
					}
				}
				if (ret == SQLITE_BUSY) {
					//Unable to get lock database
					std::string error = sqlite3_errmsg(m_database_conn);
					sqlite3_reset(statement);
					exec_rollback();
					sqlite3_reset(m_statements[stmt::begin]);
					throw nl::database_exception(error, nl::database_exception::DB_BUSY);
				}
				else if (ret == SQLITE_ERROR || ret == SQLITE_MISUSE) {
					//error in the statement or statement misuse
					//finalize the statement
					//cache the most recent database code
					std::string error = sqlite3_errmsg(m_database_conn);
					int code = sqlite3_errcode(m_database_conn);

					//remove faulty statement, the user should get a hint that there is a problem, 
					sqlite3_finalize(statement);
					m_statements.erase(iter);

					exec_rollback();
					sqlite3_reset(m_statements[stmt::begin]);
					throw nl::database_exception(error, code);
				} else if (ret == SQLITE_DONE) //query done succefully
				{
					//this is important: if the reset fails; there is a problem with the VM, need to at least recompile the 
					//... statement, so remove and throw a failed reset exeception, 
					if (sqlite3_reset(statement) != SQLITE_OK) {
						//finalize the statement
						//cache the error from the reset operation
						std::string error = sqlite3_errmsg(m_database_conn);
						
						sqlite3_finalize(statement);
						m_statements.erase(iter);

						exec_rollback();
						sqlite3_reset(m_statements[stmt::begin]);
						throw nl::database_exception(error, nl::database_exception::DB_STMT_FAILED_RESET);
					}
					sqlite3_step(m_statements[roll_back ? stmt::rollback : stmt::end]);
					sqlite3_reset(m_statements[roll_back ? stmt::rollback : stmt::end]);
					sqlite3_reset(m_statements[begin]);
					return rel;
					
				}
				else {
					//ret gave us something we arent expecting, very unlikely but an extra branch hopefully wont help
					std::string error = sqlite3_errmsg(m_database_conn);
					int code = sqlite3_errcode(m_database_conn);

					exec_rollback();
					sqlite3_reset(m_statements[stmt::begin]);
					throw nl::database_exception(error, code);
				}
			} else {
				//error might occur if the database is locked by another connection
				//throw an exception with the data
				std::string error = sqlite3_errmsg(m_database_conn);
				int code = sqlite3_errcode(m_database_conn);
				exec_rollback();
				sqlite3_reset(m_statements[stmt::begin]);
				throw nl::database_exception(error, code);
				
			}
		}

		template<typename row_t>
		row_t do_query_retrive_row(statement_index index)
		{
			constexpr size_t size = std::tuple_size_v<row_t> - 1;
			auto iter = m_statements.find(index);
			if (iter == m_statements.end()) {
				throw nl::database_exception("INVALID STATMENT INDEX", nl::database_exception::DB_STMT_INVALID);
			}
			
			auto [idx, statement] = *iter;
			if ((sqlite3_step(m_statements[begin])) == SQLITE_DONE)
			{
				auto ret = sqlite3_step(statement);
				if (ret == SQLITE_ERROR || ret == SQLITE_MISUSE) {
					//error in the statement
					std::string error = sqlite3_errmsg(m_database_conn);
					int code = sqlite3_errcode(m_database_conn);

					sqlite3_finalize(statement);
					m_statements.erase(iter);

					exec_rollback();
					sqlite3_reset(m_statements[stmt::begin]);
					throw nl::database_exception(error, code);
				}
				else if (ret == SQLITE_BUSY) {
					//unable to aquire lock on database
					//roll back the statement
					std::string error = sqlite3_errmsg(m_database_conn);
					sqlite3_reset(statement);

					exec_rollback();
					sqlite3_reset(m_statements[stmt::begin]);
					throw nl::database_exception(error, nl::database_exception::DB_BUSY);

				}else{ //ret == SQLITE_ROW OR SQLITE_DONE
					row_t row;
					if (ret == SQLITE_ROW) {
						//we have a row to pick up 
						row = std::move(nl::detail::loop<size>::template do_retrive<row_t>(statement));
					}
					if (sqlite3_reset(statement) != SQLITE_OK) {
						//cache return string, 
						//possibly problem in the VM, remove statement so that the statment can be repared
						std::string error = sqlite3_errmsg(m_database_conn);
						int code = sqlite3_errcode(m_database_conn);

						sqlite3_finalize(statement);
						m_statements.erase(iter);

						exec_rollback();
						sqlite3_reset(m_statements[stmt::begin]);
						throw nl::database_exception(error, nl::database_exception::DB_STMT_FAILED_RESET);
					}
					sqlite3_step(m_statements[ roll_back ? stmt::rollback : stmt::end]);
					sqlite3_reset(m_statements[roll_back ? stmt::rollback : stmt::end]);
					sqlite3_reset(m_statements[stmt::begin]);
					return row;
				} 
			}
			else{
				//unable to begin a transaction in shared, most likely busy 
				std::string error = sqlite3_errmsg(m_database_conn);
				int code = sqlite3_errcode(m_database_conn);
				
				exec_rollback();
				sqlite3_reset(m_statements[stmt::begin]);
				throw nl::database_exception(error, code);
			}
		}



		template<typename relation>
		bool do_query_insert(statement_index index, const relation& rel)
		{
			static_assert(nl::detail::is_relation_v<relation>, "relation is not a valid relation type");
			constexpr size_t size = std::tuple_size_v<typename relation::tuple_t> - 1;
			auto  iter = m_statements.find(index);
			if (iter == m_statements.end()){
				throw nl::database_exception("Invalud statement index", nl::database_exception::DB_STMT_INVALID);
			}
			auto statement = iter->second;
			if (sqlite3_step(m_statements[begin_immediate]) == SQLITE_DONE){
				for (auto& tuple : rel) {
					if (!nl::detail::loop<size>::do_bind(statement, tuple)) {
						//error only happends if we cant bind a particular value to 
						//... need to finalize and remove the statement, error in the statement
						std::string error = std::string(sqlite3_errmsg(m_database_conn));
						int code = sqlite3_errmsg(m_database_conn);

						sqlite3_clear_bindings(statement);
						sqlite3_finalize(statement);
						m_statements.erase(iter);


						exec_rollback();
						sqlite3_reset(m_statements[stmt::begin_immediate]);
						throw nl::database_exception(error, code);
						
					}
					int ret = sqlite3_step(statement);
					if (ret == SQLITE_ERROR || ret == SQLITE_MISUSE) {
						std::string error = std::string(sqlite3_errmsg(m_database_conn));
						int code = sqlite3_errmsg(m_database_conn);

						sqlite3_clear_bindings(statement);
						sqlite3_finalize(statement);
						m_statements.erase(iter);


						exec_rollback();
						sqlite3_reset(m_statements[stmt::begin_immediate]);
						throw nl::database_exception(error, code);
					}
					else if (ret == SQLITE_BUSY) {
						std::string error = std::string(sqlite3_errmsg(m_database_conn));
						sqlite3_reset(statement);
						sqlite3_clear_bindings(statement);
						exec_rollback();
						sqlite3_reset(m_statements[stmt::begin_immediate]);
						throw nl::database_exception(error, nl::database_exception::DB_BUSY);

					}else if (ret == SQLITE_DONE) {
							if(sqlite3_reset(statement) != SQLITE_OK) {
								std::string error = std::string(sqlite3_errmsg(m_database_conn));

								sqlite3_clear_bindings(statement);
								sqlite3_finalize(statement);
								m_statements.erase(iter);

								exec_rollback();
								sqlite3_reset(m_statements[stmt::begin_immediate]);
								throw nl::database_exception(error, nl::database_exception::DB_STMT_FAILED_RESET);
							}
							sqlite3_clear_bindings(statement);
					}
				}
				sqlite3_step(m_statements[roll_back ? stmt::rollback : stmt::end]);
				sqlite3_reset(m_statements[roll_back ? stmt::rollback : stmt::end]);
				sqlite3_reset(m_statements[begin_immediate]);

				//compatability with the api
				return true;
				
			}
			else {
				std::string error = sqlite3_errmsg(m_database_conn);
				int code = sqlite3_errcode(m_database_conn);

				exec_rollback();
				sqlite3_reset(m_statements[stmt::begin_immediate]);
				throw nl::database_exception(error, code);
			}
		}

		template<typename row_t>
		bool do_query_insert_row(statement_index index, const row_t& row)
		{
			constexpr size_t size = std::tuple_size_v<row_t> -1;
			auto iter = m_statements.find(index);
			if (iter == m_statements.end()){
				throw nl::database_exception("INVALID STATEMENT INDEX", nl::database_exception::DB_STMT_INVALID);
			}
			auto statement = iter->second;
			//bind the data
			if (!nl::detail::loop<size>::do_bind(statement, row)) {
				//error only happends if we cant bind a particular value to 
				//... need to finalize and remove the statement, error in the statement
				std::string error = std::string(sqlite3_errmsg(m_database_conn));
				int code = sqlite3_errmsg(m_database_conn);

				sqlite3_clear_bindings(statement);
				sqlite3_finalize(statement);
				m_statements.erase(iter);
				throw nl::database_exception(error, code);

			}

			if (sqlite3_step(m_statements[begin_immediate]) == SQLITE_DONE)
			{
				int ret = sqlite3_step(statement);
				if (ret == SQLITE_ERROR || ret == SQLITE_MISUSE) {
					std::string error = std::string(sqlite3_errmsg(m_database_conn));
					int code = sqlite3_errmsg(m_database_conn);

					sqlite3_clear_bindings(statement);
					sqlite3_finalize(statement);
					m_statements.erase(iter);


					exec_rollback();
					sqlite3_reset(m_statements[stmt::begin_immediate]);
					throw nl::database_exception(error, code);
				}
				else if (ret == SQLITE_BUSY) {
					std::string error = std::string(sqlite3_errmsg(m_database_conn));
					sqlite3_reset(statement);
					sqlite3_clear_bindings(statement);
					exec_rollback();
					sqlite3_reset(m_statements[stmt::begin_immediate]);
					throw nl::database_exception(error, nl::database_exception::DB_BUSY);

				}
				else if (ret == SQLITE_DONE) {
					if (sqlite3_reset(statement) != SQLITE_OK){
						std::string error = std::string(sqlite3_errmsg(m_database_conn));

						sqlite3_clear_bindings(statement);
						sqlite3_finalize(statement);
						m_statements.erase(iter);

						exec_rollback();
						sqlite3_reset(m_statements[stmt::begin_immediate]);
						throw nl::database_exception(error, nl::database_exception::DB_STMT_FAILED_RESET);
					}

					sqlite3_clear_bindings(statement);
					sqlite3_step(m_statements[roll_back ? stmt::rollback : stmt::end]);
					sqlite3_reset(m_statements[roll_back ? stmt::rollback : stmt::end]);
					sqlite3_reset(m_statements[begin_immediate]);

					//compatability with the api
					return true;
					
				}
			} else {
				//cannot execute begin_immediate, probably because of failed lock aquisation 
				std::string error = sqlite3_errmsg(m_database_conn);
				int code = sqlite3_errcode(m_database_conn);

				exec_rollback();
				sqlite3_reset(m_statements[stmt::begin_immediate]);
				throw nl::database_exception(error, code);
			}
			
		}
		
		inline bool do_query_insert_row(...)
		{
			assert(false && "Invalid do_query_insert_row relation arguments");
			return false;
		}

		template<typename relation, typename... T>
		bool do_query_insert_para(statement_index index, const relation& rel, const T&...args)
		{
			using first_para_t = nl::detail::first_type_from_list_t<T...>;
			static_assert(std::disjunction_v<std::is_convertible<first_para_t, std::string>, std::is_integral<first_para_t>>, "Parameters must be either an interger or a string");
			static_assert(std::conjunction_v<std::is_convertible<T, std::conditional_t<std::is_integral_v<first_para_t>, first_para_t, std::string>>...>, "All parameters must be the same type");
			static_assert(nl::detail::is_relation_v<relation>, "relation is not a valid relation type");


			const std::array<std::reference_wrapper<const first_para_t>, sizeof...(T)> parameters{ std::ref(args)... };

			constexpr size_t size = std::tuple_size_v<typename relation::tuple_t> - 1;
			auto iter = m_statements.find(index);
			if (iter == m_statements.end()){
				throw nl::database_exception("INVALID STATMENT INDEX", nl::database_exception::DB_STMT_INVALID);
			}
			auto statement = iter->second;
			//TODO: the execution of the begin immediate should be close to the execution of the statement, 
			//the bind do not need an opened transcation
			if (sqlite3_step(m_statements[stmt::begin_immediate]) == SQLITE_DONE)
			{
				for (auto& tuple : rel) {
					if (!nl::detail::loop<size>::do_bind(statement, tuple)) {
						//error only happends if we cant bind a particular value to 
						//... need to finalize and remove the statement, error in the statement
						std::string error = std::string(sqlite3_errmsg(m_database_conn));
						int code = sqlite3_errcode(m_database_conn);

						sqlite3_clear_bindings(statement);
						sqlite3_finalize(statement);
						m_statements.erase(iter);


						exec_rollback();
						sqlite3_reset(m_statements[stmt::begin_immediate]);
						throw nl::database_exception(error, code);

					}
					int ret = sqlite3_step(statement);
					if (ret == SQLITE_ERROR || ret == SQLITE_MISUSE) {
						std::string error = std::string(sqlite3_errmsg(m_database_conn));
						int code = sqlite3_errcode(m_database_conn);

						sqlite3_clear_bindings(statement);
						sqlite3_finalize(statement);
						m_statements.erase(iter);


						exec_rollback();
						sqlite3_reset(m_statements[stmt::begin_immediate]);
						throw nl::database_exception(error, code);
					}
					else if (ret == SQLITE_BUSY) {
						std::string error = std::string(sqlite3_errmsg(m_database_conn));
						sqlite3_reset(statement);
						sqlite3_clear_bindings(statement);
						exec_rollback();
						sqlite3_reset(m_statements[stmt::begin_immediate]);
						throw nl::database_exception(error, nl::database_exception::DB_BUSY);

					}
					else if (ret == SQLITE_DONE) {
						if (sqlite3_reset(statement) != SQLITE_OK) {
							std::string error = std::string(sqlite3_errmsg(m_database_conn));

							sqlite3_clear_bindings(statement);
							sqlite3_finalize(statement);
							m_statements.erase(iter);

							exec_rollback();
							sqlite3_reset(m_statements[stmt::begin_immediate]);
							throw nl::database_exception(error, nl::database_exception::DB_STMT_FAILED_RESET);
						}
						sqlite3_clear_bindings(statement);
					}
				}
				sqlite3_step(m_statements[roll_back ? stmt::rollback : stmt::end]);
				sqlite3_reset(m_statements[roll_back ? stmt::rollback : stmt::end]);
				sqlite3_reset(m_statements[begin_immediate]);

				//compatability with the api
				return true;

			}
			else {
				std::string error = sqlite3_errmsg(m_database_conn);
				int code = sqlite3_errcode(m_database_conn);

				exec_rollback();
				sqlite3_reset(m_statements[stmt::begin_immediate]);
				throw nl::database_exception(error, code);
			}
		}

		template<typename row_t, typename... T>
		bool do_query_insert_para_row(statement_index index, const row_t& row, const T&...args)
		{
			using first_para_t = nl::detail::first_type_from_list_t<T...>;
			static_assert(std::disjunction_v<std::is_convertible<first_para_t, std::string>, std::is_integral<first_para_t>>, "Parameters must be either an interger or a string");
			static_assert(std::conjunction_v<std::is_convertible<T, std::conditional_t<std::is_integral_v<first_para_t>, first_para_t, std::string>>...>, "All parameters must be the same type");

			const std::array<std::reference_wrapper<const first_para_t>, sizeof...(T)> parameters{ std::ref(args)... };
			constexpr size_t size = std::tuple_size_v<row_t> -1;
			auto iter = m_statements.find(index);
			if (iter == m_statements.end()){
				throw nl::database_exception("INVALID STATMENT INDEX", nl::database_exception::DB_STMT_INVALID);
			}
			auto statement = iter->second;
			//bind the parameters
			if (!nl::detail::loop<size>::do_bind_para(statement, row, parameters)) {
				std::string error = std::string(sqlite3_errmsg(m_database_conn));
				int code = sqlite3_errcode(m_database_conn);

				sqlite3_clear_bindings(statement);
				sqlite3_finalize(statement);
				m_statements.erase(iter);
				throw nl::database_exception(error, code);
			}

			if (sqlite3_step(m_statements[stmt::begin_immediate]) == SQLITE_DONE)
			{
					//execute the insert statement
					int ret = sqlite3_step(statement);
					if (ret == SQLITE_ERROR || ret == SQLITE_MISUSE) {
						std::string error = std::string(sqlite3_errmsg(m_database_conn));
						int code = sqlite3_errcode(m_database_conn);

						sqlite3_clear_bindings(statement);
						sqlite3_finalize(statement);
						m_statements.erase(iter);

						exec_rollback();
						sqlite3_reset(m_statements[stmt::begin_immediate]);
						throw nl::database_exception(error, code);
					}
					else if (ret == SQLITE_BUSY) {
						//cannot aquire lock on the database
						std::string error = sqlite3_errmsg(m_database_conn);
						int code = sqlite3_errcode(m_database_conn);
						sqlite3_reset(statement);
						sqlite3_clear_bindings(statement);

						exec_rollback();
						sqlite3_reset(m_statements[stmt::begin_immediate]);
						throw nl::database_exception(error, nl::database_exception::DB_BUSY);

					} else { // ret == SQLITE_DONE
						if (sqlite3_reset(statement) != SQLITE_OK)
						{
							std::string error  = sqlite3_errmsg(m_database_conn);
							sqlite3_clear_bindings(statement);


							sqlite3_finalize(statement);
							m_statements.erase(iter);

							exec_rollback();
							sqlite3_reset(m_statements[stmt::begin_immediate]);
							throw nl::database_exception(error, nl::database_exception::DB_STMT_FAILED_RESET);
						}

						sqlite3_clear_bindings(statement);

						sqlite3_step(m_statements[roll_back ? stmt::rollback : stmt::end]);
						sqlite3_reset(m_statements[roll_back ? stmt::rollback : stmt::end]);
						sqlite3_reset(m_statements[stmt::begin_immediate]);
						return true;
						
					}
			} else { //cannot executute begin_immediate, probably due to a lock
				std::string error = sqlite3_errmsg(m_database_conn);
				int code = sqlite3_errcode(m_database_conn);

				exec_rollback();
				sqlite3_reset(m_statements[stmt::begin_immediate]);
				throw nl::database_exception(error, code);
			}
		}

		template<typename row_t, typename... T>
		bool do_update_para(statement_index index, const row_t& row, const T&... args)
		{
			using first_para_t = nl::detail::first_type_from_list_t<T...>;
			static_assert(std::disjunction_v<std::is_convertible<first_para_t, std::string>, std::is_integral<first_para_t>>, "Parameters must be either an interger or a string");
			static_assert(std::conjunction_v<std::is_convertible<T, std::conditional_t<std::is_integral_v<first_para_t>, first_para_t, std::string>>...>, "All parameters must be the same type");
			const std::array<std::reference_wrapper<const first_para_t>, sizeof...(T)> parameters{ std::ref(args)... };
			constexpr size_t size = std::tuple_size_v<row_t> - 1;

			auto iter = m_statements.find(index);
			if (iter == m_statements.end())
			{
				throw nl::database_exception("INVALID STATEMENT INDEX", nl::database_exception::DB_STMT_INVALID);
			}
			auto statement = iter->second;
			//bind the parameters
			if (!nl::detail::loop<size>::do_bind_para(statement, row, parameters)) {
				std::string error = std::string(sqlite3_errmsg(m_database_conn));
				int code = sqlite3_errcode(m_database_conn);

				sqlite3_clear_bindings(statement);
				sqlite3_finalize(statement);
				m_statements.erase(iter);
				throw nl::database_exception(error, code);
			}

			if (sqlite3_step(m_statements[stmt::begin_immediate]) == SQLITE_DONE)
			{
				//execute the update statement
				int ret = sqlite3_step(statement);
				if (ret == SQLITE_ERROR || ret == SQLITE_MISUSE) {
					std::string error = std::string(sqlite3_errmsg(m_database_conn));
					int code = sqlite3_errcode(m_database_conn);

					sqlite3_clear_bindings(statement);
					sqlite3_finalize(statement);
					m_statements.erase(iter);

					exec_rollback();
					sqlite3_reset(m_statements[stmt::begin_immediate]);
					throw nl::database_exception(error, code);
				}
				else if (ret == SQLITE_BUSY) {
					//cannot aquire lock on the database
					std::string error = sqlite3_errmsg(m_database_conn);
					int code = sqlite3_errcode(m_database_conn);
					sqlite3_reset(statement);
					sqlite3_clear_bindings(statement);

					exec_rollback();
					sqlite3_reset(m_statements[stmt::begin_immediate]);
					throw nl::database_exception(error, nl::database_exception::DB_BUSY);

				}
				else { // ret == SQLITE_DONE
					if (sqlite3_reset(statement) != SQLITE_OK)
					{
						std::string error = sqlite3_errmsg(m_database_conn);
						sqlite3_clear_bindings(statement);


						sqlite3_finalize(statement);
						m_statements.erase(iter);

						exec_rollback();
						sqlite3_reset(m_statements[stmt::begin_immediate]);
						throw nl::database_exception(error, nl::database_exception::DB_STMT_FAILED_RESET);
					}

					sqlite3_clear_bindings(statement);

					sqlite3_step(m_statements[roll_back ? stmt::rollback : stmt::end]);
					sqlite3_reset(m_statements[roll_back ? stmt::rollback : stmt::end]);
					sqlite3_reset(m_statements[stmt::begin_immediate]);
					return true;

				}
			} else { //cannot executute begin_immediate, probably due to a lock
				std::string error = sqlite3_errmsg(m_database_conn);
				int code = sqlite3_errcode(m_database_conn);

				exec_rollback();
				sqlite3_reset(m_statements[stmt::begin_immediate]);
				throw nl::database_exception(error, code);
			}
		}

	private:
		//cannot copy or assign a database connection, every created on is a new connection, 
		//a database connection is a resource that is usually only one in a process
		database(const database&) = delete;
		database& operator=(const database&) = delete;
		


		sqlite3* m_database_conn{nullptr};
		statements m_statements{};
		std::string m_error_msg{};
	};


};
