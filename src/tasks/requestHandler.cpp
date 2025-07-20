#include "webserv.hpp"
#include "config.hpp"
#include "error.hpp"
#include "dispatcher.hpp"
#include "http.hpp"
#include "url.hpp"
#include "webserv.hpp"
#include "ystl.hpp"
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <vector>
#include "tasks.hpp"

typedef Webserv::Error Error;
typedef Webserv::Config::Server::Location Location;


typedef Webserv::RequestHandler RequestHandler;

RequestHandler::RequestHandler(const ServerData& sd, int cfd): sData(sd), clientSocketFd(cfd) {};

Result<RequestHandler*, Error> RequestHandler::tryMake(int cfd, ServerData &data) {
	RequestHandler* rHandler = new RequestHandler(data, cfd);
	return rHandler;
}

int RequestHandler::getDescriptor() const {
	return clientSocketFd;
}

Webserv::IOMode RequestHandler::getIOMode() const {
	return READ_MODE;
}

Result<bool, Error> RequestHandler::runTask(FDTaskDispatcher& dispatcher) {
	Option<Error> error = handleRequest(dispatcher);

	if (error.isSome()) {
		if (error.get().tag == Error::SHUTDOWN_SIGNAL) {
			return error.get();
		}

		std::string respContent = makeErrorPage(error.get());
		ConnectionInfo conn;
		conn.connectionFd = clientSocketFd;
		Result<ResponseHandler*, Error> response = ResponseHandler::tryMake(sData, conn);
		if (response.isError()) {
			return response.getError();
		}
		response.getValue()->setResponseData(respContent);
		response.getValue()->setResponseCode(error.get().getHTTPCode());
		dispatcher.registerHandler(response.getValue());
	}

	return false;
}

Option<Error> RequestHandler::handleRequest(FDTaskDispatcher& dispatcher) {
	char buffer[MSG_BUF_SIZE] = {0}; // TODO: parametrize this
	long readResult = read(clientSocketFd, (void*)buffer, MSG_BUF_SIZE);

	std::cout << "Read result: " << readResult << "\nContent:\n" << buffer << std::endl;
	std::string bufStr = std::string(buffer);

	Result<HTTPRequest, HTTPRequestError> maybeRequest = HTTPRequest::fromText(bufStr);
	if (maybeRequest.isError()) {
		return Error(Error::HTTP_ERROR, httpRequestErrorMessage(maybeRequest.getError()));
	}

	HTTPRequest request = maybeRequest.getValue();
	std::cout << "Got this request:" << request << std::endl;
	if (request.getData() == "close") {
		return Error(Error::SHUTDOWN_SIGNAL);
	}

	// Analyze request
	for (std::vector<std::pair<Url, Location> >::const_iterator it = sData.locations.begin(); it != sData.locations.end(); it++) {
		if (request.getPath().matchSegments(it->first)) {
			Option<Error> error = handleLocation(it->first, it->second, request, dispatcher);
			if (error.isSome()) {
				return error;
			}

			return NONE;
		}
	}
	return NONE;
}

Option<Error> RequestHandler::handleLocation(
	const Url& path,
	const Config::Server::Location& location,
	HTTPRequest& request,
	FDTaskDispatcher& dispatcher
) {
	std::string root;
	if (location.root.isSome()) {
		root = location.root.get();
	}
	else if (sData.config.defaultRoot.isSome()) {
		root = sData.config.defaultRoot.get();
	}
	else {
		return Error(Error::HTTP_ERROR, "Missing root location in configuration");
	}

	ConnectionInfo conn;
	conn.connectionFd = clientSocketFd;
	
	Option<std::string> fileContent = NONE;
	Url rootUrl = Url::fromString(root).get();
	Url tail = request.getPath().tailDiff(path);

	// Just send the index file
	if (tail.getSegments().size() == 0) {
		std::string indexStr = location.index.getOr("index.html");
		Url index = Url::fromString(indexStr).get();

		std::string respFilePath = (rootUrl + tail + index).toString(false);
		std::cout << "Trying to load: " << respFilePath << std::endl;
		std::ifstream respFile(respFilePath.c_str());

		if (respFile.is_open()) {
			fileContent = readAll(respFile);
		}
	}
	else {
		std::string respFilePath = (rootUrl + tail).toString(false);
		std::cout << "Trying to load: " << respFilePath << std::endl;
		std::ifstream respFile(respFilePath.c_str());

		if (respFile.is_open()) {
			fileContent = readAll(respFile);
		}
	}

	if (fileContent.isSome()) {
		Result<ResponseHandler*, Error> response = ResponseHandler::tryMake(sData, conn);
		if (response.isError()) {
			return response.getError();
		}
		response.getValue()->setResponseData(fileContent.get());
		dispatcher.registerHandler(response.getValue());
		return NONE;
	}
	else {
		return Error(Error::FILE_NOT_FOUND, (rootUrl + tail).toString(false));
	}
	
	return Error(Error::GENERIC_ERROR, "Not implemented");
}
