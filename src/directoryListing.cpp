#include "webserv.hpp"
#include <dirent.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>


std::string Webserv::makeDirectoryListing(const std::string &path, bool topLevel) {
	// (void)path;
	 (void)topLevel;
	// TODO: finish this function!
	// You must read directory contents using `opendir`, `readdir` and `closedir`,
	// and then form a HTML document that contains links to the specified directory
	// resources.
	
	// Replace this with a proper return value.
	DIR *dir = opendir(path.c_str());

	if(!dir)
		return "couldnt open directory";


	std::ostringstream html;
	html << "<html><head><title> Index of " << path << "</title></head></body>";
	html << "<h1> Index of " << path << "</h1><hr><pre>";
		
	struct dirent *entry;

	while ((entry = readdir(dir)) != NULL){
		std::string name = entry->d_name;
		if (name == "." || name == "..") continue;

		std::string fullpath = path;
		if(fullpath[fullpath.length() - 1] != '/')
			fullpath += "/";
		fullpath += name;


		struct stat st;
		if(stat(fullpath.c_str(), &st) == -1)
			continue;
		

		std::string displayName = name;
		if(S_ISDIR(st.st_mode))
			displayName += "/";

		html << "<a href=\" " << displayName << "\">" << displayName << "</a>";


	}

	html << "</hr></body></html>";
	closedir(dir);
	return html.str();
	//return std::string(":P (topLevel is ") + (topLevel ? "true" : "false") + ")";
}