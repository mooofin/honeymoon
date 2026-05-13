#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <dlfcn.h>
#include <string>
#include <string_view>
#include <vector>

namespace honeymoon::syntax {

enum class HighlightKind {
  None,
  Comment,
  String,
  Number,
  Keyword,
  Type,
  Function,
  Preprocessor,
};

extern "C" {
struct TSLanguage;
struct TSParser;
struct TSTree;
struct TSNode {
  uint32_t context[4];
  const void *id;
  const TSTree *tree;
};
}

class TreeSitterHighlighter {
public:
  TreeSitterHighlighter() = default;

  ~TreeSitterHighlighter() {
    if (tree_ && api_.ts_tree_delete)
      api_.ts_tree_delete(tree_);
    if (parser_ && api_.ts_parser_delete)
      api_.ts_parser_delete(parser_);
    if (lib_tree_sitter_)
      dlclose(lib_tree_sitter_);
    if (lib_lang_c_)
      dlclose(lib_lang_c_);
    if (lib_lang_cpp_)
      dlclose(lib_lang_cpp_);
  }

  TreeSitterHighlighter(const TreeSitterHighlighter &) = delete;
  TreeSitterHighlighter &operator=(const TreeSitterHighlighter &) = delete;

  bool set_language_for_file(const std::string &filename) {
    if (filename == last_filename_)
      return active_;
    last_filename_ = filename;

    LanguageMode requested = language_for_filename(filename);
    if (requested == LanguageMode::None) {
      active_ = false;
      mode_ = LanguageMode::None;
      last_content_.clear();
      style_map_.clear();
      if (tree_ && api_.ts_tree_delete) {
        api_.ts_tree_delete(tree_);
        tree_ = nullptr;
      }
      return false;
    }

    if (!ensure_parser()) {
      active_ = false;
      return false;
    }

    const TSLanguage *lang = load_language(requested);
    if (!lang || !api_.ts_parser_set_language(parser_, lang)) {
      active_ = false;
      return false;
    }

    if (mode_ != requested) {
      last_content_.clear();
      style_map_.clear();
      if (tree_ && api_.ts_tree_delete) {
        api_.ts_tree_delete(tree_);
        tree_ = nullptr;
      }
    }

    mode_ = requested;
    active_ = true;
    return true;
  }

  bool active() const noexcept { return active_; }

  void update(const std::string &content) {
    if (!active_ || !parser_)
      return;
    if (content == last_content_)
      return;

    TSTree *next =
        api_.ts_parser_parse_string(parser_, tree_, content.c_str(), content.size());
    if (!next)
      return;
    if (tree_ && api_.ts_tree_delete)
      api_.ts_tree_delete(tree_);
    tree_ = next;

    last_content_ = content;
    style_map_.assign(content.size(), HighlightKind::None);
    collect_styles();
  }

  HighlightKind kind_at(size_t byte_index) const noexcept {
    if (byte_index >= style_map_.size())
      return HighlightKind::None;
    return style_map_[byte_index];
  }

private:
  using ts_parser_new_fn              = TSParser *(*)();
  using ts_parser_delete_fn           = void (*)(TSParser *);
  using ts_parser_set_language_fn     = bool (*)(TSParser *, const TSLanguage *);
  using ts_parser_parse_string_fn     = TSTree *(*)(TSParser *, const TSTree *, const char *, uint32_t);
  using ts_tree_delete_fn             = void (*)(TSTree *);
  using ts_tree_root_node_fn          = TSNode (*)(const TSTree *);
  using ts_node_type_fn               = const char *(*)(TSNode);
  using ts_node_child_count_fn        = uint32_t (*)(TSNode);
  using ts_node_child_fn              = TSNode (*)(TSNode, uint32_t);
  using ts_node_start_byte_fn         = uint32_t (*)(TSNode);
  using ts_node_end_byte_fn           = uint32_t (*)(TSNode);
  using ts_node_is_null_fn            = bool (*)(TSNode);
  using tree_sitter_language_fn       = const TSLanguage *(*)();

  template <typename Fn>
  static Fn load_symbol(void *handle, const char *symbol) noexcept {
    return reinterpret_cast<Fn>(dlsym(handle, symbol));
  }

  struct Api {
    ts_parser_new_fn              ts_parser_new            = nullptr;
    ts_parser_delete_fn           ts_parser_delete         = nullptr;
    ts_parser_set_language_fn     ts_parser_set_language   = nullptr;
    ts_parser_parse_string_fn     ts_parser_parse_string   = nullptr;
    ts_tree_delete_fn             ts_tree_delete           = nullptr;
    ts_tree_root_node_fn          ts_tree_root_node        = nullptr;
    ts_node_type_fn               ts_node_type             = nullptr;
    ts_node_child_count_fn        ts_node_child_count      = nullptr;
    ts_node_child_fn              ts_node_child            = nullptr;
    ts_node_start_byte_fn         ts_node_start_byte       = nullptr;
    ts_node_end_byte_fn           ts_node_end_byte         = nullptr;
    ts_node_is_null_fn            ts_node_is_null          = nullptr;
  } api_;

  enum class LanguageMode { None, C, Cpp };

  struct Span {
    size_t start;
    size_t end;
    HighlightKind kind;
  };

  void *lib_tree_sitter_ = nullptr;
  void *lib_lang_c_      = nullptr;
  void *lib_lang_cpp_    = nullptr;
  const TSLanguage *lang_c_   = nullptr;
  const TSLanguage *lang_cpp_ = nullptr;

  TSParser *parser_ = nullptr;
  TSTree   *tree_   = nullptr;

  bool               active_     = false;
  bool               api_loaded_ = false;
  LanguageMode       mode_       = LanguageMode::None;
  std::string        last_filename_;
  std::string        last_content_;
  std::vector<HighlightKind> style_map_;

  static constexpr int priority(HighlightKind k) noexcept {
    switch (k) {
    case HighlightKind::Comment:     return 7;
    case HighlightKind::String:      return 6;
    case HighlightKind::Preprocessor: return 5;
    case HighlightKind::Function:    return 4;
    case HighlightKind::Type:        return 3;
    case HighlightKind::Keyword:     return 2;
    case HighlightKind::Number:      return 1;
    case HighlightKind::None:        return 0;
    }
    return 0;
  }

  static constexpr LanguageMode language_for_filename(std::string_view filename) noexcept {
    if (filename.ends_with(".c"))
      return LanguageMode::C;
    if (filename.ends_with(".cc")   || filename.ends_with(".cpp") ||
        filename.ends_with(".cxx")  || filename.ends_with(".hpp") ||
        filename.ends_with(".hh")   || filename.ends_with(".hxx") ||
        filename.ends_with(".h"))
      return LanguageMode::Cpp;
    return LanguageMode::None;
  }

  template <size_t N>
  static void *open_first(const std::array<const char *, N> &names) noexcept {
    for (const char *name : names) {
      if (!name)
        continue;
      void *h = dlopen(name, RTLD_NOW | RTLD_LOCAL);
      if (h)
        return h;
    }
    return nullptr;
  }

  bool load_api() {
    if (api_loaded_)
      return true;

    lib_tree_sitter_ = open_first(std::array<const char *, 4>{
        "libtree-sitter.so", "libtree-sitter.so.0",
        "libtree-sitter.so.0.22", nullptr});
    if (!lib_tree_sitter_)
      return false;

    api_.ts_parser_new          = load_symbol<ts_parser_new_fn>(lib_tree_sitter_, "ts_parser_new");
    api_.ts_parser_delete       = load_symbol<ts_parser_delete_fn>(lib_tree_sitter_, "ts_parser_delete");
    api_.ts_parser_set_language = load_symbol<ts_parser_set_language_fn>(lib_tree_sitter_, "ts_parser_set_language");
    api_.ts_parser_parse_string = load_symbol<ts_parser_parse_string_fn>(lib_tree_sitter_, "ts_parser_parse_string");
    api_.ts_tree_delete         = load_symbol<ts_tree_delete_fn>(lib_tree_sitter_, "ts_tree_delete");
    api_.ts_tree_root_node      = load_symbol<ts_tree_root_node_fn>(lib_tree_sitter_, "ts_tree_root_node");
    api_.ts_node_type           = load_symbol<ts_node_type_fn>(lib_tree_sitter_, "ts_node_type");
    api_.ts_node_child_count    = load_symbol<ts_node_child_count_fn>(lib_tree_sitter_, "ts_node_child_count");
    api_.ts_node_child          = load_symbol<ts_node_child_fn>(lib_tree_sitter_, "ts_node_child");
    api_.ts_node_start_byte     = load_symbol<ts_node_start_byte_fn>(lib_tree_sitter_, "ts_node_start_byte");
    api_.ts_node_end_byte       = load_symbol<ts_node_end_byte_fn>(lib_tree_sitter_, "ts_node_end_byte");
    api_.ts_node_is_null        = load_symbol<ts_node_is_null_fn>(lib_tree_sitter_, "ts_node_is_null");

    api_loaded_ = api_.ts_parser_new          && api_.ts_parser_delete       &&
                  api_.ts_parser_set_language  && api_.ts_parser_parse_string &&
                  api_.ts_tree_delete          && api_.ts_tree_root_node     &&
                  api_.ts_node_type            && api_.ts_node_child_count   &&
                  api_.ts_node_child           && api_.ts_node_start_byte    &&
                  api_.ts_node_end_byte        && api_.ts_node_is_null;
    return api_loaded_;
  }

  bool ensure_parser() {
    if (!load_api())
      return false;
    if (!parser_)
      parser_ = api_.ts_parser_new();
    return parser_ != nullptr;
  }

  template <size_t N>
  static const TSLanguage *load_language_lib(void *&lib,
                                              const std::array<const char *, N> &names,
                                              const char *sym,
                                              const TSLanguage *&cache) {
    if (cache)
      return cache;
    if (!lib) {
      lib = open_first(names);
    }
    if (!lib)
      return nullptr;
    auto fn = load_symbol<tree_sitter_language_fn>(lib, sym);
    if (!fn)
      return nullptr;
    cache = fn();
    return cache;
  }

  const TSLanguage *load_language(LanguageMode mode) {
    if (mode == LanguageMode::C) {
      return load_language_lib(
          lib_lang_c_,
          std::array<const char *, 4>{
              "libtree-sitter-c.so", "libtree-sitter-c.so.0",
              "tree-sitter-c.so", nullptr},
          "tree_sitter_c", lang_c_);
    }
    if (mode == LanguageMode::Cpp) {
      return load_language_lib(
          lib_lang_cpp_,
          std::array<const char *, 4>{
              "libtree-sitter-cpp.so", "libtree-sitter-cpp.so.0",
              "tree-sitter-cpp.so", nullptr},
          "tree_sitter_cpp", lang_cpp_);
    }
    return nullptr;
  }

  static HighlightKind classify_node(std::string_view node_type) noexcept {
    struct Rule {
      std::string_view pattern;
      HighlightKind kind;
      enum Match : uint8_t { Exact, Prefix, Substring } match;
    };

    static constexpr Rule rules[] = {
        {"comment",            HighlightKind::Comment,      Rule::Substring},
        {"string",             HighlightKind::String,       Rule::Substring},
        {"char_literal",       HighlightKind::String,       Rule::Substring},
        {"number",             HighlightKind::Number,       Rule::Substring},
        {"preproc_",           HighlightKind::Preprocessor, Rule::Prefix},
        {"type",               HighlightKind::Type,         Rule::Substring},
        {"if_statement",       HighlightKind::Keyword,      Rule::Exact},
        {"for_statement",      HighlightKind::Keyword,      Rule::Exact},
        {"while_statement",    HighlightKind::Keyword,      Rule::Exact},
        {"switch_statement",   HighlightKind::Keyword,      Rule::Exact},
        {"return_statement",   HighlightKind::Keyword,      Rule::Exact},
        {"break_statement",    HighlightKind::Keyword,      Rule::Exact},
        {"continue_statement", HighlightKind::Keyword,      Rule::Exact},
    };

    for (const Rule &r : rules) {
      bool matched = false;
      switch (r.match) {
      case Rule::Exact:     matched = (node_type == r.pattern);           break;
      case Rule::Prefix:    matched = (node_type.rfind(r.pattern, 0) == 0); break;
      case Rule::Substring: matched = (node_type.find(r.pattern) != std::string_view::npos); break;
      }
      if (matched)
        return r.kind;
    }

    if (node_type.find("function") != std::string_view::npos &&
        node_type.find("declaration") != std::string_view::npos)
      return HighlightKind::Function;

    return HighlightKind::None;
  }

  void collect_styles() {
    if (!tree_)
      return;

    TSNode root = api_.ts_tree_root_node(tree_);
    if (api_.ts_node_is_null(root))
      return;

    std::vector<Span> spans;
    std::vector<TSNode> stack;
    stack.push_back(root);

    while (!stack.empty()) {
      TSNode node = stack.back();
      stack.pop_back();
      if (api_.ts_node_is_null(node))
        continue;

      std::string_view node_type{api_.ts_node_type(node)};
      HighlightKind kind = classify_node(node_type);
      if (kind != HighlightKind::None) {
        size_t start = api_.ts_node_start_byte(node);
        size_t end   = api_.ts_node_end_byte(node);
        if (start < end) {
          spans.push_back({start, end, kind});
        }
      }

      uint32_t child_count = api_.ts_node_child_count(node);
      for (uint32_t i = 0; i < child_count; ++i) {
        stack.push_back(api_.ts_node_child(node, child_count - 1 - i));
      }
    }

    for (const auto &[start, end, kind] : spans) {
      size_t capped_end = std::min(end, style_map_.size());
      for (size_t i = start; i < capped_end; ++i) {
        if (priority(kind) >= priority(style_map_[i]))
          style_map_[i] = kind;
      }
    }
  }
};

} // namespace honeymoon::syntax
