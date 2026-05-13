#include "concepts.hpp"
#include "history.hpp"
#include "input.hpp"
#include "keybinder.hpp"
#include "logo.hpp"
#include "treesitter.hpp"
#include <algorithm>
#include <cctype>
#include <concepts>
#include <expected>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <ranges>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace honeymoon::kernel {

struct HomeState {
  int selection = 0;
};
struct EditorState {
  std::string filename;
};
struct FileSearchState {
  std::string query;
};
struct TextSearchState {
  std::string query;
  size_t start_idx;
  bool forward;
};
struct GotoLineState {
  std::string query;
};
struct RecentFilesState {
  int selection = 0;
};
struct SettingsState {
  int selection = 0;
};
struct HelpState {};
struct AboutState {};

using EditorMode =
    std::variant<HomeState, EditorState, FileSearchState, TextSearchState,
                 GotoLineState, RecentFilesState, SettingsState, HelpState,
                 AboutState>;

template <typename BufferPolicy, typename TerminalPolicy>
  requires EditableBuffer<BufferPolicy> && TerminalDevice<TerminalPolicy>
class Editor {
public:
  Editor() : output_buffer("") {
    update_window_size();
    bind_default_keys();
    std::stringstream ss{std::string(honeymoon::STARTUP_LOGO)};
    std::string line;
    while (std::getline(ss, line)) {
      logo_lines.push_back(line);
    }
    if (!logo_lines.empty() && logo_lines[0].empty())
      logo_lines.erase(logo_lines.begin());
    if (!logo_lines.empty() && logo_lines.back().empty())
      logo_lines.pop_back();

    recent_files = honeymoon::util::load_history(".honeymoon_history");
    mode = HomeState{};
    status_message = "Welcome to Honeymoon";
  }

  void open(const std::string &filename) {
    current_filename = filename;
    buffer.load_from_file(filename);
    status_message = "Opened " + filename;
    mode = EditorState{filename};
    syntax_engine.set_language_for_file(filename);
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
  EditorMode mode;
  std::vector<std::string> recent_files;
  honeymoon::syntax::TreeSitterHighlighter syntax_engine;

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
      if (std::holds_alternative<GotoLineState>(mode)) {
        mode = EditorState{current_filename};
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
      mode = TextSearchState{
          .query = "", .start_idx = buffer.get_cursor(), .forward = true};
      status_message = "I-Search: ";
    };
    actions["search_backward"] = [this]() {
      mode = TextSearchState{
          .query = "", .start_idx = buffer.get_cursor(), .forward = false};
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
      mode = GotoLineState{.query = ""};
      status_message = "Go to line: ";
    };

    actions["find_file"] = [this]() {
      mode = FileSearchState{.query = ""};
      status_message = "Find File: ";
    };
    actions["list_buffers"] = [this]() {
      mode = RecentFilesState{.selection = 0};
    };
    actions["kill_buffer"] = [this]() {
      current_filename = "[No Name]";
      buffer = BufferPolicy();
      mode = HomeState{};
      status_message = "Buffer Closed";
    };
    actions["select_all"] = [this]() {
      selection_anchor = 0;
      buffer.move_gap(buffer.size());
      status_message = "Select All";
    };
    actions["help_key"] = [this]() {
      mode = HelpState{};
      status_message = "Help: Describe Key";
    };
    actions["help_func"] = [this]() {
      mode = HelpState{};
      status_message = "Help: Describe Function";
    };

    root_node = std::make_shared<KeyNode>();
    current_node = root_node;

    // Load all keybindings from keybinds.moon

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

    auto render_visitor = [this](auto &state) {
      using T = std::decay_t<decltype(state)>;
      if constexpr (std::is_same_v<T, EditorState> ||
                    std::is_same_v<T, TextSearchState> ||
                    std::is_same_v<T, GotoLineState>) {
        output_buffer.append("\x1b[?25l\x1b[H");
        draw_rows();
        draw_status_bar();
        draw_message_bar();
        place_cursor();
      } else {
        output_buffer.append("\x1b[?25l\x1b[2J\x1b[H");
        draw_centered_view();
      }
    };

    std::visit(render_visitor, mode);
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
    std::visit(
        [this, menu_start_y, logo_start_y](auto &state) {
          draw_state_ui(state, menu_start_y, logo_start_y);
        },
        mode);
  }

  void draw_state_ui(HomeState &state, int y, int) {
    for (int i = 0; i < (int)home_menu.size(); ++i) {
      if (i == state.selection)
        output_buffer.append("\x1b[7m");
      draw_centered_text(y + i, home_menu[i]);
      if (i == state.selection)
        output_buffer.append("\x1b[m");
    }
  }

  void draw_state_ui(AboutState &, int y, int) {
    draw_centered_text(y, "Honeymoon Editor v0.1");
    draw_centered_text(y + 2, "A minimal C++ editor.");
    draw_centered_text(y + 4, "Made by Muffin");
    draw_centered_text(y + 6, "Press Esc to return.");
  }

  void draw_state_ui(HelpState &, int y, int) {
    draw_centered_text(y, "Keys:");
    draw_centered_text(y + 1, "C-x C-c: Quit");
    draw_centered_text(y + 2, "C-x C-s: Save");
    draw_centered_text(y + 4, "Press Esc to return.");
  }

  void draw_state_ui(SettingsState &state, int y, int logo_y) {
    draw_centered_text(logo_y, "SETTINGS");
    for (int i = 0; i < (int)settings_menu.size(); ++i) {
      std::string label = settings_menu[i];
      std::string val;
      if (label == "Line Numbers")
        val = settings.show_line_numbers ? " [ON] " : " [OFF]";
      else if (label == "Syntax Highlighting")
        val = settings.syntax_highlighting ? " [ON] " : " [OFF]";
      else if (label == "Tab Width")
        val = " [" + std::to_string(settings.tab_width) + "] ";

      if (i == state.selection)
        output_buffer.append("\x1b[7m");
      draw_centered_text(y + i, label + val);
      if (i == state.selection)
        output_buffer.append("\x1b[m");
    }
  }

  void draw_state_ui(RecentFilesState &state, int y, int) {
    if (recent_files.empty()) {
      draw_centered_text(y, "No recent files.");
    } else {
      for (int i = 0; i < (int)recent_files.size(); ++i) {
        if (i == state.selection)
          output_buffer.append("\x1b[7m");
        draw_centered_text(y + i, recent_files[i]);
        if (i == state.selection)
          output_buffer.append("\x1b[m");
      }
    }
  }

  void draw_state_ui(FileSearchState &state, int y, int) {
    draw_centered_text(y, "Search File: " + state.query);
  }

  template <typename T> void draw_state_ui(T &, int, int) {}

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

  const char *color_for_tree_sitter(honeymoon::syntax::HighlightKind kind) {
    using honeymoon::syntax::HighlightKind;
    switch (kind) {
    case HighlightKind::Comment:
      return "\x1b[90m";
    case HighlightKind::String:
      return "\x1b[32m";
    case HighlightKind::Number:
      return "\x1b[36m";
    case HighlightKind::Keyword:
      return "\x1b[35m";
    case HighlightKind::Type:
      return "\x1b[34m";
    case HighlightKind::Function:
      return "\x1b[33m";
    case HighlightKind::Preprocessor:
      return "\x1b[95m";
    case HighlightKind::None:
      return nullptr;
    }
    return nullptr;
  }

  void draw_rows() {
    std::string content = buffer.get_content();
    bool using_tree_sitter = false;
    if (settings.syntax_highlighting &&
        syntax_engine.set_language_for_file(current_filename)) {
      syntax_engine.update(content);
      using_tree_sitter = syntax_engine.active();
    }
    auto lines_view = content | std::views::split('\n');
    auto cur = get_visual_cursor();

    // Vertical scroll
    if (cur.r < (int)scroll_row)
      scroll_row = cur.r;
    if (cur.r >= (int)scroll_row + window_rows)
      scroll_row = cur.r - window_rows + 1;

    // Horizontal scroll (account for line-number gutter: 5 chars)
    int visible_cols = window_cols - 5;
    if (visible_cols < 1) visible_cols = 1;
    if (cur.c < (int)scroll_col)
      scroll_col = cur.c;
    if (cur.c >= (int)scroll_col + visible_cols)
      scroll_col = cur.c - visible_cols + 1;

    // Precompute absolute byte offset for each line (O(n) once)
    std::vector<size_t> line_offsets;
    line_offsets.push_back(0);
    for (size_t i = 0; i < content.size(); ++i) {
      if (content[i] == '\n')
        line_offsets.push_back(i + 1);
    }

    int y = 0;
    for (auto line : lines_view | std::views::drop(scroll_row) |
                         std::views::take(window_rows)) {
      size_t file_row = y + scroll_row;

      if (settings.show_line_numbers)
        output_buffer.append(
            std::format("\x1b[36m{:4} \x1b[39m", file_row + 1));
      else
        output_buffer.append(" ");

      std::string_view line_view(line.data(), line.size());
      size_t line_start_abs = (file_row < line_offsets.size())
                                  ? line_offsets[file_row]
                                  : 0;

      // Apply horizontal scroll
      if (scroll_col < line_view.size())
        line_view = line_view.substr(scroll_col);
      else
        line_view = line_view.substr(0, 0);
      if (line_view.size() > (size_t)visible_cols)
        line_view = line_view.substr(0, visible_cols);

      for (size_t i = 0; i < line_view.size(); ++i) {
        size_t abs = line_start_abs + scroll_col + i;
        bool sel = (selection_anchor != std::string::npos &&
                    abs >= std::min(selection_anchor, buffer.get_cursor()) &&
                    abs < std::max(selection_anchor, buffer.get_cursor()));
        if (sel)
          output_buffer.append("\x1b[7m");

        char c = line_view[i];
        bool had_syntax_style = false;
        if (settings.syntax_highlighting) {
          if (!sel && using_tree_sitter) {
            auto kind = syntax_engine.kind_at(abs);
            if (const char *style = color_for_tree_sitter(kind)) {
              output_buffer.append(style);
              had_syntax_style = true;
            }
          } else if (!sel) {
            if (std::isdigit(static_cast<unsigned char>(c))) {
              output_buffer.append("\x1b[36m");
              had_syntax_style = true;
            } else if (c == '"') {
              output_buffer.append("\x1b[32m");
              had_syntax_style = true;
            }
          }
        }

        output_buffer.append(1, c);
        if (sel || had_syntax_style)
          output_buffer.append("\x1b[m");
      }

      output_buffer.append("\x1b[K\r\n");
      y++;
    }

    // Fill remaining rows with ~
    for (; y < window_rows; y++) {
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
      output_buffer.append("\x1b[K\r\n");
    }
  }

  void draw_status_bar() {
    std::string stat = std::format("File: {}{} ", current_filename,
                                    buffer.is_dirty() ? " [+]" : "");
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
    int r = cur.r - scroll_row, c = cur.c - (int)scroll_col;
    if (r < 0)
      r = 0;
    if (r >= window_rows)
      r = window_rows - 1;
    int gutter = settings.show_line_numbers ? 5 : 1;
    output_buffer.append(std::format("\x1b[{};{}H", r + 1, c + 1 + gutter));
  }

  void process_keypress() {
    Key k = terminal.read_key();
    if (k == Key::None)
      return;

    std::visit([this, k](auto &state) { this->handle_input(state, k); }, mode);
  }

  void handle_input(EditorState &, Key k) {
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

  void handle_input(TextSearchState &state, Key k) {
    if (k == Key::Enter || k == Key::Esc) {
      mode = EditorState{current_filename};
      status_message = "";
      return;
    }
    if (k == Key::Ctrl_G) {
      mode = EditorState{current_filename};
      buffer.move_gap(state.start_idx);
      status_message = "Cancelled";
      return;
    }

    bool next = false;
    if (k == Key::Backspace || k == Key::Ctrl_H) {
      if (!state.query.empty())
        state.query.pop_back();
    } else if (k == Key::Ctrl_S) {
      state.forward = true;
      next = true;
    } else if (k == Key::Ctrl_R) {
      state.forward = false;
      next = true;
    } else if (is_printable((int)k)) {
      state.query.push_back((char)k);
    }

    std::string c = buffer.get_content();
    size_t start_pos = buffer.get_cursor();
    if (next)
      start_pos =
          (state.forward) ? start_pos + 1 : (start_pos > 0 ? start_pos - 1 : 0);

    size_t found = std::string::npos;
    if (state.forward)
      found = c.find(state.query, start_pos);
    else
      found = c.rfind(state.query, start_pos);

    if (found != std::string::npos) {
      buffer.move_gap(found);
      status_message = "I-Search: " + state.query;
    } else {
      status_message = "Failing I-Search: " + state.query;
    }
  }

  void handle_input(HomeState &state, Key k) {
    if (k == Key::Esc) {
      mode = EditorState{current_filename}; // Escapes from home ehe
      return;
    }

    if (k == Key::ArrowUp || k == Key::Ctrl_P) {
      state.selection--;
      if (state.selection < 0)
        state.selection = home_menu.size() - 1;
    } else if (k == Key::ArrowDown || k == Key::Ctrl_N) {
      state.selection++;
      if (state.selection >= (int)home_menu.size())
        state.selection = 0;
    } else if (k == Key::Enter) {
      std::string sel = home_menu[state.selection];
      if (sel == "Quit")
        should_quit = true;
      else if (sel == "About")
        mode = AboutState{};
      else if (sel == "Help")
        mode = HelpState{};
      else if (sel == "Settings")
        mode = SettingsState{};
      else if (sel == "Recent Files")
        mode = RecentFilesState{};
      else if (sel == "File Searcher")
        mode = FileSearchState{};
    }
  }

  void handle_input(RecentFilesState &state, Key k) {
    if (k == Key::Esc) {
      mode = HomeState{};
      return;
    }
    if (!recent_files.empty()) {
      if (k == Key::ArrowUp) {
        state.selection--;
        if (state.selection < 0)
          state.selection = recent_files.size() - 1;
      } else if (k == Key::ArrowDown) {
        state.selection++;
        if (state.selection >= (int)recent_files.size())
          state.selection = 0;
      } else if (k == Key::Enter) {
        open(recent_files[state.selection]);
      }
    }
  }

  void handle_input(FileSearchState &state, Key k) {
    if (k == Key::Esc) {
      mode = HomeState{};
      return;
    }
    if (k == Key::Enter) {
      if (!state.query.empty())
        open(state.query);
    } else if (k == Key::Backspace || k == Key::Ctrl_H) {
      if (!state.query.empty())
        state.query.pop_back();
    } else if (is_printable((int)k))
      state.query.push_back((char)k);
  }

  void handle_input(GotoLineState &state, Key k) {
    if (k == Key::Esc || k == Key::Ctrl_G) {
      mode = EditorState{current_filename};
      status_message = "Cancelled";
      return;
    }
    if (k == Key::Enter) {
      if (!state.query.empty()) {
        try {
          int line_num = std::stoi(state.query);
          std::string c = buffer.get_content();
          size_t idx = 0;
          int current_line = 1;
          while (idx < c.size() && current_line < line_num) {
            if (c[idx] == '\n')
              current_line++;
            idx++;
          }
          buffer.move_gap(idx);
          status_message = "Jumped to line " + state.query;
        } catch (...) {
          status_message = "Invalid number";
        }
      }
      mode = EditorState{current_filename};
    } else if (k == Key::Backspace || k == Key::Ctrl_H) {
      if (!state.query.empty())
        state.query.pop_back();
    } else if (isdigit((char)k))
      state.query.push_back((char)k);
  }

  void handle_input(SettingsState &, Key k) {
    if (k == Key::Esc) {
      mode = HomeState{};
      return;
    }
    // will add more settings here , currently thinking of ways to get the
    // template heavy code or split it across some files
  }

  void handle_input(HelpState &, Key k) {
    if (k == Key::Esc)
      mode = HomeState{};
  }
  void handle_input(AboutState &, Key k) {
    if (k == Key::Esc)
      mode = HomeState{};
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
    std::string c = buffer.get_content();
    size_t cur = buffer.get_cursor();

    // Find word-2 end: scan backward from cursor past separator(s) then word
    size_t word2_end = cur;
    while (word2_end > 0 && is_separator(c[word2_end - 1]))
      word2_end--;
    size_t word2_start = word2_end;
    while (word2_start > 0 && !is_separator(c[word2_start - 1]))
      word2_start--;

    // Find word-1: scan backward from word-2 past separators
    size_t word1_end = word2_start;
    while (word1_end > 0 && is_separator(c[word1_end - 1]))
      word1_end--;
    size_t word1_start = word1_end;
    while (word1_start > 0 && !is_separator(c[word1_start - 1]))
      word1_start--;

    if (word1_start >= word2_start || word1_start == word1_end ||
        word2_start == word2_end) {
      status_message = "Not enough words";
      return;
    }

    // Extract the three segments
    std::string word1 = buffer.get_range(word1_start, word1_end);
    std::string sep = buffer.get_range(word1_end, word2_start);
    std::string word2 = buffer.get_range(word2_start, word2_end);

    // Delete both words + separator, re-insert swapped
    buffer.delete_range(word1_start, word2_end);
    buffer.move_gap(word1_start);
    buffer.insert_string(word2);
    buffer.insert_string(sep);
    buffer.insert_string(word1);

    // Place cursor after the transposed pair
    buffer.move_gap(word1_start + word2.size() + sep.size() + word1.size());

    status_message = "Transposed words";
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
        // Dedent: remove up to tab_width spaces from line start
        std::string c = buffer.get_content();
        size_t cur = buffer.get_cursor();
        size_t line_start = cur;
        while (line_start > 0 && c[line_start - 1] != '\n')
          line_start--;

        size_t spaces = 0;
        while (line_start + spaces < c.size() &&
               c[line_start + spaces] == ' ' &&
               (int)spaces < settings.tab_width)
          spaces++;

        if (spaces > 0) {
          buffer.delete_range(line_start, line_start + spaces);
          size_t cursor_adj = std::min(spaces, cur - line_start);
          buffer.move_gap(cur - cursor_adj);
          status_message = "Dedented";
        } else {
          status_message = "No indent to remove";
        }
      }
    }
  }
};
} // namespace honeymoon::kernel
