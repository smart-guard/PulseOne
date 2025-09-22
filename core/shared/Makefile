# =============================================================================
# shared/Makefile - PulseOne 공유 라이브러리 (크로스 플랫폼)
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
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -fPIC

# Windows 크로스 컴파일 지원
ifeq ($(CROSS_COMPILE_WINDOWS),1)
    CXX = x86_64-w64-mingw32-g++
    CXXFLAGS += -DWIN32 -D_WIN32_WINNT=0x0601 -DWIN32_LEAN_AND_MEAN -DNOMINMAX
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
BUILD_DIR = build
LIB_DIR = lib

# Include 경로 (collector 패턴과 동일)
INCLUDES = -I$(INCLUDE_DIR) \
           -I$(INCLUDE_DIR)/Platform \
           -I$(INCLUDE_DIR)/Common \
           -I$(INCLUDE_DIR)/Utils \
           -I$(INCLUDE_DIR)/Database \
           -I$(INCLUDE_DIR)/Database/Entities \
           -I$(INCLUDE_DIR)/Database/Repositories \
           -I$(INCLUDE_DIR)/Client \
           -I$(INCLUDE_DIR)/Export

# 라이브러리 감지 (collector 패턴)
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")

# 조건부 플래그
ifeq ($(HAS_NLOHMANN_JSON),1)
    CXXFLAGS += -DHAS_NLOHMANN_JSON
endif
ifeq ($(HAS_HIREDIS),1)
    CXXFLAGS += -DHAS_HIREDIS
endif

# 소스 파일 수집 (현재는 헤더 전용이지만 향후 확장 대비)
COMMON_SOURCES = $(wildcard $(SRC_DIR)/Common/*.cpp) \
                $(wildcard $(SRC_DIR)/Utils/*.cpp) \
                $(wildcard $(SRC_DIR)/Platform/*.cpp)

DATABASE_SOURCES = $(wildcard $(SRC_DIR)/Database/*.cpp) \
                  $(wildcard $(SRC_DIR)/Database/Entities/*.cpp) \
                  $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)

CLIENT_SOURCES = $(wildcard $(SRC_DIR)/Client/*.cpp)

EXPORT_SOURCES = $(wildcard $(SRC_DIR)/Export/*.cpp)

# 오브젝트 파일
COMMON_OBJECTS = $(COMMON_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
DATABASE_OBJECTS = $(DATABASE_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
CLIENT_OBJECTS = $(CLIENT_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
EXPORT_OBJECTS = $(EXPORT_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 라이브러리 파일들
COMMON_LIB = $(LIB_DIR)/libpulseone-common$(LIB_SUFFIX)
DATABASE_LIB = $(LIB_DIR)/libpulseone-database$(LIB_SUFFIX)
CLIENT_LIB = $(LIB_DIR)/libpulseone-client$(LIB_SUFFIX)
EXPORT_LIB = $(LIB_DIR)/libpulseone-export$(LIB_SUFFIX)

# 색상 정의
GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m

# =============================================================================
# 메인 타겟들
# =============================================================================

.PHONY: all clean common database client export test-headers help info

all: $(BUILD_DIR) $(LIB_DIR) common database client export
	@echo -e "$(GREEN)✅ 모든 공유 라이브러리 빌드 완료 [$(PLATFORM)]$(NC)"

# 디렉토리 생성
$(BUILD_DIR) $(LIB_DIR):
	@mkdir -p $@
	@mkdir -p $(BUILD_DIR)/Common $(BUILD_DIR)/Utils $(BUILD_DIR)/Platform
	@mkdir -p $(BUILD_DIR)/Database/Entities $(BUILD_DIR)/Database/Repositories
	@mkdir -p $(BUILD_DIR)/Client $(BUILD_DIR)/Export

# =============================================================================
# 라이브러리 빌드 (헤더 전용, 향후 소스 추가 대비)
# =============================================================================

common: $(COMMON_LIB)
$(COMMON_LIB): $(COMMON_OBJECTS) | $(LIB_DIR)
	@echo -e "$(BLUE)🔧 Building libpulseone-common [$(PLATFORM)]$(NC)"
ifneq ($(COMMON_OBJECTS),)
	@ar rcs $@ $(COMMON_OBJECTS)
	@ranlib $@ 2>/dev/null || true
else
	@echo "# Header-only library" > /tmp/dummy.c && $(CXX) -c /tmp/dummy.c -o /tmp/dummy.o
	@ar rcs $@ /tmp/dummy.o && rm -f /tmp/dummy.c /tmp/dummy.o
endif

database: $(DATABASE_LIB)
$(DATABASE_LIB): $(DATABASE_OBJECTS) | $(LIB_DIR)
	@echo -e "$(BLUE)🔧 Building libpulseone-database [$(PLATFORM)]$(NC)"
ifneq ($(DATABASE_OBJECTS),)
	@ar rcs $@ $(DATABASE_OBJECTS)
	@ranlib $@ 2>/dev/null || true
else
	@echo "# Header-only library" > /tmp/dummy.c && $(CXX) -c /tmp/dummy.c -o /tmp/dummy.o
	@ar rcs $@ /tmp/dummy.o && rm -f /tmp/dummy.c /tmp/dummy.o
endif

client: $(CLIENT_LIB)
$(CLIENT_LIB): $(CLIENT_OBJECTS) | $(LIB_DIR)
	@echo -e "$(BLUE)🔧 Building libpulseone-client [$(PLATFORM)]$(NC)"
ifneq ($(CLIENT_OBJECTS),)
	@ar rcs $@ $(CLIENT_OBJECTS)
	@ranlib $@ 2>/dev/null || true
else
	@echo "# Header-only library" > /tmp/dummy.c && $(CXX) -c /tmp/dummy.c -o /tmp/dummy.o
	@ar rcs $@ /tmp/dummy.o && rm -f /tmp/dummy.c /tmp/dummy.o
endif

export: $(EXPORT_LIB)
$(EXPORT_LIB): $(EXPORT_OBJECTS) | $(LIB_DIR)
	@echo -e "$(BLUE)🔧 Building libpulseone-export [$(PLATFORM)]$(NC)"
ifneq ($(EXPORT_OBJECTS),)
	@ar rcs $@ $(EXPORT_OBJECTS)
	@ranlib $@ 2>/dev/null || true
else
	@echo "# Header-only library" > /tmp/dummy.c && $(CXX) -c /tmp/dummy.c -o /tmp/dummy.o
	@ar rcs $@ /tmp/dummy.o && rm -f /tmp/dummy.c /tmp/dummy.o
endif

# =============================================================================
# 오브젝트 파일 컴파일
# =============================================================================

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️ Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 테스트 및 검증
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
	@echo "Testing Export headers..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only include/Export/ExportTypes.h 2>/dev/null && echo "  ✅ ExportTypes.h" || echo "  ❌ ExportTypes.h"
	@echo -e "$(GREEN)✅ Header syntax check completed [$(PLATFORM)]$(NC)"

test-compile:
	@echo -e "$(BLUE)🧪 Testing compilation [$(PLATFORM)]...$(NC)"
	@echo '#include "Common/BasicTypes.h"' > /tmp/test_shared.cpp
	@echo '#include "Export/ExportTypes.h"' >> /tmp/test_shared.cpp
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
# 정보 및 상태
# =============================================================================

info:
	@echo -e "$(BLUE)📋 PulseOne Shared Library Build Info$(NC)"
	@echo "=================================="
	@echo "Platform: $(PLATFORM)"
	@echo "Compiler: $(CXX)"
	@echo "Flags: $(CXXFLAGS)"
	@echo "Header files: $(shell find $(INCLUDE_DIR) -name '*.h' 2>/dev/null | wc -l)"
	@echo "Source files: $(words $(COMMON_SOURCES) $(DATABASE_SOURCES) $(CLIENT_SOURCES) $(EXPORT_SOURCES))"
	@echo ""
	@echo "Library Support:"
	@echo "JSON (nlohmann): $(if $(filter 1,$(HAS_NLOHMANN_JSON)),✅ available,❌ missing)"
	@echo "Redis (hiredis): $(if $(filter 1,$(HAS_HIREDIS)),✅ available,❌ missing)"
	@echo ""
	@echo "Libraries:"
	@echo "libpulseone-common: $(if $(wildcard $(COMMON_LIB)),✅ exists,❌ missing)"
	@echo "libpulseone-database: $(if $(wildcard $(DATABASE_LIB)),✅ exists,❌ missing)"
	@echo "libpulseone-client: $(if $(wildcard $(CLIENT_LIB)),✅ exists,❌ missing)"
	@echo "libpulseone-export: $(if $(wildcard $(EXPORT_LIB)),✅ exists,❌ missing)"

# =============================================================================
# 유틸리티
# =============================================================================

clean:
	@echo -e "$(YELLOW)🧹 Cleaning shared libraries [$(PLATFORM)]...$(NC)"
	@rm -rf $(BUILD_DIR) $(LIB_DIR)
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
	@echo "  make all          - Build all shared libraries"
	@echo "  make common       - Build common library (BasicTypes, Utils, etc.)"
	@echo "  make database     - Build database library (Entities, Repositories)"
	@echo "  make client       - Build client library (Redis, InfluxDB clients)"
	@echo "  make export       - Build export library (Export types)"
	@echo ""
	@echo -e "$(GREEN)Testing:$(NC)"
	@echo "  make test-headers - Test header file syntax"
	@echo "  make test-compile - Test compilation"
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
	@echo -e "$(YELLOW)Header files: $(shell find $(INCLUDE_DIR) -name '*.h' 2>/dev/null | wc -l)$(NC)"

.DEFAULT_GOAL := help
