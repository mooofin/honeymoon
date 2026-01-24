/*
 * Keyboard definitions.
 * Enums are better than raw ints.
 */
#pragma once
#include <string_view>
#include <optional>
#include <cstdint>

enum class Key : int {
    None = -999,
    Null = 0, Ctrl_Space = 0, Ctrl_2 = 0, Tab = 9,
    Backspace = 127, Enter = 13, Esc = 27, Ctrl_Slash = 31,
    Ctrl_A=1, Ctrl_B=2, Ctrl_C=3, Ctrl_D=4, Ctrl_E=5, Ctrl_F=6, Ctrl_G=7,
    Ctrl_H=8, Ctrl_I=9, Ctrl_J=10, Ctrl_K=11, Ctrl_L=12, Ctrl_M=13, Ctrl_N=14,
    Ctrl_O=15, Ctrl_P=16, Ctrl_Q=17, Ctrl_R=18, Ctrl_S=19, Ctrl_T=20, Ctrl_U=21,
    Ctrl_V=22, Ctrl_W=23, Ctrl_X=24, Ctrl_Y=25, Ctrl_Z=26,
    ArrowLeft = -1000, ArrowRight, ArrowUp, ArrowDown, Del, Home, End, PageUp, PageDown,
    ShiftTab = -1001
};

struct KeyChord {
    int key_code;
    bool operator==(const KeyChord& other) const { return key_code == other.key_code; }
};

constexpr bool is_printable(int k) { return k >= 32 && k < 127; }
