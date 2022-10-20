#include "../pch.h"
#include "Database.h"
std::int32_t nl::sql_extension_func_aggregate::sTextEncoding = SQLITE_UTF8;
size_t nl::database::index_counter = 0;


nl::database::database()
:m_database_conn(nullptr){
	
}

nl::database::database(const std::string_view& database_file)
:m_database_conn(nullptr){
	if (database_file.empty()){
		if (sqlite3_open(nullptr, &m_database_conn) == SQLITE_ERROR){
			throw nl::database_exception("FATAL DATABASE ERROR: CANNOT OPEN MEMORY DATABASE",
					nl::database_exception::DB_FAILED_TO_CREATE);
		}
	}else{
		if (sqlite3_open(database_file.data(), &m_database_conn) == SQLITE_ERROR){
			throw nl::database_exception("FATAL DATABASE ERROR: CANNOT OPEN DATABASE FILE",
					nl::database_exception::DB_FAILED_TO_CREATE);
		}
	}
	//create a begin, begin_immediate, end and rollback statments, 0, 1, 2 and 3 in the m_statments table
	query q;
	prepare_query(q.begin());
	prepare_query(q.clear().begin_immediate());
	prepare_query(q.clear().end());
	prepare_query(q.clear().roll_back());

}

nl::database::database(const std::filesystem::path& database_file)
{
	if(sqlite3_open(database_file.string().data(), &m_database_conn) == SQLITE_ERROR){
		throw nl::database_exception("FATAL DATABASE ERROR: CANNOT OPEN DATABASE FILE",
			nl::database_exception::DB_FAILED_TO_CREATE);
	}
	//create a begin, begin_immediate, end and rollback statments, 0, 1, 2 and 3 in the m_statments table
	query q;
	prepare_query(q.begin());
	prepare_query(q.clear().begin_immediate());
	prepare_query(q.clear().end());
	prepare_query(q.clear().roll_back());
}




nl::database::database(database&& connection) noexcept
{
	if (m_database_conn != nullptr){
		sqlite3_close(m_database_conn);
	}
	m_database_conn = std::move(connection.m_database_conn);
	m_statements = std::move(connection.m_statements);
	m_error_msg = std::move(connection.m_error_msg);
}

nl::database& nl::database::operator=(database&& connection) noexcept
{
	// TODO: insert return statement here
	if (m_database_conn != nullptr){
		sqlite3_close(m_database_conn);
	}
	m_database_conn = std::move(connection.m_database_conn);
	m_statements = std::move(connection.m_statements);
	m_error_msg = std::move(connection.m_error_msg);
	return (*this);
}

void nl::database::open(const std::filesystem::path& database_file)
{
	if (m_database_conn != nullptr) {
		sqlite3_close(m_database_conn);
	}
	if (sqlite3_open(database_file.string().data(), &m_database_conn) == SQLITE_ERROR) {
			throw nl::database_exception("FATAL DATABASE ERROR: CANNOT OPEN DATABASE FILE",
				nl::database_exception::DB_FAILED_TO_CREATE);
	}
	//create a begin, begin_immediate, end and rollback statments, 0, 1, 2 and 3 in the m_statments table
	m_statements.clear();
	query q;
	prepare_query(q.begin());
	prepare_query(q.clear().begin_immediate());
	prepare_query(q.clear().end());
	prepare_query(q.clear().roll_back());
}

nl::database::~database()
{
	if (!m_statements.empty()) {
		for (auto& stmt : m_statements){
			sqlite3_finalize((stmt.second));
		}
	}
	if (m_database_conn){
		sqlite3_close(m_database_conn);
	}
}

nl::database::statement_index nl::database::prepare_query(const std::string& query)
{
//#if DEBUG 
	assert(!query.empty() && "Prepare query is empty");
//#end if

	if (!sqlite3_complete(query.c_str())){	
		throw nl::database_exception(sqlite3_errmsg(m_database_conn),
				nl::database_exception::DB_STMT_INCOMPLETE);
	}
	statement_type statement;
	const char* tail = nullptr;
	if ((sqlite3_prepare_v2(m_database_conn, query.data(), query.size(), &statement, &tail)) == SQLITE_OK){
		size_t index = index_counter++; //forever increasing index_counter
		auto [iter, inserted] = m_statements.insert({ index, statement });
		if (inserted){
			return iter->first;
		}
		else{
			sqlite3_finalize(statement);
			throw nl::database_exception("Could not create query statement, query handle already exist",
				nl::database_exception::DB_STMT_ALREADY_EXIST);
		}
	}
	else {
		//error in spqlite
		throw nl::database_exception(sqlite3_errmsg(m_database_conn), sqlite3_errcode(m_database_conn));
	}
}

nl::database::statement_index nl::database::prepare_query(const nl::query& query)
{
	return prepare_query(query.get_query());
}

void nl::database::remove_statement(nl::database::statement_index index)
{
	auto iter = m_statements.find(index);
	if (iter != m_statements.end()) {
		sqlite3_finalize(iter->second);
		m_statements.erase(iter);
	}
}

void nl::database::set_commit_handler(commit_callback callback, void* UserData)
{
	sqlite3_commit_hook(m_database_conn, callback, UserData);
}

bool nl::database::set_trace_handler(trace_callback callback, std::uint32_t mask, void* UserData)
{
	return (sqlite3_trace_v2(m_database_conn, mask, callback, UserData) == SQLITE_OK);
}

bool nl::database::set_busy_handler(busy_callback callback, void* UserData)
{
	return (sqlite3_busy_handler(m_database_conn, callback, UserData) != SQLITE_ERROR);
}

void nl::database::set_rowback_handler(rollback_callback callback, void* UserData)
{
	sqlite3_rollback_hook(m_database_conn, callback, UserData);
}

void nl::database::set_update_handler(update_callback callback, void* UserData)
{
	sqlite3_update_hook(m_database_conn, callback, UserData);
}

bool nl::database::set_auth_handler(auth callback, void* UserData)
{
	return (sqlite3_set_authorizer(m_database_conn, callback, UserData) != SQLITE_ERROR);
}

void nl::database::set_progress_handler(progress_callback callback, void* UserData, int frq)
{
	sqlite3_progress_handler(m_database_conn, frq, callback, UserData);
}

bool nl::database::register_extension(const sql_extension_func_aggregate& ext)
{
	return (sqlite3_create_function(m_database_conn, ext.fName.c_str(), ext.fArgCount, nl::sql_extension_func_aggregate::sTextEncoding,
		ext.fUserData, ext.fFunc, ext.fStep, ext.fFinal) == SQLITE_OK);
}

void nl::database::flush_database()
{
	int ret = sqlite3_db_cacheflush(m_database_conn);
	if (ret != SQLITE_OK) {
		//throw exception
		throw nl::database_exception(sqlite3_errmsg(m_database_conn),
			sqlite3_errcode(m_database_conn));
	}
}

void nl::database::connect(const std::string_view& file)
{
	if (m_database_conn) {
		//already holds a connection
		sqlite3_close(m_database_conn);
		m_database_conn = nullptr;
	}

	if (file.empty()) {
		//opening in memory 
		if (sqlite3_open(nullptr, &m_database_conn) == SQLITE_ERROR) {
			throw nl::database_exception("FATAL DATABASE ERROR: CANNOT OPEN DATABASE FILE",
				nl::database_exception::DB_FAILED_TO_CREATE);
		}
	}
	else {
		if (sqlite3_open(file.data(), &m_database_conn) == SQLITE_ERROR)
		{
			throw nl::database_exception("FATAL DATABASE ERROR: CANNOT OPEN DATABASE FILE",
				nl::database_exception::DB_FAILED_TO_CREATE);
		}
	}
}

void nl::database::cancel()
{
	//cancel an operation
	sqlite3_interrupt(m_database_conn);
}

void nl::database::exec_once(statement_index index)
{
	auto iter = m_statements.find(index);
	if (iter == m_statements.end()) {
		throw nl::database_exception("INVALID STATMENT INDEX", nl::database_exception::DB_STMT_INVALID);
	}
	auto [index_, statement] = *iter;
	if (statement)
	{
		if (sqlite3_step(statement) == SQLITE_DONE){
			if (sqlite3_reset(statement) == SQLITE_SCHEMA) {
				sqlite3_finalize(statement);
				m_statements.erase(iter);
				throw nl::database_exception(sqlite3_errmsg(m_database_conn), sqlite3_errcode(m_database_conn));
			}
		}
		else {
			sqlite3_finalize(statement);
			m_statements.erase(iter);
			throw nl::database_exception(sqlite3_errmsg(m_database_conn), sqlite3_errcode(m_database_conn));
		}
	}
}

void nl::database::exec_rollback()
{
	sqlite3_step(m_statements[stmt::rollback]);
	sqlite3_reset(m_statements[stmt::rollback]);
}

