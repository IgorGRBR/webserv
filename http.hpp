#ifndef HTTP_HPP
#define HTTP_HPP

#include "url.hpp"
#include "ystl.hpp"
#include <map>
#include <ostream>
#include <string>
namespace Webserv {
	enum HTTPMethod {
		GET,
		POST,
		PUT,
		DELETE,
	};

	const char* httpMethodName(HTTPMethod);
	Option<HTTPMethod> httpMethodFromStr(const std::string &);

	enum HTTPRequestError {
		INVALID_REQUEST,
		INVALID_METHOD,
		NO_DATA_SEGMENT,
	};

	const char* httpRequestErrorMessage(HTTPRequestError);

	class HTTPRequest {
	public:
		static Result<HTTPRequest, HTTPRequestError> fromText(std::string&);

		const Url& getPath() const;
		const std::string& getData() const;
		HTTPMethod getMethod() const;
	
	private:
		HTTPRequest();
		Url path;
		std::string data;
		HTTPMethod method;
	};

	std::ostream& operator<<(std::ostream&, const HTTPRequest&);

	enum HTTPReturnCode {
		HTTP_NONE = 0,
		HTTP_OK = 200,
		HTTP_INTERNAL_SERVER_ERROR = 400,
		HTTP_FORBIDDEN = 403,
		HTTP_NOT_FOUND = 404,
	};

	const char* httpReturnCodeMessage(HTTPReturnCode);

	class HTTPResponse {
	public:
		HTTPResponse(Url, HTTPReturnCode = HTTP_NONE);
		void setData(const std::string&);
		void setContentType(const std::string&);
		void setHeader(const std::string&, const std::string&);
		void setCode(HTTPReturnCode);
		std::string build() const;

	private:
		Url resourcePath;
		HTTPReturnCode retCode;
		std::string data;
		std::map<std::string, std::string> headers;
	};
}

#endif