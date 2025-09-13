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

typedef Webserv::Error Error;
typedef Webserv::Config::Server::Location Location;
typedef Webserv::HTTPResponse HTTPResponse;

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


std::string Webserv::makeErrorPage(Error e) {
	HTMLTemplate temp;
	temp.bind("ERR_KIND", e.getTagMessage());
	temp.bind("ERR_MSG", e.message);
	return temp.apply(errorTemplate);
}

std::string Webserv::readAll(std::ifstream& ifs) {
	std::ostringstream stream;

	stream << ifs.rdbuf();

	return stream.str();
}

Option<uint> Webserv::hexStrToUInt(const std::string& str) {
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

Option<std::string> Webserv::getFileExtension(const std::string& fileName) {
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

Option<int> Webserv::strToInt(const std::string& str) {
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