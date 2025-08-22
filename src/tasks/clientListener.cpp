#include "webserv.hpp"
#include "config.hpp"
#include "error.hpp"
#include "dispatcher.hpp"
#include "url.hpp"
#include "ystl.hpp"
#include <cstring>
#include <unistd.h>
#include "tasks.hpp"

typedef Webserv::Error Error;
typedef Webserv::Config::Server::Location Location;
typedef Webserv::ClientListener ClientListener;

ClientListener::ClientListener(const ServerData& data, int fd): sData(data), socketFd(fd) {}

Result<ClientListener*, Error> ClientListener::tryMake(Config& config, Config::Server &serverConfig) {
	ServerData sData;

	ushort port = serverConfig.port.getOr(config.defaultPort);

	if (serverConfig.defaultRoot.isNone()) {
		sData.rootPath = Url();
	}
	else {
		Option<Url> maybePath = Url::fromString(serverConfig.defaultRoot.get());
		if (maybePath.isNone()) {
			sData.rootPath = Url();
		}
		else {
			sData.rootPath = maybePath.get();
		}
	}

	for (std::map<std::string, Location>::const_iterator it = serverConfig.locations.begin(); it != serverConfig.locations.end(); it++) {
		Option<Url> url = Url::fromString(it->first);
		if (url.isNone()) {
			return Error(Error::PATH_PARSING_ERROR);
		}
		sData.locations.insertLocation(url.get(), &it->second);
	}

	sData.maxRequestSize = serverConfig.maxRequestSize.getOr(config.maxRequestSize);

	// So, lets start with making a socket object
	sData.socketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (sData.socketFd == -1) {
		return Error(Error::SOCKET_CREATION_FAILURE);
	}

	sData.addressLen = sizeof(sData.address);
	sData.address.sin_family = AF_INET;
	sData.address.sin_addr.s_addr = INADDR_ANY;
	sData.address.sin_port = htons(port);
    std::memset(sData.address.sin_zero, '\0', sizeof(sData.address.sin_zero));

	if (bind(sData.socketFd, (sockaddr*)&sData.address, sData.addressLen) < 0) {
		close(sData.socketFd);
		return Error(Error::SOCKET_BIND_FAILURE);
	}

	if (listen(sData.socketFd, 200) < 0) { //TODO: figure out what to do with the second parameter
		close(sData.socketFd);
		return Error(Error::SOCKET_LISTEN_FAILURE);
	}

	ClientListener *listener = new ClientListener(sData, sData.socketFd);
	return listener;
}

int ClientListener::getDescriptor() const {
	return socketFd;
}

Webserv::IOMode ClientListener::getIOMode() const {
	return READ_MODE;
}

Result<bool, Webserv::Error> ClientListener::runTask(FDTaskDispatcher& dispatcher) {
	int clientSocket = accept(socketFd, (sockaddr *)&sData.address, (socklen_t *)&sData.addressLen);
	if (clientSocket < 0) {
		close(socketFd);
		return Error(Error::SOCKET_ACCEPT_FAILURE);
	}

	Result<RequestHandler*, Error> reqHandler = RequestHandler::tryMake(clientSocket, sData);
	if (reqHandler.isError()) {
		return reqHandler.getError();
	}
	dispatcher.registerTask(reqHandler.getValue());
	return true;
}

ClientListener::~ClientListener() {
	close(socketFd);
}