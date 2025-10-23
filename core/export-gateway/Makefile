# =============================================================================
# core/export-gateway/Makefile - PulseOne Export Gateway (크로스플랫폼)
# 🔥 완전 수정 버전 - include 경로 우선순위 수정
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

# 🔥 hiredis 감지 (pkg-config 우선, 없으면 헤더 테스트)
HAS_HIREDIS := $(shell pkg-config --exists hiredis 2>/dev/null && echo "1" || echo "0")
ifeq ($(HAS_HIREDIS),0)
    HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
endif

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
# Include 경로들 (🔥 로컬 include를 최우선으로!)
# =============================================================================

# 🔥 1순위: 로컬 include (export-gateway 자체)
INCLUDES = -I$(INCLUDE_DIR)

# 🔥 2순위: Shared 라이브러리 include
ifeq ($(HAS_SHARED),1)
    INCLUDES += -I$(SHARED_INCLUDE_DIR)
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
    INCLUDES += -I$(SHARED_INCLUDE_DIR)/Data
endif

# 🔥 3순위: 시스템 include
INCLUDES += -I/usr/local/include

ifeq ($(CROSS_COMPILE_WINDOWS),1)
    INCLUDES += -I$(MINGW_PREFIX)/include
endif

# =============================================================================
# 라이브러리 링크
# =============================================================================

# Shared 라이브러리들 (의존성 순서 중요!)
SHARED_LIBS =
ifeq ($(HAS_SHARED),1)
    CXXFLAGS += -DHAS_SHARED_LIBS=1
    SHARED_LIBS = -L$(SHARED_LIB_DIR)
    SHARED_LIBS += -lpulseone-alarm
    SHARED_LIBS += -lpulseone-export
    SHARED_LIBS += -lpulseone-data
    SHARED_LIBS += -lpulseone-database  
    SHARED_LIBS += -lpulseone-client
    SHARED_LIBS += -lpulseone-common
    SHARED_LIBS += -lpulseone-security
    SHARED_LIBS += -L/usr/local/lib
endif

# 외부 라이브러리들
UTILITY_LIBS =

# 🔥 hiredis 링크 (pkg-config 우선, 없으면 직접 링크)
ifeq ($(HAS_HIREDIS),1)
    HIREDIS_LIBS := $(shell pkg-config --libs hiredis 2>/dev/null)
    ifneq ($(HIREDIS_LIBS),)
        UTILITY_LIBS += $(HIREDIS_LIBS)
        CXXFLAGS += -DHAVE_REDIS=1
        $(info ✅ hiredis found via pkg-config)
    else
        UTILITY_LIBS += -lhiredis
        CXXFLAGS += -DHAVE_REDIS=1
        $(info ✅ hiredis found (direct link))
    endif
else
    $(info ⚠️  hiredis not found - Redis features disabled)
endif

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

ifeq ($(HAS_HTTPLIB),1)
    CXXFLAGS += -DHAVE_HTTPLIB=1
endif

ifeq ($(HAS_NLOHMANN_JSON),1)
    CXXFLAGS += -DHAS_NLOHMANN_JSON=1
endif

# 🔥 기본 시스템 라이브러리 (SQLite3 추가!)
ifeq ($(IS_WINDOWS),1)
    BASIC_LIBS = -lpthread -lstdc++ -lm -lsqlite3
    PLATFORM_LIBS = -lws2_32 -lwsock32 -liphlpapi
else
    BASIC_LIBS = -lpthread -lm -ldl -lsqlite3
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
# 소스 파일 및 오브젝트 파일 (✅ 실제 존재하는 파일만)
# =============================================================================

# 🆕 Alarm 디렉토리 (실제 존재 확인됨)
ALARM_SOURCES = $(wildcard $(SRC_DIR)/Alarm/*.cpp)
ALARM_OBJECTS = $(ALARM_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 🆕 Schedule 디렉토리 (실제 존재 확인됨)
SCHEDULE_SOURCES = $(wildcard $(SRC_DIR)/Schedule/*.cpp)
SCHEDULE_OBJECTS = $(SCHEDULE_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 🔄 Transform 디렉토리 (PayloadTransformer)
TRANSFORM_SOURCES = $(wildcard $(SRC_DIR)/Transform/*.cpp)
TRANSFORM_OBJECTS = $(TRANSFORM_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 🔌 CSP 디렉토리 (실제 존재 확인됨)
CSP_SOURCES = $(wildcard $(SRC_DIR)/CSP/*.cpp)
CSP_OBJECTS = $(CSP_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 📤 Export 디렉토리 (존재하면 추가)
EXPORT_SOURCES = $(wildcard $(SRC_DIR)/Export/*.cpp)
EXPORT_OBJECTS = $(EXPORT_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 🛠️ Utils 디렉토리 (있으면 추가)
UTILS_SOURCES = $(wildcard $(SRC_DIR)/Utils/*.cpp)
UTILS_OBJECTS = $(UTILS_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 📌 Main 파일
MAIN_SOURCES = $(SRC_DIR)/main.cpp
MAIN_OBJECT = $(BUILD_DIR)/main.o

# 📦 전체 오브젝트 파일
ALL_OBJECTS = $(ALARM_OBJECTS) $(SCHEDULE_OBJECTS) $(TRANSFORM_OBJECTS) $(CSP_OBJECTS) $(EXPORT_OBJECTS) $(UTILS_OBJECTS) $(MAIN_OBJECT)

# 🎯 최종 실행 파일
TARGET = $(BIN_DIR)/export-gateway$(EXE_SUFFIX)

# =============================================================================
# 테스트 디렉토리 구조
# =============================================================================

TEST_DIR = tests
TEST_BUILD_DIR = build/tests
TEST_BIN_DIR = bin/tests

TEST_SOURCES = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_DIR)/%.cpp=$(TEST_BUILD_DIR)/%.o)
TEST_EXECUTABLES = $(TEST_SOURCES:$(TEST_DIR)/%.cpp=$(TEST_BIN_DIR)/%)

# =============================================================================
# 메인 타겟들
# =============================================================================

.PHONY: all clean clean-all run test check-libs debug-vars

# 기본 타겟
all: check-libs $(TARGET)
	@echo -e "$(GREEN)✅ Build completed: $(TARGET)$(NC)"

# 최종 링크
$(TARGET): $(ALL_OBJECTS) | $(BIN_DIR)
	@echo -e "$(BLUE)🔗 Linking $@$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(LDFLAGS) -o $@
	@echo -e "$(GREEN)✅ Export Gateway built successfully!$(NC)"

# 디렉토리 생성
$(BUILD_DIR) $(BIN_DIR) $(TEST_BUILD_DIR) $(TEST_BIN_DIR):
	@mkdir -p $@

# 컴파일 규칙
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️  Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 유틸리티
# =============================================================================

# ✅ clean (빌드 파일만 삭제)
clean:
	@echo -e "$(YELLOW)🧹 Cleaning build files...$(NC)"
	@find $(BUILD_DIR) -type f -name '*.o' -delete 2>/dev/null || true
	@find $(BIN_DIR) -type f -delete 2>/dev/null || true
	@echo -e "$(GREEN)✅ Clean completed$(NC)"

# ✅ clean-all (디렉토리까지 삭제)
clean-all:
	@echo -e "$(YELLOW)🧹 Deep cleaning...$(NC)"
	@rm -rf $(BUILD_DIR) $(BIN_DIR) 2>/dev/null || true
	@echo -e "$(GREEN)✅ Deep clean completed$(NC)"

# ✅ run (실행)
run: $(TARGET)
	@echo -e "$(BLUE)🚀 Running Export Gateway...$(NC)"
	@$(TARGET)

# ✅ check-libs (라이브러리 확인)
check-libs:
	@echo -e "$(BLUE)🔍 Checking PulseOne Shared Libraries [$(PLATFORM)]...$(NC)"
	@echo "Shared lib directory: $(SHARED_LIB_DIR)"
	@echo "HAS_SHARED: $(HAS_SHARED)"
	@echo "HAS_HIREDIS: $(HAS_HIREDIS)"
	@echo "HAS_CURL: $(HAS_CURL)"
	@echo "HAS_OPENSSL: $(HAS_OPENSSL)"
ifeq ($(HAS_SHARED),1)
	@echo -e "Shared libs:"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-security$(LIB_SUFFIX)" ] && echo -e "  libpulseone-security: ✅" || echo -e "  libpulseone-security: ❌"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-common$(LIB_SUFFIX)" ] && echo -e "  libpulseone-common: ✅" || echo -e "  libpulseone-common: ❌"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-database$(LIB_SUFFIX)" ] && echo -e "  libpulseone-database: ✅" || echo -e "  libpulseone-database: ❌"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-client$(LIB_SUFFIX)" ] && echo -e "  libpulseone-client: ✅" || echo -e "  libpulseone-client: ❌"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-data$(LIB_SUFFIX)" ] && echo -e "  libpulseone-data: ✅" || echo -e "  libpulseone-data: ❌"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-export$(LIB_SUFFIX)" ] && echo -e "  libpulseone-export: ✅" || echo -e "  libpulseone-export: ❌"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-alarm$(LIB_SUFFIX)" ] && echo -e "  libpulseone-alarm: ✅" || echo -e "  libpulseone-alarm: ❌"
	@echo -e "$(GREEN)✅ HAS_SHARED_LIBS=1 - LogManager available$(NC)"
else
	@echo -e "$(YELLOW)⚠️  Shared libraries not found$(NC)"
	@echo -e "$(YELLOW)⚠️  Build will continue without shared libraries$(NC)"
endif
	@echo -e "$(GREEN)✅ Shared libraries checked$(NC)"

# ✅ debug-vars (디버깅용 변수 출력)
debug-vars:
	@echo "=== Platform Info ==="
	@echo "PLATFORM: $(PLATFORM)"
	@echo "IS_WINDOWS: $(IS_WINDOWS)"
	@echo "EXE_SUFFIX: $(EXE_SUFFIX)"
	@echo ""
	@echo "=== Directories ==="
	@echo "PROJECT_ROOT: $(PROJECT_ROOT)"
	@echo "SHARED_DIR: $(SHARED_DIR)"
	@echo "SHARED_LIB_DIR: $(SHARED_LIB_DIR)"
	@echo ""
	@echo "=== Source Files ==="
	@echo "ALARM_SOURCES: $(ALARM_SOURCES)"
	@echo "SCHEDULE_SOURCES: $(SCHEDULE_SOURCES)"
	@echo "TRANSFORM_SOURCES: $(TRANSFORM_SOURCES)"
	@echo "CSP_SOURCES: $(CSP_SOURCES)"
	@echo "EXPORT_SOURCES: $(EXPORT_SOURCES)"
	@echo "UTILS_SOURCES: $(UTILS_SOURCES)"
	@echo "MAIN_SOURCES: $(MAIN_SOURCES)"
	@echo ""
	@echo "=== Object Files ==="
	@echo "ALARM_OBJECTS: $(ALARM_OBJECTS)"
	@echo "SCHEDULE_OBJECTS: $(SCHEDULE_OBJECTS)"
	@echo "TRANSFORM_OBJECTS: $(TRANSFORM_OBJECTS)"
	@echo "CSP_OBJECTS: $(CSP_OBJECTS)"
	@echo ""
	@echo "=== Total ==="
	@echo "ALL_OBJECTS count: $(words $(ALL_OBJECTS))"
	@echo ""
	@echo "=== Flags ==="
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "INCLUDES: $(INCLUDES)"
	@echo "LDFLAGS: $(LDFLAGS)"

# =============================================================================
# 테스트 타겟들
# =============================================================================

test: $(TEST_BIN_DIR)/test_integration
	@echo -e "$(BLUE)🧪 Running integration tests...$(NC)"
	@$(TEST_BIN_DIR)/test_integration

$(TEST_BIN_DIR)/test_integration: $(TEST_BUILD_DIR)/test_integration.o $(filter-out $(MAIN_OBJECT), $(ALL_OBJECTS)) | $(TEST_BIN_DIR)
	@echo -e "$(BLUE)🔗 Linking test_integration$(NC)"
	@$(CXX) $^ $(LDFLAGS) -o $@

$(TEST_BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp | $(TEST_BUILD_DIR)
	@echo -e "$(YELLOW)⚙️  Compiling test $<...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 특수 타겟들
# =============================================================================

.PHONY: install uninstall help

install:
	@echo -e "$(BLUE)📦 Installing Export Gateway...$(NC)"
	@mkdir -p /usr/local/bin
	@cp $(TARGET) /usr/local/bin/
	@chmod +x /usr/local/bin/export-gateway$(EXE_SUFFIX)
	@echo -e "$(GREEN)✅ Installation completed$(NC)"

uninstall:
	@echo -e "$(YELLOW)🗑️  Uninstalling Export Gateway...$(NC)"
	@rm -f /usr/local/bin/export-gateway$(EXE_SUFFIX)
	@echo -e "$(GREEN)✅ Uninstallation completed$(NC)"

help:
	@echo "PulseOne Export Gateway - Makefile Commands"
	@echo ""
	@echo "Build Commands:"
	@echo "  make              - Build the project"
	@echo "  make clean        - Clean build files (keep directories)"
	@echo "  make clean-all    - Deep clean (remove directories)"
	@echo "  make run          - Build and run"
	@echo ""
	@echo "Test Commands:"
	@echo "  make test         - Run integration tests"
	@echo ""
	@echo "Utility Commands:"
	@echo "  make check-libs   - Check library dependencies"
	@echo "  make debug-vars   - Show build variables"
	@echo "  make install      - Install to /usr/local/bin"
	@echo "  make uninstall    - Uninstall from /usr/local/bin"
	@echo ""
	@echo "Platform: $(PLATFORM)"
	@echo "Target: $(TARGET)"