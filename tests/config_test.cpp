#include <gtest/gtest.h>
#include "config.hpp"
#include "test_utils.hpp"
#include <cstdlib>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using Config = honeymoon::config::Config;

// ── Config loading from file ──────────────────────────────────────────────

TEST(ConfigTest, LoadSetsDefaultsForMissingFile) {
    Config cfg;
    bool loaded = cfg.load();
    // With no config file found, load() should return false
    // but defaults should remain intact
    EXPECT_FALSE(loaded);
    EXPECT_TRUE(cfg.show_line_numbers);
    EXPECT_TRUE(cfg.syntax_highlighting);
    EXPECT_EQ(cfg.tab_width, 4);
}

TEST(ConfigTest, ParseTabWidth) {
    honeymoon::test::TempFile tf("tab_width 2\n");
    ASSERT_TRUE(tf);

    // Temporarily change cwd so find_path picks up our file
    // Instead: directly parse by setting up the config path
    // Actually, Config::load() reads from find_path() which searches XDG paths.
    // The easiest approach is to create ".honeymoonrc" in cwd.

    // Write a config to ./.honeymoonrc and test
    {
        FILE* f = fopen(".honeymoonrc", "w");
        ASSERT_TRUE(f);
        fprintf(f, "tab_width 8\n");
        fclose(f);
    }

    Config cfg;
    bool loaded = cfg.load();
    EXPECT_TRUE(loaded);
    EXPECT_EQ(cfg.tab_width, 8);

    // Clean up
    unlink(".honeymoonrc");
}

TEST(ConfigTest, ParseShowLineNumbers) {
    {
        FILE* f = fopen(".honeymoonrc", "w");
        ASSERT_TRUE(f);
        fprintf(f, "show_line_numbers false\n");
        fclose(f);
    }

    Config cfg;
    cfg.load();
    EXPECT_FALSE(cfg.show_line_numbers);

    unlink(".honeymoonrc");
}

TEST(ConfigTest, ParseSyntaxHighlighting) {
    {
        FILE* f = fopen(".honeymoonrc", "w");
        ASSERT_TRUE(f);
        fprintf(f, "syntax_highlighting false\n");
        fclose(f);
    }

    Config cfg;
    cfg.load();
    EXPECT_FALSE(cfg.syntax_highlighting);

    unlink(".honeymoonrc");
}

TEST(ConfigTest, IgnoresComments) {
    {
        FILE* f = fopen(".honeymoonrc", "w");
        ASSERT_TRUE(f);
        fprintf(f, "# this is a comment\ntab_width 2\n");
        fclose(f);
    }

    Config cfg;
    cfg.load();
    EXPECT_EQ(cfg.tab_width, 2);

    unlink(".honeymoonrc");
}

TEST(ConfigTest, IgnoresBlankLines) {
    {
        FILE* f = fopen(".honeymoonrc", "w");
        ASSERT_TRUE(f);
        fprintf(f, "\n\n\ntab_width 2\n\n");
        fclose(f);
    }

    Config cfg;
    cfg.load();
    EXPECT_EQ(cfg.tab_width, 2);

    unlink(".honeymoonrc");
}

// ── Config Save ───────────────────────────────────────────────────────────

TEST(ConfigTest, SaveAndReload) {
    // Create a temp dir with the required subdirectory structure.
    // save() writes to HOME/.config/honeymoon/config.moon but uses mkdir()
    // (not mkdir -p), so parents must exist.
    char tmpdir[] = "/tmp/honeymoon_save_XXXXXX";
    ASSERT_NE(mkdtemp(tmpdir), nullptr);

    mkdir((std::string(tmpdir) + "/.config").c_str(), 0755);
    std::string config_dir = std::string(tmpdir) + "/.config/honeymoon";
    mkdir(config_dir.c_str(), 0755);

    char old_home[4096] = {};
    char* home_env = getenv("HOME");
    if (home_env)
        snprintf(old_home, sizeof(old_home), "%s", home_env);
    setenv("HOME", tmpdir, 1);
    unsetenv("XDG_CONFIG_HOME");

    Config cfg;
    cfg.tab_width = 8;
    cfg.show_line_numbers = false;
    cfg.syntax_highlighting = false;

    bool saved = cfg.save();
    EXPECT_TRUE(saved);

    // Reload from a fresh config
    Config reload;
    bool loaded = reload.load();
    EXPECT_TRUE(loaded);
    EXPECT_EQ(reload.tab_width, 8);
    EXPECT_FALSE(reload.show_line_numbers);
    EXPECT_FALSE(reload.syntax_highlighting);

    // Clean up
    if (!old_home[0])
        unsetenv("HOME");
    else
        setenv("HOME", old_home, 1);
    std::string cfg_file = config_dir + "/config.moon";
    unlink(cfg_file.c_str());
    rmdir(config_dir.c_str());
    rmdir((std::string(tmpdir) + "/.config").c_str());
    rmdir(tmpdir);
}

// ── Helper methods ────────────────────────────────────────────────────────

TEST(ConfigTest, FileExists) {
    honeymoon::test::TempFile tf("content");
    ASSERT_TRUE(tf);

    EXPECT_TRUE(Config::file_exists(tf.path()));
    EXPECT_FALSE(Config::file_exists("/definitely/not/a/file/12345"));
}

TEST(ConfigTest, FindPathChecksCwdFirst) {
    {
        FILE* f = fopen(".honeymoonrc", "w");
        ASSERT_TRUE(f);
        fclose(f);
    }

    std::string path = Config::find_path();
    EXPECT_EQ(path, "./.honeymoonrc");

    unlink(".honeymoonrc");
}
