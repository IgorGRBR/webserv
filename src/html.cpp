#include "html.hpp"
#include <cstddef>
#include <sys/types.h>

typedef Webserv::HTMLTemplate HTMLTemplate;

HTMLTemplate::HTMLTemplate(): kvmap() {}

void HTMLTemplate::bind(const std::string& key, const std::string& value) {
	kvmap["$" + key] = value;
}

std::string HTMLTemplate::apply(std::string text) const {
	for (std::map<std::string, std::string>::const_iterator it = kvmap.begin(); it != kvmap.end(); it++) {
		size_t i = 0;
		while (true) {
			i = text.find(it->first);
			if (i == text.npos) break;
			text.replace(i, it->first.length(), it->second);
			i += it->second.length();
		}
	}
	return text;
}