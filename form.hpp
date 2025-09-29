#ifndef FORM_HPP
#define FORM_HPP

#include "error.hpp"
#include "http.hpp"
#include <map>
#include <string>

namespace Webserv {
	class Form {
	public:
		static Result<Form, Error> fromRequest(const HTTPRequest& request);
		static Result<Form, Error> fromString(const std::string& boundary, const std::string& formStr);

		Option<std::string> operator[](const std::string&);
		Option<std::string> operator[](const std::string&) const;
	private:
		std::map<std::string, std::string> fields;
	};
}

#endif