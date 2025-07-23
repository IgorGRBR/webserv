#ifndef CONFIG_HPP
#define CONFIG_HPP
#include <map>
#include <string>
#include <vector>
#include "ystl.hpp"

#define HTTP_GET_FLAG (1<<0)
#define HTTP_POST_FLAG (1<<1)
#define HTTP_PUT_FLAG (1<<2)
#define HTTP_DELETE_FLAG (1<<3)
#define HTTP_ALL_FLAGS (HTTP_GET_FLAG | HTTP_POST_FLAG | HTTP_PUT_FLAG | HTTP_DELETE_FLAG)

namespace Webserv {
	// `Config` stores parsed configuration for the web server
	struct Config {

		// `Config::Server` stores parsed configuration of each "subserver". 
		struct Server {

			// `Config::Server::Location` stores parsed configuration of each "subserver" location property.
			struct Location {

				// The path to the root directory, from which the subserver will serve files upon the request.
				Option<std::string> root;

				// Uuuh, not entirely sure on this one. Perhaps it stores paths to each individual error page, based on
				// the return code of that error. Not implemented yet...
				std::map<ushort, std::string> errPages;

				// I forgor.
				Option<std::string> proxyPass;

				// The default return code (???)
				ushort retCode;

				// Specifies whether there should be directory listing being served instead of an index file.
				bool dirListing;

				// Optional index file for the location.
				Option<std::string> index;

				// Flags for the HTTP methods this location is allowed to process. Not implemented yet.
				unsigned char allowedMethods;
			};

			// A map of locations and their paths.
			std::map<std::string, Location> locations;

			// Optional port that will be used for this server.
			Option<ushort> port;

			// Optional name that will be necessary to communicate with the server. Not implemented yet.
			Option<std::string> serverName;

			// Default root path. Only works if there is no "/" location defined. Also not yet implemented.
			Option<std::string> defaultRoot;
		};

		// List of servers
		std::vector<Server> servers;
		
		// Default port of each server
		ushort defaultPort;
	};

	// Token parsing stuff. Only used to parse the configuration, which is basically solved at this point, so no
	// need to delve here.
	class Token {
	public:
		enum Tag {
			SYMBOL,
			OPAREN,
			CPAREN,
		};
		Token(char);
		Token(std::string);

		const char *getTagDesc() const;
		const std::string getSym() const;
		Token::Tag getTag() const;
	private:
		Token();
		Tag tag;
		std::string sym;
	};
	
	// Represents an error that might have occured during the parsing of the config file.
	enum ConfigError {
		CONFIG_FILE_NOT_FOUND,
		UNEXPECTED_OPAREN,
		UNEXPECTED_CPAREN,
		UNEXPECTED_SYMBOL,
		UNEXPECTED_EOF,
		UNEXPECTED_TOKEN,
		CONFIG_PARSING_ERROR,
	};

	// Attempts to read and parse configuration from the file provided by the `path`. Returns either a `Config` object,
	// or an error that might have occured.
	Result<Config, ConfigError> readConfigFromFile(std::string path);

	// Converts a string to an unsigned character, representing a set of flags for HTTP methods.
	Option<unsigned char> strToHttpMethod(std::string&);
}

#endif