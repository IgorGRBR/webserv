#ifndef TASKS
#define TASKS
#include "ystl.hpp"
#include "dispatcher.hpp"
#include "http.hpp"
#include "webserv.hpp"

namespace Webserv {

	// `ClientListener` is a task that performs the act of establishing connections with the
	// HTTP clients connecting to the server on their port, and spawning the corresponding request handler.
	// This task is constantly alive and active.
	class ClientListener: public IFDTask {
	public:
		static Result<ClientListener*, Error> tryMake(Config::Server&, ushort);
		Result<bool, Error> runTask(FDTaskDispatcher&);
		int getDescriptor() const;
		IOMode getIOMode() const;
		~ClientListener();
	private:
		ClientListener(const ServerData&, int);
		ServerData sData;
		int socketFd;
	};

	// `RequestHandler` is a task that reads the HTTP request contents from its file descriptor, parses it,
	// handles the requests and produces a response handler.
	class RequestHandler: public IFDTask {
	public:
		static Result<RequestHandler*, Error> tryMake(int, ServerData& data);
		Result<bool, Error> runTask(FDTaskDispatcher&);
		Option<Error> handleRequest(FDTaskDispatcher&);
		int getDescriptor() const;
		IOMode getIOMode() const;
		Option<Error> handleLocation(const Url&, const Config::Server::Location&, HTTPRequest&, FDTaskDispatcher&);
	private:
		RequestHandler(const ServerData&, int);
		
		ServerData sData;
		int clientSocketFd;
	};

	// `ResponseHandler` is a task that is responsible for building a HTTP response and sending it back to the client.
	class ResponseHandler: public IFDTask, public IFDConsumer {
	public:
		static Result<ResponseHandler*, Error> tryMake(ServerData&, ConnectionInfo&);
		Result<bool, Error> runTask(FDTaskDispatcher&);
		int getDescriptor() const;
		IOMode getIOMode() const;
		void consumeFileData(const std::string&);
		void setResponseData(const std::string&);
		void setResponseCode(HTTPReturnCode);
		~ResponseHandler();
	private:
		ResponseHandler(ServerData&, ConnectionInfo&);

		bool ready;
		ServerData sData;
		ConnectionInfo conn;
		std::string responseData;
		HTTPReturnCode responseCode;
	};
}

#endif