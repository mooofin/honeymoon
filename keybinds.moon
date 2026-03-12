# Honeymoon Editor Keybindings
# =============================
# Syntax: key [key...] action
#
# Special keys:
#   C-x    = Ctrl+x
#   M-x    = Meta (Alt/Esc) + x
#   Enter, Tab, Backspace, Del, Esc
#   Up, Down, Left, Right
#   Home, End, PageUp, PageDown
#
# Examples:
#   C-x C-s save_file         # Chord: Ctrl-x then Ctrl-s
#   M-w copy                   # Meta-w (or Esc w)
#   C-g cancel                 # Single key

# === Basic Operations ===
C-Space mark_set
C-g cancel
C-w cut
M-w copy
C-y yank
Enter newline
C-j newline

# === Movement ===
C-f move_right
C-b move_left
C-n move_down
C-p move_up
Right move_right
Left move_left
Down move_down
Up move_up

C-a move_line_start
C-e move_line_end
M-f move_word_forward
M-b move_word_backward

# === Editing ===
Backspace delete_backward
Del delete_forward
C-k kill_line
M-d kill_word
C-t transpose_chars
M-t transpose_words
Tab indent
ShiftTab dedent

# === Search ===
C-s search_forward
C-r search_backward

# === File Operations ===
C-x C-s save_file
C-x C-f find_file
C-x C-c quit

# === Buffer Operations ===
C-x C-b list_buffers
C-x b list_buffers
C-x k kill_buffer
C-x h select_all

# === Navigation ===
M-g goto_line
C-l recenter

# === Help ===
C-h k help_key
C-h f help_func

# === Undo (not yet implemented) ===
C-/ undo

# === Custom Bindings ===
# Add your own keybindings below this line
# Example: C-q quit
