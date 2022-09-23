#include "nl_net_exceptions.h"


nl::session_error::session_error(const beast::error_code& ec)
: m_error_code(ec) {
}

const char* nl::session_error::what() const noexcept
{
	return m_error_code.message().c_str();
}
