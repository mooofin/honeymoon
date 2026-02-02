#pragma once
#include "input.hpp"
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace honeymoon::config {

class KeyBinder {
public:
  struct Binding {
    std::vector<Key> keys;
    std::string action;
  };

  static std::vector<Binding> load_from_file(const std::string &path) {
    std::vector<Binding> bindings;
    std::ifstream f(path);
    if (!f.is_open())
      return bindings;

    std::string line;
    while (std::getline(f, line)) {
      size_t comment_pos = line.find('#');
      if (comment_pos != std::string::npos) {
        line = line.substr(0, comment_pos);
      }
      if (line.empty())
        continue;

      std::stringstream ss(line);
      std::string word;
      std::vector<std::string> words;
      while (ss >> word)
        words.push_back(word);

      if (words.size() < 2)
        continue;

      Binding b;
      b.action = words.back();
      for (size_t i = 0; i < words.size() - 1; ++i) {
        Key k = key_from_string(words[i]);
        if (k != Key::None)
          b.keys.push_back(k);
      }

      if (!b.keys.empty()) {
        bindings.push_back(b);
      }
    }
    return bindings;
  }
};

} // namespace honeymoon::config
