# =============================================================================
# shared/Makefile - PulseOne 공유 라이브러리 (OpenSSL + Security + Data 지원)
# 🔥 Redis 활성화 수정 완료 (HAVE_REDIS=1 플래그 추가)
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
CXX ?= g++
AR ?= ar
RANLIB ?= ranlib
CXXFLAGS = -std=c++17 -Wall -Wextra -O0 -DHAS_SHARED_LIBS=1

# Windows 크로스 컴파일 지원
ifeq ($(CROSS_COMPILE_WINDOWS),1)
    CXX = x86_64-w64-mingw32-g++
    AR = x86_64-w64-mingw32-ar
    RANLIB = x86_64-w64-mingw32-ranlib
    CXXFLAGS += -DWIN32 -D_WIN32_WINNT=0x0A00 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DDBLIB_STATIC -DLOGLIB_STATIC -DCURL_STATICLIB -DPULSEONE_STATIC
    PLATFORM := Windows-Cross
    MINGW_PREFIX = /usr/x86_64-w64-mingw32
    INCLUDES += -I$(MINGW_PREFIX)/include
    LIB_SUFFIX = .a
else ifeq ($(IS_WINDOWS),1)
    CXXFLAGS += -DPULSEONE_WINDOWS=1
    LIB_SUFFIX = .a
else
    CXXFLAGS += -DPULSEONE_LINUX=1
    LIB_SUFFIX = .a
endif

# 디렉토리 설정
SRC_DIR = src
INCLUDE_DIR = include
BUILD_ROOT = build
BUILD_DIR = $(BUILD_ROOT)/$(PLATFORM)
LIB_ROOT = lib
LIB_DIR = $(LIB_ROOT)/$(PLATFORM)

# Include 경로 (Data 추가!)
INCLUDES = -I$(INCLUDE_DIR) \
           -I$(INCLUDE_DIR)/Platform \
           -I$(INCLUDE_DIR)/Common \
           -I$(INCLUDE_DIR)/Utils \
           -I$(INCLUDE_DIR)/Security \
           -I$(INCLUDE_DIR)/Database \
           -I$(INCLUDE_DIR)/Database/Entities \
           -I$(INCLUDE_DIR)/Database/Repositories \
           -I$(INCLUDE_DIR)/Client \
           -I$(INCLUDE_DIR)/Data \
           -I$(INCLUDE_DIR)/Alarm \
           -I$(INCLUDE_DIR)/Logging \
           -I$(INCLUDE_DIR)/Event \
           -Imodules/LogLib/include \
           -Imodules/DbLib/include \
           -I../collector/include

# =============================================================================
# 라이브러리 감지 (OpenSSL 추가!)
# =============================================================================

HAS_CURL := $(shell pkg-config --exists libcurl 2>/dev/null && echo "1" || echo "0")
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_PQXX := $(shell pkg-config --exists libpqxx 2>/dev/null && echo "1" || echo "0")

# 🔥 OpenSSL 감지 (S3Client용 - 필수!)
HAS_OPENSSL := $(shell pkg-config --exists openssl 2>/dev/null && echo "1" || echo "0")
ifeq ($(HAS_OPENSSL),0)
    HAS_OPENSSL := $(shell echo '\#include <openssl/sha.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
endif

# =============================================================================
# 조건부 컴파일 플래그
# =============================================================================

ifeq ($(HAS_CURL),1)
    CXXFLAGS += -DHAS_CURL
    LDFLAGS += $(shell pkg-config --libs libcurl)
endif

ifneq ($(HAS_NLOHMANN_JSON),0)
    CXXFLAGS += -DHAS_NLOHMANN_JSON
endif

ifeq ($(HAS_PQXX),1)
    CXXFLAGS += -DHAS_PQXX=1
endif

# 🔥🔥🔥 Redis 플래그 수정 (핵심!)
ifeq ($(HAS_HIREDIS),1)
    CXXFLAGS += -DHAS_HIREDIS -DHAVE_REDIS=1
    HIREDIS_CFLAGS := $(shell pkg-config --cflags hiredis 2>/dev/null)
    CXXFLAGS += $(HIREDIS_CFLAGS)
    $(info ✅ Redis enabled: -DHAVE_REDIS=1)
endif

# 🔥 OpenSSL 컴파일 플래그 (가장 중요!)
ifeq ($(HAS_OPENSSL),1)
    OPENSSL_LIBS := $(shell pkg-config --libs openssl 2>/dev/null)
    OPENSSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null)
    ifneq ($(OPENSSL_LIBS),)
        CXXFLAGS += -DHAS_OPENSSL=1 $(OPENSSL_CFLAGS)
        LDFLAGS += $(OPENSSL_LIBS)
        $(info ✅ OpenSSL found via pkg-config)
    else
        CXXFLAGS += -DHAS_OPENSSL=1
        LDFLAGS += -lssl -lcrypto
        $(info ✅ OpenSSL found (direct link))
    endif
else
    $(warning ⚠️  OpenSSL not found - S3 AWS Signature V4 will NOT work!)
    $(warning     Install: sudo apt-get install libssl-dev pkg-config)
endif

# =============================================================================
# 소스 파일 수집 (Data 추가!)
# =============================================================================

COMMON_SOURCES = $(wildcard $(SRC_DIR)/Common/*.cpp) \
                $(wildcard $(SRC_DIR)/Utils/*.cpp) \
                $(wildcard $(SRC_DIR)/Platform/*.cpp) \
                src/Logging/LogManager.cpp \
                src/Logging/LogLevelManager.cpp \
                $(wildcard modules/LogLib/src/*.cpp)

# 🆕 DbLib 소스
DBLIB_SOURCES = $(wildcard modules/DbLib/src/*.cpp)

SECURITY_SOURCES = $(wildcard $(SRC_DIR)/Security/*.cpp)

DATABASE_SOURCES = $(wildcard $(SRC_DIR)/Database/*.cpp) \
                  $(wildcard $(SRC_DIR)/Database/Entities/*.cpp) \
                  $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)

CLIENT_SOURCES = $(wildcard $(SRC_DIR)/Client/*.cpp)

# 🆕 Data 소스 (DataReader, DataWriter)
DATA_SOURCES = $(wildcard $(SRC_DIR)/Data/*.cpp)

EXPORT_SOURCES = $(wildcard $(SRC_DIR)/Export/*.cpp)

ALARM_SOURCES = $(wildcard $(SRC_DIR)/Alarm/*.cpp)
EVENT_SOURCES = $(wildcard $(SRC_DIR)/Event/*.cpp)

# 오브젝트 파일 (src/ 아래 소스들)
COMMON_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(filter $(SRC_DIR)/%,$(COMMON_SOURCES))) \
                 $(patsubst modules/%.cpp,$(BUILD_DIR)/%.o,$(filter modules/%,$(COMMON_SOURCES)))
SECURITY_OBJECTS = $(SECURITY_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
DATABASE_OBJECTS = $(DATABASE_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
CLIENT_OBJECTS = $(CLIENT_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
DATA_OBJECTS = $(DATA_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
EXPORT_OBJECTS = $(EXPORT_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
ALARM_OBJECTS = $(ALARM_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
EVENT_OBJECTS = $(EVENT_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
DBLIB_OBJECTS = $(patsubst modules/DbLib/src/%.cpp,$(BUILD_DIR)/DbLib/src/%.o,$(DBLIB_SOURCES))

# 라이브러리 파일들 (Data 추가!)
COMMON_LIB = $(LIB_DIR)/libpulseone-common$(LIB_SUFFIX)
SECURITY_LIB = $(LIB_DIR)/libpulseone-security$(LIB_SUFFIX)
DATABASE_LIB = $(LIB_DIR)/libpulseone-database$(LIB_SUFFIX)
CLIENT_LIB = $(LIB_DIR)/libpulseone-client$(LIB_SUFFIX)
DATA_LIB = $(LIB_DIR)/libpulseone-data$(LIB_SUFFIX)
EXPORT_LIB = $(LIB_DIR)/libpulseone-export$(LIB_SUFFIX)
ALARM_LIB = $(LIB_DIR)/libpulseone-alarm$(LIB_SUFFIX)
EVENT_LIB = $(LIB_DIR)/libpulseone-event$(LIB_SUFFIX)
DBLIB_LIB = $(LIB_DIR)/libpulseone-dblib$(LIB_SUFFIX)

# 색상 정의
GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m

# =============================================================================
# 메인 타겟들
# =============================================================================

.PHONY: all clean common security database client data export alarm event dblib test-dblib test-headers help info openssl-check

all: $(BUILD_DIR) $(LIB_DIR) common security database client data export alarm event dblib
	@echo -e "$(GREEN)✅ 모든 공유 라이브러리 빌드 완료 [$(PLATFORM)]$(NC)"

# 디렉토리 생성 (Data 추가!)
$(BUILD_DIR) $(LIB_DIR):
	@mkdir -p $@
	@mkdir -p $(BUILD_DIR)/Common $(BUILD_DIR)/Utils $(BUILD_DIR)/Platform
	@mkdir -p $(BUILD_DIR)/Security
	@mkdir -p $(BUILD_DIR)/Database/Entities $(BUILD_DIR)/Database/Repositories
	@mkdir -p $(BUILD_DIR)/Client
	@mkdir -p $(BUILD_DIR)/Data
	@mkdir -p $(BUILD_DIR)/Export
	@mkdir -p $(BUILD_DIR)/Alarm
	@mkdir -p $(BUILD_DIR)/Event
	@mkdir -p $(BUILD_DIR)/LogLib
	@mkdir -p $(BUILD_DIR)/DbLib/src
	@mkdir -p $(BUILD_DIR)/Logging

# =============================================================================
# 라이브러리 빌드 (Data 추가!)
# =============================================================================

common: $(COMMON_LIB)
$(COMMON_LIB): $(COMMON_OBJECTS) | $(LIB_DIR)
	@echo -e "$(BLUE)🔧 Building libpulseone-common [$(PLATFORM)]$(NC)"
ifneq ($(COMMON_OBJECTS),)
	@$(AR) rcs $@ $(COMMON_OBJECTS)
	@echo "🔍 Checking symbols in libpulseone-common:"
	@nm -C $@ | grep "LogManager::Info" || echo "❌ LogManager::Info symbol MISSING!"
	@nm -C $@ | grep "ConfigManager::reload" || echo "❌ ConfigManager::reload symbol MISSING!"
	@$(RANLIB) $@
else
	@echo '/* Header-only library - dummy object */' > /tmp/dummy.c && $(CXX) -c /tmp/dummy.c -o /tmp/dummy.o
	@$(AR) rcs $@ /tmp/dummy.o && rm -f /tmp/dummy.c /tmp/dummy.o
endif

security: $(SECURITY_LIB)
$(SECURITY_LIB): $(SECURITY_OBJECTS) | $(LIB_DIR)
	@echo -e "$(BLUE)🔧 Building libpulseone-security [$(PLATFORM)]$(NC)"
ifneq ($(SECURITY_OBJECTS),)
	@$(AR) rcs $@ $(SECURITY_OBJECTS)
	@$(RANLIB) $@
else
	@echo '/* Header-only library - dummy object */' > /tmp/dummy.c && $(CXX) -c /tmp/dummy.c -o /tmp/dummy.o
	@$(AR) rcs $@ /tmp/dummy.o && rm -f /tmp/dummy.c /tmp/dummy.o
endif

database: $(DATABASE_LIB)
$(DATABASE_LIB): $(DATABASE_OBJECTS) | $(LIB_DIR)
	@echo -e "$(BLUE)🔧 Building libpulseone-database [$(PLATFORM)]$(NC)"
ifneq ($(DATABASE_OBJECTS),)
	@$(AR) rcs $@ $(DATABASE_OBJECTS)
	@echo "🔍 Checking symbols in libpulseone-database:"
	@nm -C $@ | grep "RepositoryFactory::getInstance" || echo "❌ RepositoryFactory::getInstance symbol MISSING!"
	@$(RANLIB) $@
else
	@echo '/* Header-only library - dummy object */' > /tmp/dummy.c && $(CXX) -c /tmp/dummy.c -o /tmp/dummy.o
	@$(AR) rcs $@ /tmp/dummy.o && rm -f /tmp/dummy.c /tmp/dummy.o
endif

client: $(CLIENT_LIB)
$(CLIENT_LIB): $(CLIENT_OBJECTS) | $(LIB_DIR)
	@echo -e "$(BLUE)🔧 Building libpulseone-client [$(PLATFORM)]$(NC)"
ifneq ($(CLIENT_OBJECTS),)
	@$(AR) rcs $@ $(CLIENT_OBJECTS)
	@$(RANLIB) $@
else
	@echo '/* Header-only library - dummy object */' > /tmp/dummy.c && $(CXX) -c /tmp/dummy.c -o /tmp/dummy.o
	@$(AR) rcs $@ /tmp/dummy.o && rm -f /tmp/dummy.c /tmp/dummy.o
endif

# 🆕 Data 라이브러리 빌드 (DataReader, DataWriter)
data: $(DATA_LIB)
$(DATA_LIB): $(DATA_OBJECTS) | $(LIB_DIR)
	@echo -e "$(BLUE)🔧 Building libpulseone-data [$(PLATFORM)]$(NC)"
ifneq ($(DATA_OBJECTS),)
	@$(AR) rcs $@ $(DATA_OBJECTS)
	@$(RANLIB) $@
	@echo -e "$(GREEN)   ✅ DataReader.cpp, DataWriter.cpp compiled$(NC)"
else
	@echo '/* Header-only library - dummy object */' > /tmp/dummy_data.c && $(CXX) -c /tmp/dummy_data.c -o /tmp/dummy_data.o
	@$(AR) rcs $@ /tmp/dummy_data.o && rm -f /tmp/dummy_data.c /tmp/dummy_data.o
endif

export: $(EXPORT_LIB)
$(EXPORT_LIB): $(EXPORT_OBJECTS) | $(LIB_DIR)
	@echo -e "$(BLUE)🔧 Building libpulseone-export [$(PLATFORM)]$(NC)"
ifneq ($(EXPORT_OBJECTS),)
	@$(AR) rcs $@ $(EXPORT_OBJECTS)
	@$(RANLIB) $@
else
	@echo '/* Header-only library - dummy object */' > /tmp/dummy_export.c && $(CXX) -c /tmp/dummy_export.c -o /tmp/dummy_export.o
	@$(AR) rcs $@ /tmp/dummy_export.o && rm -f /tmp/dummy_export.c /tmp/dummy_export.o
endif

alarm: $(ALARM_LIB)
$(ALARM_LIB): $(ALARM_OBJECTS) | $(LIB_DIR)
	@echo -e "$(BLUE)🔧 Building libpulseone-alarm [$(PLATFORM)]$(NC)"
ifneq ($(ALARM_OBJECTS),)
	@$(AR) rcs $@ $(ALARM_OBJECTS)
	@$(RANLIB) $@
else
	@echo '/* Header-only library - dummy object */' > /tmp/dummy.c && $(CXX) -c /tmp/dummy.c -o /tmp/dummy.o
	@$(AR) rcs $@ /tmp/dummy.o && rm -f /tmp/dummy.c /tmp/dummy.o
endif

# 🆕 DbLib 라이브러리 빌드
dblib: $(DBLIB_LIB)
$(DBLIB_LIB): $(DBLIB_OBJECTS) | $(LIB_DIR)
	@echo -e "$(BLUE)🔧 Building libpulseone-dblib [$(PLATFORM)]$(NC)"
ifneq ($(DBLIB_OBJECTS),)
	@$(AR) rcs $@ $(DBLIB_OBJECTS)
	@$(RANLIB) $@
else
	@echo '/* Header-only library - dummy object */' > /tmp/dummy.c && $(CXX) -c /tmp/dummy.c -o /tmp/dummy.o
	@$(AR) rcs $@ /tmp/dummy.o && rm -f /tmp/dummy.c /tmp/dummy.o
endif

# 🆕 Event 라이브러리 빌드
event: $(EVENT_LIB)
$(EVENT_LIB): $(EVENT_OBJECTS) | $(LIB_DIR)
	@echo -e "$(BLUE)🔧 Building libpulseone-event [$(PLATFORM)]$(NC)"
ifneq ($(EVENT_OBJECTS),)
	@$(AR) rcs $@ $(EVENT_OBJECTS)
	@$(RANLIB) $@
else
	@echo '/* Header-only library - dummy object */' > /tmp/dummy_event.c && $(CXX) -c /tmp/dummy_event.c -o /tmp/dummy_event.o
	@$(AR) rcs $@ /tmp/dummy_event.o && rm -f /tmp/dummy_event.c /tmp/dummy_event.o
endif

# =============================================================================
# 오브젝트 파일 컴파일
# =============================================================================

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️ Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# LogLib 소스 컴파일용 별도 규칙
$(BUILD_DIR)/LogLib/src/%.o: modules/LogLib/src/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️ Compiling LogLib $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# DbLib 소스 컴파일용 별도 규칙
$(BUILD_DIR)/DbLib/src/%.o: modules/DbLib/src/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️ Compiling DbLib $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# DbLib 테스트 빌드 및 실행
test-dblib: dblib common client security
	@echo -e "$(BLUE)🧪 Building DbLib Standalone Test...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) modules/DbLib/tests/test_dblib_core.cpp \
		$(DBLIB_LIB) $(CLIENT_LIB) $(COMMON_LIB) $(SECURITY_LIB) \
		-lsqlite3 -lpqxx -lpq -lmysqlclient -lhiredis $(LDFLAGS) \
		-o $(BUILD_DIR)/test_dblib_core
	@echo -e "$(GREEN)🚀 Running DbLib Standalone Test...$(NC)"
	@./$(BUILD_DIR)/test_dblib_core

# =============================================================================
# OpenSSL 확인 타겟 (디버깅용)
# =============================================================================

openssl-check:
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "🔒 OpenSSL 설정 확인 (Shared 라이브러리)"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "HAS_OPENSSL = $(HAS_OPENSSL)"
ifeq ($(HAS_OPENSSL),1)
	@echo "✅ OpenSSL 활성화됨"
	@echo "   CXXFLAGS: $(CXXFLAGS)" | grep -o "HAS_OPENSSL=1" && echo "   ✅ -DHAS_OPENSSL=1 플래그 적용됨" || echo "   ❌ 플래그 누락!"
	@pkg-config --modversion openssl 2>/dev/null && echo "   OpenSSL 버전: $$(pkg-config --modversion openssl)" || echo "   버전 확인 불가"
else
	@echo "❌ OpenSSL 비활성화됨 - S3Client가 dummy_hash 사용!"
	@echo "   해결: sudo apt-get install libssl-dev pkg-config"
endif
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# 🔥 Redis 설정 확인 타겟 추가
redis-check:
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "🔴 Redis/hiredis 설정 확인 (Shared 라이브러리)"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "HAS_HIREDIS = $(HAS_HIREDIS)"
ifeq ($(HAS_HIREDIS),1)
	@echo "✅ hiredis 활성화됨"
	@echo "   컴파일 플래그 확인:"
	@echo "$(CXXFLAGS)" | grep -q "DHAVE_REDIS=1" && echo "     ✅ -DHAVE_REDIS=1 적용됨" || echo "     ❌ -DHAVE_REDIS=1 누락!"
	@echo "$(CXXFLAGS)" | grep -q "DHAS_HIREDIS" && echo "     ✅ -DHAS_HIREDIS 적용됨" || echo "     ❌ -DHAS_HIREDIS 누락!"
	@pkg-config --exists hiredis && echo "   hiredis pkg-config: ✅ 사용 가능" || echo "   hiredis pkg-config: ❌ 없음"
	@pkg-config --modversion hiredis 2>/dev/null && echo "   hiredis 버전: $$(pkg-config --modversion hiredis)" || echo "   버전 확인 불가"
else
	@echo "❌ hiredis 비활성화됨 - Redis가 시뮬레이션 모드로 동작!"
	@echo "   해결: sudo apt-get install libhiredis-dev"
endif
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# =============================================================================
# 테스트 및 검증 (Data 헤더 추가!)
# =============================================================================

test-headers:
	@echo -e "$(BLUE)🔍 Testing shared header files [$(PLATFORM)]...$(NC)"
	@echo "Testing Common headers..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only include/Common/BasicTypes.h 2>/dev/null && echo "  ✅ BasicTypes.h" || echo "  ❌ BasicTypes.h"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only include/Common/Enums.h 2>/dev/null && echo "  ✅ Enums.h" || echo "  ❌ Enums.h"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only include/Common/Structs.h 2>/dev/null && echo "  ✅ Structs.h" || echo "  ❌ Structs.h"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only include/Common/Utils.h 2>/dev/null && echo "  ✅ Utils.h" || echo "  ❌ Utils.h"
	@echo "Testing Utils headers..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only include/Utils/LogManager.h 2>/dev/null && echo "  ✅ LogManager.h" || echo "  ❌ LogManager.h"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only include/Utils/ConfigManager.h 2>/dev/null && echo "  ✅ ConfigManager.h" || echo "  ❌ ConfigManager.h"
	@echo "Testing Security headers..."
	@if [ -f include/Security/SecretManager.h ]; then $(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only include/Security/SecretManager.h 2>/dev/null && echo "  ✅ SecretManager.h" || echo "  ❌ SecretManager.h"; fi
	@echo "Testing Data headers..."
	@if [ -f include/Data/DataReader.h ]; then $(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only include/Data/DataReader.h 2>/dev/null && echo "  ✅ DataReader.h" || echo "  ❌ DataReader.h"; fi
	@if [ -f include/Data/DataWriter.h ]; then $(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only include/Data/DataWriter.h 2>/dev/null && echo "  ✅ DataWriter.h" || echo "  ❌ DataWriter.h"; fi
	@if [ -f include/Data/RedisDataTypes.h ]; then $(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only include/Data/RedisDataTypes.h 2>/dev/null && echo "  ✅ RedisDataTypes.h" || echo "  ❌ RedisDataTypes.h"; fi
	@echo "Testing Alarm headers..."
	@if [ -f include/Alarm/AlarmTypes.h ]; then $(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only include/Alarm/AlarmTypes.h 2>/dev/null && echo "  ✅ AlarmTypes.h" || echo "  ❌ AlarmTypes.h"; fi
	@echo "Testing Export headers..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only include/Export/ExportTypes.h 2>/dev/null && echo "  ✅ ExportTypes.h" || echo "  ❌ ExportTypes.h"
	@echo -e "$(GREEN)✅ Header syntax check completed [$(PLATFORM)]$(NC)"

test-compile:
	@echo -e "$(BLUE)🧪 Testing compilation [$(PLATFORM)]...$(NC)"
	@echo '#include "Common/BasicTypes.h"' > /tmp/test_shared.cpp
	@echo '#include "Export/ExportTypes.h"' >> /tmp/test_shared.cpp
	@if [ -f include/Security/SecretManager.h ]; then echo '#include "Security/SecretManager.h"' >> /tmp/test_shared.cpp; fi
	@if [ -f include/Data/DataReader.h ]; then echo '#include "Data/DataReader.h"' >> /tmp/test_shared.cpp; fi
	@if [ -f include/Data/DataWriter.h ]; then echo '#include "Data/DataWriter.h"' >> /tmp/test_shared.cpp; fi
	@if [ -f include/Alarm/AlarmTypes.h ]; then echo '#include "Alarm/AlarmTypes.h"' >> /tmp/test_shared.cpp; fi
	@echo 'int main() { return 0; }' >> /tmp/test_shared.cpp
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c /tmp/test_shared.cpp -o /tmp/test_shared.o 2>/dev/null && echo "  ✅ Compilation test passed" || echo "  ❌ Compilation test failed"
	@rm -f /tmp/test_shared.cpp /tmp/test_shared.o

# =============================================================================
# 크로스 컴파일 지원
# =============================================================================

windows-cross:
	@echo -e "$(YELLOW)🔧 Building for Windows (cross-compile)...$(NC)"
	$(MAKE) CROSS_COMPILE_WINDOWS=1 all

# =============================================================================
# 정보 및 상태 (Data 추가!)
# =============================================================================

info:
	@echo -e "$(BLUE)📋 PulseOne Shared Library Build Info$(NC)"
	@echo "=================================="
	@echo "Platform: $(PLATFORM)"
	@echo "Compiler: $(CXX)"
	@echo "Flags: $(CXXFLAGS)"
	@echo "Header files: $(shell find $(INCLUDE_DIR) -name '*.h' 2>/dev/null | wc -l)"
	@echo "Source files: $(words $(COMMON_SOURCES) $(SECURITY_SOURCES) $(DATABASE_SOURCES) $(CLIENT_SOURCES) $(DATA_SOURCES) $(EXPORT_SOURCES) $(ALARM_SOURCES))"
	@echo ""
	@echo "Library Support:"
	@echo "CURL (libcurl): $(if $(filter 1,$(HAS_CURL)),✅ available,❌ missing)"
	@echo "JSON (nlohmann): $(if $(filter 1,$(HAS_NLOHMANN_JSON)),✅ available,❌ missing)"
	@echo "Redis (hiredis): $(if $(filter 1,$(HAS_HIREDIS)),✅ available,❌ missing)"
	@echo "OpenSSL: $(if $(filter 1,$(HAS_OPENSSL)),✅ available,❌ missing) $(if $(filter 1,$(HAS_OPENSSL)),<- S3 서명 가능,<- S3 서명 불가!)"
	@echo ""
	@echo "Libraries:"
	@echo "libpulseone-common: $(if $(wildcard $(COMMON_LIB)),✅ exists,❌ missing)"
	@echo "libpulseone-security: $(if $(wildcard $(SECURITY_LIB)),✅ exists,❌ missing)"
	@echo "libpulseone-database: $(if $(wildcard $(DATABASE_LIB)),✅ exists,❌ missing)"
	@echo "libpulseone-client: $(if $(wildcard $(CLIENT_LIB)),✅ exists,❌ missing)"
	@echo "libpulseone-data: $(if $(wildcard $(DATA_LIB)),✅ exists (DataReader/Writer),❌ missing)"
	@echo "libpulseone-export: $(if $(wildcard $(EXPORT_LIB)),✅ exists,❌ missing)"
	@echo "libpulseone-export: $(if $(wildcard $(EXPORT_LIB)),✅ exists,❌ missing)"
	@echo "libpulseone-alarm: $(if $(wildcard $(ALARM_LIB)),✅ exists,❌ missing)"
	@echo ""
	@echo "DEBUG: COMMON_SOURCES = $(COMMON_SOURCES)"
	@echo "DEBUG: DATABASE_SOURCES = $(DATABASE_SOURCES)"

# =============================================================================
# 유틸리티
# =============================================================================

clean:
	@echo -e "$(YELLOW)🧹 Cleaning shared libraries [$(PLATFORM)]...$(NC)"
	@rm -rf $(BUILD_DIR)
	@rm -rf $(LIB_DIR)/*.a $(LIB_DIR)/*.so 2>/dev/null || true
	@echo -e "$(GREEN)✅ Clean completed$(NC)"

install: all
	@echo -e "$(BLUE)📦 Installing shared libraries [$(PLATFORM)]...$(NC)"
ifeq ($(IS_WINDOWS),1)
	@echo "Windows install not implemented yet"
else
	@sudo mkdir -p /usr/local/lib/pulseone /usr/local/include/pulseone
	@sudo cp -r $(LIB_DIR)/* /usr/local/lib/pulseone/ 2>/dev/null || true
	@sudo cp -r $(INCLUDE_DIR)/* /usr/local/include/pulseone/ 2>/dev/null || true
	@sudo ldconfig 2>/dev/null || true
	@echo -e "$(GREEN)✅ Installation completed$(NC)"
endif

help:
	@echo -e "$(BLUE)🚀 PulseOne Shared Library Makefile$(NC)"
	@echo "======================================"
	@echo ""
	@echo -e "$(GREEN)Main Targets:$(NC)"
	@echo "  make              - Build all shared libraries (기본)"
	@echo "  make all          - Build all shared libraries"
	@echo "  make common       - Build common library (BasicTypes, Utils, etc.)"
	@echo "  make security     - Build security library (SecretManager, etc.)"
	@echo "  make database     - Build database library (Entities, Repositories)"
	@echo "  make client       - Build client library (S3Client, Redis, InfluxDB)"
	@echo "  make data         - Build data library (DataReader, DataWriter) 🆕"
	@echo "  make export       - Build export library (Export types)"
	@echo "  make alarm        - Build alarm library (Alarm types)"
	@echo ""
	@echo -e "$(GREEN)Testing:$(NC)"
	@echo "  make test-headers - Test header file syntax"
	@echo "  make test-compile - Test compilation"
	@echo "  make openssl-check - Check OpenSSL configuration"
	@echo "  make redis-check  - Check Redis/hiredis configuration 🆕"
	@echo ""
	@echo -e "$(GREEN)Cross-Compilation:$(NC)"
	@echo "  make windows-cross - Build for Windows (requires mingw-w64)"
	@echo ""
	@echo -e "$(GREEN)Utilities:$(NC)"
	@echo "  make info         - Show build information"
	@echo "  make clean        - Clean build files"
	@echo "  make install      - Install to system (Linux only)"
	@echo "  make help         - Show this help"
	@echo ""
	@echo -e "$(YELLOW)Current Platform: $(PLATFORM)$(NC)"
	@echo -e "$(YELLOW)OpenSSL Status: $(if $(filter 1,$(HAS_OPENSSL)),✅ Enabled,❌ Disabled)$(NC)"
	@echo -e "$(YELLOW)Redis Status: $(if $(filter 1,$(HAS_HIREDIS)),✅ Enabled with HAVE_REDIS=1,❌ Disabled)$(NC)"
	@echo -e "$(YELLOW)Data Library: DataReader.cpp + DataWriter.cpp 🆕$(NC)"

# =============================================================================
# 🔥 기본 타겟 변경: make만 입력해도 빌드되도록!
# =============================================================================

.DEFAULT_GOAL := all