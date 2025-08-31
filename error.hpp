#ifndef ERROR_HPP
#define ERROR_HPP

#include "http.hpp"
#include <string>
#include <sys/types.h>
namespace Webserv {

	// `Error` is a generic error type that represents an error that might have occured
	// during the runtime of the server. Most of these are intended to be handle-able and recovereable.
	struct Error {

		// A simple tag that represents the kind of a error.
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
			FORM_PARSING_ERROR,
		};

		// Constructs an error instance with provided tag and no associated message.
		Error(Tag);

		// Constructs an error instance with provided tag and a message that can provide additional context.
		Error(Tag, const std::string& message);

		// Returns a short description of the error tag.
		const char* getTagMessage() const;

		// Returns an HTTP return code that can be associated with the error. 
		HTTPReturnCode getHTTPCode() const;

		Tag tag;
		std::string message;
	};
}

#endif