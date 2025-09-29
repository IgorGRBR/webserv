#include "webserv.hpp"
#include "config.hpp"
#include "error.hpp"
#include "dispatcher.hpp"
#include "http.hpp"
#include "webserv.hpp"
#include "ystl.hpp"
#include <unistd.h>
#include "tasks.hpp"
#include <fstream>
#include <string>
#include <cctype>  // for tolower

#define SEND_ERROR(dispatcher, errorObj) { \
		Option<Error> critical = sendError(dispatcher, errorObj); \
		if (critical.isSome()) return critical.get(); \
		return false; \
	} \

#define SEND_ERROR_OVERRIDE(dispatcher, errorObj) { \
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
			if (method.isSome() && method.get() != GET && !reqBuilder.isChunked() && method.get() != DELETE) {
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
	}
	return false;
}

Option<Error> RequestHandler::sendError(FDTaskDispatcher& dispatcher, Error error, HTTPReturnCode codeOverride) {
	ConnectionInfo conn;
	conn.connectionFd = clientSocketFd;

	HTTPResponse resp = HTTPResponse(Url());
	resp.setCode(codeOverride == HTTP_NONE? error.getHTTPCode() : codeOverride);

	std::string errorPageContent;
	bool customPageFound = false;
	std::string errorPagePath;

	// Check location-specific error pages first
	if (location.isSome()) {
		const std::map<ushort, std::string>& locErrPages = location.get().location->errPages;
		std::map<ushort, std::string>::const_iterator it = locErrPages.find(error.getHTTPCode());
		if (it != locErrPages.end()) {
			errorPagePath = it->second;
		}
	}

	// If no location error page found, check server-level error pages
	if (errorPagePath.empty()) {
		std::map<ushort, std::string>::const_iterator sit = sData.config.errPages.find(error.getHTTPCode());
		if (sit != sData.config.errPages.end()) {
			errorPagePath = sit->second;
		}
	}

	// Attempt to read custom error page file
	if (!errorPagePath.empty()) {
		if (readErrorPageFromFile(errorPagePath, errorPageContent)) {
			customPageFound = true;
		}
#ifdef DEBUG
		else {
			std::cerr << "Failed to read custom error page file: " << errorPagePath << std::endl;
		}
#endif
	}

	if (customPageFound) {
		resp.setData(errorPageContent);
		resp.setContentType(determineContentType(errorPagePath));
	} else {
		resp.setData(makeErrorPage(error));
		resp.setContentType(contentTypeString(HTML));
	}

	Result<ResponseHandler*, Error> maybeRespTask =
		ResponseHandler::tryMake(conn, resp);
	if (maybeRespTask.isError()) {
		return maybeRespTask.getError();
	}
	dispatcher.registerTask(maybeRespTask.getValue());
	return NONE;
}


bool RequestHandler::readErrorPageFromFile(const std::string& filePath, std::string& content) {
	std::ifstream file(filePath.c_str());
	if (!file.is_open()) {
#ifdef DEBUG
		std::cerr << "Failed to open error page file: " << filePath << std::endl;
#endif
		return false;
	}

	std::string line;
	content.clear();
	while (std::getline(file, line)) {
		content += line + "\n";
	}

	file.close();
	return !content.empty();
}


std::string RequestHandler::determineContentType(const std::string& filePath) {
	// Find last dot for extension
	size_t dotPos = filePath.rfind('.');
	if (dotPos == std::string::npos) {
		return contentTypeString(HTML); // Default to HTML if no extension
	}

	std::string ext = filePath.substr(dotPos + 1);
	// Convert extension to lowercase (manual C++98 style)
	for (size_t i = 0; i < ext.size(); ++i) {
		ext[i] = (char)tolower(ext[i]);
	}

	if (ext == "html" || ext == "htm") {
		return contentTypeString(HTML);
	} else if (ext == "txt") {
		return "text/plain";  // Directly return plain text MIME type string here
	} else {
   		return contentTypeString(HTML); // Default fallback
	}

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