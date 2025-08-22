#include "http.hpp"
#include "error.hpp"
#include "url.hpp"
#include "ystl.hpp"
#include <cstddef>
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

Option<uint> Webserv::HTTPRequest::getContentLength() const {
	Option<std::string> contentLengthStr = getHeader("Content-Length");
	if (contentLengthStr.isNone()) {
		return NONE;
	}

	uint contentLength;
	std::stringstream contentLengthStream(contentLengthStr.get());
	contentLengthStream >> contentLength;

	return contentLength;
}

std::ostream& Webserv::operator<<(std::ostream& os, const HTTPRequest& req) {
	os << "HTTPRequest(" << httpMethodName(req.getMethod()) << ", "
		<< req.getPath().toString() << ", " << req.getData() << ")";
	return os;
};

void Webserv::HTTPRequest::setData(const std::string& str) {
	data = str;
}


Option<std::string> Webserv::HTTPRequest::getHeader(const std::string& key) const {
	if (headers.find(key) != headers.end()) {
		return headers.at(key);
	}
	return NONE;
}

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

std::string Webserv::contentTypeString(HTTPContentType cType) {
	switch (cType) {
		case PLAIN_TEXT:
			return ("text/plain");
		case HTML:
			return ("text/html");
		case JS:
			return ("text/javascript");
		case CSS:
			return ("text/css");
		case PNG:
			return ("image/png");
		case JPEG:
			return ("image/jpeg");
		default:
			return ("application/octet-stream");
	};
	return "";
}

Webserv::HTTPContentType Webserv::getContentType(const Url& url) {
	std::string extensions[] = {"txt", "html", "js", "css", "png", "jpeg"};
	Option<std::string> urlOpt = url.getExtension();
	if (urlOpt.isNone()) {
		return (BYTE_STREAM);
	}
	int	i = 0;
	std::string urlExt = strToLower(urlOpt.get());

	// TOASK: Why is this check here?
	if (!urlExt.empty() && urlExt[0] == '.')
		urlExt = urlExt.substr(1);
	for (i = 0; i < 6; i++) {
		if (urlExt == extensions[i]){
			break;
		}
	}
	switch (i) {
		case (0):
			return (PLAIN_TEXT);
		case (1):
			return (HTML);
		case (2):
			return (JS);
		case (3):
			return (CSS);
		case (4):
			return (PNG);
		case (5):
			return (JPEG);
		default:
			return (BYTE_STREAM);
	}
}

typedef Webserv::HTTPRequest::Builder Builder;

Builder::Builder() {};

Result<Builder::State, Webserv::Error> Builder::appendData(const std::string& str) {
	bool headerComplete = false;
	switch (internalState) {
        case INITIAL:
			if (lines.size() > 0) {
				// This is so inefficient...
				std::string lastCombined = lines.back() + str;
				if (lastCombined.find("\r\n\r\n") != std::string::npos) {
					internalState = HEADER_COMPLETE;
					headerComplete = true;
				}
			}
			
			lines.push_back(str);

			if (lines.back().find("\r\n\r\n") != std::string::npos) {
				internalState = HEADER_COMPLETE;
				headerComplete = true;
			}
			break;
        case HEADER_COMPLETE:
			lines.push_back(str);
			dataSize += str.size();
			break;
        }

	if (headerComplete) {
		std::stringstream collectedDataStream;
		for (uint i = 0; i < lines.size(); i++) {
			collectedDataStream << lines[i];
		}

		std::string collectedDataString = collectedDataStream.str();
		size_t doubleNlPos = collectedDataString.find("\r\n\r\n");
		std::string headerStr = collectedDataString.substr(0, doubleNlPos + 4);

		std::string rest = collectedDataString.substr(doubleNlPos + 4, collectedDataString.size() - doubleNlPos);
		lines.clear();
		lines.push_back(rest);
		dataSize = rest.size();

		HTTPRequest* reqPtr = new HTTPRequest();
		Result<HTTPRequest, HTTPRequestError> maybeRequest = HTTPRequest::fromText(headerStr);
		if (maybeRequest.isError()) {
			return Error(Error::HTTP_ERROR, httpRequestErrorMessage(maybeRequest.getError()));
		}
		*reqPtr = maybeRequest.getValue();
		request = reqPtr;
	}

	return internalState;
}

Option<Webserv::Url> Builder::getHeaderPath() const {
	if (request.isMoved())
		return NONE;
	else
		return request->getPath();
}

Option<uint> Builder::getContentLength() const {
	if (request.isMoved())
		return NONE;
	else
	 	return request->getContentLength();
}

uint Builder::getDataSize() const {
	return dataSize;
}

Result<UniquePtr<Webserv::HTTPRequest>, Webserv::Error> Builder::build() {
	std::stringstream collectedDataStream;
	for (uint i = 0; i < lines.size(); i++) {
		collectedDataStream << lines[i];
	}
	request->setData(collectedDataStream.str());
	return request;
}
