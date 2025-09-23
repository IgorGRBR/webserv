#include "dispatcher.hpp"
#include "error.hpp"
#include "http.hpp"
#include "tasks.hpp"
#include "webserv.hpp"
#include <string>
#include <unistd.h>
#include <sstream>

#define SEND_ERROR(dispatcher, errorObj) { \
		Option<Error> critical = sendError(dispatcher, errorObj); \
		if (critical.isSome()) return critical.get(); \
		return false; \
	} \

namespace Webserv {
	ChunkReader::ChunkReader(ServerData sd, int fd, uint limit, HTTPRequest& req, LocationTreeNode::LocationSearchResult& loc):
		sData(sd),
		clientSocketFd(fd),
		lines(),
		specifiedSize(),
		sizeLimit(limit),
		size(0),
		request(req),
		location(loc)
	{
		(void)sizeLimit;
	};

	Result<bool, Error> ChunkReader::runTask(FDTaskDispatcher& dispatcher) {
		(void)dispatcher;
		char buffer[MSG_BUF_SIZE + 1] = {0};
		long readResult = read(clientSocketFd, (void*)buffer, sData.maxRequestSize);
	
		if (readResult == 0) {
			finalize(dispatcher);
			return false;
		}

		std::string chunkStr = std::string(buffer);

		if (specifiedSize.isNone()) {
			if (!lines.empty()) {
				if (lines.back()[lines.back().size() - 1] == '\r'
				&& chunkStr[0] == '\n') {
					Option<Error> critical = parseSize(dispatcher);
					if (critical.isSome()) return critical.get();
				}
			}

			lines.push_back(chunkStr);

			if (lines.back().find("\r\n") != std::string::npos) {
				Option<Error> critical = parseSize(dispatcher);
				if (critical.isSome()) return critical.get();
			}
		}
		if (specifiedSize.isSome()) {
			if (specifiedSize.get() == 0) {
				return false;
			}
			lines.push_back(chunkStr);
			uint limit = specifiedSize.get();
			if (size < limit) {
				return true;
			}
			else if (size == limit) {
				std::stringstream dataStream;
				for (uint i = 0; i < lines.size(); i++) {
					dataStream << lines[i];
				}
#ifdef DEBUG
				std::cout << "Received a chunk: " << dataStream.str() << std::endl;
#endif
				return true;
			}
			else {
				Option<Error> critical = sendError(dispatcher, Error(Error::GENERIC_ERROR, "Chunk data is too big"));
				return false;
			}
		}
		return false;
	};

	int ChunkReader::getDescriptor() const {
		return clientSocketFd;
	}

	IOMode ChunkReader::getIOMode() const {
		return READ_MODE;
	}

	Option<Error> ChunkReader::parseSize(FDTaskDispatcher& dispatcher) {
		std::stringstream dataStream;
		for (uint i = 0; i < lines.size(); i++) {
			dataStream << lines[i];
		}

		std::string dataString = dataStream.str();
		uint nlPos = dataString.find("\r\n");
		std::string sizeStr = dataString.substr(0, nlPos);

		specifiedSize = hexStrToUInt(sizeStr);

		if (specifiedSize.isNone()) {
			Option<Error> critical = sendError(dispatcher, Error(Error::GENERIC_ERROR, "Hex to int conversion failed"));
			if (critical.isSome()) return critical;
		}
		
		std::string rest = dataString.substr(nlPos + 1, dataString.size() - nlPos - 1);
		size = rest.size();
		lines.clear();
		lines.push_back(rest);
		return NONE;
	}

	Option<Error> ChunkReader::sendError(FDTaskDispatcher& dispatcher, Error error) {
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

	Option<Error> ChunkReader::finalize(FDTaskDispatcher& dispatcher) {
		std::stringstream dataStream;
		for (std::vector<std::string>::iterator it = lines.begin(); it != lines.end(); it++) {
			dataStream << *it;
		}
		HTTPRequest newRequest = request.withData(dataStream.str());

		// TODO: the following code was duplicated from `requestHandler.cpp`. Maybe move it to
		// `request.cpp`?
		Result<SharedPtr<IFDTask>, Error> nextTask = handleRequest(
			request,
			location,
			sData,
			clientSocketFd
		);

		if (nextTask.isError()) {
			Option<Error> critical = sendError(dispatcher, nextTask.getError());
			if (critical.isSome()) return critical.get();
			return NONE;
		}

		dispatcher.registerTask(nextTask.getValue());
		Option<SharedPtr<CGIReader> > maybeReader = nextTask.getValue().tryAs<CGIReader>();
		if (maybeReader.isSome()) {
			dispatcher.registerTask(maybeReader.get()->getResponseHandler().tryAs<IFDTask>().get());
			if (maybeReader.get()->getWriter().isSome())
				dispatcher.registerTask(maybeReader.get()->getWriter().get().tryAs<IFDTask>().get());
		}
		return NONE;
	}
}