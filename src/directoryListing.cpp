#include "webserv.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <ctime>
#include <sstream>
#include <iomanip> // Still available in C++98 for setw
#include <string>

std::string Webserv::makeDirectoryListing(const std::string &path, bool topLevel) {
    (void)topLevel;
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        return "<html><body>Could not open directory: " + path + "</body></html>";
    }

    std::ostringstream html;
    html << "<html><head><title>Index of " << path << "</title></head><body>";
    html << "<h1>Index of " << path << "</h1><hr><pre>";

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string fullpath = path;
        if (fullpath[fullpath.size() - 1] != '/')
            fullpath += "/";
        fullpath += name;

        struct stat st;
        if (stat(fullpath.c_str(), &st) == -1)
            continue; // skip if cannot stat

        // Format last modified time
        char timebuf[20];
        std::tm *tm_info = std::localtime(&st.st_mtime);
        std::strftime(timebuf, sizeof(timebuf), "%d-%b-%Y %H:%M", tm_info);

        std::string displayName = name;
        if (S_ISDIR(st.st_mode))
            displayName += "/";

        html << "<a href=\"" << displayName << "\">" << displayName << "</a>";
        int spaces = 50 - static_cast<int>(displayName.length());
        if (spaces < 1) spaces = 1;
        html << std::string(spaces, ' ') 
             << timebuf << "  " << st.st_size << "\n";
    }

    html << "</pre><hr></body></html>";
    closedir(dir);
    return html.str();
}