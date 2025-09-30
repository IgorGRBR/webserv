#include "webserv.hpp"
#include "config.hpp"
#include "error.hpp"
#include "dispatcher.hpp"
#include "http.hpp"
#include "ystl.hpp"
#include <unistd.h>
#include "tasks.hpp"
#include <string>


typedef Webserv::Error Error;
typedef Webserv::Config::Server::Location Location;
typedef Webserv::ResponseHandler ResponseHandler;

ResponseHandler::ResponseHandler(const ConnectionInfo& ci):
	IFDTask(ci.connectionFd, WRITE_MODE),
	conn(ci),
	response(NONE)
{}

ResponseHandler::ResponseHandler(ConnectionInfo& ci, const HTTPResponse& resp):
	IFDTask(ci.connectionFd, WRITE_MODE),
	conn(ci),
	response(resp)
{}

Result<ResponseHandler*, Error> ResponseHandler::tryMake(ConnectionInfo& ci, const HTTPResponse& resp) {
	return new ResponseHandler(ci, resp);
}

Result<bool, Error> ResponseHandler::runTask(FDTaskDispatcher&) {
	if (response.isNone()) return true;
	std::string responseStr = response.get().build();

	// Write response to client socket with error checking
	write(conn.connectionFd, responseStr.c_str(), responseStr.length());

	return false;
}

void ResponseHandler::setResponse(const HTTPResponse& resp) {
	response = resp;
}

ResponseHandler::~ResponseHandler() {
#ifdef DEBUG
	std::cout << "Destroying response handler" << std::endl;
#endif
	close(conn.connectionFd);
}
