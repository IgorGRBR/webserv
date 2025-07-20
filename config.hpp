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
	// Stores configuration for the web server
	struct Config {
		struct Server {
			struct Location {
				Option<std::string> root;
				std::map<ushort, std::string> errPages;
				Option<std::string> proxyPass;
				ushort retCode;
				bool dirListing;
				Option<std::string> index;
				unsigned char allowedMethods;
			};

			std::map<std::string, Location> locations;
			Option<ushort> port;
			Option<std::string> serverName;
			Option<std::string> defaultRoot;
		};

		std::vector<Server> servers;
		ushort defaultPort;
	};

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
	
	enum ConfigError {
		CONFIG_FILE_NOT_FOUND,
		UNEXPECTED_OPAREN,
		UNEXPECTED_CPAREN,
		UNEXPECTED_SYMBOL,
		UNEXPECTED_EOF,
		UNEXPECTED_TOKEN,
		CONFIG_PARSING_ERROR,
	};

	Result<Config, ConfigError> readConfigFromFile(std::string path);

	Option<unsigned char> strToHttpMethod(std::string&);
}

#endif