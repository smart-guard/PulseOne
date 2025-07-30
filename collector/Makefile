# =============================================================================
# PulseOne Collector Makefile - 수정된 버전 (CollectorApplication 포함)
# =============================================================================

# nlohmann/json 라이브러리 체크
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")

# 컴파일러 설정
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -DPULSEONE_DEBUG_MODE -O0 -Iinclude $(shell pkg-config --cflags libmodbus)
LDFLAGS = 
LIBS = -lpthread $(shell pkg-config --libs libmodbus) -lpqxx -lpq -lsqlite3 -lpaho-mqtt3c -lpaho-mqttpp3 -lbacnet -lmysqlclient

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
# 🔧 수정: 올바른 소스 파일 그룹 정의
# =============================================================================

# Core 소스들 (수정: Application.cpp 올바른 경로)
CORE_SOURCES := $(wildcard $(SRC_DIR)/Core/*.cpp)

# 🔧 DEBUG: Core 소스 확인을 위한 출력
$(info 🔍 DEBUG: CORE_SOURCES = $(CORE_SOURCES))

# 기존 Utils, Config 등 (확실히 존재하는 것들)
UTILS_SOURCES := $(wildcard $(SRC_DIR)/Utils/*.cpp)
CONFIG_SOURCES := $(wildcard $(SRC_DIR)/Config/*.cpp)

# 🔧 DEBUG: 기존 소스들 확인
$(info 🔍 DEBUG: UTILS_SOURCES = $(UTILS_SOURCES))
$(info 🔍 DEBUG: CONFIG_SOURCES = $(CONFIG_SOURCES))

# Workers 소스들 (아직 없을 수 있음)
WORKERS_BASE_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp)
WORKERS_PROTOCOL_SOURCES := $(wildcard $(SRC_DIR)/Workers/Protocol/*.cpp)
WORKERS_COMPONENTS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Components/*.cpp)
WORKERS_SOURCES := $(WORKERS_BASE_SOURCES) $(WORKERS_PROTOCOL_SOURCES) $(WORKERS_COMPONENTS_SOURCES) $(wildcard $(SRC_DIR)/Workers/WorkerFactory.cpp)

# Drivers 소스들 (아직 없을 수 있음)
DRIVERS_COMMON_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Common/*.cpp)
DRIVERS_MODBUS_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Modbus/*.cpp)
DRIVERS_MQTT_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Mqtt/*.cpp)
DRIVERS_BACNET_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Bacnet/*.cpp)
DRIVERS_SOURCES := $(DRIVERS_COMMON_SOURCES) $(DRIVERS_MODBUS_SOURCES) $(DRIVERS_MQTT_SOURCES) $(DRIVERS_BACNET_SOURCES)

# Database 소스들 (있을 수도 있음)
DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Entities/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)

# Client 소스들 (있을 수도 있음)
CLIENT_SOURCES := $(wildcard $(SRC_DIR)/Client/*.cpp)

# Plugin 소스들 (아직 없을 수 있음)
PLUGIN_SOURCES := $(wildcard $(SRC_DIR)/Plugin/*.cpp)

# 메인 소스
MAIN_SOURCE = $(SRC_DIR)/main.cpp

# 🔧 수정: 확실히 존재하는 소스들만 포함
EXISTING_SOURCES = $(CORE_SOURCES) $(UTILS_SOURCES) $(CONFIG_SOURCES)
OPTIONAL_SOURCES = $(WORKERS_SOURCES) $(DRIVERS_SOURCES) $(DATABASE_SOURCES) $(CLIENT_SOURCES) $(PLUGIN_SOURCES)

# 전체 소스들 (main 제외, 존재하는 것들만)
ALL_LIB_SOURCES = $(EXISTING_SOURCES) $(OPTIONAL_SOURCES)

# 🔧 DEBUG: 최종 소스 목록 확인
$(info 🔍 DEBUG: ALL_LIB_SOURCES = $(ALL_LIB_SOURCES))

# 오브젝트 파일들
ALL_LIB_OBJECTS = $(ALL_LIB_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
MAIN_OBJECT = $(MAIN_SOURCE:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
ALL_OBJECTS = $(ALL_LIB_OBJECTS) $(MAIN_OBJECT)

# =============================================================================
# 메인 타겟들
# =============================================================================

.DEFAULT_GOAL := all

# 🔧 수정: 파일 존재 확인 추가
check-sources:
	@echo -e "$(BLUE)🔍 Checking required source files...$(NC)"
	@echo -e "$(YELLOW)Main source:$(NC)"
	@if [ -f "$(MAIN_SOURCE)" ]; then \
		echo -e "  $(GREEN)✅ $(MAIN_SOURCE)$(NC)"; \
	else \
		echo -e "  $(RED)❌ $(MAIN_SOURCE) (MISSING)$(NC)"; \
		exit 1; \
	fi
	@echo -e "$(YELLOW)Core sources:$(NC)"
	@if [ -z "$(CORE_SOURCES)" ]; then \
		echo -e "  $(RED)❌ No Core sources found in $(SRC_DIR)/Core/$(NC)"; \
		echo -e "  $(YELLOW)Expected: $(SRC_DIR)/Core/Application.cpp$(NC)"; \
		ls -la $(SRC_DIR)/Core/ 2>/dev/null || echo "  $(RED)❌ $(SRC_DIR)/Core/ directory not found$(NC)"; \
		exit 1; \
	else \
		for src in $(CORE_SOURCES); do \
			echo -e "  $(GREEN)✅ $$src$(NC)"; \
		done \
	fi
	@echo -e "$(YELLOW)Utils/Config sources:$(NC)"
	@for src in $(UTILS_SOURCES) $(CONFIG_SOURCES); do \
		if [ -f "$$src" ]; then \
			echo -e "  $(GREEN)✅ $$src$(NC)"; \
		else \
			echo -e "  $(RED)❌ $$src (MISSING)$(NC)"; \
		fi \
	done

# 전체 빌드 (소스 확인 포함)
all: check-sources directories $(TARGET)
	@echo -e "$(GREEN)🎉 Build completed successfully!$(NC)"
	@echo -e "$(BLUE)📦 Executable: $(BIN_DIR)/$(TARGET)$(NC)"
	@echo -e "$(BLUE)📊 Object files: $(words $(ALL_OBJECTS))$(NC)"

# 실행 파일 생성
$(TARGET): $(ALL_OBJECTS) | $(BIN_DIR)
	@echo -e "$(BLUE)🔗 Linking $(TARGET)...$(NC)"
	@echo -e "$(YELLOW)   Objects: $(words $(ALL_OBJECTS))$(NC)"
	@echo -e "$(YELLOW)   Libraries: $(LIBS)$(NC)"
	$(CXX) $(LDFLAGS) -o $(BIN_DIR)/$@ $(ALL_OBJECTS) $(LIBS)

# =============================================================================
# 🔧 수정: 더 정확한 오브젝트 파일 생성 규칙
# =============================================================================

# Core 파일들 (수정된 경로)
$(BUILD_DIR)/Core/%.o: $(SRC_DIR)/Core/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(GREEN)🏛️  Compiling Core: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Utils 파일들
$(BUILD_DIR)/Utils/%.o: $(SRC_DIR)/Utils/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(GREEN)🔧 Compiling Utils: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Config 파일들 (있다면)
$(BUILD_DIR)/Config/%.o: $(SRC_DIR)/Config/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(BLUE)⚙️  Compiling Config: $<$(NC)"
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
	@mkdir -p $(BUILD_DIR)/{Core,Workers/{Base,Protocol,Components},Drivers/{Common,Modbus,Mqtt,Bacnet},Database,Client,Utils,Config,Plugin}
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
# 🔧 수정: 올바른 최소 빌드
# =============================================================================

# 최소 빌드 (핵심 파일만, 올바른 파일명 사용)
minimal: MINIMAL_SOURCES = $(SRC_DIR)/main.cpp $(SRC_DIR)/Core/Application.cpp $(SRC_DIR)/Utils/LogManager.cpp $(SRC_DIR)/Utils/ConfigManager.cpp
minimal: MINIMAL_OBJECTS = $(MINIMAL_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
minimal: directories
	@echo -e "$(BLUE)🔍 Checking minimal build sources...$(NC)"
	@for src in $(MINIMAL_SOURCES); do \
		if [ -f "$$src" ]; then \
			echo -e "  $(GREEN)✅ $$src$(NC)"; \
		else \
			echo -e "  $(RED)❌ $$src (MISSING)$(NC)"; \
			echo -e "  $(YELLOW)Available files in $$(dirname $$src):$(NC)"; \
			ls -la $$(dirname $$src) 2>/dev/null || echo "    $(RED)Directory not found$(NC)"; \
			exit 1; \
		fi \
	done
	@$(MAKE) $(MINIMAL_OBJECTS)
	@echo -e "$(BLUE)🔗 Linking minimal build...$(NC)"
	$(CXX) $(LDFLAGS) -o $(BIN_DIR)/$(TARGET)_minimal $(MINIMAL_OBJECTS) $(LIBS)
	@echo -e "$(GREEN)✅ Minimal build completed: $(BIN_DIR)/$(TARGET)_minimal$(NC)"

# 빌드 모드별 타겟들
debug: CXXFLAGS += -g -DPULSEONE_DEBUG_MODE -O0
debug: CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
debug: check-json clean all
	@echo -e "$(GREEN)🐛 Debug build completed - verbose logging enabled$(NC)"

release: CXXFLAGS += -O3 -DNDEBUG  
release: CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
release: clean all
	@echo -e "$(GREEN)🚀 Release build completed - optimized for performance$(NC)"

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
	@echo -e "$(GREEN)Utils files:$(NC) $(words $(UTILS_SOURCES))"  
	@echo -e "$(BLUE)Config files:$(NC) $(words $(CONFIG_SOURCES))"
	@echo -e "$(BLUE)Worker files:$(NC) $(words $(WORKERS_SOURCES))"
	@echo -e "$(YELLOW)Driver files:$(NC) $(words $(DRIVERS_SOURCES))"
	@echo -e "$(GREEN)Database files:$(NC) $(words $(DATABASE_SOURCES))"
	@echo -e "$(BLUE)Client files:$(NC) $(words $(CLIENT_SOURCES))"
	@echo -e "$(YELLOW)Plugin files:$(NC) $(words $(PLUGIN_SOURCES))"
	@echo -e "$(YELLOW)Total source files:$(NC) $(words $(ALL_LIB_SOURCES))"

# 파일 구조 확인
check-structure:
	@echo -e "$(BLUE)📁 Checking project structure...$(NC)"
	@echo -e "$(YELLOW)Source directories:$(NC)"
	@find $(SRC_DIR) -type d | sed 's/^/  /'
	@echo -e "$(YELLOW)Source files:$(NC)"
	@find $(SRC_DIR) -name "*.cpp" | sed 's/^/  /'
	@echo -e "$(YELLOW)Header files:$(NC)"
	@find $(INCLUDE_DIR) -name "*.h" | sed 's/^/  /' 2>/dev/null || echo "  No header files found"

# 문법 체크
syntax-check:
	@echo -e "$(BLUE)📝 Syntax checking...$(NC)"
	@for src in $(ALL_LIB_SOURCES) $(MAIN_SOURCE); do \
		if [ -f "$$src" ]; then \
			echo -e "$(YELLOW)Checking: $$src$(NC)"; \
			$(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only $$src || exit 1; \
		fi \
	done
	@echo -e "$(GREEN)✅ All syntax checks passed$(NC)"

# PHONY 타겟들
.PHONY: all clean run debug release minimal analyze check-headers syntax-check info install directories check-sources check-structure check-json

# 도움말
help:
	@echo -e "$(BLUE)🔧 PulseOne Collector Build System$(NC)"
	@echo ""
	@echo -e "$(GREEN)Main targets:$(NC)"
	@echo "  all              - Build the complete project"
	@echo "  minimal          - Build only core components"
	@echo "  clean            - Remove all build files"
	@echo "  run              - Build and run the application"
	@echo ""
	@echo -e "$(GREEN)Build modes:$(NC)"
	@echo "  debug            - Build with debug symbols and verbose logging"
	@echo "  release          - Build optimized for production"
	@echo ""
	@echo -e "$(GREEN)Development tools:$(NC)"
	@echo "  check-sources    - Verify all required source files exist"
	@echo "  check-structure  - Show project file structure"
	@echo "  syntax-check     - Check syntax without linking"
	@echo "  analyze          - Analyze code structure"
	@echo "  check-json       - Check JSON library availability"
	@echo ""
	@echo -e "$(GREEN)Examples:$(NC)"
	@echo "  make check-sources       # Check if all files exist"
	@echo "  make minimal             # Quick build with core files only"
	@echo "  make debug               # Build debug version"
	@echo "  make clean && make       # Clean rebuild"


