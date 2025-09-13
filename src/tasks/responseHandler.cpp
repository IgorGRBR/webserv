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

ResponseHandler::ResponseHandler(ConnectionInfo& ci, const HTTPResponse& resp):
	conn(ci),
	response(resp)
{}

Result<ResponseHandler*, Error> ResponseHandler::tryMake(ConnectionInfo& ci, const HTTPResponse& resp) {
	return new ResponseHandler(ci, resp);
}


Result<bool, Error> ResponseHandler::runTask(FDTaskDispatcher&) {
	std::string responseStr = response.build();

	// Write response to client socket with error checking
	write(conn.connectionFd, responseStr.c_str(), responseStr.length());

	return false;
}

int ResponseHandler::getDescriptor() const {
	return conn.connectionFd;
}

Webserv::IOMode ResponseHandler::getIOMode() const {
	return WRITE_MODE;
}

ResponseHandler::~ResponseHandler() {
	std::cout << "Destroying response handler" << std::endl;
	close(conn.connectionFd);
}
