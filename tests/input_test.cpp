#include <gtest/gtest.h>
#include "input.hpp"

// ── is_printable ──────────────────────────────────────────────────────────

TEST(InputTest, IsPrintableReturnsTrueForPrintable) {
    EXPECT_TRUE(is_printable('a'));
    EXPECT_TRUE(is_printable('Z'));
    EXPECT_TRUE(is_printable('0'));
    EXPECT_TRUE(is_printable('!'));
    EXPECT_TRUE(is_printable('~'));
    EXPECT_TRUE(is_printable(32));   // space
    EXPECT_TRUE(is_printable(126));  // ~
}

TEST(InputTest, IsPrintableReturnsFalseForNonPrintable) {
    EXPECT_FALSE(is_printable(0));
    EXPECT_FALSE(is_printable(10));  // newline
    EXPECT_FALSE(is_printable(27));  // escape
    EXPECT_FALSE(is_printable(31));
    EXPECT_FALSE(is_printable(127));
    EXPECT_FALSE(is_printable(-1));
}

// ── to_string ─────────────────────────────────────────────────────────────

TEST(InputTest, ToStringArrowKeys) {
    EXPECT_EQ(to_string(Key::ArrowLeft), "Left");
    EXPECT_EQ(to_string(Key::ArrowRight), "Right");
    EXPECT_EQ(to_string(Key::ArrowUp), "Up");
    EXPECT_EQ(to_string(Key::ArrowDown), "Down");
}

TEST(InputTest, ToStringSpecialKeys) {
    EXPECT_EQ(to_string(Key::Enter), "Enter");
    EXPECT_EQ(to_string(Key::Tab), "Tab");
    EXPECT_EQ(to_string(Key::Esc), "Esc");
    EXPECT_EQ(to_string(Key::Backspace), "Backspace");
    EXPECT_EQ(to_string(Key::Del), "Del");
    EXPECT_EQ(to_string(Key::Home), "Home");
    EXPECT_EQ(to_string(Key::End), "End");
    EXPECT_EQ(to_string(Key::PageUp), "PageUp");
    EXPECT_EQ(to_string(Key::PageDown), "PageDown");
    EXPECT_EQ(to_string(Key::ShiftTab), "ShiftTab");
}

TEST(InputTest, ToStringCtrlKeys) {
    EXPECT_EQ(to_string(Key::Ctrl_A), "C-a");
    EXPECT_EQ(to_string(Key::Ctrl_B), "C-b");
    EXPECT_EQ(to_string(Key::Ctrl_X), "C-x");
    EXPECT_EQ(to_string(Key::Ctrl_C), "C-c");
    EXPECT_EQ(to_string(Key::Ctrl_Z), "C-z");
    EXPECT_EQ(to_string(Key::Ctrl_Space), "C-Space");
    EXPECT_EQ(to_string(Key::Ctrl_Slash), "C-/");
}

TEST(InputTest, ToStringPrintable) {
    EXPECT_EQ(to_string(static_cast<Key>('A')), "A");
    EXPECT_EQ(to_string(static_cast<Key>('z')), "z");
    EXPECT_EQ(to_string(static_cast<Key>('1')), "1");
}

TEST(InputTest, ToStringUnknown) {
    EXPECT_EQ(to_string(static_cast<Key>(200)), "Unknown");
}

// ── key_from_string ───────────────────────────────────────────────────────

TEST(InputTest, KeyFromStringArrowKeys) {
    EXPECT_EQ(key_from_string("Left"), Key::ArrowLeft);
    EXPECT_EQ(key_from_string("Right"), Key::ArrowRight);
    EXPECT_EQ(key_from_string("Up"), Key::ArrowUp);
    EXPECT_EQ(key_from_string("Down"), Key::ArrowDown);
}

TEST(InputTest, KeyFromStringSpecialKeys) {
    EXPECT_EQ(key_from_string("Enter"), Key::Enter);
    EXPECT_EQ(key_from_string("Tab"), Key::Tab);
    EXPECT_EQ(key_from_string("Esc"), Key::Esc);
    EXPECT_EQ(key_from_string("Backspace"), Key::Backspace);
    EXPECT_EQ(key_from_string("Del"), Key::Del);
    EXPECT_EQ(key_from_string("Home"), Key::Home);
    EXPECT_EQ(key_from_string("End"), Key::End);
    EXPECT_EQ(key_from_string("PageUp"), Key::PageUp);
    EXPECT_EQ(key_from_string("PageDown"), Key::PageDown);
    EXPECT_EQ(key_from_string("ShiftTab"), Key::ShiftTab);
}

TEST(InputTest, KeyFromStringCtrlKeys) {
    EXPECT_EQ(key_from_string("C-a"), Key::Ctrl_A);
    EXPECT_EQ(key_from_string("C-x"), Key::Ctrl_X);
    EXPECT_EQ(key_from_string("C-Space"), Key::Ctrl_Space);
    EXPECT_EQ(key_from_string("C-/"), Key::Ctrl_Slash);
}

TEST(InputTest, KeyFromStringSingleChar) {
    EXPECT_EQ(key_from_string("A"), Key('A'));
    EXPECT_EQ(key_from_string("x"), Key('x'));
    EXPECT_EQ(key_from_string("3"), Key('3'));
}

TEST(InputTest, KeyFromStringReturnsNoneForInvalid) {
    EXPECT_EQ(key_from_string("NotAKey"), Key::None);
    EXPECT_EQ(key_from_string(""), Key::None);
}

// ── KeyChord ──────────────────────────────────────────────────────────────

TEST(InputTest, KeyChordEquality) {
    KeyChord a{42};
    KeyChord b{42};
    KeyChord c{99};
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}
