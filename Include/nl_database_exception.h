#pragma once
#include <exception>
#include <string_view>
#include <SQLite/sqlite3.h>

namespace nl
{
	//holds exceptions on the database
	class database_exception : public std::exception
	{
	public:
		//error codes
		enum : std::uint32_t
		{
			DB_ERROR = SQLITE_ERROR,
			DB_SCHEMA_CHANGE = SQLITE_SCHEMA,
			DB_BUSY = SQLITE_BUSY,
			DB_MISUSE = SQLITE_MISUSE,
			DB_STMT_ALREADY_EXIST = 300,
			DB_STMT_INCOMPLETE,
			DB_STMT_FAILED_TO_PREPARE,
			DB_STMT_FAILED_RESET,
			DB_STMT_INVALID,
			DB_FAILED_TO_CREATE,
			DB_FAILED_INTERTION

		};

		database_exception(std::string_view message, std::uint32_t code);
		virtual const char* what() const noexcept override;

		constexpr inline std::uint32_t code() const { return m_code; }
	private:
		std::uint32_t m_code;
		std::string_view m_message;
	};

}


