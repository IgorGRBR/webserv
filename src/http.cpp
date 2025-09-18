#include "http.hpp"
#include "error.hpp"
#include "url.hpp"
#include "webserv.hpp"
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

	bool emptyLine = false;
	while (std::getline(s, line)) {
		if (line == "" || line == "\r") {
			emptyLine = true;
			break;
		}

		// Parse a header
		size_t colonPos = line.find(":");
		if (colonPos == line.npos) {
			return INVALID_REQUEST; // TODO: a better error message
		}
		std::string paramName = line.substr(0, colonPos);
		std::string paramValue = trimString(trimString(
				line.substr(colonPos + 1, line.size() - colonPos),
				' '
			), '\r');

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

bool Webserv::HTTPRequest::isForm() const {
	Option<std::string> maybeContentType = getHeader("Content-Type");
	if (maybeContentType.isNone()) return false;
	return maybeContentType.get().find("multipart/form-data") != std::string::npos;

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

std::string Webserv::HTTPRequest::toString() const {
	std::stringstream result;

	result << httpMethodName(method) << " " << path.toString() << " HTTP/1.1" << std::endl;
	
	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); it++) {
		result << it->first << ": " << it->second << std::endl;
	}

	result << std::endl << data;

	return result.str();
}

HTTPResponse::HTTPResponse(Webserv::Url uri, ReturnCode retCode): resourcePath(uri), retCode(retCode), headers() {
	headers["Content-Type"] = "text/html";
}

Option<Webserv::HTTPResponse> HTTPResponse::fromString(const std::string& text) {
	HTTPResponse response((Url()));
	std::stringstream s(text);

	std::string line;
	if (!std::getline(s, line)) {
		return NONE;
	}

	// Parse the start line
	std::stringstream firstLineStream(line);
	std::string word;

	// Path

	// First - we skip the HTTP version part.
	if (!std::getline(firstLineStream, word, ' ')) {
		return NONE;
	}

	// Now we read return code in to `word` itself.
	if (!std::getline(firstLineStream, word, ' ')) {
		return NONE;
	}

	Option<int> maybeCode = strToInt(word);
	if (maybeCode.isSome()) {
		response.retCode = static_cast<HTTPReturnCode>(maybeCode.get());
	}
	else {
		return NONE;
	}

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
			return NONE;
		}
		std::string paramName = word;

		if (!std::getline(wordStream, word, ':')) {
			return NONE;
		}
		std::string paramValue = trimString(trimString(word, ' '), '\r');

		response.headers[paramName] = paramValue;
	}

	// Now read the data segment if it is present
	if (!emptyLine) return NONE;
	std::getline(s, line, '\0');
	response.data = line;

	return response;
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
		case HTTP_CONTINUE:
			return "Continue";
		case HTTP_SWITCHING_PROTOCOLS:
			return "Switching protocols";
		case HTTP_PROCESSING:
			return "Processing";
		case HTTP_EARLY_HINTS:
			return "Early hints";
		case HTTP_CREATED:
			return "Created";
		case HTTP_ACCEPTED:
			return "Accepted";
		case HTTP_NON_AUTHORITATIVE_INFORMATION:
			return "Non-Autoritative Information";
		case HTTP_NO_CONTENT:
			return "No content";
		case HTTP_RESET_CONTENT:
			return "Reset content";
		case HTTP_PARTIAL_CONTENT:
			return "Partial content";
		case HTTP_MULTI_STATUS:
			return "Multi-status";
		case HTTP_ALREADY_REPORTED:
			return "Already reported";
		case HTTP_IM_USED:
			return "IM used";
		case HTTP_MULTIPLE_CHOICES:
			return "Multiple choices";
		case HTTP_MOVED_PERMANENTLY:
			return "Moved permanently";
		case HTTP_FOUND:
			return "Found";
		case HTTP_SEE_OTHER:
			return "See other";
		case HTTP_NOT_MODIFIED:
			return "Not modified";
		case HTTP_USE_PROXY:
			return "Use proxy";
		case HTTP_TEMPORARY_REDIRECT:
			return "Temporary redirect";
		case HTTP_PERMANENT_REDIRECT:
			return "Permanent redirect";
		case HTTP_BAD_REQUEST:
			return "Bad request";
		case HTTP_UNATHORIZED:
			return "Unauthorized";
		case HTTP_PAYMENT_REQUIRED:
			return "Payment required";
		case HTTP_METHOD_NOT_ALLOWED:
			return "Method not allowed";
		case HTTP_NOT_ACCEPTABLE:
			return "Not acceptable";
		case HTTP_PROXY_AUTHENTICATION_REQUIRED:
			return "Proxy authentication required";
		case HTTP_REQUEST_TIMEOUT:
			return "Request timeout";
		case HTTP_CONFLICT:
			return "Conflict";
		case HTTP_GONE:
			return "Gone";
		case HTTP_LENGTH_REQUIRED:
			return "Length required";
		case HTTP_PRECONDITION_FAILED:
			return "Precondition required";
		case HTTP_PAYLOAD_TOO_LARGE:
			return "Payload too large";
		case HTTP_URI_TOO_LONG:
			return "URI too long";
		case HTTP_UNSUPPORTED_MEDIA_TYPE:
			return "Unsupported media type";
		case HTTP_RANGE_NOT_SATISFIABLE:
			return "Range not satisfiable";
		case HTTP_EXPECTATION_FAILED:
			return "Expectation failed";
		case HTTP_IM_A_TEAPOT:
			return "I'm a teapot";
		case HTTP_MISDIRECTED_REQUEST:
			return "Misdirected request";
		case HTTP_UNPROCESSABLE_CONTENT:
			return "Unprocessable content";
		case HTTP_LOCKED:
			return "Locked";
		case HTTP_FAILED_DEPENDENCY:
			return "Failed dependency";
		case HTTP_TOO_EARLY:
			return "Too early";
		case HTTP_UPGRADE_REQUIRED:
			return "Upgrade required";
		case HTTP_PRECONDITION_REQUIRED:
			return "Precondition required";
		case HTTP_TOO_MANY_REQUESTS:
			return "Too many requests";
		case HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE:
			return "Request header fields too large";
		case HTTP_UNAVAILABLE_FOR_LEGAL_REASONS:
			return "Unavailable for legal reasons";
		case HTTP_NOT_IMPLEMENTED:
			return "Not implemented";
		case HTTP_BAD_GATEWAY:
			return "Bad gateway";
		case HTTP_SERVICE_UNAVAILABLE:
			return "Service unavailable";
		case HTTP_GATEWAY_TIMEOUT:
			return "Gateway timeout";
		case HTTP_VERSION_NOT_SUPPORTED:
			return "HTTP version not supported";
		case HTTP_VARIANT_ALSO_NEGOTIATES:
			return "Variant also negotiates";
		case HTTP_INSUFFICIENT_STORAGE:
			return "Insufficient storage";
		case HTTP_LOOP_DETECTED:
			return "Loop detected";
		case HTTP_NOT_EXTENDED:
			return "Not extended";
		case HTTP_NETWORK_AUTHENTICATION_REQUIRED:
			return "Network authentication required";
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

Builder::Builder():
	// This absolute BS language will yell at you for leaving complex data types in templates uninitialized,
	// yet will happily let you slip through an uninitialized enum. Fantastic.
	lines(), internalState(INITIAL) {};

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

Option<HTTPMethod> Builder::getHTTPMethod() const {
	if (request.isMoved())
		return NONE;
	else
		return request->getMethod();
}

Option<std::string> Builder::getHost() const {
	if (request.isMoved())
		return NONE;
	else
	 	return request->getHeader("Host");
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
