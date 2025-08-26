#include "dispatcher.hpp"
#include "error.hpp"
#include "tasks.hpp"
#include "webserv.hpp"
#include <string>
#include <unistd.h>
#include <sstream>

namespace Webserv {
	ChunkReader::ChunkReader(ServerData sd, int fd, uint limit):
		sData(sd),
		clientSocketFd(fd),
		lines(),
		specifiedSize(),
		sizeLimit(limit),
		size(0) {
			(void)sizeLimit; // <= here
		};

	Result<bool, Error> ChunkReader::runTask(FDTaskDispatcher& dispatcher) {
		(void)dispatcher;
		char buffer[MSG_BUF_SIZE + 1] = {0};
		long readResult = read(clientSocketFd, (void*)buffer, sData.maxRequestSize);
	
		if (readResult == sData.maxRequestSize && buffer[readResult] != '\0') {
			return Error(Error::GENERIC_ERROR, "Read result was larger than allowed.");
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

				std::cout << "Received a chunk: " << dataStream.str() << std::endl;

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

		Result<ResponseHandler*, Error> maybeRequest = ResponseHandler::tryMakeErrorPage(sData, conn, error);
		if (maybeRequest.isError()) {
			return maybeRequest.getError();
		}
		dispatcher.registerTask(maybeRequest.getValue());
		return NONE;
	}

}