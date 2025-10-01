# =============================================================================
# core/export-gateway/Makefile - PulseOne Export Gateway (크로스플랫폼)
# libcurl 자동 감지 및 링크 추가
# =============================================================================

# 플랫폼 감지
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

# Windows 크로스 컴파일 지원
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

# 디버그/릴리즈 플래그
ifeq ($(BUILD_TYPE),Debug)
    CXXFLAGS += -g -O0 -fno-omit-frame-pointer
    CXXFLAGS += -DPULSEONE_DEBUG=1
else
    CXXFLAGS += -DNDEBUG -O2
    CXXFLAGS += -DPULSEONE_DEBUG=0
endif

# =============================================================================
# 디렉토리 구조
# =============================================================================

EXPORT_GATEWAY_DIR = $(shell pwd)
PROJECT_ROOT = $(shell dirname $(shell dirname $(EXPORT_GATEWAY_DIR)))
SHARED_DIR = $(PROJECT_ROOT)/core/shared

SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin

SHARED_INCLUDE_DIR = $(SHARED_DIR)/include
SHARED_LIB_DIR = $(SHARED_DIR)/lib

# =============================================================================
# 라이브러리 감지
# =============================================================================

HAS_SHARED := $(shell [ -d "$(SHARED_LIB_DIR)" ] && [ -f "$(SHARED_LIB_DIR)/libpulseone-common$(LIB_SUFFIX)" ] && echo "1" || echo "0")
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HTTPLIB := $(shell echo '\#include <httplib.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")

# 🔥 libcurl 자동 감지 (pkg-config 우선, 없으면 직접 테스트)
HAS_CURL := $(shell pkg-config --exists libcurl 2>/dev/null && echo "1" || echo "0")
ifeq ($(HAS_CURL),0)
    HAS_CURL := $(shell echo '\#include <curl/curl.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
endif

# OpenSSL 감지 (S3 서명용)
HAS_OPENSSL := $(shell pkg-config --exists openssl 2>/dev/null && echo "1" || echo "0")
ifeq ($(HAS_OPENSSL),0)
    HAS_OPENSSL := $(shell echo '\#include <openssl/sha.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
endif

# =============================================================================
# Include 경로들
# =============================================================================

INCLUDES = -I$(INCLUDE_DIR)

ifeq ($(HAS_SHARED),1)
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
    INCLUDES += -I$(SHARED_INCLUDE_DIR)/Security
endif

INCLUDES += -I/usr/local/include

ifeq ($(CROSS_COMPILE_WINDOWS),1)
    INCLUDES += -I$(MINGW_PREFIX)/include
endif

# =============================================================================
# 라이브러리 링크
# =============================================================================

# Shared 라이브러리
ifeq ($(HAS_SHARED),1)
    SHARED_LIBS = -L$(SHARED_LIB_DIR)
    SHARED_LIBS += -lpulseone-alarm
    SHARED_LIBS += -lpulseone-database  
    SHARED_LIBS += -lpulseone-client
    SHARED_LIBS += -lpulseone-export
    SHARED_LIBS += -lpulseone-common
    SHARED_LIBS += -lpulseone-security
    CXXFLAGS += -DHAS_SHARED_LIBS=1
    $(info ✅ HAS_SHARED=1 detected - LogManager available)
else
    SHARED_LIBS =
    $(info ⚠️  HAS_SHARED=0 - LogManager not available)
endif

# 외부 라이브러리들
UTILITY_LIBS =

# 🔥 libcurl 링크 (pkg-config 우선, 없으면 직접 링크)
ifeq ($(HAS_CURL),1)
    CURL_LIBS := $(shell pkg-config --libs libcurl 2>/dev/null)
    ifneq ($(CURL_LIBS),)
        UTILITY_LIBS += $(CURL_LIBS)
        CXXFLAGS += -DHAS_CURL=1
        $(info ✅ libcurl found via pkg-config)
    else
        UTILITY_LIBS += -lcurl
        CXXFLAGS += -DHAS_CURL=1
        $(info ✅ libcurl found (direct link))
    endif
else
    $(info ⚠️  libcurl not found - HTTP/S3 features disabled)
endif

# 🔥 OpenSSL 링크 (S3 서명용)
ifeq ($(HAS_OPENSSL),1)
    OPENSSL_LIBS := $(shell pkg-config --libs openssl 2>/dev/null)
    ifneq ($(OPENSSL_LIBS),)
        UTILITY_LIBS += $(OPENSSL_LIBS)
        CXXFLAGS += -DHAS_OPENSSL=1
        $(info ✅ OpenSSL found via pkg-config)
    else
        UTILITY_LIBS += -lssl -lcrypto
        CXXFLAGS += -DHAS_OPENSSL=1
        $(info ✅ OpenSSL found (direct link))
    endif
else
    $(info ⚠️  OpenSSL not found - S3 signature features disabled)
endif

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

# 🔥 최종 링크 순서 (중요: curl과 ssl은 shared libs 뒤에)
LDFLAGS = $(SHARED_LIBS) $(UTILITY_LIBS) $(BASIC_LIBS) $(PLATFORM_LIBS)

# 색상 정의
GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m

# =============================================================================
# 소스 파일 및 오브젝트 파일
# =============================================================================

CSP_SOURCES = $(wildcard $(SRC_DIR)/CSP/*.cpp)
CSP_OBJECTS = $(CSP_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

EXPORT_SOURCES = $(wildcard $(SRC_DIR)/Export/*.cpp)
EXPORT_OBJECTS = $(EXPORT_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

UTILS_SOURCES = $(wildcard $(SRC_DIR)/Utils/*.cpp)
UTILS_OBJECTS = $(UTILS_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

MAIN_SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
MAIN_OBJECTS = $(MAIN_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

ALL_OBJECTS = $(CSP_OBJECTS) $(EXPORT_OBJECTS) $(UTILS_OBJECTS) $(MAIN_OBJECTS)

TARGET = $(BIN_DIR)/export-gateway$(EXE_SUFFIX)

# =============================================================================
# 메인 타겟들
# =============================================================================

.PHONY: all clean debug release test install help shared-check debug-config

all: shared-check $(BUILD_DIR) $(BIN_DIR) $(TARGET)

debug:
	@$(MAKE) BUILD_TYPE=Debug all

release:
	@$(MAKE) BUILD_TYPE=Release all

# =============================================================================
# 디렉토리 생성
# =============================================================================

$(BUILD_DIR) $(BIN_DIR):
	@mkdir -p $@
	@mkdir -p $(BUILD_DIR)/CSP $(BUILD_DIR)/Export $(BUILD_DIR)/Utils

# =============================================================================
# Shared 라이브러리 의존성 체크
# =============================================================================

shared-check:
	@echo -e "$(BLUE)🔍 Checking PulseOne Shared Libraries [$(PLATFORM)]...$(NC)"
	@echo "Shared lib directory: $(SHARED_LIB_DIR)"
	@echo "HAS_SHARED: $(HAS_SHARED)"
	@echo "HAS_CURL: $(HAS_CURL)"
	@echo "HAS_OPENSSL: $(HAS_OPENSSL)"
	@if [ ! -d "$(SHARED_LIB_DIR)" ]; then \
		echo -e "$(RED)❌ Shared library directory not found$(NC)"; \
		echo -e "$(YELLOW)💡 Build shared libraries: cd $(SHARED_DIR) && make all$(NC)"; \
		exit 1; \
	fi
	@echo -e "Shared libs:"
	@echo -e "  libpulseone-security: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-security$(LIB_SUFFIX)),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  libpulseone-common: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-common$(LIB_SUFFIX)),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  libpulseone-database: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-database$(LIB_SUFFIX)),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  libpulseone-client: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-client$(LIB_SUFFIX)),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  libpulseone-export: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-export$(LIB_SUFFIX)),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  libpulseone-alarm: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-alarm$(LIB_SUFFIX)),$(GREEN)✅$(NC),$(RED)❌$(NC))"
ifeq ($(HAS_SHARED),1)
	@echo -e "$(GREEN)✅ HAS_SHARED_LIBS=1 - LogManager available$(NC)"
else
	@echo -e "$(YELLOW)⚠️  HAS_SHARED_LIBS not set - Logging disabled$(NC)"
endif
	@echo -e "$(GREEN)✅ Shared libraries checked$(NC)"

# =============================================================================
# 실행 파일 빌드
# =============================================================================

$(TARGET): $(ALL_OBJECTS) | $(BIN_DIR)
	@mkdir -p $(BIN_DIR)  # ← 이 줄 추가 (방어적 생성)
	@echo -e "$(GREEN)🔗 Linking Export Gateway [$(BUILD_TYPE)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) -o $@ $(LDFLAGS)
	@echo -e "$(GREEN)✅ Export Gateway built: $@$(NC)"

# =============================================================================
# 오브젝트 파일 컴파일
# =============================================================================

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️ Compiling $< [$(BUILD_TYPE)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 유틸리티
# =============================================================================

clean:
	@echo -e "$(YELLOW)🧹 Cleaning...$(NC)"
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo -e "$(GREEN)✅ Clean completed$(NC)"

help:
	@echo -e "$(BLUE)🚀 PulseOne Export Gateway Makefile$(NC)"
	@echo "====================================="
	@echo ""
	@echo "Main Targets:"
	@echo "  make all          - Build export gateway"
	@echo "  make debug        - Build in debug mode"
	@echo "  make release      - Build in release mode"
	@echo "  make clean        - Clean build files"
	@echo "  make help         - Show this help"
	@echo ""
	@echo "Current: Platform=$(PLATFORM), HAS_CURL=$(HAS_CURL), HAS_OPENSSL=$(HAS_OPENSSL)"

.DEFAULT_GOAL := all