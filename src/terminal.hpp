/*
 * TTY Driver.
 * Disables canonical mode/echo so we can do the rendering.
 */
#pragma once
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <iostream>
#include <string>
#include <utility>
#include "input.hpp"
#include "concepts.hpp"

namespace honeymoon::driver {
    class Terminal {
    public:
        Terminal() { if (!enable_raw_mode()) std::cerr << "Kernel Panic: No Raw Mode" << std::endl; }
        ~Terminal() { disable_raw_mode(); }
        Terminal(const Terminal&) = delete;
        Terminal& operator=(const Terminal&) = delete;

        std::pair<int, int> get_window_size() const {
            struct winsize ws;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) return {24, 80};
            return {ws.ws_row, ws.ws_col};
        }

        Key read_key() {
            int n; char c;
            while ((n = read(STDIN_FILENO, &c, 1)) != 1) { if (n == -1 && errno != EAGAIN) return Key::None; }
            if (c == 27) {
                char seq[3];
                if (read(STDIN_FILENO, &seq[0], 1) != 1) return Key::Esc;
                if (read(STDIN_FILENO, &seq[1], 1) != 1) return Key::Esc;
                if (seq[0] == '[') {
                    if (seq[1] >= '0' && seq[1] <= '9') {
                        if (read(STDIN_FILENO, &seq[2], 1) != 1) return Key::Esc;
                        if (seq[2] == '~') {
                            switch (seq[1]) {
                                case '1': case '7': return Key::Home;
                                case '3': return Key::Del;
                                case '4': case '8': return Key::End;
                                case '5': return Key::PageUp;
                                case '6': return Key::PageDown;
                            }
                        }
                    } else {
                        switch (seq[1]) {
                            case 'A': return Key::ArrowUp; case 'B': return Key::ArrowDown;
                            case 'C': return Key::ArrowRight; case 'D': return Key::ArrowLeft;
                            case 'H': return Key::Home; case 'F': return Key::End;
                        }
                    }
                } else if (seq[0] == 'O') {
                    switch (seq[1]) { case 'H': return Key::Home; case 'F': return Key::End; }
                }
                return Key::Esc;
            }
            return static_cast<Key>(c);
        }

        void write_raw(std::string_view s) { write(STDOUT_FILENO, s.data(), s.size()); }

    private:
        struct termios orig_termios;
        bool raw_mode_enabled = false;

        bool enable_raw_mode() {
            if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return false;
            struct termios raw = orig_termios;
            raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
            raw.c_oflag &= ~(OPOST);
            raw.c_cflag |= (CS8);
            raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
            raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 1;
            if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return false;
            raw_mode_enabled = true;
            return true;
        }

        void disable_raw_mode() {
            if (raw_mode_enabled) { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); raw_mode_enabled = false; }
        }
    };
    static_assert(honeymoon::kernel::TerminalDevice<Terminal>);
}
