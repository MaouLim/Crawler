#ifndef _CRAWLER_DEBUG_H_
#define _CRAWLER_DEBUG_H_

#include <iostream>
#include <mutex>
#include <string>

#include <consoleapi2.h>
#include <winbase.h>

namespace tools {
	
	static std::mutex stdout_mutex;

	enum class debug_type {
		INFO, WARNING, FATAL
	};

	inline void log(
		debug_type         type, 
		const std::string& method, 
		const std::string& message
	) {
		std::lock_guard<std::mutex> locker(stdout_mutex);

		switch (type) {
			case debug_type::INFO : {
				SetConsoleTextAttribute(
					GetStdHandle(STD_OUTPUT_HANDLE), 
					FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
				);
				break;
			}

			case debug_type::WARNING : {
				SetConsoleTextAttribute(
					GetStdHandle(STD_OUTPUT_HANDLE),
					FOREGROUND_INTENSITY | FOREGROUND_BLUE
				);
				break;
			}

			case debug_type::FATAL: {
				SetConsoleTextAttribute(
					GetStdHandle(STD_OUTPUT_HANDLE),
					FOREGROUND_INTENSITY | FOREGROUND_RED
				);
				break;
			}

			default : { }
		}

		std::cout << "[" << std::this_thread::get_id() << "]\t" 
		          << method << ": " 
		          << message << std::endl;
	}
}

#endif