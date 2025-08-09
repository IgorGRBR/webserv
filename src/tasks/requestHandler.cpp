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
		dispatcher.registerTask(response.getValue());
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

	// TODO: maybe move this check into configuration parsing step?
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

	Url respFileUrl = rootUrl + tail;
	std::string respFilePath = respFileUrl.toString(false);
	std::cout << "Trying to load: " << respFilePath << std::endl;

	// Here we should check if the path is a directory or a file, and *then* send back the response.
	FSType fsType = checkFSType(respFilePath);
	switch (fsType) {
	case FS_NONE:
		fileContent = NONE;
		break;
	case FS_FILE: {
			std::ifstream respFile(respFilePath.c_str());
			if (respFile.is_open()) {
				fileContent = readAll(respFile);
			}
		}
		break;
	case FS_DIRECTORY:
		std::string indexStr = location.index.getOr("index.html");
		Url index = Url::fromString(indexStr).get();

		std::string indexFilePath = (respFileUrl + index).toString(false);
		std::cout << "Trying to load: " << indexFilePath << std::endl;
		std::ifstream respFile(indexFilePath.c_str());

		if (respFile.is_open()) { // Try to load index file
			fileContent = readAll(respFile);
		}
		else { // Try to create directory listing
			fileContent = makeDirectoryListing(respFilePath, tail.getSegments().size() == 0);
		}
		break;
	}

	if (fileContent.isSome()) {
		Result<ResponseHandler*, Error> response = ResponseHandler::tryMake(sData, conn);
		if (response.isError()) {
			return response.getError();
		}
		response.getValue()->setResponseData(fileContent.get());
		dispatcher.registerTask(response.getValue());
		return NONE;
	}
	else {
		return Error(Error::FILE_NOT_FOUND, respFilePath);
	}
	
	return Error(Error::GENERIC_ERROR, "Not implemented");
}
