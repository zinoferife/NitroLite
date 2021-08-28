#pragma once
#include <string>
#include <string_view>
#include <utility>
#include <cassert>
#include <SQLite/sqlite3.h>

#include "relation.h"

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
		enum
		{
			BADSTMT = size_t(-1)
		};
		database_connection();
		database_connection(const std::string_view& database_file);
		~database_connection();

		statement_index prepare_query(const std::string_view& query);
		inline const statements& get_statement() const { return m_statements; }

		template<typename relation>
		relation do_query(statement_index& index)
		{
			
		}

	private:
		sqlite3* m_database_conn;
		statements m_statements;

	};


};
