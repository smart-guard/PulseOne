CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

TARGET = main

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
