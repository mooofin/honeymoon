#include <gtest/gtest.h>
#include "undo.hpp"
#include <string>

// Test helper that exposes protected UndoHistory members
struct TestUndo : honeymoon::mem::UndoHistory<> {
    using honeymoon::mem::UndoHistory<>::snapshot_for_undo;
    using honeymoon::mem::UndoHistory<>::apply_undo;
    using honeymoon::mem::UndoHistory<>::apply_redo;
    using honeymoon::mem::UndoHistory<>::close_typing_group;
};

// ── Basic Undo ────────────────────────────────────────────────────────────

TEST(UndoTest, UndoReturnsPreviousSnapshot) {
    TestUndo undo;
    undo.snapshot_for_undo("hello", 5);

    auto result = undo.apply_undo("hello!", 6);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->content, "hello");
    EXPECT_EQ(result->cursor, 5);
}

TEST(UndoTest, UndoEmptyStackReturnsNullopt) {
    TestUndo undo;
    auto result = undo.apply_undo("current", 0);
    EXPECT_FALSE(result.has_value());
}

// ── Basic Redo ────────────────────────────────────────────────────────────

TEST(UndoTest, RedoAfterUndo) {
    TestUndo undo;
    undo.snapshot_for_undo("first", 5);
    undo.apply_undo("second", 6);   // after undo: redo stack has "second"

    auto result = undo.apply_redo("first", 5);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->content, "second");
    EXPECT_EQ(result->cursor, 6);
}

TEST(UndoTest, RedoEmptyStackReturnsNullopt) {
    TestUndo undo;
    EXPECT_FALSE(undo.apply_redo("cur", 0).has_value());
}

// ── Typing Group ──────────────────────────────────────────────────────────

TEST(UndoTest, TypingGroupPreventsMultipleSnapshots) {
    TestUndo undo;
    undo.snapshot_for_undo("initial", 0);
    undo.snapshot_for_undo("should not appear", 1);  // typing group active → skipped
    undo.close_typing_group();

    EXPECT_TRUE(undo.apply_undo("after typing", 1).has_value());
    // Second undo should return nullopt since only 1 snapshot was taken
    EXPECT_FALSE(undo.apply_undo("after typing", 1).has_value());
}

// ── Undo After Redo (new action clears redo) ──────────────────────────────

TEST(UndoTest, NewSnapshotClearsRedoStack) {
    TestUndo undo;
    undo.snapshot_for_undo("a", 1);
    undo.apply_undo("b", 1);       // redo now has "b"

    // Close typing group and take a new snapshot (simulates new action)
    undo.close_typing_group();
    undo.snapshot_for_undo("c", 1);

    // Redo should be empty now
    EXPECT_FALSE(undo.apply_redo("c", 1).has_value());
}

// ── Max Undo Levels ────────────────────────────────────────────────────────

TEST(UndoTest, RespectsMaxUndoLevels) {
    TestUndo undo;
    for (int i = 0; i < 150; ++i) {
        undo.close_typing_group();
        undo.snapshot_for_undo(std::to_string(i), i);
    }

    // Only the last 100 should remain (max_undo_levels = 100)
    int undo_count = 0;
    while (undo.apply_undo("current", 0).has_value())
        ++undo_count;

    EXPECT_EQ(undo_count, 100);
}

// ── Full Cycle ────────────────────────────────────────────────────────────
//
// Simulates typing "", 'a', 'b', 'c'.  snapshot_for_undo() is called BEFORE
// each edit, so the stack holds the pre-edit states: ["", "a", "ab"].
// The current buffer content "abc" is passed directly to apply_undo as
// cur_content — it is never pushed onto the stack itself.

TEST(UndoTest, FullUndoRedoCycle) {
    TestUndo undo;

    undo.close_typing_group();
    undo.snapshot_for_undo("", 0);    // before typing 'a'
    undo.close_typing_group();
    undo.snapshot_for_undo("a", 1);   // before typing 'b'
    undo.close_typing_group();
    undo.snapshot_for_undo("ab", 2);  // before typing 'c'
    // Current state is now "abc" — no snapshot for it, it is passed as cur_content

    // Undo 3 times
    auto s1 = undo.apply_undo("abc", 3);
    ASSERT_TRUE(s1.has_value());
    EXPECT_EQ(s1->content, "ab");

    auto s2 = undo.apply_undo(s1->content, s1->cursor);
    ASSERT_TRUE(s2.has_value());
    EXPECT_EQ(s2->content, "a");

    auto s3 = undo.apply_undo(s2->content, s2->cursor);
    ASSERT_TRUE(s3.has_value());
    EXPECT_EQ(s3->content, "");

    // Redo 2 times
    auto r1 = undo.apply_redo(s3->content, s3->cursor);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->content, "a");

    auto r2 = undo.apply_redo(r1->content, r1->cursor);
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->content, "ab");
}
