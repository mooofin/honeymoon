#pragma once

#include <cstdio>
#include <string>
#include <unistd.h>

namespace honeymoon::test {

// Creates a temporary file with the given content, returns the filename.
// The file is automatically unlinked (deleted when the object is destroyed).
class TempFile {
public:
    explicit TempFile(const std::string& content) {
        char path[] = "/tmp/honeymoon_test_XXXXXX";
        fd_ = mkstemp(path);
        if (fd_ >= 0) {
            path_ = path;
            write(fd_, content.data(), content.size());
            close(fd_);
        }
    }

    ~TempFile() {
        if (!path_.empty()) {
            unlink(path_.c_str());
        }
    }

    const std::string& path() const { return path_; }
    explicit operator bool() const { return !path_.empty(); }

private:
    int fd_ = -1;
    std::string path_;
};

} // namespace honeymoon::test
