#include "config.hpp"
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "ystl.hpp"

#ifndef MSG_BUF_SIZE
#define MSG_BUF_SIZE 20000
#endif

#ifndef MAX_REQ_SIZE
#define MAX_REQ_SIZE 1000000
#endif

namespace Webserv {

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
			}
			c++;
		}
	}
	
	typedef Result<Config*, ConfigError> ParseResult;
	typedef Result<Config::Server, ConfigError> ServerResult;
	typedef Result<Config::Server::Location, ConfigError> LocationResult;
	struct ParserContext {
		Config& config;
		std::vector<Token>::iterator& it;
		std::vector<Token>::iterator& end;
		uint depth;
	};
	
	LocationResult parseLocationDirective(ParserContext &ctx) {
		if (ctx.it == ctx.end) return UNEXPECTED_EOF;
		
		Config::Server::Location location;
		location.allowCGI = false;
		location.dirListing = false;
		std::string sym;
		location.allowedMethods = HTTP_ALL_FLAGS;
		while (ctx.it != ctx.end) {
			switch (ctx.it->getTag()) {
				case Token::SYMBOL:
					sym = std::string(ctx.it->getSym());
					if (sym == "root") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						location.root = ctx.it->getSym();
					}
					else if (sym == "proxyPass") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						location.proxyPass = ctx.it->getSym();
					}
					else if (sym == "errorPage") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						std::stringstream s(std::string(ctx.it->getSym()));
						ushort errCode;
						if (!(s >> errCode)) return NOT_A_NUMBER;
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						location.errPages[errCode] = std::string(ctx.it->getSym());
					}
					else if (sym == "dirListing") { // Automatic directory listing
						location.dirListing = true;
					}
					else if (sym == "index") { // Default file
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						location.index = ctx.it->getSym();
					}
					else if (sym == "fileUpload") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						location.fileUploadFieldId = ctx.it->getSym();
					}
					else if (sym == "allowMethod") { // Allow single method
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						if (location.allowedMethods == HTTP_ALL_FLAGS) {
							location.allowedMethods = 0;
						}
						std::string metStr = ctx.it->getSym();
						Option<unsigned char> methodFlag = strToHttpMethod(metStr);
						if (methodFlag.isSome()) {
							location.allowedMethods |= methodFlag.get();
						}
						else {
							return UNEXPECTED_SYMBOL;
						}
					}
					else if (sym == "maxRequestSize") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						std::stringstream s(std::string(ctx.it->getSym()));
	
						uint maxSize;
						if (!(s >> maxSize)) return NOT_A_NUMBER;
						location.maxRequestSize = maxSize;
					}
					else if (sym == "allowCGI") {
						location.allowCGI = true;
					}
					else if (sym == "redirect") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						location.redirection = ctx.it->getSym();
					}
					else return UNEXPECTED_SYMBOL;
					break;
				case Token::OPAREN:
					return UNEXPECTED_OPAREN;
				case Token::CPAREN:
					return location;
				default:
					return UNEXPECTED_TOKEN;
			}
			ctx.it++;
		}
		return UNEXPECTED_EOF;
	}
	
	ServerResult parseServerDirective(ParserContext& ctx) {
		if (ctx.it == ctx.end) return UNEXPECTED_EOF;
	
		std::string sym;
		Config::Server server;
		server.optional = false;
		while (ctx.it != ctx.end) {
			switch (ctx.it->getTag()) {
				case Token::SYMBOL:
					sym = ctx.it->getSym();
					if (sym == "listen") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						std::stringstream s(std::string(ctx.it->getSym()));
						ushort port;
						if (!(s >> port)) return NOT_A_NUMBER;
						server.port = port;
					}
					else if (sym == "optional") {
						server.optional = true;
					}
					else if (sym == "serverName") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() == Token::SYMBOL) {
							server.serverNames.insert(ctx.it->getSym());
						}
						else if (ctx.it->getTag() == Token::OPAREN) {
							while (ctx.it->getTag() != Token::CPAREN) {
								if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
								switch (ctx.it->getTag()) {
								case Token::SYMBOL:{
									server.serverNames.insert(ctx.it->getSym());
									break;
								}
								case Token::CPAREN: {
									break;
								}
								default:
									return UNEXPECTED_TOKEN;
								}
							}
						}
						else {
							return UNEXPECTED_TOKEN;
						}
					}
					else if (sym == "location") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						std::string locationPath = ctx.it->getSym();
	
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::OPAREN) return UNEXPECTED_TOKEN;
						ctx.it++;
	
						LocationResult res = parseLocationDirective(ctx);
						if (res.isError()) return res.getError();
						server.locations[locationPath] = res.getValue();
					}
					else if (sym == "root") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						server.defaultRoot = Option<std::string>(ctx.it->getSym());
					}
					else if (sym == "maxRequestSize") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						std::stringstream s(std::string(ctx.it->getSym()));
	
						uint maxSize;
						if (!(s >> maxSize)) return NOT_A_NUMBER;
						server.maxRequestSize = maxSize;
					}
					else if (sym == "errorPage") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						std::stringstream s(std::string(ctx.it->getSym()));
						ushort errCode;
						if (!(s >> errCode)) return NOT_A_NUMBER;
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						server.errPages[errCode] = std::string(ctx.it->getSym());
					}
					else return UNEXPECTED_SYMBOL;
					break;
				case Token::OPAREN:
					return UNEXPECTED_OPAREN;
				case Token::CPAREN:
					return server;
				default:
					return UNEXPECTED_TOKEN;
			}
			ctx.it++;
		}
		return UNEXPECTED_EOF;
	}
	
	ParseResult parseConfig(ParserContext& ctx) {
		if (ctx.it == ctx.end) return UNEXPECTED_EOF;
	
		std::string sym;
		ServerResult res = CONFIG_PARSING_ERROR;
		ParseResult parseRes = CONFIG_PARSING_ERROR;
		while (ctx.it != ctx.end) {
			switch (ctx.it->getTag()) {
				case Token::SYMBOL:
					sym = std::string(ctx.it->getSym());
					if (sym == "server") {
						if (++ctx.it != ctx.end && ctx.it->getTag() == Token::OPAREN) {
							ctx.it++;
							res = parseServerDirective(ctx);
							if (res.isOk()) ctx.config.servers.push_back(res.getValue());
							else return res.getError();
						}
						else return UNEXPECTED_TOKEN;
					}
					else if (sym == "maxRequestSize") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						std::stringstream s(std::string(ctx.it->getSym()));
	
						uint maxSize;
						if (!(s >> maxSize)) return NOT_A_NUMBER;
						ctx.config.maxRequestSize = maxSize;
					}
					else if (sym == "messageBufferSize") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() != Token::SYMBOL) return UNEXPECTED_TOKEN;
						std::stringstream s(std::string(ctx.it->getSym()));
	
						uint size;
						if (!(s >> size)) return NOT_A_NUMBER;
						ctx.config.maxRequestSize = size;
					}
					else if (sym == "cgiBinds") {
						if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
						if (ctx.it->getTag() == Token::OPAREN) {
							while (true) {
								if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
								if (ctx.it->getTag() == Token::CPAREN)
									break;
	
								if (ctx.it->getTag() != Token::SYMBOL) {
									return UNEXPECTED_TOKEN;
								}
								std::string fileExt = ctx.it->getSym();
								if (++ctx.it == ctx.end) return UNEXPECTED_EOF;
	
								if (ctx.it->getTag() != Token::SYMBOL) {
									return UNEXPECTED_TOKEN;
								}
								std::string execPath = ctx.it->getSym();
	
								ctx.config.cgiBinds[fileExt] = execPath;
							}
						}
						else {
							return UNEXPECTED_TOKEN;
						}
					}
					else return UNEXPECTED_SYMBOL;
					break;
				case Token::OPAREN:
					ctx.it++;
					parseRes = parseConfig(ctx);
					if (parseRes.isOk()) ctx.config = *parseRes.getValue();
					else return parseRes.getError();
					break;
				case Token::CPAREN:
					if (ctx.depth == 0)
						return UNEXPECTED_CPAREN;
					else
						 return &ctx.config;
					break;
				default:
					return UNEXPECTED_TOKEN;
			}
			ctx.it++;
		}
	
		if (ctx.depth == 0)
			return &ctx.config;
		else
			 return UNEXPECTED_EOF;
	}
	
	Result<Config, ConfigError> readConfigFromFile(std::string path) {
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
		config.maxRequestSize = MAX_REQ_SIZE;
		config.messageBufferSize = MSG_BUF_SIZE;
		config.cgiBinds["py"] = "/usr/bin/python3";
		config.cgiBinds["lua"] = "/usr/bin/luajit";
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
	
	Option<unsigned char> strToHttpMethod(std::string& str) {
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

	std::string configErrorDescription(ConfigError ce) {
		switch (ce) {
			case CONFIG_FILE_NOT_FOUND: return "File not found";
			case UNEXPECTED_OPAREN: return "Unexpected opening parenthesis occured";
			case UNEXPECTED_CPAREN: return "Unexpected closing parenthesis occured";
			case UNEXPECTED_SYMBOL: return "Unexpected symbol occured";
			case UNEXPECTED_EOF: return "Unexpected end-of-file";
			case UNEXPECTED_TOKEN: return "Unexpected token occured";
			case CONFIG_PARSING_ERROR: return "Parsing error";
			case NOT_A_NUMBER: return "Not a number";
		}
		return "";
	}
}
