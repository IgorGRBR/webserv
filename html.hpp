#ifndef HTML_HPP
#define HTML_HPP

#include <map>
#include <string>
namespace Webserv {
	class HTMLTemplate {
	public:
		HTMLTemplate();
		void bind(const std::string& key, const std::string& value);
		std::string apply(std::string text) const;
	private:
		std::map<std::string, std::string> kvmap;
	};
}

#endif