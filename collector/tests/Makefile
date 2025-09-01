# =============================================================================
# collector/tests/Makefile - Complete Enhanced Version with Step6 RestAPI Test
# 모든 단계별 테스트 로그 파일 자동 생성 지원
# =============================================================================

# 성능 최적화 설정
NPROC := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += -j$(NPROC) --output-sync=target

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
HAS_MQTT_CPP := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqttpp3 >/dev/null 2>&1 && echo "1" || echo "0")
HAS_BACNET_STACK := $(shell [ -f "/usr/local/lib/libbacnet.a" ] && echo "1" || echo "0")
HAS_QUICKJS := $(shell pkg-config --exists quickjs && echo "1" || echo "0")
HAS_HTTPLIB := $(shell echo '\#include <httplib.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")

# 컴파일러 설정 (플랫폼별)
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -DPULSEONE_DEBUG_MODE \
           -O1 -fno-omit-frame-pointer \
           -Wno-unused-but-set-variable -Wno-unused-variable \
           -Wno-unused-parameter \
           -fdiagnostics-color=always

# 플랫폼별 컴파일 플래그 추가
ifeq ($(IS_WINDOWS),1)
    CXXFLAGS += -DPULSEONE_WINDOWS=1 -DPULSEONE_LINUX=0 \
                -D_WIN32_WINNT=0x0A00 -DWIN32_LEAN_AND_MEAN -DNOMINMAX
    PLATFORM_LIBS = -lws2_32 -liphlpapi -lkernel32 -luser32
else
    CXXFLAGS += -DPULSEONE_WINDOWS=0 -DPULSEONE_LINUX=1
    PLATFORM_LIBS = 
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

# Include 경로
INCLUDES = -I$(INCLUDE_DIR) \
           -I$(INCLUDE_DIR)/Platform \
           -I$(INCLUDE_DIR)/Api \
           -I$(INCLUDE_DIR)/Alarm \
           -I$(INCLUDE_DIR)/VirtualPoint \
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
           -I$(INCLUDE_DIR)/Pipeline \
           -I$(INCLUDE_DIR)/Storage \
           -I$(INCLUDE_DIR)/Client

# Modbus 라이브러리 설정
ifneq ($(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lmodbus >/dev/null 2>&1; echo $$?),0)
    MODBUS_LIBS = 
    MODBUS_INCLUDES = 
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

# MQTT 라이브러리 설정
ifeq ($(HAS_MQTT_C),1)
    MQTT_C_LIBS = -lpaho-mqtt3c
else
    MQTT_C_LIBS = 
endif

ifeq ($(HAS_MQTT_CPP),1)
    MQTT_CPP_LIBS = -lpaho-mqttpp3
    INCLUDES += -I/usr/local/include
else
    MQTT_CPP_LIBS = 
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
    REDIS_LIBS = -lhiredis -lhiredis_ssl
    CXXFLAGS += -DHAS_HIREDIS
else
    REDIS_LIBS = 
endif

# JSON 라이브러리 설정
ifeq ($(HAS_NLOHMANN_JSON),1)
    CXXFLAGS += -DHAS_NLOHMANN_JSON
endif

# QuickJS 라이브러리 설정
ifeq ($(HAS_QUICKJS),1)
    QUICKJS_LIBS = $(shell pkg-config --libs quickjs 2>/dev/null || echo "-lquickjs")
    QUICKJS_INCLUDES = $(shell pkg-config --cflags quickjs 2>/dev/null || echo "-I/usr/local/include")
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

# 전체 라이브러리 설정
BASIC_LIBS = -lpthread -lsqlite3 $(QUICKJS_LIBS) $(HTTP_LIBS) -lm -ldl $(PLATFORM_LIBS)
OPTIONAL_LIBS = $(MODBUS_LIBS) $(MYSQL_LIBS) $(PGSQL_LIBS) $(MQTT_C_LIBS) $(MQTT_CPP_LIBS) $(BACNET_LIBS) $(REDIS_LIBS)
LIBS = $(BASIC_LIBS) $(OPTIONAL_LIBS)
TEST_LIBS = -lgtest -lgtest_main -pthread

# 소스 파일 수집
CORE_SOURCES := $(wildcard $(SRC_DIR)/Core/*.cpp)
UTILS_SOURCES := $(wildcard $(SRC_DIR)/Utils/*.cpp)
CONFIG_SOURCES := $(wildcard $(SRC_DIR)/Config/*.cpp)
DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Entities/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)
CLIENT_SOURCES := $(wildcard $(SRC_DIR)/Client/*.cpp)
WORKERS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Protocol/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Components/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/WorkerFactory.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/WorkerManager.cpp)
STORAGE_SOURCES := $(wildcard $(SRC_DIR)/Storage/*.cpp)

ifeq ($(HAS_HTTPLIB),1)
    NETWORK_SOURCES := $(wildcard $(SRC_DIR)/Network/*.cpp)
    API_SOURCES := $(wildcard $(SRC_DIR)/Api/*.cpp)
else
    NETWORK_SOURCES :=
    API_SOURCES :=
endif

DRIVERS_SOURCES = $(wildcard $(SRC_DIR)/Drivers/Common/*.cpp)
ifneq ($(MODBUS_LIBS),)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Modbus/*.cpp)
endif
ifeq ($(HAS_MQTT_CPP),1)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Mqtt/*.cpp)
endif
ifeq ($(HAS_BACNET_STACK),1)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Bacnet/*.cpp)
endif

PIPELINE_SOURCES := $(wildcard $(SRC_DIR)/Pipeline/*.cpp)
ALARM_SOURCES := $(wildcard $(SRC_DIR)/Alarm/*.cpp)
VIRTUALPOINT_SOURCES := $(wildcard $(SRC_DIR)/VirtualPoint/*.cpp)
PLATFORM_SOURCES := $(wildcard $(SRC_DIR)/Platform/*.cpp)
PLUGIN_SOURCES := $(wildcard $(SRC_DIR)/Plugin/*.cpp)

ALL_SOURCES = $(CORE_SOURCES) $(UTILS_SOURCES) $(CONFIG_SOURCES) \
              $(DATABASE_SOURCES) $(CLIENT_SOURCES) $(WORKERS_SOURCES) \
              $(DRIVERS_SOURCES) $(PIPELINE_SOURCES) $(ALARM_SOURCES) \
              $(VIRTUALPOINT_SOURCES) $(NETWORK_SOURCES) $(API_SOURCES) \
              $(PLATFORM_SOURCES) $(PLUGIN_SOURCES) $(STORAGE_SOURCES) 

ALL_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(ALL_SOURCES))

TEST_OBJECTS = $(OBJ_DIR)/test_step1_config_connection.o \
               $(OBJ_DIR)/test_step2_database_entity.o \
               $(OBJ_DIR)/test_step3_protocol_worker.o \
               $(OBJ_DIR)/test_step4_driver_data_validation.o \
               $(OBJ_DIR)/test_step5_complete_db_integration_validation.o \
               $(OBJ_DIR)/test_step6_rest_api_worker_reload.o \
               $(OBJ_DIR)/test_step6_enhanced_pipeline.o \
               $(OBJ_DIR)/test_network_api.o \
               $(OBJ_DIR)/test_platform_compat.o \
               $(OBJ_DIR)/test_workermanager.o

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
	@mkdir -p $(OBJ_DIR)/Pipeline $(OBJ_DIR)/Alarm $(OBJ_DIR)/VirtualPoint $(OBJ_DIR)/Plugin
	@mkdir -p $(OBJ_DIR)/Network $(OBJ_DIR)/Api $(OBJ_DIR)/Platform $(OBJ_DIR)/Storage
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

$(BIN_DIR)/test_step3: $(ALL_OBJECTS) $(OBJ_DIR)/test_step3_protocol_worker.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 3 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step3_protocol_worker.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 3 Test build completed!$(NC)"

$(BIN_DIR)/test_step4: $(ALL_OBJECTS) $(OBJ_DIR)/test_step4_driver_data_validation.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 4 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step4_driver_data_validation.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 4 Test build completed!$(NC)"

$(BIN_DIR)/test_step5: $(ALL_OBJECTS) $(OBJ_DIR)/test_step5_complete_db_integration_validation.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 5 Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step5_complete_db_integration_validation.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 5 Test build completed!$(NC)"

$(BIN_DIR)/test_step6: $(ALL_OBJECTS) $(OBJ_DIR)/test_step6_rest_api_worker_reload.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 6 RestAPI-WorkerManager Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step6_rest_api_worker_reload.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 6 RestAPI-WorkerManager Test build completed!$(NC)"

$(BIN_DIR)/test_step6_pipeline: $(ALL_OBJECTS) $(OBJ_DIR)/test_step6_enhanced_pipeline.o
	@mkdir -p $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Step 6 Enhanced Pipeline Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step6_enhanced_pipeline.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Step 6 Enhanced Pipeline Test build completed!$(NC)"

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

# =============================================================================
# 향상된 단계별 테스트 실행 (로그 포함)
# =============================================================================

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
	@./$(BIN_DIR)/test_step5 2>&1 | tee $(LOG_FILE)
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
step6-pipeline-build: create-dirs $(BIN_DIR)/test_step6_pipeline

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
	@for step in step1 step2 step3 step4 step5 step6 step6_pipeline network platform workermanager; do \
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

all-tests-build: create-dirs $(BIN_DIR)/test_step1 $(BIN_DIR)/test_step2 $(BIN_DIR)/test_step3 $(BIN_DIR)/test_step4 $(BIN_DIR)/test_step5 $(BIN_DIR)/test_step6 $(BIN_DIR)/test_step6_pipeline $(BIN_DIR)/test_network $(BIN_DIR)/test_platform $(BIN_DIR)/test_workermanager
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
	@$(MAKE) step1-test step2-test step3-test step4-test step5-test step6-test network-test platform-test workermanager-test
	@echo ""
	@echo "========================================" >> $(MASTER_LOG)
	@echo "All Tests Completed: $(shell date)" >> $(MASTER_LOG)
	@echo "========================================" >> $(MASTER_LOG)
	@echo -e "$(GREEN)🎉 All tests completed with logging!$(NC)"
	@echo -e "$(CYAN)Master log: $(MASTER_LOG)$(NC)"
	@$(MAKE) logs-summary

quick-test: workermanager-test platform-test step6-test
	@echo -e "$(GREEN)✅ Quick test completed with logging$(NC)"

restapi-test: step6-test network-test
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
	@echo "Binary Status:"
	@echo "Step 1: $(if $(wildcard $(BIN_DIR)/test_step1),✅ exists,❌ missing)"
	@echo "Step 2: $(if $(wildcard $(BIN_DIR)/test_step2),✅ exists,❌ missing)"
	@echo "Step 3: $(if $(wildcard $(BIN_DIR)/test_step3),✅ exists,❌ missing)"
	@echo "Step 4: $(if $(wildcard $(BIN_DIR)/test_step4),✅ exists,❌ missing)"
	@echo "Step 5: $(if $(wildcard $(BIN_DIR)/test_step5),✅ exists,❌ missing)"
	@echo "Step 6 RestAPI: $(if $(wildcard $(BIN_DIR)/test_step6),✅ exists,❌ missing)"
	@echo "Step 6 Pipeline: $(if $(wildcard $(BIN_DIR)/test_step6_pipeline),✅ exists,❌ missing)"
	@echo "Network: $(if $(wildcard $(BIN_DIR)/test_network),✅ exists,❌ missing)"
	@echo "Platform: $(if $(wildcard $(BIN_DIR)/test_platform),✅ exists,❌ missing)"
	@echo "WorkerManager: $(if $(wildcard $(BIN_DIR)/test_workermanager),✅ exists,❌ missing)"

help:
	@echo -e "$(BLUE)🚀 PulseOne Enhanced Build System with Step 6 RestAPI Integration$(NC)"
	@echo -e "$(YELLOW)====================================================================$(NC)"
	@echo ""
	@echo -e "$(MAGENTA)🎯 Enhanced Features:$(NC)"
	@echo "  Current platform: $(PLATFORM)"
	@echo "  Auto-logging: All tests create timestamped log files"
	@echo "  Log directory: $(LOG_DIR)"
	@echo "  NEW: Step 6 RestAPI-WorkerManager integration test"
	@echo ""
	@echo -e "$(GREEN)Step-by-Step Test Commands (with logging):$(NC)"
	@echo "  make step1-test   - Step 1: Configuration and connection test"
	@echo "  make step2-test   - Step 2: Database entity validation test"
	@echo "  make step3-test   - Step 3: Protocol worker property validation test"
	@echo "  make step4-test   - Step 4: Driver data validation test"
	@echo "  make step5-test   - Step 5: Complete DB integration validation test"
	@echo "  make step6-test   - Step 6: RestAPI → WorkerManager integration test (NEW!)"
	@echo "  make step6-pipeline-test - Step 6: Enhanced alarm+virtualpoint pipeline test"
	@echo "  make network-test - Network: REST API server test"
	@echo "  make platform-test - Platform: Cross-platform compatibility test"
	@echo "  make workermanager-test - WorkerManager: Singleton and lifecycle test"
	@echo ""
	@echo -e "$(GREEN)Quick Build Commands:$(NC)"
	@echo "  make step1-build, step2-build, ..., step6-build"
	@echo "  make all-tests-build - Build all tests"
	@echo ""
	@echo -e "$(GREEN)Integrated Execution:$(NC)"
	@echo "  make all-tests-run - Run all tests with comprehensive logging"
	@echo "  make quick-test    - Run core tests (WorkerManager + Platform + Step6)"
	@echo "  make restapi-test  - Run RestAPI related tests (Step6 + Network)"
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
	@echo -e "$(CYAN)💡 Step 6 RestAPI Integration Test Features:$(NC)"
	@echo "  - DB change → API call → Worker reload flow"
	@echo "  - Engineer property update simulation"
	@echo "  - Redis data consistency verification"
	@echo "  - RestAPI callback connectivity testing"
	@echo "  - Multi-device reload stress testing"

.PHONY: create-log-dir logs-summary check-logs clean-old-logs clean-all-logs \
        step1-test step2-test step3-test step4-test step5-test step6-test step6-pipeline-test \
        network-test platform-test workermanager-test quick-test all-tests-run restapi-test \
        clean clean-all help status

# 자동 종속성 생성
-include $(ALL_OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)