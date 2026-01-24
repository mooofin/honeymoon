#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>

namespace honeymoon::util {
    inline std::vector<std::string> load_history(const std::string& path) {
        std::vector<std::string> lines;
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) lines.push_back(line);
        }
        return lines;
    }

    inline void save_history(const std::string& path, const std::vector<std::string>& lines) {
        std::ofstream file(path);
        for (const auto& line : lines) file << line << "\n";
    }

    inline void add_to_history(std::vector<std::string>& history, const std::string& item) {
        auto it = std::find(history.begin(), history.end(), item);
        if (it != history.end()) history.erase(it);
        history.insert(history.begin(), item);
        if (history.size() > 10) history.resize(10);
    }
}
