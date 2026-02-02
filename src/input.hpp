/*
 * Keyboard definitions.
 * Enums are better than raw ints.
 */
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

enum class Key : int {
  None = -999,
  Null = 0,
  Ctrl_Space = 0,
  Ctrl_2 = 0,
  Tab = 9,
  Backspace = 127,
  Enter = 13,
  Esc = 27,
  Ctrl_Slash = 31,
  Ctrl_A = 1,
  Ctrl_B = 2,
  Ctrl_C = 3,
  Ctrl_D = 4,
  Ctrl_E = 5,
  Ctrl_F = 6,
  Ctrl_G = 7,
  Ctrl_H = 8,
  Ctrl_I = 9,
  Ctrl_J = 10,
  Ctrl_K = 11,
  Ctrl_L = 12,
  Ctrl_M = 13,
  Ctrl_N = 14,
  Ctrl_O = 15,
  Ctrl_P = 16,
  Ctrl_Q = 17,
  Ctrl_R = 18,
  Ctrl_S = 19,
  Ctrl_T = 20,
  Ctrl_U = 21,
  Ctrl_V = 22,
  Ctrl_W = 23,
  Ctrl_X = 24,
  Ctrl_Y = 25,
  Ctrl_Z = 26,
  ArrowLeft = -1000,
  ArrowRight,
  ArrowUp,
  ArrowDown,
  Del,
  Home,
  End,
  PageUp,
  PageDown,
  ShiftTab = -1001
};

struct KeyChord {
  int key_code;
  bool operator==(const KeyChord &other) const {
    return key_code == other.key_code;
  }
};

constexpr bool is_printable(int k) { return k >= 32 && k < 127; }

inline std::string to_string(Key k) {
  switch (k) {
  case Key::ArrowLeft:
    return "Left";
  case Key::ArrowRight:
    return "Right";
  case Key::ArrowUp:
    return "Up";
  case Key::ArrowDown:
    return "Down";
  case Key::Del:
    return "Del";
  case Key::Home:
    return "Home";
  case Key::End:
    return "End";
  case Key::PageUp:
    return "PageUp";
  case Key::PageDown:
    return "PageDown";
  case Key::Enter:
    return "Enter";
  case Key::Tab:
    return "Tab";
  case Key::ShiftTab:
    return "ShiftTab";
  case Key::Backspace:
    return "Backspace";
  case Key::Esc:
    return "Esc";
  case Key::Ctrl_Space:
    return "C-Space";
  case Key::Ctrl_A:
    return "C-a";
  case Key::Ctrl_B:
    return "C-b";
  case Key::Ctrl_C:
    return "C-c";
  case Key::Ctrl_D:
    return "C-d";
  case Key::Ctrl_E:
    return "C-e";
  case Key::Ctrl_F:
    return "C-f";
  case Key::Ctrl_G:
    return "C-g";
  case Key::Ctrl_H:
    return "C-h";
  // case Key::Ctrl_I: return "C-i"; // Same as Tab
  case Key::Ctrl_J:
    return "C-j";
  case Key::Ctrl_K:
    return "C-k";
  case Key::Ctrl_L:
    return "C-l";
  // case Key::Ctrl_M: return "C-m"; // Same as Enter
  case Key::Ctrl_N:
    return "C-n";
  case Key::Ctrl_O:
    return "C-o";
  case Key::Ctrl_P:
    return "C-p";
  case Key::Ctrl_Q:
    return "C-q";
  case Key::Ctrl_R:
    return "C-r";
  case Key::Ctrl_S:
    return "C-s";
  case Key::Ctrl_T:
    return "C-t";
  case Key::Ctrl_U:
    return "C-u";
  case Key::Ctrl_V:
    return "C-v";
  case Key::Ctrl_W:
    return "C-w";
  case Key::Ctrl_X:
    return "C-x";
  case Key::Ctrl_Y:
    return "C-y";
  case Key::Ctrl_Z:
    return "C-z";
  case Key::Ctrl_Slash:
    return "C-/";
  default:
    if (is_printable((int)k))
      return std::string(1, (char)k);
    return "Unknown";
  }
}

inline Key key_from_string(std::string_view s) {
  if (s == "Left")
    return Key::ArrowLeft;
  if (s == "Right")
    return Key::ArrowRight;
  if (s == "Up")
    return Key::ArrowUp;
  if (s == "Down")
    return Key::ArrowDown;
  if (s == "Del")
    return Key::Del;
  if (s == "Home")
    return Key::Home;
  if (s == "End")
    return Key::End;
  if (s == "PageUp")
    return Key::PageUp;
  if (s == "PageDown")
    return Key::PageDown;
  if (s == "Enter")
    return Key::Enter;
  if (s == "Tab")
    return Key::Tab;
  if (s == "ShiftTab")
    return Key::ShiftTab;
  if (s == "Backspace")
    return Key::Backspace;
  if (s == "Esc")
    return Key::Esc;
  if (s == "C-Space")
    return Key::Ctrl_Space;
  if (s == "C-a")
    return Key::Ctrl_A;
  if (s == "C-b")
    return Key::Ctrl_B;
  if (s == "C-c")
    return Key::Ctrl_C;
  if (s == "C-d")
    return Key::Ctrl_D;
  if (s == "C-e")
    return Key::Ctrl_E;
  if (s == "C-f")
    return Key::Ctrl_F;
  if (s == "C-g")
    return Key::Ctrl_G;
  if (s == "C-h")
    return Key::Ctrl_H;
  if (s == "C-i")
    return Key::Ctrl_I;
  if (s == "C-j")
    return Key::Ctrl_J;
  if (s == "C-k")
    return Key::Ctrl_K;
  if (s == "C-l")
    return Key::Ctrl_L;
  if (s == "C-m")
    return Key::Ctrl_M;
  if (s == "C-n")
    return Key::Ctrl_N;
  if (s == "C-o")
    return Key::Ctrl_O;
  if (s == "C-p")
    return Key::Ctrl_P;
  if (s == "C-q")
    return Key::Ctrl_Q;
  if (s == "C-r")
    return Key::Ctrl_R;
  if (s == "C-s")
    return Key::Ctrl_S;
  if (s == "C-t")
    return Key::Ctrl_T;
  if (s == "C-u")
    return Key::Ctrl_U;
  if (s == "C-v")
    return Key::Ctrl_V;
  if (s == "C-w")
    return Key::Ctrl_W;
  if (s == "C-x")
    return Key::Ctrl_X;
  if (s == "C-y")
    return Key::Ctrl_Y;
  if (s == "C-z")
    return Key::Ctrl_Z;
  if (s == "C-/")
    return Key::Ctrl_Slash;
  if (s.size() == 1)
    return (Key)s[0];
  return Key::None;
}
