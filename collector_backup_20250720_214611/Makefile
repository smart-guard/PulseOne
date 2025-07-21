# PulseOne Collector 간단한 Makefile

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
TARGET = pulseone_collector

# 핵심 소스만
SOURCES = src/main.cpp \
          src/Core/CollectorApplication.cpp \
          src/Config/ConfigManager.cpp \
          src/Utils/LogManager.cpp

OBJECTS = $(SOURCES:.cpp=.o)
LIBS = -lpthread

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "🔗 Linking $(TARGET)..."
	$(CXX) -o $@ $^ $(LIBS)
	@echo "✅ Build successful!"

%.o: %.cpp
	@echo "🔨 Compiling: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run

# 디버그 빌드 (디버깅 로그 활성화)
debug: CXXFLAGS += -g -DDEBUG -O0
debug: clean $(TARGET)
	@echo "✅ Debug build completed - verbose logging enabled"

# 일반 빌드에서는 디버깅 로그 비활성화 (현재 상태 유지)
