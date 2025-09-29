#ifndef LOCATION_TREE_HPP
#define LOCATION_TREE_HPP

#include "config.hpp"
#include "url.hpp"
#include <map>
#include <string>
namespace Webserv {

	// `LocationTreeNode` is a node of a tree data structure, that is used to store and lookup
	// location config references.
	class LocationTreeNode {
	public:
		// Constructs an empty node.
		LocationTreeNode();

		// This enum represents an error that might occur during location reference insertion or lookup.
		enum Error {
			INSERTION_ERROR,
		};

		// A simple POD struct that represents a successful search result of the tree.
		// The result contains a pointer to the location config itself, it's path in the tree,
		// and the tail of the query that got left out from the tree.
		struct LocationSearchResult {
			const Config::Server::Location* location;
			Url locationPath;
			Url tail;
		};

		// Attempts to find a location config at the specified url.
		Option<LocationSearchResult> tryFindLocation(Url locationUrl) const;

		// Inserts a location pointer into the tree by the provided `url` as a path.
		// Automatically creates sub-nodes that are necessary to represent the provided `url` in the tree.
		Option<Error> insertLocation(Url url, const Config::Server::Location* insertedLocation);
	private:
		Option<const Config::Server::Location*> location;
		std::map<std::string, LocationTreeNode> nodes;

		Option<LocationSearchResult> innerLocationAsResult(Url) const;
	};
}

#endif