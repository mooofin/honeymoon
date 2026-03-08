load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "honeymoon",
    srcs = [
        "src/main.cpp",
        "src/editor.hpp",
        "src/buffer.hpp",
        "src/terminal.hpp",
        "src/input.hpp",
        "src/keybinder.hpp",
        "src/concepts.hpp",
        "src/history.hpp",
        "src/logo.hpp",
    ],
    copts = ["-std=c++23"],
    includes = ["src"],
)
