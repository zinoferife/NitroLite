#pragma once
#include <exception>
#include <string>
#include <boost/beast.hpp>

//the exception thrown from the network on request or response;



namespace beast = boost::beast;

namespace nl {
	class session_error : public std::exception
	{
	public:
		session_error(const beast::error_code& ec);
		virtual const char* what() const noexcept override; 

		const beast::error_code& error_code() const noexcept{
			return m_error_code;
		}

	private:
		beast::error_code m_error_code;

	};


}


