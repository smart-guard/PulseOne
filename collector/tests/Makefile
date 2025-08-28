# =============================================================================
# collector/tests/Makefile - WorkerManager + API 통합 완성본
# WorkerManager, DeviceApiCallbacks 포함한 완전한 컴파일 지원
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

# 증분 컴파일용 디렉토리
OBJ_DIR = ./obj
BIN_DIR = ./bin
DATA_DIR = ./data

# 경로 설정
PARENT_DIR = ..
INCLUDE_DIR = $(PARENT_DIR)/include
SRC_DIR = $(PARENT_DIR)/src

# Include 경로 (WorkerManager 추가)
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
           -I$(INCLUDE_DIR)/Client

# 라이브러리 설정
ifneq ($(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lmodbus >/dev/null 2>&1; echo $$?),0)
    MODBUS_LIBS = 
    MODBUS_INCLUDES = 
else
    MODBUS_LIBS = -lmodbus
    MODBUS_INCLUDES = $(shell pkg-config --cflags libmodbus 2>/dev/null || echo "")
    INCLUDES += $(MODBUS_INCLUDES)
endif

MYSQL_CONFIG := $(shell which mysql_config 2>/dev/null)
ifneq ($(MYSQL_CONFIG),)
    MYSQL_LIBS = $(shell mysql_config --libs)
    MYSQL_INCLUDES = $(shell mysql_config --include)
    INCLUDES += $(MYSQL_INCLUDES)
else
    MYSQL_LIBS = 
    CXXFLAGS += -DDISABLE_MYSQL_FEATURES
endif

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

ifneq ($(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpqxx >/dev/null 2>&1; echo $$?),0)
    PGSQL_LIBS = 
else
    PGSQL_LIBS = -lpqxx -lpq
endif

ifeq ($(HAS_BACNET_STACK), 1)
    BACNET_LIBS = -lbacnet -lm
    BACNET_INCLUDES = -I/usr/local/include/bacnet
    INCLUDES += $(BACNET_INCLUDES)
    CXXFLAGS += -DHAS_BACNET_STACK=1
else
    CXXFLAGS += -DHAS_BACNET_STACK=0
    BACNET_LIBS = 
endif

ifeq ($(HAS_HIREDIS),1)
    REDIS_LIBS = -lhiredis -lhiredis_ssl
    CXXFLAGS += -DHAS_HIREDIS
else
    REDIS_LIBS = 
endif

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
    $(warning ⚠️  httplib not available - REST API disabled)
endif

# 전체 라이브러리 설정 (플랫폼 라이브러리 포함)
BASIC_LIBS = -lpthread -lsqlite3 $(QUICKJS_LIBS) $(HTTP_LIBS) -lm -ldl $(PLATFORM_LIBS)
OPTIONAL_LIBS = $(MODBUS_LIBS) $(MYSQL_LIBS) $(PGSQL_LIBS) $(MQTT_C_LIBS) $(MQTT_CPP_LIBS) $(BACNET_LIBS) $(REDIS_LIBS)
LIBS = $(BASIC_LIBS) $(OPTIONAL_LIBS)
TEST_LIBS = -lgtest -lgtest_main -pthread

# =============================================================================
# 소스 파일 수집 - WorkerManager와 API 완전 포함
# =============================================================================

# Core 소스들
CORE_SOURCES := $(wildcard $(SRC_DIR)/Core/*.cpp)

# Utils 소스들
UTILS_SOURCES := $(wildcard $(SRC_DIR)/Utils/*.cpp)

# Config 소스들
CONFIG_SOURCES := $(wildcard $(SRC_DIR)/Config/*.cpp)

# Database 소스들
DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Entities/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)

# Client 소스들
CLIENT_SOURCES := $(wildcard $(SRC_DIR)/Client/*.cpp)

# Workers 소스들 - WorkerManager 포함!
WORKERS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Protocol/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Components/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/WorkerFactory.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/WorkerManager.cpp)

# Network 소스들 (조건부 포함)
ifeq ($(HAS_HTTPLIB),1)
    NETWORK_SOURCES := $(wildcard $(SRC_DIR)/Network/*.cpp)
    $(info ✅ Including Network sources: $(words $(NETWORK_SOURCES)) files)
else
    NETWORK_SOURCES :=
    $(warning ⚠️  Network sources excluded - httplib not available)
endif

# API 소스들 - DeviceApiCallbacks 포함! (조건부 포함)
ifeq ($(HAS_HTTPLIB),1)
    API_SOURCES := $(wildcard $(SRC_DIR)/Api/*.cpp)
    $(info ✅ Including API sources: $(words $(API_SOURCES)) files)
    $(info    - DeviceApiCallbacks.cpp included for REST API device control)
else
    API_SOURCES :=
    $(warning ⚠️  API sources excluded - httplib not available)
endif

# Drivers 소스들 (조건부 포함)
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

# Pipeline 소스들
PIPELINE_SOURCES := $(wildcard $(SRC_DIR)/Pipeline/*.cpp)

# 알람 및 가상포인트 소스들
ALARM_SOURCES := $(wildcard $(SRC_DIR)/Alarm/*.cpp)
VIRTUALPOINT_SOURCES := $(wildcard $(SRC_DIR)/VirtualPoint/*.cpp)

# Platform 소스들
PLATFORM_SOURCES := $(wildcard $(SRC_DIR)/Platform/*.cpp)

# Plugin 소스들
PLUGIN_SOURCES := $(wildcard $(SRC_DIR)/Plugin/*.cpp)

# 전체 소스 파일 (WorkerManager, API 포함)
ALL_SOURCES = $(CORE_SOURCES) $(UTILS_SOURCES) $(CONFIG_SOURCES) \
              $(DATABASE_SOURCES) $(CLIENT_SOURCES) $(WORKERS_SOURCES) \
              $(DRIVERS_SOURCES) $(PIPELINE_SOURCES) $(ALARM_SOURCES) \
              $(VIRTUALPOINT_SOURCES) $(NETWORK_SOURCES) $(API_SOURCES) \
              $(PLATFORM_SOURCES) $(PLUGIN_SOURCES)

# 오브젝트 파일 생성
ALL_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(ALL_SOURCES))

# 테스트 파일들 정의
TEST_OBJECTS = $(OBJ_DIR)/test_step3_workers.o \
               $(OBJ_DIR)/test_step4_driver_data_validation.o \
               $(OBJ_DIR)/test_step5_complete_db_integration_validation.o \
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
# 증분 컴파일 타겟들 (WorkerManager, Api 디렉토리 추가)
# =============================================================================

.DEFAULT_GOAL := help

# 디렉토리 생성 (WorkerManager, Api 디렉토리 추가)
$(OBJ_DIR):
	@echo -e "$(BLUE)🏗️ Creating object directories for $(PLATFORM) (with WorkerManager + API)...$(NC)"
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/Core $(OBJ_DIR)/Utils $(OBJ_DIR)/Config 
	@mkdir -p $(OBJ_DIR)/Database/Entities $(OBJ_DIR)/Database/Repositories
	@mkdir -p $(OBJ_DIR)/Client $(OBJ_DIR)/Workers/Base $(OBJ_DIR)/Workers/Protocol $(OBJ_DIR)/Workers/Components
	@mkdir -p $(OBJ_DIR)/Drivers/Common $(OBJ_DIR)/Drivers/Modbus $(OBJ_DIR)/Drivers/Mqtt $(OBJ_DIR)/Drivers/Bacnet
	@mkdir -p $(OBJ_DIR)/Pipeline $(OBJ_DIR)/Alarm $(OBJ_DIR)/VirtualPoint $(OBJ_DIR)/Plugin
	@mkdir -p $(OBJ_DIR)/Network $(OBJ_DIR)/Api $(OBJ_DIR)/Platform
	@echo -e "$(GREEN)✅ Directories created for $(PLATFORM) platform (WorkerManager + API support)$(NC)"

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# 개별 오브젝트 파일 컴파일
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Generating dependencies for $<$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $@ $< > $(patsubst %.o,%.d,$@)
	@echo -e "$(YELLOW)⚙️  Compiling $< [$(PLATFORM)]$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# WorkerManager 테스트 컴파일
$(OBJ_DIR)/test_workermanager.o: test_workermanager.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Generating dependencies for test_workermanager.cpp$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $@ $< > $(patsubst %.o,%.d,$@)
	@echo -e "$(MAGENTA)👷 Compiling WorkerManager Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 플랫폼 호환성 테스트
$(OBJ_DIR)/test_platform_compat.o: test_platform_compat.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Generating dependencies for test_platform_compat.cpp$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $@ $< > $(patsubst %.o,%.d,$@)
	@echo -e "$(MAGENTA)🛡️  Compiling Platform Compatibility Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 기존 테스트 파일들 컴파일
$(OBJ_DIR)/test_step3_workers.o: test_step3_workers.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Generating dependencies for test_step3_workers.cpp$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $@ $< > $(patsubst %.o,%.d,$@)
	@echo -e "$(YELLOW)👷 Compiling test_step3_workers [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step4_driver_data_validation.o: test_step4_driver_data_validation.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Generating dependencies for test_step4_driver_data_validation.cpp$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $@ $< > $(patsubst %.o,%.d,$@)
	@echo -e "$(YELLOW)🔌 Compiling test_step4_driver_data_validation [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step5_complete_db_integration_validation.o: test_step5_complete_db_integration_validation.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Generating dependencies for test_step5_complete_db_integration_validation.cpp$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $@ $< > $(patsubst %.o,%.d,$@)
	@echo -e "$(YELLOW)🗄️  Compiling test_step5_complete_db_integration_validation [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step6_enhanced_pipeline.o: test_step6_enhanced_pipeline.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Generating dependencies for test_step6_enhanced_pipeline.cpp$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $@ $< > $(patsubst %.o,%.d,$@)
	@echo -e "$(YELLOW)⚡ Compiling test_step6_enhanced_pipeline [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Network API 테스트
$(OBJ_DIR)/test_network_api.o: test_network_api.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Generating dependencies for test_network_api.cpp$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $@ $< > $(patsubst %.o,%.d,$@)
	@echo -e "$(YELLOW)🌐 Compiling test_network_api [$(PLATFORM)]...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Api 파일들 컴파일 룰 수정
$(BUILD_DIR)/Api/%.o: $(SRC_DIR)/Api/%.cpp | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Api
	@echo -e "$(GREEN)🔗 Compiling Api: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 의존성 파일 생성 룰도 수정
$(BUILD_DIR)/Api/%.d: $(SRC_DIR)/Api/%.cpp | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Api
	@echo -e "🔨 Generating dependencies for $<"
	@$(CXX) -MM $(CXXFLAGS) $(INCLUDES) $< > $@.tmp
	@sed 's,\($*\)\.o[ :]*,$(BUILD_DIR)/Api/\1.o $@ : ,g' < $@.tmp > $@
	@rm -f $@.tmp
# =============================================================================
# 테스트 실행 파일들 (WorkerManager 테스트 추가)
# =============================================================================

# WorkerManager 테스트
$(BIN_DIR)/test_workermanager: $(ALL_OBJECTS) $(OBJ_DIR)/test_workermanager.o | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking WorkerManager Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_workermanager.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ WorkerManager Test build completed for $(PLATFORM)!$(NC)"

# 플랫폼 호환성 테스트
$(BIN_DIR)/test_platform: $(ALL_OBJECTS) $(OBJ_DIR)/test_platform_compat.o | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking Platform Compatibility Test [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_platform_compat.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Platform Compatibility Test build completed for $(PLATFORM)!$(NC)"

$(BIN_DIR)/test_step3: $(ALL_OBJECTS) $(OBJ_DIR)/test_step3_workers.o | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking test_step3 [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step3_workers.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ test_step3 build completed for $(PLATFORM)!$(NC)"

$(BIN_DIR)/test_step4: $(ALL_OBJECTS) $(OBJ_DIR)/test_step4_driver_data_validation.o | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking test_step4 (Driver Data Validation) [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step4_driver_data_validation.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ test_step4 (Driver Data Validation) build completed for $(PLATFORM)!$(NC)"

$(BIN_DIR)/test_step5: $(ALL_OBJECTS) $(OBJ_DIR)/test_step5_complete_db_integration_validation.o | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking test_step5 [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step5_complete_db_integration_validation.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ test_step5 build completed for $(PLATFORM)!$(NC)"

$(BIN_DIR)/test_step6: $(ALL_OBJECTS) $(OBJ_DIR)/test_step6_enhanced_pipeline.o | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking test_step6 (Enhanced Alarm+VirtualPoint Pipeline) [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step6_enhanced_pipeline.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ test_step6 (Enhanced Alarm+VirtualPoint Pipeline) build completed for $(PLATFORM)!$(NC)"

# Network API 테스트
$(BIN_DIR)/test_network: $(ALL_OBJECTS) $(OBJ_DIR)/test_network_api.o | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking test_network (REST API Server) [$(PLATFORM)]...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_network_api.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ test_network (REST API Server) build completed for $(PLATFORM)!$(NC)"

# =============================================================================
# 메인 타겟들 (WorkerManager 테스트 추가)
# =============================================================================

# WorkerManager 테스트 빌드/실행
workermanager-build: $(BIN_DIR)/test_workermanager

workermanager-test: workermanager-build
	@echo -e "$(MAGENTA)👷 Running WorkerManager Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)Testing WorkerManager singleton and worker lifecycle management$(NC)"
	@./$(BIN_DIR)/test_workermanager

# 플랫폼 호환성 테스트
platform-build: $(BIN_DIR)/test_platform

platform-test: platform-build
	@echo -e "$(MAGENTA)🛡️  Running Platform Compatibility Test [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)Testing cross-platform functionality$(NC)"
	@./$(BIN_DIR)/test_platform

# 개별 테스트 빌드
step3-build: $(BIN_DIR)/test_step3

step4-build: $(BIN_DIR)/test_step4

step5-build: $(BIN_DIR)/test_step5

step6-build: $(BIN_DIR)/test_step6

network-build: $(BIN_DIR)/test_network

# 개별 테스트 실행
step3-test: step3-build
	@echo -e "$(PURPLE)🚀 Running Step 3 (Workers) tests [$(PLATFORM)]...$(NC)"
	@./$(BIN_DIR)/test_step3

step4-test: step4-build
	@echo -e "$(PURPLE)🚀 Running Step 4 (Driver Data Validation) tests [$(PLATFORM)]...$(NC)"
	@./$(BIN_DIR)/test_step4

step5-test: step5-build
	@echo -e "$(PURPLE)🚀 Running Step 5 (Complete DB Integration) tests [$(PLATFORM)]...$(NC)"
	@./$(BIN_DIR)/test_step5

step6-test: step6-build
	@echo -e "$(PURPLE)🚀 Running Step 6 (Enhanced Alarm+VirtualPoint Pipeline) tests [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)This test requires Redis server running$(NC)"
	@echo -e "$(CYAN)Test data should be present in DB$(NC)"
	@./$(BIN_DIR)/test_step6

network-test: network-build
	@echo -e "$(PURPLE)🚀 Running Network API tests [$(PLATFORM)]...$(NC)"
	@echo -e "$(CYAN)This test starts REST API server on port 8080$(NC)"
	@./$(BIN_DIR)/test_network

# 모든 테스트 빌드 (WorkerManager 테스트 포함)
all-tests-build: $(BIN_DIR)/test_workermanager $(BIN_DIR)/test_platform $(BIN_DIR)/test_step3 $(BIN_DIR)/test_step4 $(BIN_DIR)/test_step5 $(BIN_DIR)/test_step6 $(BIN_DIR)/test_network
	@echo -e "$(GREEN)✅ All tests build completed for $(PLATFORM) (WorkerManager + Platform + Step 3-6 + Network)!$(NC)"

# 모든 테스트 실행 (WorkerManager 테스트 포함)
all-tests-run: all-tests-build
	@echo -e "$(BLUE)🚀 Running all tests in sequence on $(PLATFORM)...$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(MAGENTA)WorkerManager: Singleton and Lifecycle Test$(NC)"
	@./$(BIN_DIR)/test_workermanager
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(MAGENTA)Platform: Cross-Platform Compatibility Test$(NC)"
	@./$(BIN_DIR)/test_platform
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(PURPLE)Step 3: Workers Test$(NC)"
	@./$(BIN_DIR)/test_step3
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(PURPLE)Step 4: Driver Data Validation Test$(NC)"
	@./$(BIN_DIR)/test_step4
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(PURPLE)Step 5: Complete DB Integration Test$(NC)"
	@./$(BIN_DIR)/test_step5
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(PURPLE)Step 6: Enhanced Alarm+VirtualPoint Pipeline Test$(NC)"
	@./$(BIN_DIR)/test_step6
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo -e "$(PURPLE)Network: REST API Server Test$(NC)"
	@./$(BIN_DIR)/test_network
	@echo -e "$(GREEN)🎉 All tests completed successfully on $(PLATFORM)!$(NC)"

# 전체 클린 빌드
full-build: clean all-tests-build
	@echo -e "$(GREEN)✅ Full build completed for $(PLATFORM)!$(NC)"

# 빠른 테스트 실행 (WorkerManager + 플랫폼 테스트)
test: workermanager-test platform-test step3-test

# 클린
clean:
	@echo -e "$(YELLOW)🧹 Cleaning $(PLATFORM) build files...$(NC)"
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo -e "$(GREEN)✅ Clean completed for $(PLATFORM)$(NC)"

# 빌드 상태 확인 (WorkerManager 정보 포함)
status:
	@echo -e "$(BLUE)🏗️ Build Status for $(PLATFORM) (with WorkerManager + API)$(NC)"
	@echo -e "$(YELLOW)===============================================$(NC)"
	@echo "Platform: $(PLATFORM)"
	@echo "Windows: $(IS_WINDOWS), Linux: $(IS_LINUX)"
	@echo "Object files: $(words $(wildcard $(OBJ_DIR)/**/*.o)) / $(words $(ALL_OBJECTS))"
	@echo "Source files: $(words $(ALL_SOURCES))"
	@echo "Workers sources: $(words $(WORKERS_SOURCES)) (includes WorkerManager)"
	@echo "API sources: $(words $(API_SOURCES)) (includes DeviceApiCallbacks)"
	@echo "Network sources: $(words $(NETWORK_SOURCES))"
	@echo "Platform sources: $(words $(PLATFORM_SOURCES))"
	@echo ""
	@echo "WorkerManager binary: $(if $(wildcard $(BIN_DIR)/test_workermanager),✅ exists,❌ missing)"
	@echo "Platform binary: $(if $(wildcard $(BIN_DIR)/test_platform),✅ exists,❌ missing)"
	@echo "Step 3 binary: $(if $(wildcard $(BIN_DIR)/test_step3),✅ exists,❌ missing)"
	@echo "Step 4 binary: $(if $(wildcard $(BIN_DIR)/test_step4),✅ exists,❌ missing)"
	@echo "Step 5 binary: $(if $(wildcard $(BIN_DIR)/test_step5),✅ exists,❌ missing)"
	@echo "Step 6 binary: $(if $(wildcard $(BIN_DIR)/test_step6),✅ exists,❌ missing)"
	@echo "Network binary: $(if $(wildcard $(BIN_DIR)/test_network),✅ exists,❌ missing)"
	@echo ""
	@echo -e "$(BLUE)📚 Libraries:$(NC)"
	@echo "  Modbus: $(if $(MODBUS_LIBS),✅ available,❌ unavailable)"
	@echo "  MQTT: $(if $(MQTT_CPP_LIBS),✅ available,❌ unavailable)"
	@echo "  BACnet: $(if $(BACNET_LIBS),✅ available,❌ unavailable)"
	@echo "  Redis: $(if $(REDIS_LIBS),✅ available,❌ unavailable)"
	@echo "  QuickJS: $(if $(QUICKJS_LIBS),✅ available $(QUICKJS_LIBS),❌ unavailable)"
	@echo "  HTTP: $(if $(filter -DHAVE_HTTPLIB=1,$(CXXFLAGS)),✅ available,❌ unavailable)"
	@echo "  Platform libs: $(PLATFORM_LIBS)"

# 종속성 확인 (WorkerManager 정보 포함)
check-deps:
	@echo -e "$(BLUE)🔍 Library dependency check for $(PLATFORM) (WorkerManager + API)$(NC)"
	@echo -e "$(YELLOW)===============================================$(NC)"
	@echo "Platform: $(PLATFORM) (Windows: $(IS_WINDOWS), Linux: $(IS_LINUX))"
	@echo "Total sources: $(words $(ALL_SOURCES))"
	@echo "Workers sources: $(words $(WORKERS_SOURCES)) (includes WorkerManager.cpp)"
	@echo "API sources: $(words $(API_SOURCES)) (includes DeviceApiCallbacks.cpp)"
	@echo "Network sources: $(words $(NETWORK_SOURCES))"
	@echo "Platform sources: $(words $(PLATFORM_SOURCES))"
	@echo "Platform libraries: $(PLATFORM_LIBS)"
	@echo ""
	@echo -e "$(CYAN)Critical for WorkerManager + API:$(NC)"
	@echo "  HTTP support: $(if $(filter -DHAVE_HTTPLIB=1,$(CXXFLAGS)),✅ available - REST API enabled,❌ unavailable - REST API disabled)"
	@echo "  JSON support: $(if $(filter -DHAS_NLOHMANN_JSON,$(CXXFLAGS)),✅ available,❌ unavailable)"
	@echo "  Threading: ✅ available (pthread)"
	@echo ""
	@echo -e "$(CYAN)Optional Libraries:$(NC)"
	@echo "  MQTT: $(if $(MQTT_CPP_LIBS),✅ available $(MQTT_CPP_LIBS),❌ unavailable)"
	@echo "  Modbus: $(if $(MODBUS_LIBS),✅ available $(MODBUS_LIBS),❌ unavailable)"
	@echo "  BACnet: $(if $(BACNET_LIBS),✅ available $(BACNET_LIBS),❌ unavailable)"
	@echo "  Redis: $(if $(REDIS_LIBS),✅ available $(REDIS_LIBS),❌ unavailable)"
	@echo "  QuickJS: $(if $(QUICKJS_LIBS),✅ available $(QUICKJS_LIBS),❌ unavailable)"

# Windows 준비 확인
check-windows-ready:
	@echo -e "$(MAGENTA)🪟 Windows Compatibility Check$(NC)"
	@echo -e "$(YELLOW)==============================$(NC)"
	@echo "Platform detection: $(if $(filter Windows,$(PLATFORM)),✅ Windows detected,ℹ️  Running on $(PLATFORM))"
	@echo "PlatformCompat.h: $(if $(wildcard $(INCLUDE_DIR)/Platform/PlatformCompat.h),✅ exists,❌ missing - run 'mkdir -p ../include/Platform' and create file)"
	@echo "Platform includes: $(if $(findstring -I$(INCLUDE_DIR)/Platform,$(INCLUDES)),✅ included,❌ missing)"
	@echo "Windows flags: $(if $(filter -DPULSEONE_WINDOWS=1,$(CXXFLAGS)),✅ Windows build ready,ℹ️  Linux build)"
	@echo "Windows libs: $(if $(PLATFORM_LIBS),✅ $(PLATFORM_LIBS),ℹ️  Linux - no Windows libs needed)"
	@echo "WorkerManager ready: $(if $(wildcard $(SRC_DIR)/Workers/WorkerManager.cpp),✅ WorkerManager.cpp exists,❌ WorkerManager.cpp missing)"
	@echo "API ready: $(if $(wildcard $(SRC_DIR)/Api/DeviceApiCallbacks.cpp),✅ DeviceApiCallbacks.cpp exists,❌ DeviceApiCallbacks.cpp missing)"
	@echo ""
	@echo -e "$(GREEN)Status: $(if $(wildcard $(INCLUDE_DIR)/Platform/PlatformCompat.h),🎯 Ready for cross-platform compilation,⚠️  Need to create PlatformCompat.h)$(NC)"

# 도움말 (WorkerManager 테스트 추가)
help:
	@echo -e "$(BLUE)🚀 PulseOne Build System with WorkerManager + API Support$(NC)"
	@echo -e "$(YELLOW)======================================================$(NC)"
	@echo ""
	@echo -e "$(MAGENTA)🎯 New Features:$(NC)"
	@echo "  Current platform: $(PLATFORM)"
	@echo "  make workermanager-test  - Test WorkerManager singleton and lifecycle"
	@echo "  make workermanager-build - Build WorkerManager test only"
	@echo ""
	@echo -e "$(MAGENTA)🛡️ Platform Support:$(NC)"
	@echo "  make platform-test    - Test cross-platform compatibility"
	@echo "  make platform-build   - Build platform compatibility test"
	@echo "  make check-windows-ready - Check Windows compilation readiness"
	@echo ""
	@echo -e "$(GREEN)Individual test commands:$(NC)"
	@echo "  make step3-test   - Step 3: Workers test"
	@echo "  make step4-test   - Step 4: Driver data validation test"
	@echo "  make step5-test   - Step 5: Complete DB integration test"
	@echo "  make step6-test   - Step 6: Enhanced alarm+virtualpoint pipeline test"
	@echo "  make network-test - Network: REST API server test"
	@echo ""
	@echo -e "$(GREEN)Quick build commands:$(NC)"
	@echo "  make step3-build  - Build Step 3 only"
	@echo "  make step4-build  - Build Step 4 only"
	@echo "  make step5-build  - Build Step 5 only"
	@echo "  make step6-build  - Build Step 6 only"
	@echo "  make network-build - Build Network test only"
	@echo "  make all-tests-build - Build all tests (includes WorkerManager)"
	@echo ""
	@echo -e "$(GREEN)Integrated execution:$(NC)"
	@echo "  make all-tests-run - Run all tests in sequence (includes WorkerManager + platform)"
	@echo "  make full-build   - Complete clean build"
	@echo "  make test         - Quick test (WorkerManager + platform + step3)"
	@echo "  make clean        - Delete all build files"
	@echo ""
	@echo -e "$(GREEN)Status check:$(NC)"
	@echo "  make status       - Check build status (shows WorkerManager + API info)"
	@echo "  make check-deps   - Check library availability"
	@echo ""
	@echo -e "$(CYAN)💡 Next steps for full functionality:$(NC)"
	@echo "  1. Ensure httplib is available for REST API support"
	@echo "  2. Run 'make workermanager-test' to verify WorkerManager functionality"
	@echo "  3. Run 'make network-test' to test REST API with WorkerManager integration"
	@echo "  4. Create WorkerManager.h and WorkerManager.cpp if missing"

# 자동 종속성 생성
-include $(ALL_OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)

.PHONY: full-build test clean status check-deps help \
        step3-build step3-test step4-build step4-test step5-build step5-test \
        step6-build step6-test network-build network-test all-tests-build all-tests-run \
        platform-build platform-test check-windows-ready workermanager-build workermanager-test