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
	char buffer[MSG_BUF_SIZE + 1] = {0};
	long readResult = read(clientSocketFd, (void*)buffer, MSG_BUF_SIZE);
	std::string bufStr = std::string(buffer);
	(void)readResult;
#ifdef DEBUG
	std::cout << "Read result: " << readResult << "\nContent:\n" << bufStr << std::endl;
#endif
	Result<HTTPRequest::Builder::State, Error> state = reqBuilder.appendData(bufStr);

	if (state.isError()) {
		Option<Error> critical = sendError(dispatcher, state.getError());
		if (critical.isSome()) return critical.get();
		return false;
	};

	switch (state.getValue()) {
	case HTTPRequest::Builder::INITIAL:
		return true; // Do nothing I guess?
		break;
	case HTTPRequest::Builder::HEADER_COMPLETE:
		if (dataSizeLimit.isNone()) {
			location = sData.locations.tryFindLocation(reqBuilder.getHeaderPath().get());
			if (location.isSome()) {
				dataSizeLimit = location.get().location->maxRequestSize.getOr(sData.maxRequestSize);
			}
			else {
				// TODO: make sure the error code is correct
				Option<Error> critical = sendError(dispatcher, Error(Error::RESOURCE_NOT_FOUND));
				if (critical.isSome()) return critical.get();
				return false;
			}
			Option<uint> contLength = reqBuilder.getContentLength();
			if (contLength.isSome()) {
				if (contLength.get() > dataSizeLimit.get()) {
					// TODO: set the correct error tag
					Option<Error> critical = sendError(dispatcher, Error(Error::GENERIC_ERROR,
						"HTTP message content length is too large!"));
					if (critical.isSome()) return critical.get();
					return false;
				}
				dataSizeLimit = contLength;
			}
		}
		if (dataSizeLimit.isSome()) {
			uint limit = dataSizeLimit.get();
			uint size = reqBuilder.getDataSize();
			if (size < limit) {
				// Do nothing?
				return true;
			}
			else if (size == limit) {
				Result<UniquePtr<HTTPRequest>, Error> maybeRequest = reqBuilder.build();
				if (maybeRequest.isError()) {
					Option<Error> critical = sendError(dispatcher, maybeRequest.getError());
					if (critical.isSome()) return critical.get();
					return false;
				}

				UniquePtr<HTTPRequest> request = maybeRequest.getValue();
				if (false) { // TODO: chunked check
					dispatcher.registerTask(new ChunkReader(sData, clientSocketFd, 
						location.get().location->maxRequestSize.getOr(sData.maxRequestSize)));
					return false;
				}
	
				if (request->getData() == "close") {
					return Error(Error::SHUTDOWN_SIGNAL);
				}
				else {
					Result<IFDTask*, Error> nextTask = handleRequest(
						request.ref(),
						location.get(),
						sData,
						clientSocketFd
					);
					if (nextTask.isError()) {
						Option<Error> critical = sendError(dispatcher, nextTask.getError());
						if (critical.isSome()) return critical.get();
						return false;
					}
					dispatcher.registerTask(nextTask.getValue());
				}
				return false;
			}
			else {
				// TODO: set the correct error tag
				Option<Error> critical = sendError(dispatcher, Error(Error::GENERIC_ERROR,
					"HTTP message content length is too large!"));
				if (critical.isSome()) return critical.get();
				return false;
			}
		}
		break;
	}
	return false;
}

Option<Error> RequestHandler::sendError(FDTaskDispatcher& dispatcher, Error error) {
	ConnectionInfo conn;
	conn.connectionFd = clientSocketFd;

	Result<ResponseHandler*, Error> maybeRequest = ResponseHandler::tryMakeErrorPage(sData, conn, error);
	if (maybeRequest.isError()) {
		return maybeRequest.getError();
	}
	dispatcher.registerTask(maybeRequest.getValue());
	return NONE;
}

RequestHandler::~RequestHandler() {
	std::cout << "Destroying request handler" << std::endl;
}