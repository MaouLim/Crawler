#ifndef _CRAWLER_MESSAGE_QUEUE_H_
#define _CRAWLER_MESSAGE_QUEUE_H_

#include <deque>
#include <memory>

#include <bounded_blocking_queue.h>
#include <message_base.h>

namespace tools {

	template <typename _MessageCatagoty>
	using msg_ptr = std::shared_ptr<message_base<_MessageCatagoty>>;

	template <typename _MessageCatagoty>
	using message_queue = bounded_blocking_queue<
		msg_ptr<_MessageCatagoty>,
		std::deque<msg_ptr<_MessageCatagoty>>
	>;

}

#endif