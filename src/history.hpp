#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace honeymoon::util {
    inline std::vector<std::string> load_history(const std::string& path) {
        std::vector<std::string> lines;
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) return lines;
        char buf[4096];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n <= 0) return lines;
        buf[n] = '\0';
        char* p = buf;
        while (*p) {
            char* nl = (char*)memchr(p, '\n', (buf + n) - p);
            if (!nl) nl = buf + n;
            *nl = '\0';
            if (*p) lines.push_back(std::string(p, nl - p));
            p = nl + 1;
        }
        return lines;
    }

    inline void save_history(const std::string& path, const std::vector<std::string>& lines) {
        int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) return;
        for (const auto& line : lines) {
            (void)write(fd, line.c_str(), line.size());
            (void)write(fd, "\n", 1);
        }
        close(fd);
    }

    inline void add_to_history(std::vector<std::string>& history, const std::string& item) {
        for (size_t i = 0; i < history.size(); ++i) {
            if (history[i] == item) {
                history.erase(history.begin() + i);
                break;
            }
        }
        history.insert(history.begin(), item);
        if (history.size() > 10) history.resize(10);
    }
}
