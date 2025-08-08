# =============================================================================
# PulseOne Collector 메인 Makefile - 전체 프로젝트 빌드
# /app/collector/Makefile
# =============================================================================

# CPU 코어 수 자동 감지 및 병렬 처리 설정
NPROC := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += -j$(NPROC)

# 🔧 라이브러리 체크 (동적 감지) - MQTT 비동기 지원 추가
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_C := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3c >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_AS := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3as >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_CPP := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqttpp3 >/dev/null 2>&1 && echo "1" || echo "0")
HAS_BACNET_STACK := $(shell [ -f "/usr/local/lib/libbacnet.a" ] && echo "1" || echo "0")

# 컴파일러 설정
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -fPIC -Wno-unused-but-set-variable
LDFLAGS = 

# 경로 설정
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin
TARGET = pulseone-collector

# 🔥 핵심: 모든 필요한 헤더 경로 포함
INCLUDES = -I$(INCLUDE_DIR) \
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
# 라이브러리 설정 (조건부 링킹) - MQTT 섹션 수정
# =============================================================================

# 기본 라이브러리
BASIC_LIBS = -lpthread -lsqlite3

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

# 🔧 MQTT 라이브러리 (수정됨 - 비동기 SSL 지원 추가)
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

# 전체 라이브러리 설정
OPTIONAL_LIBS = $(MODBUS_LIBS) $(MYSQL_LIBS) $(PGSQL_LIBS) $(MQTT_LIBS) $(BACNET_LIBS) $(REDIS_LIBS)
LIBS = $(BASIC_LIBS) $(OPTIONAL_LIBS)

# 색상 정의
GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
PURPLE = \033[0;35m
NC = \033[0m

# =============================================================================
# 소스 파일 정의 (계층적 구조)
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

# 메인 소스
MAIN_SOURCE = $(SRC_DIR)/main.cpp

# 전체 소스 조합
ALL_LIB_SOURCES = $(CORE_SOURCES) $(UTILS_SOURCES) $(CONFIG_SOURCES) \
                  $(DATABASE_SOURCES) $(CLIENT_SOURCES) $(WORKERS_SOURCES) \
                  $(DRIVERS_SOURCES) $(PIPELINE_SOURCES) $(PLUGIN_SOURCES)

# 오브젝트 파일들
ALL_LIB_OBJECTS = $(ALL_LIB_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
MAIN_OBJECT = $(MAIN_SOURCE:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
ALL_OBJECTS = $(ALL_LIB_OBJECTS) $(MAIN_OBJECT)

# =============================================================================
# 메인 타겟들
# =============================================================================

.DEFAULT_GOAL := help

# 디렉토리 생성 (병렬 컴파일 안전)
$(BUILD_DIR):
	@echo -e "$(BLUE)📁 Creating build directories...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/Core
	@mkdir -p $(BUILD_DIR)/Workers/Base
	@mkdir -p $(BUILD_DIR)/Workers/Protocol  
	@mkdir -p $(BUILD_DIR)/Workers/Components
	@mkdir -p $(BUILD_DIR)/Drivers/Common
	@mkdir -p $(BUILD_DIR)/Drivers/Modbus
	@mkdir -p $(BUILD_DIR)/Drivers/Mqtt
	@mkdir -p $(BUILD_DIR)/Drivers/Bacnet
	@mkdir -p $(BUILD_DIR)/Database
	@mkdir -p $(BUILD_DIR)/Database/Entities
	@mkdir -p $(BUILD_DIR)/Database/Repositories
	@mkdir -p $(BUILD_DIR)/Client
	@mkdir -p $(BUILD_DIR)/Utils
	@mkdir -p $(BUILD_DIR)/Config
	@mkdir -p $(BUILD_DIR)/Pipeline
	@mkdir -p $(BUILD_DIR)/Plugin

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

directories: $(BUILD_DIR) $(BIN_DIR)

# 의존성 확인 (MQTT 정보 개선)
check-deps:
	@echo -e "$(BLUE)🔍 라이브러리 의존성 체크$(NC)"
	@echo -e "$(YELLOW)========================================$(NC)"
	@echo ""
	@echo -e "$(BLUE)📦 감지된 라이브러리들:$(NC)"
	@echo "  Modbus: $(if $(MODBUS_LIBS),✅ $(MODBUS_LIBS),❌ not found)"
	@echo "  MySQL: $(if $(MYSQL_LIBS),✅ found,❌ not found)"
	@echo "  PostgreSQL: $(if $(PGSQL_LIBS),✅ $(PGSQL_LIBS),❌ not found)"
	@echo "  MQTT: $(if $(MQTT_LIBS),✅ $(MQTT_LIBS),❌ not found)"
	@echo "  BACnet: $(if $(BACNET_LIBS),✅ $(BACNET_LIBS),❌ not found)"
	@echo "  Redis: $(if $(REDIS_LIBS),✅ $(REDIS_LIBS),❌ not found)"
	@echo ""
	@echo -e "$(BLUE)🔧 MQTT 상세 체크:$(NC)"
	@echo "  $(MQTT_STATUS)"
	@echo ""
	@echo -e "$(BLUE)🔧 컴파일 설정:$(NC)"
	@echo "  LIBS: $(LIBS)"
	@echo "  Total Sources: $(words $(ALL_LIB_SOURCES))"
	@echo "  CXXFLAGS: $(CXXFLAGS)"

# 🔧 MQTT 전용 체크 타겟 추가
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

# 소스 파일 확인
check-sources:
	@echo -e "$(BLUE)🔍 Checking required source files...$(NC)"
	@echo -e "$(YELLOW)Main source:$(NC)"
	@if [ -f "$(MAIN_SOURCE)" ]; then \
		echo -e "  ✅ $(MAIN_SOURCE)"; \
	else \
		echo -e "  ❌ $(MAIN_SOURCE) (MISSING)"; \
		exit 1; \
	fi
	@echo -e "$(YELLOW)Source counts:$(NC)"
	@echo "  Core: $(words $(CORE_SOURCES))"
	@echo "  Utils: $(words $(UTILS_SOURCES))"
	@echo "  Config: $(words $(CONFIG_SOURCES))"
	@echo "  Database: $(words $(DATABASE_SOURCES))"
	@echo "  Workers: $(words $(WORKERS_SOURCES))"
	@echo "  Drivers: $(words $(DRIVERS_SOURCES))"
	@echo "  Client: $(words $(CLIENT_SOURCES))"
	@echo "  Pipeline: $(words $(PIPELINE_SOURCES))"
	@echo "  Plugin: $(words $(PLUGIN_SOURCES))"

# 전체 빌드
all: check-deps check-sources directories $(TARGET)
	@echo -e "$(GREEN)🎉 Build completed successfully!$(NC)"
	@echo -e "$(BLUE)📦 Executable: $(BIN_DIR)/$(TARGET)$(NC)"
	@echo -e "$(BLUE)📊 Total object files: $(words $(ALL_OBJECTS))$(NC)"

# 메인 실행 파일
$(TARGET): $(ALL_OBJECTS) | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking $(TARGET)...$(NC)"
	@echo -e "$(YELLOW)   Objects: $(words $(ALL_OBJECTS))$(NC)"
	@echo -e "$(YELLOW)   Libraries: $(LIBS)$(NC)"
	@mkdir -p $(BIN_DIR)
	$(CXX) $(LDFLAGS) -o $(BIN_DIR)/$@ $(ALL_OBJECTS) $(LIBS)
	@echo -e "$(GREEN)✅ $(TARGET) built successfully!$(NC)"

# =============================================================================
# 오브젝트 파일 컴파일 규칙
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

# 메인 파일
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp | $(BUILD_DIR)
	@echo -e "$(BLUE)🚀 Compiling Main: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 유틸리티 타겟들
# =============================================================================

# 정리
clean:
	@echo -e "$(YELLOW)🧹 Cleaning build files...$(NC)"
	-rm -rf $(BUILD_DIR) $(BIN_DIR) 2>/dev/null || true
	@echo -e "$(GREEN)✅ Clean completed$(NC)"

# 실행
run: $(TARGET)
	@echo -e "$(BLUE)🚀 Running $(TARGET)...$(NC)"
	cd $(BIN_DIR) && ./$(TARGET)

# 빌드 모드별 타겟들
debug: CXXFLAGS += -g -DPULSEONE_DEBUG_MODE -O0
debug: CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
debug: check-deps clean all
	@echo -e "$(GREEN)🐛 Debug build completed - verbose logging enabled$(NC)"

release: CXXFLAGS += -O3 -DNDEBUG  
release: CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
release: clean all
	@echo -e "$(GREEN)🚀 Release build completed - optimized for performance$(NC)"

# 테스트 빌드 (라이브러리 없이)
test-build:
	@echo -e "$(BLUE)🧪 Testing build without optional libraries...$(NC)"
	$(MAKE) MODBUS_LIBS= MQTT_LIBS= BACNET_LIBS= REDIS_LIBS= all

# 진단
diagnose:
	@echo -e "$(BLUE)🏥 시스템 진단$(NC)"
	@echo -e "$(YELLOW)===============$(NC)"
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

# 도움말
help:
	@echo -e "$(BLUE)🔧 PulseOne Collector Build System$(NC)"
	@echo -e "$(YELLOW)===================================$(NC)"
	@echo ""
	@echo -e "$(GREEN)🚀 주요 명령어:$(NC)"
	@echo "  make all              - 전체 프로젝트 빌드"
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
	@echo "  make diagnose         - 전체 시스템 진단"
	@echo ""
	@echo -e "$(GREEN)🎯 일반적인 사용법:$(NC)"
	@echo "  make check-mqtt       # MQTT 라이브러리 확인"
	@echo "  make check-deps       # 모든 라이브러리 확인"
	@echo "  make clean && make    # 클린 빌드"
	@echo "  make debug            # 개발 시 디버그 빌드"
	@echo "  make run              # 빌드 후 바로 실행"

# PHONY 타겟들
.PHONY: all clean run debug release check-deps check-sources check-mqtt test-build diagnose directories help

# 자동 종속성 생성 (고급 기능)
-include $(ALL_OBJECTS:.o=.d)

$(BUILD_DIR)/%.d: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@echo -e "$(YELLOW)🔗 Generating dependencies for $<$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(BUILD_DIR)/$*.o $< > $@