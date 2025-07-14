# OS 감지 및 컴파일러 설정
ifeq ($(OS),Windows_NT)
    TARGET = collector.exe
    CXX = g++
    CXXFLAGS = -Wall -O2
    RM = del /Q
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        TARGET = collector
        CXX = g++
        CXXFLAGS = -Wall -O2
        RM = rm -f
    else ifeq ($(UNAME_S),Darwin)
        TARGET = collector
        CXX = clang++
        CXXFLAGS = -Wall -O2
        RM = rm -f
    endif
endif

# 디렉토리 및 파일 설정
SRC_DIR = src
BIN_DIR = bin
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:.cpp=.o)
EXECUTABLE = $(BIN_DIR)/$(TARGET)

# 빌드 타겟
build: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $(EXECUTABLE) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(SRC_DIR)/*.o
	@rm -rf $(BIN_DIR)

.PHONY: all build clean
