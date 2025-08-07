# =============================================================================
# collector/tests/Makefile - MQTT 감지 수정 버전
# =============================================================================

# CPU 코어 수 자동 감지 및 병렬 처리 설정
NPROC := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += -j$(NPROC)

# 🔧 라이브러리 체크 (직접 링킹 테스트로 변경)
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")

# 🔥 MQTT 라이브러리 체크 (링킹 테스트로 변경)
HAS_MQTT_C := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3c >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_CPP := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqttpp3 >/dev/null 2>&1 && echo "1" || echo "0")

# 🔥 BACnet 체크 (직접 파일 확인으로 변경)
HAS_BACNET_STACK := $(shell [ -f "/usr/local/lib/libbacnet.a" ] && echo "1" || echo "0")

# 컴파일러 설정
CXX = g++
CXXFLAGS = -Wno-unused-but-set-variable -std=c++17 -Wall -Wextra -g -DPULSEONE_DEBUG_MODE -O0
LDFLAGS = 

# 경로 설정
PARENT_DIR = ..
INCLUDE_DIR = $(PARENT_DIR)/include
SRC_DIR = $(PARENT_DIR)/src
BIN_DIR = ./bin
DATA_DIR = ./data

# Include 경로
INCLUDES = -I$(INCLUDE_DIR)

# 🔧 라이브러리별 설정 (링킹 테스트 결과 기반)

# Modbus 라이브러리
ifneq ($(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lmodbus >/dev/null 2>&1; echo $$?),0)
    $(warning ⚠️  libmodbus not found - Modbus features will be disabled)
    MODBUS_LIBS = 
    MODBUS_INCLUDES = 
else
    MODBUS_LIBS = -lmodbus
    MODBUS_INCLUDES = $(shell pkg-config --cflags libmodbus 2>/dev/null || echo "")
    INCLUDES += $(MODBUS_INCLUDES)
    $(info ✅ Modbus found via linking test)
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

# 🔥 MQTT 라이브러리 설정 (수정됨)
ifeq ($(HAS_MQTT_C),1)
    MQTT_C_LIBS = -lpaho-mqtt3c
    $(info ✅ MQTT C found via linking test)
else
    $(warning ⚠️  paho-mqtt3c not found via linking test)
    MQTT_C_LIBS = 
endif

ifeq ($(HAS_MQTT_CPP),1)
    MQTT_CPP_LIBS = -lpaho-mqttpp3
    INCLUDES += -I/usr/local/include
    $(info ✅ MQTT C++ found via linking test)
else
    $(warning ⚠️  paho-mqttpp3 not found via linking test)
    MQTT_CPP_LIBS = 
endif

# PostgreSQL 라이브러리
ifneq ($(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpqxx >/dev/null 2>&1; echo $$?),0)
    $(warning ⚠️  libpqxx not found - PostgreSQL features will be disabled)
    PGSQL_LIBS = 
else
    PGSQL_LIBS = -lpqxx -lpq
    $(info ✅ PostgreSQL found via linking test)
endif

# BACnet 라이브러리 설정
ifeq ($(HAS_BACNET_STACK), 1)
    BACNET_LIBS = -lbacnet -lm
    BACNET_INCLUDES = -I/usr/local/include/bacnet
    INCLUDES += $(BACNET_INCLUDES)
    CXXFLAGS += -DHAS_BACNET_STACK=1
    $(info ✅ BACnet found: /usr/local/lib/libbacnet.a)
else
    CXXFLAGS += -DHAS_BACNET_STACK=0
    BACNET_LIBS = 
    $(warning ⚠️  BACnet library not found)
endif

# Redis 라이브러리 설정
ifeq ($(HAS_HIREDIS),1)
    REDIS_LIBS = -lhiredis -lhiredis_ssl
    CXXFLAGS += -DHAS_HIREDIS
    $(info ✅ Redis found via header test)
else
    $(warning ⚠️  hiredis not found - Redis functionality will be limited)
    REDIS_LIBS = 
endif

# JSON 라이브러리 플래그
ifeq ($(HAS_NLOHMANN_JSON),1)
    CXXFLAGS += -DHAS_NLOHMANN_JSON
    $(info ✅ nlohmann/json found)
endif

# 🔧 전체 라이브러리 설정
BASIC_LIBS = -lpthread -lsqlite3
OPTIONAL_LIBS = $(MODBUS_LIBS) $(MYSQL_LIBS) $(PGSQL_LIBS) $(MQTT_C_LIBS) $(MQTT_CPP_LIBS) $(BACNET_LIBS) $(REDIS_LIBS)
LIBS = $(BASIC_LIBS) $(OPTIONAL_LIBS)

# 테스트 라이브러리
TEST_LIBS = -lgtest -lgtest_main -pthread

# 색상 정의
GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m

# =============================================================================
# 소스 파일 그룹 정의
# =============================================================================

# 기본 필수 소스들
UTILS_SOURCES := $(wildcard $(SRC_DIR)/Utils/*.cpp)
CONFIG_SOURCES := $(wildcard $(SRC_DIR)/Config/*.cpp)

# Database 소스들
DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Entities/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)

# Client 소스들
CLIENT_SOURCES := $(wildcard $(SRC_DIR)/Client/*.cpp)

# Workers 소스들
WORKERS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Protocol/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Components/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/WorkerFactory.cpp)

# 🔧 Drivers 소스들 (라이브러리별 조건부 포함)
DRIVERS_SOURCES = $(wildcard $(SRC_DIR)/Drivers/Common/*.cpp)

# Modbus 드라이버
ifneq ($(MODBUS_LIBS),)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Modbus/*.cpp)
endif

# 🔥 MQTT 드라이버 (이제 정상 포함됨)
ifeq ($(HAS_MQTT_CPP),1)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Mqtt/*.cpp)
    $(info ✅ Including MQTT driver sources)
endif

# BACnet 드라이버
ifeq ($(HAS_BACNET_STACK),1)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Bacnet/*.cpp)
endif

# Pipeline 소스들
PIPELINE_SOURCES := $(wildcard $(SRC_DIR)/Pipeline/*.cpp)

# 단계별 소스 그룹
STEP1_SOURCES = $(UTILS_SOURCES) $(CONFIG_SOURCES)
STEP2_SOURCES = $(STEP1_SOURCES) $(DATABASE_SOURCES) $(CLIENT_SOURCES)
STEP3_SOURCES = $(STEP2_SOURCES) $(WORKERS_SOURCES) $(DRIVERS_SOURCES) $(PIPELINE_SOURCES)

# =============================================================================
# 메인 타겟들
# =============================================================================

.DEFAULT_GOAL := help

# 디렉토리 생성
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

directories: $(BIN_DIR)

clean:
	@echo -e "$(YELLOW)🧹 Cleaning test build files...$(NC)"
	rm -rf $(BIN_DIR)
	@echo -e "$(GREEN)✅ Clean completed$(NC)"

# 🔧 개선된 라이브러리 체크
check-deps:
	@echo -e "$(BLUE)🔍 라이브러리 의존성 체크$(NC)"
	@echo -e "$(YELLOW)========================================$(NC)"
	@echo ""
	@echo -e "$(BLUE)📦 감지된 라이브러리들:$(NC)"
	@echo "  Modbus: $(if $(MODBUS_LIBS),✅ $(MODBUS_LIBS),❌ not found)"
	@echo "  MySQL: $(if $(MYSQL_LIBS),✅ found,❌ not found)"
	@echo "  PostgreSQL: $(if $(PGSQL_LIBS),✅ $(PGSQL_LIBS),❌ not found)"
	@echo "  MQTT C: $(if $(MQTT_C_LIBS),✅ $(MQTT_C_LIBS),❌ not found)"
	@echo "  MQTT C++: $(if $(MQTT_CPP_LIBS),✅ $(MQTT_CPP_LIBS),❌ not found)"
	@echo "  BACnet: $(if $(BACNET_LIBS),✅ $(BACNET_LIBS),❌ not found)"
	@echo "  Redis: $(if $(REDIS_LIBS),✅ $(REDIS_LIBS),❌ not found)"
	@echo ""
	@echo -e "$(BLUE)🔧 컴파일 설정:$(NC)"
	@echo "  LIBS: $(LIBS)"
	@echo "  INCLUDES: $(INCLUDES)"
	@echo ""
	@echo -e "$(BLUE)📊 소스 파일 개수:$(NC)"
	@echo "  UTILS: $(words $(UTILS_SOURCES))"
	@echo "  DATABASE: $(words $(DATABASE_SOURCES))"
	@echo "  WORKERS: $(words $(WORKERS_SOURCES))"
	@echo "  DRIVERS: $(words $(DRIVERS_SOURCES))"
	@echo "  TOTAL STEP3: $(words $(STEP3_SOURCES))"

# 🔥 이제 MQTT 포함한 전체 빌드
debug-step3: $(BIN_DIR)
	@echo -e "$(BLUE)🔍 Step3 전체 기능 컴파일 (MQTT 포함)$(NC)"
	@echo -e "$(YELLOW)================================$(NC)"
	@echo ""
	@echo -e "$(BLUE)📋 포함된 기능들:$(NC)"
	@echo "  Modbus: $(if $(MODBUS_LIBS),✅ 포함,❌ 제외)"
	@echo "  MySQL: $(if $(MYSQL_LIBS),✅ 포함,❌ 제외)"
	@echo "  MQTT: $(if $(MQTT_CPP_LIBS),✅ 포함,❌ 제외)"
	@echo "  BACnet: $(if $(BACNET_LIBS),✅ 포함,❌ 제외)"
	@echo "  Redis: $(if $(REDIS_LIBS),✅ 포함,❌ 제외)"
	@echo ""
	@if [ ! -f "test_step3_workers.cpp" ]; then \
		echo -e "$(RED)❌ test_step3_workers.cpp 파일이 없습니다!$(NC)"; \
		exit 1; \
	fi
	@echo -e "$(BLUE)🔨 전체 기능 컴파일 시작...$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		test_step3_workers.cpp \
		$(STEP3_SOURCES) \
		$(TEST_LIBS) $(LIBS) -o $(BIN_DIR)/test_step3 || \
	(echo -e "$(RED)❌ 컴파일 실패!$(NC)" && exit 1)
	@echo -e "$(GREEN)✅ Step3 전체 기능 컴파일 성공! (MQTT 포함)$(NC)"

# 도움말
help:
	@echo -e "$(BLUE)🔧 PulseOne Collector 테스트 시스템$(NC)"
	@echo -e "$(YELLOW)=======================================$(NC)"
	@echo ""
	@echo -e "$(GREEN)🚀 이제 MQTT가 정상 감지됩니다!$(NC)"
	@echo ""
	@echo -e "$(GREEN)추천 명령어:$(NC)"
	@echo "  make check-deps       - 라이브러리 감지 상태 확인"
	@echo "  make clean && make debug-step3  - 전체 기능 컴파일 (MQTT 포함)"
	@echo ""
	@echo -e "$(GREEN)진단 명령어:$(NC)"
	@echo "  make diagnose         - 환경 진단"

# 진단
diagnose:
	@echo -e "$(BLUE)🏥 시스템 진단$(NC)"
	@echo -e "$(YELLOW)===============$(NC)"
	@echo ""
	@echo -e "$(BLUE)직접 링킹 테스트:$(NC)"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3c >/dev/null 2>&1 && echo "  ✅ MQTT C 링크 성공" || echo "  ❌ MQTT C 링크 실패"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqttpp3 >/dev/null 2>&1 && echo "  ✅ MQTT C++ 링크 성공" || echo "  ❌ MQTT C++ 링크 실패"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - -lmodbus >/dev/null 2>&1 && echo "  ✅ Modbus 링크 성공" || echo "  ❌ Modbus 링크 실패"
	@echo "int main(){return 0;}" | $(CXX) -x c++ - -lhiredis >/dev/null 2>&1 && echo "  ✅ Redis 링크 성공" || echo "  ❌ Redis 링크 실패"

.PHONY: all clean help diagnose check-deps debug-step3 directories