# =============================================================================
# PulseOne Collector Makefile - 새 폴더 구조 지원
# =============================================================================
# nlohmann/json 라이브러리 체크
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")

# 컴파일러 설정
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -DPULSEONE_DEBUG_MODE -O0 -Iinclude $(shell pkg-config --cflags libmodbus)
LDFLAGS = 
LIBS = -lpthread $(shell pkg-config --libs libmodbus) -lpqxx -lpq -lsqlite3 -lpaho-mqtt3c -lpaho-mqttpp3 -lbacnet

# 기존 CXXFLAGS에 JSON 플래그 추가
ifeq ($(HAS_NLOHMANN_JSON),1)
    CXXFLAGS += -DHAS_NLOHMANN_JSON
endif


# 프로젝트 설정
PROJECT_NAME = pulseone_collector
TARGET = $(PROJECT_NAME)

# 디렉토리 설정
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# Include 경로
INCLUDES = -I$(INCLUDE_DIR)

# 색상 정의
GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m

# =============================================================================
# 소스 파일 그룹 (새 폴더 구조 기반)
# =============================================================================

# Core 소스들
CORE_SOURCES := $(wildcard $(SRC_DIR)/Core/*.cpp)

# Workers 소스들
WORKERS_BASE_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp)
WORKERS_PROTOCOL_SOURCES := $(wildcard $(SRC_DIR)/Workers/Protocol/*.cpp)
WORKERS_COMPONENTS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Components/*.cpp)
WORKERS_SOURCES := $(WORKERS_BASE_SOURCES) $(WORKERS_PROTOCOL_SOURCES) $(WORKERS_COMPONENTS_SOURCES)

# Drivers 소스들
DRIVERS_COMMON_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Common/*.cpp)
DRIVERS_MODBUS_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Modbus/*.cpp)
DRIVERS_MQTT_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Mqtt/*.cpp)
DRIVERS_BACNET_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Bacnet/*.cpp)
DRIVERS_SOURCES := $(DRIVERS_COMMON_SOURCES) $(DRIVERS_MODBUS_SOURCES) $(DRIVERS_MQTT_SOURCES) $(DRIVERS_BACNET_SOURCES)

# Database 소스들
DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp)

# Client 소스들
CLIENT_SOURCES := $(wildcard $(SRC_DIR)/Client/*.cpp)

# Utils 소스들
UTILS_SOURCES := $(wildcard $(SRC_DIR)/Utils/*.cpp)

# Plugin 소스들
PLUGIN_SOURCES := $(wildcard $(SRC_DIR)/Plugin/*.cpp)

# 메인 소스
MAIN_SOURCE = $(SRC_DIR)/main.cpp

# 전체 소스들 (main 제외)
ALL_LIB_SOURCES = $(CORE_SOURCES) $(WORKERS_SOURCES) $(DRIVERS_SOURCES) \
                  $(DATABASE_SOURCES) $(CLIENT_SOURCES) $(UTILS_SOURCES) $(PLUGIN_SOURCES)

# 오브젝트 파일들
ALL_LIB_OBJECTS = $(ALL_LIB_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
MAIN_OBJECT = $(MAIN_SOURCE:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
ALL_OBJECTS = $(ALL_LIB_OBJECTS) $(MAIN_OBJECT)

# =============================================================================
# 메인 타겟들
# =============================================================================

.DEFAULT_GOAL := all

# 전체 빌드
all: directories $(TARGET)
	@echo -e "$(GREEN)🎉 Build completed successfully!$(NC)"
	@echo -e "$(BLUE)📦 Executable: $(BIN_DIR)/$(TARGET)$(NC)"
	@echo -e "$(BLUE)📊 Object files: $(words $(ALL_OBJECTS))$(NC)"

# 실행 파일 생성
$(TARGET): $(ALL_OBJECTS) | $(BIN_DIR)
	@echo -e "$(BLUE)🔗 Linking $(TARGET)...$(NC)"
	@echo -e "$(YELLOW)   Libraries: $(LIBS)$(NC)"
	$(CXX) $(LDFLAGS) -o $(BIN_DIR)/$@ $(ALL_OBJECTS) $(LIBS)

# =============================================================================
# 오브젝트 파일 생성 규칙 (카테고리별 색상)
# =============================================================================

# Core 파일들
$(BUILD_DIR)/Core/%.o: $(SRC_DIR)/Core/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(GREEN)🏛️  Compiling Core: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Workers 파일들
$(BUILD_DIR)/Workers/%.o: $(SRC_DIR)/Workers/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(BLUE)👷 Compiling Worker: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Drivers 파일들
$(BUILD_DIR)/Drivers/%.o: $(SRC_DIR)/Drivers/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)🔌 Compiling Driver: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Database 파일들
$(BUILD_DIR)/Database/%.o: $(SRC_DIR)/Database/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(GREEN)🗄️  Compiling Database: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Client 파일들
$(BUILD_DIR)/Client/%.o: $(SRC_DIR)/Client/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(BLUE)📡 Compiling Client: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Utils 파일들
$(BUILD_DIR)/Utils/%.o: $(SRC_DIR)/Utils/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(GREEN)🔧 Compiling Utils: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Plugin 파일들
$(BUILD_DIR)/Plugin/%.o: $(SRC_DIR)/Plugin/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)🔌 Compiling Plugin: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 메인 파일
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(BLUE)🚀 Compiling Main: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 유틸리티 타겟들
# =============================================================================

# 디렉토리 생성
directories:
	@mkdir -p $(BUILD_DIR)/{Core,Workers/{Base,Protocol,Components},Drivers/{Common,Modbus,Mqtt,Bacnet},Database,Client,Utils,Plugin}
	@mkdir -p $(BIN_DIR)

# 정리
clean:
	@echo -e "$(YELLOW)🧹 Cleaning build files...$(NC)"
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo -e "$(GREEN)✅ Clean completed$(NC)"

# 실행
run: $(TARGET)
	@echo -e "$(BLUE)🚀 Running $(TARGET)...$(NC)"
	cd $(BIN_DIR) && ./$(TARGET)

# =============================================================================
# 빌드 모드별 타겟들
# =============================================================================

# 디버그 빌드
debug: CXXFLAGS += -g -DPULSEONE_DEBUG_MODE -O0
debug: CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
debug: check-json clean all
	@echo -e "$(GREEN)🐛 Debug build completed - verbose logging enabled$(NC)"
# 릴리즈 빌드
release: CXXFLAGS += -O3 -DNDEBUG
release: CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
release: clean all
	@echo -e "$(GREEN)🚀 Release build completed - optimized for performance$(NC)"

# 최소 빌드 (핵심 파일만)
minimal: MINIMAL_SOURCES = $(SRC_DIR)/main.cpp $(SRC_DIR)/Core/Application.cpp $(SRC_DIR)/Utils/LogManager.cpp $(SRC_DIR)/Utils/ConfigManager.cpp
minimal: MINIMAL_OBJECTS = $(MINIMAL_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
minimal: directories $(MINIMAL_OBJECTS)
	@echo -e "$(BLUE)🔗 Linking minimal build...$(NC)"
	$(CXX) $(LDFLAGS) -o $(BIN_DIR)/$(TARGET)_minimal $(MINIMAL_OBJECTS) $(LIBS)
	@echo -e "$(GREEN)✅ Minimal build completed: $(BIN_DIR)/$(TARGET)_minimal$(NC)"

# =============================================================================
# 개발 도구들
# =============================================================================

# JSON 라이브러리 체크
check-json:
	@echo "🔍 Checking nlohmann/json..."
	@if [ "$(HAS_NLOHMANN_JSON)" = "0" ]; then \
		echo "⚠️  nlohmann/json not found - using fallback"; \
		echo "   Install: sudo apt-get install nlohmann-json3-dev"; \
	else \
		echo "✅ nlohmann/json found"; \
	fi
	
# 코드 분석
analyze:
	@echo -e "$(BLUE)📊 Analyzing code structure...$(NC)"
	@echo -e "$(GREEN)Core files:$(NC) $(words $(CORE_SOURCES))"
	@echo -e "$(BLUE)Worker files:$(NC) $(words $(WORKERS_SOURCES))"
	@echo -e "$(YELLOW)Driver files:$(NC) $(words $(DRIVERS_SOURCES))"
	@echo -e "$(GREEN)Database files:$(NC) $(words $(DATABASE_SOURCES))"
	@echo -e "$(BLUE)Client files:$(NC) $(words $(CLIENT_SOURCES))"
	@echo -e "$(GREEN)Utils files:$(NC) $(words $(UTILS_SOURCES))"
	@echo -e "$(YELLOW)Total source files:$(NC) $(words $(ALL_LIB_SOURCES))"

# 헤더 의존성 체크
check-headers:
	@echo -e "$(BLUE)🔍 Checking header dependencies...$(NC)"
	@find $(INCLUDE_DIR) -name "*.h" -exec echo "📄 {}" \;

# 빠른 문법 체크 (컴파일만, 링크 안함)
syntax-check:
	@echo -e "$(BLUE)📝 Syntax checking...$(NC)"
	@for src in $(ALL_LIB_SOURCES) $(MAIN_SOURCE); do \
		echo -e "$(YELLOW)Checking: $$src$(NC)"; \
		$(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only $$src || exit 1; \
	done
	@echo -e "$(GREEN)✅ All syntax checks passed$(NC)"

# 개발 환경 정보
info:
	@echo -e "$(BLUE)📋 Build Information$(NC)"
	@echo -e "$(GREEN)Compiler:$(NC) $(CXX)"
	@echo -e "$(GREEN)Flags:$(NC) $(CXXFLAGS)"
	@echo -e "$(GREEN)Includes:$(NC) $(INCLUDES)"
	@echo -e "$(GREEN)Libraries:$(NC) $(LIBS)"
	@echo -e "$(GREEN)Target:$(NC) $(BIN_DIR)/$(TARGET)"

# 설치 (시스템 디렉토리로 복사)
install: $(TARGET)
	@echo -e "$(BLUE)📦 Installing $(TARGET)...$(NC)"
	sudo cp $(BIN_DIR)/$(TARGET) /usr/local/bin/
	@echo -e "$(GREEN)✅ Installation completed: /usr/local/bin/$(TARGET)$(NC)"

# =============================================================================
# PHONY 타겟들
# =============================================================================

.PHONY: all clean run debug release minimal analyze check-headers syntax-check info install directories

# =============================================================================
# 도움말
# =============================================================================

help:
	@echo -e "$(BLUE)🔧 PulseOne Collector Build System$(NC)"
	@echo ""
	@echo -e "$(GREEN)Main targets:$(NC)"
	@echo "  all          - Build the complete project"
	@echo "  clean        - Remove all build files"
	@echo "  run          - Build and run the application"
	@echo ""
	@echo -e "$(GREEN)Build modes:$(NC)"
	@echo "  debug        - Build with debug symbols and verbose logging"
	@echo "  release      - Build optimized for production"
	@echo "  minimal      - Build only core components"
	@echo ""
	@echo -e "$(GREEN)Development tools:$(NC)"
	@echo "  analyze      - Analyze code structure"
	@echo "  syntax-check - Check syntax without linking"
	@echo "  check-headers - Check header dependencies"
	@echo "  info         - Show build configuration"
	@echo "  install      - Install to system directory"
	@echo ""
	@echo -e "$(GREEN)Examples:$(NC)"
	@echo "  make debug   - Build debug version"
	@echo "  make clean && make release - Clean rebuild for production"
	@echo "  make syntax-check - Quick syntax validation"

# =============================================================================
# PulseOne Collector Makefile - 테스트 타겟 추가
# 기존 Makefile 끝부분에 이 내용을 추가하세요
# =============================================================================

# 테스트 디렉토리 설정
TEST_DIR = test
TEST_BUILD_DIR = build/test
TEST_BIN_DIR = bin/test

# 테스트 소스 파일들
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_DIR)/%.cpp=$(TEST_BUILD_DIR)/%.o)
TEST_TARGETS = $(TEST_SOURCES:$(TEST_DIR)/test_%.cpp=$(TEST_BIN_DIR)/test_%)

# 테스트용 라이브러리 (메인 앱과 동일하지만 main.o 제외)
TEST_LIB_OBJECTS = $(filter-out $(BUILD_DIR)/main.o,$(ALL_OBJECTS))

# =============================================================================
# 테스트 타겟들
# =============================================================================

.PHONY: tests test-mqtt test-all test-clean test-run

# 모든 테스트 빌드
tests: directories $(TEST_TARGETS)
	@echo -e "$(GREEN)✅ All tests built successfully$(NC)"
	@echo -e "$(CYAN)📋 Available test executables:$(NC)"
	@for test in $(TEST_TARGETS); do \
		if [ -f "$$test" ]; then \
			echo -e "   $(GREEN)✅ $$test$(NC)"; \
		fi \
	done

# MQTT 드라이버 테스트 빌드
test-mqtt: directories $(TEST_BIN_DIR)/test_mqtt_driver
	@echo -e "$(GREEN)✅ MQTT Driver test built: $(TEST_BIN_DIR)/test_mqtt_driver$(NC)"

# 개별 테스트 빌드 규칙
$(TEST_BIN_DIR)/test_%: $(TEST_BUILD_DIR)/test_%.o $(TEST_LIB_OBJECTS)
	@mkdir -p $(TEST_BIN_DIR)
	@echo -e "$(BLUE)🔗 Linking test: $@$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $< $(TEST_LIB_OBJECTS) $(LIBS)
	@chmod +x $@

# 테스트 오브젝트 파일 컴파일
$(TEST_BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(TEST_BUILD_DIR)
	@echo -e "$(YELLOW)🧪 Compiling test: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 모든 테스트 실행
test-run: tests
	@echo -e "$(CYAN)🧪 Running all tests...$(NC)"
	@echo -e "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@passed=0; total=0; \
	for test in $(TEST_TARGETS); do \
		if [ -f "$$test" ]; then \
			echo -e "$(YELLOW)🏃 Running: $$test$(NC)"; \
			total=$$((total + 1)); \
			if $$test; then \
				echo -e "$(GREEN)✅ PASSED: $$test$(NC)"; \
				passed=$$((passed + 1)); \
			else \
				echo -e "$(RED)❌ FAILED: $$test$(NC)"; \
			fi; \
			echo ""; \
		fi \
	done; \
	echo -e "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"; \
	echo -e "$(CYAN)📊 Test Results: $$passed/$$total passed$(NC)"; \
	if [ $$passed -eq $$total ]; then \
		echo -e "$(GREEN)🎉 All tests passed!$(NC)"; \
	else \
		echo -e "$(RED)⚠️  Some tests failed$(NC)"; \
	fi

# MQTT 테스트만 실행
test-mqtt-run: $(TEST_BIN_DIR)/test_mqtt_driver
	@echo -e "$(CYAN)🧪 Running MQTT Driver test...$(NC)"
	@echo -e "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	$(TEST_BIN_DIR)/test_mqtt_driver

# 테스트 정리
test-clean:
	@echo -e "$(YELLOW)🧹 Cleaning test files...$(NC)"
	rm -rf $(TEST_BUILD_DIR) $(TEST_BIN_DIR)
	@echo -e "$(GREEN)✅ Test clean completed$(NC)"

# 테스트 디렉토리 생성 (기존 directories 타겟에 추가)
test-directories:
	@mkdir -p $(TEST_DIR)
	@mkdir -p $(TEST_BUILD_DIR)
	@mkdir -p $(TEST_BIN_DIR)

# =============================================================================
# 전체 정리 타겟 (기존 clean에 추가)
# =============================================================================

# 전체 정리 (기존 clean 타겟을 확장)
clean-all: clean test-clean
	@echo -e "$(GREEN)✅ Complete clean finished$(NC)"

# =============================================================================
# 헬프 업데이트 (기존 help 타겟에 추가)
# =============================================================================

test-help:
	@echo -e "$(CYAN)🧪 PulseOne Test System$(NC)"
	@echo -e "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo -e "$(YELLOW)Test targets:$(NC)"
	@echo -e "  $(GREEN)tests$(NC)          - Build all tests"
	@echo -e "  $(GREEN)test-mqtt$(NC)      - Build MQTT driver test only"
	@echo -e "  $(GREEN)test-run$(NC)       - Build and run all tests"
	@echo -e "  $(GREEN)test-mqtt-run$(NC)  - Build and run MQTT test only"
	@echo -e "  $(GREEN)test-clean$(NC)     - Clean test build files"
	@echo -e "  $(GREEN)test-help$(NC)      - Show this help"
	@echo -e ""
	@echo -e "$(YELLOW)Usage examples:$(NC)"
	@echo -e "  make tests                    # Build all tests"
	@echo -e "  make test-mqtt-run           # Quick MQTT test"
	@echo -e "  make test-run                # Build and run all tests"
	@echo -e "  make clean-all               # Clean everything including tests"
	@echo -e "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"

# BACnet 설정 추가
BACNET_CFLAGS = $(shell pkg-config --cflags bacnet)
BACNET_LIBS = $(shell pkg-config --libs bacnet)

CXXFLAGS += $(BACNET_CFLAGS)
LIBS += $(BACNET_LIBS)

