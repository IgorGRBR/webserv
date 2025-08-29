#include "config.hpp"
#include "dispatcher.hpp"
#include "error.hpp"
#include "http.hpp"
#include "tasks.hpp"
#include "webserv.hpp"

typedef Result<Webserv::IFDTask*, Webserv::Error> TaskResult;
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

	TaskResult handleLocation(
		const Url& path,
		const Config::Server::Location& location,
		HTTPRequest& request,
		ServerData& sData,
		int clientSocketFd
	) {
		std::string root;

		// TODO: maybe move this check into configuration parsing step?
		if (location.root.isSome()) {
			root = location.root.get();
		}
		else if (sData.config.defaultRoot.isSome()) {
			root = sData.config.defaultRoot.get();
		}
		else {
			return Error(Error::HTTP_ERROR, "Missing root location in configuration");
		}

		ConnectionInfo conn;
		conn.connectionFd = clientSocketFd;

		Option<std::string> fileContent = NONE;
		Url rootUrl = Url::fromString(root).get();
		Url tail = request.getPath().tailDiff(path);

		if (tail.getSegments().empty()
		&& request.getMethod() == POST
		&& location.fileUploadFieldId.isSome()) {
			// TODO: handle file uploading here
			// 1) Get the uploaded file from the request (Note: seems to work for .txt and .html but not for .jpeg?)
			std::string requestData = request.getData();
			std::cout << "(DEBUG) Received file data:\n" << requestData << std::endl; //This is an example of the received file data: ------WebKitFormBoundaryG8fEUJTawKXLd6iV
			//Content-Disposition: form-data; name="file"; filename="cool.txt"
			//Content-Type: text/plain

			//this is a cool file
			//------WebKitFormBoundaryG8fEUJTawKXLd6iV--
			// 2) Validate the uploaded file
			Option<std::string> requestHeader = request.getHeader("Content-Type");
			if (requestHeader.isNone()) {
				return Error(Error::HTTP_ERROR, "Missing Content-Type header"); //Maybe it needs a more specific error?
			}
			std::cout << "(DEBUG) Received Content-Type header:\n" << requestHeader.get() << std::endl; //This is an example of the content-type header: multipart/form-data; boundary=----WebKitFormBoundaryG8fEUJTawKXLd6iV
			// 3) Parse the request body
			// 4) Store the uploaded file in exampleSite/upload
			// return Error(Error::GENERIC_ERROR, "Not implemented (REMOVE ME)");
		}

		Url respFileUrl = rootUrl + tail;
		std::string respFilePath = respFileUrl.toString(false);
		std::cout << "Trying to load: " << respFilePath << std::endl;

		// Here we should check if the path is a directory or a file, and *then* send back the response.
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
			Result<ResponseHandler*, Error> response = ResponseHandler::tryMake(sData, conn);
			if (response.isError()) {
				std::cout << "ERROR: Failed to create ResponseHandler!" << std::endl;
				return response.getError();
			}
			response.getValue()->setResponseData(fileContent.get());
			response.getValue()->setResponseContentType(contentType);
			return response.getValue();
		}
		else {
			return Error(Error::FILE_NOT_FOUND, respFilePath);
		}

		return Error(Error::GENERIC_ERROR, "Not implemented");
	}

	TaskResult analyzeRequest(HTTPRequest& request, ServerData& sData, int clientSocketFd) {
		// Analyze request
		Option<LocationTreeNode::LocationSearchResult> maybeLocation = sData.locations.tryFindLocation(request.getPath());
		if (maybeLocation.isSome()) {
			LocationTreeNode::LocationSearchResult result = maybeLocation.get();
			const Location* location = result.location;
			if (!checkIfMethodIsInByte(request.getMethod(), location->allowedMethods)) {
					return Error(Error::GENERIC_ERROR, "HTTP method is not allowed");
			};
			return handleLocation(result.locationPath, *location, request, sData, clientSocketFd);
		}
		return Error(Error::RESOURCE_NOT_FOUND, "Could not find a resource on the specified path.");
	};

	TaskResult handleRequest(HTTPRequest& request, LocationTreeNode::LocationSearchResult& query, ServerData& sData, int clientSocketFd) {
		const Location* location = query.location;
		if (!checkIfMethodIsInByte(request.getMethod(), location->allowedMethods)) {
				return Error(Error::GENERIC_ERROR, "HTTP method is not allowed");
		};
		return handleLocation(query.locationPath, *location, request, sData, clientSocketFd);
	};
}

