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

std::string trimString(const std::string& str, char c) {
	uint lastIndex = str.length() - 1;
	while (lastIndex > 0 && str[lastIndex] == c) {
		lastIndex--;
	}
	if (lastIndex == 0) return "";

	uint firstIndex = 0;
	while (str[firstIndex] == c) {
		firstIndex++;
	}

	std::string newStr;
	for (uint i = firstIndex; i <= lastIndex; i++) {
		newStr.push_back(str[i]);
	}
	return newStr;
};
