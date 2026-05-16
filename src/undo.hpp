#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace honeymoon::mem {

template <typename...>
struct UndoHistory {
protected:
  struct Snapshot {
    std::string content;
    size_t cursor;
  };

  std::vector<Snapshot> undo_stack;
  std::vector<Snapshot> redo_stack;
  bool typing_group_active = false;
  size_t max_undo_levels = 100;

  void snapshot_for_undo(const std::string &content, size_t cursor) {
    if (typing_group_active)
      return;
    undo_stack.push_back({content, cursor});
    if (undo_stack.size() > max_undo_levels)
      undo_stack.erase(undo_stack.begin());
    typing_group_active = true;
  }

  void close_typing_group() { typing_group_active = false; }

  std::optional<Snapshot> apply_undo(const std::string &cur_content,
                                      size_t cur_cursor) {
    if (undo_stack.empty())
      return std::nullopt;
    redo_stack.push_back({cur_content, cur_cursor});
    Snapshot s = undo_stack.back();
    undo_stack.pop_back();
    typing_group_active = false;
    return s;
  }

  std::optional<Snapshot> apply_redo(const std::string &cur_content,
                                      size_t cur_cursor) {
    if (redo_stack.empty())
      return std::nullopt;
    undo_stack.push_back({cur_content, cur_cursor});
    Snapshot s = redo_stack.back();
    redo_stack.pop_back();
    typing_group_active = false;
    return s;
  }
};

} // namespace honeymoon::mem
