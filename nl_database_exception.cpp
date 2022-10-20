#include "nl_database_exception.h"

nl::database_exception::database_exception(std::string_view message, std::uint32_t code)
: m_message(message), m_code(code){
}

const char* nl::database_exception::what() const noexcept
{
	return m_message.data();
}
