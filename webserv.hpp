#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <cstdlib>
#include <fstream>
#include "config.hpp"
#include "error.hpp"
#include "url.hpp"
#include <netinet/in.h>
#include <string>
#include <sys/types.h>

#ifndef MSG_BUF_SIZE
#define MSG_BUF_SIZE 4000
#endif

// echo -n "close" | nc localhost 8080

namespace Webserv {
	enum Method {
		Get,
		Post,
		Delete,
	};

	struct ServerData {
		Config::Server config;
		ushort port;
		int socketFd;
		sockaddr_in address;
		uint addressLen;
		Url rootPath;
		std::vector<std::pair<Url, Config::Server::Location> > locations;
	};

	struct ConnectionInfo {
		int connectionFd;
	};

	std::string makeErrorPage(Error);

	std::string readAll(std::ifstream&);
}

#endif