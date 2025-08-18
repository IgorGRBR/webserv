#include "webserv.hpp"
#include "config.hpp"
#include "error.hpp"
#include "dispatcher.hpp"
#include "http.hpp"
#include "webserv.hpp"
#include "ystl.hpp"
#include <cstring>
#include <unistd.h>
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
	char* buffer = new char[sData.maxRequestSize + 1];
	long readResult = read(clientSocketFd, (void*)buffer, sData.maxRequestSize);

	if (readResult == sData.maxRequestSize && buffer[readResult] != '\0') {
		delete[] buffer;
		return Error(Error::GENERIC_ERROR, "Read result was larger than allowed.");
	}
	buffer[readResult] = '\0';

	std::string bufStr = std::string(buffer);
	delete[] buffer;
#ifdef DEBUG
	std::cout << "Read result: " << readResult << "\nContent:\n" << bufStr << std::endl;
#endif

	Result<HTTPRequest, HTTPRequestError> maybeRequest = HTTPRequest::fromText(bufStr);
	if (maybeRequest.isError()) {
		return Error(Error::HTTP_ERROR, httpRequestErrorMessage(maybeRequest.getError()));
	}

	request = maybeRequest.getValue();
	std::cout << "Got this request:" << request.get() << std::endl;
	if (request.get().getData() == "close") {
		return Error(Error::SHUTDOWN_SIGNAL);
	}

	Result<IFDTask*, Error> nextTask = handleRequest(request.get(), sData, clientSocketFd);

	if (nextTask.isError()) {
		return nextTask.getError();
	}
	dispatcher.registerTask(nextTask.getValue());

	return false;
}
