#ifndef TASKS
#define TASKS
#include "ystl.hpp"
#include "dispatcher.hpp"
#include "http.hpp"
#include "webserv.hpp"

namespace Webserv {
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