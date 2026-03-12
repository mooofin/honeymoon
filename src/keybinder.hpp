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
        std::string token = words[i];
        
        // Handle M- prefix (Meta key = Esc + key)
        if (token.size() >= 3 && token[0] == 'M' && token[1] == '-') {
          b.keys.push_back(Key::Esc);
          token = token.substr(2); // Remove "M-"
        }
        
        Key k = key_from_string(token);
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
