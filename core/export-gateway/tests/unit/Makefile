# =============================================================================
# Makefile for Circuit Breaker Unit Test
# =============================================================================

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# 경로 설정 (실제 프로젝트 구조에 맞게 조정)
INCLUDE_DIRS = -I../../include \
               -I../../../shared/include \
               -I../../../database/include

# 소스 파일 경로 (export-gateway 기준)
FAILUREPROTECTOR_SRC = ../../src/CSP/FailureProtector.cpp
LOGMANAGER_SRC = ../../../shared/src/Utils/LogManager.cpp

# 라이브러리
LIBS = -lpthread

# 빌드 디렉토리
BIN_DIR = bin
BUILD_DIR = build

# 타겟
TARGET = $(BIN_DIR)/test_circuit_breaker
TEST_SRC = test_circuit_breaker.cpp

.PHONY: all test clean check help

all: $(TARGET)

# 실제 빌드
$(TARGET): $(TEST_SRC) | $(BIN_DIR) $(BUILD_DIR)
	@echo "🔨 Building Circuit Breaker test..."
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) \
		$(TEST_SRC) \
		$(FAILUREPROTECTOR_SRC) \
		$(LOGMANAGER_SRC) \
		-o $(TARGET) $(LIBS)
	@echo "✅ Build complete: $(TARGET)"

# 디렉토리 생성
$(BIN_DIR) $(BUILD_DIR):
	@mkdir -p $@

# 빌드 & 실행
test: $(TARGET)
	@echo ""
	@echo "🧪 Running Circuit Breaker tests..."
	@echo ""
	@$(TARGET)

# 빠른 컴파일 & 실행 (임시 디렉토리)
quick: $(TEST_SRC)
	@echo "🚀 Quick compile & test..."
	@$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) \
		$(TEST_SRC) \
		$(FAILUREPROTECTOR_SRC) \
		$(LOGMANAGER_SRC) \
		-o /tmp/test_cb $(LIBS)
	@echo ""
	@/tmp/test_cb

# 파일 존재 확인
check:
	@echo "📁 Checking source files..."
	@echo -n "  FailureProtector.cpp: "
	@test -f $(FAILUREPROTECTOR_SRC) && echo "✅ Found" || echo "❌ Not found"
	@echo -n "  LogManager.cpp: "
	@test -f $(LOGMANAGER_SRC) && echo "✅ Found" || echo "❌ Not found"
	@echo -n "  test_circuit_breaker.cpp: "
	@test -f $(TEST_SRC) && echo "✅ Found" || echo "❌ Not found"

# 정리
clean:
	@rm -rf $(BIN_DIR) $(BUILD_DIR)
	@rm -f /tmp/test_cb
	@echo "✅ Clean complete"

# 도움말
help:
	@echo "Usage:"
	@echo "  make        - Build test"
	@echo "  make test   - Build and run test"
	@echo "  make quick  - Quick compile & run"
	@echo "  make check  - Check if source files exist"
	@echo "  make clean  - Clean"