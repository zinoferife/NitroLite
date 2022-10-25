#pragma once
//nitrolite uses http for communication with the outside world
//it seralised tuples as json can be sent to servers that understands the schema

//targets are resources on the server which are tables in the database
//very request froms a session with the server connecting to it, sending a message and reading a response
//The io_context is created by applications that uses this session object
//Lunched to makes a request and signalled to the calling module when there is a response, most likely the modules that made the request 
//



#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <fmt/format.h>

#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>

#include <nlohmann/json.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <deque>
#include <sstream>
#include <algorithm>

#include <vector>
#include <mutex>
#include <future>


#include "nl_net_exceptions.h"


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

#define NITROLITE_USER_AGENT_STRING "nitrolite_1"

namespace nl {
	namespace js = nlohmann;
	//the session is a single request and reponse cycle 
	//should runs on the thread or threads that call one of the 

	template<class response_body, class request_body = http::empty_body>
	class session : public std::enable_shared_from_this<session<response_body, request_body>>
	{
	public:
		static_assert(std::conjunction_v<http::is_body<response_body>, http::is_body<request_body>>,
			"response body or request body is not supported by the session class");

		typedef http::request<request_body> request_type;
		typedef http::response<response_body> response_type;
		typedef std::promise<typename response_body::value_type> promise_t;
		typedef std::future<typename response_body::value_type> future_t;

		explicit session(net::io_context& ioc) : m_resolver(net::make_strand(ioc)),
			m_stream(net::make_strand(ioc))
		{
			//set the maximum size of the read flat_buffer, to avoid buffer overflow
			//catch buffer_overflow errors when this maximum is exceeeded, 
			//but how do i know the maximum buffer size for both the input sequence and the output sequence
		}
		session(session&& rhs)
		{
			m_resolver = std::move(rhs.m_resolver);
			m_stream = std::move(rhs.m_resolver);
			m_buffer = std::move(rhs.m_buffer);
			m_req =  std::move(rhs.m_req);
			m_res =  std::move(rhs.m_res);
			m_promise = std::move(rhs.m_promise);
		}
		
		session& operator=(session&& rhs) {
			m_resolver = std::move(rhs.m_resolver);
			m_stream = std::move(rhs.m_resolver);
			m_buffer = std::move(rhs.m_buffer);
			m_req = std::move(rhs.m_req);
			m_res = std::move(rhs.m_res);
			m_promise = std::move(rhs.m_promise);
		}


		//the copy constructor and the copy assignment is deleted, to prevent copying
		session(const session&) = delete;
		session& operator=(const session&) = delete;

		~session() {}


		template<http::verb verb>
		future_t req(const std::string& host,
			const std::string& port,
			const std::string& target,
			typename request_type::body_type const& body = typename request_type::body_type{}) {
			prepare_request(target, host, verb, body, 11);
			m_resolver.async_resolve(host,
				port,
				beast::bind_front_handler(
					&session::on_resolve,
					this->shared_from_this()));
			return (m_promise.get_future());
		}

		void cancel() {
			m_stream.socket().cancel();
		}

	private:
		//call backs for async functions
		void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
			if (ec)
				return on_fail(ec);

			// Set a timeout on the operation
			m_stream.expires_after(std::chrono::seconds(30));

			// Make the connection on the IP address we get from a lookup
			m_stream.async_connect(results,
				beast::bind_front_handler(
					&session::on_connect,
					this->shared_from_this()));
		}
		void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint) {
			if (ec)
				return on_fail(ec);

			// Set a timeout on the operation
			m_stream.expires_after(std::chrono::seconds(30));

			// Send the HTTP request to the remote host
			http::async_write(m_stream, 
				m_req, beast::bind_front_handler(&session::on_write,
				this->shared_from_this()));
		}
		void on_write(beast::error_code ec, size_t bytes) {
			boost::ignore_unused(bytes);

			if (ec)
				return on_fail(ec);

			// Receive the HTTP response
			http::async_read(m_stream, m_buffer, m_res,
				beast::bind_front_handler(
					&session::on_read,
					this->shared_from_this()));
		}
		
		void on_read(beast::error_code ec, size_t bytes) {
			boost::ignore_unused(bytes);

			if (ec)
				return on_fail(ec);

			// Gracefully close the socket
			m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);

			// not_connected happens sometimes so don't bother reporting it.
			if (ec && ec != beast::errc::not_connected)
				return on_fail(ec);

			//push the result of the respone to the 
			m_promise.set_value(m_res.body());
	
		}

		void on_fail(beast::error_code ec) {
			//causes the future end of the communication channel to throw an exception
			try {
				//throw 
				throw nl::session_error(ec);
			}
			catch (...) {
				try {
					m_promise.set_exception(std::current_exception());
				}
				catch (...) {} // do nothing if set_exception throws. mostly writing to a used promise
			}

		}

	private:
		//functions
		void prepare_request(const std::string& target,
			const std::string& host,
			http::verb verb,
			typename request_type::body_type const& body,
			int version
		)
		{
			if constexpr (std::is_same_v<request_body, http::string_body>) {
				//if not empty body
				//string bodies
				m_req.version(version);
				m_req.method(verb);
				m_req.target(target.c_str());
				m_req.set(http::field::host, host.c_str());
				m_req.set(http::field::user_agent, NITROLITE_USER_AGENT_STRING);
				m_req.body() = body;
				m_req.prepare_payload();
			}
			else if constexpr (std::is_same_v<request_body, http::file_body>) {
				//if request is a file body 
				http::request<http::file_body> req_{ std::piecewise_construct,
					std::make_tuple(std::move(body)) };
				req_.method(verb);
				req_.version(version);
				req_.target(target.c_str());
				req_.set(http::field::host, host.c_str());
				req_.set(http::field::user_agent, NITROLITE_USER_AGENT_STRING);
				m_req = std::move(req_);
				m_req.prepare_payload();

			}
			else if constexpr (std::is_same_v<request_body, http::empty_body>) {
				// the body is empty, usual get request have empty bodies
				m_req.version(version);
				m_req.method(verb);
				m_req.target(target.c_str());
				m_req.set(http::field::host, host.c_str());
				m_req.set(http::field::user_agent, NITROLITE_USER_AGENT_STRING);
			}
			//ignore all other bodies for now
		}


	private:
		void cancel_operation() {

		}


	private:
		
		tcp::resolver m_resolver;
		beast::tcp_stream m_stream;

		beast::flat_buffer m_buffer;
		request_type m_req;
		response_type m_res;
	
		//promise
		promise_t m_promise;

	};
	template<typename res_body, typename req_body = http::empty_body>
	using http_session_weak_ptr = std::weak_ptr<session<res_body, req_body>>;

	template<typename res_body, typename req_body = http::empty_body>
	using http_session_shared_ptr = std::shared_ptr<session<res_body, req_body>>;


	// message frame??
	//message parser??
	//message serialiser??
	//if T isnt a string, need to provide a way to read and write T from buffers
	//for example if T is a std::fstream, or a user defined type
	// the parser class has a single static function that reads frbuffer into T
	// the serialzer has a single static function that writes from T into the buffer
	//they both return true if the read and write is sucessful

	template<typename T>
	class parser;

	template<typename T>
	class serializer;

	namespace detail {
		template<typename T>
		struct has_serializer : std::false_type {};

		template<typename T>
		struct has_parser : std::false_type {};


		template<>
		struct has_serializer<std::string> : std::true_type {};

		template<>
		struct has_parser<std::string> : std::true_type {};
	}

	//implement the serialiser and parser for a string
	template<>
	class parser<std::string>
	{
		static bool read(std::string& value, const beast::flat_buffer& buffer)
		{
			if (buffer.size() == 0) return false;

			//assume the whole readable range is for the string
			auto read_buf = buffer.data();
			value = beast::buffers_to_string(net::buffer(read_buf));
			return !value.empty();
		}
	};

	template<>
	class serializer<std::string>
	{
		static bool write(std::string& value, beast::flat_buffer& buffer)
		{
			const size_t size = value.size();
			auto prep_buffer = buffer.prepare(size);
			auto it = std::copy(value.begin(), value.end(), (std::uint8_t*)prep_buffer.data());
			buffer.commit(size);
			return true;
		}

	};

	

	//websocket session
	template<typename T = std::string>
	class ws_session : public std::enable_shared_from_this<ws_session<T>>
	{
	public:
		
	};



	//expects the user to hold on to the object of ws_session class even if the class can just keep pushing its life time
	//The user can hold a weak_ptr to the object, and only access it if it is still active
	//for simple defination
	template<typename T = std::string>
	using ws_session_weak_ptr = std::weak_ptr<ws_session<T>>;

	template<typename T = std::string>
	using ws_session_shared_ptr = std::shared_ptr<ws_session<T>>;

	
	
}

