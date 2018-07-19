#ifndef CRAWLER_RESOVLER_H
#define CRAWLER_RESOVLER_H

#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

namespace tools {

	class string_resovler {
	public:
		virtual ~string_resovler() = default;

		template <typename _Predicate>
		size_t resovle(const std::string& source, _Predicate&& predicate) {
			size_t offset = 0, count = 0;
			std::string result;

			while (offset < source.length()) {
				offset = this->process(source, offset, result);
				if (source.length() <= offset) { break; }
				predicate(source, offset - result.length(), result);
				++count;
			}

			return count;
		}

	protected:
		/*
		 * @param source  the string to process.
		 * @param pos     the current position to start parsing.
		 * @param out     the next result will store in out.
		 * @ret           when process ended, the offset of the rest string.
		 */
		virtual size_t process(const std::string& source, size_t pos, std::string& out) = 0;
	};
}

namespace crawler {

	/*
	 * Created by Liu Bowen on 2018-07-07
	 */
	class response_resovler : public tools::string_resovler {
	public:

		size_t process(const std::string &source, size_t pos, std::string &out) override {
			out = "";

			auto url = source.substr(0, source.find('\r'));
			auto hostName = url.substr(0, source.find('/'));

			boost::regex urlreg("<a[^>]+href=[\"|\'](?!javascript:)(.*?)[\"|\']");
			boost::regex addreg("(\\/[^\\/](.*?))|(\\/)");
			boost::regex doublereg("\\/\\/(.*?)");
			boost::regex httpreg("(http\\:\\/\\/)|(https\\:\\/\\/)");

			boost::smatch m;
			boost::smatch http_match;

			std::string::const_iterator it_begin = source.begin() + pos;
			std::string::const_iterator it_end = source.end();

			if (boost::regex_search(it_begin, it_end, m, urlreg)) {
				std::string result = m[1].str();
				if (boost::regex_match(result, addreg)) {//replace url begin with '/'
					if (result.compare("/") == 0) {
						out.append(hostName);
					}
					else {
						out.append(hostName);
						out.append(result);
					}
				}
				else if (boost::regex_match(result, doublereg)) {//replace url begin with '//'
					result.replace(0, 2, "");
					if (result[result.length() - 1] == '/') {
						result.erase(result.end() - 1);
					}
					out.append(result);
				}
				else {
					std::string::const_iterator result_begin = result.begin();
					std::string::const_iterator result_end = result.end();
					if (boost::regex_search(result_begin, result_end, http_match,
						httpreg)) {//replcae url begin with 'http' or 'https'
						result = result.replace(0, http_match[0].second - result.begin(), "");
					}
					out.append(result);
				}
				it_begin = m[0].second;
			}
			else {
				return ++pos;
			}

			return it_begin - source.begin();
		}
	};
}
#endif
