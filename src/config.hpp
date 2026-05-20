#pragma once
#include <cstdlib>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace honeymoon::config {

struct Config {
  bool show_line_numbers = true;
  bool syntax_highlighting = true;
  int tab_width = 4;
  int scroll_offset = 0;
  int font_size = 12;

  static bool file_exists(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    close(fd);
    return true;
  }

  static std::string find_path() {
    if (file_exists("./.honeymoonrc")) return "./.honeymoonrc";
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
      std::string p = std::string(xdg) + "/honeymoon/config.moon";
      if (file_exists(p)) return p;
    }
    const char *home = std::getenv("HOME");
    if (home && home[0]) {
      std::string p = std::string(home) + "/.config/honeymoon/config.moon";
      if (file_exists(p)) return p;
    }
    return "";
  }

  static std::string default_save_path() {
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0])
      return std::string(xdg) + "/honeymoon/config.moon";
    const char *home = std::getenv("HOME");
    if (home && home[0])
      return std::string(home) + "/.config/honeymoon/config.moon";
    return "./.honeymoonrc";
  }

  bool load() {
    std::string path = find_path();
    if (path.empty()) return false;
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return false;
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
      char* sp = (char*)memchr(line, ' ', nl - line);
      if (!sp) { char* tab = (char*)memchr(line, '\t', nl - line); sp = tab; }
      if (!sp) continue;
      *sp = '\0';
      char* key = line;
      char* val = sp + 1;
      if (!*key || !*val) continue;
      if (strcmp(key, "tab_width") == 0)
        tab_width = atoi(val);
      else if (strcmp(key, "show_line_numbers") == 0)
        show_line_numbers = (strcmp(val, "true") == 0);
      else if (strcmp(key, "syntax_highlighting") == 0)
        syntax_highlighting = (strcmp(val, "true") == 0);
    }
    return true;
  }

  bool save() {
    std::string path = default_save_path();
    size_t slash = path.rfind('/');
    if (slash != std::string::npos) {
      std::string dir = path.substr(0, slash);
      mkdir(dir.c_str(), 0755);
    }
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;
    char buf[512];
    int len = snprintf(buf, sizeof(buf),
      "# Honeymoon Configuration\n"
      "tab_width %d\n"
      "show_line_numbers %s\n"
      "syntax_highlighting %s\n",
      tab_width, show_line_numbers ? "true" : "false",
      syntax_highlighting ? "true" : "false");
    if (len > 0) (void)write(fd, buf, len);
    close(fd);
    return true;
  }
};

} // namespace honeymoon::config
