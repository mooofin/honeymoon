/*
 * Gap Buffer.
 * It's a std::vector with a hole in the middle. O(1) at the cursor, O(N) everywhere else.
 */
#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <memory>
#include "concepts.hpp"

namespace honeymoon::mem {
    template <typename CharT = char>
    requires honeymoon::kernel::CharType<CharT>
    class GapBuffer {
    public:
        using value_type = CharT;
        using size_type = size_t;
        static constexpr size_type DEFAULT_GAP_SIZE = 1024;

        GapBuffer() : gap_start(0), gap_end(DEFAULT_GAP_SIZE) { buffer.resize(DEFAULT_GAP_SIZE); }
        GapBuffer(GapBuffer&&) noexcept = default;
        GapBuffer& operator=(GapBuffer&&) noexcept = default;
        GapBuffer(const GapBuffer&) = delete;
        GapBuffer& operator=(const GapBuffer&) = delete;
        ~GapBuffer() = default;

        void load_from_file(const std::string& filename) {
            std::ifstream file(filename, std::ios::binary);
            if (!file.is_open()) return;
            file.seekg(0, std::ios::end);
            size_type size = file.tellg();
            file.seekg(0, std::ios::beg);
            buffer.resize(size + DEFAULT_GAP_SIZE);
            file.read(reinterpret_cast<char*>(buffer.data()), size);
            gap_start = size;
            gap_end = buffer.size();
        }

        void save_to_file(const std::string& filename) {
            std::ofstream file(filename, std::ios::binary);
            if (!file.is_open()) return;
            if (gap_start > 0) file.write(reinterpret_cast<const char*>(buffer.data()), gap_start);
            if (gap_end < buffer.size()) file.write(reinterpret_cast<const char*>(buffer.data() + gap_end), buffer.size() - gap_end);
            dirty = false;
        }

        void insert_char(CharT c) {
            if (gap_start == gap_end) expand_gap();
            buffer[gap_start++] = c;
            dirty = true;
        }

        void insert_string(const std::string& s) { for (char c : s) insert_char(c); }

        void delete_char() {
            if (gap_start > 0) { gap_start--; dirty = true; }
        }
        
        void delete_forward() {
            if (gap_end < buffer.size()) { gap_end++; dirty = true; }
        }

        void delete_range(size_type start, size_type end) {
            if (start > end) std::swap(start, end);
            if (end > size()) end = size();
            move_gap(start);
            size_type count = end - start;
            while (count > 0 && gap_end < buffer.size()) { gap_end++; count--; }
            dirty = true;
        }

        void move_gap(size_type position) {
            if (position < gap_start) {
                size_type move = gap_start - position;
                std::copy(buffer.begin() + position, buffer.begin() + gap_start, buffer.begin() + gap_end - move);
                gap_start -= move; gap_end -= move;
            } else if (position > gap_start) {
                size_type move = position - gap_start;
                std::copy(buffer.begin() + gap_end, buffer.begin() + gap_end + move, buffer.begin() + gap_start);
                gap_start += move; gap_end += move;
            }
        }
        
        std::string get_content() const {
            std::string res;
            res.reserve(size());
            res.append(reinterpret_cast<const char*>(buffer.data()), gap_start);
            res.append(reinterpret_cast<const char*>(buffer.data() + gap_end), buffer.size() - gap_end);
            return res;
        }

        std::string get_range(size_type start, size_type end) const {
            if (start > end) std::swap(start, end);
            if (end > size()) end = size();
            std::string res; res.reserve(end - start);
            for (size_type i = start; i < end; ++i) res.push_back(get_char_at(i));
            return res;
        }

        CharT get_char_at(size_t index) const {
            if (index < gap_start) return buffer[index];
            return buffer[gap_end + (index - gap_start)];
        }

        size_type size() const { return buffer.size() - (gap_end - gap_start); }
        size_type get_cursor() const { return gap_start; }
        bool is_dirty() const { return dirty; }
        void set_dirty(bool d) { dirty = d; }

    private:
        std::vector<CharT> buffer;
        size_type gap_start;
        size_type gap_end;
        bool dirty = false;

        void expand_gap() {
            size_type old_size = buffer.size();
            size_type chunk_size = std::max(DEFAULT_GAP_SIZE, old_size / 2);
            buffer.resize(old_size + chunk_size);
            size_type post_gap = old_size - gap_end;
            std::copy_backward(buffer.begin() + gap_end, buffer.begin() + old_size, buffer.end());
            gap_end = buffer.size() - post_gap;
        }
    };
}
