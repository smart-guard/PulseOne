# =============================================================================
# PulseOne Collector Makefile - 새 폴더 구조 지원
# =============================================================================

# 컴파일러 설정
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -DDEBUG -O0 -Iinclude $(shell pkg-config --cflags libmodbus)
LDFLAGS = 
LIBS = -lpthread $(shell pkg-config --libs libmodbus) -lpqxx -lpq -lsqlite3

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
debug: CXXFLAGS += -g -DDEBUG -O0
debug: CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
debug: clean all
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