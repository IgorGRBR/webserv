#include "webserv.hpp"

std::string Webserv::makeDirectoryListing(const std::string &path, bool topLevel) {
	(void)path;
	(void)topLevel;
	// TODO: finish this function!
	// You must read directory contents using `opendir`, `readdir` and `closedir`,
	// and then form a HTML document that contains links to the specified directory
	// resources.
	
	// Replace this with a proper return value.
	return std::string(":P (topLevel is ") + (topLevel ? "true" : "false") + ")";
}