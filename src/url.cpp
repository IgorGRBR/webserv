#include "url.hpp"
#include "webserv.hpp"
#include "ystl.hpp"
#include <cstdio>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <vector>

namespace Webserv {
	Url::Url(): segments(), protocol(), port(0), absolute(false) {}
	
	Option<Url> Url::fromString(const std::string& str) {
		Url url;
		std::string segment;
		std::stringstream urlStream(str);
	
		if (getline(urlStream, segment, '/') && str[0] != '/') {
			// Try parse protocol
			if (segment[segment.length() - 1] == ':')
				url.protocol = segment.substr(0, segment.length() - 1);
			
			if (url.protocol.length() > 0) {
				getline(urlStream, segment, '/');
				if (segment.length() > 0) return NONE;
				getline(urlStream, segment, '/');
			}
			if (segment.find(':') != segment.npos) {
				std::string portStr;
				std::stringstream portStream(segment);
	
				getline(portStream, portStr, ':');
				url.segments.push_back(portStr);
				portStream >> url.port;
			}
			else {
				if (segment.length() > 0) url.segments.push_back(segment);
			}
		}
		else if (str[0] == '/')
			url.absolute = true;
	
		std::string queryString;
		while (getline(urlStream, segment, '/')) {
			size_t questionMark = segment.find("?");
			if (questionMark != segment.npos) {
				std::string finalSegment = segment.substr(0, questionMark);
				queryString = segment.substr(questionMark + 1, segment.size() - questionMark - 1);
				break;
			}
			if (segment.length() > 0) url.segments.push_back(segment);
		}

		// Parse query
		if (!queryString.empty()) {
			std::stringstream queryStream(queryString);
			std::string queryElem;
			while (getline(queryStream, queryElem, '&')) {
				size_t equalSign = queryElem.find("=");
				if (equalSign == queryElem.npos) {
					return NONE;
				}
				std::string key = queryElem.substr(0, equalSign);
				std::string value = queryElem.substr(equalSign + 1, queryElem.size() - equalSign - 1);
				url.query[key] = value;
			}
		}
	
		return url;
	}
	
	const std::vector<std::string>& Url::getSegments() const {
		return segments;
	}
	
	const std::string& Url::getProtocol() const {
		return protocol;
	}
	
	ushort Url::getPort() const {
		return port;
	}
	
	std::string Url::toString(bool headSlash, bool localFS) const {
		headSlash = headSlash || absolute;
		std::stringstream result;
		if (!localFS && protocol.length() > 0) {
			result << protocol << ":/";
		}
		if (headSlash or protocol.length()) result << "/";
		for (std::vector<std::string>::const_iterator it = segments.begin(); it != segments.end(); it++) {
			if (it == segments.begin()) {
				result << *it;
				if (port > 0) result << ":" << port;
			}
			else {
				result << "/" << *it;
			}
		}
		if (!localFS && !query.empty()) {
			result << "?";
			bool first = true;
			for (std::map<std::string, std::string>::const_iterator it = query.begin(); it != query.end(); it++) {
				if (!first) {
					result << "&";
				}
				first = false;
				result << it->first << "=" << it->second;
			}
		}
		return result.str();
	}
	
	bool Url::operator==(const Url& other) const {
		return absolute == other.absolute
			&& port == other.port
			&& protocol == other.protocol
			&& segments == other.segments
			&& query == other.query;
	}
	
	bool Url::operator!=(const Url& other) const {
		return !(*this == other);
	}
	
	Url Url::operator+(const Url& other) const {
		Url newUrl = *this;
		for (std::vector<std::string>::const_iterator it = other.segments.begin(); it != other.segments.end(); it++) {
			newUrl.segments.push_back(*it);
		}
		return newUrl;
	}
	
	bool Url::matchSegments(const Url& other) const {
		if (segments.size() < other.segments.size()) return false;
		for (uint i = 0; i < other.segments.size(); i++) {
			if (segments[i] != other.segments[i])
				return false;
		}
		return true;
	}
	
	Url Url::tailDiff(const Url& other) const {
		if (!matchSegments(other)) return Url();
		if (segments.size() > other.segments.size()) {
			Url result;
			for (uint i = other.segments.size(); i < segments.size(); i++) {
				result.segments.push_back(segments[i]);
			}
			return result;
		}
		else if (other.segments.size() > segments.size()) {
			Url result;
			for (uint i = segments.size(); i < other.segments.size(); i++) {
				result.segments.push_back(other.segments[i]);
			}
			return result;
		}
		return Url();
	}
	
	std::vector<std::string> Url::tailDiffVec(const Url& other) const {
		std::vector<std::string> result;
		if (!matchSegments(other)) return result;
		if (segments.size() > other.segments.size()) {
			for (uint i = other.segments.size(); i < segments.size(); i++) {
				result.push_back(segments[i]);
			} 
		}
		else if (other.segments.size() > segments.size()) {
			for (uint i = segments.size(); i < other.segments.size(); i++) {
				result.push_back(other.segments[i]);
			}
		}
		return result;
	}
	
	Option<std::string> Url::getExtension() const {
		if (segments.size() == 0) return NONE;
		return getFileExtension(segments.back());
	}

	Url Url::head() const {
		Url newUrl = *this;
		if (newUrl.segments.size() > 0) {
			newUrl.segments = std::vector<std::string>();
			newUrl.segments.push_back(segments[0]);
		}
		return newUrl;
	}

	Url Url::exceptLast() const {
		Url newUrl = *this;
		if (newUrl.segments.size() > 0) {
			newUrl.segments.pop_back();
		}
		return newUrl;
	}
	
	Url Url::tail() const {
		Url newUrl = *this;
		if (newUrl.segments.size() > 0) {
			newUrl.segments.erase(newUrl.segments.begin());
		}
		return newUrl;
	}

	Option<std::string> Url::getQuery(const std::string& key) const {
		if (query.find(key) == query.end())
			return NONE;
		return query.at(key);
	}
}
