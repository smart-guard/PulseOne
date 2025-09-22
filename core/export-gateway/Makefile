# =============================================================================
# core/export-gateway/Makefile - PulseOne Export Gateway (크로스플랫폼)
# =============================================================================

# 플랫폼 감지 (shared Makefile과 동일한 로직)
UNAME := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME),Linux)
    PLATFORM := Linux
    IS_WINDOWS := 0
else ifeq ($(OS),Windows_NT)
    PLATFORM := Windows
    IS_WINDOWS := 1
else
    PLATFORM := $(UNAME)
    IS_WINDOWS := 0
endif

# 컴파일러 설정
CXX = g++
CC = gcc

# 기본 컴파일 플래그
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -fPIC
CFLAGS = -std=c11 -Wall -Wextra -O2 -fPIC

# Windows 크로스 컴파일 지원 (shared와 동일)
ifeq ($(CROSS_COMPILE_WINDOWS),1)
    CXX = x86_64-w64-mingw32-g++
    CC = x86_64-w64-mingw32-gcc
    CXXFLAGS += -DWIN32 -D_WIN32_WINNT=0x0601 -DWIN32_LEAN_AND_MEAN -DNOMINMAX
    CFLAGS += -DWIN32 -D_WIN32_WINNT=0x0601 -DWIN32_LEAN_AND_MEAN -DNOMINMAX
    PLATFORM := Windows-Cross
    MINGW_PREFIX = /usr/x86_64-w64-mingw32
    INCLUDES += -I$(MINGW_PREFIX)/include
    EXE_SUFFIX = .exe
    LIB_SUFFIX = .a
else ifeq ($(IS_WINDOWS),1)
    CXXFLAGS += -DPULSEONE_WINDOWS=1
    CFLAGS += -DPULSEONE_WINDOWS=1
    EXE_SUFFIX = .exe
    LIB_SUFFIX = .a
else
    CXXFLAGS += -DPULSEONE_LINUX=1
    CFLAGS += -DPULSEONE_LINUX=1
    EXE_SUFFIX =
    LIB_SUFFIX = .a
endif

# 디버그/릴리즈 플래그 (DEBUG 매크로 충돌 방지)
ifeq ($(BUILD_TYPE),Debug)
    CXXFLAGS += -g -O0 -fno-omit-frame-pointer
    # DEBUG 매크로 충돌 방지: PULSEONE_DEBUG 사용
    CXXFLAGS += -DPULSEONE_DEBUG=1
else
    CXXFLAGS += -DNDEBUG -O2
    CXXFLAGS += -DPULSEONE_DEBUG=0
endif

# =============================================================================
# 라이브러리 감지 및 설정 (collector 패턴)
# =============================================================================

# Shared 라이브러리 감지
HAS_SHARED := $(shell [ -d "$(SHARED_LIB_DIR)" ] && echo "1" || echo "0")

# 동적 라이브러리 감지 (collector와 동일)
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HTTPLIB := $(shell echo '\#include <httplib.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")

# =============================================================================
# Include 경로들 (collector 패턴 적용)
# =============================================================================

# 기본 Include 경로
INCLUDES = -I$(INCLUDE_DIR)

# Shared 라이브러리 Include (collector와 동일 패턴)
ifeq ($(HAS_SHARED),1)
    # Shared 라이브러리를 우선순위로 설정
    INCLUDES := -I$(SHARED_INCLUDE_DIR) $(INCLUDES)
    INCLUDES += -I$(SHARED_INCLUDE_DIR)/Platform
    INCLUDES += -I$(SHARED_INCLUDE_DIR)/Common
    INCLUDES += -I$(SHARED_INCLUDE_DIR)/Utils
    INCLUDES += -I$(SHARED_INCLUDE_DIR)/Database
    INCLUDES += -I$(SHARED_INCLUDE_DIR)/Database/Entities
    INCLUDES += -I$(SHARED_INCLUDE_DIR)/Database/Repositories
    INCLUDES += -I$(SHARED_INCLUDE_DIR)/Alarm
    INCLUDES += -I$(SHARED_INCLUDE_DIR)/Export
    INCLUDES += -I$(SHARED_INCLUDE_DIR)/Client
endif

# 외부 라이브러리 Include
INCLUDES += -I/usr/local/include

# Windows 크로스 컴파일용 Include
ifeq ($(CROSS_COMPILE_WINDOWS),1)
    INCLUDES += -I$(MINGW_PREFIX)/include
endif

# =============================================================================
# 라이브러리 링크 (collector 패턴)
# =============================================================================

# Shared 라이브러리 (collector와 동일 순서)
ifeq ($(HAS_SHARED),1)
    SHARED_LIBS = -L$(SHARED_LIB_DIR)
    SHARED_LIBS += -lpulseone-alarm
    SHARED_LIBS += -lpulseone-database  
    SHARED_LIBS += -lpulseone-client
    SHARED_LIBS += -lpulseone-export
    SHARED_LIBS += -lpulseone-common
    CXXFLAGS += -DHAS_SHARED_LIBS=1
else
    SHARED_LIBS =
endif

# 외부 라이브러리들
UTILITY_LIBS =
ifeq ($(HAS_HIREDIS),1)
    UTILITY_LIBS += -lhiredis
    CXXFLAGS += -DHAVE_REDIS=1
endif

ifeq ($(HAS_HTTPLIB),1)
    CXXFLAGS += -DHAVE_HTTPLIB=1
endif

ifeq ($(HAS_NLOHMANN_JSON),1)
    CXXFLAGS += -DHAS_NLOHMANN_JSON=1
endif

# 기본 시스템 라이브러리
ifeq ($(IS_WINDOWS),1)
    BASIC_LIBS = -lpthread -lstdc++ -lm
    PLATFORM_LIBS = -lws2_32 -lwsock32 -liphlpapi
else
    BASIC_LIBS = -lpthread -lm -ldl
    PLATFORM_LIBS =
endif

# 최종 라이브러리 링킹 순서 (collector와 동일)
LDFLAGS += $(SHARED_LIBS) $(UTILITY_LIBS) $(BASIC_LIBS) $(PLATFORM_LIBS)

# 색상 정의
GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m

# =============================================================================
# 디렉토리 구조
# =============================================================================

# 현재 디렉토리
EXPORT_GATEWAY_DIR = $(shell pwd)
PROJECT_ROOT = $(shell dirname $(shell dirname $(EXPORT_GATEWAY_DIR)))
SHARED_DIR = $(PROJECT_ROOT)/core/shared

# 소스 디렉토리
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# Shared 라이브러리 디렉토리
SHARED_INCLUDE_DIR = $(SHARED_DIR)/include
SHARED_LIB_DIR = $(SHARED_DIR)/lib

# =============================================================================
# Include 경로들
# =============================================================================

INCLUDES = -I$(INCLUDE_DIR) \
           -I$(SHARED_INCLUDE_DIR) \
           -I/usr/local/include \
           -I/usr/include

# =============================================================================
# 라이브러리 링크
# =============================================================================

# PulseOne Shared 라이브러리들
PULSEONE_LIBS = -L$(SHARED_LIB_DIR) \
                -lpulseone-common \
                -lpulseone-database \
                -lpulseone-client \
                -lpulseone-export \
                -lpulseone-alarm

# 외부 라이브러리들 (collector와 동일한 환경)
EXTERNAL_LIBS = -lhiredis \
                -lcurl \
                -lssl \
                -lcrypto \
                -ljson \
                -lpthread \
                -ldl

# 전체 링크 플래그
LDFLAGS += $(PULSEONE_LIBS) $(EXTERNAL_LIBS)

# =============================================================================
# 소스 파일 및 오브젝트 파일
# =============================================================================

# CSP 관련 소스들
CSP_SOURCES = $(wildcard $(SRC_DIR)/CSP/*.cpp)
CSP_OBJECTS = $(CSP_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Export 관련 소스들 (향후 확장)
EXPORT_SOURCES = $(wildcard $(SRC_DIR)/Export/*.cpp)
EXPORT_OBJECTS = $(EXPORT_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 유틸리티 소스들
UTILS_SOURCES = $(wildcard $(SRC_DIR)/Utils/*.cpp)
UTILS_OBJECTS = $(UTILS_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 메인 소스
MAIN_SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
MAIN_OBJECTS = $(MAIN_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 전체 오브젝트들
ALL_OBJECTS = $(CSP_OBJECTS) $(EXPORT_OBJECTS) $(UTILS_OBJECTS) $(MAIN_OBJECTS)

# 실행 파일 (플랫폼별 확장자)
TARGET = $(BIN_DIR)/export-gateway$(EXE_SUFFIX)

# =============================================================================
# 메인 타겟들
# =============================================================================

.PHONY: all clean debug release test install help shared-check

# 기본 타겟
all: shared-check $(BUILD_DIR) $(BIN_DIR) $(TARGET)

# 디버그 빌드
debug:
	@$(MAKE) BUILD_TYPE=Debug all

# 릴리즈 빌드 
release:
	@$(MAKE) BUILD_TYPE=Release all

# =============================================================================
# 디렉토리 생성
# =============================================================================

$(BUILD_DIR) $(BIN_DIR):
	@mkdir -p $@
	@mkdir -p $(BUILD_DIR)/CSP $(BUILD_DIR)/Export $(BUILD_DIR)/Utils

# =============================================================================
# Shared 라이브러리 의존성 체크 (collector 패턴)
# =============================================================================

shared-check:
	@echo -e "$(BLUE)🔍 Checking PulseOne Shared Libraries [$(PLATFORM)]...$(NC)"
	@if [ ! -d "$(SHARED_LIB_DIR)" ]; then \
		echo -e "$(RED)❌ Shared library directory not found: $(SHARED_LIB_DIR)$(NC)"; \
		echo -e "$(YELLOW)💡 Please build shared libraries first:$(NC)"; \
		echo -e "$(YELLOW)   cd $(SHARED_DIR) && make all$(NC)"; \
		exit 1; \
	fi
	@echo -e "Shared libs:"
	@echo -e "  libpulseone-common: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-common$(LIB_SUFFIX)),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  libpulseone-database: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-database$(LIB_SUFFIX)),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  libpulseone-client: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-client$(LIB_SUFFIX)),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  libpulseone-export: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-export$(LIB_SUFFIX)),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  libpulseone-alarm: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-alarm$(LIB_SUFFIX)),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "$(GREEN)✅ Shared libraries checked$(NC)"

# =============================================================================
# 실행 파일 빌드
# =============================================================================

$(TARGET): $(ALL_OBJECTS) | $(BIN_DIR)
	@echo -e "$(GREEN)🔗 Linking Export Gateway [$(BUILD_TYPE)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) -o $@ $(LDFLAGS)
	@echo -e "$(GREEN)✅ Export Gateway built successfully: $@$(NC)"

# =============================================================================
# 오브젝트 파일 컴파일
# =============================================================================

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️ Compiling $< [$(BUILD_TYPE)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 테스트 타겟들
# =============================================================================

# CSP 컴포넌트만 테스트
test-csp: $(CSP_OBJECTS)
	@echo -e "$(BLUE)🧪 Testing CSP components...$(NC)"
	@echo -e "$(GREEN)✅ CSP compilation test passed$(NC)"

# 헤더 파일 문법 검사 (오류 메시지 표시)
test-headers:
	@echo -e "$(BLUE)🔍 Testing header files [$(PLATFORM)]...$(NC)"
	@echo "Testing CSP headers..."
	@if [ -f "$(INCLUDE_DIR)/CSP/AlarmMessage.h" ]; then \
		$(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only $(INCLUDE_DIR)/CSP/AlarmMessage.h && \
		echo "  ✅ AlarmMessage.h" || echo "  ❌ AlarmMessage.h"; \
	else \
		echo "  ⚠️ AlarmMessage.h not found"; \
	fi
	@echo -e "$(GREEN)✅ Header syntax check completed [$(PLATFORM)]$(NC)"

# 전체 컴파일 테스트 (링크 없이)
test-compile: $(ALL_OBJECTS)
	@echo -e "$(GREEN)✅ All sources compiled successfully$(NC)"

# =============================================================================
# 실행 및 개발 타겟들
# =============================================================================

# 실행
run: $(TARGET)
	@echo -e "$(BLUE)🚀 Running Export Gateway...$(NC)"
	@cd $(BIN_DIR) && ./export-gateway

# 설정 파일 생성 (개발용)
config:
	@echo -e "$(BLUE)⚙️ Creating sample configuration...$(NC)"
	@mkdir -p config
	@echo '{ "csp_gateway": { "building_id": 1001, "debug_mode": true } }' > config/export_config.json
	@echo -e "$(GREEN)✅ Sample config created: config/export_config.json$(NC)"

# =============================================================================
# 크로스 컴파일 지원 (shared와 동일 패턴)
# =============================================================================

# Windows 크로스 컴파일
windows-cross:
	@echo -e "$(YELLOW)🔧 Building Export Gateway for Windows (cross-compile)...$(NC)"
	$(MAKE) CROSS_COMPILE_WINDOWS=1 all

# =============================================================================
# 정리 및 유틸리티
# =============================================================================

clean:
	@echo -e "$(YELLOW)🧹 Cleaning export-gateway build files...$(NC)"
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo -e "$(GREEN)✅ Clean completed$(NC)"

clean-all: clean
	@echo -e "$(YELLOW)🧹 Cleaning shared libraries...$(NC)"
	@cd $(SHARED_DIR) && $(MAKE) clean

# 의존성 정보
deps-info:
	@echo -e "$(BLUE)📋 Export Gateway Dependencies$(NC)"
	@echo "=================================="
	@echo "Platform: $(PLATFORM)"
	@echo "Build Type: $(BUILD_TYPE)"
	@echo "Shared Dir: $(SHARED_DIR)"
	@echo "Include Paths:"
	@echo "  - $(INCLUDE_DIR)"
	@echo "  - $(SHARED_INCLUDE_DIR)"
	@echo ""
	@echo "Required Libraries:"
	@echo "  - PulseOne Shared (common, database, client, export, alarm)"
	@echo "  - hiredis (Redis client)"
	@echo "  - libcurl (HTTP/S3 client)"
	@echo "  - nlohmann/json (JSON processing)"

# 빌드 상태 확인
status:
	@echo -e "$(BLUE)📊 Export Gateway Build Status$(NC)"
	@echo "=============================="
	@echo "Target: $(if $(wildcard $(TARGET)),$(GREEN)✅ Built$(NC),$(RED)❌ Not built$(NC))"
	@echo "Source files: $(words $(CSP_SOURCES) $(EXPORT_SOURCES) $(UTILS_SOURCES) $(MAIN_SOURCES))"
	@echo "Object files: $(if $(wildcard $(BUILD_DIR)/*.o),$(words $(wildcard $(BUILD_DIR)/*/*.o)),0)"
	@echo "Shared libs: $(if $(wildcard $(SHARED_LIB_DIR)/*.a),$(GREEN)✅ Available$(NC),$(RED)❌ Missing$(NC))"

# 도움말
help:
	@echo -e "$(BLUE)🚀 PulseOne Export Gateway Makefile$(NC)"
	@echo "====================================="
	@echo ""
	@echo -e "$(GREEN)Main Targets:$(NC)"
	@echo "  make all          - Build export gateway (default)"
	@echo "  make debug        - Build in debug mode"
	@echo "  make release      - Build in release mode"
	@echo "  make run          - Build and run export gateway"
	@echo ""
	@echo -e "$(GREEN)Testing:$(NC)"
	@echo "  make test-csp     - Test CSP components only"
	@echo "  make test-headers - Test header file syntax"
	@echo "  make test-compile - Test compilation without linking"
	@echo ""
	@echo -e "$(GREEN)Development:$(NC)"
	@echo "  make config       - Create sample configuration"
	@echo "  make deps-info    - Show dependency information"
	@echo "  make status       - Show build status"
	@echo ""
	@echo -e "$(GREEN)Utilities:$(NC)"
	@echo "  make clean        - Clean build files"
	@echo "  make clean-all    - Clean everything including shared libs"
	@echo "  make help         - Show this help"
	@echo ""
	@echo -e "$(YELLOW)Build Types:$(NC)"
	@echo "  BUILD_TYPE=Debug  - Debug build with symbols"
	@echo "  BUILD_TYPE=Release - Optimized release build (default)"
	@echo ""
	@echo -e "$(YELLOW)Current Settings:$(NC)"
	@echo "  Platform: $(PLATFORM)"
	@echo "  Build Type: $(BUILD_TYPE)"
	@echo "  Shared Dir: $(SHARED_DIR)"

.DEFAULT_GOAL := all