# =============================================================================
# PulseOne Collector - 완전한 Makefile (Shared 통합 - 최종 검증 완료)
# =============================================================================

# 기본 설정
SHELL := /bin/bash
.DEFAULT_GOAL := help
MAKEFLAGS += --warn-undefined-variables --no-builtin-rules

# 플랫폼 감지
UNAME_S := $(shell uname -s)
NPROC := 1
MAKEFLAGS += -j1
DEBUG := 1

# 플랫폼별 설정
ifeq ($(UNAME_S),Linux)
PLATFORM := Linux
IS_WINDOWS := 0
IS_LINUX := 1
else ifeq ($(OS),Windows_NT)
PLATFORM := Windows
IS_WINDOWS := 1
IS_LINUX := 0
else
PLATFORM := $(UNAME_S)
IS_WINDOWS := 0
IS_LINUX := 0
endif

# Windows 크로스 컴파일 지원
ifeq ($(CROSS_COMPILE_WINDOWS),1)
PLATFORM := Windows-Cross
IS_WINDOWS := 1
IS_LINUX := 0
CXX := x86_64-w64-mingw32-g++
MINGW_PREFIX := /usr/x86_64-w64-mingw32
endif

# =============================================================================
# 디렉토리 설정
# =============================================================================

SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build
BIN_DIR := bin
DIST_DIR := dist
TARGET := pulseone-collector

# =============================================================================
# 라이브러리 및 Include 경로 설정 (통합)
# =============================================================================

# Shared 라이브러리 경로
SHARED_BASE_DIR := ../shared
SHARED_INCLUDE_DIR := $(SHARED_BASE_DIR)/include
SHARED_LIB_DIR := $(SHARED_BASE_DIR)/lib

# 동적 라이브러리 감지 (강제 활성화)
HAS_SHARED := 1
HAS_NLOHMANN_JSON := 1
HAS_HIREDIS := 1
HAS_HTTPLIB := 1
# QuickJS 강제 활성화
HAS_QUICKJS := 1
HAS_CURL := 1

# 프로토콜 라이브러리 감지 (강제 활성화)
HAS_MODBUS := 1
HAS_MQTT_C := 1
HAS_MQTT_CPP := 1
HAS_BACNET := 1
HAS_BLE := 1
HAS_OPCUA := 1
HAS_ROS := 1
HAS_HTTP_REST := 1

# Collector 전용 Include 경로
INCLUDES := -I$(INCLUDE_DIR) \
            -I$(INCLUDE_DIR)/Config \
            -I$(INCLUDE_DIR)/Alarm \
            -I$(INCLUDE_DIR)/Workers \
            -I$(INCLUDE_DIR)/Workers/Base \
            -I$(INCLUDE_DIR)/Workers/Protocol \
            -I$(INCLUDE_DIR)/Workers/Components \
            -I$(INCLUDE_DIR)/Drivers \
            -I$(INCLUDE_DIR)/Drivers/Common \
            -I$(INCLUDE_DIR)/Pipeline \
            -I$(INCLUDE_DIR)/Storage \
            -I$(INCLUDE_DIR)/VirtualPoint \
            -I$(INCLUDE_DIR)/Scripting \
            -I$(INCLUDE_DIR)/Network \
            -I$(INCLUDE_DIR)/Api \
            -I$(INCLUDE_DIR)/Drivers/OPCUA

# Shared 라이브러리 Include
ifeq ($(HAS_SHARED),1)
INCLUDES += -I$(SHARED_INCLUDE_DIR) \
            -I$(SHARED_INCLUDE_DIR)/Platform \
            -I$(SHARED_INCLUDE_DIR)/Common \
            -I$(SHARED_INCLUDE_DIR)/Utils \
            -I$(SHARED_INCLUDE_DIR)/Database \
            -I$(SHARED_INCLUDE_DIR)/Database/Entities \
            -I$(SHARED_INCLUDE_DIR)/Database/Repositories \
            -I$(SHARED_INCLUDE_DIR)/Data \
            -I$(SHARED_INCLUDE_DIR)/Alarm \
            -I$(SHARED_INCLUDE_DIR)/Export \
            -I$(SHARED_INCLUDE_DIR)/Security \
            -I$(SHARED_INCLUDE_DIR)/Client \
            -I$(SHARED_BASE_DIR)/modules/DbLib/include \
            -I$(SHARED_BASE_DIR)/modules/LogLib/include
endif

# 외부 라이브러리 Include
INCLUDES += -I/usr/local/include
ifeq ($(HAS_QUICKJS),1)
INCLUDES += -I/usr/local/include/quickjs -I/usr/include/quickjs
endif

# =============================================================================
# 컴파일러 및 플래그 설정
# =============================================================================

# 기본 컴파일러
ifeq ($(CROSS_COMPILE_WINDOWS),1)
CXX := x86_64-w64-mingw32-g++
else
CXX := g++
endif

# 공통 컴파일 플래그
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -fPIC
CXXFLAGS += -Wno-unused-parameter -Wno-unused-variable
CXXFLAGS += -fdiagnostics-color=always
CXXFLAGS += -DHAS_NLOHMANN_JSON=$(HAS_NLOHMANN_JSON)
CXXFLAGS += -DHAS_HTTPLIB=$(HAS_HTTPLIB)

# 플랫폼별 플래그
ifeq ($(IS_WINDOWS),1)
CXXFLAGS += -DPULSEONE_WINDOWS=1 -DPULSEONE_LINUX=0
CXXFLAGS += -DWIN32 -D_WIN32_WINNT=0x0601
CXXFLAGS += -DWIN32_LEAN_AND_MEAN -DNOMINMAX
ifeq ($(CROSS_COMPILE_WINDOWS),1)
LDFLAGS := -L$(MINGW_PREFIX)/lib -static
else
LDFLAGS := -Wl,--gc-sections -static
endif
else
    CXXFLAGS += -DPULSEONE_WINDOWS=0 -DPULSEONE_LINUX=1
    CXXFLAGS += -march=native -mtune=native
    LDFLAGS += -L/usr/local/lib
    ifeq ($(UNAME_S),Darwin)
        LDFLAGS += -Wl,-dead_strip
    else
        LDFLAGS += -Wl,--gc-sections
    endif
endif

# 디버그/릴리즈 플래그
ifeq ($(DEBUG),1)
CXXFLAGS += -DDEBUG -g -O0 -fno-omit-frame-pointer
else
CXXFLAGS += -DNDEBUG -O3
endif

# =============================================================================
# 라이브러리 링킹 설정
# =============================================================================

# 기본 시스템 라이브러리
ifeq ($(IS_WINDOWS),1)
BASIC_LIBS := -lpthread -lstdc++ -lm -lsqlite3
PLATFORM_LIBS := -lws2_32 -lwsock32 -liphlpapi -lrpcrt4 -luuid
PLATFORM_LIBS += -lwinmm -lkernel32 -luser32 -ladvapi32
else
BASIC_LIBS := -lpthread -lsqlite3 -lm -ldl
PLATFORM_LIBS :=
endif

# Shared 라이브러리
ifeq ($(HAS_SHARED),1)
SHARED_LIBS := -L$(SHARED_LIB_DIR)
SHARED_LIBS += -lpulseone-alarm
SHARED_LIBS += -lpulseone-export
SHARED_LIBS += -lpulseone-data
SHARED_LIBS += -lpulseone-database
SHARED_LIBS += -lpulseone-client
SHARED_LIBS += -lpulseone-common
SHARED_LIBS += -lpulseone-security
SHARED_LIBS += -lpulseone-dblib
CXXFLAGS += -DHAS_SHARED_LIBS=1
else
SHARED_LIBS :=
endif

# 프로토콜 라이브러리
PROTOCOL_LIBS :=
ifeq ($(HAS_MODBUS),1)
PROTOCOL_LIBS += -lmodbus
CXXFLAGS += -DHAS_MODBUS=1
endif

ifeq ($(HAS_MQTT_CPP),1)
PROTOCOL_LIBS += -lpaho-mqttpp3
CXXFLAGS += -DHAS_MQTT_CPP=1
endif

ifeq ($(HAS_MQTT_C),1)
PROTOCOL_LIBS += -lpaho-mqtt3c
CXXFLAGS += -DHAS_MQTT=1
endif

ifeq ($(HAS_BACNET),1)
PROTOCOL_LIBS += -lbacnet
CXXFLAGS += -DHAS_BACNET=1
endif

# BLE for Linux (BlueZ)
ifeq ($(HAS_BLE),1)
PROTOCOL_LIBS += -lbluetooth
CXXFLAGS += -DHAS_BLUETOOTH=1
endif

ifeq ($(HAS_OPCUA),1)
PROTOCOL_LIBS += -lopen62541
CXXFLAGS += -DHAS_OPCUA=1
endif

ifeq ($(HAS_ROS),1)
CXXFLAGS += -DHAS_ROS=1
endif

ifeq ($(HAS_HTTP_REST),1)
CXXFLAGS += -DHAS_HTTP_REST=1
endif

# 유틸리티 라이브러리
UTILITY_LIBS :=
ifeq ($(HAS_HIREDIS),1)
UTILITY_LIBS += -lhiredis
CXXFLAGS += -DHAVE_REDIS=1
endif

ifeq ($(HAS_HTTPLIB),1)
CXXFLAGS += -DHAVE_HTTPLIB=1
else
CXXFLAGS += -DHAVE_HTTPLIB=0
endif

ifeq ($(HAS_QUICKJS),1)
QUICKJS_LIB_PATH := $(shell find /usr/local/lib -name "libquickjs.a" 2>/dev/null | head -1)
ifneq ($(QUICKJS_LIB_PATH),)
UTILITY_LIBS += $(QUICKJS_LIB_PATH)
else
QUICKJS_LIBS := $(shell pkg-config --libs quickjs 2>/dev/null || echo "-lquickjs")
UTILITY_LIBS += $(QUICKJS_LIBS)
endif
CXXFLAGS += -DHAS_QUICKJS=1
endif

ifeq ($(HAS_CURL),1)
UTILITY_LIBS += $(shell pkg-config --libs libcurl)
CXXFLAGS += -DHAVE_CURL=1
endif

# 최종 라이브러리 링킹 순서
LIBS := $(SHARED_LIBS) $(PROTOCOL_LIBS) $(UTILITY_LIBS) -lpqxx -lpq -lmysqlclient $(BASIC_LIBS) $(PLATFORM_LIBS)

# =============================================================================
# 소스 파일 수집
# =============================================================================

# Core 소스
CORE_SOURCES := $(wildcard $(SRC_DIR)/Core/*.cpp)

# Config 소스
CONFIG_SOURCES := $(wildcard $(SRC_DIR)/Config/*.cpp)

# Alarm 소스
ALARM_SOURCES := $(wildcard $(SRC_DIR)/Alarm/*.cpp)

# Workers 소스
WORKERS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp)
WORKERS_SOURCES += $(wildcard $(SRC_DIR)/Workers/Components/*.cpp)
WORKERS_SOURCES += $(wildcard $(SRC_DIR)/Workers/*.cpp)

# 프로토콜별 워커 필터링
ifeq ($(HAS_MODBUS),1)
WORKERS_SOURCES += $(wildcard $(SRC_DIR)/Workers/Protocol/Modbus*.cpp)
endif

ifeq ($(HAS_BACNET),1)
WORKERS_SOURCES += $(wildcard $(SRC_DIR)/Workers/Protocol/BACnet*.cpp)
endif

ifneq ($(HAS_MQTT_C)$(HAS_MQTT_CPP),00)
WORKERS_SOURCES += $(wildcard $(SRC_DIR)/Workers/Protocol/MQTT*.cpp)
endif

ifeq ($(HAS_OPCUA),1)
WORKERS_SOURCES += $(wildcard $(SRC_DIR)/Workers/Protocol/OPCUA*.cpp)
endif

ifeq ($(HAS_BLE),1)
WORKERS_SOURCES += $(wildcard $(SRC_DIR)/Workers/Protocol/Ble*.cpp)
endif

ifeq ($(HAS_ROS),1)
WORKERS_SOURCES += $(wildcard $(SRC_DIR)/Workers/Protocol/ROS*.cpp)
endif

ifeq ($(HAS_HTTP_REST),1)
WORKERS_SOURCES += $(wildcard $(SRC_DIR)/Workers/Protocol/Http*.cpp)
endif

# Pipeline 및 Storage
PIPELINE_SOURCES = $(wildcard $(SRC_DIR)/Pipeline/*.cpp) $(wildcard $(SRC_DIR)/Pipeline/Stages/*.cpp)
STORAGE_SOURCES := $(wildcard $(SRC_DIR)/Storage/*.cpp)

# VirtualPoint
VIRTUALPOINT_SOURCES := $(wildcard $(SRC_DIR)/VirtualPoint/*.cpp)

# Scripting
SCRIPTING_SOURCES := $(wildcard $(SRC_DIR)/Scripting/*.cpp)

# 드라이버 소스
DRIVERS_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Common/*.cpp)

ifeq ($(HAS_MODBUS),1)
DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Modbus/*.cpp)
endif

ifeq ($(HAS_MQTT_CPP),1)
DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Mqtt/*.cpp)
endif

ifeq ($(HAS_BACNET),1)
DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Bacnet/*.cpp)
endif

# 추가 드라이버 (항시 포함)
DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Ble/*.cpp)
DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/OPCUA/*.cpp)
DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/OPCUA/*.c)
# DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/HttpRest/*.cpp)
DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Ros/*.cpp)

# 네트워크 및 API
ifeq ($(HAS_HTTPLIB),1)
NETWORK_SOURCES := $(wildcard $(SRC_DIR)/Network/*.cpp)
API_SOURCES := $(wildcard $(SRC_DIR)/Api/*.cpp)
else
NETWORK_SOURCES :=
API_SOURCES :=
endif

# 전체 소스 파일 목록
ALL_SOURCES := $(CORE_SOURCES) $(CONFIG_SOURCES) $(ALARM_SOURCES)
ALL_SOURCES += $(WORKERS_SOURCES) $(DRIVERS_SOURCES) $(PIPELINE_SOURCES)
ALL_SOURCES += $(STORAGE_SOURCES) $(VIRTUALPOINT_SOURCES) $(SCRIPTING_SOURCES)
ALL_SOURCES += $(NETWORK_SOURCES) $(API_SOURCES)

# 오브젝트 파일 (cpp -> o, c -> o 자동 변환)
ALL_OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(filter %.cpp,$(ALL_SOURCES)))
ALL_OBJECTS += $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(filter %.c,$(ALL_SOURCES)))
MAIN_OBJECT := $(BUILD_DIR)/main.o

# =============================================================================
# 빌드 디렉토리 자동 생성
# =============================================================================

BUILD_SUBDIRS := $(BUILD_DIR)/Core $(BUILD_DIR)/Config $(BUILD_DIR)/Alarm
BUILD_SUBDIRS += $(BUILD_DIR)/Workers $(BUILD_DIR)/Workers/Base
BUILD_SUBDIRS += $(BUILD_DIR)/Workers/Protocol $(BUILD_DIR)/Workers/Components
BUILD_SUBDIRS += $(BUILD_DIR)/Drivers $(BUILD_DIR)/Drivers/Common
BUILD_SUBDIRS += $(BUILD_DIR)/Drivers/Modbus $(BUILD_DIR)/Drivers/Mqtt $(BUILD_DIR)/Drivers/Bacnet
BUILD_SUBDIRS += $(BUILD_DIR)/Drivers/Ble $(BUILD_DIR)/Drivers/OPCUA $(BUILD_DIR)/Drivers/HttpRest $(BUILD_DIR)/Drivers/Ros
BUILD_SUBDIRS += $(BUILD_DIR)/Pipeline $(BUILD_DIR)/Storage
BUILD_SUBDIRS += $(BUILD_DIR)/VirtualPoint $(BUILD_DIR)/Scripting $(BUILD_DIR)/Network $(BUILD_DIR)/Api

# =============================================================================
# 색상 정의
# =============================================================================

GREEN := \033[0;32m
BLUE := \033[0;34m
YELLOW := \033[1;33m
RED := \033[0;31m
CYAN := \033[0;36m
NC := \033[0m

# =============================================================================
# 메인 타겟들
# =============================================================================

.PHONY: all debug release windows-cross clean distclean check-env status run help create-dirs

all: check-env create-dirs $(TARGET)
	@echo -e "$(GREEN)✅ Build completed for $(PLATFORM)!$(NC)"
	@echo -e "Binary: $(CYAN)$(BIN_DIR)/$(TARGET)$(NC)"
	@echo -e "Sources: $(CYAN)$(words $(ALL_SOURCES))$(NC) files"

debug:
	$(MAKE) DEBUG=1 all

release:
	$(MAKE) DEBUG=0 all

windows-cross:
	@echo -e "$(YELLOW)🔧 Building for Windows (cross-compile)...$(NC)"
	$(MAKE) CROSS_COMPILE_WINDOWS=1 all

# =============================================================================
# 디렉토리 생성
# =============================================================================

create-dirs: | $(BUILD_DIR) $(BIN_DIR) $(DIST_DIR)
	@mkdir -p $(BUILD_SUBDIRS)

$(BUILD_DIR) $(BIN_DIR) $(DIST_DIR):
	@mkdir -p $@

# =============================================================================
# 컴파일 규칙
# =============================================================================

$(TARGET): $(ALL_OBJECTS) $(MAIN_OBJECT) | $(BIN_DIR)
	@echo -e "$(BLUE)🔗 Linking $(TARGET)...$(NC)"
	$(CXX) $(LDFLAGS) -o $(BIN_DIR)/$@ $(ALL_OBJECTS) $(MAIN_OBJECT) $(LIBS)
	@echo -e "$(GREEN)✅ $(TARGET) built successfully!$(NC)"
	@echo -e "Size: $(CYAN)$(shell du -h $(BIN_DIR)/$(TARGET) 2>/dev/null | cut -f1 || echo 'N/A')$(NC)"

# 컴파일 규칙 (.c 추가)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | create-dirs
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️ Compiling $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | create-dirs
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️ Compiling $< (C source)$(NC)"
	gcc -O2 -fPIC $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp | create-dirs
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️ Compiling main.cpp$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 의존성 생성
# =============================================================================

$(BUILD_DIR)/%.d: $(SRC_DIR)/%.cpp | create-dirs
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(BUILD_DIR)/$*.o $< > $@

-include $(ALL_OBJECTS:.o=.d)

# =============================================================================
# 유틸리티 타겟들
# =============================================================================

check-env:
	@echo -e "$(BLUE)🔍 Environment check for $(PLATFORM)...$(NC)"
	@echo -e "Platform: $(CYAN)$(PLATFORM)$(NC)"
	@echo -e "Compiler: $(CYAN)$(CXX)$(NC)"
	@echo -e "Shared libs: $(if $(HAS_SHARED),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "Sources: $(CYAN)$(words $(ALL_SOURCES))$(NC) files"
	@echo -e "Protocol support:"
	@echo -e "  Modbus: $(if $(HAS_MODBUS),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  MQTT: $(if $(HAS_MQTT_C),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  MQTT++: $(if $(HAS_MQTT_CPP),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  BACnet: $(if $(HAS_BACNET),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  QuickJS: $(if $(HAS_QUICKJS),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  Curl: $(if $(HAS_CURL),$(GREEN)✅$(NC),$(RED)❌$(NC))"

status:
	@echo -e "$(BLUE)📊 Build Status$(NC)"
	@echo -e "Platform: $(CYAN)$(PLATFORM)$(NC)"
	@echo -e "Target: $(if $(wildcard $(BIN_DIR)/$(TARGET)),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "Objects: $(CYAN)$(words $(wildcard $(BUILD_DIR)/**/*.o))$(NC)"

run: $(TARGET)
	@echo -e "$(GREEN)🚀 Running $(TARGET)...$(NC)"
	$(BIN_DIR)/$(TARGET)

clean:
	@echo -e "$(YELLOW)🧹 Cleaning build artifacts...$(NC)"
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo -e "$(GREEN)✅ Clean completed$(NC)"

distclean:
	@echo -e "$(RED)🗑️ Complete cleanup...$(NC)"
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(DIST_DIR)
	@echo -e "$(GREEN)✅ Complete cleanup finished$(NC)"

help:
	@echo -e "$(BLUE)🔧 PulseOne Collector Build System$(NC)"
	@echo ""
	@echo -e "$(GREEN)Building:$(NC)"
	@echo "  make all              - Build with Shared libraries"
	@echo "  make debug            - Debug build (-g -O0)"
	@echo "  make release          - Release build (-O3 -flto)"
	@echo "  make windows-cross    - Cross-compile for Windows"
	@echo "  make run              - Build and run"
	@echo ""
	@echo -e "$(GREEN)Maintenance:$(NC)"
	@echo "  make clean            - Clean build artifacts"
	@echo "  make distclean        - Complete cleanup"
	@echo ""
	@echo -e "$(GREEN)Utilities:$(NC)"
	@echo "  make check-env        - Check build environment"
	@echo "  make status           - Show build status"
	@echo ""
	@echo -e "$(YELLOW)Architecture:$(NC)"
	@echo "  Shared: Platform, Common, Utils, Database, Client, Security, Data"
	@echo "  Collector: Config, Workers, Drivers, Pipeline, Storage, Alarm"