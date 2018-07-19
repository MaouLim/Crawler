#ifndef _CRAWLER_CORE_H_
#define _CRAWLER_CORE_H_

#include <thread>
#include <fstream>

#include <threadsafe_ostream.h>
#include <resovler.h>
#include <filter.h>
#include <request.h>
#include <message_queue.h>
#include <messages.h>
#include <debug.h>

#include <boost/algorithm/string.hpp>

namespace crawler {

	class core {

		typedef std::shared_ptr<tools::string_resovler>     resovler_ptr;
		typedef std::shared_ptr<tools::ts_ofstream>         ofstream_ptr;
		typedef std::shared_ptr<tools::filter<url_message>> filter_ptr;

	public:
		enum class status { UNAVAILABLE, READY, RUNNING };

		typedef crawler_msg_catagory                   message_catagory;
		typedef tools::message_queue<message_catagory> queue_type;

		~core() {
			shutdown();
			if (m_thd_analyze.joinable()) { m_thd_analyze.join(); }
			if (m_thd_filter.joinable())  { m_thd_filter.join();  }
		}

		explicit core(
			const url_message& seed, 
			const std::string& path = default_output_path
		) : 
			m_seeds(max_seeds), 
			m_candidates(max_candidates), 
			m_resps(max_resps),
			m_output_path(path)
		{
			m_seeds.wait_and_push(new url_message(seed));
			m_stat = status::READY;
		}

		template <typename _ForwardItr>
		core(
			_ForwardItr        first, 
			_ForwardItr        last, 
			const std::string& path = default_output_path
		) :
			m_seeds(max_seeds),
			m_candidates(max_candidates),
			m_resps(max_resps),
			m_output_path(path)
		{
			while (first != last) {
				m_seeds.wait_and_push(new url_message(*first));
				++first;
			}
			m_stat = status::READY;
		}

		bool run() {
			if (status::READY != m_stat) {
				return false;
			}

			m_stat = status::RUNNING;

			m_thd_analyze = std::thread(&core::_analyze_loop, this);
			m_thd_filter  = std::thread(&core::_filter_loop,  this);

			this->_request_loop();

			return true;
		}

		void shutdown() {
			if (status::RUNNING != m_stat) { return; }

			m_stat = status::UNAVAILABLE;

			m_seeds.clear();
			m_candidates.clear();
			m_resps.clear();

			m_seeds.wait_and_push(new stop_signal());
			m_candidates.wait_and_push(new stop_signal());
			m_resps.wait_and_push(new stop_signal());
		}

		const std::string& output_path() const { return m_output_path; }

	private:
		/*
		 * @note  main logic function
		 */
		void _request_loop() {

			http_req_executor executor(max_threads);

			size_t count = 0;

			while (status::RUNNING == m_stat) {

				auto msg = m_seeds.wait_and_pop_for(timeout_20s);

				if (nullptr == msg) { this->shutdown(); break; }
				if (message_catagory::STOP == msg->catagory()) { break; }

				if (message_catagory::URL == msg->catagory()) {
					auto url_msg = dynamic_cast<url_message*>(msg.get());
					assert(nullptr != url_msg);

					std::shared_ptr<http_req> req(new http_req(url_msg->url()));

					req->add_handler(
						std::bind(&_handle_resp, std::ref(m_resps), url_msg->url(), std::placeholders::_1)
					);

					executor.commit(req);

					if (max_total_seeds < ++count) { this->shutdown(); break; }
				}

#ifdef _DEBUG_OUTPUT_ERROR_INFO_
				else {
					tools::log(tools::debug_type::WARNING, "_request_loop", "Unknown type of message.");
				}
#endif
			}
		}

		void _analyze_loop() {

			boost::asio::thread_pool pool;
			std::ofstream fstream(m_output_path, std::ios::app | std::ios::out);

			resovler_ptr resovler(new response_resovler());
			ofstream_ptr stream(new tools::ts_ofstream(std::move(fstream)));

			while (status::RUNNING == m_stat) {
				auto msg = m_resps.wait_and_pop();
				if (message_catagory::STOP == msg->catagory()) {
					break;
				}

				if (message_catagory::HTTP_RESP == msg->catagory()) {
					boost::asio::post(
						pool, 
						std::bind(
							&_analyze_task, 
							std::ref(m_candidates), 
							resovler, 
							stream, 
							msg
						)
					);
				}
#ifdef _DEBUG_OUTPUT_ERROR_INFO_
				else {
					tools::log(tools::debug_type::WARNING, "_analyze_loop", "Unknown type of message.");
				}
#endif
			}

			pool.stop();
		}

		void _filter_loop() {
			filter_ptr filter(new bloom_filter<1600000, 110000>());

			while (status::RUNNING == m_stat) {
				auto msg = m_candidates.wait_and_pop();
				if (message_catagory::STOP == msg->catagory()) {
					break;
				}

				if (message_catagory::URL == msg->catagory()) {
					auto url_msg = dynamic_cast<url_message*>(msg.get());
					assert(nullptr != url_msg);

					if (filter->test(*url_msg)) {
						if (!m_seeds.wait_and_push_for(std::move(msg), timeout_1s)) {
							tools::log(tools::debug_type::FATAL, "_filter_loop", "Seeds queue is too small.");
						}
					}
				}
#ifdef _DEBUG_OUTPUT_ERROR_INFO_
				else {
					tools::log(tools::debug_type::WARNING, "_filter_loop", "Unknown type of message.");
				}
#endif
			}
		}

		static void _handle_resp(
			queue_type& queue, const std::string& url, const std::string& resp
		) {
			static const std::string ok_code("200");
			if (ok_code != resp.substr(9, 3)) {
#ifdef _DEBUG_OUTPUT_ERROR_INFO_
				tools::log(
					tools::debug_type::WARNING, 
					"_handle_resp", 
					std::string("Bad response: ") + resp.substr(0, 16) + "..."
				);
#endif
				return;
			}

			std::string& ref_resp = const_cast<std::string&>(resp);
			std::string& ref_url  = const_cast<std::string&>(url);

			// todo should do pre-process
			if(
				!queue.wait_and_push_for(
					new http_resp_message(std::move(ref_url), std::move(ref_resp)), 
					timeout_1s
				)
			) {
				tools::log(tools::debug_type::FATAL, "_handle_resp", "Response queue is too small.");
			}
		}

		static void _handle_url_analyzed(
			queue_type&        candidates, 
			ofstream_ptr       stream,
			const std::string& request_url,
			const std::string& source, 
			size_t             offset, 
			const std::string& result
		) {
			std::string tmp(result);
			boost::trim(tmp);
			if (tmp.empty() || !_valid_url(tmp)) { return; }

			if (
				!candidates.wait_and_push_for(new url_message(result), timeout_1s)
			) {
				tools::log(tools::debug_type::FATAL, "_handle_url_analyzed", "Candidates queue is too small.");
				return;
			}

			*stream << request_url + "\t" + result + "\n";

#ifdef _DEBUG_OUTPUT_ERROR_INFO_
			tools::log(
				tools::debug_type::INFO, 
				"_handle_url_analyzed", 
				std::string("Resovled url: ") + result
			);
#endif
		}

		static void _analyze_task(
			queue_type&         candidates, 
			resovler_ptr        resovler, 
			ofstream_ptr        stream,
			queue_type::pointer msg
		) {
			const auto resp_msg = dynamic_cast<http_resp_message*>(msg.get());
			assert(nullptr != resp_msg);

			resovler->resovle(
				resp_msg->response(),
				std::bind(
					&_handle_url_analyzed,
					std::ref(candidates),
					stream,
					resp_msg->request_url(),
					std::placeholders::_1,
					std::placeholders::_2,
					std::placeholders::_3
				)
			);
		}

		static bool _valid_url(const std::string& url) {
			for (auto each : url) {
				if ('\n' == each || '\r' == each || '\t' == each) {
					return false;
				}
			}
			return true;
		}

	public:
		static const std::string default_output_path;

	private:
		static const size_t max_total_seeds = 10000u;

		static const size_t max_seeds      = 1024u;
		static const size_t max_candidates = 4096u;
		static const size_t max_resps      = 256u;

		static const size_t max_threads = 32u;

		static const std::chrono::seconds timeout_20s;
		static const std::chrono::seconds timeout_1s;

		queue_type  m_seeds;
		queue_type  m_candidates;
		queue_type  m_resps;

		std::string m_output_path;

		std::thread m_thd_analyze;
		std::thread m_thd_filter;
		
		status      m_stat;
	};

	const std::string core::default_output_path("out.txt");

	const std::chrono::seconds core::timeout_20s(20);
	const std::chrono::seconds core::timeout_1s(1);
}

#endif
