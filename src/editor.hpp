/*
 * The Kernel.
 * Policy-based design. 
 */
#pragma once
#include <string>
#include <vector>
#include <format>
#include <algorithm>
#include "concepts.hpp"
#include "input.hpp"

namespace honeymoon::kernel {
    template <typename BufferPolicy, typename TerminalPolicy>
    requires EditableBuffer<BufferPolicy> && TerminalDevice<TerminalPolicy>
    class Editor {
    public:
        Editor() : output_buffer("") { update_window_size(); }

        void open(const std::string& filename) {
            current_filename = filename;
            buffer.load_from_file(filename);
            status_message = "Opened " + filename;
        }

        void run() {
            while (!should_quit) { refresh_screen(); process_keypress(); }
            terminal.write_raw("\x1b[2J\x1b[H"); // Cleanup
        }

    private:
        TerminalPolicy terminal;
        BufferPolicy buffer;
        bool should_quit = false;
        std::string current_filename = "[No Name]";
        std::string status_message = "Honeymoon | C-x C-c: Quit | C-x C-s: Save | C-SP: Mark";
        std::string output_buffer;
        std::string clipboard;
        size_t scroll_row = 0, scroll_col = 0;
        int window_rows = 0, window_cols = 0;
        bool waiting_for_chord = false, waiting_for_meta = false;
        int last_ctrl_key = 0;
        size_t selection_anchor = std::string::npos; 

        void update_window_size() {
            auto [rows, cols] = terminal.get_window_size();
            window_rows = rows; window_cols = cols;
            if (window_rows > 2) window_rows -= 2;
        }
        
        void refresh_screen() {
            update_window_size();
            output_buffer.clear();
            output_buffer.append("\x1b[?25l\x1b[H"); 
            draw_rows(); draw_status_bar(); draw_message_bar(); place_cursor();
            output_buffer.append("\x1b[?25h");
            terminal.write_raw(output_buffer);
        }
        
        struct EditorCursor { size_t idx; int r; int c; };

        EditorCursor get_visual_cursor() {
            std::string content = buffer.get_content();
            size_t cursor = buffer.get_cursor();
            int r = 0, c = 0;
            for (size_t i = 0; i < cursor && i < content.size(); ++i) {
                if (content[i] == '\n') { r++; c = 0; } else { c++; }
            }
            return {cursor, r, c};
        }
        
        void draw_rows() {
            std::string content = buffer.get_content();
            std::vector<size_t> lines = {0};
            for (size_t i = 0; i < content.size(); ++i) if (content[i] == '\n') lines.push_back(i + 1);
            
            EditorCursor cur = get_visual_cursor();
            if (cur.r < (int)scroll_row) scroll_row = cur.r;
            if (cur.r >= (int)scroll_row + window_rows) scroll_row = cur.r - window_rows + 1;
            
            for (int y = 0; y < window_rows; y++) {
                size_t file_row = y + scroll_row;
                if (file_row < lines.size()) {
                    output_buffer.append(std::format("\x1b[36m{:4} \x1b[39m", file_row + 1));
                } else { output_buffer.append("     "); }

                if (file_row >= lines.size()) {
                    if (content.empty() && y == window_rows / 3) {
                         std::string msg = "Honeymoon Editor";
                         int pad = (window_cols - 5 - msg.length()) / 2;
                         if (pad > 0) output_buffer.append("~"); 
                         output_buffer.append(std::string(std::max(0, pad-1), ' ')).append(msg);
                    } else { output_buffer.append("~"); }
                } else {
                    size_t start = lines[file_row];
                    size_t len = (file_row + 1 < lines.size()) ? lines[file_row+1] - start - 1 : content.size() - start;
                    if (len > (size_t)(window_cols - 5)) len = window_cols - 5;
                    std::string_view line_view(content.data() + start, len);
                    
                    for(size_t i = 0; i < len; ++i) {
                        size_t abs = start + i;
                        bool sel = (selection_anchor != std::string::npos && abs >= std::min(selection_anchor, buffer.get_cursor()) && abs < std::max(selection_anchor, buffer.get_cursor()));
                        if (sel) output_buffer.append("\x1b[7m");
                        
                        char c = line_view[i];
                        if (isdigit(c) && !sel) output_buffer.append("\x1b[36m");
                        else if (c == '"' && !sel) output_buffer.append("\x1b[32m");
                        
                        output_buffer.append(1, c);
                        if (sel || isdigit(c) || c == '"') output_buffer.append("\x1b[m");
                    }
                }
                output_buffer.append("\x1b[K\r\n");
            }
        }

        void draw_status_bar() {
            std::string stat = std::format("File: {} {}", current_filename, buffer.is_dirty()?"[+]":"");
            std::string rstat = std::format("{}/{}", get_visual_cursor().r + 1, buffer.size());
            size_t len = stat.length(), rlen = rstat.length();
            if (len > (size_t)window_cols) len = window_cols;
            output_buffer.append("\x1b[7m").append(stat.substr(0, len));
            while (len < (size_t)window_cols) {
                if (window_cols - len == rlen) { output_buffer.append(rstat); break; }
                output_buffer.append(" "); len++;
            }
            output_buffer.append("\x1b[m\r\n");
        }

        void draw_message_bar() { output_buffer.append("\x1b[K").append(status_message); }
        
        void place_cursor() {
            EditorCursor cur = get_visual_cursor();
            int r = cur.r - scroll_row, c = cur.c - scroll_col;
            if (r < 0) r = 0; if (r >= window_rows) r = window_rows - 1;
            output_buffer.append(std::format("\x1b[{};{}H", r + 1, c + 1 + 5));
        }
        
        void process_keypress() {
            Key k = terminal.read_key();
            if (k == Key::None) return;
            if (k == Key::Esc) { waiting_for_meta = true; status_message = "M-"; return; }
            if (waiting_for_meta) { process_meta(k); return; }
            if (waiting_for_chord) { process_chord(k); return; }

            switch (k) {
                case Key::Ctrl_X: waiting_for_chord = true; last_ctrl_key = (int)k; status_message = "C-x"; break;
                case Key::Ctrl_Space: selection_anchor = buffer.get_cursor(); status_message = "Mark Set"; break;
                case Key::Ctrl_G: selection_anchor = std::string::npos; waiting_for_chord=false; waiting_for_meta=false; status_message="Quit"; break;
                case Key::Ctrl_W: 
                    if (selection_anchor != std::string::npos) {
                        size_t c = buffer.get_cursor(); clipboard = buffer.get_range(selection_anchor, c);
                        buffer.delete_range(selection_anchor, c); selection_anchor = std::string::npos; status_message = "Cut";
                    } else status_message = "No selection"; break;
                case Key::Ctrl_Y: if(!clipboard.empty()) { buffer.insert_string(clipboard); status_message="Yank"; } else status_message="Empty"; break;
                case Key::Enter: buffer.insert_char('\n'); break;
                case Key::Backspace: case Key::Ctrl_H: buffer.delete_char(); break;
                case Key::Del: buffer.delete_forward(); break;
                case Key::ArrowUp: case Key::Ctrl_P: move_cursor_2d(-1, 0); break;
                case Key::ArrowDown: case Key::Ctrl_N: move_cursor_2d(1, 0); break;
                case Key::ArrowLeft: case Key::Ctrl_B: move_cursor_lin(-1); break;
                case Key::ArrowRight: case Key::Ctrl_F: move_cursor_lin(1); break;
                default: if (is_printable((int)k)) buffer.insert_char((char)k); break;
            }
        }
        
        void process_chord(Key k) {
            waiting_for_chord = false; status_message = "";
            if (last_ctrl_key == (int)Key::Ctrl_X) {
                switch(k) {
                    case Key::Ctrl_C: should_quit = true; break;
                    case Key::Ctrl_S: buffer.save_to_file(current_filename); status_message = "Saved"; break;
                    default: status_message = "Unknown Chord"; break;
                }
            }
        }
    
        void process_meta(Key k) {
            waiting_for_meta = false; status_message = "";
            if ((char)k == 'w') {
                 if (selection_anchor != std::string::npos) {
                    clipboard = buffer.get_range(selection_anchor, buffer.get_cursor());
                    selection_anchor = std::string::npos; status_message = "Copy";
                 } else status_message = "No selection";
            } else status_message = "Unknown Meta";
        }
        
        void move_cursor_lin(int off) {
            long long np = (long long)buffer.get_cursor() + off;
            if (np < 0) np = 0; if (np > (long long)buffer.size()) np = buffer.size();
            buffer.move_gap((size_t)np);
        }
        
        void move_cursor_2d(int rd, int cd) {
            EditorCursor cur = get_visual_cursor();
            int tr = cur.r + rd; if (tr < 0) tr = 0;
            std::string c = buffer.get_content();
            int br = 0; size_t i = 0;
            while (br < tr && i < c.size()) { if (c[i] == '\n') br++; i++; }
            if (br < tr) { buffer.move_gap(c.size()); return; }
            int tc = cur.c + cd; 
            size_t eol = i; while (eol < c.size() && c[eol] != '\n') eol++;
            if (tc < 0) tc = 0; if (tc > (int)(eol - i)) tc = eol - i;
            buffer.move_gap(i + tc);
        }
    };
}
