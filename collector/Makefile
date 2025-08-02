# =============================================================================
# PulseOne Collector Makefile - 수정된 버전 (Redis 라이브러리 포함)
# =============================================================================

# nlohmann/json 라이브러리 체크
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")

# 🔧 수정: hiredis 라이브러리 체크 추가
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")

# BACnet Stack 체크 (기존 hiredis 체크 밑에 추가)
HAS_BACNET_STACK := $(shell pkg-config --exists bacnet && echo "1" || echo "0")

# 컴파일러 설정
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -DPULSEONE_DEBUG_MODE -O0 -Iinclude $(shell pkg-config --cflags libmodbus) -I/usr/local/include/bacnet
LDFLAGS = 

# 🔧 수정: Redis 라이브러리 추가
LIBS = -lpthread \
       $(shell pkg-config --libs libmodbus) \
       -lpqxx -lpq -lsqlite3 \
       -lpaho-mqtt3c -lpaho-mqttpp3 \
       -lbacnet -lmysqlclient \
       -lhiredis -lhiredis_ssl

# 기존 CXXFLAGS에 JSON 플래그 추가
ifeq ($(HAS_NLOHMANN_JSON),1)
    CXXFLAGS += -DHAS_NLOHMANN_JSON
endif

# 🔧 추가: hiredis 플래그 추가
ifeq ($(HAS_HIREDIS),1)
    CXXFLAGS += -DHAS_HIREDIS
else
    $(warning ⚠️  hiredis not found - Redis functionality will be limited)
    $(warning    Install with: sudo apt-get install libhiredis-dev)
endif

ifeq ($(HAS_BACNET_STACK), 1)
    CXXFLAGS += -DHAS_BACNET_STACK=1
else
    CXXFLAGS += -DHAS_BACNET_STACK=0
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
# 소스 파일 그룹 정의 (기존과 동일)
# =============================================================================

# Core 소스들
CORE_SOURCES := $(wildcard $(SRC_DIR)/Core/*.cpp)

# 기존 Utils, Config 등
UTILS_SOURCES := $(wildcard $(SRC_DIR)/Utils/*.cpp)
CONFIG_SOURCES := $(wildcard $(SRC_DIR)/Config/*.cpp)

# Workers 소스들
WORKERS_BASE_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp)
WORKERS_PROTOCOL_SOURCES := $(wildcard $(SRC_DIR)/Workers/Protocol/*.cpp)
WORKERS_COMPONENTS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Components/*.cpp)
WORKERS_SOURCES := $(WORKERS_BASE_SOURCES) $(WORKERS_PROTOCOL_SOURCES) $(WORKERS_COMPONENTS_SOURCES) $(wildcard $(SRC_DIR)/Workers/WorkerFactory.cpp)

# Drivers 소스들
DRIVERS_COMMON_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Common/*.cpp)
DRIVERS_MODBUS_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Modbus/*.cpp)
DRIVERS_MQTT_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Mqtt/*.cpp)
DRIVERS_BACNET_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Bacnet/*.cpp)
DRIVERS_SOURCES := $(DRIVERS_COMMON_SOURCES) $(DRIVERS_MODBUS_SOURCES) $(DRIVERS_MQTT_SOURCES) $(DRIVERS_BACNET_SOURCES)

# Database 소스들
DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Entities/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)

# 🔧 수정: Client 소스들 (Redis 포함)
CLIENT_SOURCES := $(wildcard $(SRC_DIR)/Client/*.cpp)

# Plugin 소스들
PLUGIN_SOURCES := $(wildcard $(SRC_DIR)/Plugin/*.cpp)

# 메인 소스
MAIN_SOURCE = $(SRC_DIR)/main.cpp

# 전체 소스들
EXISTING_SOURCES = $(CORE_SOURCES) $(UTILS_SOURCES) $(CONFIG_SOURCES)
OPTIONAL_SOURCES = $(WORKERS_SOURCES) $(DRIVERS_SOURCES) $(DATABASE_SOURCES) $(CLIENT_SOURCES) $(PLUGIN_SOURCES)

ALL_LIB_SOURCES = $(EXISTING_SOURCES) $(OPTIONAL_SOURCES)

# 오브젝트 파일들
ALL_LIB_OBJECTS = $(ALL_LIB_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
MAIN_OBJECT = $(MAIN_SOURCE:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
ALL_OBJECTS = $(ALL_LIB_OBJECTS) $(MAIN_OBJECT)

# =============================================================================
# 메인 타겟들
# =============================================================================

.DEFAULT_GOAL := all

# 🔧 수정: 라이브러리 체크 포함
check-deps:
	@echo -e "$(BLUE)🔍 Checking dependencies...$(NC)"
	@echo -e "$(YELLOW)Required libraries:$(NC)"
	@if [ "$(HAS_NLOHMANN_JSON)" = "1" ]; then \
		echo -e "  $(GREEN)✅ nlohmann/json$(NC)"; \
	else \
		echo -e "  $(YELLOW)⚠️  nlohmann/json (optional)$(NC)"; \
	fi
	@if [ "$(HAS_HIREDIS)" = "1" ]; then \
		echo -e "  $(GREEN)✅ hiredis$(NC)"; \
	else \
		echo -e "  $(RED)❌ hiredis (required for Redis support)$(NC)"; \
		echo -e "     Install: sudo apt-get install libhiredis-dev"; \
	fi
	@pkg-config --exists libmodbus && echo -e "  $(GREEN)✅ libmodbus$(NC)" || echo -e "  $(RED)❌ libmodbus$(NC)"
	@echo -e "$(YELLOW)Checking libraries:$(NC)"
	@for lib in pthread hiredis pqxx pq sqlite3 paho-mqtt3c paho-mqttpp3 mysqlclient; do \
		if ldconfig -p | grep -q "lib$$lib"; then \
			echo -e "  $(GREEN)✅ $$lib$(NC)"; \
		else \
			echo -e "  $(YELLOW)⚠️  $$lib (may not be installed)$(NC)"; \
		fi \
	done

# 파일 존재 확인 (기존 코드 유지)
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
	@echo -e "$(YELLOW)Client sources (Redis):$(NC)"
	@for src in $(CLIENT_SOURCES); do \
		if [ -f "$$src" ]; then \
			echo -e "  $(GREEN)✅ $$src$(NC)"; \
		else \
			echo -e "  $(YELLOW)⚠️  $$src (optional)$(NC)"; \
		fi \
	done

# 전체 빌드 (의존성 및 소스 확인 포함)
all: check-deps check-sources directories $(TARGET)
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
# 오브젝트 파일 생성 규칙 (기존과 동일, Client 추가)
# =============================================================================

# Core 파일들
$(BUILD_DIR)/Core/%.o: $(SRC_DIR)/Core/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(GREEN)🏛️  Compiling Core: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Utils 파일들
$(BUILD_DIR)/Utils/%.o: $(SRC_DIR)/Utils/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(GREEN)🔧 Compiling Utils: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Config 파일들
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

# 🔧 수정: Client 파일들 (Redis 포함)
$(BUILD_DIR)/Client/%.o: $(SRC_DIR)/Client/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(BLUE)📡 Compiling Client (Redis): $<$(NC)"
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

# 빌드 모드별 타겟들
debug: CXXFLAGS += -g -DPULSEONE_DEBUG_MODE -O0
debug: CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
debug: check-deps clean all
	@echo -e "$(GREEN)🐛 Debug build completed - verbose logging enabled$(NC)"

release: CXXFLAGS += -O3 -DNDEBUG  
release: CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
release: clean all
	@echo -e "$(GREEN)🚀 Release build completed - optimized for performance$(NC)"

# Redis 전용 테스트 빌드
redis-test: check-deps
	@echo -e "$(BLUE)🧪 Testing Redis components only...$(NC)"
	@if [ "$(HAS_HIREDIS)" != "1" ]; then \
		echo -e "$(RED)❌ hiredis not found - cannot test Redis$(NC)"; \
		exit 1; \
	fi
	@echo -e "$(GREEN)✅ Redis test ready$(NC)"

# 🔧 추가: hiredis 체크 타겟
check-redis:
	@echo "🔍 Checking Redis/hiredis setup..."
	@if [ "$(HAS_HIREDIS)" = "1" ]; then \
		echo "✅ hiredis headers found"; \
		if ldconfig -p | grep -q libhiredis; then \
			echo "✅ hiredis library installed"; \
		else \
			echo "⚠️  hiredis library may not be properly installed"; \
		fi \
	else \
		echo "❌ hiredis headers not found"; \
		echo "   Install: sudo apt-get install libhiredis-dev"; \
		echo "   Or compile from source: https://github.com/redis/hiredis"; \
	fi

# PHONY 타겟들
.PHONY: all clean run debug release check-deps check-sources check-redis redis-test directories

# 도움말
help:
	@echo -e "$(BLUE)🔧 PulseOne Collector Build System$(NC)"
	@echo ""
	@echo -e "$(GREEN)Main targets:$(NC)"
	@echo "  all              - Build the complete project"
	@echo "  clean            - Remove all build files"
	@echo "  run              - Build and run the application"
	@echo ""
	@echo -e "$(GREEN)Build modes:$(NC)"
	@echo "  debug            - Build with debug symbols and verbose logging"
	@echo "  release          - Build optimized for production"
	@echo ""
	@echo -e "$(GREEN)Development tools:$(NC)"
	@echo "  check-deps       - Check all library dependencies"
	@echo "  check-sources    - Verify all required source files exist"
	@echo "  check-redis      - Check Redis/hiredis installation"
	@echo "  redis-test       - Test Redis components only"
	@echo ""
	@echo -e "$(GREEN)Examples:$(NC)"
	@echo "  make check-deps          # Check if all libraries are available"
	@echo "  make check-redis         # Specifically check Redis setup"
	@echo "  make redis-test          # Test Redis compilation"
	@echo "  make clean && make       # Clean rebuild"