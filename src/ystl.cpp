#include "ystl.hpp"
#include <cctype>
#include <sys/types.h>
#include <sys/stat.h>


std::string strToLower(const std::string& str) {
	std::string newStr;
	for (uint i = 0; i < str.length(); i++) {
		newStr.push_back(std::tolower(str[i]));
	}
	return newStr;
};

std::string trimString(const std::string& str, char c) {
	uint firstIndex = 0;
	while (str[firstIndex] == c) {
		firstIndex++;
	}

	uint lastIndex = str.length() - 1;
	while (lastIndex > 0 && str[lastIndex] == c) {
		lastIndex--;
	}
	
	if (lastIndex < firstIndex) return "";

	std::string newStr;
	for (uint i = firstIndex; i <= lastIndex; i++) {
		newStr.push_back(str[i]);
	}
	return newStr;
};

FSType checkFSType(const std::string &path) {
	struct stat s;
	if(stat(path.c_str(), &s) == 0) {
		if( s.st_mode & S_IFDIR ) {
			return FS_DIRECTORY;
		}
		else if( s.st_mode & S_IFREG ) {
			return FS_FILE;
		}
		else {
			return FS_NONE;
		}
	}
	else {
		return FS_NONE;
	}
}