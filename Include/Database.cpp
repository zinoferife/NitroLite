#include "../pch.h"
#include "Database.h"
std::int32_t nl::sql_extension_func_aggregate::sTextEncoding = SQLITE_UTF8;

nl::database_connection::database_connection()
:m_database_conn(nullptr){
	
}

nl::database_connection::database_connection(const std::string_view& database_file)
:m_database_conn(nullptr){
	if (database_file.empty()){
		if (sqlite3_open(nullptr, &m_database_conn) == SQLITE_ERROR){
			throw std::exception("FATAL DATABASE ERROR: CANNOT OPEN MEMORY DATABASE");
		}
	}else{
		if (sqlite3_open(database_file.data(), &m_database_conn) == SQLITE_ERROR){
			throw std::exception("FATAL DATABASE ERROR: CANNOT OPEN DATABASE FILE");
		}
	}
	//create a begin, begin_immediate, end and rollback statments, 0, 1, 2 and 3 in the m_statments table
	query q;
	prepare_query(q.begin());
	prepare_query(q.clear().begin_immediate());
	prepare_query(q.clear().end());
	prepare_query(q.clear().roll_back());

}

nl::database_connection::database_connection(const database_connection&& connection) noexcept
{
	if (m_database_conn != nullptr){
		sqlite3_close(m_database_conn);
	}
	m_database_conn = std::move(connection.m_database_conn);
	m_statements = std::move(connection.m_statements);
	m_error_msg = std::move(connection.m_error_msg);
}

nl::database_connection& nl::database_connection::operator=(const database_connection&& connection) noexcept
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

nl::database_connection::~database_connection()
{
	if (!m_statements.empty()) {
		for (auto& stmt : m_statements){
			sqlite3_finalize(stmt);
		}
	}
	if (m_database_conn){
		sqlite3_close(m_database_conn);
	}
}

nl::database_connection::statement_index nl::database_connection::prepare_query(const std::string& query)
{
	assert(!query.empty() && "Prepare query is empty");
	if (!sqlite3_complete(query.c_str())){
		m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
		return BADSTMT;
	}
	m_statements.emplace_back(nullptr);
	const char* tail = nullptr;
	if ((sqlite3_prepare(m_database_conn, query.data(), query.size(), &m_statements.back(), &tail)) == SQLITE_OK){
		return statements::size_type(m_statements.size() - 1);
	}
	m_statements.pop_back();
	m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
	return BADSTMT;
}

nl::database_connection::statement_index nl::database_connection::prepare_query(const nl::query& query)
{
	return prepare_query(query.get_query());
}

void nl::database_connection::remove_statement(nl::database_connection::statement_index index)
{
	assert(index < m_statements.size() && "Invalid statement index to remove");
	auto iter = m_statements.begin();
	std::advance(iter, index);
	m_statements.erase(iter);
}

bool nl::database_connection::set_commit_handler(commit_callback callback, void* UserData)
{
	return false;
}

bool nl::database_connection::set_trace_handler(trace_callback callback, std::uint32_t mask, void* UserData)
{
	return false;
}

bool nl::database_connection::set_busy_handler(busy_callback callback, void* UserData)
{
	return false;
}

bool nl::database_connection::set_auth_handler(auth callback, void* UserData)
{
	return false;
}

void nl::database_connection::set_progress_handler(progress_callback callback, void* UserData, int frq)
{
}

bool nl::database_connection::register_extension(const sql_extension_func_aggregate& ext)
{
	return (sqlite3_create_function(m_database_conn, ext.fName.c_str(), ext.fArgCount, nl::sql_extension_func_aggregate::sTextEncoding,
		ext.fUserData, ext.fFunc, ext.fStep, ext.fFinal) == SQLITE_OK);
}

bool nl::database_connection::exec_once(statement_index index)
{
	assert(index < m_statements.size() && "Invalid statement index");
	statements::reference statement = m_statements[index];
	if (statement)
	{
		if (sqlite3_step(statement) == SQLITE_DONE){
			if (sqlite3_reset(statement) == SQLITE_SCHEMA) {
				m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
				return false;
			}
			return true;
		}
		m_error_msg = std::string(sqlite3_errmsg(m_database_conn));
		return false;
	}
}

