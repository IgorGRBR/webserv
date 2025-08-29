#ifndef URL_HPP
#define URL_HPP

#include "ystl.hpp"
#include <map>
#include <string>
#include <sys/types.h>
#include <vector>
namespace Webserv {

	// `Url` class represents a programmer-friendly representation of a url.
	class Url {
	public:

		// Constructs an empty Url.
		Url();

		// Attempts to construct a url from given string.
		static Option<Url> fromString(const std::string&);

		// Retrieves the segments of a url.
		const std::vector<std::string>& getSegments() const;

		// Retrieves the protocol specified at the beginning of the url. If the string is empty,
		// the protocol was not specified.
		const std::string& getProtocol() const;

		// Returns the port of the resource, specified in the first segment of url after ':'.
		ushort getPort() const;

		// Returns an extension of the resource, specified after the last `.` in the last segment of the url.
		Option<std::string> getExtension() const;

		// Converts the `Url` object to string.
		std::string toString(bool headShash = true) const;

		// Checks if two `Url` objects are equal.
		bool operator==(const Url&) const;

		// Checks if two `Url` objects are not equal.
		bool operator!=(const Url&) const;

		// Concatenates two `Url` objects.
		Url operator+(const Url&) const;

		// Helper function that returns true, if the starting segments of a shorter url of the two is contained within
		// the start of the longer one.
		// Example:
		// `Url.fromString("/foo/bar").get().matchSegments(Url.fromString("/foo/bar/baz").get()); // returns true`
		// `Url.fromString("/foo/bar").get().matchSegments(Url.fromString("/foo/baz/bar").get()); // returns false`
		// `Url.fromString("/foo/bar/baz").get().matchSegments(Url.fromString("/bar/baz").get()); // returns false`
		bool matchSegments(const Url&) const;

		// Returns the tail of the longer url from both, if both urls have matching starting segments. Otherwise, returns an
		// empty `Url`.
		Url tailDiff(const Url&) const;

		// Returns the tail of the longer url as a vector of strings from both, if both urls have matching starting segments.
		// Otherwise, returns an empty `Url`.
		std::vector<std::string> tailDiffVec(const Url&) const;

		// Returns the tail of the url (Url minus the first segment element).
		Url tail() const;

		Option<std::string> getQuery(const std::string&) const;
	private:
		std::vector<std::string> segments;
		std::string protocol;
		ushort port; // Maybe this shouldn't be in Url class?
		std::map<std::string, std::string> query;
	};
}

#endif