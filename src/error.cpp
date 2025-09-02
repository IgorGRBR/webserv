#include "error.hpp"
#include "http.hpp"
#include <sys/types.h>

typedef Webserv::Error ServerError;

Webserv::Error::Error(ServerError::Tag t): tag(t) {}

Webserv::Error::Error(Tag t, const std::string& message):
	tag(t), message(message) {};

const char* Webserv::Error::getTagMessage() const {
	switch (tag) {
		case SOCKET_CREATION_FAILURE:
			return "Failed to create a socket";
		case SOCKET_BIND_FAILURE:
			return "Failed to bind a socket";
		case SOCKET_LISTEN_FAILURE:
			return "Failed to set listen mode for a socket";
		case SOCKET_ACCEPT_FAILURE:
			return "Failed to accept message from a socket";
		case HTTP_ERROR:
			return "Inner HTTP error";
		case SHUTDOWN_SIGNAL:
			return "Dummy shutdown signal";
		case PATH_PARSING_ERROR:
			return "Error in path construction";
		case GENERIC_ERROR:
			return "Generic ahh error. Tell the programmer they suck ass at their job";
		case EPOLL_ERROR:
			return "EPOLL error";
		case RESOURCE_NOT_FOUND:
			return "Resource not found";
		case FILE_NOT_FOUND:
			return "File not found";
		case ALLOC_ERROR:
			return "Allocation failure";
	}
	return "Unknown";
}

Webserv::HTTPReturnCode Webserv::Error::getHTTPCode() const {
	switch (tag) {
		case GENERIC_ERROR: return HTTP_INTERNAL_SERVER_ERROR;
		case SOCKET_CREATION_FAILURE: return HTTP_INTERNAL_SERVER_ERROR;
		case SOCKET_BIND_FAILURE: return HTTP_INTERNAL_SERVER_ERROR;
		case SOCKET_LISTEN_FAILURE: return HTTP_INTERNAL_SERVER_ERROR;
		case SOCKET_ACCEPT_FAILURE: return HTTP_INTERNAL_SERVER_ERROR;
		case HTTP_ERROR: return HTTP_INTERNAL_SERVER_ERROR;
		case SHUTDOWN_SIGNAL: return HTTP_OK;
		case PATH_PARSING_ERROR: return HTTP_FORBIDDEN;
		case EPOLL_ERROR: return HTTP_INTERNAL_SERVER_ERROR;
		case RESOURCE_NOT_FOUND: return HTTP_NOT_FOUND;
		case FILE_NOT_FOUND: return HTTP_NOT_FOUND;
		case ALLOC_ERROR: return HTTP_INTERNAL_SERVER_ERROR;
	}
	return HTTP_NONE;
}
