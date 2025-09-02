#include "config.hpp"
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "ystl.hpp"

typedef Webserv::Config Config;
typedef Webserv::ConfigError ConfigError;
typedef Webserv::Token Token;

Token::Token(char c) {
	if (c == '(') tag = OPAREN;
	else if (c == ')') tag = CPAREN;
	else {
		tag = SYMBOL;
		sym = std::string(1, c);
	}
}

Token::Token(std::string str):
	tag(SYMBOL), sym(str) {}

const std::string Token::getSym() const {
	return sym;
}

Token::Tag Token::getTag() const {
	return tag;
}

const char* Token::getTagDesc() const {
	switch (tag) {
		case SYMBOL:
			return "SYMBOL";
		case OPAREN:
			return "OPAREN";
		case CPAREN:
			return "CPAREN";
	}
	return "IDK";
}

void tokenizeLine(std::vector<Token>& tokens, std::string& line, char& stringState, std::string& symBuf) {
	if (stringState == '\0' && symBuf.length() > 0) {
		tokens.push_back(Token(symBuf));
		symBuf = std::string();
	}
	const char* c = line.c_str();
	while (*c != '\0' && (stringState || *c != '#')) {
		if (stringState) {
			if (*c == '"' || *c == '\'') {
				stringState = '\0';
				tokens.push_back(Token(symBuf));
				symBuf = std::string();
				c++;
				continue;
			}
			symBuf.push_back(*c);
		}
		else {
			if (*c == '"' || *c == '\'') {
				stringState = *c;
				c++;
				continue;
			}

			if (*c == '(' || *c == ')') {
				if (symBuf.length() > 0) {
					tokens.push_back(Token(symBuf));
					symBuf = std::string();
				}
				tokens.push_back(Token(*c));
			}
			else if (std::isspace(*c)) {
				if (symBuf.length() > 0) {
					tokens.push_back(Token(symBuf));
					symBuf = std::string();
				}
			}
			else {
				symBuf.push_back(*c);
			}

			// if (*c == '(' || *c == ')') tokens.push_back(Token(*c));
			// else if (!std::isspace(*c)) symBuf.push_back(*c);
	
			// if ((*c == '(' || *c == ')' || std::isspace(*c))) {
			// 	if (*c == '(') tokens.push_back(Token(*c));
			// 	tokens.push_back(Token(symBuf));
			// 	symBuf = std::string();
			// }
		}
		c++;
	}
}

typedef Result<Config*, Webserv::ConfigError> ParseResult;
typedef Result<Config::Server, Webserv::ConfigError> ServerResult;
typedef Result<Config::Server::Location, Webserv::ConfigError> LocationResult;
struct ParserContext {
	Config& config;
	std::vector<Token>::iterator& it;
	std::vector<Token>::iterator& end;
	uint depth;
};

LocationResult parseLocationDirective(ParserContext &ctx) {
	if (ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
	
	Config::Server::Location location;
	std::string sym;
	location.allowedMethods = HTTP_ALL_FLAGS;
	while (ctx.it != ctx.end) {
		switch (ctx.it->getTag()) {
			case Webserv::Token::SYMBOL:
				sym = std::string(ctx.it->getSym());
				if (sym == "root") {
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					location.root = ctx.it->getSym();
				}
				else if (sym == "proxyPass") {
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					location.proxyPass = ctx.it->getSym();
				}
				else if (sym == "return") {
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					std::stringstream s(std::string(ctx.it->getSym()));
					// TODO: maybe handle an error here?
					ushort ret;
					s >> ret;
					location.retCode = ret;
				}
				else if (sym == "errorPage") {
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					std::stringstream s(std::string(ctx.it->getSym()));
					ushort errCode;
					s >> errCode;
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					location.errPages[errCode] = std::string(ctx.it->getSym());
				}
				else if (sym == "dirListing") { // Automatic directory listing
					location.dirListing = true;
				}
				else if (sym == "index") { // Default file
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					location.index = ctx.it->getSym();
				}
				else if (sym == "fileUpload") {
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					location.fileUploadFieldId = ctx.it->getSym();
				}
				else if (sym == "allowMethod") { // Allow single method
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					if (location.allowedMethods == HTTP_ALL_FLAGS) {
						location.allowedMethods = 0;
					}
					std::string metStr = ctx.it->getSym();
					Option<unsigned char> methodFlag = Webserv::strToHttpMethod(metStr);
					if (methodFlag.isSome()) {
						location.allowedMethods |= methodFlag.get();
					}
					else {
						return Webserv::UNEXPECTED_SYMBOL;
					}
				}
				else if (sym == "maxRequestSize") {
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					std::stringstream s(std::string(ctx.it->getSym()));

					uint maxSize;
					s >> maxSize;
					location.maxRequestSize = maxSize;
				}
				else if (sym == "redirect") {
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					location.redirection = ctx.it->getSym();
				}
				else return Webserv::UNEXPECTED_SYMBOL;
				break;
			case Webserv::Token::OPAREN:
				return Webserv::UNEXPECTED_OPAREN;
			case Webserv::Token::CPAREN:
				return location;
			default:
				return Webserv::UNEXPECTED_TOKEN;
		}
		ctx.it++;
	}
	return Webserv::UNEXPECTED_EOF;
}

ServerResult parseServerDirective(ParserContext& ctx) {
	if (ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;

	std::string sym;
	Config::Server server;
	while (ctx.it != ctx.end) {
		switch (ctx.it->getTag()) {
			case Webserv::Token::SYMBOL:
				sym = ctx.it->getSym();
				if (sym == "listen") {
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					std::stringstream s(std::string(ctx.it->getSym()));
					// TODO: maybe handle an error here?
					ushort port;
					s >> port;
					server.port = port;
				}
				else if (sym == "serverName") {
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() == Webserv::Token::SYMBOL) {
						server.serverNames.insert(ctx.it->getSym());
					}
					else if (ctx.it->getTag() == Webserv::Token::OPAREN) {
						while (ctx.it->getTag() != Webserv::Token::CPAREN) {
							if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
							switch (ctx.it->getTag()) {
							case Webserv::Token::SYMBOL:{
								server.serverNames.insert(ctx.it->getSym());
								break;
							}
							case Webserv::Token::CPAREN: {
								break;
							}
							default:
								return Webserv::UNEXPECTED_TOKEN;
							}
						}
					}
					else {
						return Webserv::UNEXPECTED_TOKEN;
					}
				}
				else if (sym == "location") {
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					std::string locationPath = ctx.it->getSym();

					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::OPAREN) return Webserv::UNEXPECTED_TOKEN;
					ctx.it++;

					LocationResult res = parseLocationDirective(ctx);
					if (res.isError()) return res.getError();
					server.locations[locationPath] = res.getValue();
				}
				else if (sym == "root") {
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					server.defaultRoot = Option<std::string>(ctx.it->getSym());
				}
				else if (sym == "maxRequestSize") {
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					std::stringstream s(std::string(ctx.it->getSym()));

					uint maxSize;
					s >> maxSize;
					server.maxRequestSize = maxSize;
				}
				else return Webserv::UNEXPECTED_SYMBOL;
				break;
			case Webserv::Token::OPAREN:
				return Webserv::UNEXPECTED_OPAREN;
			case Webserv::Token::CPAREN:
				return server;
			default:
				return Webserv::UNEXPECTED_TOKEN;
		}
		ctx.it++;
	}
	return Webserv::UNEXPECTED_EOF;
}

ParseResult parseConfig(ParserContext& ctx) {
	if (ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;

	std::string sym;
	ServerResult res = Webserv::CONFIG_PARSING_ERROR;
	ParseResult parseRes = Webserv::CONFIG_PARSING_ERROR;
	while (ctx.it != ctx.end) {
		switch (ctx.it->getTag()) {
			case Webserv::Token::SYMBOL:
				sym = std::string(ctx.it->getSym());
				if (sym == "server") {
					if (++ctx.it != ctx.end && ctx.it->getTag() == Webserv::Token::OPAREN) {
						ctx.it++;
						res = parseServerDirective(ctx);
						if (res.isOk()) ctx.config.servers.push_back(res.getValue());
						else return res.getError();
					}
					else return Webserv::UNEXPECTED_TOKEN;
				}
				else if (sym == "maxRequestSize") {
					if (++ctx.it == ctx.end) return Webserv::UNEXPECTED_EOF;
					if (ctx.it->getTag() != Webserv::Token::SYMBOL) return Webserv::UNEXPECTED_TOKEN;
					std::stringstream s(std::string(ctx.it->getSym()));

					uint maxSize;
					s >> maxSize;
					ctx.config.maxRequestSize = maxSize;
				}
				else return Webserv::UNEXPECTED_SYMBOL;
				break;
			case Webserv::Token::OPAREN:
				ctx.it++;
				parseRes = parseConfig(ctx);
				if (parseRes.isOk()) ctx.config = *parseRes.getValue();
				else return parseRes.getError();
				break;
			case Webserv::Token::CPAREN:
				if (ctx.depth == 0)
					return Webserv::UNEXPECTED_CPAREN;
				else
				 	return &ctx.config;
				break;
			default:
				return Webserv::UNEXPECTED_TOKEN;
		}
		ctx.it++;
	}

	if (ctx.depth == 0)
		return &ctx.config;
	else
	 	return Webserv::UNEXPECTED_EOF;
}

Result<Config, ConfigError> Webserv::readConfigFromFile(std::string path) {
	std::ifstream fileStream(path.c_str());

	if (!fileStream.is_open())
		return CONFIG_FILE_NOT_FOUND;

	std::string line;
	std::vector<Token> tokens;
	char stringState = '\0';
	std::string symBuf;
	while (std::getline(fileStream, line)) {
		tokenizeLine(tokens, line, stringState, symBuf);
	}

	Config config;
	config.defaultPort = 8080;
	config.maxRequestSize = 1000000; // TODO: maybe move this to a configurable macro?
	std::vector<Token>::iterator tokenIt = tokens.begin();
	std::vector<Token>::iterator tokenEnd = tokens.end();
	ParserContext context = (ParserContext) {
		.config = config,
		.it = tokenIt,
		.end = tokenEnd,
		.depth = 0,
	};
	Result<Config*, ConfigError> result = parseConfig(context);
	if (result.isError())
		return result.getError();

	return config;
}

Option<unsigned char> Webserv::strToHttpMethod(std::string& str) {
	std::string lcstr = strToLower(str);
	if (lcstr == "get") {
		return HTTP_GET_FLAG;
	}
	else if (lcstr == "post") {
		return HTTP_POST_FLAG;
	}
	else if (lcstr == "put") {
		return HTTP_PUT_FLAG;
	}
	else if (lcstr == "delete") {
		return HTTP_DELETE_FLAG;
	}
	return NONE;
}