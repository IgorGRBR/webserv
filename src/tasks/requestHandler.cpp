#include "webserv.hpp"
#include "config.hpp"
#include "error.hpp"
#include "dispatcher.hpp"
#include "http.hpp"
#include "webserv.hpp"
#include "ystl.hpp"
#include <unistd.h>
#include "tasks.hpp"
#include <string>

#define SEND_ERROR(dispatcher, errorObj) { \
		Option<Error> critical = sendError(dispatcher, errorObj); \
		if (critical.isSome()) return critical.get(); \
		return false; \
	} \

typedef Webserv::Error Error;
typedef Webserv::Config::Server::Location Location;

typedef Webserv::RequestHandler RequestHandler;

RequestHandler::RequestHandler(const ServerData& sd, int cfd):
	IFDTask(cfd, READ_MODE),
	sData(sd),
	clientSocketFd(cfd),
	reqBuilder(),
	location(),
	dataSizeLimit(),
	chunked(false) {};

Result<RequestHandler*, Error> RequestHandler::tryMake(int cfd, ServerData &data) {
	RequestHandler* rHandler = new RequestHandler(data, cfd);
#ifdef DEBUG
	std::cout << "making new request handler for id: " << cfd << std::endl;
#endif
	return rHandler;
}

Result<bool, Error> RequestHandler::runTask(FDTaskDispatcher& dispatcher) {
	// char buffer[MSG_BUF_SIZE + 1] = {0};
	char* buffer = new char[sData.messageBufferSize + 1]();
	long readResult = read(clientSocketFd, (void*)buffer, sData.messageBufferSize);
	buffer[sData.messageBufferSize] = '\0';
	std::string bufStr = std::string(buffer, readResult);
	delete[] buffer;
#ifdef DEBUG
	std::cout << "Read result: " << readResult << "\nContent:\n" << bufStr << std::endl;
#endif

	if (readResult == 0) {
		// I have exactly zero clue why this happens, but this does happen occasionally when you
		// go back a page in the browser.
		if (reqBuilder.isChunked()) {
			Option<Error> maybeError = finalize(dispatcher);
			if (maybeError.isSome()) {
				Error& err = maybeError.get();
				if (err.tag == Error::SHUTDOWN_SIGNAL) {
					return err;
				}
				SEND_ERROR(dispatcher, err);
			}
		}
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
				SEND_ERROR(dispatcher, Error(HTTP_BAD_REQUEST, "Missing host"));
			}
			std::string host = maybeHost.get();
			if (sData.serverNames.find(host) == sData.serverNames.end()) {
				SEND_ERROR(dispatcher, Error(HTTP_FORBIDDEN, "Invalid host"));
			}
		}
		if (dataSizeLimit.isNone()) {
			location = sData.locations.tryFindLocation(reqBuilder.getHeaderPath().get());
			if (location.isSome()) {
				dataSizeLimit = location.get().location->maxRequestSize.getOr(sData.maxRequestSize);
			}
			else {
				SEND_ERROR(dispatcher, Error(Error::RESOURCE_NOT_FOUND, "Specified location not found"));
			}
			Option<HTTPMethod> method = reqBuilder.getHTTPMethod();
			if (
				method.isSome()
				&& method.get() != GET
				&& !reqBuilder.isChunked()
				&& method.get() != DELETE
				&& method.get() != HEAD
			) {
				Option<uint> maybeContLength = reqBuilder.getContentLength();
				if (maybeContLength.isNone()) {
					SEND_ERROR(dispatcher, Error(HTTP_LENGTH_REQUIRED, 
						"HTTP message is missing Content-Length header"));
				}
				uint contLength = maybeContLength.get();
				if (contLength > dataSizeLimit.get()) {
					SEND_ERROR(dispatcher, Error(HTTP_PAYLOAD_TOO_LARGE,
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
			if (size < limit || (reqBuilder.isChunked() && !reqBuilder.chunkedReadFinished())) {
				// Do nothing?
				return true;
			}
			else if (size == limit || (reqBuilder.isChunked() && reqBuilder.chunkedReadFinished())) {
				Option<Error> maybeError = finalize(dispatcher);
				if (maybeError.isSome()) {
					if (maybeError.get().tag == Error::SHUTDOWN_SIGNAL) {
						return maybeError.get();
					}
					SEND_ERROR(dispatcher, maybeError.get());
				}
				return false;
			}
			else {
				SEND_ERROR(dispatcher, Error(HTTP_PAYLOAD_TOO_LARGE, "HTTP message content length is too large!"));
			}
		}
		break;
		case HTTPRequest::Builder::CHUNKED_READ_COMPLETE: {
			Option<Error> maybeError = finalize(dispatcher);
			if (maybeError.isSome()) {
				Error& err = maybeError.get();
				if (err.tag == Error::SHUTDOWN_SIGNAL) {
					return err;
				}
				SEND_ERROR(dispatcher, err);
			}
		}
		break;
	}
	return false;
}

Option<Error> RequestHandler::sendError(FDTaskDispatcher& dispatcher, Error error) {
	ConnectionInfo conn;
	conn.connectionFd = clientSocketFd;
	return sendErrorPage(conn, sData, location, dispatcher, error);
}


Option<Error> RequestHandler::finalize(Webserv::FDTaskDispatcher& dispatcher) {
	Result<UniquePtr<HTTPRequest>, Error> maybeRequest = reqBuilder.build();
	if (maybeRequest.isError()) {
		return maybeRequest.getError();
	}

	UniquePtr<HTTPRequest> request = maybeRequest.getValue();

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
			return nextTask.getError();
		}
		dispatcher.registerTask(nextTask.getValue());
		Option<SharedPtr<CGIReader> > maybeReader = nextTask.getValue().tryAs<CGIReader>();
		if (maybeReader.isSome()) {
			dispatcher.registerTask(maybeReader.get()->getResponseHandler().tryAs<IFDTask>().get());
			if (maybeReader.get()->getWriter().isSome())
				dispatcher.registerTask(maybeReader.get()->getWriter().get().tryAs<IFDTask>().get());
		}
	}
	return NONE;
}

RequestHandler::~RequestHandler() {
#ifdef DEBUG
	std::cout << "unmaking request handler for id: " << clientSocketFd << std::endl;
#endif
}