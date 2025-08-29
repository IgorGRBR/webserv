#ifndef TASKS
#define TASKS
#include "ystl.hpp"
#include "dispatcher.hpp"
#include "http.hpp"
#include "webserv.hpp"
#include <vector>

namespace Webserv {

	// `ClientListener` is a task that performs the act of establishing connections with the
	// HTTP clients connecting to the server on their port, and spawning the corresponding request handler.
	// This task is constantly alive and active.
	class ClientListener: public IFDTask {
	public:
		static Result<ClientListener*, Error> tryMake(Config&, Config::Server&);
		Result<bool, Error> runTask(FDTaskDispatcher&);
		int getDescriptor() const;
		IOMode getIOMode() const;
		~ClientListener();
	private:
		ClientListener(const ServerData&, int);
		ServerData sData;
		int socketFd;
	};

	class ChunkReader: public IFDTask {
	public:
		ChunkReader(ServerData, int, uint);
		Result<bool, Error> runTask(FDTaskDispatcher&);
		int getDescriptor() const;
		IOMode getIOMode() const;
	private:
		Option<Error> sendError(FDTaskDispatcher&, Error);
		Option<Error> parseSize(FDTaskDispatcher& dispatcher);
		
		ServerData sData;
		int clientSocketFd;
		std::vector<std::string> lines;
		Option<uint> specifiedSize;
		uint sizeLimit;
		uint size;
	};

	// `RequestHandler` is a task that reads the HTTP request contents from its file descriptor, parses it,
	// handles the requests and produces a response handler.
	class RequestHandler: public IFDTask {
	public:
		static Result<RequestHandler*, Error> tryMake(int, ServerData& data);
		Result<bool, Error> runTask(FDTaskDispatcher&);
		int getDescriptor() const;
		IOMode getIOMode() const;
		~RequestHandler();
	private:
		RequestHandler(const ServerData&, int);
		Option<Error> sendError(FDTaskDispatcher&, Error);

		ServerData sData;
		int clientSocketFd;
		// Option<HTTPRequest> request;
		HTTPRequest::Builder reqBuilder;
		Option<LocationTreeNode::LocationSearchResult> location;
		Option<uint> dataSizeLimit;
	};

	// `ResponseHandler` is a task that is responsible for building a HTTP response and sending it back to the client.
	class ResponseHandler: public IFDTask, public IFDConsumer {
	public:
		static Result<ResponseHandler*, Error> tryMake(ServerData&, ConnectionInfo&);
		static Result<ResponseHandler*, Error> tryMakeErrorPage(ServerData&, ConnectionInfo&, Error);
		Result<bool, Error> runTask(FDTaskDispatcher&);
		int getDescriptor() const;
		IOMode getIOMode() const;
		void consumeFileData(const std::string&);
		void setResponseData(const std::string&);
		void setResponseCode(HTTPReturnCode);
		void setResponseContentType(HTTPContentType);
		~ResponseHandler();
	private:
		ResponseHandler(ServerData&, ConnectionInfo&);

		bool ready;
		ServerData sData;
		ConnectionInfo conn;
		std::string responseData;
		HTTPReturnCode responseCode;
		HTTPContentType contentType;
	};

	class CGIReader: public IFDTask {
	public:
		Result<bool, Error> runTask(FDTaskDispatcher&);
		int getDescriptor() const;
		IOMode getIOMode() const;
	private:
	};

	class CGIWriter: public IFDTask {
	public:
		Result<bool, Error> runTask(FDTaskDispatcher&);
		int getDescriptor() const;
		IOMode getIOMode() const;
	private:
	};

	Result<std::pair<CGIWriter, CGIReader>, Error> makeCGIPipeline(const std::map<std::string, std::string>&);
}

#endif