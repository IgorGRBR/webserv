#include "locationTree.hpp"
#include "tasks.hpp"
#include "webserv.hpp"
#include "config.hpp"
#include "error.hpp"
#include "html.hpp"
#include "http.hpp"
#include <cctype>
#include <unistd.h>
#include "webserv.hpp"
#include "ystl.hpp"
#include <sstream>

const std::string errorTemplate =
"<!DOCTYPE html>" 
"<html lang=\"en\">"
"<head>"
"	<meta charset=\"UTF-8\">"
"	<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"	<title>Webserv Error</title>"
"	<style>"
"		body {"
"			font-family: Arial, sans-serif;"
"			background-color: #f8f9fa;"
"			color: #333;"
"			display: flex;"
"			justify-content: center;"
"			align-items: center;"
"			height: 100vh;"
"			margin: 0;"
"		}"
"		.error-box {"
"			background: white;"
"			padding: 2rem;"
"			border-radius: 8px;"
"			box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1);"
"			max-width: 500px;"
"			text-align: center;"
"			white-space: pre-wrap;"
"		}"
"		h1 {"
"			color: #d9534f;"
"			margin-bottom: 1rem;"
"		}"
"		p {"
"			margin: 0.5rem 0;"
"		}"
"		.footer {"
"			margin-top: 1.5rem;"
"			font-size: 0.9rem;"
"			color: #888;"
"		}"
"	</style>"
"</head>"
"<body>"
"	<div class=\"error-box\">"
"		<h1>$ERR_KIND</h1>"
"		<p>$ERR_MSG</p>"
"		<div class=\"footer\">Webserv error page</div>"
"	</div>"
"</body>"
"</html>";

namespace Webserv {
	std::string makeErrorPage(Error e) {
		HTMLTemplate temp;
		temp.bind("ERR_KIND", e.getTagMessage());
		temp.bind("ERR_MSG", e.message);
		return temp.apply(errorTemplate);
	}
	
	std::string readAll(std::ifstream& ifs) {
		std::ostringstream stream;
	
		stream << ifs.rdbuf();
	
		return stream.str();
	}
	
	Option<uint> hexStrToUInt(const std::string& str) {
		std::string lower = strToLower(str);
		uint result = 0;
		for (uint i = 0; i < lower.size(); i++) {
			if (!std::isalnum(lower[i]) || lower[i] > 'f') {
				return NONE;
			}
			char delta = std::isalpha(lower[i]) ? ('a' - 10) : '0';
			result *= 16;
			result += lower[i] - delta;
		}
		return result;
	}
	
	Option<std::string> getFileExtension(const std::string& fileName) {
		std::string trimmedFilename = trimString(fileName, '.');
		if (trimmedFilename.find(".") == std::string::npos) {
			return NONE;
		}
	
		std::stringstream stream(trimmedFilename);
		std::string line;
		std::string extension;
		while (getline(stream, line, '.')) {
			extension = line;
		};
		return extension;
	}
	
	Option<int> strToInt(const std::string& str) {
		int result = 0;
		bool negative = false;
		bool readingNumber = false;
		for (uint i = 0; i < str.size(); i++) {
			if (str[i] == '-' && !readingNumber) {
				negative = !negative;
			}
			else if (std::isdigit(str[i])) {
				readingNumber = true;
				result *= 10;
				result += str[i] - '0';
			}
			else {
				return NONE;
			}
		}
		return result * (negative ? -1 : 1);
	}

	static bool readErrorPageFromFile(const std::string& filePath, std::string& content) {
		std::ifstream file(filePath.c_str());
		if (!file.is_open()) {
	#ifdef DEBUG
			std::cerr << "Failed to open error page file: " << filePath << std::endl;
	#endif
			return false;
		}

		std::string line;
		content.clear();
		while (std::getline(file, line)) {
			content += line + "\n";
		}

		file.close();
		return !content.empty();
	}

	Option<Error> sendErrorPage(
		ConnectionInfo conn,
		ServerData sData,
		Option<LocationTreeNode::LocationSearchResult> location,
		FDTaskDispatcher& dispatcher,
		Error error
	) {
		HTTPResponse resp = HTTPResponse(Url());
		resp.setCode(error.getHTTPCode());

		std::string errorPageContent;
		bool customPageFound = false;
		std::string errorPagePath;

		// Check location-specific error pages first
		if (location.isSome()) {
			const std::map<ushort, std::string>& locErrPages = location.get().location->errPages;
			std::map<ushort, std::string>::const_iterator it = locErrPages.find(error.getHTTPCode());
			if (it != locErrPages.end()) {
				errorPagePath = it->second;
			}
		}

		// If no location error page found, check server-level error pages
		if (errorPagePath.empty()) {
			std::map<ushort, std::string>::const_iterator sit = sData.config.errPages.find(error.getHTTPCode());
			if (sit != sData.config.errPages.end()) {
				errorPagePath = sit->second;
			}
		}

		// Attempt to read custom error page file
		if (!errorPagePath.empty()) {
			if (readErrorPageFromFile(errorPagePath, errorPageContent)) {
				customPageFound = true;
			}
#ifdef DEBUG
			else {
				std::cerr << "Failed to read custom error page file: " << errorPagePath << std::endl;
			}
#endif
		}

		if (customPageFound) {
			resp.setData(errorPageContent);
			resp.setContentType(contentTypeString(getContentType(Url::fromString(errorPagePath).get())));
		} else {
			resp.setData(makeErrorPage(error));
			resp.setContentType(contentTypeString(HTML));
		}

		Result<ResponseHandler*, Error> maybeRespTask =
			ResponseHandler::tryMake(conn, resp);
		if (maybeRespTask.isError()) {
			return maybeRespTask.getError();
		}
		dispatcher.registerTask(maybeRespTask.getValue());
		return NONE;
	}
}
