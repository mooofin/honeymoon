CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

SRC = src/main.cpp
TARGET = honeymoon

all: $(TARGET)

$(TARGET): $(SRC) src/*.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
