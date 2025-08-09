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

// This macro is a temporary solution for specifying the buffer size of HTTP request messages server can listen to.
// TODO: Make it should be configureable within config file.
#ifndef MSG_BUF_SIZE
#define MSG_BUF_SIZE 4000
#endif

// `Webserv` is the main namespace we are working with. Everything related to the web server functionality
// should reside in this namespace.
namespace Webserv {

	// This enum represents the HTTP method of the received HTTP request.
	enum Method {
		Get,
		Post,
		Delete,
	};

	// `ServerData` contains the parsed configuration of the server instance.
	// TODO: Should probably remove the `config` field, and replace it with it's parsed variant as well.
	struct ServerData {
		Config::Server config;
		ushort port;
		int socketFd;
		sockaddr_in address;
		uint addressLen;
		Url rootPath;
		std::vector<std::pair<Url, Config::Server::Location> > locations;
	};

	// This struct will contain all the necessary details about current connection to the client.
	// It will be important when generating HTTP responses.
	struct ConnectionInfo {
		int connectionFd;
	};

	// A simple function that returns the layout of error page in HTML format as a string.
	std::string makeErrorPage(Error);

	// A function that constructs a directory listing for the provided path.
	// If `topLevel` is set to true, the generated listing must not contain a
	// listing for the parent directory; in other words - no `..` allowed if
	// `topLevel` is true.
	std::string makeDirectoryListing(const std::string& path, bool topLevel = true);

	// Reads everything from the input stream.
	std::string readAll(std::ifstream&);
}

#endif