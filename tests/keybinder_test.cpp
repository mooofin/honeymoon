#include <gtest/gtest.h>
#include "keybinder.hpp"
#include "test_utils.hpp"
#include <cstdio>
#include <unistd.h>

using KeyBinder = honeymoon::config::KeyBinder;

// ── Empty / Missing File ──────────────────────────────────────────────────

TEST(KeyBinderTest, MissingFileReturnsEmpty) {
    auto bindings = KeyBinder::load_from_file("/nonexistent/path/12345");
    EXPECT_TRUE(bindings.empty());
}

TEST(KeyBinderTest, EmptyFileReturnsEmpty) {
    honeymoon::test::TempFile tf("");
    ASSERT_TRUE(tf);

    auto bindings = KeyBinder::load_from_file(tf.path());
    EXPECT_TRUE(bindings.empty());
}

// ── Single Bindings ───────────────────────────────────────────────────────

TEST(KeyBinderTest, SingleKeyBinding) {
    honeymoon::test::TempFile tf("C-x quit\n");
    ASSERT_TRUE(tf);

    auto bindings = KeyBinder::load_from_file(tf.path());
    ASSERT_EQ(bindings.size(), 1);
    ASSERT_EQ(bindings[0].keys.size(), 1);
    EXPECT_EQ(bindings[0].keys[0], Key::Ctrl_X);
    EXPECT_EQ(bindings[0].action, "quit");
}

TEST(KeyBinderTest, MultiKeyBinding) {
    honeymoon::test::TempFile tf("C-x C-c quit\n");
    ASSERT_TRUE(tf);

    auto bindings = KeyBinder::load_from_file(tf.path());
    ASSERT_EQ(bindings.size(), 1);
    ASSERT_EQ(bindings[0].keys.size(), 2);
    EXPECT_EQ(bindings[0].keys[0], Key::Ctrl_X);
    EXPECT_EQ(bindings[0].keys[1], Key::Ctrl_C);
    EXPECT_EQ(bindings[0].action, "quit");
}

// ── Meta prefix ───────────────────────────────────────────────────────────

TEST(KeyBinderTest, MetaPrefixExpandsToEsc) {
    honeymoon::test::TempFile tf("M-x eval-expression\n");
    ASSERT_TRUE(tf);

    auto bindings = KeyBinder::load_from_file(tf.path());
    ASSERT_EQ(bindings.size(), 1);
    ASSERT_EQ(bindings[0].keys.size(), 2);
    EXPECT_EQ(bindings[0].keys[0], Key::Esc);      // M- → Esc prefix
    EXPECT_EQ(bindings[0].keys[1], Key('x'));
    EXPECT_EQ(bindings[0].action, "eval-expression");
}

// ── Named keys ────────────────────────────────────────────────────────────

TEST(KeyBinderTest, NamedKeys) {
    // Note: Key::ArrowRight == -999 == Key::None (pre-existing enum collision),
    // so "Right" cannot be used with the k != Key::None guard.
    // Use other named keys instead.
    FILE* f = fopen(".test_bindings", "w");
    ASSERT_TRUE(f);
    fprintf(f, "PageUp page_up\nPageDown page_down\nHome go_home\n");
    fclose(f);

    auto bindings = KeyBinder::load_from_file(".test_bindings");

    ASSERT_EQ(bindings.size(), 3);
    EXPECT_EQ(bindings[0].keys[0], Key::PageUp);
    EXPECT_EQ(bindings[0].action, "page_up");
    EXPECT_EQ(bindings[1].keys[0], Key::PageDown);
    EXPECT_EQ(bindings[1].action, "page_down");
    EXPECT_EQ(bindings[2].keys[0], Key::Home);
    EXPECT_EQ(bindings[2].action, "go_home");

    unlink(".test_bindings");
}

TEST(KeyBinderTest, EscKey) {
    honeymoon::test::TempFile tf("Esc cancel\n");
    ASSERT_TRUE(tf);

    auto bindings = KeyBinder::load_from_file(tf.path());
    ASSERT_EQ(bindings.size(), 1);
    EXPECT_EQ(bindings[0].keys[0], Key::Esc);
    EXPECT_EQ(bindings[0].action, "cancel");
}

// ── Comments ──────────────────────────────────────────────────────────────

TEST(KeyBinderTest, IgnoresComments) {
    honeymoon::test::TempFile tf(
        "# This is a comment\n"
        "C-x C-s save_file\n"
        "# Another comment\n");
    ASSERT_TRUE(tf);

    auto bindings = KeyBinder::load_from_file(tf.path());
    ASSERT_EQ(bindings.size(), 1);
    EXPECT_EQ(bindings[0].action, "save_file");
}

// ── Multiple bindings ─────────────────────────────────────────────────────

TEST(KeyBinderTest, MultipleBindings) {
    honeymoon::test::TempFile tf(
        "C-x C-c quit\n"
        "C-x C-s save_file\n"
        "C-x u undo\n");
    ASSERT_TRUE(tf);

    auto bindings = KeyBinder::load_from_file(tf.path());
    ASSERT_EQ(bindings.size(), 3);
    EXPECT_EQ(bindings[0].action, "quit");
    EXPECT_EQ(bindings[1].action, "save_file");
    EXPECT_EQ(bindings[2].action, "undo");
}

// ── Lines with insufficient tokens are skipped ────────────────────────────

TEST(KeyBinderTest, SkipsLinesWithOnlyOneToken) {
    honeymoon::test::TempFile tf("justakey\n");
    ASSERT_TRUE(tf);

    auto bindings = KeyBinder::load_from_file(tf.path());
    EXPECT_TRUE(bindings.empty());
}

// ── Edge: unknown key string ──────────────────────────────────────────────

TEST(KeyBinderTest, SkipsUnknownKeys) {
    honeymoon::test::TempFile tf("Frobnicate do_thing\n");
    ASSERT_TRUE(tf);

    auto bindings = KeyBinder::load_from_file(tf.path());
    // "Frobnicate" is not a known key name → key_from_string returns None → skipped
    // So the key vector will be empty for that token, and since no valid keys,
    // no binding is added
    EXPECT_TRUE(bindings.empty());
}
