#include "ystl.hpp"
#include <cctype>
#include <sys/types.h>

std::string strToLower(const std::string& str) {
	std::string newStr;
	for (uint i = 0; i < str.length(); i++) {
		newStr.push_back(std::tolower(str[i]));
	}
	return newStr;
};