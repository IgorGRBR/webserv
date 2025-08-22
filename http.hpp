#ifndef HTTP_HPP
#define HTTP_HPP

#include "url.hpp"
#include "ystl.hpp"
#include <map>
#include <ostream>
#include <string>
namespace Webserv {

	// This represents the method of HTTP message.
	enum HTTPMethod {
		GET,
		POST,
		PUT,
		DELETE,
	};

	// Returns a name of the HTTP method as a c-string.
	const char* httpMethodName(HTTPMethod);

	// Attempts to parse HTTP method from a string.
	Option<HTTPMethod> httpMethodFromStr(const std::string &);

	// Represents an error that might have occured when parsing the HTTP request.
	enum HTTPRequestError {
		INVALID_REQUEST,
		INVALID_METHOD,
		NO_DATA_SEGMENT,
	};

	// Returns a short description of the HTTP request error.
	const char* httpRequestErrorMessage(HTTPRequestError);

	struct Error;

	// `HTTPRequest` is a container that contains parsed HTTP request data.
	// TODO: Parse the request header into `std::map<std::string, std::string>`.
	class HTTPRequest {
	public:
		class Builder {
		public:
			enum State {
				INITIAL,
				HEADER_COMPLETE,
			};
			Builder();
			Result<State, Error> appendData(const std::string&);
			Result<UniquePtr<HTTPRequest>, Error> build();
			uint getDataSize() const;
			Option<Url> getHeaderPath() const;
			Option<uint> getContentLength() const;
		private:
			uint dataSize;
			std::vector<std::string> lines;
			State internalState;
			UniquePtr<HTTPRequest> request;
		};

		// Attempts to construct a `HTTPRequest` instance from provided text.
		static Result<HTTPRequest, HTTPRequestError> fromText(std::string&);

		// Returns the path of the request as an `Url`.
		const Url& getPath() const;

		// Retrieves the data segment of the request.
		const std::string& getData() const;

		// Retrieves the HTTP method of the request.
		HTTPMethod getMethod() const;

		Option<uint> getContentLength() const;

		Option<std::string> getHeader(const std::string&) const;
	
	private:
		// It's here so you can't construct an empty HTTPRequest.
		void setData(const std::string&);
		HTTPRequest();
		Url path;
		std::string data;
		HTTPMethod method;
		std::map<std::string, std::string> headers;
	};

	std::ostream& operator<<(std::ostream&, const HTTPRequest&);

	// This enum represents possible return codes a server may respond with.
	// TODO: refactor this thing into a struct of static ushorts. We are *NOT* goint to be writing down
	// every single HTTP return code into this enum by hand...
	enum HTTPReturnCode {
		HTTP_NONE = 0,
		HTTP_OK = 200,
		HTTP_INTERNAL_SERVER_ERROR = 400,
		HTTP_FORBIDDEN = 403,
		HTTP_NOT_FOUND = 404,
	};

	// Returns a short description of the HTTP return code.
	const char* httpReturnCodeMessage(HTTPReturnCode);

	// `HTTPResponse` is a builder that builds a HTTP response with the provided information.
	class HTTPResponse {
	public:

		// Constructs an `HTTPResponse` with the provided url and return code.
		// The default content type of a response is `text/html`.
		HTTPResponse(Url, HTTPReturnCode = HTTP_NONE);

		// Sets the data segment of the response to the provided string.
		void setData(const std::string&);

		// Configures the content type of the response.
		void setContentType(const std::string&);

		// Sets a single key in the response header to the provided value.
		void setHeader(const std::string& key, const std::string& value);

		// Sets the HTTP return code of a response.
		void setCode(HTTPReturnCode);

		// Builds response into a string.
		std::string build() const;

	private:
		Url resourcePath;
		HTTPReturnCode retCode;
		std::string data;
		std::map<std::string, std::string> headers;
	};

	enum HTTPContentType {
		BYTE_STREAM,
		PLAIN_TEXT,
		HTML,
		JS,
		CSS,
		PNG,
		JPEG,
	};

	// Returns a string that represents an HTTP content type in the header.
	std::string contentTypeString(HTTPContentType); // TODO

	// Determines content type based on the file extension from the Url.
	HTTPContentType getContentType(const Url&); // TODO
}

#endif