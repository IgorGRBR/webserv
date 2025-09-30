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
		HEAD,
	};

	// Returns a name of the HTTP method as a c-string.
	const char* httpMethodName(HTTPMethod);

	// Attempts to parse HTTP method from a string.
	Option<HTTPMethod> httpMethodFromStr(const std::string &);

	// Represents an error that might have occured when parsing the HTTP request.
	enum HTTPRequestError {
		INVALID_REQUEST,
		MISSING_STARTING_LINE,
		MISSING_HTTP_METHOD,
		INVALID_HTTP_METHOD,
		MISSING_HTTP_PATH,
		INVALID_HTTP_HEADER,
		NO_DATA_SEGMENT,
	};

	// Returns a short description of the HTTP request error.
	const char* httpRequestErrorMessage(HTTPRequestError);

	struct Error;

	// `HTTPRequest` is a container that contains parsed HTTP request data.
	class HTTPRequest {
	public:
		class Builder {
		public:
			enum State {
				INITIAL,
				HEADER_COMPLETE,
				CHUNKED_READ_COMPLETE,
			};
			Builder();
			Result<State, Error> appendData(const std::string&);
			Result<UniquePtr<HTTPRequest>, Error> build();
			uint getDataSize() const;
			Option<Url> getHeaderPath() const;
			Option<uint> getContentLength() const;
			Option<HTTPMethod> getHTTPMethod() const;
			Option<std::string> getHost() const;
			Option<std::string> getHeader(const std::string&) const;
			bool isChunked() const;
			bool chunkedReadFinished() const;
		private:
			Result<State, Error> readChunk(const std::string&);

			uint dataSize;
			std::vector<std::string> lines;
			State internalState;
			UniquePtr<HTTPRequest> request;
			bool chunked;
			std::string chunkReadLeftover;
			uint chunkSizeLeftover;
			bool chunkedReadComplete;
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

		bool isForm() const;

		std::string toString() const;

		HTTPRequest withData(const std::string&) const;

		Option<HTTPRequest> unchunked() const;
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
	enum HTTPReturnCode {
		HTTP_NONE = 0,
		// 1xx Informational response
		HTTP_CONTINUE = 100,
		HTTP_SWITCHING_PROTOCOLS = 101,
		HTTP_PROCESSING = 102,
		HTTP_EARLY_HINTS = 103,

		//2xx success
		HTTP_OK = 200,
		HTTP_CREATED = 201,
		HTTP_ACCEPTED = 202,
		HTTP_NON_AUTHORITATIVE_INFORMATION = 203,
		HTTP_NO_CONTENT = 204,
		HTTP_RESET_CONTENT = 205,
		HTTP_PARTIAL_CONTENT = 206,
		HTTP_MULTI_STATUS = 207,
		HTTP_ALREADY_REPORTED = 208,
		HTTP_IM_USED = 226,

		// 3xx Redirection
		HTTP_MULTIPLE_CHOICES = 300,
		HTTP_MOVED_PERMANENTLY = 301,
		HTTP_FOUND = 302,
		HTTP_SEE_OTHER = 303,
		HTTP_NOT_MODIFIED = 304,
		HTTP_USE_PROXY = 305,
		HTTP_TEMPORARY_REDIRECT = 307,
		HTTP_PERMANENT_REDIRECT = 308,

		//4xx client errors
		HTTP_BAD_REQUEST = 400,
		HTTP_UNATHORIZED = 401,
		HTTP_PAYMENT_REQUIRED = 402,
		HTTP_FORBIDDEN = 403,
		HTTP_NOT_FOUND = 404,
		HTTP_METHOD_NOT_ALLOWED = 405,
		HTTP_NOT_ACCEPTABLE = 406,
		HTTP_PROXY_AUTHENTICATION_REQUIRED = 407,
		HTTP_REQUEST_TIMEOUT = 408,
		HTTP_CONFLICT = 409,
		HTTP_GONE = 410,
		HTTP_LENGTH_REQUIRED = 411,
		HTTP_PRECONDITION_FAILED = 412,
		HTTP_PAYLOAD_TOO_LARGE = 413,
		HTTP_URI_TOO_LONG = 414,
		HTTP_UNSUPPORTED_MEDIA_TYPE = 415,
		HTTP_RANGE_NOT_SATISFIABLE = 416,
		HTTP_EXPECTATION_FAILED = 417,
		HTTP_IM_A_TEAPOT = 418, //Not sure about this. Seems more of an easter egg than an actual status code.
		HTTP_MISDIRECTED_REQUEST = 421,
		HTTP_UNPROCESSABLE_CONTENT = 422,
		HTTP_LOCKED = 423,
		HTTP_FAILED_DEPENDENCY = 424,
		HTTP_TOO_EARLY = 425,
		HTTP_UPGRADE_REQUIRED = 426,
		HTTP_PRECONDITION_REQUIRED = 428,
		HTTP_TOO_MANY_REQUESTS = 429,
		HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
		HTTP_UNAVAILABLE_FOR_LEGAL_REASONS = 451,

		// 5xx server errors
		HTTP_INTERNAL_SERVER_ERROR = 500,
		HTTP_NOT_IMPLEMENTED = 501,
		HTTP_BAD_GATEWAY = 502,
		HTTP_SERVICE_UNAVAILABLE = 503,
		HTTP_GATEWAY_TIMEOUT = 504,
		HTTP_VERSION_NOT_SUPPORTED = 505,
		HTTP_VARIANT_ALSO_NEGOTIATES = 506,
		HTTP_INSUFFICIENT_STORAGE = 507,
		HTTP_LOOP_DETECTED = 508,
		HTTP_NOT_EXTENDED = 510,
		HTTP_NETWORK_AUTHENTICATION_REQUIRED = 511
	};

	// Returns a short description of the HTTP return code.
	const char* httpReturnCodeMessage(HTTPReturnCode);

	// `HTTPResponse` is a builder that builds a HTTP response with the provided information.
	class HTTPResponse {
	public:

		// Constructs an `HTTPResponse` with the provided url and return code.
		// The default content type of a response is `text/html`.
		HTTPResponse(Url, HTTPReturnCode = HTTP_NONE);

		static Option<HTTPResponse> fromString(const std::string&);

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
	std::string contentTypeString(HTTPContentType);

	// Determines content type based on the file extension from the Url.
	HTTPContentType getContentType(const Url&);
}

#endif
