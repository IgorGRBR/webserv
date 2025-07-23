#ifndef HTML_HPP
#define HTML_HPP

#include <map>
#include <string>
namespace Webserv {

	// `HTMLTemplate` contains a set of HTML templating variables, and performs templating upon
	// HTML strings.
	class HTMLTemplate {
	public:

		// Constructs an empty HTML template.
		HTMLTemplate();

		// Binds a variable name (`key`) to the specified `value`.
		void bind(const std::string& key, const std::string& value);

		// Performs the templating upon the HTML string stored in `text`. Returns a new string, where
		// variable names are replaced with their corresponding bound values.
		std::string apply(std::string text) const;
	private:
		std::map<std::string, std::string> kvmap;
	};
}

#endif