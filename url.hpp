#ifndef URL_HPP
#define URL_HPP

#include "ystl.hpp"
#include <string>
#include <sys/types.h>
#include <vector>
namespace Webserv {
	class Url {
	public:
		Url();
		static Option<Url> fromString(const std::string&);

		const std::vector<std::string>& getSegments() const;
		const std::string& getProtocol() const;
		ushort getPort() const;
		Option<std::string> getExtension() const;
		std::string toString(bool headShash = true) const;
		bool operator==(const Url&) const;
		bool operator!=(const Url&) const;
		Url operator+(const Url&) const;

		bool matchSegments(const Url&) const;
		Url tailDiff(const Url&) const;
		std::vector<std::string> tailDiffVec(const Url&) const;
	private:
		std::vector<std::string> segments;
		std::string protocol;
		ushort port; // Maybe this shouldn't be in Url class?
	};
}

#endif