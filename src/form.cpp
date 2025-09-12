#include "form.hpp"
#include "error.hpp"
#include "ystl.hpp"
#include <cstddef>
#include <sstream>
#include <utility>

// TODO: all of the errors in here should be sufficiently descriptive (and have proper tags!)
namespace Webserv {
	Result<Form, Error> Form::fromRequest(const HTTPRequest& request) {
		Option<std::string> maybeContentType = request.getHeader("Content-Type");
		if (maybeContentType.isNone()) {
			return Error(Error::FORM_PARSING_ERROR, "Error when parsing request content type");
		}

		const std::string& contentType = maybeContentType.get();
		size_t boundaryIdx = contentType.find("boundary=");
		if (boundaryIdx == contentType.npos) {
			return Error(Error::FORM_PARSING_ERROR, "Error when parsing request content type");
		}
		size_t restIdx = contentType.find_first_of(" \t\r\n\0", boundaryIdx + 11);
		if (restIdx == contentType.npos) {
			return Error(Error::FORM_PARSING_ERROR, "Error when parsing request content type");
		}

		return Form::fromString(
			contentType.substr(boundaryIdx + 10, contentType.size() - restIdx - 1),
			request.getData()
		);
	}

	static Option<std::pair<std::string, std::string> > parseFormStr(const std::string& formStr) {
		size_t doubleNl = formStr.find("\r\n\r\n");
		if (doubleNl == formStr.npos)
			return NONE;
		std::string header = formStr.substr(0, doubleNl);
		std::string data = formStr.substr(doubleNl + 4, formStr.size() - doubleNl - 4);

		std::stringstream headerStream(header);
		std::string line;
		std::string fieldName;
		while (std::getline(headerStream, line)) {
			if (line.empty() || line[0] == '\r') {
				break;
			}
			size_t colon = line.find(":");
			if (colon == line.npos) return NONE;
			std::string key = line.substr(0, colon);
			std::string data = trimString(line.substr(colon, line.size() - colon - 1), ' ');
			if (key == "Content-Disposition") {
				// Parse field name
				size_t namePos = data.find("name=");
				if (namePos == data.npos) return NONE;
				size_t quotePos = namePos + 6;
				while (true) {
					quotePos = data.find("\"", quotePos);
					if (quotePos == data.npos) return NONE;
					if (data[quotePos - 1] != '\'') break;
				}
				fieldName = data.substr(namePos + 6, data.size() - quotePos - 1);
			}
		}
		return std::make_pair(fieldName, data);
	}

	Result<Form, Error> Form::fromString(const std::string& boundary, const std::string& dataStr) {
		Form form;

		std::string properBoundary = "--" + boundary;
		size_t formIdx = dataStr.find(properBoundary) + properBoundary.size();

		while (true) {
			size_t nextIdx = dataStr.find(properBoundary, formIdx);
			if (nextIdx == dataStr.npos) // TODO: This might be a problem, make sure it isn't.
				break;
			std::string formStr = dataStr.substr(formIdx, nextIdx - formIdx);
			Option<std::pair<std::string, std::string> > kvPair = parseFormStr(formStr);
			if (kvPair.isNone()) {
				return Error(Error::FORM_PARSING_ERROR, "Error when parsing form field");
			}
			form.fields[kvPair.get().first] = kvPair.get().second;
			formIdx = nextIdx + properBoundary.size();
		}

		return form;
	}

	Option<std::string> Form::operator[](const std::string& key) {
		if (fields.find(key) != fields.end()) {
			return fields[key];
		}
		return NONE;
	}

	Option<std::string> Form::operator[](const std::string& key) const {
		if (fields.find(key) != fields.end()) {
			return fields.at(key);
		}
		return NONE;
	}

	std::string Form::toCGIString() const {
		std::stringstream result;
		// TODO: what?
		return result.str();
	}
}