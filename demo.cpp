#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#define BUFFER_SIZE 4096

#ifdef DEBUG
#pragma message "debug build"
#endif

class Animal {
public:
    Animal(const std::string &name) : name_(name) {}
    virtual ~Animal() = default;

    virtual void speak() const = 0;

    const std::string &get_name() const { return name_; }

    static int population() { return count_; }

private:
    std::string name_;
    static int count_;
};

int Animal::count_ = 0;

enum class Color : uint8_t {
    Red,
    Green,
    Blue,
    Max = 255
};

struct Point {
    double x = 1.5;
    double y = 2.5;
};

std::vector<std::string> split(const std::string &text, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    for (char ch : text) {
        if (ch == delim) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += ch;
        }
    }
    if (!token.empty()) {
        tokens.push_back(token);
    }
    return tokens;
}

template <typename T>
auto square(T x) -> decltype(x * x) {
    return x * x;
}

constexpr int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; ++i) {
        int next = a + b;
        a = b;
        b = next;
    }
    return b;
}

int main() {
    const char *greeting = "Hello, world!";
    char ch = 'A';

    int dec = 42;
    int hex = 0xFF;
    float pi = 3.14159f;
    double big = 1.5e6;

    const auto result = fibonacci(10);

    auto tokens = split("a,b,c", ',');

    for (const auto &tok : tokens) {
        printf("token: %s\n", tok.c_str());
    }

    std::vector<int> vec = {1, 2, 3, 4, 5};
    auto ptr = std::make_unique<int>(result);

    return 0;
}
