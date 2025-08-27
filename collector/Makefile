# =============================================================================
# PulseOne Collector 메인 Makefile - 전체 모듈 완전 통합 완성본
# /app/collector/Makefile
# 🔥 API, Network, Alarm, VirtualPoint, Platform 모든 새 모듈 완전 포함
# =============================================================================

# CPU 코어 수 자동 감지 및 병렬 처리 설정
NPROC := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += -j$(NPROC)

# 🔧 플랫폼 감지
UNAME := $(shell uname -s)
ifeq ($(UNAME),Linux)
    PLATFORM := Linux
    IS_WINDOWS := 0
    IS_LINUX := 1
else ifeq ($(OS),Windows_NT)
    PLATFORM := Windows
    IS_WINDOWS := 1
    IS_LINUX := 0
else
    PLATFORM := $(UNAME)
    IS_WINDOWS := 0
    IS_LINUX := 0
endif

# 🔧 라이브러리 체크 (동적 감지) - 모든 필수 라이브러리 포함
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_C := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3c >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_AS := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3as >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_CPP := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqttpp3 >/dev/null 2>&1 && echo "1" || echo "0")
HAS_BACNET_STACK := $(shell [ -f "/usr/local/lib/libbacnet.a" ] && echo "1" || echo "0")
HAS_HTTPLIB := $(shell echo '\#include <httplib.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_QUICKJS := $(shell pkg-config --exists quickjs && echo "1" || echo "0")

# 컴파일러 설정
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -fPIC \
           -Wno-unused-but-set-variable -Wno-unused-variable \
           -Wno-unused-parameter \
           -fdiagnostics-color=always
LDFLAGS = 

# 🔧 플랫폼별 컴파일 플래그
ifeq ($(IS_WINDOWS),1)
    CXXFLAGS += -DPULSEONE_WINDOWS=1 -DPULSEONE_LINUX=0 \
                -D_WIN32_WINNT=0x0A00 -DWIN32_LEAN_AND_MEAN -DNOMINMAX
    PLATFORM_LIBS = -lws2_32 -liphlpapi -lkernel32 -luser32
else
    CXXFLAGS += -DPULSEONE_WINDOWS=0 -DPULSEONE_LINUX=1
    PLATFORM_LIBS = 
endif

# 경로 설정
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin
TARGET = pulseone-collector

# 🔥 핵심: 모든 새 모듈 헤더 경로 완전 포함
INCLUDES = -I$(INCLUDE_DIR) \
           -I$(INCLUDE_DIR)/Api \
           -I$(INCLUDE_DIR)/Network \
           -I$(INCLUDE_DIR)/Platform \
           -I$(INCLUDE_DIR)/Alarm \
           -I$(INCLUDE_DIR)/VirtualPoint \
           -I$(INCLUDE_DIR)/Core \
           -I$(INCLUDE_DIR)/Workers \
           -I$(INCLUDE_DIR)/Workers/Base \
           -I$(INCLUDE_DIR)/Workers/Protocol \
           -I$(INCLUDE_DIR)/Workers/Components \
           -I$(INCLUDE_DIR)/Database \
           -I$(INCLUDE_DIR)/Database/Entities \
           -I$(INCLUDE_DIR)/Database/Repositories \
           -I$(INCLUDE_DIR)/Common \
           -I$(INCLUDE_DIR)/Utils \
           -I$(INCLUDE_DIR)/Config \
           -I$(INCLUDE_DIR)/Drivers \
           -I$(INCLUDE_DIR)/Drivers/Common \
           -I$(INCLUDE_DIR)/Pipeline \
           -I$(INCLUDE_DIR)/Client

# =============================================================================
# 라이브러리 설정 (조건부 링킹) - 모든 새 라이브러리 포함
# =============================================================================

# 기본 라이브러리 (플랫폼별 라이브러리 포함)
BASIC_LIBS = -lpthread -lsqlite3 -lm -ldl $(PLATFORM_LIBS)

# Modbus 라이브러리
ifneq ($(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lmodbus >/dev/null 2>&1; echo $$?),0)
    $(warning ⚠️  libmodbus not found - Modbus features will be disabled)
    MODBUS_LIBS = 
    MODBUS_INCLUDES = 
else
    MODBUS_LIBS = -lmodbus
    MODBUS_INCLUDES = $(shell pkg-config --cflags libmodbus 2>/dev/null || echo "")
    INCLUDES += $(MODBUS_INCLUDES)
    $(info ✅ Modbus found)
endif

# MySQL 라이브러리
MYSQL_CONFIG := $(shell which mysql_config 2>/dev/null)
ifneq ($(MYSQL_CONFIG),)
    MYSQL_LIBS = $(shell mysql_config --libs)
    MYSQL_INCLUDES = $(shell mysql_config --include)
    INCLUDES += $(MYSQL_INCLUDES)
    $(info ✅ MySQL found: $(MYSQL_CONFIG))
else
    $(warning ⚠️  mysql_config not found - MySQL features will be disabled)
    MYSQL_LIBS = 
    CXXFLAGS += -DDISABLE_MYSQL_FEATURES
endif

# 🔧 MQTT 라이브러리 (개선된 감지 - 비동기 SSL 지원)
MQTT_LIBS = 
MQTT_STATUS = 

# MQTT C 라이브러리 (동기)
ifeq ($(HAS_MQTT_C),1)
    MQTT_LIBS += -lpaho-mqtt3c
    MQTT_STATUS += "MQTT-C:✅ "
else
    MQTT_STATUS += "MQTT-C:❌ "
endif

# MQTT C 비동기 SSL 라이브러리 (핵심!)
ifeq ($(HAS_MQTT_AS),1)
    MQTT_LIBS += -lpaho-mqtt3as
    MQTT_STATUS += "MQTT-AS:✅ "
else
    MQTT_STATUS += "MQTT-AS:❌ "
endif

# MQTT C++ 라이브러리
ifeq ($(HAS_MQTT_CPP),1)
    MQTT_LIBS += -lpaho-mqttpp3
    MQTT_STATUS += "MQTT-CPP:✅"
    INCLUDES += -I/usr/local/include
else
    MQTT_STATUS += "MQTT-CPP:❌"
endif

# MQTT 상태 출력
ifneq ($(MQTT_LIBS),)
    $(info ✅ MQTT Libraries: $(MQTT_LIBS))
    $(info    Status: $(MQTT_STATUS))
else
    $(warning ⚠️  No MQTT libraries found - MQTT features disabled)
endif

# PostgreSQL 라이브러리
ifneq ($(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpqxx >/dev/null 2>&1; echo $$?),0)
    $(warning ⚠️  libpqxx not found - PostgreSQL features will be disabled)
    PGSQL_LIBS = 
else
    PGSQL_LIBS = -lpqxx -lpq
    $(info ✅ PostgreSQL found)
endif

# BACnet 라이브러리
ifeq ($(HAS_BACNET_STACK), 1)
    BACNET_LIBS = -lbacnet -lm
    BACNET_INCLUDES = -I/usr/local/include/bacnet
    INCLUDES += $(BACNET_INCLUDES)
    CXXFLAGS += -DHAS_BACNET_STACK=1
    $(info ✅ BACnet found)
else
    CXXFLAGS += -DHAS_BACNET_STACK=0
    BACNET_LIBS = 
    $(warning ⚠️  BACnet library not found)
endif

# Redis 라이브러리
ifeq ($(HAS_HIREDIS),1)
    REDIS_LIBS = -lhiredis -lhiredis_ssl
    CXXFLAGS += -DHAS_HIREDIS
    $(info ✅ Redis found)
else
    $(warning ⚠️  hiredis not found - Redis functionality will be limited)
    REDIS_LIBS = 
endif

# JSON 라이브러리
ifeq ($(HAS_NLOHMANN_JSON),1)
    CXXFLAGS += -DHAS_NLOHMANN_JSON
    $(info ✅ nlohmann/json found)
endif

# 🔥 HTTP 라이브러리 설정 (API/Network 모듈용)
ifeq ($(HAS_HTTPLIB),1)
    HTTP_LIBS = 
    CXXFLAGS += -DHAVE_HTTPLIB=1
    $(info ✅ httplib.h found - API/Network modules enabled)
else
    HTTP_LIBS = 
    CXXFLAGS += -DHAVE_HTTPLIB=0
    $(warning ⚠️  httplib.h not found - API/Network modules disabled)
endif

# 🔥 QuickJS 라이브러리 설정 (스크립트 알람용)
ifeq ($(HAS_QUICKJS),1)
    QUICKJS_LIBS = $(shell pkg-config --libs quickjs 2>/dev/null || echo "-lquickjs")
    QUICKJS_INCLUDES = $(shell pkg-config --cflags quickjs 2>/dev/null || echo "-I/usr/local/include")
    INCLUDES += $(QUICKJS_INCLUDES)
    CXXFLAGS += -DHAS_QUICKJS=1
    $(info ✅ QuickJS found - Script alarms enabled)
else
    QUICKJS_LIBS = 
    CXXFLAGS += -DHAS_QUICKJS=0
    $(warning ⚠️  QuickJS not found - Script alarms disabled)
endif

# 전체 라이브러리 설정
OPTIONAL_LIBS = $(MODBUS_LIBS) $(MYSQL_LIBS) $(PGSQL_LIBS) $(MQTT_LIBS) \
                $(BACNET_LIBS) $(REDIS_LIBS) $(HTTP_LIBS) $(QUICKJS_LIBS)
LIBS = $(BASIC_LIBS) $(OPTIONAL_LIBS)

# 색상 정의
GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
PURPLE = \033[0;35m
CYAN = \033[0;36m
MAGENTA = \033[0;95m
NC = \033[0m

# =============================================================================
# 소스 파일 정의 (계층적 구조) - 모든 새 모듈 완전 포함
# =============================================================================

# 기본 필수 소스들
CORE_SOURCES := $(wildcard $(SRC_DIR)/Core/*.cpp)
UTILS_SOURCES := $(wildcard $(SRC_DIR)/Utils/*.cpp)
CONFIG_SOURCES := $(wildcard $(SRC_DIR)/Config/*.cpp)

# Database 소스들
DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Entities/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)

# Client 소스들
CLIENT_SOURCES := $(wildcard $(SRC_DIR)/Client/*.cpp)

# Workers 소스들 (순서 중요!)
WORKERS_BASE_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp)
WORKERS_PROTOCOL_SOURCES := $(wildcard $(SRC_DIR)/Workers/Protocol/*.cpp)
WORKERS_COMPONENTS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Components/*.cpp)
WORKERS_SOURCES := $(WORKERS_BASE_SOURCES) $(WORKERS_PROTOCOL_SOURCES) $(WORKERS_COMPONENTS_SOURCES) \
                  $(wildcard $(SRC_DIR)/Workers/WorkerFactory.cpp)

# Drivers 소스들 (조건부 포함)
DRIVERS_COMMON_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Common/*.cpp)
DRIVERS_SOURCES = $(DRIVERS_COMMON_SOURCES)

# Modbus 드라이버
ifneq ($(MODBUS_LIBS),)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Modbus/*.cpp)
endif

# MQTT 드라이버 (수정됨 - 더 유연한 조건)
ifneq ($(MQTT_LIBS),)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Mqtt/*.cpp)
    $(info ✅ Including MQTT driver sources)
else
    $(warning ⚠️  MQTT driver sources excluded - no MQTT libraries)
endif

# BACnet 드라이버
ifeq ($(HAS_BACNET_STACK),1)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Bacnet/*.cpp)
endif

# Pipeline 소스들
PIPELINE_SOURCES := $(wildcard $(SRC_DIR)/Pipeline/*.cpp)

# Plugin 소스들
PLUGIN_SOURCES := $(wildcard $(SRC_DIR)/Plugin/*.cpp)

# 🔥 알람 소스들 (QuickJS 스크립트 알람 포함)
ALARM_SOURCES := $(wildcard $(SRC_DIR)/Alarm/*.cpp)

# 🔥 가상포인트 소스들
VIRTUALPOINT_SOURCES := $(wildcard $(SRC_DIR)/VirtualPoint/*.cpp)

# 🔥 Network 소스들 (조건부 포함 - httplib 필요)
ifeq ($(HAS_HTTPLIB),1)
    NETWORK_SOURCES := $(wildcard $(SRC_DIR)/Network/*.cpp)
    $(info ✅ Including Network sources: $(words $(NETWORK_SOURCES)) files)
else
    NETWORK_SOURCES :=
    $(warning ⚠️  Network sources excluded - httplib not available)
endif

# 🔥 API 소스들 (조건부 포함 - httplib 필요)
ifeq ($(HAS_HTTPLIB),1)
    API_SOURCES := $(wildcard $(SRC_DIR)/Api/*.cpp)
    $(info ✅ Including API sources: $(words $(API_SOURCES)) files)
else
    API_SOURCES :=
    $(warning ⚠️  API sources excluded - httplib not available)
endif

# 🔥 Platform 소스들 (헤더 온리지만 향후 확장 대비)
PLATFORM_SOURCES := $(wildcard $(SRC_DIR)/Platform/*.cpp)

# 메인 소스
MAIN_SOURCE = $(SRC_DIR)/main.cpp

# 🔥 전체 소스 조합 (모든 새 모듈 완전 포함)
ALL_LIB_SOURCES = $(CORE_SOURCES) $(UTILS_SOURCES) $(CONFIG_SOURCES) \
                  $(DATABASE_SOURCES) $(CLIENT_SOURCES) $(WORKERS_SOURCES) \
                  $(DRIVERS_SOURCES) $(PIPELINE_SOURCES) $(PLUGIN_SOURCES) \
                  $(ALARM_SOURCES) $(VIRTUALPOINT_SOURCES) $(NETWORK_SOURCES) \
                  $(API_SOURCES) $(PLATFORM_SOURCES)

# 오브젝트 파일들
ALL_LIB_OBJECTS = $(ALL_LIB_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
MAIN_OBJECT = $(MAIN_SOURCE:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
ALL_OBJECTS = $(ALL_LIB_OBJECTS) $(MAIN_OBJECT)

# =============================================================================
# 메인 타겟들
# =============================================================================

.DEFAULT_GOAL := help

# 🔥 디렉토리 생성 (모든 새 모듈 디렉토리 완전 포함)
$(BUILD_DIR):
	@echo -e "$(BLUE)📁 Creating build directories for $(PLATFORM)...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Core
	@mkdir -p $(BUILD_DIR)/Utils $(BUILD_DIR)/Config $(BUILD_DIR)/Database/Entities $(BUILD_DIR)/Database/Repositories
	@mkdir -p $(BUILD_DIR)/Client $(BUILD_DIR)/Workers/Base $(BUILD_DIR)/Workers/Protocol $(BUILD_DIR)/Workers/Components
	@mkdir -p $(BUILD_DIR)/Drivers/Common $(BUILD_DIR)/Drivers/Modbus $(BUILD_DIR)/Drivers/Mqtt $(BUILD_DIR)/Drivers/Bacnet
	@mkdir -p $(BUILD_DIR)/Pipeline $(BUILD_DIR)/Alarm $(BUILD_DIR)/VirtualPoint $(BUILD_DIR)/Plugin 
	@mkdir -p $(BUILD_DIR)/Network $(BUILD_DIR)/Api $(BUILD_DIR)/Platform
	@echo -e "$(GREEN)✅ Build directories created for $(PLATFORM) (all modules)$(NC)"

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

directories: $(BUILD_DIR) $(BIN_DIR)

# 🔧 소스 파일 검증 (모든 모듈 완전 포함)
check-sources:
	@echo -e "$(BLUE)🔍 Checking required source files for $(PLATFORM)...$(NC)"
	@echo -e "$(YELLOW)Main source:$(NC)"
	@if [ -f "$(MAIN_SOURCE)" ]; then \
		echo -e "  ✅ $(MAIN_SOURCE)"; \
	else \
		echo -e "  ❌ $(MAIN_SOURCE) (MISSING)"; \
		exit 1; \
	fi
	@echo -e "$(YELLOW)Source counts by module:$(NC)"
	@echo "  Core: $(words $(CORE_SOURCES))"
	@echo "  Utils: $(words $(UTILS_SOURCES))"
	@echo "  Config: $(words $(CONFIG_SOURCES))"
	@echo "  Database: $(words $(DATABASE_SOURCES))"
	@echo "  Workers: $(words $(WORKERS_SOURCES))"
	@echo "  Drivers: $(words $(DRIVERS_SOURCES))"
	@echo "  Client: $(words $(CLIENT_SOURCES))"
	@echo "  Pipeline: $(words $(PIPELINE_SOURCES))"
	@echo "  Plugin: $(words $(PLUGIN_SOURCES))"
	@echo "  🔥 Alarm: $(words $(ALARM_SOURCES))"
	@echo "  🔥 VirtualPoint: $(words $(VIRTUALPOINT_SOURCES))"
	@echo "  🔥 Network: $(words $(NETWORK_SOURCES))"
	@echo "  🔥 Api: $(words $(API_SOURCES))"
	@echo "  🔥 Platform: $(words $(PLATFORM_SOURCES))"
	@echo -e "$(GREEN)Total sources: $(words $(ALL_LIB_SOURCES)) files$(NC)"

# 🔥 의존성 확인 (모든 새 라이브러리 완전 포함)
check-deps:
	@echo -e "$(BLUE)🔍 라이브러리 의존성 체크 for $(PLATFORM)$(NC)"
	@echo -e "$(YELLOW)========================================$(NC)"
	@echo "Platform: $(PLATFORM) (Windows: $(IS_WINDOWS), Linux: $(IS_LINUX))"
	@echo ""
	@echo -e "$(BLUE)📦 기본 라이브러리들:$(NC)"
	@echo "  Modbus: $(if $(MODBUS_LIBS),✅ $(MODBUS_LIBS),❌ not found)"
	@echo "  MySQL: $(if $(MYSQL_LIBS),✅ found,❌ not found)"
	@echo "  PostgreSQL: $(if $(PGSQL_LIBS),✅ $(PGSQL_LIBS),❌ not found)"
	@echo "  MQTT: $(if $(MQTT_LIBS),✅ $(MQTT_LIBS),❌ not found)"
	@echo "  BACnet: $(if $(BACNET_LIBS),✅ $(BACNET_LIBS),❌ not found)"
	@echo "  Redis: $(if $(REDIS_LIBS),✅ $(REDIS_LIBS),❌ not found)"
	@echo ""
	@echo -e "$(BLUE)🔥 새 모듈 라이브러리들:$(NC)"
	@echo "  HTTP (API/Network): $(if $(filter -DHAVE_HTTPLIB=1,$(CXXFLAGS)),✅ available,❌ unavailable)"
	@echo "  QuickJS (Script Alarms): $(if $(QUICKJS_LIBS),✅ available $(QUICKJS_LIBS),❌ unavailable)"
	@echo "  Platform libs: $(if $(PLATFORM_LIBS),✅ $(PLATFORM_LIBS),ℹ️  no platform-specific libs)"
	@echo ""
	@echo -e "$(BLUE)🔧 MQTT 상세 상태:$(NC)"
	@echo "  $(MQTT_STATUS)"
	@echo ""
	@echo -e "$(BLUE)🔧 모듈별 소스 집계:$(NC)"
	@echo "  전체 소스 파일: $(words $(ALL_LIB_SOURCES))"
	@echo "  Alarm 모듈: $(words $(ALARM_SOURCES)) files"
	@echo "  VirtualPoint 모듈: $(words $(VIRTUALPOINT_SOURCES)) files"
	@echo "  Network 모듈: $(words $(NETWORK_SOURCES)) files"
	@echo "  Api 모듈: $(words $(API_SOURCES)) files"
	@echo "  Platform 모듈: $(words $(PLATFORM_SOURCES)) files"
	@echo ""
	@echo -e "$(BLUE)🔧 최종 링커 라이브러리:$(NC)"
	@echo "  LIBS: $(LIBS)"

# 전체 빌드
all: check-deps check-sources directories $(TARGET)
	@echo -e "$(GREEN)🎉 Build completed successfully for $(PLATFORM) (all modules)!$(NC)"
	@echo -e "$(BLUE)📦 Executable: $(BIN_DIR)/$(TARGET)$(NC)"
	@echo -e "$(BLUE)📊 Total object files: $(words $(ALL_OBJECTS))$(NC)"
	@echo -e "$(BLUE)🔥 Included modules: Core, Utils, Config, Database, Workers, Drivers, Pipeline, Plugin, Alarm, VirtualPoint, Network, Api, Platform$(NC)"

# 메인 실행 파일
$(TARGET): $(ALL_OBJECTS) | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking $(TARGET) for $(PLATFORM) (all modules)...$(NC)"
	@echo -e "$(YELLOW)   Objects: $(words $(ALL_OBJECTS))$(NC)"
	@echo -e "$(YELLOW)   Libraries: $(LIBS)$(NC)"
	@mkdir -p $(BIN_DIR)
	$(CXX) $(LDFLAGS) -o $(BIN_DIR)/$@ $(ALL_OBJECTS) $(LIBS)
	@echo -e "$(GREEN)✅ $(TARGET) built successfully for $(PLATFORM) with all modules!$(NC)"

# =============================================================================
# 오브젝트 파일 컴파일 규칙 (모든 새 모듈 완전 포함)
# =============================================================================

# Core 파일들
$(BUILD_DIR)/Core/%.o: $(SRC_DIR)/Core/%.cpp | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Core
	@echo -e "$(GREEN)🏛️  Compiling Core: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Utils 파일들
$(BUILD_DIR)/Utils/%.o: $(SRC_DIR)/Utils/%.cpp | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Utils
	@echo -e "$(GREEN)🔧 Compiling Utils: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Config 파일들
$(BUILD_DIR)/Config/%.o: $(SRC_DIR)/Config/%.cpp | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Config
	@echo -e "$(BLUE)⚙️  Compiling Config: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Workers 파일들
$(BUILD_DIR)/Workers/%.o: $(SRC_DIR)/Workers/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo -e "$(BLUE)👷 Compiling Worker: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Drivers 파일들
$(BUILD_DIR)/Drivers/%.o: $(SRC_DIR)/Drivers/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)🔌 Compiling Driver: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Database 파일들
$(BUILD_DIR)/Database/%.o: $(SRC_DIR)/Database/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo -e "$(GREEN)🗄️  Compiling Database: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Client 파일들
$(BUILD_DIR)/Client/%.o: $(SRC_DIR)/Client/%.cpp | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Client
	@echo -e "$(BLUE)📡 Compiling Client: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Pipeline 파일들
$(BUILD_DIR)/Pipeline/%.o: $(SRC_DIR)/Pipeline/%.cpp | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Pipeline
	@echo -e "$(PURPLE)⚡ Compiling Pipeline: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Plugin 파일들
$(BUILD_DIR)/Plugin/%.o: $(SRC_DIR)/Plugin/%.cpp | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Plugin
	@echo -e "$(YELLOW)🔌 Compiling Plugin: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 🔥 Alarm 파일들 컴파일 (QuickJS 스크립트 알람 지원)
$(BUILD_DIR)/Alarm/%.o: $(SRC_DIR)/Alarm/%.cpp | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Alarm
	@echo -e "$(RED)🚨 Compiling Alarm: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 🔥 VirtualPoint 파일들 컴파일
$(BUILD_DIR)/VirtualPoint/%.o: $(SRC_DIR)/VirtualPoint/%.cpp | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/VirtualPoint
	@echo -e "$(CYAN)🎯 Compiling VirtualPoint: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 🔥 Network 파일들 컴파일 (REST API 서버)
$(BUILD_DIR)/Network/%.o: $(SRC_DIR)/Network/%.cpp | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Network
	@echo -e "$(MAGENTA)🌐 Compiling Network: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 🔥 Api 파일들 컴파일
$(BUILD_DIR)/Api/%.o: $(SRC_DIR)/Api/%.cpp | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Api
	@echo -e "$(GREEN)🔗 Compiling Api: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 🔥 Platform 파일들 컴파일 (크로스 플랫폼 호환성)
$(BUILD_DIR)/Platform/%.o: $(SRC_DIR)/Platform/%.cpp | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Platform
	@echo -e "$(CYAN)🛡️  Compiling Platform: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 메인 파일
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp | $(BUILD_DIR)
	@echo -e "$(BLUE)🚀 Compiling Main: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 유틸리티 타겟들
# =============================================================================

# 정리
clean:
	@echo -e "$(YELLOW)🧹 Cleaning build files for $(PLATFORM)...$(NC)"
	-rm -rf $(BUILD_DIR) $(BIN_DIR) 2>/dev/null || true
	@echo -e "$(GREEN)✅ Clean completed for $(PLATFORM)$(NC)"

# 실행
run: $(TARGET)
	@echo -e "$(BLUE)🚀 Running $(TARGET) on $(PLATFORM)...$(NC)"
	cd $(BIN_DIR) && ./$(TARGET)

# 빌드 모드별 타겟들
debug: CXXFLAGS += -g -DPULSEONE_DEBUG_MODE -O0
debug: CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
debug: check-deps clean all
	@echo -e "$(GREEN)🐛 Debug build completed for $(PLATFORM) - verbose logging enabled$(NC)"

release: CXXFLAGS += -O3 -DNDEBUG  
release: CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
release: clean all
	@echo -e "$(GREEN)🚀 Release build completed for $(PLATFORM) - optimized for performance$(NC)"

# 테스트 빌드 (라이브러리 없이)
test-build:
	@echo -e "$(BLUE)🧪 Testing build without optional libraries on $(PLATFORM)...$(NC)"
	$(MAKE) MODBUS_LIBS= MQTT_LIBS= BACNET_LIBS= REDIS_LIBS= HTTP_LIBS= QUICKJS_LIBS= all

# 🔧 MQTT 전용 체크 타겟
check-mqtt:
	@echo -e "$(BLUE)🔍 MQTT 라이브러리 상세 체크$(NC)"
	@echo -e "$(YELLOW)================================$(NC)"
	@echo ""
	@echo -e "$(BLUE)설치된 MQTT 라이브러리:$(NC)"
	@ls -la /usr/local/lib/libpaho-mqtt* 2>/dev/null || echo "  ❌ No MQTT libraries in /usr/local/lib"
	@echo ""
	@echo -e "$(BLUE)시스템 경로의 MQTT 라이브러리:$(NC)"
	@ldconfig -p | grep paho || echo "  ❌ No paho libraries in system path"
	@echo ""
	@echo -e "$(BLUE)링크 테스트:$(NC)"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3c >/dev/null 2>&1 && echo "  ✅ mqtt3c: OK" || echo "  ❌ mqtt3c: Failed"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3as >/dev/null 2>&1 && echo "  ✅ mqtt3as: OK" || echo "  ❌ mqtt3as: Failed"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqttpp3 >/dev/null 2>&1 && echo "  ✅ mqttpp3: OK" || echo "  ❌ mqttpp3: Failed"
	@echo ""
	@echo -e "$(BLUE)조합 테스트:$(NC)"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - $(MQTT_LIBS) >/dev/null 2>&1 && echo "  ✅ All MQTT libs combined: OK" || echo "  ❌ Combined test: Failed"

# 🔥 HTTP/API 전용 체크
check-http:
	@echo -e "$(BLUE)🔍 HTTP/API 라이브러리 상세 체크$(NC)"
	@echo -e "$(YELLOW)====================================$(NC)"
	@echo ""
	@echo -e "$(BLUE)헤더 체크:$(NC)"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - >/dev/null 2>&1 && echo "  ✅ 기본 컴파일: OK" || echo "  ❌ 기본 컴파일: Failed"
	@echo '\#include <httplib.h>' | $(CXX) -E -x c++ - $(INCLUDES) >/dev/null 2>&1 && echo "  ✅ httplib.h: Found" || echo "  ❌ httplib.h: Not found"
	@echo ""
	@echo -e "$(BLUE)Api/Network 모듈 활성화 상태:$(NC)"
	@echo "  API modules: $(if $(filter -DHAVE_HTTPLIB=1,$(CXXFLAGS)),✅ enabled,❌ disabled)"
	@echo "  Network sources: $(words $(NETWORK_SOURCES)) files"
	@echo "  Api sources: $(words $(API_SOURCES)) files"

# 🔥 QuickJS/Alarm 전용 체크
check-quickjs:
	@echo -e "$(BLUE)🔍 QuickJS/Alarm 라이브러리 상세 체크$(NC)"
	@echo -e "$(YELLOW)=====================================$(NC)"
	@echo ""
	@echo -e "$(BLUE)QuickJS 체크:$(NC)"
	@pkg-config --exists quickjs && echo "  ✅ quickjs pkg-config: OK" || echo "  ❌ quickjs pkg-config: Failed"
	@pkg-config --libs quickjs 2>/dev/null && echo "  ✅ quickjs libs found" || echo "  ❌ quickjs libs not found"
	@echo ""
	@echo -e "$(BLUE)Alarm 모듈 활성화 상태:$(NC)"
	@echo "  Script alarms: $(if $(filter -DHAS_QUICKJS=1,$(CXXFLAGS)),✅ enabled,❌ disabled)"
	@echo "  Alarm sources: $(words $(ALARM_SOURCES)) files"
	@echo "  QuickJS libs: $(if $(QUICKJS_LIBS),$(QUICKJS_LIBS),none)"

# 🔥 빌드 상태 확인 (모든 모듈 포함)
status:
	@echo -e "$(BLUE)🏗️ Complete Build Status for $(PLATFORM)$(NC)"
	@echo -e "$(YELLOW)======================================$(NC)"
	@echo "Platform: $(PLATFORM) (Windows: $(IS_WINDOWS), Linux: $(IS_LINUX))"
	@echo "Object files: $(words $(wildcard $(BUILD_DIR)/**/*.o)) / $(words $(ALL_OBJECTS))"
	@echo ""
	@echo -e "$(BLUE)📊 소스 파일 통계:$(NC)"
	@echo "  전체 소스: $(words $(ALL_LIB_SOURCES)) files"
	@echo "  Core/기본: $(words $(CORE_SOURCES))$(if $(UTILS_SOURCES), + $(words $(UTILS_SOURCES))) + $(words $(CONFIG_SOURCES)) + $(words $(DATABASE_SOURCES)) = $(shell expr $(words $(CORE_SOURCES)) + $(words $(UTILS_SOURCES)) + $(words $(CONFIG_SOURCES)) + $(words $(DATABASE_SOURCES))) files"
	@echo "  Workers/Drivers: $(words $(WORKERS_SOURCES)) + $(words $(DRIVERS_SOURCES)) = $(shell expr $(words $(WORKERS_SOURCES)) + $(words $(DRIVERS_SOURCES))) files"
	@echo "  🔥 새 모듈들: $(words $(ALARM_SOURCES)) + $(words $(VIRTUALPOINT_SOURCES)) + $(words $(NETWORK_SOURCES)) + $(words $(API_SOURCES)) + $(words $(PLATFORM_SOURCES)) = $(shell expr $(words $(ALARM_SOURCES)) + $(words $(VIRTUALPOINT_SOURCES)) + $(words $(NETWORK_SOURCES)) + $(words $(API_SOURCES)) + $(words $(PLATFORM_SOURCES))) files"
	@echo "  기타: $(words $(CLIENT_SOURCES)) + $(words $(PIPELINE_SOURCES)) + $(words $(PLUGIN_SOURCES)) = $(shell expr $(words $(CLIENT_SOURCES)) + $(words $(PIPELINE_SOURCES)) + $(words $(PLUGIN_SOURCES))) files"
	@echo ""
	@echo -e "$(BLUE)📚 라이브러리 상태:$(NC)"
	@echo "  기본: Modbus$(if $(MODBUS_LIBS),✅,❌), MQTT$(if $(MQTT_LIBS),✅,❌), BACnet$(if $(BACNET_LIBS),✅,❌), Redis$(if $(REDIS_LIBS),✅,❌)"
	@echo "  🔥 새 라이브러리: HTTP$(if $(filter -DHAVE_HTTPLIB=1,$(CXXFLAGS)),✅,❌), QuickJS$(if $(QUICKJS_LIBS),✅,❌), Platform$(if $(PLATFORM_LIBS),✅,ℹ️ )"
	@echo ""
	@echo -e "$(BLUE)📁 빌드 파일:$(NC)"
	@echo "  Binary: $(if $(wildcard $(BIN_DIR)/$(TARGET)),✅ $(BIN_DIR)/$(TARGET),❌ missing)"
	@echo "  Build dir: $(if $(wildcard $(BUILD_DIR)),✅ exists,❌ missing)"

# 진단
diagnose:
	@echo -e "$(BLUE)🏥 종합 시스템 진단 for $(PLATFORM)$(NC)"
	@echo -e "$(YELLOW)===============$(NC)"
	@echo ""
	@echo -e "$(BLUE)플랫폼 정보:$(NC)"
	@echo "  Platform: $(PLATFORM)"
	@echo "  Windows build: $(IS_WINDOWS)"
	@echo "  Linux build: $(IS_LINUX)"
	@echo "  Platform libs: $(if $(PLATFORM_LIBS),$(PLATFORM_LIBS),none)"
	@echo ""
	@echo -e "$(BLUE)컴파일러:$(NC)"
	@$(CXX) --version | head -1
	@echo ""
	@echo -e "$(BLUE)직접 링킹 테스트:$(NC)"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3c >/dev/null 2>&1 && echo "  ✅ MQTT C 링크 성공" || echo "  ❌ MQTT C 링크 실패"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3as >/dev/null 2>&1 && echo "  ✅ MQTT AS 링크 성공" || echo "  ❌ MQTT AS 링크 실패"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqttpp3 >/dev/null 2>&1 && echo "  ✅ MQTT C++ 링크 성공" || echo "  ❌ MQTT C++ 링크 실패"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - -lmodbus >/dev/null 2>&1 && echo "  ✅ Modbus 링크 성공" || echo "  ❌ Modbus 링크 실패"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - -lhiredis >/dev/null 2>&1 && echo "  ✅ Redis 링크 성공" || echo "  ❌ Redis 링크 실패"
	@echo ""
	@echo -e "$(BLUE)🔥 새 모듈 진단:$(NC)"
	@echo '\#include <httplib.h>' | $(CXX) -E -x c++ - $(INCLUDES) >/dev/null 2>&1 && echo "  ✅ HTTP 라이브러리 헤더 OK" || echo "  ❌ HTTP 라이브러리 헤더 실패"
	@pkg-config --exists quickjs && echo "  ✅ QuickJS pkg-config OK" || echo "  ❌ QuickJS pkg-config 실패"

# 🔥 크로스 플랫폼 준비 확인
check-cross-platform:
	@echo -e "$(MAGENTA)🛡️  Cross-Platform Compatibility Check$(NC)"
	@echo -e "$(YELLOW)=======================================$(NC)"
	@echo "Platform detection: $(if $(filter Windows,$(PLATFORM)),✅ Windows detected,$(if $(filter Linux,$(PLATFORM)),✅ Linux detected,ℹ️  Other: $(PLATFORM)))"
	@echo "PlatformCompat.h: $(if $(wildcard $(INCLUDE_DIR)/Platform/PlatformCompat.h),✅ exists,❌ missing)"
	@echo "Platform includes: $(if $(findstring -I$(INCLUDE_DIR)/Platform,$(INCLUDES)),✅ included,❌ missing)"
	@echo "Platform flags: $(if $(IS_WINDOWS),✅ Windows build ready,$(if $(IS_LINUX),✅ Linux build ready,❓ Unknown platform))"
	@echo "Platform libs: $(if $(PLATFORM_LIBS),✅ $(PLATFORM_LIBS),ℹ️  No platform-specific libs)"
	@echo "Platform sources: $(words $(PLATFORM_SOURCES)) files"
	@echo ""
	@echo -e "$(GREEN)Status: $(if $(wildcard $(INCLUDE_DIR)/Platform/PlatformCompat.h),🎯 Ready for cross-platform compilation,⚠️  Consider creating PlatformCompat.h)$(NC)"

# 도움말
help:
	@echo -e "$(BLUE)🔧 PulseOne Collector Complete Build System$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo "Platform: $(PLATFORM)"
	@echo "All modules: Core, Utils, Config, Database, Workers, Drivers, Pipeline, Plugin"
	@echo "🔥 New modules: Alarm, VirtualPoint, Network, Api, Platform"
	@echo ""
	@echo -e "$(GREEN)🚀 주요 명령어:$(NC)"
	@echo "  make all              - 전체 프로젝트 빌드 (모든 모듈 포함)"
	@echo "  make clean            - 빌드 파일 정리"
	@echo "  make run              - 빌드 후 실행"
	@echo ""
	@echo -e "$(GREEN)🔨 빌드 모드:$(NC)"
	@echo "  make debug            - 디버그 모드 빌드 (상세 로깅)"
	@echo "  make release          - 릴리즈 모드 빌드 (최적화)"
	@echo "  make test-build       - 최소 라이브러리로 테스트 빌드"
	@echo ""
	@echo -e "$(GREEN)🔍 진단 도구:$(NC)"
	@echo "  make check-deps       - 라이브러리 의존성 확인"
	@echo "  make check-sources    - 소스 파일 존재 확인"
	@echo "  make check-mqtt       - MQTT 라이브러리 상세 체크"
	@echo "  make check-http       - HTTP/API 라이브러리 상세 체크"
	@echo "  make check-quickjs    - QuickJS/Alarm 라이브러리 상세 체크"
	@echo "  make check-cross-platform - 크로스 플랫폼 호환성 체크"
	@echo "  make diagnose         - 전체 시스템 진단"
	@echo "  make status           - 빌드 상태 확인"
	@echo ""
	@echo -e "$(CYAN)🔥 새 모듈 상태:$(NC)"
	@echo "  API/Network: $(if $(filter -DHAVE_HTTPLIB=1,$(CXXFLAGS)),✅ enabled ($(shell expr $(words $(API_SOURCES)) + $(words $(NETWORK_SOURCES))) files),❌ disabled)"
	@echo "  Alarm/Script: $(if $(filter -DHAS_QUICKJS=1,$(CXXFLAGS)),✅ enabled ($(words $(ALARM_SOURCES)) files),❌ disabled)"
	@echo "  VirtualPoint: $(words $(VIRTUALPOINT_SOURCES)) files"
	@echo "  Platform: $(words $(PLATFORM_SOURCES)) files"
	@echo ""
	@echo -e "$(GREEN)🎯 추천 사용법:$(NC)"
	@echo "  make check-deps && make status    # 전체 상태 확인"
	@echo "  make clean && make all           # 클린 빌드"
	@echo "  make debug                       # 개발 시 디버그 빌드"
	@echo "  make diagnose                    # 문제 발생 시 진단"

# PHONY 타겟들
.PHONY: all clean run debug release check-deps check-sources check-mqtt check-http check-quickjs \
        test-build diagnose directories status help check-cross-platform

# 자동 종속성 생성 (고급 기능)
-include $(ALL_OBJECTS:.o=.d)

$(BUILD_DIR)/%.d: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@echo -e "$(YELLOW)🔗 Generating dependencies for $<$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(BUILD_DIR)/$*.o $< > $@