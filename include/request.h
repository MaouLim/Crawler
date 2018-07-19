#ifndef _CRAWLER_REQUEST_H_
#define _CRAWLER_REQUEST_H_

#include <memory>

#include <boost/shared_array.hpp>
#include <boost/asio.hpp>

#include <debug.h>

namespace bsys = boost::system;

namespace crawler {

	template <typename _Request>
	class request_executor;

	template <typename _ResponseHandler>
	class request {
	public:
		typedef _ResponseHandler                            resp_handler;
		typedef std::vector<resp_handler>                   handlers_type;
		typedef request_executor<request<_ResponseHandler>> executor_type;

		request() = default;

		request(request&& other) noexcept : 
			handlers(std::move(other.handlers)) { }

		request& operator=(request&& other) noexcept {
			if (this == &other) { return *this; }
			handlers = std::move(other.handlers);
			return *this;
		}

		virtual ~request() = default;

		void add_handler(resp_handler&& handler) { handlers.push_back(handler); }
		const handlers_type& get_handlers() const { return handlers; }

	protected:
		handlers_type handlers;
	};

	template <typename _ResponseHandler>
	class http_request_executor;

	template <typename _ResponseHandler>
	class http_request : public request<_ResponseHandler> {

		typedef request<_ResponseHandler>      base_type;
		typedef http_request<_ResponseHandler> self_type;

	public:
		typedef typename base_type::resp_handler          resp_handler;
		typedef typename base_type::handlers_type         handlers_type;
		typedef typename http_request_executor<self_type> executor_type;

		explicit http_request(const std::string& http_url) { this->_parse(http_url); }

		http_request(const std::string& host, const std::string& url) : 
			m_host(host), m_req_url(url) { }


		http_request(self_type&& other) noexcept :
			base_type(std::move(other)),
			m_host(std::move(other.m_host)),
			m_req_url(std::move(other.m_req_url)) { }

		const std::string& host() const { return m_host; }
		const std::string& url() const { return m_req_url; }

	private:
		void _parse(const std::string& http_url) {
			auto index = http_url.find_first_of('/');
			if (std::string::npos == index) {
				m_host = http_url;
				return;
			}
			
			m_host = http_url.substr(0, index);
			m_req_url  = http_url.substr(index);
		}

	private:
		std::string m_host;
		std::string m_req_url;
	};

	template <typename _ResponseHandler>
	http_request<_ResponseHandler> make_http_request(
		const std::string& host,
		const std::string& url,
		_ResponseHandler&& handler
	) {
		http_request<_ResponseHandler> req(host, url);
		req.add_handler(handler);
		return req;
	}

	typedef http_request<std::function<void(const std::string&)>> http_req;
	typedef http_request_executor<http_req>                       http_req_executor;

	template <typename _Request>
	class request_executor {
	public:
		typedef _Request request_type;
		
		virtual ~request_executor() = default;
		virtual void commit(const request_type&) { }
		virtual void commit(std::shared_ptr<request_type>) { }
	};

	template <typename _HTTPRequest>
	class http_request_executor : 
		public request_executor<_HTTPRequest> {

		typedef http_request_executor<_HTTPRequest> self_type;
		typedef request_executor<_HTTPRequest>      base_type;

	public:
		typedef typename base_type::request_type    request_type;
		typedef typename request_type::resp_handler resp_handler;

	private:
		
		typedef char byte_type;

		typedef std::shared_ptr<request_type>                 req_ptr;
		typedef std::shared_ptr<std::string>                  string_ptr;
		typedef std::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr;
		typedef boost::shared_array<byte_type>                tmp_buffer_ptr;
		typedef std::shared_ptr<std::stringbuf>               buffer_ptr;

	public:
		explicit http_request_executor(size_t threads) : m_pool(threads) { }

		~http_request_executor() {
			m_pool.stop();
			m_service.stop();
		}

		void commit(std::shared_ptr<request_type> req_ptr) override {
			boost::asio::post(
				m_pool,
				std::bind(
					&_handle_send_req, std::ref(m_service), req_ptr
				)
			);
		}

		void join() { m_pool.join(); }

	private:
		static string_ptr 
			_generate_get_request(const request_type& request) 
		{
			static const char* get_template = 
				"GET %s HTTP/1.1\r\n"
				"HOST: %s\r\n"
				"Connection: close\r\n\r\n";

			char buffer[default_buffer_size];
			int bytes_sprinted = 
				sprintf_s(buffer, default_buffer_size, get_template, request.url().c_str(), request.host().c_str());

			if (bytes_sprinted < 0) {
				throw std::exception("Error in sprintf_s().");
			}
			if (default_buffer_size <= bytes_sprinted) {
				throw std::overflow_error("Request is too long.");
			}

			return std::make_shared<std::string>(buffer, bytes_sprinted);
		}

		static void _handle_read_resp(
			req_ptr                 req, 
			buffer_ptr              buff, 
			tmp_buffer_ptr          tmp_buff,
			sock_ptr                sock,    
			const bsys::error_code& err, 
			size_t                  bytes_read
		) {
			if (2 == err.value() /* end of file */) {
				const auto& handlers = req->get_handlers();
				for (const auto& each : handlers) {
					each(buff->str());
				}
				sock->shutdown(boost::asio::socket_base::shutdown_both);
				sock->close();
				return;
			}
			if (bsys::errc::success != err.value()) {
				// todo with error
				tools::log(tools::debug_type::WARNING, "_handle_read_resp", err.message());

				sock->shutdown(boost::asio::socket_base::shutdown_both);
				sock->close();
				return;
			}

			buff->sputn(tmp_buff.get(), bytes_read);

			sock->async_read_some(
				boost::asio::buffer(tmp_buff.get(), default_buffer_size),
				std::bind(
					&_handle_read_resp,
					req,
					buff,
					tmp_buff,
					sock,
					std::placeholders::_1,
					std::placeholders::_2
				)
			);
		}

		static void _handle_connection(
			req_ptr                 req,
			sock_ptr                sock,
			const bsys::error_code& err
		) {
			if (bsys::errc::success != err.value()) {

				tools::log(tools::debug_type::WARNING, "_handle_connection", err.message());

				sock->shutdown(boost::asio::socket_base::shutdown_both);
				sock->close();
				return;
			}

			tmp_buffer_ptr recv_tmp_buff(new byte_type[default_buffer_size]);
			buffer_ptr     recv_buff(new std::stringbuf());

			auto req_str_ptr = _generate_get_request(*req);

			boost::asio::async_write(
				*sock,
				boost::asio::buffer(req_str_ptr->data(), req_str_ptr->length()),
				[req_str_ptr](const bsys::error_code& err, size_t bytes_written) {
					if (bsys::errc::success == err.value()) { return; }
					tools::log(tools::debug_type::WARNING, "lamda function in async_write", err.message());
				}
			);

			sock->async_read_some(
				boost::asio::buffer(recv_tmp_buff.get(), default_buffer_size),
				std::bind(
					&_handle_read_resp,
					req,
					recv_buff,
					recv_tmp_buff,
					sock,
					std::placeholders::_1,
					std::placeholders::_2
				)
			);
		}

		static void _handle_send_req(
			boost::asio::io_service& service,
			req_ptr                  req
		) {
			boost::asio::ip::tcp::resolver resolver(service);
			boost::asio::ip::tcp::resolver::query query(req->host(), "80");

			try {
				auto result = resolver.resolve(query);
				if (result.end() == result) { return; }

				auto req_str_ptr = _generate_get_request(*req);

				for (auto itr = result.begin(); result.end() != itr; ++itr) {

					boost::asio::ip::tcp::endpoint remote = *result.begin();
					sock_ptr sock(new boost::asio::ip::tcp::socket(service));

					sock->async_connect(
						remote,
						std::bind(
							&_handle_connection,
							req,
							sock,
							std::placeholders::_1
						)
					);
				}

				service.run();
			} 
			catch (const std::exception& ex) {
				tools::log(
					tools::debug_type::WARNING, 
					"_handle_send_req", 
					std::string(ex.what()) + ": " + req->host()
				);
			}
		}

	private:
		static const size_t default_buffer_size = 2048u;

	private:
		boost::asio::io_service  m_service;
		boost::asio::thread_pool m_pool;
	};
}

#endif