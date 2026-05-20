#include "concepts.hpp"
#include "config.hpp"
#include "undo.hpp"
#include "history.hpp"
#include "input.hpp"
#include "keybinder.hpp"
#include "logo.hpp"
#include "treesitter.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <concepts>
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
class Editor : private honeymoon::config::Config,
               private honeymoon::mem::UndoHistory<> {
public:
  Editor() {
    update_window_size();
    bind_default_keys();
    load();
    std::string logo_str(honeymoon::STARTUP_LOGO);
    size_t lpos = 0;
    while (lpos < logo_str.size()) {
      size_t lend = logo_str.find('\n', lpos);
      if (lend == std::string::npos) lend = logo_str.size();
      logo_lines.push_back(logo_str.substr(lpos, lend - lpos));
      lpos = lend + 1;
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
  struct RenderBuf {
    char data[4096];
    size_t len = 0;
    void clear() { len = 0; }
    RenderBuf& append(const char* s) { while (*s && len < sizeof(data)) data[len++] = *s++; return *this; }
    RenderBuf& append(char c) { if (len < sizeof(data)) data[len++] = c; return *this; }
    RenderBuf& append(int n, char c) { if (n <= 0) return *this; size_t avail = sizeof(data) - len; size_t m = (size_t)n < avail ? (size_t)n : avail; for (size_t i = 0; i < m; i++) data[len++] = c; return *this; }
    RenderBuf& append(const char* s, size_t n) { size_t m = n < sizeof(data) - len ? n : sizeof(data) - len; memcpy(data + len, s, m); len += m; return *this; }
    RenderBuf& append(const std::string& s) { append(s.data(), s.size()); return *this; }
  } output_buffer;
  char* clipboard = nullptr;
  size_t clipboard_len = 0;
  size_t scroll_row = 0, scroll_col = 0;
  int window_rows = 0, window_cols = 0;
  size_t selection_anchor = std::string::npos;
  std::vector<std::string> logo_lines;
  EditorMode mode;
  std::vector<std::string> recent_files;
  honeymoon::syntax::TreeSitterHighlighter syntax_engine;

  enum ActionId : uint8_t {
    ACT_NONE = 0,
    ACT_QUIT, ACT_SAVE_FILE, ACT_MARK_SET, ACT_CANCEL, ACT_CUT, ACT_YANK,
    ACT_MOVE_LINE_START, ACT_MOVE_LINE_END, ACT_KILL_LINE, ACT_RECENTER,
    ACT_TRANSPOSE_CHARS, ACT_NEWLINE, ACT_SEARCH_FORWARD, ACT_SEARCH_BACKWARD,
    ACT_INDENT, ACT_DEDENT, ACT_DELETE_BACKWARD, ACT_DELETE_FORWARD,
    ACT_MOVE_UP, ACT_MOVE_DOWN, ACT_MOVE_LEFT, ACT_MOVE_RIGHT,
    ACT_UNDO, ACT_REDO, ACT_COPY, ACT_MOVE_WORD_BACKWARD, ACT_MOVE_WORD_FORWARD,
    ACT_KILL_WORD, ACT_TRANSPOSE_WORDS, ACT_GOTO_LINE, ACT_FIND_FILE,
    ACT_LIST_BUFFERS, ACT_KILL_BUFFER, ACT_SELECT_ALL, ACT_HELP_KEY, ACT_HELP_FUNC,
  };

  struct ActionEntry { const char* name; ActionId id; };
  static constexpr ActionEntry action_table[] = {
    {"quit", ACT_QUIT}, {"save_file", ACT_SAVE_FILE},
    {"mark_set", ACT_MARK_SET}, {"cancel", ACT_CANCEL},
    {"cut", ACT_CUT}, {"yank", ACT_YANK},
    {"move_line_start", ACT_MOVE_LINE_START}, {"move_line_end", ACT_MOVE_LINE_END},
    {"kill_line", ACT_KILL_LINE}, {"recenter", ACT_RECENTER},
    {"transpose_chars", ACT_TRANSPOSE_CHARS}, {"newline", ACT_NEWLINE},
    {"search_forward", ACT_SEARCH_FORWARD}, {"search_backward", ACT_SEARCH_BACKWARD},
    {"indent", ACT_INDENT}, {"dedent", ACT_DEDENT},
    {"delete_backward", ACT_DELETE_BACKWARD}, {"delete_forward", ACT_DELETE_FORWARD},
    {"move_up", ACT_MOVE_UP}, {"move_down", ACT_MOVE_DOWN},
    {"move_left", ACT_MOVE_LEFT}, {"move_right", ACT_MOVE_RIGHT},
    {"undo", ACT_UNDO}, {"redo", ACT_REDO},
    {"copy", ACT_COPY}, {"move_word_backward", ACT_MOVE_WORD_BACKWARD},
    {"move_word_forward", ACT_MOVE_WORD_FORWARD}, {"kill_word", ACT_KILL_WORD},
    {"transpose_words", ACT_TRANSPOSE_WORDS}, {"goto_line", ACT_GOTO_LINE},
    {"find_file", ACT_FIND_FILE}, {"list_buffers", ACT_LIST_BUFFERS},
    {"kill_buffer", ACT_KILL_BUFFER}, {"select_all", ACT_SELECT_ALL},
    {"help_key", ACT_HELP_KEY}, {"help_func", ACT_HELP_FUNC},
  };

  static ActionId lookup_action(const std::string& name) {
    for (auto& e : action_table) {
      if (name == e.name) return e.id;
    }
    return ACT_NONE;
  }

  void set_clipboard(const std::string& s) {
    free(clipboard);
    clipboard_len = s.size();
    clipboard = (char*)malloc(clipboard_len + 1);
    if (clipboard) { memcpy(clipboard, s.data(), clipboard_len); clipboard[clipboard_len] = '\0'; }
  }

  void set_clipboard(const char* s, size_t n) {
    free(clipboard);
    clipboard_len = n;
    clipboard = (char*)malloc(n + 1);
    if (clipboard) { memcpy(clipboard, s, n); clipboard[n] = '\0'; }
  }

  void execute_action(ActionId id) {
    switch (id) {
      case ACT_QUIT: should_quit = true; break;
      case ACT_SAVE_FILE: buffer.save_to_file(current_filename); status_message = "Saved"; break;
      case ACT_MARK_SET: selection_anchor = buffer.get_cursor(); status_message = "Mark Set"; break;
      case ACT_CANCEL: {
        if (std::holds_alternative<GotoLineState>(mode)) {
          mode = EditorState{current_filename}; status_message = "Cancelled";
        } else {
          selection_anchor = std::string::npos; current_node = root_node; pending_key_count = 0; status_message = "Quit";
        } break;
      }
      case ACT_CUT: {
        if (selection_anchor != std::string::npos) {
          snapshot_for_undo(buffer.get_content(), buffer.get_cursor());
          size_t c = buffer.get_cursor(); set_clipboard(buffer.get_range(selection_anchor, c));
          buffer.delete_range(selection_anchor, c); selection_anchor = std::string::npos; status_message = "Cut";
        } else status_message = "No selection";
        break;
      }
      case ACT_YANK: {
        if (clipboard && clipboard_len) { snapshot_for_undo(buffer.get_content(), buffer.get_cursor()); buffer.insert_string(clipboard, clipboard_len); status_message = "Yank"; }
        else { status_message = "Empty"; }
        break;
      }
      case ACT_MOVE_LINE_START: move_line_start(); break;
      case ACT_MOVE_LINE_END: move_line_end(); break;
      case ACT_KILL_LINE: kill_to_eol(); break;
      case ACT_RECENTER: recenter_view(); break;
      case ACT_TRANSPOSE_CHARS: transpose_chars(); break;
      case ACT_NEWLINE: snapshot_for_undo(buffer.get_content(), buffer.get_cursor()); buffer.insert_char('\n'); break;
      case ACT_SEARCH_FORWARD: mode = TextSearchState{.query = "", .start_idx = buffer.get_cursor(), .forward = true}; status_message = "I-Search: "; break;
      case ACT_SEARCH_BACKWARD: mode = TextSearchState{.query = "", .start_idx = buffer.get_cursor(), .forward = false}; status_message = "I-Search Back: "; break;
      case ACT_INDENT: perform_indent(true); break;
      case ACT_DEDENT: perform_indent(false); break;
      case ACT_DELETE_BACKWARD: snapshot_for_undo(buffer.get_content(), buffer.get_cursor()); buffer.delete_char(); break;
      case ACT_DELETE_FORWARD: snapshot_for_undo(buffer.get_content(), buffer.get_cursor()); buffer.delete_forward(); break;
      case ACT_MOVE_UP: move_cursor_2d(-1, 0); break;
      case ACT_MOVE_DOWN: move_cursor_2d(1, 0); break;
      case ACT_MOVE_LEFT: move_cursor_lin(-1); break;
      case ACT_MOVE_RIGHT: move_cursor_lin(1); break;
      case ACT_UNDO: {
        std::string cur = buffer.get_content(); size_t cur_cursor = buffer.get_cursor();
        auto result = apply_undo(cur, cur_cursor);
        if (result) { buffer.delete_range(0, buffer.size()); buffer.move_gap(0); buffer.insert_string(result->content); buffer.move_gap(result->cursor); status_message = "Undo"; }
        else { status_message = "Nothing to undo"; }
        break;
      }
      case ACT_REDO: {
        std::string cur = buffer.get_content(); size_t cur_cursor = buffer.get_cursor();
        auto result = apply_redo(cur, cur_cursor);
        if (result) { buffer.delete_range(0, buffer.size()); buffer.move_gap(0); buffer.insert_string(result->content); buffer.move_gap(result->cursor); status_message = "Redo"; }
        else { status_message = "Nothing to redo"; }
        break;
      }
      case ACT_COPY: {
        if (selection_anchor != std::string::npos) { set_clipboard(buffer.get_range(selection_anchor, buffer.get_cursor())); selection_anchor = std::string::npos; status_message = "Copy"; }
        else { status_message = "No selection"; }
        break;
      }
      case ACT_MOVE_WORD_BACKWARD: move_word_backward(); break;
      case ACT_MOVE_WORD_FORWARD: move_word_forward(); break;
      case ACT_KILL_WORD: kill_word(); break;
      case ACT_TRANSPOSE_WORDS: transpose_words(); break;
      case ACT_GOTO_LINE: mode = GotoLineState{.query = ""}; status_message = "Go to line: "; break;
      case ACT_FIND_FILE: mode = FileSearchState{.query = ""}; status_message = "Find File: "; break;
      case ACT_LIST_BUFFERS: mode = RecentFilesState{.selection = 0}; break;
      case ACT_KILL_BUFFER: current_filename = "[No Name]"; buffer = BufferPolicy(); mode = HomeState{}; status_message = "Buffer Closed"; break;
      case ACT_SELECT_ALL: selection_anchor = 0; buffer.move_gap(buffer.size()); status_message = "Select All"; break;
      case ACT_HELP_KEY: mode = HelpState{}; status_message = "Help: Describe Key"; break;
      case ACT_HELP_FUNC: mode = HelpState{}; status_message = "Help: Describe Function"; break;
      default: break;
    }
  }

  struct KeyNode {
    Key key;
    KeyNode* next_sibling = nullptr;
    KeyNode* first_child = nullptr;
    std::string action;
  };

  KeyNode* root_node = nullptr;
  KeyNode* current_node = nullptr;
  Key pending_keys[16];
  int pending_key_count = 0;

public:
  ~Editor() { delete_key_tree(root_node); free(clipboard); }
private:

  void delete_key_tree(KeyNode* n) {
    if (!n) return;
    delete_key_tree(n->first_child);
    delete_key_tree(n->next_sibling);
    delete n;
  }

  KeyNode* find_child(KeyNode* parent, Key k) {
    for (KeyNode* c = parent->first_child; c; c = c->next_sibling) {
      if (c->key == k) return c;
    }
    return nullptr;
  }

  KeyNode* add_child(KeyNode* parent, Key k) {
    KeyNode* c = new KeyNode;
    c->key = k;
    c->next_sibling = parent->first_child;
    parent->first_child = c;
    return c;
  }

  void bind_default_keys() {
    root_node = new KeyNode;
    current_node = root_node;
    load_custom_binds();
  }

  void add_binding(const std::vector<Key> &keys, const std::string &action) {
    if (keys.empty()) return;
    KeyNode* node = root_node;
    for (const auto &k : keys) {
      KeyNode* child = find_child(node, k);
      if (!child) child = add_child(node, k);
      node = child;
    }
    node->action = action;
  }

  void load_custom_binds() {
    auto binds = honeymoon::config::KeyBinder::load_from_file("keybinds.moon");
    for (auto &b : binds) {
      add_binding(b.keys, b.action);
    }
  }


  static constexpr const char* home_menu[] = {
      "File Searcher", "Recent Files", "Settings", "Help", "About", "Quit"};
  static constexpr int home_menu_n = 6;
  static constexpr const char* settings_menu[] = {
      "Line Numbers", "Syntax Highlighting", "Tab Width", "Back"};
  static constexpr int settings_menu_n = 4;

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
    terminal.write_raw(output_buffer.data, output_buffer.len);
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
      output_buffer.append("\x1b[" + std::to_string(y + 1) + ";1H");
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
    for (int i = 0; i < home_menu_n; ++i) {
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
    for (int i = 0; i < settings_menu_n; ++i) {
      std::string label = settings_menu[i];
      std::string val;
      if (label == "Line Numbers")
        val = show_line_numbers ? " [ON] " : " [OFF]";
      else if (label == "Syntax Highlighting")
        val = syntax_highlighting ? " [ON] " : " [OFF]";
      else if (label == "Tab Width")
        val = " [" + std::to_string(tab_width) + "] ";

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
    output_buffer.append("\x1b[" + std::to_string(y + 1) + ";" +
                             std::to_string(pad + 1) + "H")
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

  static constexpr const char* color_for_tree_sitter(honeymoon::syntax::HighlightKind kind) {
    using honeymoon::syntax::HighlightKind;
    constexpr const char* table[] = {
      nullptr,         // None
      "\x1b[90m",     // Comment
      "\x1b[32m",     // String
      "\x1b[36m",     // Number
      "\x1b[35m",     // Keyword
      "\x1b[34m",     // Type
      "\x1b[33m",     // Function
      "\x1b[95m",     // Preprocessor
    };
    auto idx = static_cast<int>(kind);
    return (idx >= 0 && idx < 8) ? table[idx] : nullptr;
  }

  void draw_rows() {
    std::string content = buffer.get_content();
    bool using_tree_sitter = false;
    if (syntax_highlighting &&
        syntax_engine.set_language_for_file(current_filename)) {
      syntax_engine.update(content);
      using_tree_sitter = syntax_engine.active();
    }
    auto cur = get_visual_cursor();


    if (cur.r < (int)scroll_row)
      scroll_row = cur.r;
    if (cur.r >= (int)scroll_row + window_rows)
      scroll_row = cur.r - window_rows + 1;


    int visible_cols = window_cols - 5;
    if (visible_cols < 1) visible_cols = 1;
    if (cur.c < (int)scroll_col)
      scroll_col = cur.c;
    if (cur.c >= (int)scroll_col + visible_cols)
      scroll_col = cur.c - visible_cols + 1;


    std::vector<size_t> line_offsets;
    line_offsets.push_back(0);
    for (size_t i = 0; i < content.size(); ++i) {
      if (content[i] == '\n')
        line_offsets.push_back(i + 1);
    }

    int y = 0;
    size_t line_pos = 0;
    for (size_t i = 0; i < scroll_row && line_pos < content.size(); ++i) {
      size_t nl = content.find('\n', line_pos);
      if (nl == std::string::npos) { line_pos = content.size(); break; }
      line_pos = nl + 1;
    }
    for (; y < window_rows && line_pos <= content.size(); ++y) {
      size_t nl = content.find('\n', line_pos);
      size_t line_end = (nl == std::string::npos) ? content.size() : nl;
      size_t file_row = y + scroll_row;
      std::string_view line_view(content.data() + line_pos, line_end - line_pos);
      line_pos = (nl == std::string::npos) ? content.size() + 1 : nl + 1;

      if (show_line_numbers)
        {
          auto n = std::to_string(file_row + 1);
          output_buffer.append("\x1b[36m" + std::string(4 - n.size(), ' ') +
                               n + " \x1b[39m");
        }
      else
        output_buffer.append(" ");

      size_t line_start_abs = (file_row < line_offsets.size())
                                  ? line_offsets[file_row]
                                  : 0;


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
        if (syntax_highlighting) {
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
    }


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
    std::string stat = "File: " + current_filename +
                       (buffer.is_dirty() ? " [+]" : "") + " ";
    std::string rstat = std::to_string(get_visual_cursor().r + 1) + "/" +
                        std::to_string(buffer.size());
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
    int gutter = show_line_numbers ? 5 : 1;
    output_buffer.append("\x1b[" + std::to_string(r + 1) + ";" +
                         std::to_string(c + 1 + gutter) + "H");
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

    KeyNode* next = find_child(current_node, k);
    if (next) {
      current_node = next;
      if (pending_key_count < 16) pending_keys[pending_key_count++] = k;

      if (!current_node->action.empty() && !current_node->first_child) {
        ActionId aid = lookup_action(current_node->action);
        current_node = root_node;
        pending_key_count = 0;
        status_message = "";
        close_typing_group();
        if (aid != ACT_NONE)
          execute_action(aid);
        else
          status_message = std::string("Action not found: ") + current_node->action;
      } else {
        std::string msg;
        for (int i = 0; i < pending_key_count; ++i) {
          if (i) msg += ' ';
          msg += to_string(pending_keys[i]);
        }
        status_message = msg;
      }
    } else {
      if (current_node != root_node) {
        status_message = "Undefined Key";
        current_node = root_node;
        pending_key_count = 0;
      } else {
        if (is_printable((int)k) && k != Key::Esc) {
          snapshot_for_undo(buffer.get_content(), buffer.get_cursor());
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
      mode = EditorState{current_filename};
      return;
    }

    if (k == Key::ArrowUp || k == Key::Ctrl_P) {
      state.selection--;
      if (state.selection < 0)
        state.selection = home_menu_n - 1;
    } else if (k == Key::ArrowDown || k == Key::Ctrl_N) {
      state.selection++;
      if (state.selection >= home_menu_n)
        state.selection = 0;
    } else if (k == Key::Enter) {
      switch (state.selection) {
        case 0: mode = FileSearchState{}; break;
        case 1: mode = RecentFilesState{}; break;
        case 2: mode = SettingsState{}; break;
        case 3: mode = HelpState{}; break;
        case 4: mode = AboutState{}; break;
        case 5: should_quit = true; break;
      }
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
        char* end = nullptr;
        long line_num = strtol(state.query.c_str(), &end, 10);
        if (end == state.query.c_str() || *end != '\0') {
          status_message = "Invalid number";
        } else {
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
        }
      }
      mode = EditorState{current_filename};
    } else if (k == Key::Backspace || k == Key::Ctrl_H) {
      if (!state.query.empty())
        state.query.pop_back();
    } else if (isdigit((char)k))
      state.query.push_back((char)k);
  }

  void handle_input(SettingsState &state, Key k) {
    if (k == Key::Esc) {
      mode = HomeState{};
      return;
    }
    if (k == Key::ArrowUp || k == Key::Ctrl_P) {
      state.selection--;
      if (state.selection < 0)
        state.selection = settings_menu_n - 1;
    } else if (k == Key::ArrowDown || k == Key::Ctrl_N) {
      state.selection++;
      if (state.selection >= settings_menu_n)
        state.selection = 0;
    } else if (k == Key::Enter) {
      std::string sel = settings_menu[state.selection];
      if (sel == "Line Numbers") {
        show_line_numbers = !show_line_numbers;
        save();
      } else if (sel == "Syntax Highlighting") {
        syntax_highlighting = !syntax_highlighting;
        save();
      } else if (sel == "Tab Width") {
        if (tab_width == 2)
          tab_width = 4;
        else if (tab_width == 4)
          tab_width = 8;
        else
          tab_width = 2;
        save();
      } else if (sel == "Back") {
        mode = HomeState{};
      }
    }
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
    snapshot_for_undo(buffer.get_content(), buffer.get_cursor());
    size_t start = buffer.get_cursor();
    std::string c = buffer.get_content();
    size_t end = start;
    while (end < c.size() && c[end] != '\n')
      end++;
    if (start == end && end < c.size())
      end++;
    if (end > start) {
      set_clipboard(buffer.get_range(start, end));
      buffer.delete_range(start, end);
      status_message = "Killed line";
    }
  }

  void kill_word() {
    snapshot_for_undo(buffer.get_content(), buffer.get_cursor());
    size_t start = buffer.get_cursor();
    move_word_forward();
    size_t end = buffer.get_cursor();
    if (end > start) {
      set_clipboard(buffer.get_range(start, end));
      buffer.delete_range(start, end);
      status_message = "Killed word";
    }
  }

  void transpose_chars() {
    snapshot_for_undo(buffer.get_content(), buffer.get_cursor());
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
    snapshot_for_undo(buffer.get_content(), buffer.get_cursor());
    std::string c = buffer.get_content();
    size_t cur = buffer.get_cursor();


    size_t word2_end = cur;
    while (word2_end > 0 && is_separator(c[word2_end - 1]))
      word2_end--;
    size_t word2_start = word2_end;
    while (word2_start > 0 && !is_separator(c[word2_start - 1]))
      word2_start--;


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


    std::string word1 = buffer.get_range(word1_start, word1_end);
    std::string sep = buffer.get_range(word1_end, word2_start);
    std::string word2 = buffer.get_range(word2_start, word2_end);


    buffer.delete_range(word1_start, word2_end);
    buffer.move_gap(word1_start);
    buffer.insert_string(word2);
    buffer.insert_string(sep);
    buffer.insert_string(word1);


    buffer.move_gap(word1_start + word2.size() + sep.size() + word1.size());

    status_message = "Transposed words";
  }

  void recenter_view() {
    EditorCursor cur = get_visual_cursor();
    scroll_row = std::max(0, cur.r - window_rows / 2);
  }

  void perform_indent(bool forward) {
    snapshot_for_undo(buffer.get_content(), buffer.get_cursor());
    if (selection_anchor != std::string::npos) {
      status_message = "Block indent todo";
    } else {
      if (forward) {
        for (int i = 0; i < tab_width; ++i)
          buffer.insert_char(' ');
      } else {

        std::string c = buffer.get_content();
        size_t cur = buffer.get_cursor();
        size_t line_start = cur;
        while (line_start > 0 && c[line_start - 1] != '\n')
          line_start--;

        size_t spaces = 0;
        while (line_start + spaces < c.size() &&
               c[line_start + spaces] == ' ' &&
               (int)spaces < tab_width)
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
}
