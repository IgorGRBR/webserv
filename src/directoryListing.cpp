#include "webserv.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <ctime>
#include <sstream>
#include <string>

std::string Webserv::makeDirectoryListing(const std::string &diskPath, const std::string &urlPath, bool topLevel, bool fileUploading) {
    (void)topLevel;
    (void)fileUploading;
    DIR *dir = opendir(diskPath.c_str());
    if (!dir) {
        return "<html><body>Could not open directory: " + diskPath + "</body></html>";
    }

    std::ostringstream html;
    html << "<html><head><title>Index of " << urlPath << "</title></head><body>";
    html << "<h1>Index of " << urlPath << "</h1><hr><pre>";

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string fullpath = diskPath;
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

        // Build correct URL href for the link
        std::string href = urlPath;
        if (href.empty() || href[href.size() - 1] != '/')
            href += "/";
        href += name;
        if (S_ISDIR(st.st_mode))
            href += "/";

        html << "<a href=\"" << href << "\">" << displayName << "</a>";
        int spaces = 50 - static_cast<int>(displayName.length());
        if (spaces < 1) spaces = 1;
        html << std::string(spaces, ' ')
             << timebuf << "  " << st.st_size << "\n";
    }

    html << "</pre><hr>";
    if (fileUploading) {
        html << "File uploading is enabled.<br>"
            "<form method=\"post\" enctype=\"multipart/form-data\">"
            "<label for=\"file\">Pick a file!</label>"
            "<br>"
            "<input id=\"file\" name=\"file\" type=\"file\"/>"
            "<br>"
            // "<label for=\"fileName\">Type a file name!</label>"
            // "<input id=\"fileName\" name=\"fileName\" type=\"text\"/>"
            "<button>Upload</button>"
            "</form>";
    }
    html << "</body></html>";
    closedir(dir);
    return html.str();
}
