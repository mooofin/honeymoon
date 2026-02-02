#pragma once
#include "concepts.hpp"
#include "history.hpp"
#include "input.hpp"
#include "keybinder.hpp"
#include "logo.hpp"
#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace honeymoon::kernel {
enum class Mode {
  Editor,
  Home,
  FileSearch,
  TextSearch,
  GotoLine,
  RecentFiles,
  Settings,
  Help,
  About
};

template <typename BufferPolicy, typename TerminalPolicy>
  requires EditableBuffer<BufferPolicy> && TerminalDevice<TerminalPolicy>
class Editor {
public:
  Editor() : output_buffer("") {
    update_window_size();
    bind_default_keys();
    std::stringstream ss(std::string(honeymoon::STARTUP_LOGO));
    std::string line;
    while (std::getline(ss, line)) {
      logo_lines.push_back(line);
    }
    if (!logo_lines.empty() && logo_lines[0].empty())
      logo_lines.erase(logo_lines.begin());
    if (!logo_lines.empty() && logo_lines.back().empty())
      logo_lines.pop_back();

    recent_files = honeymoon::util::load_history(".honeymoon_history");
    current_mode = Mode::Home;
    status_message = "Welcome to Honeymoon";
  }

  void open(const std::string &filename) {
    current_filename = filename;
    buffer.load_from_file(filename);
    status_message = "Opened " + filename;
    current_mode = Mode::Editor;
    honeymoon::util::add_to_history(recent_files, filename);
    honeymoon::util::save_history(".honeymoon_history", recent_files);
  }

  void run() {
    while (!should_quit) {
      refresh_screen();
      process_keypress();
    }
    terminal.write_raw("\x1b[2J\x1b[H");
  }

private:
  TerminalPolicy terminal;
  BufferPolicy buffer;
  bool should_quit = false;
  std::string current_filename = "[No Name]";
  std::string status_message =
      "Honeymoon | C-x C-c: Quit | C-x C-s: Save | C-SP: Mark";
  std::string output_buffer;
  std::string clipboard;
  size_t scroll_row = 0, scroll_col = 0;
  int window_rows = 0, window_cols = 0;
  size_t selection_anchor = std::string::npos;
  std::vector<std::string> logo_lines;
  Mode current_mode = Mode::Home;
  int menu_selection = 0;
  std::vector<std::string> recent_files;
  std::string search_query;
  size_t search_start_idx = 0;
  bool search_forward = true;

  std::map<std::string, std::function<void()>> actions;

  struct KeyNode {
    std::map<Key, std::shared_ptr<KeyNode>> children;
    std::string action;
  };

  std::shared_ptr<KeyNode> root_node;
  std::shared_ptr<KeyNode> current_node;
  std::vector<Key> pending_keys;

  void bind_default_keys() {
    actions["quit"] = [this]() { should_quit = true; };
    actions["save_file"] = [this]() {
      buffer.save_to_file(current_filename);
      status_message = "Saved";
    };
    actions["mark_set"] = [this]() {
      selection_anchor = buffer.get_cursor();
      status_message = "Mark Set";
    };
    actions["cancel"] = [this]() {
      if (current_mode == Mode::GotoLine) {
        current_mode = Mode::Editor;
        status_message = "Cancelled";
      } else {
        selection_anchor = std::string::npos;
        current_node = root_node;
        pending_keys.clear();
        status_message = "Quit";
      }
    };
    actions["cut"] = [this]() {
      if (selection_anchor != std::string::npos) {
        size_t c = buffer.get_cursor();
        clipboard = buffer.get_range(selection_anchor, c);
        buffer.delete_range(selection_anchor, c);
        selection_anchor = std::string::npos;
        status_message = "Cut";
      } else
        status_message = "No selection";
    };
    actions["yank"] = [this]() {
      if (!clipboard.empty()) {
        buffer.insert_string(clipboard);
        status_message = "Yank";
      } else
        status_message = "Empty";
    };
    actions["move_line_start"] = [this]() { move_line_start(); };
    actions["move_line_end"] = [this]() { move_line_end(); };
    actions["kill_line"] = [this]() { kill_to_eol(); };
    actions["recenter"] = [this]() { recenter_view(); };
    actions["transpose_chars"] = [this]() { transpose_chars(); };
    actions["newline"] = [this]() { buffer.insert_char('\n'); };
    actions["search_forward"] = [this]() {
      current_mode = Mode::TextSearch;
      search_forward = true;
      search_start_idx = buffer.get_cursor();
      search_query = "";
      status_message = "I-Search: ";
    };
    actions["search_backward"] = [this]() {
      current_mode = Mode::TextSearch;
      search_forward = false;
      search_start_idx = buffer.get_cursor();
      search_query = "";
      status_message = "I-Search Back: ";
    };
    actions["indent"] = [this]() { perform_indent(true); };
    actions["dedent"] = [this]() { perform_indent(false); };
    actions["delete_backward"] = [this]() { buffer.delete_char(); };
    actions["delete_forward"] = [this]() { buffer.delete_forward(); };
    actions["move_up"] = [this]() { move_cursor_2d(-1, 0); };
    actions["move_down"] = [this]() { move_cursor_2d(1, 0); };
    actions["move_left"] = [this]() { move_cursor_lin(-1); };
    actions["move_right"] = [this]() { move_cursor_lin(1); };
    actions["undo"] = [this]() { status_message = "Undo not impl"; };

    actions["copy"] = [this]() {
      if (selection_anchor != std::string::npos) {
        clipboard = buffer.get_range(selection_anchor, buffer.get_cursor());
        selection_anchor = std::string::npos;
        status_message = "Copy";
      } else
        status_message = "No selection";
    };
    actions["move_word_backward"] = [this]() { move_word_backward(); };
    actions["move_word_forward"] = [this]() { move_word_forward(); };
    actions["kill_word"] = [this]() { kill_word(); };
    actions["transpose_words"] = [this]() { transpose_words(); };
    actions["goto_line"] = [this]() {
      current_mode = Mode::GotoLine;
      search_query = "";
      status_message = "Go to line: ";
    };

    actions["find_file"] = [this]() {
      current_mode = Mode::FileSearch;
      search_query = "";
      status_message = "Find File: ";
    };
    actions["list_buffers"] = [this]() {
      current_mode = Mode::RecentFiles;
      menu_selection = 0;
    };
    actions["kill_buffer"] = [this]() {
      current_filename = "[No Name]";
      buffer = BufferPolicy();
      current_mode = Mode::Home;
      status_message = "Buffer Closed";
    };
    actions["select_all"] = [this]() {
      selection_anchor = 0;
      buffer.move_gap(buffer.size());
      status_message = "Select All";
    };
    actions["help_key"] = [this]() {
      current_mode = Mode::Help;
      status_message = "Help: Describe Key";
    };
    actions["help_func"] = [this]() {
      current_mode = Mode::Help;
      status_message = "Help: Describe Function";
    };

    root_node = std::make_shared<KeyNode>();
    current_node = root_node;

    add_binding({Key::Ctrl_Space}, "mark_set");
    add_binding({Key::Ctrl_G}, "cancel");
    add_binding({Key::Ctrl_W}, "cut");
    add_binding({Key::Ctrl_Y}, "yank");
    add_binding({Key::Ctrl_A}, "move_line_start");
    add_binding({Key::Ctrl_E}, "move_line_end");
    add_binding({Key::Ctrl_K}, "kill_line");
    add_binding({Key::Ctrl_L}, "recenter");
    add_binding({Key::Ctrl_T}, "transpose_chars");
    add_binding({Key::Ctrl_J}, "newline");
    add_binding({Key::Enter}, "newline");
    add_binding({Key::Ctrl_S}, "search_forward");
    add_binding({Key::Ctrl_R}, "search_backward");
    add_binding({Key::Tab}, "indent");
    add_binding({Key::ShiftTab}, "dedent");
    add_binding({Key::Backspace}, "delete_backward");
    add_binding({Key::Del}, "delete_forward");
    add_binding({Key::ArrowUp}, "move_up");
    add_binding({Key::Ctrl_P}, "move_up");
    add_binding({Key::ArrowDown}, "move_down");
    add_binding({Key::Ctrl_N}, "move_down");
    add_binding({Key::ArrowLeft}, "move_left");
    add_binding({Key::Ctrl_B}, "move_left");
    add_binding({Key::ArrowRight}, "move_right");
    add_binding({Key::Ctrl_F}, "move_right");
    add_binding({Key::Ctrl_Slash}, "undo");

    add_binding({Key::Ctrl_X, Key::Ctrl_C}, "quit");
    add_binding({Key::Ctrl_X, Key::Ctrl_S}, "save_file");
    add_binding({Key::Ctrl_X, Key::Ctrl_F}, "find_file");
    add_binding({Key::Ctrl_X, Key::Ctrl_B}, "list_buffers");
    add_binding({Key::Ctrl_X, (Key)'b'}, "list_buffers");
    add_binding({Key::Ctrl_X, (Key)'k'}, "kill_buffer");
    add_binding({Key::Ctrl_X, (Key)'h'}, "select_all");

    for (const auto &[key, act] : std::vector<std::pair<Key, std::string>>{
             {(Key)'w', "copy"},
             {(Key)'b', "move_word_backward"},
             {(Key)'f', "move_word_forward"},
             {(Key)'d', "kill_word"},
             {(Key)'t', "transpose_words"},
             {(Key)'g', "goto_line"}}) {
      add_binding({Key::Esc, key}, act);
      char c = (char)key;
      if (c >= 'a' && c <= 'z') {
        char upper = c - 32;
        add_binding({Key::Esc, (Key)upper}, act);
      }
    }

    add_binding({Key::Ctrl_H, (Key)'k'}, "help_key");
    add_binding({Key::Ctrl_H, (Key)'f'}, "help_func");

    load_custom_binds();
  }

  void add_binding(const std::vector<Key> &keys, const std::string &action) {
    if (keys.empty())
      return;
    std::shared_ptr<KeyNode> node = root_node;
    for (const auto &k : keys) {
      if (node->children.find(k) == node->children.end()) {
        node->children[k] = std::make_shared<KeyNode>();
      }
      node = node->children[k];
    }
    node->action = action;
  }

  void load_custom_binds() {
    auto binds = honeymoon::config::KeyBinder::load_from_file("keybinds.moon");
    for (auto &b : binds) {
      add_binding(b.keys, b.action);
    }
  }

  struct EditorSettings {
    bool show_line_numbers = true;
    bool syntax_highlighting = true;
    int tab_width = 4;
  } settings;

  const std::vector<std::string> home_menu = {
      "File Searcher", "Recent Files", "Settings", "Help", "About", "Quit"};
  const std::vector<std::string> settings_menu = {
      "Line Numbers", "Syntax Highlighting", "Tab Width", "Back"};

  void update_window_size() {
    auto [rows, cols] = terminal.get_window_size();
    window_rows = rows;
    window_cols = cols;
    if (window_rows > 2)
      window_rows -= 2;
  }

  void refresh_screen() {
    update_window_size();
    output_buffer.clear();
    if (current_mode == Mode::Editor || current_mode == Mode::TextSearch ||
        current_mode == Mode::GotoLine) {
      output_buffer.append("\x1b[?25l\x1b[H");
      draw_rows();
      draw_status_bar();
      draw_message_bar();
      place_cursor();
    } else {
      output_buffer.append("\x1b[?25l\x1b[2J\x1b[H");
      draw_centered_view();
    }

    output_buffer.append("\x1b[?25h");
    terminal.write_raw(output_buffer);
  }

  int get_display_width(const std::string &s) {
    int w = 0;
    for (size_t i = 0; i < s.length(); ++i) {
      unsigned char c = (unsigned char)s[i];
      if ((c & 0xC0) != 0x80)
        w++;
    }
    return w;
  }

  void draw_logo(int start_y) {
    int max_width = 0;
    for (const auto &line : logo_lines) {
      int w = get_display_width(line);
      if (w > max_width)
        max_width = w;
    }
    int pad = (window_cols - max_width) / 2;
    if (pad < 0)
      pad = 0;

    for (int i = 0; i < (int)logo_lines.size(); ++i) {
      int y = start_y + i;
      if (y >= window_rows)
        break;
      output_buffer.append(std::format("\x1b[{};{}H", y + 1, 1));
      output_buffer.append(std::string(pad, ' ')).append(logo_lines[i]);
    }
  }

  void draw_centered_view() {

    int logo_start_y = window_rows / 5;
    if (logo_start_y < 1)
      logo_start_y = 1;
    draw_logo(logo_start_y);

    int menu_start_y = logo_start_y + logo_lines.size() + 2;

    if (current_mode == Mode::Home) {
      for (int i = 0; i < (int)home_menu.size(); ++i) {
        std::string item = home_menu[i];
        if (i == menu_selection)
          output_buffer.append("\x1b[7m");
        draw_centered_text(menu_start_y + i, item);
        if (i == menu_selection)
          output_buffer.append("\x1b[m");
      }
    } else if (current_mode == Mode::About) {
      draw_centered_text(menu_start_y, "Honeymoon Editor v0.1");
      draw_centered_text(menu_start_y + 2, "A minimal C++ editor.");
      draw_centered_text(menu_start_y + 4, "Made by Muffin");
      draw_centered_text(menu_start_y + 6, "Press Esc to return.");
    } else if (current_mode == Mode::Help) {
      draw_centered_text(menu_start_y, "Keys:");
      draw_centered_text(menu_start_y + 1, "C-x C-c: Quit");
      draw_centered_text(menu_start_y + 2, "C-x C-s: Save");
      draw_centered_text(menu_start_y + 4, "Press Esc to return.");
    } else if (current_mode == Mode::Settings) {
      draw_centered_text(logo_start_y, "SETTINGS");
      for (int i = 0; i < (int)settings_menu.size(); ++i) {
        std::string label = settings_menu[i];
        std::string val;
        if (label == "Line Numbers")
          val = settings.show_line_numbers ? " [ON] " : " [OFF]";
        else if (label == "Syntax Highlighting")
          val = settings.syntax_highlighting ? " [ON] " : " [OFF]";
        else if (label == "Tab Width")
          val = " [" + std::to_string(settings.tab_width) + "] ";
        else
          val = "";

        if (i == menu_selection)
          output_buffer.append("\x1b[7m");
        draw_centered_text(menu_start_y + i, label + val);
        if (i == menu_selection)
          output_buffer.append("\x1b[m");
      }
    } else if (current_mode == Mode::RecentFiles) {
      if (recent_files.empty()) {
        draw_centered_text(menu_start_y, "No recent files.");
      } else {
        for (int i = 0; i < (int)recent_files.size(); ++i) {
          if (i == menu_selection)
            output_buffer.append("\x1b[7m");
          draw_centered_text(menu_start_y + i, recent_files[i]);
          if (i == menu_selection)
            output_buffer.append("\x1b[m");
        }
      }
    } else if (current_mode == Mode::FileSearch) {
      draw_centered_text(menu_start_y, "Search File: " + search_query);
    }
  }

  void draw_centered_text(int y, const std::string &text) {
    if (y >= window_rows)
      return;
    int width = get_display_width(text);
    int pad = (window_cols - width) / 2;
    if (pad < 0)
      pad = 0;
    output_buffer.append(std::format("\x1b[{};{}H", y + 1, pad + 1))
        .append(text);
  }

  struct EditorCursor {
    size_t idx;
    int r;
    int c;
  };

  EditorCursor get_visual_cursor() {
    std::string content = buffer.get_content();
    size_t cursor = buffer.get_cursor();
    int r = 0, c = 0;
    for (size_t i = 0; i < cursor && i < content.size(); ++i) {
      if (content[i] == '\n') {
        r++;
        c = 0;
      } else {
        c++;
      }
    }
    return {cursor, r, c};
  }

  void draw_rows() {
    std::string content = buffer.get_content();
    std::vector<size_t> lines = {0};
    for (size_t i = 0; i < content.size(); ++i)
      if (content[i] == '\n')
        lines.push_back(i + 1);

    EditorCursor cur = get_visual_cursor();
    if (cur.r < (int)scroll_row)
      scroll_row = cur.r;
    if (cur.r >= (int)scroll_row + window_rows)
      scroll_row = cur.r - window_rows + 1;

    for (int y = 0; y < window_rows; y++) {
      size_t file_row = y + scroll_row;
      if (file_row < lines.size()) {
        if (settings.show_line_numbers)
          output_buffer.append(
              std::format("\x1b[36m{:4} \x1b[39m", file_row + 1));
        else
          output_buffer.append(" ");
      } else {
        output_buffer.append("     ");
      }

      if (file_row >= lines.size()) {
        if (content.empty()) {
          int logo_start_y = window_rows / 3;
          int logo_row = y - logo_start_y;
          if (logo_row >= 0 && logo_row < (int)logo_lines.size()) {
            std::string &msg = logo_lines[logo_row];
            int width = get_display_width(msg);
            int pad = (window_cols - 5 - width) / 2;
            if (pad > 0)
              output_buffer.append("~");
            output_buffer.append(std::string(std::max(0, pad - 1), ' '))
                .append(msg);
          } else {
            output_buffer.append("~");
          }
        } else {
          output_buffer.append("~");
        }
      } else {
        size_t start = lines[file_row];
        size_t len = (file_row + 1 < lines.size())
                         ? lines[file_row + 1] - start - 1
                         : content.size() - start;
        if (len > (size_t)(window_cols - 5))
          len = window_cols - 5;
        std::string_view line_view(content.data() + start, len);

        for (size_t i = 0; i < len; ++i) {
          size_t abs = start + i;
          bool sel = (selection_anchor != std::string::npos &&
                      abs >= std::min(selection_anchor, buffer.get_cursor()) &&
                      abs < std::max(selection_anchor, buffer.get_cursor()));
          if (sel)
            output_buffer.append("\x1b[7m");

          char c = line_view[i];
          if (settings.syntax_highlighting) {
            if (isdigit(c) && !sel)
              output_buffer.append("\x1b[36m");
            else if (c == '"' && !sel)
              output_buffer.append("\x1b[32m");
          }

          output_buffer.append(1, c);
          if (settings.syntax_highlighting) {
            if (sel || isdigit(c) || c == '"')
              output_buffer.append("\x1b[m");
          } else if (sel)
            output_buffer.append("\x1b[m");
        }
      }
      output_buffer.append("\x1b[K\r\n");
    }
  }

  void draw_status_bar() {
    std::string stat = std::format("File: {} ", current_filename,
                                   buffer.is_dirty() ? "[+]" : "");
    std::string rstat =
        std::format("{}/{}", get_visual_cursor().r + 1, buffer.size());
    size_t len = stat.length(), rlen = rstat.length();
    if (len > (size_t)window_cols)
      len = window_cols;
    output_buffer.append("\x1b[7m").append(stat.substr(0, len));
    while (len < (size_t)window_cols) {
      if (window_cols - len == rlen) {
        output_buffer.append(rstat);
        break;
      }
      output_buffer.append(" ");
      len++;
    }
    output_buffer.append("\x1b[m\r\n");
  }

  void draw_message_bar() {
    output_buffer.append("\x1b[K").append(status_message);
  }

  void place_cursor() {
    EditorCursor cur = get_visual_cursor();
    int r = cur.r - scroll_row, c = cur.c - scroll_col;
    if (r < 0)
      r = 0;
    if (r >= window_rows)
      r = window_rows - 1;
    output_buffer.append(std::format("\x1b[{};{}H", r + 1, c + 1 + 5));
  }

  void process_keypress() {
    Key k = terminal.read_key();
    if (k == Key::None)
      return;

    if (current_mode == Mode::TextSearch) {
      if (k == Key::Enter || k == Key::Esc) {
        current_mode = Mode::Editor;
        status_message = "";
        return;
      }
      if (k == Key::Ctrl_G) {
        current_mode = Mode::Editor;
        buffer.move_gap(search_start_idx);
        status_message = "Cancelled";
        return;
      }

      bool next = false;
      if (k == Key::Backspace || k == Key::Ctrl_H) {
        if (!search_query.empty())
          search_query.pop_back();
      } else if (k == Key::Ctrl_S) {
        search_forward = true;
        next = true;
      } else if (k == Key::Ctrl_R) {
        search_forward = false;
        next = true;
      } else if (is_printable((int)k)) {
        search_query.push_back((char)k);
      }

      std::string c = buffer.get_content();
      size_t start_pos = buffer.get_cursor();
      if (next)
        start_pos = (search_forward) ? start_pos + 1
                                     : (start_pos > 0 ? start_pos - 1 : 0);

      size_t found = std::string::npos;
      if (search_forward)
        found = c.find(search_query, start_pos);
      else
        found = c.rfind(search_query, start_pos);

      if (found != std::string::npos) {
        buffer.move_gap(found);
        status_message = "I-Search: " + search_query;
      } else {
        status_message = "Failing I-Search: " + search_query;
      }
      return;
    }

    if (current_mode != Mode::Editor) {
      if (k == Key::Esc) {
        current_mode = Mode::Home;
        menu_selection = 0;
        search_query.clear();
        return;
      }

      if (current_mode == Mode::Home) {
        if (k == Key::ArrowUp || k == Key::Ctrl_P) {
          menu_selection--;
          if (menu_selection < 0)
            menu_selection = home_menu.size() - 1;
        } else if (k == Key::ArrowDown || k == Key::Ctrl_N) {
          menu_selection++;
          if (menu_selection >= (int)home_menu.size())
            menu_selection = 0;
        } else if (k == Key::Enter) {
          std::string sel = home_menu[menu_selection];
          if (sel == "Quit")
            should_quit = true;
          else if (sel == "About")
            current_mode = Mode::About;
          else if (sel == "Help")
            current_mode = Mode::Help;
          else if (sel == "Settings")
            current_mode = Mode::Settings;
          else if (sel == "Recent Files") {
            current_mode = Mode::RecentFiles;
            menu_selection = 0;
          } else if (sel == "File Searcher") {
            current_mode = Mode::FileSearch;
            search_query = "";
          }
        }
      } else if (current_mode == Mode::RecentFiles) {
        if (!recent_files.empty()) {
          if (k == Key::ArrowUp) {
            menu_selection--;
            if (menu_selection < 0)
              menu_selection = recent_files.size() - 1;
          } else if (k == Key::ArrowDown) {
            menu_selection++;
            if (menu_selection >= (int)recent_files.size())
              menu_selection = 0;
          } else if (k == Key::Enter) {
            open(recent_files[menu_selection]);
          }
        }
      } else if (current_mode == Mode::FileSearch) {
        if (k == Key::Enter) {
          if (!search_query.empty())
            open(search_query);
        } else if (k == Key::Backspace || k == Key::Ctrl_H) {
          if (!search_query.empty())
            search_query.pop_back();
        } else if (is_printable((int)k))
          search_query.push_back((char)k);
      } else if (current_mode == Mode::GotoLine) {
        if (k == Key::Enter) {
          if (!search_query.empty()) {
            try {
              int line = std::stoi(search_query);
              std::string c = buffer.get_content();
              size_t idx = 0;
              int current_line = 1;
              while (idx < c.size() && current_line < line) {
                if (c[idx] == '\n')
                  current_line++;
                idx++;
              }
              buffer.move_gap(idx);
              status_message = "Jumped to line " + search_query;
            } catch (...) {
              status_message = "Invalid number";
            }
          }
          current_mode = Mode::Editor;
        } else if (k == Key::Esc || k == Key::Ctrl_G) {
          current_mode = Mode::Editor;
          status_message = "Cancelled";
        } else if (k == Key::Backspace || k == Key::Ctrl_H) {
          if (!search_query.empty())
            search_query.pop_back();
        } else if (isdigit((char)k))
          search_query.push_back((char)k);
      }
      return;
    }

    if (k == Key::Esc && current_node == root_node) {
      if (selection_anchor != std::string::npos) {
        selection_anchor = std::string::npos;
        status_message = "Selection Cancelled";
        return;
      }
    }

    auto it = current_node->children.find(k);
    if (it != current_node->children.end()) {
      current_node = it->second;
      pending_keys.push_back(k);

      if (!current_node->action.empty() && current_node->children.empty()) {

        std::string act = current_node->action;
        current_node = root_node;
        pending_keys.clear();
        status_message = "";
        if (actions.count(act))
          actions[act]();
        else
          status_message = "Action not found: " + act;
      } else {

        std::string msg = "";
        for (auto pk : pending_keys) {
          msg += to_string(pk) + " ";
        }
        status_message = msg;
      }
    } else {

      if (current_node != root_node) {

        status_message = "Undefined Key";
        current_node = root_node;
        pending_keys.clear();
      } else {

        if (is_printable((int)k) && k != Key::Esc) {
          buffer.insert_char((char)k);
          status_message = "";
        } else {
          status_message = "Unbound Key";
        }
      }
    }
  }

  void move_cursor_lin(int off) {
    long long np = (long long)buffer.get_cursor() + off;
    if (np < 0)
      np = 0;
    if (np > (long long)buffer.size())
      np = buffer.size();
    buffer.move_gap((size_t)np);
  }

  void move_cursor_2d(int rd, int cd) {
    EditorCursor cur = get_visual_cursor();
    int tr = cur.r + rd;
    if (tr < 0)
      tr = 0;
    std::string c = buffer.get_content();
    int br = 0;
    size_t i = 0;
    while (br < tr && i < c.size()) {
      if (c[i] == '\n')
        br++;
      i++;
    }
    if (br < tr) {
      buffer.move_gap(c.size());
      return;
    }
    int tc = cur.c + cd;
    size_t eol = i;
    while (eol < c.size() && c[eol] != '\n')
      eol++;
    if (tc < 0)
      tc = 0;
    if (tc > (int)(eol - i))
      tc = eol - i;
    buffer.move_gap(i + tc);
  }

  bool is_separator(char c) { return std::isspace(c) || std::ispunct(c); }

  void move_word_forward() {
    std::string c = buffer.get_content();
    size_t idx = buffer.get_cursor();
    if (idx >= c.size())
      return;
    while (idx < c.size() && is_separator(c[idx]))
      idx++;
    while (idx < c.size() && !is_separator(c[idx]))
      idx++;
    buffer.move_gap(idx);
  }

  void move_word_backward() {
    size_t idx = buffer.get_cursor();
    if (idx == 0)
      return;
    std::string c = buffer.get_content();
    idx--;
    while (idx > 0 && is_separator(c[idx]))
      idx--;
    while (idx > 0 && !is_separator(c[idx]))
      idx--;
    if (is_separator(c[idx]))
      idx++;
    buffer.move_gap(idx);
  }

  void move_line_start() {
    std::string c = buffer.get_content();
    size_t idx = buffer.get_cursor();
    while (idx > 0 && c[idx - 1] != '\n')
      idx--;
    buffer.move_gap(idx);
  }

  void move_line_end() {
    std::string c = buffer.get_content();
    size_t idx = buffer.get_cursor();
    while (idx < c.size() && c[idx] != '\n')
      idx++;
    buffer.move_gap(idx);
  }

  void kill_to_eol() {
    size_t start = buffer.get_cursor();
    std::string c = buffer.get_content();
    size_t end = start;
    while (end < c.size() && c[end] != '\n')
      end++;
    if (start == end && end < c.size())
      end++;
    if (end > start) {
      clipboard = buffer.get_range(start, end);
      buffer.delete_range(start, end);
      status_message = "Killed line";
    }
  }

  void kill_word() {
    size_t start = buffer.get_cursor();
    move_word_forward();
    size_t end = buffer.get_cursor();
    if (end > start) {
      clipboard = buffer.get_range(start, end);
      buffer.delete_range(start, end);
      status_message = "Killed word";
    }
  }

  void transpose_chars() {
    size_t idx = buffer.get_cursor();
    if (idx == 0 || buffer.size() < 2)
      return;
    std::string c = buffer.get_content();
    if (idx >= c.size())
      idx--;
    if (idx > 0) {
      char a = c[idx - 1];
      char b = c[idx];
      buffer.delete_range(idx - 1, idx + 1);
      buffer.move_gap(idx - 1);
      buffer.insert_char(b);
      buffer.insert_char(a);
    }
  }

  void transpose_words() {
    size_t orig = buffer.get_cursor();
    move_word_backward();
    move_word_forward();
    status_message = "Transpose words not fully impl";
    buffer.move_gap(orig);
  }

  void recenter_view() {
    EditorCursor cur = get_visual_cursor();
    scroll_row = std::max(0, cur.r - window_rows / 2);
  }

  void perform_indent(bool forward) {
    if (selection_anchor != std::string::npos) {
      status_message = "Block indent todo";
    } else {
      if (forward) {
        for (int i = 0; i < settings.tab_width; ++i)
          buffer.insert_char(' ');
      } else {
      }
    }
  }
};
} // namespace honeymoon::kernel
