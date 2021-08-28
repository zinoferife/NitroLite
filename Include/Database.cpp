#include "../pch.h"
#include "Database.h"
nl::database_connection::database_connection()
{
}

nl::database_connection::database_connection(const std::string_view& database_file)
{
}

nl::database_connection::~database_connection()
{
}

nl::database_connection::statement_index nl::database_connection::prepare_query(const std::string_view& query)
{
	return statement_index();
}
