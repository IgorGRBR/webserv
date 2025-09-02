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
#include <string>


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

void ResponseHandler::setResponseHeader(const std::string& key, const std::string& value) {
    extraHeaders[key] = value;
}

Result<ResponseHandler*, Error> ResponseHandler::tryMake(ServerData& sd, ConnectionInfo& ci) {
	return new ResponseHandler(sd, ci);
}

Result<ResponseHandler*, Error> ResponseHandler::tryMakeErrorPage(ServerData & sd, ConnectionInfo & ci, Error error) {
	ResponseHandler* response = new ResponseHandler(sd, ci);

	std::string respContent = makeErrorPage(error);
	response->setResponseData(respContent);
	response->setResponseCode(error.getHTTPCode());
	response->setResponseContentType(HTML);

	return response;
}

Result<bool, Error> ResponseHandler::runTask(FDTaskDispatcher&) {
    if (!ready) return true;

    HTTPResponse response(Url(), responseCode);
    response.setData(responseData);
    response.setContentType(contentTypeString(contentType));

    // Add all extra headers before building the response string
    for (std::map<std::string, std::string>::iterator it = extraHeaders.begin(); it != extraHeaders.end(); ++it) {
        response.setHeader(it->first, it->second);
    }

    std::string responseStr = response.build();

    // Write response to client socket with error checking
    ssize_t written = write(conn.connectionFd, responseStr.c_str(), responseStr.length());
    if (written == -1) {
        perror("Write to socket failed");
        // Log or handle the error (optional)
    } else if (written < (ssize_t)responseStr.length()) {
        std::cerr << "Partial write to socket: " << written << " bytes written out of " << responseStr.length() << std::endl;
        // Handle partial write if necessary (not common for sockets)
    }

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
	std::cout << "Destroying response handler" << std::endl;
	close(conn.connectionFd);
}
