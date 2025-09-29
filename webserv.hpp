#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <fstream>
#include "config.hpp"
#include "dispatcher.hpp"
#include "error.hpp"
#include "http.hpp"
#include "locationTree.hpp"
#include "url.hpp"
#include "ystl.hpp"
#include <netinet/in.h>
#include <string>
#include <sys/types.h>

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
	struct ServerData {
		Config::Server config;
		ushort port;
		int socketFd;
		sockaddr_in address;
		uint addressLen;
		Url rootPath;
		uint maxRequestSize;
		LocationTreeNode locations;
		std::set<std::string> serverNames;
		std::map<std::string, std::string> cgiInterpreters;
		char** envp;
		uint messageBufferSize;
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
	std::string makeDirectoryListing(
		const std::string& diskPath,
		const std::string& urlPath,
		bool topLevel = true,
		bool fileUploads = false
	);

	// Reads everything from the input stream.
	std::string readAll(std::ifstream&);

	Result<SharedPtr<IFDTask>, Error> handleRequest(
		HTTPRequest& request,
		LocationTreeNode::LocationSearchResult& query,
		ServerData& sData,
		int clientSocketFd);

	Option<uint> hexStrToUInt(const std::string&);

	Option<std::string> getFileExtension(const std::string& fileName);

	Option<int> strToInt(const std::string& str);
}

#endif