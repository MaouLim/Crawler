/*
 * Created  by Maou Lim on 2018-07-05
 * Modified by ty       on 2018-07-06
 */

#ifndef _CRAWLER_FILTER_H_
#define _CRAWLER_FILTER_H_

#include <bitset>

#include <messages.h>

namespace tools {

	static const int seeds[] = { 5, 7, 11, 13, 31, 37, 61, 67, 71, 73, 79 };

	inline size_t RSHash(const char* msg, int seed) {
		size_t a = 63689;
		size_t hash = 0;

		while (*msg) {
			hash = hash * a + (*msg++);
			a *= seed;
		}

		return (hash & 0x7FFFFFFF);
	}

	template <typename _Tp>
	class filter {
	public:
		typedef _Tp value_type;

		virtual ~filter() = default;

		template <typename _ForwardItr, typename _Container>
		size_t reduce(_ForwardItr first, _ForwardItr last, _Container& out) {
			size_t count = 0;

			while (first != last) {
				if (test(*first)) {
					out.push_back(*first);
					++count; ++first;
				}
			}

			return count;
		}

		/*
		 * @note  this method is to test an item whether it
		 *        will pass the filter
		 */
		virtual bool test(const value_type&) = 0;
	};
}

namespace crawler {

	/*
	 * Created by ty on 2018-07-06
	 */
	template <size_t _M, size_t _N>
	class bloom_filter : public tools::filter<url_message> {

		typedef bloom_filter<_M, _N>       self_type;
		typedef tools::filter<url_message> base_type;

	public:
		typedef typename base_type::value_type value_type;

		bloom_filter() = default;
		~bloom_filter() = default;

		bool test(const value_type& msg) override {
			bool exist = true;

			for (size_t i = 0; i < BF_k; ++i) {
				auto key = 
					tools::RSHash(msg.url().c_str(), tools::seeds[i]) % _M;

				exist &= bit[key];
				bit[key] = 1;
			}

			return !exist;
		}

	private:
		static const size_t BF_k = _M / _N * 0.693147181;

		/* 
		 * k:number of the hash functions 
		 * m:the size of bitset 
		 * n:number of strings to hash 
		 * (k = [m/n]*ln2)
		 */
		std::bitset<_M>       bit;
	};
}

#endif