# =============================================================================
# collector/tests/Makefile - Complete Enhanced Version with Step 7 REST API Backend Test
# 모든 단계별 테스트 로그 파일 자동 생성 지원 + Step 7 완전 통합
# =============================================================================

# 성능 최적화 설정
NPROC := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += -j$(NPROC) --output-sync=target

# C++ Standard
CXXFLAGS += -std=c++17

# 플랫폼 감지
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

# 라이브러리 체크 (캐시된 결과 사용)
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_C := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3c >/dev/null 2>&1 && echo "1" || echo "0")
# --- Protocol & Library Detection ---
UNAME := $(shell uname)

# 1. QuickJS (macOS header path fallback)
QUICKJS_HEADER_FOUND := $(shell [ -f "/usr/local/include/quickjs/quickjs.h" ] || [ -f "/usr/local/include/quickjs.h" ] || [ -f "/opt/homebrew/include/quickjs/quickjs.h" ] && echo "1" || echo "0")
HAS_QUICKJS := $(shell pkg-config --exists quickjs && [ "$(QUICKJS_HEADER_FOUND)" = "1" ] && echo "1" || echo "0")
ifeq ($(HAS_QUICKJS),0)
    # Manual check for macOS homebrew path if pkg-config fails
    HAS_QUICKJS := $(shell [ "$(QUICKJS_HEADER_FOUND)" = "1" ] && echo "1" || echo "0")
endif

# 2. Modbus
HAS_MODBUS := $(shell pkg-config --exists libmodbus && echo "1" || echo "0")
ifeq ($(HAS_MODBUS),0)
    HAS_MODBUS := $(shell [ -f "/usr/include/modbus/modbus.h" ] || [ -f "/usr/local/include/modbus/modbus.h" ] || [ -f "/opt/homebrew/include/modbus/modbus.h" ] && echo "1" || echo "0")
endif

# 3. BACnet (Multi-path check)
HAS_BACNET := $(shell [ -f "/usr/include/bacnet/bacdef.h" ] || [ -f "/usr/local/include/bacnet/bacdef.h" ] || [ -f "/opt/homebrew/include/bacnet/bacdef.h" ] && echo "1" || echo "0")
has_bacnet_lib := $(shell [ -f "/usr/lib/libbacnet.a" ] || [ -f "/usr/local/lib/libbacnet.a" ] || [ -f "/opt/homebrew/lib/libbacnet.a" ] && echo "1" || echo "0")
# We compile the driver regardless (for simulation), but only set HAS_BACNET_STACK if lib is found
HAS_BACNET_STACK := $(has_bacnet_lib)

# 4. MQTT (Paho C++)
HAS_MQTT := $(shell echo '\#include <mqtt/async_client.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")

# 5. HTTP (httplib)
HAS_HTTPLIB := $(shell echo '\#include <httplib.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")

# 6. CURL
HAS_CURL := $(shell pkg-config --exists libcurl && echo "1" || echo "0")
ifeq ($(HAS_CURL),0)
    HAS_CURL := $(shell [ -f "/usr/include/curl/curl.h" ] || [ -f "/usr/local/include/curl/curl.h" ] || [ -f "/opt/homebrew/include/curl/curl.h" ] && echo "1" || echo "0")
endif

# --- Compiler Tags ---
ifeq ($(HAS_MODBUS),1)
    CXXFLAGS += -DHAS_MODBUS=1
endif
ifeq ($(HAS_BACNET),1)
    CXXFLAGS += -DHAS_BACNET=1
    ifeq ($(HAS_BACNET_STACK),1)
        CXXFLAGS += -DHAS_BACNET_STACK=1
    endif
endif
ifeq ($(HAS_MQTT),1)
    CXXFLAGS += -DHAS_MQTT=1
endif
ifeq ($(HAS_QUICKJS),1)
    CXXFLAGS += -DHAS_QUICKJS=1
endif
ifeq ($(HAS_HTTPLIB),1)
    CXXFLAGS += -DHAVE_HTTPLIB=1
endif
ifeq ($(HAS_CURL),1)
    CXXFLAGS += -DHAVE_CURL=1
endif

# --- Library Setup ---
ifeq ($(HAS_MODBUS),1)
    MODBUS_LIBS = $(shell pkg-config --libs libmodbus 2>/dev/null || echo "-lmodbus")
    LIBS += $(MODBUS_LIBS)
endif

ifeq ($(HAS_BACNET_STACK),1)
    BACNET_LIBS = -lbacnet -lm
    LIBS += $(BACNET_LIBS)
    # macOS include mapping
    INCLUDES += -I/usr/local/include/bacnet -I/opt/homebrew/include/bacnet
endif

ifeq ($(HAS_MQTT),1)
    MQTT_LIBS = -lpaho-mqttpp3 -lpaho-mqtt3as
    LIBS += $(MQTT_LIBS)
endif

ifeq ($(HAS_QUICKJS),1)
    QUICKJS_LIBS = -lquickjs
    LIBS += $(QUICKJS_LIBS)
    # Ensure include path for quickjs.h
    INCLUDES += -I/usr/local/include/quickjs -I/opt/homebrew/include/quickjs
endif

# 디렉토리 설정
OBJ_DIR = ./obj
BIN_DIR = ./bin
DATA_DIR = ./data
LOG_DIR = ./logs

# 경로 설정
PARENT_DIR = ..
INCLUDE_DIR = $(PARENT_DIR)/include
SRC_DIR = $(PARENT_DIR)/src
SHARED_DIR = $(PARENT_DIR)/../shared
SHARED_INCLUDE_DIR = $(SHARED_DIR)/include
SHARED_SRC_DIR = $(SHARED_DIR)/src

# Include 경로
INCLUDES = -I$(SHARED_INCLUDE_DIR) \
           -I$(SHARED_DIR)/modules/LogLib/include \
           -I$(SHARED_DIR)/modules/DbLib/include \
           -I$(SHARED_INCLUDE_DIR)/Client \
           -I$(INCLUDE_DIR) \
           -I$(INCLUDE_DIR)/Platform \
           -I$(INCLUDE_DIR)/Api \
           -I$(INCLUDE_DIR)/Alarm \
           -I$(INCLUDE_DIR)/VirtualPoint \
           -I$(INCLUDE_DIR)/Scripting \
           -I$(INCLUDE_DIR)/Network \
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
           -I$(INCLUDE_DIR)/Drivers/OPCUA \
           -I$(INCLUDE_DIR)/Pipeline \
           -I$(INCLUDE_DIR)/Storage \
           -I$(INCLUDE_DIR)/Client \
           -I./googletest/googletest/include \
           -I./googletest/googlemock/include \
           -I/usr/local/include/quickjs

# Modbus 라이브러리 설정
ifneq ($(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lmodbus >/dev/null 2>&1; echo $$?),0)
    MODBUS_LIBS = 
else
    MODBUS_LIBS = -lmodbus
    MODBUS_INCLUDES = $(shell pkg-config --cflags libmodbus 2>/dev/null || echo "")
    INCLUDES += $(MODBUS_INCLUDES)
endif

# MySQL 라이브러리 설정
MYSQL_CONFIG := $(shell which mysql_config 2>/dev/null)
ifneq ($(MYSQL_CONFIG),)
    MYSQL_LIBS = $(shell mysql_config --libs)
    MYSQL_INCLUDES = $(shell mysql_config --include)
    INCLUDES += $(MYSQL_INCLUDES)
else
    MYSQL_LIBS = 
    CXXFLAGS += -DDISABLE_MYSQL_FEATURES
endif

ifeq ($(HAS_MQTT_CPP),1)
    MQTT_CPP_LIBS = -lpaho-mqttpp3
    CXXFLAGS += -DHAVE_MQTT_CPP
    INCLUDES += -I/usr/local/include
else
    MQTT_CPP_LIBS = 
endif

ifeq ($(HAS_MQTT_C),1)
    MQTT_C_LIBS = -lpaho-mqtt3c
    CXXFLAGS += -DHAVE_MQTT
else
    MQTT_C_LIBS = 
endif

# PostgreSQL 라이브러리 설정
ifneq ($(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpqxx >/dev/null 2>&1; echo $$?),0)
    PGSQL_LIBS = 
else
    PGSQL_LIBS = -lpqxx -lpq
endif

# BACnet 라이브러리 설정
ifeq ($(HAS_BACNET_STACK), 1)
    BACNET_LIBS = -lbacnet -lm
    BACNET_INCLUDES = -I/usr/local/include/bacnet
    INCLUDES += $(BACNET_INCLUDES)
    CXXFLAGS += -DHAS_BACNET_STACK=1
else
    CXXFLAGS += -DHAS_BACNET_STACK=0
    BACNET_LIBS = 
endif

# Redis 라이브러리 설정
ifeq ($(HAS_HIREDIS),1)
    REDIS_LIBS = /usr/local/lib/libhiredis.so.1.1.0 -lssl -lcrypto
    CXXFLAGS += -DHAVE_REDIS
else
    REDIS_LIBS = 
endif

# macOS/Darwin specific enablement (Manual override if pkg-config fails)
ifeq ($(UNAME),Darwin)
    # libmodbus usually not installed on macOS dev env
    HAS_LIBMODBUS = 0
    HAS_MQTT_CPP = 0
    HAS_MQTT_C = 0
    LDFLAGS += -L/opt/homebrew/opt/openssl/lib -L/opt/homebrew/lib
endif

# Extra protocol definitions for Application.cpp
ifeq ($(HAS_MODBUS),1)
    CXXFLAGS += -DHAS_MODBUS=1
endif
ifeq ($(HAS_BACNET_STACK),1)
    CXXFLAGS += -DHAS_BACNET=1
endif
ifeq ($(HAS_MQTT_CPP),1)
    CXXFLAGS += -DHAS_MQTT=1
endif
ifeq ($(HAS_QUICKJS),1)
    CXXFLAGS += -DHAS_QUICKJS=1
endif

# JSON 라이브러리 설정
ifeq ($(HAS_NLOHMANN_JSON),1)
    CXXFLAGS += -DHAS_NLOHMANN_JSON
endif

# QuickJS 라이브러리 설정
# macOS에서 /usr/local/include/quickjs 가 없을 경우 대비
QUICKJS_HEADER_FOUND := $(shell [ -f "/usr/local/include/quickjs/quickjs.h" ] || [ -f "/usr/local/include/quickjs.h" ] && echo "1" || echo "0")
ifeq ($(HAS_QUICKJS),1)
    ifeq ($(QUICKJS_HEADER_FOUND),0)
        HAS_QUICKJS := 0
    endif
endif

ifeq ($(HAS_QUICKJS),1)
    # pkg-config might return -L/usr/local/lib which is incorrect for this environment
    QUICKJS_LIBS = -L/usr/local/lib/quickjs -lquickjs
    QUICKJS_INCLUDES = $(shell pkg-config --cflags quickjs 2>/dev/null || echo "-I/usr/local/include/quickjs -I/usr/local/include")
    INCLUDES += $(QUICKJS_INCLUDES)
    CXXFLAGS += -DHAS_QUICKJS=1
else
    QUICKJS_LIBS = 
    CXXFLAGS += -DHAS_QUICKJS=0
    $(warning QuickJS not found - Script alarms will be disabled)
endif

# HTTP 라이브러리 설정
ifeq ($(HAS_HTTPLIB),1)
    HTTP_LIBS = 
    CXXFLAGS += -DHAVE_HTTPLIB=1
    $(info ✅ Including HTTP support - REST API enabled)
else
    HTTP_LIBS = 
    CXXFLAGS += -DHAVE_HTTPLIB=0
    $(warning ⚠️ httplib not available - REST API disabled)
endif

# CURL 라이브러리 설정 (Step 7용)
ifeq ($(HAS_CURL),1)
    CURL_LIBS = -lcurl
    CURL_INCLUDES = $(shell pkg-config --cflags libcurl 2>/dev/null || echo "")
    INCLUDES += $(CURL_INCLUDES)
    CXXFLAGS += -DHAS_CURL=1
    $(info ✅ CURL support enabled - Step 7 REST API tests available)
else
    CURL_LIBS = 
    CXXFLAGS += -DHAS_CURL=0
    $(warning ⚠️ libcurl not available - Step 7 tests will be disabled)
endif

# 전체 라이브러리 설정
BASIC_LIBS = -lpthread -lsqlite3 $(QUICKJS_LIBS) $(HTTP_LIBS) $(CURL_LIBS) -lm -ldl $(PLATFORM_LIBS)
OPTIONAL_LIBS = $(MODBUS_LIBS) $(MYSQL_LIBS) $(PGSQL_LIBS) $(MQTT_C_LIBS) $(MQTT_CPP_LIBS) $(BACNET_LIBS) $(REDIS_LIBS)
LIBS = $(BASIC_LIBS) $(OPTIONAL_LIBS)
# GoogleTest 설정
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    GOOGLETEST_BUILD_DIR = build-mac
else
    GOOGLETEST_BUILD_DIR = build-linux
endif

# Check if GoogleTest is available
HAS_GTEST := $(shell [ -d "./googletest/$(GOOGLETEST_BUILD_DIR)/lib" ] && echo "1" || echo "0")
ifeq ($(HAS_GTEST),1)
    TEST_LIBS = ./googletest/$(GOOGLETEST_BUILD_DIR)/lib/libgtest_main.a ./googletest/$(GOOGLETEST_BUILD_DIR)/lib/libgmock.a ./googletest/$(GOOGLETEST_BUILD_DIR)/lib/libgtest.a -lpthread
else
    # Fallback: Many BACnet tests use main() instead of GTest, so we can sometimes bypass
    TEST_LIBS = -lpthread
endif

# 소스 파일 수집
# Shared Sources
SHARED_UTILS_SOURCES := $(wildcard $(SHARED_SRC_DIR)/Utils/*.cpp)
SHARED_SECURITY_SOURCES := $(wildcard $(SHARED_SRC_DIR)/Security/*.cpp)
SHARED_DATABASE_SOURCES := $(wildcard $(SHARED_SRC_DIR)/Database/*.cpp) \
                          $(wildcard $(SHARED_SRC_DIR)/Database/Entities/*.cpp) \
                          $(wildcard $(SHARED_SRC_DIR)/Database/Repositories/*.cpp)
SHARED_LOGGING_SOURCES := $(wildcard $(SHARED_SRC_DIR)/Logging/*.cpp)
LOGLIB_SOURCES := $(wildcard $(SHARED_DIR)/modules/LogLib/src/*.cpp)
DBLIB_SOURCES := $(wildcard $(SHARED_DIR)/modules/DbLib/src/*.cpp)
SHARED_PLATFORM_SOURCES := $(wildcard $(SHARED_SRC_DIR)/Platform/*.cpp)
SHARED_SECURITY_SOURCES := $(SHARED_SRC_DIR)/Security/SecretManager.cpp

# Collector 전용 소스 파일 수집
CORE_SOURCES := $(wildcard $(SRC_DIR)/Core/*.cpp)
UTILS_SOURCES := $(wildcard $(SRC_DIR)/Utils/*.cpp)
# 중복 제거: shared에 있는 파일들은 collector/src에서 제외하거나 shared 버전을 우선함
UTILS_SOURCES := $(filter-out $(SHARED_UTILS_SOURCES),$(UTILS_SOURCES))

CONFIG_SOURCES := $(wildcard $(SRC_DIR)/Config/*.cpp)

DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Entities/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)
# shared와 겹치지 않는 collector 전용 DB 소스
COLLECTOR_ONLY_DB_SOURCES := $(SRC_DIR)/Database/RepositoryFactory.cpp
# SQLQueries.cpp 등은 shared에 존재하지 않으므로 collector에도 없으면 제거
# ls 결과 collector/src/Database 에는 아무 .cpp 가 없음
COLLECTOR_ONLY_DB_SOURCES := 

CLIENT_SOURCES := $(SHARED_SRC_DIR)/Client/RedisClientImpl.cpp \
                  $(SHARED_SRC_DIR)/Client/HttpClient.cpp \
                  $(SHARED_SRC_DIR)/Client/InfluxClientImpl.cpp
WORKERS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Protocol/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Components/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/*.cpp)
STORAGE_SOURCES := $(wildcard $(SRC_DIR)/Storage/*.cpp)

ifeq ($(HAS_HTTPLIB),1)
    NETWORK_SOURCES := $(wildcard $(SRC_DIR)/Network/*.cpp)
    API_SOURCES := $(wildcard $(SRC_DIR)/Api/*.cpp)
else
    NETWORK_SOURCES :=
    API_SOURCES :=
endif

DRIVERS_SOURCES = $(wildcard $(SRC_DIR)/Drivers/Common/*.cpp)
ifeq ($(HAS_MODBUS),1)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Modbus/*.cpp)
endif
ifeq ($(HAS_MQTT),1)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Mqtt/*.cpp)
endif
ifeq ($(HAS_BACNET),1)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Bacnet/*.cpp)
endif
ifeq ($(HAS_OPCUA),1)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/OPCUA/*.cpp)
endif
DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Ros/*.cpp)
DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Ble/*.cpp)

# Duplicate rule removed, handled in step17-opcua-build section

PIPELINE_SOURCES = $(wildcard $(SRC_DIR)/Pipeline/*.cpp) $(wildcard $(SRC_DIR)/Pipeline/Stages/*.cpp)
ALARM_SOURCES := $(wildcard $(SRC_DIR)/Alarm/*.cpp)
VIRTUALPOINT_SOURCES := $(wildcard $(SRC_DIR)/VirtualPoint/*.cpp)
SCRIPTING_SOURCES := $(wildcard $(SRC_DIR)/Scripting/*.cpp)
PLATFORM_SOURCES := $(wildcard $(SRC_DIR)/Platform/*.cpp)
PLUGIN_SOURCES := $(wildcard $(SRC_DIR)/Plugin/*.cpp)

ALL_SOURCES = $(CORE_SOURCES) $(SHARED_UTILS_SOURCES) $(CONFIG_SOURCES) \
              $(SHARED_DATABASE_SOURCES) $(COLLECTOR_ONLY_DB_SOURCES) \
              $(SHARED_SECURITY_SOURCES) \
              $(CLIENT_SOURCES) $(WORKERS_SOURCES) \
              $(DRIVERS_SOURCES) $(PIPELINE_SOURCES) $(ALARM_SOURCES) \
              $(VIRTUALPOINT_SOURCES) $(SCRIPTING_SOURCES) $(NETWORK_SOURCES) $(API_SOURCES) \
              $(SHARED_PLATFORM_SOURCES) $(PLUGIN_SOURCES) $(STORAGE_SOURCES) \
              $(SHARED_LOGGING_SOURCES) $(LOGLIB_SOURCES) $(DBLIB_SOURCES) 

# 플러그인용 드라이버 소스 (메인 바이너리에서 제외)
PLUGIN_DRIVERS_SOURCES := $(SRC_DIR)/Drivers/Mqtt/MqttDriver.cpp \
                          $(SRC_DIR)/Drivers/Bacnet/BACnetDriver.cpp \
                          $(SRC_DIR)/Drivers/Bacnet/BACnetServiceManager.cpp \
                          $(SRC_DIR)/Drivers/Bacnet/BACnetBIPPort.cpp \
                          $(SRC_DIR)/Drivers/OPCUA/OPCUADriver.cpp 

# OPC UA C Source
OPCUA_C_SOURCE := $(SRC_DIR)/Drivers/OPCUA/open62541.c

# 메인 코어 소스 (플러그인 드라이버 제외)
CORE_SOURCES_ONLY := $(filter-out $(PLUGIN_DRIVERS_SOURCES),$(ALL_SOURCES))

# 헤더만 존재하는 소스들 제외 (컴파일 대상 아님)
HEADER_ONLY_SOURCES := $(SHARED_SRC_DIR)/Platform/PlatformCompat.cpp # 만약 존재한다면
CORE_SOURCES_FINAL := $(filter-out $(HEADER_ONLY_SOURCES),$(CORE_SOURCES_ONLY))

COLLECTOR_OBJECTS = $(patsubst $(PARENT_DIR)/src/%.cpp,$(OBJ_DIR)/%.o,$(filter $(PARENT_DIR)/src/%,$(CORE_SOURCES_FINAL)))
SHARED_OBJECTS = $(patsubst $(SHARED_DIR)/src/%.cpp,$(OBJ_DIR)/shared/%.o,$(filter $(SHARED_DIR)/src/%,$(CORE_SOURCES_FINAL)))
MODULE_OBJECTS = $(patsubst $(SHARED_DIR)/modules/%.cpp,$(OBJ_DIR)/shared/modules/%.o,$(filter $(SHARED_DIR)/modules/%,$(CORE_SOURCES_FINAL)))

CORE_OBJECTS = $(COLLECTOR_OBJECTS) $(SHARED_OBJECTS) $(MODULE_OBJECTS)

# Plugin Driver Objects (for tests that need them linked statically)
PLUGIN_DRIVER_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(PLUGIN_DRIVERS_SOURCES)) \
                        $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(OPCUA_C_SOURCE))

# Plugin 테스트용 객체 (Worker, API, Network, Application 관련 객체 모두 제외 - 순수 로더 테스트용)
PLUGIN_TEST_CORE_OBJECTS = $(filter-out $(OBJ_DIR)/Workers/Protocol/% $(OBJ_DIR)/Workers/WorkerFactory.o $(OBJ_DIR)/Workers/WorkerManager.o $(OBJ_DIR)/Api/% $(OBJ_DIR)/Network/% $(OBJ_DIR)/Core/Application.o,$(CORE_OBJECTS))

# 기존 ALL_OBJECTS는 하위 호환성을 위해 유지하되, 드라이버 포함 버전으로 정의 (필요시)
# 하지만 이제 대부분의 테스트가 CORE_OBJECTS를 사용해야 함
ALL_OBJECTS = $(CORE_OBJECTS)

TEST_OBJECTS = $(OBJ_DIR)/test_step1_config_connection.o \
               $(OBJ_DIR)/test_step2_database_entity.o \
               $(OBJ_DIR)/test_step3_protocol_worker.o \
               $(OBJ_DIR)/test_step4_driver_data_validation.o \
               $(OBJ_DIR)/test_step5_complete_db_integration_validation.o \
               $(OBJ_DIR)/test_step6_rest_api_worker_reload.o \
               $(OBJ_DIR)/test_step6_enhanced_pipeline.o \
               $(OBJ_DIR)/test_step7_rest_api_backend_complete.o \
               $(OBJ_DIR)/test_step8_driver_reconnection_data_flow.o \
               $(OBJ_DIR)/test_step9_scale_and_multi_protocol.o \
               $(OBJ_DIR)/test_step10_e2e_integration_recovery.o \
               $(OBJ_DIR)/test_step11_advanced_scenarios.o \
               $(OBJ_DIR)/test_step12_state_recovery_storage.o \
               $(OBJ_DIR)/test_plugin_loading.o \
               $(OBJ_DIR)/test_network_api.o \
               $(OBJ_DIR)/test_platform_compat.o \
               $(OBJ_DIR)/test_workermanager.o \
               $(OBJ_DIR)/test_vp_concurrency.o

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
# 로그 유틸리티 함수들
# =============================================================================

# 현재 시간 스탬프 생성
TIMESTAMP := $(shell date '+%Y%m%d_%H%M%S')

# 로그 디렉토리 생성
create-log-dir:
	@mkdir -p $(LOG_DIR)

# 로그 파일명 정의 (타임스탬프 포함)
define get_log_filename
$(LOG_DIR)/$(1)_test_$(TIMESTAMP).log
endef

# 로그 요약 생성 함수
define create_log_summary
	@echo "========================================" >> $(1)
	@echo "Test Summary for $(2)" >> $(1)
	@echo "Platform: $(PLATFORM)" >> $(1)
	@echo "Timestamp: $(shell date)" >> $(1)
	@echo "Log file: $(1)" >> $(1)
	@echo "========================================" >> $(1)
endef

# 로그 분석 함수
define analyze_log
	@echo -e "$(CYAN)📊 $(2) Test Analysis:$(NC)"
	@if [ -f "$(1)" ]; then \
		echo "성공한 테스트:"; \
		grep -c "PASSED\|✅\|성공" $(1) || echo "0"; \
		echo "실패한 테스트:"; \
		grep -c "FAILED\|❌\|실패" $(1) || echo "0"; \
		echo "주요 오류:"; \
		grep -E "ERROR\|Exception\|오류" $(1) | head -3 || echo "오류 없음"; \
		echo "전체 검증률:"; \
		grep -o "[0-9]\+\.[0-9]\+%" $(1) | tail -1 || echo "N/A"; \
	else \
		echo "로그 파일을 찾을 수 없습니다: $(1)"; \
	fi
endef

# =============================================================================
# 디렉토리 자동 생성 규칙
# =============================================================================

.DEFAULT_GOAL := help

# 강제 디렉토리 생성
.PHONY: create-dirs
create-dirs: create-log-dir
	@echo -e "$(BLUE)🏗️ Creating directories for $(PLATFORM)...$(NC)"
	@mkdir -p $(OBJ_DIR) $(BIN_DIR) $(DATA_DIR) $(LOG_DIR)
	@mkdir -p $(OBJ_DIR)/Core $(OBJ_DIR)/Utils $(OBJ_DIR)/Config 
	@mkdir -p $(OBJ_DIR)/Database $(OBJ_DIR)/Database/Entities $(OBJ_DIR)/Database/Repositories
	@mkdir -p $(OBJ_DIR)/Client $(OBJ_DIR)/Workers $(OBJ_DIR)/Workers/Base $(OBJ_DIR)/Workers/Protocol $(OBJ_DIR)/Workers/Components
	@mkdir -p $(OBJ_DIR)/Drivers $(OBJ_DIR)/Drivers/Common $(OBJ_DIR)/Drivers/Modbus $(OBJ_DIR)/Drivers/Mqtt $(OBJ_DIR)/Drivers/Bacnet
	@mkdir -p $(OBJ_DIR)/Pipeline $(OBJ_DIR)/Alarm $(OBJ_DIR)/VirtualPoint $(OBJ_DIR)/Scripting $(OBJ_DIR)/Plugin
	@mkdir -p $(OBJ_DIR)/Network $(OBJ_DIR)/Api $(OBJ_DIR)/Platform $(OBJ_DIR)/Storage
	@mkdir -p $(OBJ_DIR)/shared $(OBJ_DIR)/shared/Client
	@echo -e "$(GREEN)✅ All directories created (including logs)$(NC)"

# =============================================================================
# 컴파일 규칙
# =============================================================================

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)🔨 Generating dependencies for $<$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $@ $< > $(patsubst %.o,%.d,$@)
	@echo -e "$(YELLOW)⚙️ Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 공유 소스 컴파일 규칙
$(OBJ_DIR)/shared/%.o: $(SHARED_SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)🔨 Generating dependencies for SHARED $<$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $@ $< > $(patsubst %.o,%.d,$@)
	@echo -e "$(YELLOW)⚙️ Compiling SHARED $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 모듈 소스 컴파일 규칙 (LogLib, DbLib 등)
$(OBJ_DIR)/shared/modules/%.o: $(SHARED_DIR)/modules/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)🔨 Generating dependencies for MODULE $<$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $@ $< > $(patsubst %.o,%.d,$@)
	@echo -e "$(YELLOW)⚙️ Compiling MODULE $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# C 소스 컴파일 규칙 (open62541 등)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️ Compiling C source $< [$(PLATFORM)]$(NC)"
	@gcc -c $< -o $@ -std=c99 -I$(SRC_DIR)/Drivers/OPCUA $(INCLUDES)

# 테스트 파일 컴파일 규칙
$(OBJ_DIR)/test_step1_config_connection.o: test_step1_config_connection.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)📋 Compiling Step 1 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step2_database_entity.o: test_step2_database_entity.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🗄️ Compiling Step 2 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step3_protocol_worker.o: test_step3_protocol_worker.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)👷 Compiling Step 3 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step4_driver_data_validation.o: test_step4_driver_data_validation.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🔌 Compiling Step 4 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step5_complete_db_integration_validation.o: test_step5_complete_db_integration_validation.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🔄 Compiling Step 5 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step6_rest_api_worker_reload.o: test_step6_rest_api_worker_reload.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🌐 Compiling Step 6 RestAPI-WorkerManager Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step6_enhanced_pipeline.o: test_step6_enhanced_pipeline.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)⚡ Compiling Step 6 Enhanced Pipeline Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step7_rest_api_backend_complete.o: test_step7_rest_api_backend_complete.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🎯 Compiling Step 7 Complete REST API Backend Test [$(PLATFORM)]...$(NC)"
ifeq ($(HAS_CURL),1)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
else
	@echo -e "$(RED)❌ Skipping Step 7 - libcurl not available$(NC)"
	@touch $@
endif

$(OBJ_DIR)/test_network_api.o: test_network_api.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🌐 Compiling Network Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_platform_compat.o: test_platform_compat.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🛡️ Compiling Platform Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_workermanager.o: test_workermanager.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)👷 Compiling WorkerManager Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step8_driver_reconnection_data_flow.o: test_step8_driver_reconnection_data_flow.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🔌 Compiling Step 8 Driver Reconnection Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step9_scale_and_multi_protocol.o: test_step9_scale_and_multi_protocol.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)📈 Compiling Step 9 Scale and Multi-Protocol Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step10_e2e_integration_recovery.o: test_step10_e2e_integration_recovery.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)📈 Compiling Step 10 End-to-End Integration Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step11_advanced_scenarios.o: test_step11_advanced_scenarios.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)📈 Compiling Step 11 Advanced Scenarios Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step12_state_recovery_storage.o: test_step12_state_recovery_storage.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)📈 Compiling Step 12 State Recovery & Storage Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step13_bacnet_read_write.o: test_step13_bacnet_read_write.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)📡 Compiling Step 13 RW Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step13_bacnet_discovery.o: test_step13_bacnet_discovery.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🔍 Compiling Step 13 Discovery Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_plugin_loading.o: test_plugin_loading.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🔌 Compiling Plugin Loading Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_vp_concurrency.o: test_vp_concurrency.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🔥 Compiling VP Concurrency Stress Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_enrichment_stage.o: test_enrichment_stage.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🧪 Compiling Enrichment Stage Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_alarm_stage.o: test_alarm_stage.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🔔 Compiling Alarm Stage Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 테스트 실행 파일 빌드
# =============================================================================

$(BIN_DIR)/test_step1: $(ALL_OBJECTS) $(OBJ_DIR)/test_step1_config_connection.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 1 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step1_config_connection.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 1 Test build completed!$(NC)"

$(BIN_DIR)/test_step2: $(ALL_OBJECTS) $(OBJ_DIR)/test_step2_database_entity.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 2 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step2_database_entity.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 2 Test build completed!$(NC)"

$(BIN_DIR)/test_step3: $(ALL_OBJECTS) $(PLUGIN_DRIVER_OBJECTS) $(OBJ_DIR)/test_step3_protocol_worker.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 3 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(PLUGIN_DRIVER_OBJECTS) $(OBJ_DIR)/test_step3_protocol_worker.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 3 Test build completed!$(NC)"

$(BIN_DIR)/test_step4: $(ALL_OBJECTS) $(PLUGIN_DRIVER_OBJECTS) $(OBJ_DIR)/test_step4_driver_data_validation.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 4 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(PLUGIN_DRIVER_OBJECTS) $(OBJ_DIR)/test_step4_driver_data_validation.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 4 Test build completed!$(NC)"

$(BIN_DIR)/test_step5: $(ALL_OBJECTS) $(PLUGIN_DRIVER_OBJECTS) $(OBJ_DIR)/test_step5_complete_db_integration_validation.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 5 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(PLUGIN_DRIVER_OBJECTS) $(OBJ_DIR)/test_step5_complete_db_integration_validation.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 5 Test build completed!$(NC)"

$(BIN_DIR)/test_step6: $(ALL_OBJECTS) $(PLUGIN_DRIVER_OBJECTS) $(OBJ_DIR)/test_step6_rest_api_worker_reload.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 6 RestAPI-WorkerManager Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(PLUGIN_DRIVER_OBJECTS) $(OBJ_DIR)/test_step6_rest_api_worker_reload.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 6 RestAPI-WorkerManager Test build completed!$(NC)"

# Legacy rule removed to avoid conflict with new test_step6_pipeline definition

$(BIN_DIR)/test_step7: $(filter-out $(OBJ_DIR)/Main.o, $(ALL_OBJECTS)) $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(OBJ_DIR)/test_step7_rest_api_backend_complete.o
	@mkdir -p $(BIN_DIR)
ifeq ($(HAS_CURL),1)
	@echo -e "$(PURPLE)🔗 Linking Step 7 Complete REST API Backend Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(filter-out $(OBJ_DIR)/Main.o, $(ALL_OBJECTS)) $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(OBJ_DIR)/test_step7_rest_api_backend_complete.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 7 Complete REST API Backend Test build completed!$(NC)"
else
	@echo -e "$(YELLOW)⚠️ Step 7 Test skipped - libcurl not available$(NC)"
	@echo '#!/bin/bash' > $@
	@echo 'echo "Step 7 test requires libcurl - install with: apt install libcurl4-openssl-dev"' >> $@
	@echo 'exit 1' >> $@
	@chmod +x $@
endif

$(BIN_DIR)/test_network: $(ALL_OBJECTS) $(OBJ_DIR)/test_network_api.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Network Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_network_api.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Network Test build completed!$(NC)"

$(BIN_DIR)/test_platform: $(ALL_OBJECTS) $(OBJ_DIR)/test_platform_compat.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Platform Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_platform_compat.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Platform Test build completed!$(NC)"

$(BIN_DIR)/test_workermanager: $(ALL_OBJECTS) $(OBJ_DIR)/test_workermanager.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking WorkerManager Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_workermanager.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ WorkerManager Test build completed!$(NC)"

$(BIN_DIR)/test_step8: $(ALL_OBJECTS) $(OBJ_DIR)/test_step8_driver_reconnection_data_flow.o $(PLUGIN_DRIVER_OBJECTS)
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 8 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step8_driver_reconnection_data_flow.o $(PLUGIN_DRIVER_OBJECTS) $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 8 Test build completed!$(NC)"

$(BIN_DIR)/test_step9: $(ALL_OBJECTS) $(OBJ_DIR)/test_step9_scale_and_multi_protocol.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 9 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step9_scale_and_multi_protocol.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 9 Test build completed!$(NC)"

$(BIN_DIR)/test_step10: $(ALL_OBJECTS) $(OBJ_DIR)/test_step10_e2e_integration_recovery.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 10 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step10_e2e_integration_recovery.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 10 Test build completed!$(NC)"

$(BIN_DIR)/test_step11: $(filter-out $(OBJ_DIR)/Main.o, $(ALL_OBJECTS)) $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(OBJ_DIR)/test_step11_advanced_scenarios.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 11 Test [$(PLATFORM)]...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) $(PLATFORM_LIBS) $(TEST_LIBS)
	@echo -e "$(GREEN)✅ Step 11 Test build completed!$(NC)"

$(BIN_DIR)/test_step12: $(filter-out $(OBJ_DIR)/Main.o, $(ALL_OBJECTS)) $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(OBJ_DIR)/test_step12_performance_stress.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 12 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) $(PLATFORM_LIBS) $(TEST_LIBS)
	@echo -e "$(GREEN)✅ Step 12 Test build completed!$(NC)"

$(BIN_DIR)/test_vp_concurrency: $(filter-out $(OBJ_DIR)/Main.o, $(ALL_OBJECTS)) $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(OBJ_DIR)/test_vp_concurrency.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking VP Concurrency Test [$(PLATFORM)]...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) $(PLATFORM_LIBS) $(TEST_LIBS)
	@echo -e "$(GREEN)✅ VP Concurrency Test build completed!$(NC)"

vp-concurrency-test: create-dirs $(BIN_DIR)/test_vp_concurrency
	@echo -e "$(MAGENTA)🚀 Running VP Concurrency Stress Test...$(NC)"
	@./$(BIN_DIR)/test_vp_concurrency

$(BIN_DIR)/test_enrichment_stage: $(filter-out $(OBJ_DIR)/Main.o, $(ALL_OBJECTS)) $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(OBJ_DIR)/test_enrichment_stage.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Enrichment Stage Test [$(PLATFORM)]...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) $(PLATFORM_LIBS) $(TEST_LIBS)
	@echo -e "$(GREEN)✅ Enrichment Stage Test build completed!$(NC)"

enrichment-stage-test: create-dirs $(BIN_DIR)/test_enrichment_stage
	@echo -e "$(MAGENTA)🚀 Running Enrichment Stage Test...$(NC)"
	@./$(BIN_DIR)/test_enrichment_stage

$(BIN_DIR)/test_alarm_stage: $(filter-out $(OBJ_DIR)/Main.o, $(ALL_OBJECTS)) $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(OBJ_DIR)/test_alarm_stage.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Alarm Stage Test [$(PLATFORM)]...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) $(PLATFORM_LIBS) $(TEST_LIBS)
	@echo -e "$(GREEN)✅ Alarm Stage Test build completed!$(NC)"

alarm-stage-test: create-dirs $(BIN_DIR)/test_alarm_stage
	@echo -e "$(MAGENTA)🚀 Running Alarm Stage Test...$(NC)"
	@./$(BIN_DIR)/test_alarm_stage

# Persistence Stage Test
$(OBJ_DIR)/test_persistence_stage.o: test_persistence_stage.cpp
	@mkdir -p $(dir $@)
	@echo -e "⚙️ Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR)/test_persistence_stage: $(filter-out $(OBJ_DIR)/Main.o, $(ALL_OBJECTS)) $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(OBJ_DIR)/test_persistence_stage.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Persistence Stage Test [$(PLATFORM)]...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) $(PLATFORM_LIBS) $(TEST_LIBS)
	@echo -e "$(GREEN)✅ Persistence Stage Test build completed!$(NC)"

persistence-stage-test: create-dirs $(BIN_DIR)/test_persistence_stage
	@echo -e "$(MAGENTA)🚀 Running Persistence Stage Test...$(NC)"
	@./$(BIN_DIR)/test_persistence_stage


# Step 13 Read/Write 테스트
step13-rw-build: create-dirs $(BIN_DIR)/test_step13_rw

$(BIN_DIR)/test_step13_rw: $(OBJ_DIR)/test_step13_bacnet_read_write.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Bacnet/BACnetBIPPort.o $(OBJ_DIR)/Drivers/Bacnet/BACnetServiceManager.o $(OBJ_DIR)/Workers/Protocol/BACnetWorker.o $(filter-out $(OBJ_DIR)/Workers/Protocol/% $(OBJ_DIR)/Workers/WorkerRegistry.o $(OBJ_DIR)/Workers/WorkerScheduler.o $(OBJ_DIR)/Workers/WorkerMonitor.o $(OBJ_DIR)/Workers/WorkerManager.o $(OBJ_DIR)/Workers/WorkerFactory.o $(OBJ_DIR)/Core/Application.o $(OBJ_DIR)/Core/Main.o $(OBJ_DIR)/Api/% $(OBJ_DIR)/Network/%, $(CORE_OBJECTS))
	mkdir -p $(BIN_DIR)
	echo -e "$(PURPLE)🔗 Linking Step 13 RW Test [$(PLATFORM)]...$(NC)"
	$(CXX) $(OBJ_DIR)/test_step13_bacnet_read_write.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Bacnet/BACnetBIPPort.o $(OBJ_DIR)/Drivers/Bacnet/BACnetServiceManager.o $(OBJ_DIR)/Workers/Protocol/BACnetWorker.o $(filter-out $(OBJ_DIR)/Workers/Protocol/% $(OBJ_DIR)/Workers/WorkerRegistry.o $(OBJ_DIR)/Workers/WorkerScheduler.o $(OBJ_DIR)/Workers/WorkerMonitor.o $(OBJ_DIR)/Workers/WorkerManager.o $(OBJ_DIR)/Workers/WorkerFactory.o $(OBJ_DIR)/Core/Application.o $(OBJ_DIR)/Core/Main.o $(OBJ_DIR)/Api/% $(OBJ_DIR)/Network/%, $(CORE_OBJECTS)) $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 13 RW Test build completed!$(NC)"

step13-rw-test: step13-rw-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step13_rw))
	@echo -e "$(MAGENTA)🚀 Running Step 13 Read/Write Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)BACnet Read/Write Flow Verification$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 13 RW)
	@./$(BIN_DIR)/test_step13_rw 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 13 RW)
	@echo -e "$(GREEN)Step 13 RW Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 13 Discovery 테스트
step13-discovery-build: create-dirs $(BIN_DIR)/test_step13_discovery

$(BIN_DIR)/test_step13_discovery: $(ALL_OBJECTS) $(OBJ_DIR)/test_step13_bacnet_discovery.o
ifeq ($(HAS_BACNET_STACK),1)
	# BACnet driver is already in ALL_OBJECTS if enabled
endif
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 13 Discovery Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step13_bacnet_discovery.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 13 Discovery Test build completed!$(NC)"

step13-discovery-test: step13-discovery-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step13_discovery))
	@echo -e "$(MAGENTA)🚀 Running Step 13 Discovery Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)BACnet Device Discovery & Persistence Verification$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 13 Discovery)
	@./$(BIN_DIR)/test_step13_discovery 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 13 Discovery)
	@echo -e "$(GREEN)Step 13 Discovery Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 14 Object Compilation Rules
$(OBJ_DIR)/test_step14_mqtt_jsonpath.o: test_step14_mqtt_jsonpath.cpp
	@mkdir -p $(dir $@)
	@echo -e "⚙️ Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step14_bacnet_schedule.o: test_step14_bacnet_schedule.cpp
	@mkdir -p $(dir $@)
	@echo -e "⚙️ Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Step 14 Advanced Driver Features 테스트
step14-advanced-build: create-dirs $(BIN_DIR)/test_step14_mqtt_jsonpath $(BIN_DIR)/test_step14_bacnet_schedule

$(BIN_DIR)/test_step14_mqtt_jsonpath: $(ALL_OBJECTS) $(OBJ_DIR)/test_step14_mqtt_jsonpath.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 14 MQTT JSONPath Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step14_mqtt_jsonpath.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(TEST_LIBS) $(LIBS) -o $@

$(BIN_DIR)/test_step14_bacnet_schedule: $(ALL_OBJECTS) $(OBJ_DIR)/test_step14_bacnet_schedule.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 14 BACnet Schedule Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step14_bacnet_schedule.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 14 Advanced Features Test build completed!$(NC)"

step14-advanced-test: step14-advanced-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step14_advanced))
	@echo -e "$(MAGENTA)🚀 Running Step 14 Advanced Features Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)MQTT JSONPath Verification$(NC)"
	@./$(BIN_DIR)/test_step14_mqtt_jsonpath 2>&1 | tee -a $(LOG_FILE)
	@echo -e "$(CYAN)BACnet Schedule Verification$(NC)"
	@./$(BIN_DIR)/test_step14_bacnet_schedule 2>&1 | tee -a $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 14 Advanced)
	@echo -e "$(GREEN)Step 14 Advanced Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 15 Schedule Sync 테스트
step15-sync-build: create-dirs $(BIN_DIR)/test_step15_sync

$(BIN_DIR)/test_step15_sync: $(ALL_OBJECTS) $(OBJ_DIR)/test_step15_schedule_sync.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 15 Schedule Sync Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step15_schedule_sync.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 15 Schedule Sync Test build completed!$(NC)"

step15-sync-test: step15-sync-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step15_sync))
	@echo -e "$(MAGENTA)🚀 Running Step 15 Schedule Sync Test [$(PLATFORM)]...$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 15 Sync)
	@./$(BIN_DIR)/test_step15_sync 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 15 Sync)
	@echo -e "$(GREEN)Step 15 Sync Test completed. Log saved: $(LOG_FILE)$(NC)"

$(OBJ_DIR)/test_step15_schedule_sync.o: test_step15_schedule_sync.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(CYAN)⚙️ Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Step 16 BLE Beacon Test
step16-ble-build: create-dirs $(BIN_DIR)/test_step16_ble

$(BIN_DIR)/test_step16_ble: $(ALL_OBJECTS) $(OBJ_DIR)/test_step16_ble_beacon.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 16 BLE Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step16_ble_beacon.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(TEST_LIBS) $(LIBS) -lbluetooth -o $@
	@echo -e "$(GREEN)✅ Step 16 BLE Test build completed!$(NC)"

step16-ble-test: step16-ble-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step16_ble))
	@echo -e "$(MAGENTA)🚀 Running Step 16 BLE Test [$(PLATFORM)]...$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 16 BLE)
	@./$(BIN_DIR)/test_step16_ble 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 16 BLE)
	@echo -e "$(GREEN)Step 16 BLE Test completed. Log saved: $(LOG_FILE)$(NC)"

$(OBJ_DIR)/test_step16_ble_beacon.o: test_step16_ble_beacon.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(CYAN)⚙️ Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Step 17 OPC UA Test
step17-opcua-build: create-dirs $(BIN_DIR)/test_step17_opcua

# Rule for open62541 C library (Amalgamation)
$(OBJ_DIR)/Drivers/OPCUA/open62541.o: $(SRC_DIR)/Drivers/OPCUA/open62541.c
	@mkdir -p $(dir $@)
	@echo -e "$(CYAN)⚙️ Compiling open62541 (C) [$(PLATFORM)]...$(NC)"
	@gcc -std=c99 -I$(INCLUDE_DIR)/Drivers/OPCUA -c $< -o $@

$(BIN_DIR)/test_step17_opcua: $(ALL_OBJECTS) $(OBJ_DIR)/test_step17_opcua.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 17 OPC UA Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step17_opcua.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(TEST_LIBS) $(LIBS) -lbluetooth -o $@
	@echo -e "$(GREEN)✅ Step 17 OPC UA Test build completed!$(NC)"

step17-opcua-test: step17-opcua-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step17_opcua))
	@echo -e "$(MAGENTA)🚀 Running Step 17 OPC UA Test [$(PLATFORM)]...$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 17 OPC UA)
	@./$(BIN_DIR)/test_step17_opcua 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 17 OPC UA)
	@echo -e "$(GREEN)Step 17 OPC UA Test completed. Log saved: $(LOG_FILE)$(NC)"

$(OBJ_DIR)/test_step17_opcua.o: test_step17_opcua.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(CYAN)⚙️ Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR)/test_plugin_loading: $(OBJ_DIR)/test_plugin_loading.o $(PLUGIN_TEST_CORE_OBJECTS)
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Plugin Loading Test [$(PLATFORM)]...$(NC)"
	@$(CXX) -rdynamic $^ $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Plugin Loading Test build completed!$(NC)"

# Step 16: BLE Beacon Worker
# -----------------------------------------------------------------------------

$(OBJ_DIR)/test_step16_ble_worker.o: test_step16_ble_worker.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(CYAN)⚙️ Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

step16-worker-build: create-dirs $(BIN_DIR)/test_step16_ble_worker

# Define driver objects specifically to ensure order and presence
DRIVER_OBJS = $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o \
              $(OBJ_DIR)/Drivers/Bacnet/BACnetServiceManager.o \
              $(OBJ_DIR)/Drivers/Bacnet/BACnetBIPPort.o \
              $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o \
              $(OBJ_DIR)/Drivers/OPCUA/open62541.o \
              $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o

$(BIN_DIR)/test_step16_ble_worker: $(ALL_OBJECTS) $(OBJ_DIR)/test_step16_ble_worker.o $(DRIVER_OBJS)
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 16.5 BLE Worker Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step16_ble_worker.o $(DRIVER_OBJS) $(TEST_LIBS) $(LIBS) -lbluetooth -lbacnet -o $@
	@echo -e "$(GREEN)✅ Step 16.5 BLE Worker Test build completed!$(NC)"

step16-worker-test: step16-worker-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step16_ble_worker))
	@echo -e "$(MAGENTA)🚀 Running Step 16.5 BLE Worker Test [$(PLATFORM)]...$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 16.5 BLE Worker)
	@./$(BIN_DIR)/test_step16_ble_worker 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 16.5 BLE Worker)
	@echo -e "$(GREEN)Step 16.5 BLE Worker Test completed. Log saved: $(LOG_FILE)$(NC)"

# =============================================================================
# Step 16: BLE Beacon E2E Integration
# =============================================================================

$(OBJ_DIR)/test_step16_ble_e2e.o: test_step16_ble_e2e.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(CYAN)⚙️ Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

step16-e2e-build: create-dirs $(BIN_DIR)/test_step16_ble_e2e
	@echo -e "-e $(BLUE)🔨 Building Step 16 BLE E2E Test...$(NC)"

$(BIN_DIR)/test_step16_ble_e2e: $(ALL_OBJECTS) $(OBJ_DIR)/test_step16_ble_e2e.o $(DRIVER_OBJS)
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking BLE E2E Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step16_ble_e2e.o $(DRIVER_OBJS) $(TEST_LIBS) $(LIBS) -lbluetooth -lbacnet -o $@
	@echo -e "$(GREEN)✅ BLE E2E Test build completed!$(NC)"

step16-e2e-test: step16-e2e-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step16_ble_e2e))
	@echo -e "$(MAGENTA)🚀 Running BLE E2E Test [$(PLATFORM)]...$(NC)"
	$(call create_log_summary,$(LOG_FILE),BLE E2E)
	@./$(BIN_DIR)/test_step16_ble_e2e 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),BLE E2E)
	@echo -e "$(GREEN)BLE E2E Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 17 OPC UA E2E Test
step17-e2e-build: create-dirs $(BIN_DIR)/test_step17_opcua_e2e

$(OBJ_DIR)/test_step17_opcua_e2e.o: test_step17_opcua_e2e.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(CYAN)⚙️ Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR)/test_step17_opcua_e2e: $(ALL_OBJECTS) $(OBJ_DIR)/test_step17_opcua_e2e.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 17.5 E2E Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step17_opcua_e2e.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(TEST_LIBS) $(LIBS) -lbluetooth -o $@
	@echo -e "$(GREEN)✅ Step 17.5 E2E Test build completed!$(NC)"

step17-e2e-test: step17-e2e-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step17_e2e))
	@echo -e "$(MAGENTA)🚀 Running Step 17.5 OPC UA E2E Test [$(PLATFORM)]...$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 17.5 OPC UA E2E)
	@./$(BIN_DIR)/test_step17_opcua_e2e 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 17.5 OPC UA E2E)
	@echo -e "$(GREEN)Step 17.5 OPC UA E2E Test completed. Log saved: $(LOG_FILE)$(NC)"

# Multi-Collector Isolation Test (Step 13/14 Partitioning)
multi-collector-build: create-dirs $(BIN_DIR)/test_multi_collector_isolation

$(OBJ_DIR)/test_multi_collector_isolation.o: test_multi_collector_isolation.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(CYAN)⚙️ Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR)/test_multi_collector_isolation: $(ALL_OBJECTS) $(OBJ_DIR)/test_multi_collector_isolation.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Multi-Collector Isolation Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_multi_collector_isolation.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Multi-Collector Isolation Test build completed!$(NC)"

multi-collector-test: multi-collector-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,multi_collector_isolation))
	@echo -e "$(MAGENTA)🚀 Running Multi-Collector Isolation Test [$(PLATFORM)]...$(NC)"
	$(call create_log_summary,$(LOG_FILE),Multi-Collector Isolation)
	@./$(BIN_DIR)/test_multi_collector_isolation 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Multi-Collector Isolation)
	@echo -e "$(GREEN)Multi-Collector Isolation Test completed. Log saved: $(LOG_FILE)$(NC)"

# =============================================================================
# 플러그인 빌드 규칙 (Shared Library .so 생성)
# =============================================================================

PLUGIN_DIR := $(BIN_DIR)/plugins

# 플러그인 공통 컴파일 플래그
# -shared: 공유 라이브러리 생성
# -fPIC: Position Independent Code (공유 라이브러리 필수)
PLUGIN_CXXFLAGS := $(CXXFLAGS) -DBUILD_PLUGIN -shared -fPIC

# MQTT 플러그인
mqtt-plugin: create-dirs
	@echo -e "$(CYAN)📦 Building MQTT plugin (.so)...$(NC)"
	@mkdir -p $(PLUGIN_DIR)
	@$(CXX) $(PLUGIN_CXXFLAGS) $(INCLUDES) \
		$(SRC_DIR)/Drivers/Mqtt/MqttDriver.cpp \
		$(SHARED_UTILS_SOURCES) \
		$(SHARED_SECURITY_SOURCES) \
		$(LIBS) -o $(PLUGIN_DIR)/MqttDriver.so

# BACnet 플러그인
bacnet-plugin: create-dirs
	@echo -e "$(CYAN)📦 Building BACnet plugin (.so)...$(NC)"
	@mkdir -p $(PLUGIN_DIR)
	@$(CXX) $(PLUGIN_CXXFLAGS) $(INCLUDES) \
		$(SRC_DIR)/Drivers/Bacnet/BACnetDriver.cpp \
		$(SHARED_UTILS_SOURCES) \
		$(SHARED_SECURITY_SOURCES) \
		$(LIBS) -o $(PLUGIN_DIR)/BacnetDriver.so

# OPC-UA 플러그인
opcua-plugin: create-dirs
	@echo -e "$(CYAN)📦 Building OPC-UA plugin (.so)...$(NC)"
	@mkdir -p $(PLUGIN_DIR)
	@$(CXX) $(PLUGIN_CXXFLAGS) $(INCLUDES) \
		$(SRC_DIR)/Drivers/OPCUA/OPCUADriver.cpp \
		$(SHARED_UTILS_SOURCES) \
		$(SHARED_SECURITY_SOURCES) \
		$(LIBS) -o $(PLUGIN_DIR)/OpcuaDriver.so

# Modbus 플러그인
modbus-plugin: create-dirs
	@echo -e "$(CYAN)📦 Building Modbus plugin (.so)...$(NC)"
	@mkdir -p $(PLUGIN_DIR)
	@$(CXX) $(PLUGIN_CXXFLAGS) -DHAVE_MODBUS $(INCLUDES) \
		$(SRC_DIR)/Drivers/Modbus/ModbusDriver.cpp \
		$(SRC_DIR)/Drivers/Ble/BleBeaconDriver.cpp \
		$(SRC_DIR)/Drivers/Modbus/ModbusDiagnostics.cpp \
		$(SRC_DIR)/Drivers/Modbus/ModbusConnectionPool.cpp \
		$(SRC_DIR)/Drivers/Modbus/ModbusFailover.cpp \
		$(SRC_DIR)/Drivers/Modbus/ModbusPerformance.cpp \
		$(SHARED_UTILS_SOURCES) \
		$(LIBS) -lmodbus -o $(PLUGIN_DIR)/ModbusDriver.so

# BLE 플러그인
ble-plugin: create-dirs
	@echo -e "$(CYAN)📦 Building BLE Beacon plugin (.so)...$(NC)"
	@mkdir -p $(PLUGIN_DIR)
	@$(CXX) $(PLUGIN_CXXFLAGS) $(INCLUDES) \
		$(SRC_DIR)/Drivers/Ble/BleBeaconDriver.cpp \
		$(SHARED_UTILS_SOURCES) \
		$(SHARED_SECURITY_SOURCES) \
		$(LIBS) -o $(PLUGIN_DIR)/BleBeaconDriver.so

plugins: mqtt-plugin bacnet-plugin opcua-plugin modbus-plugin
	@echo -e "$(GREEN)✅ All plugins built successfully in $(PLUGIN_DIR)$(NC)"

# =============================================================================
# 향상된 단계별 테스트 실행 (로그 포함)
# =============================================================================

# Plugin Loading 테스트 빌드
plugin-test-build: create-dirs plugins $(BIN_DIR)/test_plugin_loading

# Plugin Loading 테스트 실행
plugin-test: plugin-test-build
	$(eval LOG_FILE := $(call get_log_filename,plugin_loading))
	@echo -e "$(MAGENTA)🚀 Running Plugin Loading Test [$(PLATFORM)]...$(NC)"
	$(call create_log_summary,$(LOG_FILE),Plugin Loading)
	@./$(BIN_DIR)/test_plugin_loading 2>&1 | tee $(LOG_FILE)
	$(call analyze_log,$(LOG_FILE),Plugin Loading)

# Step 1 테스트
step1-build: create-dirs $(BIN_DIR)/test_step1

step1-test: step1-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step1))
	@echo -e "$(MAGENTA)🚀 Running Step 1 Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)Configuration and Connection Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 1)
	@./$(BIN_DIR)/test_step1 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 1)
	@echo -e "$(GREEN)Step 1 Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 2 테스트
step2-build: create-dirs $(BIN_DIR)/test_step2

step2-test: step2-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step2))
	@echo -e "$(MAGENTA)🚀 Running Step 2 Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)Database Entity Validation Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing Database-Entity mapping and CRUD operations$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 2)
	@./$(BIN_DIR)/test_step2 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 2)
	@echo -e "$(GREEN)Step 2 Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 3 테스트
step3-build: create-dirs $(BIN_DIR)/test_step3

step3-test: step3-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step3))
	@echo -e "$(MAGENTA)🚀 Running Step 3 Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)Protocol Worker Property Validation Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing Worker creation and property mapping$(NC)"
	@echo -e "$(BLUE)Entity -> DeviceInfo -> Worker property validation$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 3)
	@./$(BIN_DIR)/test_step3 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 3)
	@echo -e "$(GREEN)Step 3 Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 4 테스트
step4-build: create-dirs $(BIN_DIR)/test_step4

step4-test: step4-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step4))
	@echo -e "$(MAGENTA)🚀 Running Step 4 Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)Driver Data Validation Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing protocol drivers and data flow$(NC)"
	@echo -e "$(BLUE)Driver initialization and data validation$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 4)
	@./$(BIN_DIR)/test_step4 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 4)
	@echo -e "$(GREEN)Step 4 Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 5 테스트
step5-build: create-dirs $(BIN_DIR)/test_step5

step5-test: step5-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step5))
	@echo -e "$(MAGENTA)🚀 Running Step 5 Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)Complete DB Integration Validation Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing complete end-to-end DB integration$(NC)"
	@echo -e "$(BLUE)DB alarm recovery → Redis → Backend validation$(NC)"
	@echo -e "$(BLUE)This test requires Redis server running$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 5)
	@PULSEONE_CONFIG_DIR=./config ./$(BIN_DIR)/test_step5 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 5)
	@echo -e "$(GREEN)Step 5 Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 6 테스트 (새로운 RestAPI-WorkerManager 테스트)
step6-build: create-dirs $(BIN_DIR)/test_step6

step6-test: step6-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step6))
	@echo -e "$(MAGENTA)🚀 Running Step 6 Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)RestAPI → WorkerManager Integration Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing RestAPI → WorkerManager device reload flow$(NC)"
	@echo -e "$(BLUE)DB change → API call → Worker reload → Redis update$(NC)"
	@echo -e "$(BLUE)This test requires Redis server running$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 6)
	@./$(BIN_DIR)/test_step6 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 6)
	@echo -e "$(GREEN)Step 6 Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 6 Enhanced Pipeline 테스트 (기존)
# Step 6 Enhanced Pipeline 테스트 (기존)
step6-pipeline-build: create-dirs $(BIN_DIR)/test_step6_pipeline

$(OBJ_DIR)/test_step6_pipeline.o: test_step6_pipeline.cpp
	@mkdir -p $(dir $@)
	@echo -e "⚙️ Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR)/test_step6_pipeline: $(OBJ_DIR)/test_step6_pipeline.o $(filter-out $(OBJ_DIR)/Main.o, $(ALL_OBJECTS)) $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 6 Pipeline Test [$(PLATFORM)]...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) $(PLATFORM_LIBS) $(TEST_LIBS)
	@echo -e "$(GREEN)✅ Step 6 Pipeline Test build completed!$(NC)"

step6-pipeline-test: step6-pipeline-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step6_pipeline))
	@echo -e "$(MAGENTA)🚀 Running Step 6 Enhanced Pipeline Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)Enhanced Pipeline Test (Alarm + VirtualPoint)$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing alarm and virtual point pipeline$(NC)"
	@echo -e "$(BLUE)This test requires Redis server running$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 6 Pipeline)
	@./$(BIN_DIR)/test_step6_pipeline 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 6 Pipeline)
	@echo -e "$(GREEN)Step 6 Enhanced Pipeline Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 7 테스트 (새로운 완전 통합 테스트)
step7-build: create-dirs $(BIN_DIR)/test_step7

step7-test: step7-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step7))
	@echo -e "$(MAGENTA)🚀 Running Step 7 Complete Integration Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)REST API Backend Complete Integration Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
ifeq ($(HAS_CURL),1)
	@echo -e "$(BLUE)Testing complete API → WorkerManager → Hardware flow$(NC)"
	@echo -e "$(BLUE)DeviceApiCallbacks + HardwareApiCallbacks + WorkerManager$(NC)"
	@echo -e "$(BLUE)This test includes HTTP server on port 8081$(NC)"
	@echo -e "$(BLUE)No external dependencies required - fully self-contained$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 7)
	@./$(BIN_DIR)/test_step7 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 7)
	@echo -e "$(GREEN)Step 7 Complete Integration Test completed. Log saved: $(LOG_FILE)$(NC)"
else
	@echo -e "$(YELLOW)Step 7 requires libcurl - install with: apt install libcurl4-openssl-dev$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo "Step 7 skipped - libcurl not available" > $(LOG_FILE)
	@echo -e "$(YELLOW)Step 7 Complete Integration Test skipped. Log saved: $(LOG_FILE)$(NC)"
endif

# Step 8 테스트 (드라이버 연결 및 데이터 플로우)
step8-build: create-dirs $(BIN_DIR)/test_step8

step8-test: step8-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step8))
	@echo -e "$(MAGENTA)🚀 Running Step 8 Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)Driver Reconnection and Data Pipeline Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing driver connection유실/복구 및 Redis 파이프라인$(NC)"
	@echo -e "$(BLUE)This test requires Redis server running$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 8)
	@./$(BIN_DIR)/test_step8 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 8)
	@echo -e "$(GREEN)Step 8 Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 9 테스트
step9-build: create-dirs $(BIN_DIR)/test_step9

step9-test: step9-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step9))
	@echo -e "$(MAGENTA)🚀 Running Step 9 Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)Scale and Multi-Protocol Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing system with multiple concurrent devices (TCP/RTU)$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 9)
	@./$(BIN_DIR)/test_step9 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 9)
	@echo -e "$(GREEN)Step 9 Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 10 테스트
step10-build: create-dirs $(BIN_DIR)/test_step10

step10-test: step10-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step10))
	@echo -e "$(MAGENTA)🚀 Running Step 10 End-to-End Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)E2E Integration and Recovery Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing: Modbus -> Pipeline -> VP -> Alarm -> Recovery$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 10)
	@./$(BIN_DIR)/test_step10 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 10)
	@echo -e "$(GREEN)Step 10 Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 11 테스트
step11-build: create-dirs $(BIN_DIR)/test_step11

step11-test: step11-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step11))
	@echo -e "$(MAGENTA)🚀 Running Step 11 Advanced Scenarios [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)Advanced Alarm & Virtual Point Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing: Low Limit, Complex Script, VP Write$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 11)
	@./$(BIN_DIR)/test_step11 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 11)
	@echo -e "$(GREEN)Step 11 Test completed. Log saved: $(LOG_FILE)$(NC)"

# Step 12 테스트
step12-build: create-dirs $(BIN_DIR)/test_step12

step12-test: step12-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step12))
	@echo -e "$(MAGENTA)🚀 Running Step 12 State Recovery & Storage [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)Tiered State Recovery & Storage Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing: RDB Storage Differentiation, Warm Startup Recovery$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 12)
	@./$(BIN_DIR)/test_step12 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 12)
	@echo -e "$(GREEN)Step 12 Test completed. Log saved: $(LOG_FILE)$(NC)"

# Network 테스트
network-build: create-dirs $(BIN_DIR)/test_network

network-test: network-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,network))
	@echo -e "$(MAGENTA)🚀 Running Network Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)REST API Server Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing REST API server functionality$(NC)"
	@echo -e "$(BLUE)This test starts REST API server on port 8080$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Network)
	@./$(BIN_DIR)/test_network 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Network)
	@echo -e "$(GREEN)Network Test completed. Log saved: $(LOG_FILE)$(NC)"

# Platform 테스트
platform-build: create-dirs $(BIN_DIR)/test_platform

platform-test: platform-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,platform))
	@echo -e "$(MAGENTA)🚀 Running Platform Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)Cross-Platform Compatibility Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Platform)
	@./$(BIN_DIR)/test_platform 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Platform)
	@echo -e "$(GREEN)Platform Test completed. Log saved: $(LOG_FILE)$(NC)"

# WorkerManager 테스트
workermanager-build: create-dirs $(BIN_DIR)/test_workermanager

workermanager-test: workermanager-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,workermanager))
	@echo -e "$(MAGENTA)🚀 Running WorkerManager Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)WorkerManager Singleton and Lifecycle Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),WorkerManager)
	@./$(BIN_DIR)/test_workermanager 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),WorkerManager)
	@echo -e "$(GREEN)WorkerManager Test completed. Log saved: $(LOG_FILE)$(NC)"

# =============================================================================
# 로그 관리 유틸리티
# =============================================================================

check-logs: create-log-dir
	@echo -e "$(BLUE)📁 Log Directory Status$(NC)"
	@echo -e "$(YELLOW)========================$(NC)"
	@echo "Log directory: $(LOG_DIR)"
	@echo "Total log files: $(shell find $(LOG_DIR) -name '*.log' 2>/dev/null | wc -l)"
	@echo ""
	@echo -e "$(CYAN)Recent log files:$(NC)"
	@ls -lt $(LOG_DIR)/*.log 2>/dev/null | head -10 || echo "No log files found"

logs-summary: create-log-dir
	@echo -e "$(BLUE)📊 Latest Test Logs Summary$(NC)"
	@echo -e "$(YELLOW)=============================$(NC)"
	@for step in step1 step2 step3 step4 step5 step6 step6_pipeline step7 network platform workermanager; do \
		latest_log=$$(ls -t $(LOG_DIR)/$${step}_test_*.log 2>/dev/null | head -1); \
		if [ -n "$$latest_log" ]; then \
			echo -e "$(MAGENTA)$$step Test:$(NC)"; \
			echo "  File: $$latest_log"; \
			echo "  Size: $$(du -h $$latest_log | cut -f1)"; \
			echo "  Passed tests: $$(grep -c 'PASSED\|✅' $$latest_log 2>/dev/null || echo '0')"; \
			echo "  Failed tests: $$(grep -c 'FAILED\|❌' $$latest_log 2>/dev/null || echo '0')"; \
			echo ""; \
		fi; \
	done

clean-old-logs: create-log-dir
	@echo -e "$(YELLOW)🧹 Cleaning old log files (>7 days)...$(NC)"
	@find $(LOG_DIR) -name '*.log' -mtime +7 -delete 2>/dev/null || true
	@echo -e "$(GREEN)Old log files cleaned$(NC)"

clean-all-logs: create-log-dir
	@echo -e "$(RED)🗑️ Removing all log files...$(NC)"
	@rm -f $(LOG_DIR)/*.log 2>/dev/null || true
	@echo -e "$(GREEN)All log files removed$(NC)"

# =============================================================================
# 통합 실행
# =============================================================================

all-tests-build: create-dirs $(BIN_DIR)/test_step1 $(BIN_DIR)/test_step2 $(BIN_DIR)/test_step3 $(BIN_DIR)/test_step4 $(BIN_DIR)/test_step5 $(BIN_DIR)/test_step6 $(BIN_DIR)/test_step6_pipeline $(BIN_DIR)/test_step7 $(BIN_DIR)/test_network $(BIN_DIR)/test_platform $(BIN_DIR)/test_workermanager
	@echo -e "$(GREEN)✅ All tests build completed for $(PLATFORM)!$(NC)"

all-tests-run: all-tests-build create-log-dir
	$(eval MASTER_LOG := $(LOG_DIR)/all_tests_$(TIMESTAMP).log)
	@echo -e "$(BLUE)🚀 Running all tests with logging on $(PLATFORM)...$(NC)"
	@echo "Master log file: $(MASTER_LOG)"
	@echo "Individual test logs will be created in $(LOG_DIR)/"
	@echo ""
	@echo "========================================" > $(MASTER_LOG)
	@echo "All Tests Execution - $(PLATFORM)" >> $(MASTER_LOG)
	@echo "Started: $(shell date)" >> $(MASTER_LOG)
	@echo "Master Log: $(MASTER_LOG)" >> $(MASTER_LOG)
	@echo "========================================" >> $(MASTER_LOG)
	@echo ""
	@echo -e "$(YELLOW)Running tests in sequence...$(NC)"
	@$(MAKE) step1-test step2-test step3-test step4-test step5-test step6-test step7-test network-test platform-test workermanager-test
	@echo ""
	@echo "========================================" >> $(MASTER_LOG)
	@echo "All Tests Completed: $(shell date)" >> $(MASTER_LOG)
	@echo "========================================" >> $(MASTER_LOG)
	@echo -e "$(GREEN)🎉 All tests completed with logging!$(NC)"
	@echo -e "$(CYAN)Master log: $(MASTER_LOG)$(NC)"
	@$(MAKE) logs-summary

quick-test: workermanager-test platform-test step7-test
	@echo -e "$(GREEN)✅ Quick test completed with logging$(NC)"

restapi-test: step6-test step7-test network-test
	@echo -e "$(GREEN)✅ RestAPI integration tests completed$(NC)"

# =============================================================================
# 기존 유틸리티들
# =============================================================================

clean:
	@echo -e "$(YELLOW)🧹 Cleaning $(PLATFORM) build files...$(NC)"
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo -e "$(CYAN)Note: Log files preserved in $(LOG_DIR)/$(NC)"
	@echo -e "$(GREEN)✅ Clean completed for $(PLATFORM)$(NC)"

clean-all: clean clean-all-logs
	@echo -e "$(GREEN)✅ Complete clean (including logs) for $(PLATFORM)$(NC)"

status:
	@echo -e "$(BLUE)🏗️ Build Status for $(PLATFORM) (Enhanced Logging)$(NC)"
	@echo -e "$(YELLOW)================================================$(NC)"
	@echo "Platform: $(PLATFORM)"
	@echo "Windows: $(IS_WINDOWS), Linux: $(IS_LINUX)"
	@echo "Object files: $(words $(wildcard $(OBJ_DIR)/**/*.o)) / $(words $(ALL_OBJECTS))"
	@echo "Source files: $(words $(ALL_SOURCES))"
	@echo "Log directory: $(LOG_DIR)"
	@echo "Total log files: $(shell find $(LOG_DIR) -name '*.log' 2>/dev/null | wc -l)"
	@echo ""
	@echo "Library Support:"
	@echo "HTTP (httplib): $(if $(filter 1,$(HAS_HTTPLIB)),✅ available,❌ missing)"
	@echo "CURL: $(if $(filter 1,$(HAS_CURL)),✅ available,❌ missing)"
	@echo "JSON (nlohmann): $(if $(filter 1,$(HAS_NLOHMANN_JSON)),✅ available,❌ missing)"
	@echo "Redis (hiredis): $(if $(filter 1,$(HAS_HIREDIS)),✅ available,❌ missing)"
	@echo "MQTT C: $(if $(filter 1,$(HAS_MQTT_C)),✅ available,❌ missing)"
	@echo "MQTT C++: $(if $(filter 1,$(HAS_MQTT_CPP)),✅ available,❌ missing)"
	@echo "BACnet: $(if $(filter 1,$(HAS_BACNET_STACK)),✅ available,❌ missing)"
	@echo "QuickJS: $(if $(filter 1,$(HAS_QUICKJS)),✅ available,❌ missing)"
	@echo ""
	@echo "Binary Status:"
	@echo "Step 1: $(if $(wildcard $(BIN_DIR)/test_step1),✅ exists,❌ missing)"
	@echo "Step 2: $(if $(wildcard $(BIN_DIR)/test_step2),✅ exists,❌ missing)"
	@echo "Step 3: $(if $(wildcard $(BIN_DIR)/test_step3),✅ exists,❌ missing)"
	@echo "Step 4: $(if $(wildcard $(BIN_DIR)/test_step4),✅ exists,❌ missing)"
	@echo "Step 5: $(if $(wildcard $(BIN_DIR)/test_step5),✅ exists,❌ missing)"
	@echo "Step 6 RestAPI: $(if $(wildcard $(BIN_DIR)/test_step6),✅ exists,❌ missing)"
	@echo "Step 6 Pipeline: $(if $(wildcard $(BIN_DIR)/test_step6_pipeline),✅ exists,❌ missing)"
	@echo "Step 7 Complete: $(if $(wildcard $(BIN_DIR)/test_step7),✅ exists,❌ missing)"
	@echo "Network: $(if $(wildcard $(BIN_DIR)/test_network),✅ exists,❌ missing)"
	@echo "Platform: $(if $(wildcard $(BIN_DIR)/test_platform),✅ exists,❌ missing)"
	@echo "WorkerManager: $(if $(wildcard $(BIN_DIR)/test_workermanager),✅ exists,❌ missing)"
	@echo "Step 8: $(if $(wildcard $(BIN_DIR)/test_step8),✅ exists,❌ missing)"

debug-vars:
	@echo "SHARED_SRC_DIR: $(SHARED_SRC_DIR)"
	@echo "SHARED_LOGGING_SOURCES: $(SHARED_LOGGING_SOURCES)"
	@echo "SHARED_DATABASE_SOURCES: $(SHARED_DATABASE_SOURCES)"

help:
	@echo -e "$(BLUE)🚀 PulseOne Enhanced Build System with Complete Step 7 Integration$(NC)"
	@echo -e "$(YELLOW)====================================================================$(NC)"
	@echo ""
	@echo -e "$(MAGENTA)🎯 Enhanced Features:$(NC)"
	@echo "  Current platform: $(PLATFORM)"
	@echo "  Auto-logging: All tests create timestamped log files"
	@echo "  Log directory: $(LOG_DIR)"
	@echo "  NEW: Step 7 Complete REST API Backend Integration Test"
	@echo "  CURL support: $(if $(filter 1,$(HAS_CURL)),✅ enabled,❌ disabled (install libcurl4-openssl-dev))"
	@echo ""
	@echo -e "$(GREEN)Step-by-Step Test Commands (with logging):$(NC)"
	@echo "  make step1-test   - Step 1: Configuration and connection test"
	@echo "  make step2-test   - Step 2: Database entity validation test"
	@echo "  make step3-test   - Step 3: Protocol worker property validation test"
	@echo "  make step4-test   - Step 4: Driver data validation test"
	@echo "  make step5-test   - Step 5: Complete DB integration validation test"
	@echo "  make step6-test   - Step 6: RestAPI → WorkerManager integration test"
	@echo "  make step6-pipeline-test - Step 6: Enhanced alarm+virtualpoint pipeline test"
	@echo "  make step7-test   - Step 7: Complete REST API Backend Integration (NEW!)"
	@echo "  make network-test - Network: REST API server test"
	@echo "  make platform-test - Platform: Cross-platform compatibility test"
	@echo "  make workermanager-test - WorkerManager: Singleton and lifecycle test"
	@echo "  make step8-test   - Step 8: Driver reconnection and data pipeline test"
	@echo ""
	@echo -e "$(GREEN)Quick Build Commands:$(NC)"
	@echo "  make step1-build, step2-build, ..., step7-build"
	@echo "  make all-tests-build - Build all tests"
	@echo ""
	@echo -e "$(GREEN)Integrated Execution:$(NC)"
	@echo "  make all-tests-run - Run all tests with comprehensive logging"
	@echo "  make quick-test    - Run core tests (WorkerManager + Platform + Step7)"
	@echo "  make restapi-test  - Run RestAPI related tests (Step6 + Step7 + Network)"
	@echo ""
	@echo -e "$(CYAN)🗂️ Log Management:$(NC)"
	@echo "  make check-logs      - Check log directory status"
	@echo "  make logs-summary    - View summary of latest test logs"
	@echo "  make clean-old-logs  - Remove logs older than 7 days"
	@echo "  make clean-all-logs  - Remove all log files"
	@echo ""
	@echo -e "$(GREEN)Utility Commands:$(NC)"
	@echo "  make status       - Check build and log status"
	@echo "  make clean        - Clean build files (preserve logs)"
	@echo "  make clean-all    - Clean everything (including logs)"
	@echo "  make create-dirs  - Create all required directories"
	@echo ""
	@echo -e "$(CYAN)💡 Step 7 Complete REST API Backend Integration Features:$(NC)"
	@echo "  - Self-contained HTTP server (port 8081) - no external dependencies"
	@echo "  - DeviceApiCallbacks + HardwareApiCallbacks full integration"
	@echo "  - Worker lifecycle control (start/stop/pause/resume/restart)"
	@echo "  - Hardware control (digital/analog outputs, parameters)"
	@echo "  - System control APIs (stats, reload, reinitialization)"
	@echo "  - Concurrent request handling stress test"
	@echo "  - End-to-end integration flow validation"
	@echo "  - Comprehensive HTTP response validation"
	@echo ""
	@echo -e "$(YELLOW)🎯 Recommended Test Sequence:$(NC)"
	@echo "  1. make step7-test        - Most comprehensive test"
	@echo "  2. make restapi-test      - All REST API related tests"
	@echo "  3. make all-tests-run     - Complete system validation"
	@echo ""
	@echo -e "$(RED)⚠️ Dependencies for Step 7:$(NC)"
	@echo "  sudo apt install libcurl4-openssl-dev libgtest-dev nlohmann-json3-dev"

.PHONY: create-log-dir logs-summary check-logs clean-old-logs clean-all-logs \
        step1-test step2-test step3-test step4-test step5-test step6-test step6-pipeline-test step7-test step8-test step9-test \
        step10-test step11-test step12-test \
        network-test platform-test workermanager-test quick-test all-tests-run restapi-test \
        clean clean-all help status

# 자동 종속성 생성
-include $(ALL_OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)

# Step 13 테스트
step13-build: create-dirs $(BIN_DIR)/test_step13

step13-test: step13-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step13))
	@echo -e "$(MAGENTA)🚀 Running Step 13 BACnet Discovery Verification [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)BACnet Discovery Persistence Test$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing: Discovery Callback -> RDB Persistence$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 13)
	@./$(BIN_DIR)/test_step13 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 13)
	@echo -e "$(GREEN)Step 13 Test completed. Log saved: $(LOG_FILE)$(NC)"

$(BIN_DIR)/test_step13: $(OBJ_DIR)/test_step13_bacnet_discovery.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Bacnet/BACnetBIPPort.o $(OBJ_DIR)/Drivers/Bacnet/BACnetServiceManager.o $(OBJ_DIR)/Workers/Protocol/BACnetWorker.o $(OBJ_DIR)/Workers/Protocol/BACnetDiscoveryService.o $(filter-out $(OBJ_DIR)/Workers/Protocol/% $(OBJ_DIR)/Workers/WorkerRegistry.o $(OBJ_DIR)/Workers/WorkerScheduler.o $(OBJ_DIR)/Workers/WorkerMonitor.o $(OBJ_DIR)/Workers/WorkerManager.o $(OBJ_DIR)/Workers/WorkerFactory.o $(OBJ_DIR)/Core/Application.o $(OBJ_DIR)/Core/Main.o $(OBJ_DIR)/Api/% $(OBJ_DIR)/Network/%, $(CORE_OBJECTS))
	mkdir -p $(BIN_DIR)
	echo -e "$(PURPLE)🔗 Linking Step 13 Test [$(PLATFORM)]...$(NC)"
	$(CXX) $(OBJ_DIR)/test_step13_bacnet_discovery.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Bacnet/BACnetBIPPort.o $(OBJ_DIR)/Drivers/Bacnet/BACnetServiceManager.o $(OBJ_DIR)/Workers/Protocol/BACnetWorker.o $(OBJ_DIR)/Workers/Protocol/BACnetDiscoveryService.o $(filter-out $(OBJ_DIR)/Workers/Protocol/% $(OBJ_DIR)/Workers/WorkerRegistry.o $(OBJ_DIR)/Workers/WorkerScheduler.o $(OBJ_DIR)/Workers/WorkerMonitor.o $(OBJ_DIR)/Workers/WorkerManager.o $(OBJ_DIR)/Workers/WorkerFactory.o $(OBJ_DIR)/Core/Application.o $(OBJ_DIR)/Core/Main.o $(OBJ_DIR)/Api/% $(OBJ_DIR)/Network/%, $(CORE_OBJECTS)) $(TEST_LIBS) $(LIBS) -o $@
	echo -e "$(GREEN)✅ Step 13 Test build completed!$(NC)"
	@echo -e "$(GREEN)✅ Step 13 Test build completed!$(NC)"

$(OBJ_DIR)/test_step13_bacnet_discovery.o: test_step13_bacnet_discovery.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🔍 Compiling Step 13 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Step 14 테스트
step14-build: create-dirs $(BIN_DIR)/test_step14

step14-test: step14-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step14))
	@echo -e "$(MAGENTA)🚀 Running Step 14 BACnet Schedule Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)BACnet Weekly Schedule Test (Prop 123)$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(BLUE)Testing: Read/Write Weekly_Schedule (Prop 123)$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 14)
	@./$(BIN_DIR)/test_step14 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 14)
	@echo -e "$(GREEN)Step 14 Test completed. Log saved: $(LOG_FILE)$(NC)"

$(BIN_DIR)/test_step14: $(OBJ_DIR)/test_step14_bacnet_schedule.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Bacnet/BACnetBIPPort.o $(OBJ_DIR)/Drivers/Bacnet/BACnetServiceManager.o $(filter-out $(OBJ_DIR)/Workers/Protocol/% $(OBJ_DIR)/Workers/WorkerRegistry.o $(OBJ_DIR)/Workers/WorkerScheduler.o $(OBJ_DIR)/Workers/WorkerMonitor.o $(OBJ_DIR)/Workers/WorkerManager.o $(OBJ_DIR)/Workers/WorkerFactory.o $(OBJ_DIR)/Core/Application.o $(OBJ_DIR)/Core/Main.o $(OBJ_DIR)/Api/% $(OBJ_DIR)/Network/%, $(CORE_OBJECTS))
	mkdir -p $(BIN_DIR)
	echo -e "$(PURPLE)🔗 Linking Step 14 Test [$(PLATFORM)]...$(NC)"
	$(CXX) $(OBJ_DIR)/test_step14_bacnet_schedule.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Bacnet/BACnetBIPPort.o $(OBJ_DIR)/Drivers/Bacnet/BACnetServiceManager.o $(filter-out $(OBJ_DIR)/Workers/Protocol/% $(OBJ_DIR)/Workers/WorkerRegistry.o $(OBJ_DIR)/Workers/WorkerScheduler.o $(OBJ_DIR)/Workers/WorkerMonitor.o $(OBJ_DIR)/Workers/WorkerManager.o $(OBJ_DIR)/Workers/WorkerFactory.o $(OBJ_DIR)/Core/Application.o $(OBJ_DIR)/Core/Main.o $(OBJ_DIR)/Api/% $(OBJ_DIR)/Network/%, $(CORE_OBJECTS)) $(TEST_LIBS) $(LIBS) -o $@
	echo -e "$(GREEN)✅ Step 14 Test build completed!$(NC)"
	@echo -e "$(GREEN)✅ Step 14 Test build completed!$(NC)"

$(OBJ_DIR)/test_step14_bacnet_schedule.o: test_step14_bacnet_schedule.cpp
	@mkdir -p $(OBJ_DIR)
	@echo -e "$(CYAN)🔍 Compiling Step 14 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
# Step 19: ROS Integration E2E Test
step19-e2e-build: create-dirs $(BIN_DIR)/test_step19_ros_e2e

$(OBJ_DIR)/test_step19_ros_e2e.o: test_step19_ros_e2e.cpp $(HEADERS)
	@echo -e "-e $(PURPLE)🔨 Compiling Step 19 E2E Test...$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c test_step19_ros_e2e.cpp -o $@

$(BIN_DIR)/test_step19_ros_e2e: $(OBJ_DIR)/test_step19_ros_e2e.o $(CORE_OBJECTS) $(OPCUA_C_OBJ) $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o
	@echo -e "-e $(BLUE)🔗 Linking Step 19 E2E Test [$(PLATFORM)]...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(PLATFORM_LIBS) $(MODBUS_LIBS) $(MYSQL_LIBS) $(MQTT_CPP_LIBS) $(MQTT_C_LIBS) $(PGSQL_LIBS) $(BACNET_LIBS) $(REDIS_LIBS) $(QUICKJS_LIBS) $(CURL_LIBS) $(TEST_LIBS) -lsqlite3 -lbluetooth
	@echo -e "-e $(GREEN)✅ Step 19 E2E Test build completed!$(NC)"

step19-e2e-test: step19-e2e-build create-log-dir
	@echo -e "-e $(CYAN)🚀 Running Step 19 E2E Test (ROS Integration)...$(NC)"
	@$(BIN_DIR)/test_step19_ros_e2e > $(LOG_DIR)/step19_ros_e2e.log 2>&1 || (echo -e "-e $(RED)❌ Step 19 Test Failed! Check logs at $(LOG_DIR)/step19_ros_e2e.log$(NC)"; cat $(LOG_DIR)/step19_ros_e2e.log; exit 1)
	@echo -e "-e $(GREEN)✅ Step 19 Test Passed!$(NC)"

# =============================================================================
# Step 15: Network Scan & Discovery
# =============================================================================

step15-build: create-dirs $(BIN_DIR)/test_step15_network_scan

$(OBJ_DIR)/test_step15_network_scan.o: test_step15_network_scan.cpp $(HEADERS)
	@echo -e "-e $(PURPLE)🔨 Compiling Step 15 Test...$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c test_step15_network_scan.cpp -o $@

# ==========================================
# Step 16: BLE Beacon
# ==========================================

step16-beacon-build: create-dirs $(BIN_DIR)/test_step16_ble_beacon

$(OBJ_DIR)/test_step16_ble_beacon.o: test_step16_ble_beacon.cpp $(HEADERS)
	@echo -e "-e $(PURPLE)🔨 Compiling Step 16 Beacon Test...$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c test_step16_ble_beacon.cpp -o $@

$(BIN_DIR)/test_step16_ble_beacon: $(OBJ_DIR)/test_step16_ble_beacon.o $(CORE_OBJECTS) $(PLUGIN_DRIVER_OBJECTS)
	@echo -e "-e $(BLUE)🔗 Linking Step 16 Beacon Test...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(PLATFORM_LIBS) $(MODBUS_LIBS) $(MYSQL_LIBS) $(MQTT_CPP_LIBS) $(MQTT_C_LIBS) $(PGSQL_LIBS) $(BACNET_LIBS) $(REDIS_LIBS) $(QUICKJS_LIBS) $(CURL_LIBS) $(TEST_LIBS) -lsqlite3 -lbluetooth
	@echo -e "-e $(GREEN)✅ Step 16 Beacon Test build completed!$(NC)"

step16-beacon-test: step16-beacon-build create-log-dir
	@echo -e "-e $(CYAN)🚀 Running Step 16 Test (BLE Beacon)...$(NC)"
	@$(BIN_DIR)/test_step16_ble_beacon > $(LOG_DIR)/step16_ble_beacon.log 2>&1 || (echo -e "-e $(RED)❌ Step 16 Test Failed! Check logs at $(LOG_DIR)/step16_ble_beacon.log$(NC)"; cat $(LOG_DIR)/step16_ble_beacon.log; exit 1)
	@echo -e "-e $(GREEN)✅ Step 16 Test Passed!$(NC)"

$(BIN_DIR)/test_step15_network_scan: $(OBJ_DIR)/test_step15_network_scan.o $(CORE_OBJECTS) $(PLUGIN_DRIVER_OBJECTS)
	@echo -e "-e $(BLUE)🔗 Linking Step 15 Test...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(PLATFORM_LIBS) $(MODBUS_LIBS) $(MYSQL_LIBS) $(MQTT_CPP_LIBS) $(MQTT_C_LIBS) $(PGSQL_LIBS) $(BACNET_LIBS) $(REDIS_LIBS) $(QUICKJS_LIBS) $(CURL_LIBS) $(TEST_LIBS) -lsqlite3 -lbluetooth
	@echo -e "-e $(GREEN)✅ Step 15 Test build completed!$(NC)"

step15-test: step15-build create-log-dir
	@echo -e "-e $(CYAN)🚀 Running Step 15 Test (Network Scan)...$(NC)"
	@$(BIN_DIR)/test_step15_network_scan > $(LOG_DIR)/step15_network_scan.log 2>&1 || (echo -e "-e $(RED)❌ Step 15 Test Failed! Check logs at $(LOG_DIR)/step15_network_scan.log$(NC)"; cat $(LOG_DIR)/step15_network_scan.log; exit 1)
	@echo -e "-e $(GREEN)✅ Step 15 Test Passed!$(NC)"

# =============================================================================
# Step 20: Dynamic ROS Mapping
# =============================================================================

step20-build: create-dirs $(BIN_DIR)/test_step20_ros_dynamic

$(OBJ_DIR)/test_step20_ros_dynamic.o: test_step20_ros_dynamic.cpp $(HEADERS)
	@echo -e "-e $(PURPLE)🔨 Compiling Step 20 Test...$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c test_step20_ros_dynamic.cpp -o $@

$(BIN_DIR)/test_step20_ros_dynamic: $(OBJ_DIR)/test_step20_ros_dynamic.o $(CORE_OBJECTS) $(OPCUA_C_OBJ) $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o
	@echo -e "-e $(BLUE)🔗 Linking Step 20 Test...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(PLATFORM_LIBS) $(MODBUS_LIBS) $(MYSQL_LIBS) $(MQTT_CPP_LIBS) $(MQTT_C_LIBS) $(PGSQL_LIBS) $(BACNET_LIBS) $(REDIS_LIBS) $(QUICKJS_LIBS) $(CURL_LIBS) $(TEST_LIBS) -lsqlite3 -lbluetooth
	@echo -e "-e $(GREEN)✅ Step 20 Test build completed!$(NC)"

step20-test: step20-build create-log-dir
	@echo -e "-e $(CYAN)🚀 Running Step 20 Test (Dynamic ROS Topics)...$(NC)"
	@$(BIN_DIR)/test_step20_ros_dynamic > $(LOG_DIR)/step20_ros_dynamic.log 2>&1 || (echo -e "-e $(RED)❌ Step 20 Test Failed! Check logs at $(LOG_DIR)/step20_ros_dynamic.log$(NC)"; cat $(LOG_DIR)/step20_ros_dynamic.log; exit 1)
	@echo -e "-e $(GREEN)✅ Step 20 Test Passed!$(NC)"

# =============================================================================
# Step 21: ROS Recovery
# =============================================================================

step21-build: create-dirs $(BIN_DIR)/test_step21_ros_recovery

$(OBJ_DIR)/test_step21_ros_recovery.o: test_step21_ros_recovery.cpp $(HEADERS)
	@echo -e "-e $(PURPLE)🔨 Compiling Step 21 Test...$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c test_step21_ros_recovery.cpp -o $@

$(BIN_DIR)/test_step21_ros_recovery: $(OBJ_DIR)/test_step21_ros_recovery.o $(CORE_OBJECTS) $(OPCUA_C_OBJ) $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o
	@echo -e "-e $(BLUE)🔗 Linking Step 21 Test...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(PLATFORM_LIBS) $(MODBUS_LIBS) $(MYSQL_LIBS) $(MQTT_CPP_LIBS) $(MQTT_C_LIBS) $(PGSQL_LIBS) $(BACNET_LIBS) $(REDIS_LIBS) $(QUICKJS_LIBS) $(CURL_LIBS) $(TEST_LIBS) -lsqlite3 -lbluetooth
	@echo -e "-e $(GREEN)✅ Step 21 Test build completed!$(NC)"

step21-recovery: step21-build create-log-dir
	@echo -e "-e $(CYAN)🚀 Running Step 21 Test (ROS Recovery)...$(NC)"
	@$(BIN_DIR)/test_step21_ros_recovery > $(LOG_DIR)/step21_ros_recovery.log 2>&1 || (echo -e "-e $(RED)❌ Step 21 Test Failed! Check logs at $(LOG_DIR)/step21_ros_recovery.log$(NC)"; cat $(LOG_DIR)/step21_ros_recovery.log; exit 1)
	@echo -e "-e $(GREEN)✅ Step 21 Test Passed!$(NC)"

# =============================================================================
# Step 22: Modbus String Address
# =============================================================================

step22-build: create-dirs $(BIN_DIR)/test_step22_modbus_string

$(OBJ_DIR)/test_step22_modbus_string.o: test_step22_modbus_string.cpp $(HEADERS)
	@echo -e "-e $(PURPLE)🔨 Compiling Step 22 Test...$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c test_step22_modbus_string.cpp -o $@

$(BIN_DIR)/test_step22_modbus_string: $(OBJ_DIR)/test_step22_modbus_string.o $(CORE_OBJECTS) $(OPCUA_C_OBJ) $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o
	@echo -e "-e $(BLUE)🔗 Linking Step 22 Test...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(PLATFORM_LIBS) $(MODBUS_LIBS) $(MYSQL_LIBS) $(MQTT_CPP_LIBS) $(MQTT_C_LIBS) $(PGSQL_LIBS) $(BACNET_LIBS) $(REDIS_LIBS) $(QUICKJS_LIBS) $(CURL_LIBS) $(TEST_LIBS) -lsqlite3 -lbluetooth
	@echo -e "-e $(GREEN)✅ Step 22 Test build completed!$(NC)"

step22-modbus-string: step22-build create-log-dir
	@echo -e "-e $(CYAN)🚀 Running Step 22 Test (Modbus String Addresses)...$(NC)"
	@$(BIN_DIR)/test_step22_modbus_string > $(LOG_DIR)/step22_modbus_string.log 2>&1 || (echo -e "-e $(RED)❌ Step 22 Test Failed! Check logs at $(LOG_DIR)/step22_modbus_string.log$(NC)"; cat $(LOG_DIR)/step22_modbus_string.log; exit 1)
	@echo -e "-e $(GREEN)✅ Step 22 Test Passed!$(NC)"

# =============================================================================
# Step 23: Settings Reload & Redis Sync
# =============================================================================

step23-build: create-dirs $(BIN_DIR)/test_step23_settings_reload

$(OBJ_DIR)/test_step23_settings_reload.o: test_step23_settings_reload.cpp $(HEADERS)
	@echo -e "-e $(PURPLE)🔨 Compiling Step 23 Test...$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c test_step23_settings_reload.cpp -o $@

$(BIN_DIR)/test_step23_settings_reload: $(OBJ_DIR)/test_step23_settings_reload.o $(CORE_OBJECTS) $(OPCUA_C_OBJ) $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o
	@echo -e "-e $(BLUE)🔗 Linking Step 23 Test...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(PLATFORM_LIBS) $(MODBUS_LIBS) $(MYSQL_LIBS) $(MQTT_CPP_LIBS) $(MQTT_C_LIBS) $(PGSQL_LIBS) $(BACNET_LIBS) $(REDIS_LIBS) $(QUICKJS_LIBS) $(CURL_LIBS) $(TEST_LIBS) -lsqlite3 -lbluetooth
	@echo -e "-e $(GREEN)✅ Step 23 Test build completed!$(NC)"

step23-test: step23-build create-log-dir
	@echo -e "-e $(CYAN)🚀 Running Step 23 Test (Settings Reload & Redis Sync)...$(NC)"
	@$(BIN_DIR)/test_step23_settings_reload > $(LOG_DIR)/step23_settings_reload.log 2>&1 || (echo -e "-e $(RED)❌ Step 23 Test Failed! Check logs at $(LOG_DIR)/step23_settings_reload.log$(NC)"; cat $(LOG_DIR)/step23_settings_reload.log; exit 1)
	@echo -e "-e $(GREEN)✅ Step 23 Test Passed!$(NC)"

# Step 24 Enhanced OPC UA Test
step24-opcua-build: create-dirs $(BIN_DIR)/test_step24_opcua_enhanced

$(OBJ_DIR)/test_step24_opcua_enhanced.o: test_step24_opcua_enhanced.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(CYAN)⚙️ Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR)/test_step24_opcua_enhanced: $(ALL_OBJECTS) $(OBJ_DIR)/test_step24_opcua_enhanced.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 24 Enhanced OPC UA Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step24_opcua_enhanced.o $(OBJ_DIR)/Drivers/OPCUA/OPCUADriver.o $(OBJ_DIR)/Drivers/OPCUA/open62541.o $(OBJ_DIR)/Drivers/Bacnet/BACnetDriver.o $(OBJ_DIR)/Drivers/Mqtt/MqttDriver.o $(TEST_LIBS) $(LIBS) -lbluetooth -o $@
	@echo -e "$(GREEN)✅ Step 24 Enhanced OPC UA Test build completed!$(NC)"

step24-test: step24-opcua-build create-log-dir
	$(eval LOG_FILE := $(call get_log_filename,step24_opcua))
	@echo -e "$(MAGENTA)🚀 Running Step 24 Enhanced OPC UA Test [$(PLATFORM)]...$(NC)"
	$(call create_log_summary,$(LOG_FILE),Step 24 Enhanced OPC UA)
	@./$(BIN_DIR)/test_step24_opcua_enhanced 2>&1 | tee $(LOG_FILE)
	@echo -e "$(YELLOW)============================================$(NC)"
	$(call analyze_log,$(LOG_FILE),Step 24 Enhanced OPC UA)
	@echo -e "$(GREEN)Step 24 Enhanced OPC UA Test completed. Log saved: $(LOG_FILE)$(NC)"
# =============================================================================
