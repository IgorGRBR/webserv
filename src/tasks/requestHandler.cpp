#include "webserv.hpp"
#include "config.hpp"
#include "error.hpp"
#include "dispatcher.hpp"
#include "http.hpp"
#include "webserv.hpp"
#include "ystl.hpp"
#include <unistd.h>
#include "tasks.hpp"

#define SEND_ERROR(dispatcher, errorObj) { \
		Option<Error> critical = sendError(dispatcher, errorObj); \
		if (critical.isSome()) return critical.get(); \
		return false; \
	} \

typedef Webserv::Error Error;
typedef Webserv::Config::Server::Location Location;

typedef Webserv::RequestHandler RequestHandler;

RequestHandler::RequestHandler(const ServerData& sd, int cfd):
	sData(sd),
	clientSocketFd(cfd),
	reqBuilder(),
	location(),
	dataSizeLimit() {};

Result<RequestHandler*, Error> RequestHandler::tryMake(int cfd, ServerData &data) {
	RequestHandler* rHandler = new RequestHandler(data, cfd);
#ifdef DEBUG
	std::cout << "making new request handler for id: " << cfd << std::endl;
#endif
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
	std::string bufStr = std::string(buffer, readResult);
#ifdef DEBUG
	std::cout << "Read result: " << readResult << "\nContent:\n" << bufStr << std::endl;
#endif

	if (readResult == 0) {
		// I have exactly zero clue why this happens, but this does happen occasionally when you
		// go back a page in the browser.
		return false;
	}

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
		if (sData.serverNames.size() > 0) {
			// Check if the request host header matches the name of the server.
			Option<std::string> maybeHost = reqBuilder.getHost();
			if (maybeHost.isNone()) {
				// TODO: replace the proper error tag
				SEND_ERROR(dispatcher, Error(Error::GENERIC_ERROR, "Missing host"));
			}
			std::string host = maybeHost.get();
			if (sData.serverNames.find(host) == sData.serverNames.end()) {
				SEND_ERROR(dispatcher, Error(Error::GENERIC_ERROR, "Invalid host"));
			}
		}
		if (dataSizeLimit.isNone()) {
			location = sData.locations.tryFindLocation(reqBuilder.getHeaderPath().get());
			if (location.isSome()) {
				dataSizeLimit = location.get().location->maxRequestSize.getOr(sData.maxRequestSize);
			}
			else {
				// TODO: make sure the error code is correct
				SEND_ERROR(dispatcher, Error(Error::RESOURCE_NOT_FOUND));
			}
			Option<HTTPMethod> method = reqBuilder.getHTTPMethod();
			if (method.isSome() && method.get() != GET) {
				Option<uint> maybeContLength = reqBuilder.getContentLength();
				if (maybeContLength.isNone()) {
					// TODO: replace with proper error code
					SEND_ERROR(dispatcher, Error(Error::HTTP_ERROR, 
						"HTTP message is missing Content-Length header"));
				}
				uint contLength = maybeContLength.get();
				if (contLength > dataSizeLimit.get()) {
					// TODO: set the correct error tag
					SEND_ERROR(dispatcher, Error(Error::GENERIC_ERROR,
						"HTTP message content length is too large!"));
				}
				dataSizeLimit = contLength;
			}
			else {
				dataSizeLimit = 0;
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
					SEND_ERROR(dispatcher, maybeRequest.getError());
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
					Result<SharedPtr<IFDTask>, Error> nextTask = handleRequest(
						request.ref(),
						location.get(),
						sData,
						clientSocketFd
					);
					if (nextTask.isError()) {
						SEND_ERROR(dispatcher, nextTask.getError());
					}
					dispatcher.registerTask(nextTask.getValue());
					Option<SharedPtr<CGIReader> > maybeReader = nextTask.getValue().tryAs<CGIReader>();
					if (maybeReader.isSome()) {
						dispatcher.registerTask(maybeReader.get()->getResponseHandler().tryAs<IFDTask>().get());
						if (maybeReader.get()->getWriter().isSome())
							dispatcher.registerTask(maybeReader.get()->getWriter().get().tryAs<IFDTask>().get());
					}
				}
				return false;
			}
			else {
				// TODO: set the correct error tag
				SEND_ERROR(dispatcher, Error(Error::GENERIC_ERROR, "HTTP message content length is too large!"));
			}
		}
		break;
	}
	return false;
}

// TODO: this code has been duplicated from `chunkReader.cpp`. Extracting it into a separate function is a good idea.
Option<Error> RequestHandler::sendError(FDTaskDispatcher& dispatcher, Error error) {
	ConnectionInfo conn;
	conn.connectionFd = clientSocketFd;

	HTTPResponse resp = HTTPResponse(Url());
	resp.setCode(error.getHTTPCode());
	resp.setData(makeErrorPage(error));
	resp.setContentType(contentTypeString(HTML));

	Result<ResponseHandler*, Error> maybeRespTask = ResponseHandler::tryMake(conn, resp);
	if (maybeRespTask.isError()) {
		return maybeRespTask.getError();
	}
	dispatcher.registerTask(maybeRespTask.getValue());
	return NONE;
}

RequestHandler::~RequestHandler() {
#ifdef DEBUG
	std::cout << "unmaking request handler for id: " << clientSocketFd << std::endl;
#endif
}