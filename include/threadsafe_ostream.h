#ifndef _CRAWLER_THREADSAFE_OSTREAM_H_
#define _CRAWLER_THREADSAFE_OSTREAM_H_

#include <mutex>

namespace tools {

	template <typename _OutputStream>
	class threadsafe_ostream {

		typedef threadsafe_ostream<_OutputStream> self_type;

	public:
		typedef _OutputStream ostream_type;

		threadsafe_ostream() = default;

		~threadsafe_ostream() {
			std::lock_guard<std::mutex> locker(m_mtx);

			try {
				m_stream.flush();
				m_stream.close();
			}
			catch (...) { }
		}

		explicit threadsafe_ostream(ostream_type&& stream) {
			std::lock_guard<std::mutex> locker(m_mtx);
			m_stream = std::move(stream);
		}

		threadsafe_ostream(self_type&& other) noexcept {
			std::lock(m_mtx, other.m_mtx);
			m_stream = std::move(other.m_stream);
		}

		self_type& operator=(self_type&& other) noexcept {
			if (this == &other) { return *this; }
			std::lock(m_mtx, other.m_mtx);
			m_stream = std::move(other.m_stream);
			return *this;
		}

		template <typename _ValTp>
		self_type& operator<<(const _ValTp& val) {
			std::lock_guard<std::mutex> locker(m_mtx);
			m_stream << val;
			return *this;
		}

	private:
		mutable std::mutex m_mtx;
		ostream_type       m_stream;
	};

	typedef threadsafe_ostream<std::ofstream> ts_ofstream;
}

#endif