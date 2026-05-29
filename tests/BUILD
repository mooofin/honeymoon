load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//visibility:public"])

# Test utilities helper
cc_library(
    name = "test_utils",
    hdrs = ["test_utils.hpp"],
    deps = ["//:honeymoon_lib"],
)

# Buffer tests (GapBuffer)
cc_test(
    name = "buffer_test",
    srcs = ["buffer_test.cpp"],
    deps = [
        ":test_utils",
        "//:honeymoon_lib",
        "@googletest//:gtest_main",
    ],
)

# Undo/Redo tests
cc_test(
    name = "undo_test",
    srcs = ["undo_test.cpp"],
    deps = [
        "//:honeymoon_lib",
        "@googletest//:gtest_main",
    ],
)

# Input/Key utility tests
cc_test(
    name = "input_test",
    srcs = ["input_test.cpp"],
    deps = [
        "//:honeymoon_lib",
        "@googletest//:gtest_main",
    ],
)

# Config parsing tests
cc_test(
    name = "config_test",
    srcs = ["config_test.cpp"],
    deps = [
        ":test_utils",
        "//:honeymoon_lib",
        "@googletest//:gtest_main",
    ],
)

# KeyBinder parsing tests
cc_test(
    name = "keybinder_test",
    srcs = ["keybinder_test.cpp"],
    deps = [
        ":test_utils",
        "//:honeymoon_lib",
        "@googletest//:gtest_main",
    ],
)
