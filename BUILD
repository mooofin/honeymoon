load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

package(default_visibility = ["//visibility:public"])

# Core library - all the editor logic as a library (enables testing, reuse)
cc_library(
    name = "honeymoon_lib",
    srcs = [],
    hdrs = [
        "src/buffer.hpp",
        "src/concepts.hpp",
        "src/editor.hpp",
        "src/history.hpp",
        "src/input.hpp",
        "src/keybinder.hpp",
        "src/logo.hpp",
        "src/terminal.hpp",
    ],
    includes = ["src"],
)

# Main executable
cc_binary(
    name = "honeymoon",
    srcs = ["src/main.cpp"],
    deps = [":honeymoon_lib"],
)
