#include "config.hpp"
#include "dispatcher.hpp"
#include "error.hpp"
#include "http.hpp"
#include "tasks.hpp"
#include "url.hpp"
#include "webserv.hpp"
#include "ystl.hpp"
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iostream> //added for file uploading

typedef Result<SharedPtr<Webserv::IFDTask>, Webserv::Error> TaskResult;
typedef Webserv::Config::Server::Location Location;

namespace Webserv {
	bool checkIfMethodIsInByte(HTTPMethod method, char configByte) {
		switch (method) {
		case Webserv::GET:
			return HTTP_GET_FLAG & configByte;
		case Webserv::POST:
			return HTTP_POST_FLAG & configByte;
		case Webserv::PUT:
			return HTTP_PUT_FLAG & configByte;
		case Webserv::DELETE:
			return HTTP_DELETE_FLAG & configByte;
		}
		return false;
	}

	TaskResult handleCGI(
		int clientSocketFd,
		const Url& root,
		const Url& rest,
		const Config::Server::Location& location,
		HTTPRequest& request,
		ServerData& sData
	) {
		(void)location;
		// First - determine what kind of interpreter to run for the CGI
		if (rest.getSegments().empty()) {
			return Error(HTTP_FORBIDDEN, "No script was selected");
		}

		Url scriptLocation = root + rest.head();
		std::string extension = scriptLocation.getExtension().getOr("");
		std::string interpPath;
		if (extension.empty()) {
			interpPath = "";
		}
		else {
			if (sData.cgiInterpreters.find(extension) == sData.cgiInterpreters.end()) {
				return Error(Error::GENERIC_ERROR, "Could not find appropriate interpreter for the provided file extension");
			}
			interpPath = sData.cgiInterpreters[extension];
		}

		Option<Url> maybeInterpLocation = Url::fromString(interpPath);
		if (maybeInterpLocation.isNone()) {
			return Error(Error::FILE_NOT_FOUND, "Interpreter was not found");
		}

		ConnectionInfo conn;
		conn.connectionFd = clientSocketFd;
		// Now construct the pipeline.
		Result<CGIPipeline, Error> maybePipeline = makeCGIPipeline(
			conn,
			maybeInterpLocation.get(),
			scriptLocation,
			rest.tail(),
			request,
			sData.envp,
			sData.messageBufferSize
		);
		if (maybePipeline.isError())
			return maybePipeline.getError();

		std::string cgiStdinData;
		cgiStdinData = request.toString();

		CGIPipeline& pipeline = maybePipeline.getValue();
		pipeline.first->consumeFileData(cgiStdinData);

		return pipeline.second.tryAs<IFDTask>().get();
	}

	static Option<Error> handleFileUploadWithPUSH(
		HTTPRequest& request,
		const Url& uploadPath
	) {
		// 1) Get the uploaded file from the request
		std::string requestData = request.getData();
		// std::cout << "(DEBUG) Received file data:\n" << requestData << std::endl;

		// 2) Validate the uploaded file
		Option<std::string> requestHeader = request.getHeader("Content-Type");
		if (requestHeader.isNone()) {
			return Error(HTTP_BAD_REQUEST, "Missing Content-Type header");
		}
		// std::cout << "(DEBUG) Received Content-Type header:\n" << requestHeader.get() << std::endl;

		// 3) Parse the request body; retrieve the content type from the header
		std::string contentTypeHeader = requestHeader.get();
		std::string boundaryPrefix = "boundary=";
		std::size_t boundaryPos = contentTypeHeader.find(boundaryPrefix);
		if (boundaryPos == std::string::npos) {
			return Error(HTTP_BAD_REQUEST, "Missing boundary in Content-Type header");
		}

		std::string boundary = contentTypeHeader.substr(boundaryPos + boundaryPrefix.length(), contentTypeHeader.npos);
		std::string boundLine = "--" + boundary;
		std::size_t partStart = requestData.find(boundLine);
		if (partStart == std::string::npos) {
			return Error(HTTP_BAD_REQUEST, "Boundary not found in request data");
		}
		partStart += boundLine.length();
		if (requestData.substr(partStart, 2) == "\r\n") {
			partStart += 2;
		}
		std::string contentType = contentTypeHeader.substr(0, boundaryPos);

		std::size_t headersEndPos = requestData.find("\r\n\r\n", partStart);
		if (headersEndPos == std::string::npos){
			return Error(HTTP_BAD_REQUEST, "Malformed headers in request data");
		}
		std::string headers = requestData.substr(partStart, headersEndPos - partStart);
		// std::cout << "(DEBUG) Headers: " << headers << std::endl;

		std::string filename;
		std::size_t contentDispPos = headers.find("Content-Disposition:");
		if (contentDispPos != std::string::npos) {
			std::size_t filenamePos = headers.find("filename=\"", contentDispPos);
			if (filenamePos != std::string::npos) {
				filenamePos += 10;
				std::size_t filenameEndPos = headers.find("\"", filenamePos);
				if (filenameEndPos != std::string::npos){
					filename = headers.substr(filenamePos, filenameEndPos - filenamePos);
				}
				else {
					return Error(HTTP_BAD_REQUEST, "Malformed filename in Content-Disposition.");
				}
			}
			else {
				return Error(HTTP_BAD_REQUEST, "Filename not found in Content-Disposition.");
			}
		}
		else {
			return Error(HTTP_BAD_REQUEST, "Filename not found.");
		}

		std::size_t fileStart = headersEndPos + 4;
		std::string closingBoundary = boundLine + "--";
		std::size_t fileEnd = requestData.find(closingBoundary, fileStart);
		std::cout << closingBoundary << std::endl;
		if (fileEnd == std::string::npos) {
			std::string altClosingBoundary = "\r\n" + closingBoundary;
			fileEnd = requestData.find(altClosingBoundary, fileStart);
			if (fileEnd != std::string::npos) {
				closingBoundary = altClosingBoundary;
			}
			// std::cout << "(DEBUG) Found closing boundary: " << fileEnd << std::endl;
		}
		std::string fileContent = requestData.substr(fileStart, fileEnd - fileStart);
		fileContent = fileContent.substr(0, fileContent.length() - closingBoundary.length() - 2);

		// std::cout << "(DEBUG) Retrieved content type:\n" << contentType << std::endl;
		if (contentType == "multipart/form-data; ") {
			// 4) Store the uploaded file in exampleSite/upload
			// 4.1) Create the file in the upload directory
			std::string filePath = uploadPath.toString(false, true) + "/" + filename;
			std::ofstream uploadFile(filePath.c_str());
			if (!uploadFile.is_open()) {
				return Error(HTTP_INTERNAL_SERVER_ERROR, "Failed to create an upload file");
			}

			// 4.2) Write the file content
			uploadFile << fileContent;
		}
		return NONE;
	}

	static TaskResult handleFileUploadWithPUT(
		ConnectionInfo conn,
		HTTPRequest& request,
		const Url& uploadPath
	) {
		std::string filePath = uploadPath.toString(false, true);
		std::ofstream uploadFile(filePath.c_str());
		if (!uploadFile.is_open()) {
			return Error(HTTP_INTERNAL_SERVER_ERROR, "Failed to create/open an upload file");
		}

		uploadFile << request.getData();

		HTTPResponse response = HTTPResponse(Url(), HTTP_CREATED);

		Result<ResponseHandler*, Error> handler = ResponseHandler::tryMake(conn, response);

		if (handler.isError()) {
			return handler.getError();
		}

		return SharedPtr<IFDTask>(handler.getValue());
	}

	static TaskResult handleFileRemoval(
		ConnectionInfo conn,
		const Url& filePath
	) {
		// This might not be compliant with the subject document, but IDGAF at this point.
		int status = std::remove(filePath.toString(false, true).c_str());

		HTTPResponse response = HTTPResponse(Url(), status < 0? HTTP_INTERNAL_SERVER_ERROR : HTTP_OK);

		Result<ResponseHandler*, Error> handler = ResponseHandler::tryMake(conn, response);

		if (handler.isError()) {
			return handler.getError();
		}

		return SharedPtr<IFDTask>(handler.getValue());
	}

	TaskResult handleLocation(
	const Url& path,
	const Config::Server::Location& location,
	HTTPRequest& request,
	ServerData& sData,
	int clientSocketFd
	) {
		ConnectionInfo conn;
		conn.connectionFd = clientSocketFd;

		// Early return if redirection is configured
		if (location.redirection.isSome()) {
			std::string redirectUrl = location.redirection.get();

			HTTPResponse resp = HTTPResponse(Url());
			resp.setCode(Webserv::HTTP_MOVED_PERMANENTLY);
			resp.setHeader("Location", redirectUrl);
			resp.setHeader("Cache-Control", "no cache");
			
			Result<ResponseHandler*, Error> response = ResponseHandler::tryMake(conn, resp);
			if (response.isError()) {
				std::cout << "ERROR: Failed to create ResponseHandler!" << std::endl;
				return response.getError();
			}

			return SharedPtr<IFDTask>(response.getValue());
		}

		std::string root;

		// Existing root location checks
		if (location.root.isSome()) {
			root = location.root.get();
		}
		else if (sData.config.defaultRoot.isSome()) {
			root = sData.config.defaultRoot.get();
		}
		else {
			return Error(Error::CONFIG_ERROR, "Missing root location in configuration");
		}

		Option<std::string> fileContent = NONE;
		Url rootUrl = Url::fromString(root).get();
		Url tail = request.getPath().tailDiff(path);

		if (location.allowCGI && (request.getMethod() == POST || request.getMethod() == GET)) {
			return handleCGI(clientSocketFd, rootUrl, tail, location, request, sData);
		}

		if (tail.getSegments().empty()
		&& request.getMethod() == POST
		&& location.fileUploadFieldId.isSome()) {
			Option<Error> maybeError = handleFileUploadWithPUSH(request, rootUrl);
			if (maybeError.isSome()) {
				return maybeError.get();
			}
		}

		Url respFileUrl = rootUrl + tail;
		if (request.getMethod() == PUT) {
			return handleFileUploadWithPUT(conn, request, respFileUrl);
		}

		if (request.getMethod() == DELETE) {
			return handleFileRemoval(conn, respFileUrl);
		}

		std::string respFilePath = respFileUrl.toString(false);
		std::cout << "Trying to load: " << respFilePath << std::endl;

		// Check file system type and load content
		FSType fsType = checkFSType(respFilePath);
		HTTPContentType contentType = BYTE_STREAM;
		switch (fsType) {
		case FS_NONE:
			fileContent = NONE;
			break;
		case FS_FILE: {
				std::ifstream respFile(respFilePath.c_str());
				if (respFile.is_open()) {
					fileContent = readAll(respFile);
					contentType = getContentType(respFileUrl);
				}
				else {
					return Error(Error::GENERIC_ERROR, "Failed to open a file");
				}
			}
			break;
		case FS_DIRECTORY:
			std::string indexStr = location.index.getOr("index.html");
			Url index = Url::fromString(indexStr).get();

			std::string indexFilePath = (respFileUrl + index).toString(false);
			std::cout << "Trying to load: " << indexFilePath << std::endl;
			std::ifstream respFile(indexFilePath.c_str());

			if (respFile.is_open()) { // Try to load index file
				fileContent = readAll(respFile);
				Url indexFileUrl = respFileUrl + index;
				contentType = getContentType(indexFileUrl);
			}
			else { // Try to create directory listing
				std::string urlPath = request.getPath().toString(true);
				fileContent = makeDirectoryListing(
					respFilePath,
					urlPath,
					tail.getSegments().size() == 0,
					location.fileUploadFieldId.isSome()
				);
				contentType = HTML;
			}
			break;
		}

		if (fileContent.isSome()) {
			HTTPResponse resp = HTTPResponse(Url(), HTTP_OK);
			resp.setData(fileContent.get());
			resp.setContentType(contentTypeString(contentType));
			
			Result<ResponseHandler*, Error> response = ResponseHandler::tryMake(conn, resp);
			if (response.isError()) {
#ifdef DEBUG
				std::cout << "ERROR: Failed to create ResponseHandler!" << std::endl;
#endif
				return response.getError();
			}

			return SharedPtr<IFDTask>(response.getValue());
		}
		else {
			return Error(Error::FILE_NOT_FOUND, respFilePath);
		}

		return Error(HTTP_NOT_IMPLEMENTED, "Not implemented");
	}

	TaskResult handleRequest(HTTPRequest& request, LocationTreeNode::LocationSearchResult& query, ServerData& sData, int clientSocketFd) {
		const Location* location = query.location;
		if (!checkIfMethodIsInByte(request.getMethod(), location->allowedMethods)) {
			return Error(HTTP_METHOD_NOT_ALLOWED, "HTTP method is not allowed");
		};
		return handleLocation(query.locationPath, *location, request, sData, clientSocketFd);
	};
}

