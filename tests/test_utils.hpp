#pragma once

#include <cstdio>
#include <string>
#include <unistd.h>

namespace honeymoon::test {

class TempFile {
public:
    explicit TempFile(const std::string& content) {
        char path[] = "/tmp/honeymoon_test_XXXXXX";
        fd_ = mkstemp(path);
        if (fd_ < 0) return;
        path_ = path;
        if (!content.empty()) {
            ssize_t written = write(fd_, content.data(), content.size());
            if (written != static_cast<ssize_t>(content.size())) {
                close(fd_);
                unlink(path_.c_str());
                path_.clear();
                fd_ = -1;
                return;
            }
        }
        close(fd_);
        fd_ = -1;
    }

    ~TempFile() {
        if (!path_.empty())
            unlink(path_.c_str());
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    const std::string& path() const { return path_; }
    explicit operator bool() const { return !path_.empty(); }

private:
    int fd_ = -1;
    std::string path_;
};

// RAII wrapper that saves and restores an environment variable.
// Pass nullptr as value to unset the variable.
class ScopedEnv {
public:
    ScopedEnv(const char* name, const char* value) : name_(name) {
        const char* old = getenv(name);
        if (old) { old_value_ = old; had_value_ = true; }
        if (value) setenv(name, value, 1);
        else unsetenv(name);
    }
    ~ScopedEnv() {
        if (had_value_) setenv(name_.c_str(), old_value_.c_str(), 1);
        else unsetenv(name_.c_str());
    }
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;
private:
    std::string name_, old_value_;
    bool had_value_ = false;
};

} // namespace honeymoon::test
