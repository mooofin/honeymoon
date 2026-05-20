CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O3 -DNDEBUG -flto -fvisibility=hidden \
           -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
           -fmerge-all-constants -fno-unwind-tables -fno-asynchronous-unwind-tables \
           -fno-plt -fno-unroll-loops
LDFLAGS = -flto -s -Wl,--gc-sections -Wl,-O1 -Wl,--icf=all \
          -Wl,--hash-style=sysv -Wl,--build-id=none

SRC = src/main.cpp
TARGET = honeymoon

all: $(TARGET)

$(TARGET): $(SRC) src/*.hpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC) -ldl

clean:
	rm -f $(TARGET) test_harness test_harness.exe *.o src/*.o
