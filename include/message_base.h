#ifndef _CRAWLER_MESSAGE_BASE_H_
#define _CRAWLER_MESSAGE_BASE_H_

namespace tools {

	/* 
	 * message passed by message queue 
	 */
	template <typename _MessageCatagory>
	struct message_base {
	public:

		typedef _MessageCatagory message_catagory;

		virtual ~message_base() = default;
		virtual message_catagory catagory() const = 0;
	};
}

#endif