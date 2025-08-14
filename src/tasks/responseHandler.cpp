#include "webserv.hpp"
#include "config.hpp"
#include "error.hpp"
#include "dispatcher.hpp"
#include "http.hpp"
#include "url.hpp"
#include "ystl.hpp"
#include <cstring>
#include <unistd.h>
#include "tasks.hpp"

typedef Webserv::Error Error;
typedef Webserv::Config::Server::Location Location;
typedef Webserv::ResponseHandler ResponseHandler;

ResponseHandler::ResponseHandler(ServerData& sd, ConnectionInfo& ci):
	ready(false),
	sData(sd),
	conn(ci),
	responseCode(HTTP_OK),
	contentType(BYTE_STREAM)
{}

Result<ResponseHandler*, Error> ResponseHandler::tryMake(ServerData& sd, ConnectionInfo& ci) {
	return new ResponseHandler(sd, ci);
}

Result<bool, Error> ResponseHandler::runTask(FDTaskDispatcher&) {
	if (!ready) return true;
	HTTPResponse response(Url(), responseCode);
	response.setData(responseData);
	response.setContentType(contentTypeString(contentType));
	std::string responseStr = response.build();

	// Now respond!
	write(conn.connectionFd, responseStr.c_str(), responseStr.length());
	return false;
}

int ResponseHandler::getDescriptor() const {
	return conn.connectionFd;
}

Webserv::IOMode ResponseHandler::getIOMode() const {
	return WRITE_MODE;
}

void ResponseHandler::consumeFileData(const std::string& buffer) {
	if (buffer.length() > 0) {
		responseData += buffer;
	}
	else {
		ready = true;
	}
}

void ResponseHandler::setResponseData(const std::string& data) {
	responseData = data;
	ready = true;
}

void ResponseHandler::setResponseCode(HTTPReturnCode code) {
	responseCode = code;
}

void ResponseHandler::setResponseContentType(Webserv::HTTPContentType cType) {
	contentType = cType;
	std::cout << "(DEBUG) cType: " << contentTypeString(cType) << std::endl;
}

ResponseHandler::~ResponseHandler() {
	close(conn.connectionFd);
}
