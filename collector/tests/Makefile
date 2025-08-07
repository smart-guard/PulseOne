# =============================================================================
# collector/tests/Makefile - 🚀 고속 증분 컴파일 버전
# 모든 파일 포함하면서도 빠른 컴파일
# =============================================================================

# 🚀 성능 최적화 설정
NPROC := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += -j$(NPROC) --output-sync=target

# 🔧 라이브러리 체크 (캐시된 결과 사용)
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_C := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3c >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_CPP := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqttpp3 >/dev/null 2>&1 && echo "1" || echo "0")
HAS_BACNET_STACK := $(shell [ -f "/usr/local/lib/libbacnet.a" ] && echo "1" || echo "0")

# 컴파일러 설정 (🚀 최적화됨)
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -DPULSEONE_DEBUG_MODE \
           -O1 -fno-omit-frame-pointer \
           -Wno-unused-but-set-variable -Wno-unused-variable \
           -fdiagnostics-color=always

# 🚀 증분 컴파일용 디렉토리
OBJ_DIR = ./obj
BIN_DIR = ./bin
DATA_DIR = ./data

# 경로 설정
PARENT_DIR = ..
INCLUDE_DIR = $(PARENT_DIR)/include
SRC_DIR = $(PARENT_DIR)/src

# Include 경로
INCLUDES = -I$(INCLUDE_DIR)

# 🔧 라이브러리 설정 (기존과 동일)
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

# 전체 라이브러리 설정
BASIC_LIBS = -lpthread -lsqlite3
OPTIONAL_LIBS = $(MODBUS_LIBS) $(MYSQL_LIBS) $(PGSQL_LIBS) $(MQTT_C_LIBS) $(MQTT_CPP_LIBS) $(BACNET_LIBS) $(REDIS_LIBS)
LIBS = $(BASIC_LIBS) $(OPTIONAL_LIBS)
TEST_LIBS = -lgtest -lgtest_main -pthread

# =============================================================================
# 🚀 소스 파일 및 오브젝트 파일 정의 (모든 파일 포함)
# =============================================================================

# 모든 소스 파일 수집 (기존과 동일)
UTILS_SOURCES := $(wildcard $(SRC_DIR)/Utils/*.cpp)
CONFIG_SOURCES := $(wildcard $(SRC_DIR)/Config/*.cpp)
DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Entities/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)
CLIENT_SOURCES := $(wildcard $(SRC_DIR)/Client/*.cpp)
WORKERS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Protocol/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Components/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/WorkerFactory.cpp)

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

PIPELINE_SOURCES := $(wildcard $(SRC_DIR)/Pipeline/*.cpp)

# 🚀 전체 소스 파일 (모든 파일 포함)
ALL_SOURCES = $(UTILS_SOURCES) $(CONFIG_SOURCES) $(DATABASE_SOURCES) \
              $(CLIENT_SOURCES) $(WORKERS_SOURCES) $(DRIVERS_SOURCES) \
              $(PIPELINE_SOURCES)

# 🚀 오브젝트 파일 생성 (증분 컴파일용)
ALL_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(ALL_SOURCES))
TEST_OBJECTS = $(OBJ_DIR)/test_step3_workers.o

# 색상 정의
GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
PURPLE = \033[0;35m
NC = \033[0m

# =============================================================================
# 🚀 증분 컴파일 타겟들
# =============================================================================

.DEFAULT_GOAL := help

# 디렉토리 생성
$(OBJ_DIR):
	@echo -e "$(BLUE)📁 Creating object directories...$(NC)"
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/Utils $(OBJ_DIR)/Config $(OBJ_DIR)/Database/Entities $(OBJ_DIR)/Database/Repositories
	@mkdir -p $(OBJ_DIR)/Client $(OBJ_DIR)/Workers/Base $(OBJ_DIR)/Workers/Protocol $(OBJ_DIR)/Workers/Components
	@mkdir -p $(OBJ_DIR)/Drivers/Common $(OBJ_DIR)/Drivers/Modbus $(OBJ_DIR)/Drivers/Mqtt $(OBJ_DIR)/Drivers/Bacnet
	@mkdir -p $(OBJ_DIR)/Pipeline

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# 🚀 개별 오브젝트 파일 컴파일 (증분 컴파일)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Compiling $<$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 🚀 테스트 파일 컴파일
$(OBJ_DIR)/test_step3_workers.o: test_step3_workers.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Compiling test file...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 🚀 고속 링킹 (오브젝트 파일들만 링크)
$(BIN_DIR)/test_step3: $(ALL_OBJECTS) $(TEST_OBJECTS) | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking test_step3 ($(words $(ALL_OBJECTS)) objects)...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(TEST_OBJECTS) $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ Fast build completed!$(NC)"

# 🚀 메인 타겟들
fast-build: $(BIN_DIR)/test_step3
	@echo -e "$(GREEN)🚀 Fast incremental build completed!$(NC)"
	@echo -e "$(BLUE)📊 Compiled $(words $(ALL_OBJECTS)) object files$(NC)"

# 전체 클린 빌드 (처음 한 번만)
full-build: clean $(BIN_DIR)/test_step3
	@echo -e "$(GREEN)🏗️  Full build completed!$(NC)"

# 🚀 빠른 테스트 실행
test: fast-build
	@echo -e "$(PURPLE)🧪 Running tests...$(NC)"
	@./$(BIN_DIR)/test_step3

# 클린
clean:
	@echo -e "$(YELLOW)🧹 Cleaning...$(NC)"
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo -e "$(GREEN)✅ Clean completed$(NC)"

# 🔍 빌드 상태 확인
status:
	@echo -e "$(BLUE)📊 Build Status$(NC)"
	@echo -e "$(YELLOW)===============$(NC)"
	@echo "Object files: $(words $(wildcard $(OBJ_DIR)/**/*.o)) / $(words $(ALL_OBJECTS))"
	@echo "Source files: $(words $(ALL_SOURCES))"
	@echo "Binary: $(if $(wildcard $(BIN_DIR)/test_step3),✅ exists,❌ missing)"
	@echo ""
	@echo -e "$(BLUE)🔧 Libraries:$(NC)"
	@echo "  Modbus: $(if $(MODBUS_LIBS),✅,❌)"
	@echo "  MQTT: $(if $(MQTT_CPP_LIBS),✅,❌)"
	@echo "  BACnet: $(if $(BACNET_LIBS),✅,❌)"
	@echo "  Redis: $(if $(REDIS_LIBS),✅,❌)"

# 🚀 개발용 워크플로우
dev: fast-build test

# 종속성 확인 (기존과 동일)
check-deps:
	@echo -e "$(BLUE)🔍 라이브러리 의존성 체크$(NC)"
	@echo "  Total sources: $(words $(ALL_SOURCES))"
	@echo "  MQTT: $(if $(MQTT_CPP_LIBS),✅ $(MQTT_CPP_LIBS),❌ not found)"
	@echo "  Modbus: $(if $(MODBUS_LIBS),✅ $(MODBUS_LIBS),❌ not found)"
	@echo "  BACnet: $(if $(BACNET_LIBS),✅ $(BACNET_LIBS),❌ not found)"

# 도움말
help:
	@echo -e "$(BLUE)🚀 PulseOne Fast Build System$(NC)"
	@echo -e "$(YELLOW)==============================$(NC)"
	@echo ""
	@echo -e "$(GREEN)🚀 빠른 빌드 명령어:$(NC)"
	@echo "  make fast-build   - 🚀 증분 컴파일 (초고속)"
	@echo "  make test         - 🧪 빠른 빌드 + 테스트 실행"
	@echo "  make dev          - 🔄 개발 워크플로우 (build + test)"
	@echo ""
	@echo -e "$(GREEN)🏗️  전체 빌드:$(NC)"
	@echo "  make full-build   - 🏗️  전체 클린 빌드 (처음 한 번)"
	@echo "  make clean        - 🧹 모든 빌드 파일 삭제"
	@echo ""
	@echo -e "$(GREEN)📊 상태 확인:$(NC)"
	@echo "  make status       - 📊 빌드 상태 확인"
	@echo "  make check-deps   - 🔍 라이브러리 확인"
	@echo ""
	@echo -e "$(PURPLE)💡 사용법:$(NC)"
	@echo "  1️⃣  첫 빌드: make full-build"
	@echo "  2️⃣  이후 개발: make dev (매우 빠름!)"
	@echo "  3️⃣  코드 변경 후: make fast-build"

# 🚀 자동 종속성 생성 (고급 기능)
-include $(ALL_OBJECTS:.o=.d)

$(OBJ_DIR)/%.d: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔗 Generating dependencies for $<$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(OBJ_DIR)/$*.o $< > $@

.PHONY: fast-build full-build test dev clean status check-deps help