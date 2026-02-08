# =============================================================================
# core/export-gateway/Makefile - PulseOne Export Gateway (크로스플랫폼)
# 🔥 v3.1 - Paho MQTT C++ 라이브러리 지원 추가
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

# 🔥 hiredis 감지
HAS_HIREDIS := $(shell pkg-config --exists hiredis 2>/dev/null && echo "1" || echo "0")
ifeq ($(HAS_HIREDIS),0)
    HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
endif

HAS_HTTPLIB := $(shell echo '\#include <httplib.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")

# 🔥 libcurl 자동 감지
HAS_CURL := $(shell pkg-config --exists libcurl 2>/dev/null && echo "1" || echo "0")
ifeq ($(HAS_CURL),0)
    HAS_CURL := $(shell echo '\#include <curl/curl.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
endif

# OpenSSL 감지 (S3 서명용)
HAS_OPENSSL := $(shell pkg-config --exists openssl 2>/dev/null && echo "1" || echo "0")
ifeq ($(HAS_OPENSSL),0)
    HAS_OPENSSL := $(shell echo '\#include <openssl/sha.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
endif

# 🔥 libpqxx 감지 (DBLib 의존성)
HAS_PQXX := $(shell pkg-config --exists libpqxx 2>/dev/null && echo "1" || echo "0")

# 🔥 Paho MQTT C++ 라이브러리 감지 (v3.1 추가)
HAS_PAHO_MQTT := $(shell [ -f /usr/include/mqtt/async_client.h ] && echo "1" || echo "0")

# =============================================================================
# Include 경로들 (🔥 로컬 include를 최우선으로!)
# =============================================================================

# 🔥 1순위: 로컬 include (export-gateway 자체)
INCLUDES = -I$(INCLUDE_DIR)
INCLUDES += -I$(PROJECT_ROOT)/core/collector/include

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
    INCLUDES += -I$(SHARED_DIR)/modules/DbLib/include
    INCLUDES += -I$(SHARED_DIR)/modules/LogLib/include
endif

# 🔥 3순위: 시스템 include
INCLUDES += -I/usr/local/include

# 🔥 4순위: pkg-config 기반 include (hiredis, curl, openssl 등)
ifeq ($(HAS_HIREDIS),1)
    INCLUDES += $(shell pkg-config --cflags hiredis 2>/dev/null)
endif
ifeq ($(HAS_CURL),1)
    INCLUDES += $(shell pkg-config --cflags libcurl 2>/dev/null)
endif
ifeq ($(HAS_OPENSSL),1)
    INCLUDES += $(shell pkg-config --cflags openssl 2>/dev/null)
endif

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
    SHARED_LIBS += -lpulseone-dblib
    SHARED_LIBS += -lpulseone-client
    SHARED_LIBS += -lpulseone-common
    SHARED_LIBS += -lpulseone-security
    SHARED_LIBS += -L/usr/local/lib
endif

# 외부 라이브러리들
UTILITY_LIBS =

# 🔥 hiredis 링크
ifeq ($(HAS_HIREDIS),1)
    HIREDIS_LIBS := $(shell pkg-config --libs hiredis 2>/dev/null)
    ifneq ($(HIREDIS_LIBS),)
        UTILITY_LIBS += $(HIREDIS_LIBS)
        CXXFLAGS += -DHAVE_REDIS=1
        $(info ✅ hiredis found via pkg-config)
    else
        UTILITY_LIBS += /usr/local/lib/libhiredis.so.1.1.0
        CXXFLAGS += -DHAVE_REDIS=1
        $(info ✅ hiredis found (direct link))
    endif
else
    $(info ⚠️  hiredis not found - Redis features disabled)
endif

# 🔥 libcurl 링크
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

# 🔥 Paho MQTT C++ 링크 (v3.1 추가 - Collector와 동일)
# 🔥 Paho MQTT C++ 링크 (v3.1 추가 - Collector와 동일)
# Linker order matters: -lpaho-mqttpp3 must come before -lpaho-mqtt3a
ifeq ($(HAS_PAHO_MQTT),1)
    UTILITY_LIBS += -lpaho-mqttpp3 -lpaho-mqtt3a
    CXXFLAGS += -DHAS_PAHO_MQTT=1
    $(info ✅ Paho MQTT C++ found and linked)
else
    $(info ⚠️  Paho MQTT C++ not found - MQTT features disabled)
    # Do NOT force HAS_PAHO_MQTT=1 here if library is missing
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
    ifeq ($(HAS_PQXX),1)
        BASIC_LIBS += -lpqxx
    endif
    ifeq ($(PLATFORM),Linux)
        BASIC_LIBS += -lmariadb
    endif
    PLATFORM_LIBS =
endif

# 🔥 최종 링크 순서
LDFLAGS = $(SHARED_LIBS) $(UTILITY_LIBS) $(BASIC_LIBS) $(PLATFORM_LIBS)

# 색상 정의
GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m

# =============================================================================
# 소스 파일 및 오브젝트 파일 (v3.0 - Event 디렉토리)
# =============================================================================

# 🆕 Event 디렉토리 (EventSubscriber - 범용 이벤트 구독자) ✅
EVENT_SOURCES = $(wildcard $(SRC_DIR)/Event/*.cpp)
EVENT_OBJECTS = $(EVENT_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 🆕 Gateway Model 디렉토리
MODEL_SOURCES = $(wildcard $(SRC_DIR)/Gateway/Model/*.cpp)
MODEL_OBJECTS = $(MODEL_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 🆕 Gateway Service 디렉토리
SERVICE_SOURCES = $(wildcard $(SRC_DIR)/Gateway/Service/*.cpp)
SERVICE_OBJECTS = $(SERVICE_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 🆕 Gateway Target 디렉토리
TARGET_SOURCES = $(wildcard $(SRC_DIR)/Gateway/Target/*.cpp)
TARGET_OBJECTS = $(TARGET_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 🆕 Schedule 디렉토리 (ScheduledExporter)
SCHEDULE_SOURCES = $(wildcard $(SRC_DIR)/Schedule/*.cpp)
SCHEDULE_OBJECTS = $(SCHEDULE_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 🔄 Transform 디렉토리 (PayloadTransformer)
TRANSFORM_SOURCES = $(wildcard $(SRC_DIR)/Transform/*.cpp)
TRANSFORM_OBJECTS = $(TRANSFORM_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 🔌 CSP 디렉토리 (DynamicTargetManager, MqttTargetHandler 등)
CSP_SOURCES = $(wildcard $(SRC_DIR)/CSP/*.cpp)
CSP_OBJECTS = $(CSP_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 📤 Export 디렉토리
EXPORT_SOURCES = $(wildcard $(SRC_DIR)/Export/*.cpp)
EXPORT_OBJECTS = $(EXPORT_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 🛠️ Utils 디렉토리
UTILS_CXX_SOURCES = $(wildcard $(SRC_DIR)/Utils/*.cpp)
UTILS_C_SOURCES = $(wildcard $(SRC_DIR)/Utils/*.c)
UTILS_OBJECTS = $(UTILS_CXX_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o) \
                $(UTILS_C_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# 📌 Main 파일
MAIN_SOURCES = $(SRC_DIR)/main.cpp
MAIN_OBJECT = $(BUILD_DIR)/main.o

# 📦 전체 오브젝트 파일 (EVENT_OBJECTS로 변경) ✅
ALL_OBJECTS = $(EVENT_OBJECTS) $(MODEL_OBJECTS) $(SERVICE_OBJECTS) $(TARGET_OBJECTS) $(SCHEDULE_OBJECTS) $(TRANSFORM_OBJECTS) $(CSP_OBJECTS) $(EXPORT_OBJECTS) $(UTILS_OBJECTS) $(MAIN_OBJECT)

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

.PHONY: all clean clean-all run test test-mqtt check-libs debug-vars

# 기본 타겟
all: check-libs $(TARGET)
	@echo -e "$(GREEN)✅ Build completed: $(TARGET)$(NC)"

# 최종 링크
$(TARGET): $(ALL_OBJECTS) | $(BIN_DIR)
	@echo -e "$(BLUE)🔗 Linking $@$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(LDFLAGS) -o $@
	@echo -e "$(GREEN)✅ Export Gateway v3.1 built successfully!$(NC)"
	@echo -e "$(GREEN)   EventSubscriber: 범용 이벤트 구독자$(NC)"
	@echo -e "$(GREEN)   MqttTargetHandler: Paho MQTT C++ 지원$(NC)"

# 디렉토리 생성
$(BUILD_DIR) $(BIN_DIR) $(TEST_BUILD_DIR) $(TEST_BIN_DIR):
	@mkdir -p $@

# 컴파일 규칙
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️  Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️  Compiling C source $< [$(PLATFORM)]$(NC)"
	@$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

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
	@echo -e "$(BLUE)🚀 Running Export Gateway v3.1...$(NC)"
	@$(TARGET)

# ✅ check-libs (라이브러리 확인)
check-libs:
	@echo -e "$(BLUE)🔍 Checking PulseOne Shared Libraries [$(PLATFORM)]...$(NC)"
	@echo "Shared lib directory: $(SHARED_LIB_DIR)"
	@echo "HAS_SHARED: $(HAS_SHARED)"
	@echo "HAS_HIREDIS: $(HAS_HIREDIS)"
	@echo "HAS_CURL: $(HAS_CURL)"
	@echo "HAS_OPENSSL: $(HAS_OPENSSL)"
	@echo "HAS_PAHO_MQTT: $(HAS_PAHO_MQTT)"
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
	@echo "=== Library Detection ==="
	@echo "HAS_SHARED: $(HAS_SHARED)"
	@echo "HAS_HIREDIS: $(HAS_HIREDIS)"
	@echo "HAS_CURL: $(HAS_CURL)"
	@echo "HAS_OPENSSL: $(HAS_OPENSSL)"
	@echo "HAS_PAHO_MQTT: $(HAS_PAHO_MQTT)"
	@echo ""
	@echo "=== Source Files (v3.1) ==="
	@echo "EVENT_SOURCES: $(EVENT_SOURCES)"
	@echo "SCHEDULE_SOURCES: $(SCHEDULE_SOURCES)"
	@echo "TRANSFORM_SOURCES: $(TRANSFORM_SOURCES)"
	@echo "CSP_SOURCES: $(CSP_SOURCES)"
	@echo "EXPORT_SOURCES: $(EXPORT_SOURCES)"
	@echo "UTILS_SOURCES: $(UTILS_SOURCES)"
	@echo "MAIN_SOURCES: $(MAIN_SOURCES)"
	@echo ""
	@echo "=== Object Files ==="
	@echo "EVENT_OBJECTS: $(EVENT_OBJECTS)"
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

# 🔥 MQTT 핸들러 테스트 (단위 테스트 Makefile 호출)
test-mqtt:
	@echo -e "$(BLUE)🧪 Running MQTT handler tests...$(NC)"
	@$(MAKE) -C tests/unit test-mqtt

# 🔥 Paho MQTT C++ 설치 (Ubuntu/Debian)
install-paho-mqtt:
	@echo -e "$(BLUE)📦 Installing Paho MQTT C++ library...$(NC)"
	@apt-get update && apt-get install -y \
		libpaho-mqttpp-dev libpaho-mqttpp3-1 \
		libpaho-mqtt-dev libpaho-mqtt1.3
	@echo -e "$(GREEN)✅ Paho MQTT C++ installed$(NC)"
	@echo -e "$(YELLOW)💡 Run 'make clean && make' to rebuild with MQTT support$(NC)"

$(TEST_BIN_DIR)/test_integration: $(TEST_BUILD_DIR)/test_integration.o $(filter-out $(MAIN_OBJECT), $(ALL_OBJECTS)) | $(TEST_BIN_DIR)
	@echo -e "$(BLUE)🔗 Linking test_integration$(NC)"
	@$(CXX) $^ $(LDFLAGS) -o $@

$(TEST_BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp | $(TEST_BUILD_DIR)
	@echo -e "$(YELLOW)⚙️  Compiling test $<...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 특수 타겟들
# =============================================================================

.PHONY: install uninstall help version install-paho-mqtt

install:
	@echo -e "$(BLUE)📦 Installing Export Gateway v3.1...$(NC)"
	@mkdir -p /usr/local/bin
	@cp $(TARGET) /usr/local/bin/
	@chmod +x /usr/local/bin/export-gateway$(EXE_SUFFIX)
	@echo -e "$(GREEN)✅ Installation completed$(NC)"

uninstall:
	@echo -e "$(YELLOW)🗑️  Uninstalling Export Gateway...$(NC)"
	@rm -f /usr/local/bin/export-gateway$(EXE_SUFFIX)
	@echo -e "$(GREEN)✅ Uninstallation completed$(NC)"

version:
	@echo "PulseOne Export Gateway v3.1"
	@echo "EventSubscriber: 범용 Redis Pub/Sub 이벤트 구독자"
	@echo "ScheduledExporter: 스케줄 기반 데이터 Export"
	@echo "MqttTargetHandler: Paho MQTT C++ (Collector와 동일)"
	@echo "Platform: $(PLATFORM)"

help:
	@echo "PulseOne Export Gateway v3.1 - Makefile Commands"
	@echo ""
	@echo "Build Commands:"
	@echo "  make              - Build the project"
	@echo "  make clean        - Clean build files (keep directories)"
	@echo "  make clean-all    - Deep clean (remove directories)"
	@echo "  make run          - Build and run"
	@echo ""
	@echo "Test Commands:"
	@echo "  make test         - Run integration tests"
	@echo "  make test-mqtt    - Run MQTT handler unit tests"
	@echo ""
	@echo "Install Commands:"
	@echo "  make install-paho-mqtt - Install Paho MQTT C++ library"
	@echo "  make install      - Install to /usr/local/bin"
	@echo "  make uninstall    - Uninstall from /usr/local/bin"
	@echo ""
	@echo "Utility Commands:"
	@echo "  make check-libs   - Check library dependencies"
	@echo "  make debug-vars   - Show build variables"
	@echo "  make version      - Show version info"
	@echo ""
	@echo "Platform: $(PLATFORM)"
	@echo "Target: $(TARGET)"
	@echo ""
	@echo "v3.1 Changes:"
	@echo "  - Paho MQTT C++ 라이브러리 지원 추가"
	@echo "  - MqttTargetHandler: Collector와 동일한 async_client 사용"
	@echo "  - HAS_PAHO_MQTT 자동 감지"