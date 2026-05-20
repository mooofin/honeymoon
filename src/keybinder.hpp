#pragma once
#include "input.hpp"
#include <cstring>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

namespace honeymoon::config {

class KeyBinder {
public:
  struct Binding {
    std::vector<Key> keys;
    std::string action;
  };

  static std::vector<Binding> load_from_file(const std::string &path) {
    std::vector<Binding> bindings;
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return bindings;
    char buf[8192];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return bindings;
    buf[n] = '\0';

    char* p = buf;
    while (*p) {
      char* nl = (char*)memchr(p, '\n', (buf + n) - p);
      if (!nl) nl = buf + n;
      *nl = '\0';
      char* line = p;
      p = nl + 1;

      char* comment = (char*)memchr(line, '#', nl - line);
      if (comment) *comment = '\0';

      // Tokenize the line manually
      std::vector<std::string> words;
      char* wp = line;
      while (*wp) {
        while (*wp == ' ' || *wp == '\t') wp++;
        if (!*wp) break;
        char* start = wp;
        while (*wp && *wp != ' ' && *wp != '\t') wp++;
        words.push_back(std::string(start, wp - start));
      }

      if (words.size() < 2) continue;

      Binding b;
      b.action = words.back();
      for (size_t i = 0; i < words.size() - 1; ++i) {
        std::string token = words[i];
        if (token.size() >= 3 && token[0] == 'M' && token[1] == '-') {
          b.keys.push_back(Key::Esc);
          token = token.substr(2);
        }
        Key k = key_from_string(token);
        if (k != Key::None)
          b.keys.push_back(k);
      }
      if (!b.keys.empty())
        bindings.push_back(b);
    }
    return bindings;
  }
};

} // namespace honeymoon::config
