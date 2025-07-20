#include "webserv.hpp"
#include "config.hpp"
#include "error.hpp"
#include "html.hpp"
#include "http.hpp"
#include <cstring>
#include <unistd.h>
#include "webserv.hpp"
#include <sstream>

typedef Webserv::Error Error;
typedef Webserv::Config::Server::Location Location;
typedef Webserv::HTTPResponse HTTPResponse;

const std::string errorTemplate = "<head>Webserv error page!</head>"
"<body>"
"	<div>"
"		<h1>Internal server error</h1>"
"		An error has occured.<br>"
"		Kind: $ERR_KIND<br>"
"		$ERR_MSG<br>"
"	</div>"
"</body>";

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
