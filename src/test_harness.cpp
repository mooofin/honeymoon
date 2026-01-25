
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <queue>

// Hack to access private members for testing
#define private public
#include "editor.hpp"
#include "buffer.hpp"
#include "input.hpp"
#undef private

using namespace honeymoon::kernel;
using namespace honeymoon::mem;

class MockTerminal {
public:
    std::queue<Key> input_queue;
    std::string output;

    Key read_key() {
        if (input_queue.empty()) return Key::None; // Should probably signal exit or wait, but for test we can loop
        Key k = input_queue.front();
        input_queue.pop();
        return k;
    }

    void write_raw(std::string_view s) {
        output += s;
    }

    std::pair<int, int> get_window_size() const {
        return {24, 80};
    }

    // specific method for test to push keys
    void push_key(Key k) { input_queue.push(k); }
    void push_string(const std::string& s) {
        for (char c : s) input_queue.push(static_cast<Key>(c));
    }
};

void log_test(const std::string& name, bool passed) {
    if (passed) std::cout << "[PASS] " << name << std::endl;
    else {
        std::cout << "[FAIL] " << name << std::endl;
        exit(1);
    }
}

int main() {
    std::cout << "Starting Keybinding Verification..." << std::endl;

    // 1. Navigation Test
    {
        Editor<GapBuffer<char>, MockTerminal> editor;
        // Insert "Hello World"
        editor.terminal.push_string("Hello World");
        // Move back 5 chars (World) -> Cursor at ' '
        for(int i=0; i<5; ++i) editor.terminal.push_key(Key::Ctrl_B);
        
        // Process
        while(!editor.terminal.input_queue.empty()) editor.process_keypress();
        
        std::string content = editor.buffer.get_content();
        size_t cursor = editor.buffer.get_cursor();
        
        log_test("Content Insertion", content == "Hello World");
        log_test("Ctrl-B Navigation", cursor == 6); // "Hello " is 6 chars
        
        // Move Forward word (Alt-F)
        editor.terminal.push_key(Key::Esc); editor.terminal.push_key(static_cast<Key>('f')); 
        while(!editor.terminal.input_queue.empty()) editor.process_keypress();
        log_test("Alt-F Move Word", editor.buffer.get_cursor() == 11); // End of line
        
        // Move Start Line (Ctrl-A)
        editor.terminal.push_key(Key::Ctrl_A);
        while(!editor.terminal.input_queue.empty()) editor.process_keypress();
        log_test("Ctrl-A Start Line", editor.buffer.get_cursor() == 0);
    }

    // 2. Editing Test (Kill/Cut/Paste)
    {
        Editor<GapBuffer<char>, MockTerminal> editor;
        editor.terminal.push_string("Kill This Word");
        editor.terminal.push_key(Key::Ctrl_A); // Start
        editor.terminal.push_key(Key::Alt_D);  // Kill "Kill"
        
        while(!editor.terminal.input_queue.empty()) editor.process_keypress();
        
        log_test("Alt-D Kill Word", editor.buffer.get_content() == " This Word"); // Space remains or consumed? 
        // Logic: kill_word consumes until separator, then consumes non-separators. 
        // "Kill This" -> "Kill" matches non-sep. 
        // Correct logic in move_word_forward: while(is_sep) idx++; while(!is_sep) idx++;
        // So from start: "Kill" is !sep. " " is sep. 
        // It might be slightly off depending on exact cursor start.
        // Let's verify Ctrl-K
        
        editor.terminal.push_key(Key::Ctrl_K); // Kill rest
        while(!editor.terminal.input_queue.empty()) editor.process_keypress();
        log_test("Ctrl-K Kill Line", editor.buffer.get_content() == ""); 
        log_test("Clipboard Content", editor.clipboard == " This Word");
        
        editor.terminal.push_key(Key::Ctrl_Y); // Yank
        while(!editor.terminal.input_queue.empty()) editor.process_keypress();
        log_test("Ctrl-Y Yank", editor.buffer.get_content() == " This Word");
    }
    
    // 3. Search Test
    {
        Editor<GapBuffer<char>, MockTerminal> editor;
        editor.terminal.push_string("Find the needle in haystack");
        editor.terminal.push_key(Key::Ctrl_A); 
        
        // Start Search
        editor.terminal.push_key(Key::Ctrl_S); // Enter search mode
        editor.terminal.push_string("needle"); 
        
        while(!editor.terminal.input_queue.empty()) editor.process_keypress();
        
        log_test("Incremental Search Mode", editor.current_mode == Mode::TextSearch);
        
        // Press Enter to confirm
        editor.terminal.push_key(Key::Enter);
        while(!editor.terminal.input_queue.empty()) editor.process_keypress();
        
        size_t needle_pos = std::string("Find the ").length();
        log_test("Search Navigation", editor.buffer.get_cursor() == needle_pos);
    }
    
    // 4. Goto Line Test
    {
        Editor<GapBuffer<char>, MockTerminal> editor;
        editor.terminal.push_string("Line 1\nLine 2\nLine 3");
        editor.terminal.push_key(Key::Esc); editor.terminal.push_key(static_cast<Key>('g')); // Alt-G
        editor.terminal.push_key(static_cast<Key>('g'));
        
        // Input "2" then Enter
        editor.terminal.push_key(static_cast<Key>('2'));
        editor.terminal.push_key(Key::Enter);
        
        while(!editor.terminal.input_queue.empty()) editor.process_keypress();
        
        // Should be at start of Line 2. "Line 1\n" length = 7.
        log_test("Goto Line 2", editor.buffer.get_cursor() == 7);
    }

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
