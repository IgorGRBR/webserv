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
	response(NONE),
	writeStr(NONE)
{}

ResponseHandler::ResponseHandler(ConnectionInfo& ci, const HTTPResponse& resp):
	IFDTask(ci.connectionFd, WRITE_MODE),
	conn(ci),
	response(resp),
	writeStr(NONE)
{}

Result<ResponseHandler*, Error> ResponseHandler::tryMake(ConnectionInfo& ci, const HTTPResponse& resp) {
	return new ResponseHandler(ci, resp);
}

Result<bool, Error> ResponseHandler::runTask(FDTaskDispatcher&) {
	if (response.isNone()) return true;

	if (writeStr.isNone()) {
		writeStr = response.get().build();
	}

	// Write response to client socket with error checking
	long writeResult = write(conn.connectionFd, writeStr.get().c_str(), writeStr.get().length());

	if (writeResult <= 0) {
#ifdef DEBUG
		std::cerr << "write error :(" << std::endl;
#endif
		return false;
	}

	if (writeStr.get().length() - writeResult > 0) {
		writeStr = writeStr.get().substr(0, writeResult);
		return true;
	}

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
