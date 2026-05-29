#include <gtest/gtest.h>
#include "buffer.hpp"
#include "test_utils.hpp"
#include <string>

using GapBuffer = honeymoon::mem::GapBuffer<char>;

// ── Construction ──────────────────────────────────────────────────────────

TEST(GapBufferTest, DefaultConstructedIsEmpty) {
    GapBuffer buf;
    EXPECT_EQ(buf.size(), 0);
    EXPECT_EQ(buf.get_cursor(), 0);
    EXPECT_FALSE(buf.is_dirty());
    EXPECT_TRUE(buf.get_content().empty());
}

// ── Insertion ─────────────────────────────────────────────────────────────

TEST(GapBufferTest, InsertSingleChars) {
    GapBuffer buf;
    buf.insert_char('h');
    buf.insert_char('e');
    buf.insert_char('l');
    buf.insert_char('l');
    buf.insert_char('o');
    EXPECT_EQ(buf.get_content(), "hello");
    EXPECT_EQ(buf.size(), 5);
    EXPECT_EQ(buf.get_cursor(), 5);
    EXPECT_TRUE(buf.is_dirty());
}

TEST(GapBufferTest, InsertString) {
    GapBuffer buf;
    buf.insert_string("Hello, World!");
    EXPECT_EQ(buf.get_content(), "Hello, World!");
    EXPECT_EQ(buf.size(), 13);
}

TEST(GapBufferTest, InsertStringWithLength) {
    GapBuffer buf;
    buf.insert_string("Hello, World!", 5);
    EXPECT_EQ(buf.get_content(), "Hello");
}

TEST(GapBufferTest, InsertAtCursorPosition) {
    GapBuffer buf;
    buf.insert_string("Helo");
    buf.move_gap(3);          // move cursor after "Hel"
    buf.insert_char('l');     // insert 'l' → "Hello"
    EXPECT_EQ(buf.get_content(), "Hello");
    EXPECT_EQ(buf.get_cursor(), 4);
}

// ── Deletion ──────────────────────────────────────────────────────────────

TEST(GapBufferTest, DeleteBackward) {
    GapBuffer buf;
    buf.insert_string("hello");
    buf.delete_char();        // delete last char
    EXPECT_EQ(buf.get_content(), "hell");
    EXPECT_EQ(buf.size(), 4);
}

TEST(GapBufferTest, DeleteBackwardAtStart) {
    GapBuffer buf;
    buf.insert_string("hello");
    buf.move_gap(0);
    buf.delete_char();        // nothing to delete
    EXPECT_EQ(buf.get_content(), "hello");
}

TEST(GapBufferTest, DeleteForward) {
    GapBuffer buf;
    buf.insert_string("hello");
    buf.move_gap(0);
    buf.delete_forward();     // delete 'h'
    EXPECT_EQ(buf.get_content(), "ello");
}

TEST(GapBufferTest, DeleteForwardAtEnd) {
    GapBuffer buf;
    buf.insert_string("hello");
    buf.delete_forward();     // cursor at end, nothing to delete forward
    EXPECT_EQ(buf.get_content(), "hello");
}

TEST(GapBufferTest, DeleteRange) {
    GapBuffer buf;
    buf.insert_string("Hello, World!");
    buf.delete_range(7, 12);  // delete "World"
    EXPECT_EQ(buf.get_content(), "Hello, !");
}

TEST(GapBufferTest, DeleteRangeReversedArgs) {
    GapBuffer buf;
    buf.insert_string("abcdef");
    buf.delete_range(5, 2);   // reversed, should delete cde
    EXPECT_EQ(buf.get_content(), "abf");
}

// ── Cursor Movement ───────────────────────────────────────────────────────

TEST(GapBufferTest, MoveGapToPosition) {
    GapBuffer buf;
    buf.insert_string("hello world");
    buf.move_gap(5);
    EXPECT_EQ(buf.get_cursor(), 5);
}

TEST(GapBufferTest, MoveGapBeyondSize) {
    GapBuffer buf;
    buf.insert_string("hi");
    buf.move_gap(100);        // clamp to size
    EXPECT_EQ(buf.get_cursor(), 2);
}

// ── Content Access ────────────────────────────────────────────────────────

TEST(GapBufferTest, GetCharAt) {
    GapBuffer buf;
    buf.insert_string("hello");
    EXPECT_EQ(buf.get_char_at(0), 'h');
    EXPECT_EQ(buf.get_char_at(1), 'e');
    EXPECT_EQ(buf.get_char_at(4), 'o');
}

TEST(GapBufferTest, GetRange) {
    GapBuffer buf;
    buf.insert_string("hello world");
    EXPECT_EQ(buf.get_range(0, 5), "hello");
    EXPECT_EQ(buf.get_range(6, 11), "world");
}

TEST(GapBufferTest, GetRangeReversed) {
    GapBuffer buf;
    buf.insert_string("hello");
    EXPECT_EQ(buf.get_range(4, 1), "ell");
}

// ── File I/O ──────────────────────────────────────────────────────────────

TEST(GapBufferTest, LoadFromFile) {
    honeymoon::test::TempFile tf("Hello, File!");
    ASSERT_TRUE(tf);

    GapBuffer buf;
    buf.load_from_file(tf.path());
    EXPECT_EQ(buf.get_content(), "Hello, File!");
    EXPECT_EQ(buf.size(), 12);
    EXPECT_FALSE(buf.is_dirty());    // load does not mark dirty
}

TEST(GapBufferTest, SaveToFile) {
    honeymoon::test::TempFile tf("");
    ASSERT_TRUE(tf);

    GapBuffer buf;
    buf.insert_string("Saved content");
    buf.save_to_file(tf.path());

    // Read it back
    GapBuffer verify;
    verify.load_from_file(tf.path());
    EXPECT_EQ(verify.get_content(), "Saved content");
    EXPECT_FALSE(buf.is_dirty());    // save clears dirty
}

TEST(GapBufferTest, LoadNonexistentFile) {
    GapBuffer buf;
    buf.load_from_file("/nonexistent/path/12345");
    EXPECT_EQ(buf.size(), 0);
    EXPECT_TRUE(buf.get_content().empty());
}

// ── Dirty Flag ────────────────────────────────────────────────────────────

TEST(GapBufferTest, DirtyAfterInsert) {
    GapBuffer buf;
    EXPECT_FALSE(buf.is_dirty());
    buf.insert_char('x');
    EXPECT_TRUE(buf.is_dirty());
}

TEST(GapBufferTest, DirtyAfterDelete) {
    GapBuffer buf;
    buf.insert_string("abc");
    buf.set_dirty(false);

    buf.delete_char();
    EXPECT_TRUE(buf.is_dirty());
}

// ── Move semantics ────────────────────────────────────────────────────────

TEST(GapBufferTest, MoveConstruct) {
    GapBuffer buf;
    buf.insert_string("move test");

    GapBuffer moved(std::move(buf));
    EXPECT_EQ(moved.get_content(), "move test");
}

TEST(GapBufferTest, MoveAssign) {
    GapBuffer buf;
    buf.insert_string("assign test");

    GapBuffer target;
    target = std::move(buf);
    EXPECT_EQ(target.get_content(), "assign test");
}

// ── Gap Expansion ─────────────────────────────────────────────────────────

TEST(GapBufferTest, GapExpandsWhenFull) {
    GapBuffer buf;
    // Insert more than DEFAULT_GAP_SIZE (1024) chars to trigger expand_gap
    std::string big(GapBuffer::DEFAULT_GAP_SIZE + 100, 'x');
    buf.insert_string(big);
    EXPECT_EQ(buf.size(), GapBuffer::DEFAULT_GAP_SIZE + 100);
    EXPECT_EQ(buf.get_content(), big);
}

// ── With wchar_t ──────────────────────────────────────────────────────────

TEST(GapBufferTest, WideCharBuffer) {
    honeymoon::mem::GapBuffer<wchar_t> wbuf;
    wbuf.insert_char(L'a');
    wbuf.insert_char(L'b');
    EXPECT_EQ(wbuf.size(), 2);
}
