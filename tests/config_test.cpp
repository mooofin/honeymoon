#include <gtest/gtest.h>
#include "config.hpp"
#include "test_utils.hpp"
#include <cstdlib>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using Config = honeymoon::config::Config;

// Fixture that removes .honeymoonrc after each test even if an assertion fails.
// Also isolates HOME and XDG_CONFIG_HOME so tests don't accidentally pick up
// the developer's real config file.
class ConfigFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char* h = getenv("HOME");
        if (h) { old_home_ = h; had_home_ = true; }
        const char* x = getenv("XDG_CONFIG_HOME");
        if (x) { old_xdg_ = x; had_xdg_ = true; }
        // Point HOME somewhere that has no honeymoon config
        setenv("HOME", "/tmp", 1);
        unsetenv("XDG_CONFIG_HOME");
    }

    void TearDown() override {
        unlink(".honeymoonrc");
        if (had_home_) setenv("HOME", old_home_.c_str(), 1);
        else unsetenv("HOME");
        if (had_xdg_) setenv("XDG_CONFIG_HOME", old_xdg_.c_str(), 1);
        else unsetenv("XDG_CONFIG_HOME");
    }

private:
    std::string old_home_, old_xdg_;
    bool had_home_ = false, had_xdg_ = false;
};

// ── Config loading from file ──────────────────────────────────────────────

TEST_F(ConfigFileTest, LoadSetsDefaultsForMissingFile) {
    Config cfg;
    bool loaded = cfg.load();
    EXPECT_FALSE(loaded);
    EXPECT_TRUE(cfg.show_line_numbers);
    EXPECT_TRUE(cfg.syntax_highlighting);
    EXPECT_EQ(cfg.tab_width, 4);
}

TEST_F(ConfigFileTest, ParseTabWidth) {
    FILE* f = fopen(".honeymoonrc", "w");
    ASSERT_TRUE(f);
    fprintf(f, "tab_width 8\n");
    fclose(f);

    Config cfg;
    EXPECT_TRUE(cfg.load());
    EXPECT_EQ(cfg.tab_width, 8);
}

TEST_F(ConfigFileTest, ParseShowLineNumbers) {
    FILE* f = fopen(".honeymoonrc", "w");
    ASSERT_TRUE(f);
    fprintf(f, "show_line_numbers false\n");
    fclose(f);

    Config cfg;
    cfg.load();
    EXPECT_FALSE(cfg.show_line_numbers);
}

TEST_F(ConfigFileTest, ParseSyntaxHighlighting) {
    FILE* f = fopen(".honeymoonrc", "w");
    ASSERT_TRUE(f);
    fprintf(f, "syntax_highlighting false\n");
    fclose(f);

    Config cfg;
    cfg.load();
    EXPECT_FALSE(cfg.syntax_highlighting);
}

TEST_F(ConfigFileTest, IgnoresComments) {
    FILE* f = fopen(".honeymoonrc", "w");
    ASSERT_TRUE(f);
    fprintf(f, "# this is a comment\ntab_width 2\n");
    fclose(f);

    Config cfg;
    cfg.load();
    EXPECT_EQ(cfg.tab_width, 2);
}

TEST_F(ConfigFileTest, IgnoresBlankLines) {
    FILE* f = fopen(".honeymoonrc", "w");
    ASSERT_TRUE(f);
    fprintf(f, "\n\n\ntab_width 2\n\n");
    fclose(f);

    Config cfg;
    cfg.load();
    EXPECT_EQ(cfg.tab_width, 2);
}

TEST_F(ConfigFileTest, FindPathChecksCwdFirst) {
    FILE* f = fopen(".honeymoonrc", "w");
    ASSERT_TRUE(f);
    fclose(f);

    EXPECT_EQ(Config::find_path(), "./.honeymoonrc");
}

// ── Config Save ───────────────────────────────────────────────────────────

TEST(ConfigTest, SaveAndReload) {
    char tmpdir[] = "/tmp/honeymoon_save_XXXXXX";
    ASSERT_NE(mkdtemp(tmpdir), nullptr);

    mkdir((std::string(tmpdir) + "/.config").c_str(), 0755);
    std::string config_dir = std::string(tmpdir) + "/.config/honeymoon";
    mkdir(config_dir.c_str(), 0755);

    {
        honeymoon::test::ScopedEnv home("HOME", tmpdir);
        honeymoon::test::ScopedEnv xdg("XDG_CONFIG_HOME", nullptr);

        Config cfg;
        cfg.tab_width = 8;
        cfg.show_line_numbers = false;
        cfg.syntax_highlighting = false;

        EXPECT_TRUE(cfg.save());

        Config reload;
        EXPECT_TRUE(reload.load());
        EXPECT_EQ(reload.tab_width, 8);
        EXPECT_FALSE(reload.show_line_numbers);
        EXPECT_FALSE(reload.syntax_highlighting);
    }

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
