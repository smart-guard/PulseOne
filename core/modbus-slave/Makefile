# =============================================================================
# core/modbus-slave/Makefile
# pulseone-modbus-slave — Export Gateway와 동일한 구조를 따름
# =============================================================================

# 플랫폼 감지
UNAME := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME),Linux)
    PLATFORM   := Linux
    IS_WINDOWS := 0
else ifeq ($(OS),Windows_NT)
    PLATFORM   := Windows
    IS_WINDOWS := 1
else
    PLATFORM   := $(UNAME)
    IS_WINDOWS := 0
endif

# 컴파일러
CXX = g++
CC  = gcc

# Windows 크로스 컴파일
ifeq ($(CROSS_COMPILE_WINDOWS),1)
    CXX          = x86_64-w64-mingw32-g++-posix
    CC           = x86_64-w64-mingw32-gcc-posix
    IS_WINDOWS  := 1
    PLATFORM    := Windows-Cross
    MINGW_PREFIX = /usr/x86_64-w64-mingw32
    EXE_SUFFIX   = .exe
    LIB_SUFFIX   = .a
    CXXFLAGS    += -DWIN32 -D_WIN32_WINNT=0x0A00 -DWIN32_LEAN_AND_MEAN -DNOMINMAX
else ifeq ($(IS_WINDOWS),1)
    EXE_SUFFIX   = .exe
    LIB_SUFFIX   = .a
    CXXFLAGS    += -DPULSEONE_WINDOWS=1
else
    EXE_SUFFIX   =
    LIB_SUFFIX   = .a
    CXXFLAGS    += -DPULSEONE_LINUX=1
endif

# 기본 빌드 플래그
CXXFLAGS += -std=c++17 -Wall -Wextra -O0
INCLUDES  = -Iinclude

# 빌드 디렉토리
ifeq ($(CROSS_COMPILE_WINDOWS),1)
    BUILD_DIR = build-win
    BIN_DIR   = bin-windows
else
    BUILD_DIR = build
    BIN_DIR   = bin
endif

# =============================================================================
# 라이브러리 자동 감지
# =============================================================================

# libmodbus
HAS_MODBUS := $(shell pkg-config --exists libmodbus 2>/dev/null && echo "1" || echo "0")
ifeq ($(HAS_MODBUS),0)
    HAS_MODBUS := $(shell echo '\#include <modbus/modbus.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
endif

# hiredis
HAS_HIREDIS := $(shell pkg-config --exists hiredis 2>/dev/null && echo "1" || echo "0")
ifeq ($(HAS_HIREDIS),0)
    HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
endif

# nlohmann/json
HAS_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")

# SQLite3
HAS_SQLITE := $(shell pkg-config --exists sqlite3 2>/dev/null && echo "1" || echo "0")
ifeq ($(HAS_SQLITE),0)
    HAS_SQLITE := $(shell echo '\#include <sqlite3.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
endif

# =============================================================================
# 라이브러리 플래그 적용
# =============================================================================

ifeq ($(HAS_MODBUS),1)
    CXXFLAGS += -DHAS_MODBUS=1
    ifeq ($(CROSS_COMPILE_WINDOWS),1)
        LDFLAGS += -lmodbus -lws2_32
    else
        LDFLAGS += $(shell pkg-config --libs libmodbus 2>/dev/null || echo "-lmodbus")
    endif
    $(info ✅ libmodbus found)
else
    $(info ⚠️  libmodbus not found — stub mode)
endif

ifeq ($(HAS_HIREDIS),1)
    CXXFLAGS += -DHAVE_REDIS=1
    ifeq ($(CROSS_COMPILE_WINDOWS),1)
        LDFLAGS += -lhiredis -lws2_32
    else
        LDFLAGS += $(shell pkg-config --libs hiredis 2>/dev/null || echo "-lhiredis")
    endif
    $(info ✅ hiredis found)
else
    $(info ⚠️  hiredis not found — Redis disabled)
endif

ifeq ($(HAS_JSON),1)
    CXXFLAGS += -DHAS_NLOHMANN_JSON=1
    $(info ✅ nlohmann/json found)
else
    $(info ⚠️  nlohmann/json not found — JSON parsing disabled)
endif

ifeq ($(HAS_SQLITE),1)
    CXXFLAGS += -DHAS_SQLITE=1
    ifeq ($(CROSS_COMPILE_WINDOWS),1)
        LDFLAGS += -lsqlite3
    else
        LDFLAGS += $(shell pkg-config --libs sqlite3 2>/dev/null || echo "-lsqlite3")
    endif
    $(info ✅ sqlite3 found)
else
    $(info ⚠️  sqlite3 not found — DB mode disabled, fallback to DEVICE_IDS env)
endif

# 시스템 라이브러리
ifeq ($(IS_WINDOWS),1)
    LDFLAGS += -lpthread -static-libstdc++ -static-libgcc -lws2_32 -lwsock32
else
    LDFLAGS += -lpthread -lm
endif

# =============================================================================
# 소스 파일
# =============================================================================

SRCS = src/main.cpp \
       src/RegisterTable.cpp \
       src/ModbusSlaveServer.cpp \
       src/RedisSubscriber.cpp \
       src/MappingConfig.cpp \
       src/ClientManager.cpp \
       src/SlaveStatistics.cpp \
       src/DbMappingLoader.cpp \
       src/ModbusSupervisor.cpp

OBJS = $(SRCS:src/%.cpp=$(BUILD_DIR)/%.o)

TARGET = $(BIN_DIR)/pulseone-modbus-slave$(EXE_SUFFIX)

# =============================================================================
# 빌드 타겟
# =============================================================================

.DEFAULT_GOAL := all
.PHONY: all clean check-libs windows-cross

all: check-libs $(TARGET)
	@echo "✅ 빌드 완료: $(TARGET)"

$(TARGET): $(OBJS) | $(BIN_DIR)
	@echo "🔗 링킹 $(TARGET)..."
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	@echo "⚙️  컴파일 $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

check-libs:
	@echo "─── 라이브러리 감지 결과 [$(PLATFORM)] ───"
	@echo "  HAS_MODBUS  : $(HAS_MODBUS)"
	@echo "  HAS_HIREDIS : $(HAS_HIREDIS)"
	@echo "  HAS_JSON    : $(HAS_JSON)"
	@echo "  HAS_SQLITE  : $(HAS_SQLITE)"
	@echo "────────────────────────────────────────"

# Windows 크로스 컴파일 단축 명령
windows-cross:
	$(MAKE) CROSS_COMPILE_WINDOWS=1 all
