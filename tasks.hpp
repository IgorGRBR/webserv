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
		static Result<ClientListener*, Error> tryMake(Config&, Config::Server&, char* envp[]);
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

		//declarations for custom error pages
		bool readErrorPageFromFile(const std::string& filePath, std::string& content);
			std::string determineContentType(const std::string& filePath);

		ServerData sData;
		int clientSocketFd;
		// Option<HTTPRequest> request;
		HTTPRequest::Builder reqBuilder;
		Option<LocationTreeNode::LocationSearchResult> location;
		Option<uint> dataSizeLimit;
	};

	// `ResponseHandler` is a task that is responsible for building a HTTP response and sending it back to the client.
	class ResponseHandler: public IFDTask {
	public:
		static Result<ResponseHandler*, Error> tryMake(ConnectionInfo&, const HTTPResponse&);
		// static Result<ResponseHandler*, Error> tryMakeErrorPage(ServerData&, ConnectionInfo&, Error);
		Result<bool, Error> runTask(FDTaskDispatcher&);
		int getDescriptor() const;
		IOMode getIOMode() const;
		~ResponseHandler();
		ResponseHandler(const ConnectionInfo&);
		void setResponse(const HTTPResponse& resp);
	private:
		ResponseHandler(ConnectionInfo&, const HTTPResponse&);

		// bool ready;
		// ServerData sData;
		ConnectionInfo conn;
		// std::string responseData;
		// HTTPReturnCode responseCode;
		// HTTPContentType contentType;
		// std::map<std::string, std::string> extraHeaders;
		Option<HTTPResponse> response;
	};

	class CGIWriter: public IFDTask, public IFDConsumer {
	public:
		// static Result<UniquePtr<CGIWriter>, Error> tryMake(int fd);
		CGIWriter(int fd, bool cont = false);

		Result<bool, Error> runTask(FDTaskDispatcher&);
		int getDescriptor() const;
		IOMode getIOMode() const;
		void consumeFileData(const std::string &);
		void close();

	private:
		int fd;
		bool continuous;
		bool closed;
		std::vector<std::string> writeBuffer;
	};

	class CGIReader: public IFDTask {
	public:
		// static Result<UniquePtr<CGIReader>, Error> tryMake(int fd, uint bufSize);
		CGIReader(
			ConnectionInfo conn,
			const SharedPtr<ResponseHandler>& resp,
			int pid,
			int fd,
			uint rSize = MSG_BUF_SIZE
		);

		Result<bool, Error> runTask(FDTaskDispatcher&);
		int getDescriptor() const;
		IOMode getIOMode() const;
		std::string readAll();
		void setWriter(const SharedPtr<CGIWriter> wPtr);
		Option<SharedPtr<CGIWriter> > getWriter();
		SharedPtr<ResponseHandler> getResponseHandler();
		// void onProcessExit(FDTaskDispatcher&);
		// int getPID() const;
	private:
		int fd;
		int pid;
		uint readSize;
		std::vector<std::string> readBuffer;
		Option<SharedPtr<CGIWriter> > writer;
		SharedPtr<ResponseHandler> responseHandler;
		ConnectionInfo connectionInfo;
	};

	typedef std::pair<SharedPtr<CGIWriter>, SharedPtr<CGIReader> > CGIPipeline;
	Result<CGIPipeline, Error> makeCGIPipeline(
		ConnectionInfo conn,
		const Url& binaryLocation,
		const Url& scriptLocation,
		const Url& extraPath,
		const HTTPRequest& request,
		char** envp
	);
}

#endif