#ifndef _CRAWLER_MESSAGES_H_
#define _CRAWLER_MESSAGES_H_

#include <string>

#include <message_base.h>

namespace crawler {

	enum class crawler_msg_catagory {
		STOP, URL, HTTP_RESP
	};

	class url_message : 
		public tools::message_base<crawler_msg_catagory> {
	private:
		typedef url_message                               self_type;
		typedef tools::message_base<crawler_msg_catagory> base_type;

	public:
		typedef base_type::message_catagory message_catagory;

		url_message(const std::string& url) : m_req_url(url) { }
		url_message(std::string&& url) : m_req_url(std::move(url)) { }

		url_message(const self_type&) = default;
		url_message(self_type&& other) : m_req_url(std::move(other.m_req_url)) { }

		virtual ~url_message() = default;

		message_catagory catagory() const override { return message_catagory::URL; }

		const std::string& url() const { return m_req_url; }

	private:
		std::string m_req_url;
	};

	class http_resp_message :
		public tools::message_base<crawler_msg_catagory> {
	private:
		typedef http_resp_message                         self_type;
		typedef tools::message_base<crawler_msg_catagory> base_type;

	public:
		typedef base_type::message_catagory message_catagory;

		http_resp_message(std::string&& url, std::string&& resp) : 
			m_response(std::move(resp)) {
			assert(url.length() < m_response.length());
			memcpy(m_response.data(), url.data(), sizeof (char) * url.length());
			m_response[url.length()] = '\r';
			m_response[url.length() + 1] = '\n';
		}

		http_resp_message(const self_type& other) = default;

		http_resp_message(self_type&& other) noexcept : 
			m_response(std::move(other.m_response)) { }

		virtual ~http_resp_message() = default;

		message_catagory catagory() const override { return message_catagory::HTTP_RESP; }

		const std::string& response() const { return m_response; }

		std::string request_url() const {
			return m_response.substr(0, m_response.find_first_of('\r'));
		}

	private:
		std::string m_response;
	};

	class stop_signal : 
		public tools::message_base<crawler_msg_catagory> {
	private:
		typedef stop_signal                               self_type;
		typedef tools::message_base<crawler_msg_catagory> base_type;

	public:
		typedef base_type::message_catagory message_catagory;

		stop_signal() = default;
		virtual ~stop_signal() = default;

		message_catagory catagory() const override { return message_catagory::STOP; }
	};
}

#endif