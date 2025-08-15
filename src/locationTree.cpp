#include "locationTree.hpp"
#include "config.hpp"
#include <string>
#include <vector>

typedef Webserv::LocationTreeNode LocationNode;
typedef Webserv::Config::Server::Location Location;
typedef Webserv::Url Url;

LocationNode::LocationTreeNode() {}


Option<LocationNode::LocationSearchResult> LocationNode::innerLocationAsResult(Url locationUrl) const {
	if (location.isSome()) {
		LocationSearchResult result;
		result.location = location.get();
		result.tail = locationUrl;
		return result;
	}
	return NONE;
}

Option<LocationNode::LocationSearchResult> LocationNode::tryFindLocation(Url locationUrl) const {
	const std::vector<std::string>& segments = locationUrl.getSegments();
	if (segments.size() == 0) {
		return innerLocationAsResult(locationUrl);
	}
	const std::string& head = segments[0];
	if (nodes.find(head) == nodes.end()) {
		return innerLocationAsResult(locationUrl);
	}
	Option<LocationSearchResult> foundLocation = nodes.at(head).tryFindLocation(locationUrl.tail());
	if (foundLocation.isNone()) {
		return innerLocationAsResult(locationUrl);
	}
	foundLocation.get().locationPath = Url::fromString(segments[0]).get() + foundLocation.get().locationPath;
	return foundLocation;
}

Option<LocationNode::Error> LocationNode::insertLocation(Url url, const Location* insertedLocation) {
	const std::vector<std::string>& segments = url.getSegments();
	if (segments.size() == 0) {
		if (location.isNone()) {
			location = insertedLocation;
			return NONE;
		}
		else
			return INSERTION_ERROR;
	}

	const std::string& head = segments[0];
	if (nodes.find(head) == nodes.end()) {
		nodes[head] = LocationNode();
	}
	return nodes[head].insertLocation(url.tail(), insertedLocation);
}