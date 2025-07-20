#ifndef ERROR_HPP
#define ERROR_HPP

#include "http.hpp"
#include <string>
#include <sys/types.h>
namespace Webserv {
	struct Error {
		enum Tag {
			GENERIC_ERROR,
			SOCKET_CREATION_FAILURE,
			SOCKET_BIND_FAILURE,
			SOCKET_LISTEN_FAILURE,
			SOCKET_ACCEPT_FAILURE,
			HTTP_ERROR,
			SHUTDOWN_SIGNAL,
			PATH_PARSING_ERROR,
			EPOLL_ERROR,
			RESOURCE_NOT_FOUND,
			FILE_NOT_FOUND,
			ALLOC_ERROR,
		};

		Error(Tag);
		Error(Tag, const std::string& message);
		const char* getTagMessage() const;
		HTTPReturnCode getHTTPCode() const;

		Tag tag;
		std::string message;
	};
}

#endif