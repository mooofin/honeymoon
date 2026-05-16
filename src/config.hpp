#pragma once
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace honeymoon::config {

struct Config {
  bool show_line_numbers = true;
  bool syntax_highlighting = true;
  int tab_width = 4;
  int scroll_offset = 0;
  std::string color_theme = "default";
  int font_size = 12;

  static std::string find_path() {
    {
      std::ifstream f("./.honeymoonrc");
      if (f.is_open())
        return "./.honeymoonrc";
    }
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
      std::string p = std::string(xdg) + "/honeymoon/config.moon";
      std::ifstream f(p);
      if (f.is_open())
        return p;
    }
    const char *home = std::getenv("HOME");
    if (home && home[0]) {
      std::string p = std::string(home) + "/.config/honeymoon/config.moon";
      std::ifstream f(p);
      if (f.is_open())
        return p;
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
    if (path.empty())
      return false;
    std::ifstream f(path);
    if (!f.is_open())
      return false;
    std::string line;
    while (std::getline(f, line)) {
      size_t comment = line.find('#');
      if (comment != std::string::npos)
        line = line.substr(0, comment);
      if (line.empty())
        continue;
      std::stringstream ss(line);
      std::string key, val;
      if (!(ss >> key >> val))
        continue;
      if (key == "tab_width")
        tab_width = std::stoi(val);
      else if (key == "show_line_numbers")
        show_line_numbers = (val == "true");
      else if (key == "syntax_highlighting")
        syntax_highlighting = (val == "true");
      else if (key == "color_theme")
        color_theme = val;
      else if (key == "scroll_offset")
        scroll_offset = std::stoi(val);
      else if (key == "font_size")
        font_size = std::stoi(val);
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
    std::ofstream f(path);
    if (!f.is_open())
      return false;
    f << "# Honeymoon Configuration\n";
    f << "tab_width " << tab_width << "\n";
    f << "show_line_numbers " << (show_line_numbers ? "true" : "false")
      << "\n";
    f << "syntax_highlighting " << (syntax_highlighting ? "true" : "false")
      << "\n";
    f << "color_theme " << color_theme << "\n";
    f << "scroll_offset " << scroll_offset << "\n";
    f << "font_size " << font_size << "\n";
    return true;
  }
};

} // namespace honeymoon::config
