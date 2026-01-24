
#pragma once
#include <concepts>
#include <string>
#include <cstddef>
#include "input.hpp"

namespace honeymoon::kernel {

    template<typename T>
    concept CharType = std::same_as<T, char> || std::same_as<T, wchar_t>;

    template<typename B>
    concept EditableBuffer = requires(B b, const std::string& filename, size_t pos, size_t start, size_t end, const std::string& s) {
        { b.load_from_file(filename) } -> std::same_as<void>;
        { b.save_to_file(filename) } -> std::same_as<void>;
        { b.insert_char('c') } -> std::same_as<void>;
        { b.delete_char() } -> std::same_as<void>;
        { b.delete_forward() } -> std::same_as<void>;
        { b.move_gap(pos) } -> std::same_as<void>;
        { b.get_cursor() } -> std::convertible_to<size_t>;
        { b.size() } -> std::convertible_to<size_t>;
        { b.get_content() } -> std::convertible_to<std::string>;
        { b.is_dirty() } -> std::same_as<bool>;
        { b.set_dirty(true) } -> std::same_as<void>;
        { b.insert_string(s) } -> std::same_as<void>;
        { b.get_range(start, end) } -> std::convertible_to<std::string>;
        { b.delete_range(start, end) } -> std::same_as<void>;
    };

    template<typename T>
    concept TerminalDevice = requires(T t, std::string_view data) {
        { t.get_window_size() } -> std::same_as<std::pair<int, int>>;
        { t.read_key() } -> std::same_as<Key>;
        { t.write_raw(data) } -> std::same_as<void>;
    };
}
