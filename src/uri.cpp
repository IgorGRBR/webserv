#include "url.hpp"
#include <cstdio>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <vector>

typedef Webserv::Url Url;

Url::Url(): segments(), protocol(), port(0) {}

Option<Url> Url::fromString(const std::string& str) {
	Url uri;
	std::string segment;
	std::stringstream urlStream(str);

	if (getline(urlStream, segment, '/') && str[0] != '/') {
		// Try parse protocol
		if (segment[segment.length() - 1] == ':')
			uri.protocol = segment.substr(0, segment.length() - 1);
		
		if (uri.protocol.length() > 0) {
			getline(urlStream, segment, '/');
			if (segment.length() > 0) return NONE;
			getline(urlStream, segment, '/');
		}
		if (segment.find(':') != segment.npos) {
			std::string portStr;
			std::stringstream portStream(segment);

			getline(portStream, portStr, ':');
			uri.segments.push_back(portStr);
			portStream >> uri.port;
		}
		else {
			if (segment.length() > 0) uri.segments.push_back(segment);
		}
	}

	while (getline(urlStream, segment, '/'))
		if (segment.length() > 0) uri.segments.push_back(segment);

	return uri;
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

std::string Url::toString(bool headShash) const {
	std::stringstream result;
	if (protocol.length() > 0) {
		result << protocol << ":/";
	}
	if (headShash or protocol.length()) result << "/";
	for (std::vector<std::string>::const_iterator it = segments.begin(); it != segments.end(); it++) {
		if (it == segments.begin()) {
			result << *it;
			if (port > 0) result << ":" << port;
		}
		else {
			result << "/" << *it;
		}
	}
	return result.str();
}

bool Url::operator==(const Url& other) const {
	return port == other.port && protocol == other.protocol && segments == other.segments;
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
	std::stringstream finalSegment(segments.back());
	std::string line;
	if (!getline(finalSegment, line, '.')) {
		return NONE;
	}
	if (!getline(finalSegment, line, '.')) {
		return NONE;
	}
	return line;
}
