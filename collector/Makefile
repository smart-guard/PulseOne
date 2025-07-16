# Makefile - PulseOne Collector 빌드 설정

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Iinclude
LDFLAGS = -lpqxx -lpq -lsqlite3
SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)
TARGET = bin/collector

all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p bin
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean