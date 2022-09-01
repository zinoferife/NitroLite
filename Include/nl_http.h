#pragma once
//nitrolite uses http for communication with the outside world
//it seralised tuples as json can be sent to servers that understands the schema

//targets are resources on the server which are tables in the database
//very request froms a session with the server connecting to it, sending a message and reading a response
//The io_context is created by applications that uses this session object
//Lunched to makes a request and signalled to the calling module when there is a response, most likely the modules that made the request 
//


#include <boost/signals2.hpp>

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

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

#define NITROLITE_USER_AGENT_STRING "nitrolite_1"

namespace nl {
	namespace js = nlohmann;
	template<class request_body, class response_body = http::string_body>
	class session : public std::enable_shared_from_this<session<request_body, response_body>>
	{
	public:
		typedef http::request<request_body> request_type;
		typedef http::response<response_body> response_type;
		typedef boost::signals2::signal<void(const typename response_body::value_type & res)> response_signal_type;
		typedef boost::signals2::signal<void(const std::string& what)> error_signal_type;
		typedef boost::signals2::signal<void(void)> cancel_signal_type;

		explicit session(net::io_context& ioc) : resolver(net::make_strand(ioc)),
			stream(net::make_strand(ioc))
		{
			//set the maximum size of the read flat_buffer, to avoid buffer overflow attacks
			//catch buffer_overflow errors when this maximum is exceeeded, 
			//but how do i know the maximum buffer size for both the input sequence and the output sequence
		}

		void get(const std::string& host,
			const std::string& port,
			const std::string& target,
			const typename response_signal_type::slot_type& response_slot,
			const error_signal_type::slot_type& error_slot)
		{
			mResponseSignal.connect(response_slot);
			mErrorSignal.connect(error_slot);
			//prepare the request, 
			prepare_request(target, host, http::verb::get, http::empty_body{}, 11);
			resolver.async_resolve(host,
				port,
				beast::bind_front_handler(
					&session::on_resolve,
					this->shared_from_this()));
		}

		//put request to put data in the server
		void put(const std::string& host,
			const std::string& port,
			const std::string& target,
			typename request_type::body_type const& body,
			const typename response_signal_type::slot_type& response_slot,
			const error_signal_type::slot_type& error_slot)
		{
			mResponseSignal.connect(response_slot);
			mErrorSignal.connect(error_slot);

			//prepare request
			prepare_request(target, host, http::verb::put, body, 11);
			resolver.async_resolve(host,
				port,
				beast::bind_front_handler(
					&session::on_resolve,
					this->shared_from_this()));
		}
		void post(const std::string& host,
			const std::string& port,
			const std::string& target,
			typename request_type::body_type const& body,
			const typename response_signal_type::slot_type& response_slot,
			const error_signal_type::slot_type& error_slot)
		{
			mResponseSignal.connect(response_slot);
			mErrorSignal.connect(error_slot);

			//prepare request
			prepare_request(target, host, http::verb::post, body, 11);
			resolver.async_resolve(host,
				port,
				beast::bind_front_handler(
					&session::on_resolve,
					this->shared_from_this()));
		}

	public:
		void set_mime_type(std::string_view view) {
			mime_type = view;
		}
	private:
		//call backs for async functions
		void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
			if (ec)
				return on_fail(ec);

			// Set a timeout on the operation
			stream.expires_after(std::chrono::seconds(30));

			// Make the connection on the IP address we get from a lookup
			stream.async_connect(results,
				beast::bind_front_handler(
					&session::on_connect,
					this->shared_from_this()));
		}
		void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint) {
			if (ec)
				return on_fail(ec);

			// Set a timeout on the operation
			stream.expires_after(std::chrono::seconds(30));

			// Send the HTTP request to the remote host
			http::async_write(stream, req, beast::bind_front_handler(&session::on_write, this->shared_from_this()));
		}
		void on_write(beast::error_code ec, size_t bytes) {
			boost::ignore_unused(bytes);

			if (ec)
				return on_fail(ec);

			// Receive the HTTP response
			http::async_read(stream, buffer, res,
				beast::bind_front_handler(
					&session::on_read,
					this->shared_from_this()));
		}


		void on_read(beast::error_code ec, size_t bytes) {
			boost::ignore_unused(bytes);

			if (ec)
				return on_fail(ec);

			// Gracefully close the socket
			stream.socket().shutdown(tcp::socket::shutdown_both, ec);

			// not_connected happens sometimes so don't bother reporting it.
			if (ec && ec != beast::errc::not_connected)
				return on_fail(ec);

			//parse and collect the body, send signal to all the modules that one to signaled by
			//this session
			typename response_body::value_type body = res.body();
			//the respose slots are responsible for any multi-threading sync
			mResponseSignal(body);
		}

		void on_fail(beast::error_code ec) {
			auto what = fmt::format("{}, {:d}", ec.message(), ec.value());
			mErrorSignal(what);
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
				req.version(version);
				req.method(verb);
				req.target(target.c_str());
				req.set(http::field::host, host.c_str());
				req.set(http::field::user_agent, NITROLITE_USER_AGENT_STRING);
				req.body() = body;
				req.prepare_payload();
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
				req = std::move(req_);
				req.prepare_payload();

			}
			else if constexpr (std::is_same_v<request_body, http::empty_body>) {
				// the body is empty, usual get request have empty bodies
				req.version(version);
				req.method(verb);
				req.target(target.c_str());
				req.set(http::field::host, host.c_str());
				req.set(http::field::user_agent, NITROLITE_USER_AGENT_STRING);
			}
			//ignore all other bodies for now
		}

	public:
		//signal slots
		//connect a slot to the signal
		//returns the connection so that the slot management is done by the caller
		std::pair<boost::signals2::connection, boost::signals2::connection> slot(const typename response_signal_type::slot_type& res_slot,
			const error_signal_type::slot_type& error_slot)
		{
			return { mResponseSignal.connect(res_slot), mErrorSignal.connect(error_slot) };

		}

		void set_cancel_operation_slot(cancel_signal_type& cancel_signal) {
			cancel_signal.connect(std::bind(&cancel_operation, this));
		}

	private:
		void cancel_operation() {

		}

	private:
		response_signal_type mResponseSignal;
		error_signal_type mErrorSignal;
		tcp::resolver resolver;
		beast::tcp_stream stream;

		beast::flat_buffer buffer;
		request_type req;
		response_type res;

		//for files 
		std::string_view mime_type;
	};
	template<typename req_body, typename res_body = std::string>
	using http_session_weak_ptr = std::weak_ptr<session<req_body, res_body>>;

	template<typename req_body, typename res_body = std::string>
	using http_session_shared_ptr = std::shared_ptr<session<req_body, res_body>>;


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
		typedef boost::signals2::signal<void(const T& res)> response_signal_type;
		typedef boost::signals2::signal<void(const std::string& what)> error_signal_type;
		typedef boost::signals2::signal<void(void)> cancel_signal_type;
		typedef boost::signals2::signal<void(void)> connected_signal_type;
		typedef std::vector<beast::flat_buffer> message_queue_type;

		static_assert(detail::has_serializer<T>::value, "T dose not have a serialiser");
		static_assert(detail::has_parser<T>::value, "T does not have a parser");

		explicit ws_session(net::io_context& ioc) : m_ioc(net::make_strand(ioc)),
			m_ws(net::make_strand(ioc))
		{
			if constexpr (std::is_same_v<std::string, T>) {
				m_ws.text(true);
			}
			m_ws.control_callback([](auto frame_type, beast::string_view payload) {

			});
		}

		~ws_session() {
			m_is_active = false;
		}

	//	//target: to connect with the server though websocket, the upgrade request needs a target, 

		void connect(const std::string& host,
			const std::string& port, const std::string& target) {
			m_host = host;
			m_target = target;
			m_resolver.async_resolve(m_host, port,
				beast::bind_front_handler(&ws_session::on_resolve,
					this->shared_from_this()));
		}


		void send(T& message) {
			bool is_writing = !m_messages.empty();
			beast::flat_buffer buffer;
			bool ret = serializer<T>::template write(message, buffer);
			if (!ret)
			{
				//signal a serialiser error 

				return;
			}

			m_messages.push_back(std::move(buffer));
			if (!is_writing && m_is_active) {
				//no writing and completed handshake start write
				do_write();
			}

		}

		void close()
		{
			//gracefully close the websocket
			m_ws.async_close(beast::websocket::close_code::normal, beast::bind_front_handler(&ws_session::on_close,
				this->shared_from_this()));
		}

	private:
		void do_write() {
			auto& front = m_messages.front();
			m_ws.async_write(net::buffer(front.data()), beast::bind_front_handler(&ws_session::on_write,
				this->shared_from_this()));
		}
		void do_read() {
			//clear the buffer
			m_read_buffer.consume(m_read_buffer.size());
			m_ws.async_read(m_read_buffer, beast::bind_front_handler(&ws_session::on_read, this->shared_from_this()));
		}

	private:
		void on_read(beast::error_code ec, size_t bytes) {
			//bytes for diagonis, how to deliver it now
			if (ec) {
				return on_fail(ec);
			}
			// this runs on the networl thread, the response siganl shouldt run on this thread
			// how do make the response signal run on a different thread without spoilling this class with like multi-threaded stuff
			T message;
			bool ret = parser<T>::template read(message, m_read_buffer);
			if (!ret){
				//figure out how to send a parser failed error
				//back to the user
			}
			//singal the user with the message
			m_response_signal(message);

			//clear the bufferv and schedule another read
			m_read_buffer.consume(m_read_buffer.size());
			m_ws.async_read(m_read_buffer, beast::bind_front_handler(&ws_session::on_read, this->shared_from_this()));
		}

		void on_write(beast::error_code ec, size_t bytes) {
			if (ec) {
				return on_fail(ec);
			}
			//remove the already write 
			m_messages.pop_front();
			if (!m_messages.empty()){
				do_write();
			}
		}

		void on_connectd(beast::error_code ec,
			tcp::resolver::results_type::endpoint_type endpoints) {
			if (ec) {
				return on_fail(ec);
			}
	
			//switch off the timeout because the ws has its own system
			beast::get_lowest_layer(m_ws).expires_never();

			//set suggested timeout settings for the websocket
			m_ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

			m_ws.set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
				req.set(http::field::user_agent, std::string(fmt::format("{}-{}",NITROLITE_USER_AGENT_STRING, "websocket-client")));
				}));

			m_host = fmt::format("{}:{:d}", m_host, endpoints.port);

			//perfrom the handshake
			m_ws.async_handshake(m_host, m_target,
				beast::bind_front_handler(&ws_session::on_handshake,
					this->shared_from_this()));
		}

		void on_handshake(beast::error_code ec) {
			if (ec) {
				return on_fail(ec);
			}
			m_is_active = true;
			m_connected_signal();
			if (!m_messages.empty()) {
				do_write();
			}

			//start reading from the server after handshake. 
			do_read();
		}


		void on_resolve(beast::error_code ec,
			tcp::resolver::results_type result)
		{
			if (ec) {
				return on_fail(ec);
			}
			//set timeout for operation
			beast::get_lowest_layer(m_ws).expires_after(std::chrono::seconds(30));

			//connect to the ip address we get from the look up. 
			beast::get_lowest_layer(m_ws).async_connect(
				result,
				beast::bind_front_handler(
					&ws_session::on_connect,
					this->shared_from_this()));
		}

		void on_fail(beast::error_code ec) {
			auto string = fmt::format("{}, {:d}", ec.message(), ec.value());
			m_error_signal(string);
		}


		void on_close(beast::error_code ec) {
			if (ec) {
				//no close properly
				return on_fail(ec);
			}
			m_is_active = false;
		}

	private:
		boost::signals2::signal<void(const T& res)> m_response_signal;
		boost::signals2::signal<void(const std::string& what)> m_error_signal;
		boost::signals2::signal<void(void) > m_cancel_signal;
		boost::signals2::signal<void(void) > m_connected_signal;


		net::io_context& m_ioc;
		tcp::resolver m_resolver;
		std::string m_host;
		std::string m_target;
		websocket::stream<beast::tcp_stream> m_ws;
		beast::flat_buffer m_read_buffer;
		message_queue_type m_messages;


		bool m_is_active;

		//keeps an array of flat_buffers to sent to the server,
		// writing to these bufffers is through serializers, and reading from buffers is through parsers. 

	};



	//expects the user to hold on to the object of ws_session class even if the class can just keep pushing its life time
	//The user can hold a weak_ptr to the object, and only access it if it is still active
	//for simple defination
	template<typename T = std::string>
	using ws_session_weak_ptr = std::weak_ptr<ws_session<T>>;

	template<typename T = std::string>
	using ws_session_shared_ptr = std::shared_ptr<ws_session<T>>;


	
}

