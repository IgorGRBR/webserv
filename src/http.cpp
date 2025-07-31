#include "http.hpp"
#include "url.hpp"
#include "ystl.hpp"
#include <cstdio>
#include <ostream>
#include <sstream>
#include <string>

typedef Webserv::HTTPMethod HTTPMethod;
typedef Webserv::HTTPRequest HTTPRequest;
typedef Webserv::HTTPRequestError HTTPRequestError;
typedef Webserv::HTTPReturnCode ReturnCode;
typedef Webserv::HTTPResponse HTTPResponse;
typedef Webserv::Url Url;

const char* Webserv::httpMethodName(HTTPMethod method) {
	switch (method) {
        case GET:
			return "GET";
        case POST:
			return "POST";
        case PUT:
			return "PUT";
        case DELETE:
			return "DELETE";
        }
	return "UNKNOWN";
};

Option<HTTPMethod> Webserv::httpMethodFromStr(const std::string &str) {
	std::string lower = strToLower(str);
	if (lower == "get") return Webserv::GET;
	else if (lower == "post") return Webserv::POST;
	else if (lower == "put") return Webserv::PUT;
	else if (lower == "delete") return Webserv::DELETE;
	return NONE;
};

const char* Webserv::httpRequestErrorMessage(HTTPRequestError err) {
	switch (err) {
        case INVALID_REQUEST:
			return "Invalid request";
        case NO_DATA_SEGMENT:
			return "Data segment of the request is missing";
		case INVALID_METHOD:
			return "Invalid HTTP method received";
	}
	return "unknown";
}

Webserv::HTTPRequest::HTTPRequest() {};

Result<HTTPRequest, HTTPRequestError> Webserv::HTTPRequest::fromText(std::string& text) {
	HTTPRequest request;
	std::stringstream s(text);

	std::string line;
	if (!std::getline(s, line)) {
		return INVALID_REQUEST; // TODO: a better error message
	}
	
	// Parse the start line
	std::stringstream firstLineStream(line);
	std::string word;

	// Method
	if (!std::getline(firstLineStream, word, ' ')) {
		return INVALID_REQUEST; // TODO: a better error message
	}

	Option<HTTPMethod> maybeMethod = httpMethodFromStr(word);
	if (maybeMethod.isSome()) {
		request.method = maybeMethod.get();
	}
	else {
		return INVALID_METHOD;
	}

	// Path
	if (!std::getline(firstLineStream, word, ' ')) {
		return INVALID_REQUEST; // TODO: a better error message
	}
	{
		Option<Url> maybePath = Url::fromString(word);
		if (maybePath.isNone()) return INVALID_REQUEST;
		request.path = maybePath.get();
	}

	// HTTP version
	if (!std::getline(firstLineStream, word, ' ')) {
		return INVALID_REQUEST; // TODO: a better error message
	}

	// Rest of the messages - ignore for now
	bool emptyLine = false;
	while (std::getline(s, line)) {
		if (line == "" || line == "\r") {
			emptyLine = true;
			break;
		}

		// Parse a header
		std::string word;
		std::stringstream wordStream(line);
		if (!std::getline(wordStream, word, ':')) {
			return INVALID_REQUEST; // TODO: a better error message
		}
		std::string paramName = word;

		if (!std::getline(wordStream, word, ':')) {
			return INVALID_REQUEST; // TODO: a better error message
		}
		std::string paramValue = trimString(word, ' ');

		request.headers[paramName] = paramValue;
	}

	// Now read the data segment if it is present
	if (!emptyLine) return NO_DATA_SEGMENT;
	std::getline(s, line, '\0');
	request.data = line;

	return request;
};

const Url& Webserv::HTTPRequest::getPath() const {
	return path;
};

const std::string& Webserv::HTTPRequest::getData() const {
	return data;
};

HTTPMethod Webserv::HTTPRequest::getMethod() const {
	return method;
};

std::ostream& Webserv::operator<<(std::ostream& os, const HTTPRequest& req) {
	os << "HTTPRequest(" << httpMethodName(req.getMethod()) << ", " 
		<< req.getPath().toString() << ", " << req.getData() << ")";
	return os;
};

HTTPResponse::HTTPResponse(Webserv::Url uri, ReturnCode retCode): resourcePath(uri), retCode(retCode), headers() {
	headers["Content-Type"] = "text/html";
}

void HTTPResponse::setData(const std::string& data) {
	this->data = data;
}

void HTTPResponse::setContentType(const std::string& ctype) {
	headers["Content-Type"] = ctype;
}

const char* Webserv::httpReturnCodeMessage(HTTPReturnCode retCode) {
	switch (retCode) {
        case HTTP_NONE:
			return "None";
        case HTTP_OK:
			return "Ok";
        case HTTP_INTERNAL_SERVER_ERROR:
			return "Internal server error";
        case HTTP_FORBIDDEN:
			return "Forbidden";
        case HTTP_NOT_FOUND:
			return "Not found";
	}
	return "Unknown";
}

void HTTPResponse::setHeader(const std::string& key, const std::string& value) {
	headers[key] = value;
}

void HTTPResponse::setCode(HTTPReturnCode code) {
	retCode = code;
}

std::string HTTPResponse::build() const {
	std::stringstream result;
	result << "HTTP/1.1 " << retCode << " " << httpReturnCodeMessage(retCode) << std::endl;

	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); it++) {
		result << it->first << ": " << it->second << std::endl;
	}
	result << "Content-Length: " << data.length() << std::endl;

	result << std::endl;

	result << data;

	return result.str();
}
