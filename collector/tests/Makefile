# =============================================================================
# collector/tests/Makefile - 🚀 고속 증분 컴파일 + 드라이버 데이터 검증 추가
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
BASIC_LIBS = -lpthread -lsqlite3 $(shell pkg-config --libs quickjs) -lquickjs -lm -ldl
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

# 🔥 테스트 파일들 정의
TEST_OBJECTS = $(OBJ_DIR)/test_step3_workers.o \
               $(OBJ_DIR)/test_step4_driver_data_validation.o \
               $(OBJ_DIR)/test_step5_complete_db_integration_validation.o \
               $(OBJ_DIR)/test_step6_full_pipeline.o

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

# 🚀 테스트 파일들 컴파일
$(OBJ_DIR)/test_step3_workers.o: test_step3_workers.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Compiling test_step3_workers...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step4_driver_data_validation.o: test_step4_driver_data_validation.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Compiling test_step4_driver_data_validation...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step5_complete_db_integration_validation.o: test_step5_complete_db_integration_validation.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Compiling test_step5_complete_db_integration_validation...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/test_step6_full_pipeline.o: test_step6_full_pipeline.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔨 Compiling test_step6_full_pipeline...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 🚀 테스트 실행 파일들 (개별)
# =============================================================================

# Step 3: Workers 테스트
$(BIN_DIR)/test_step3: $(ALL_OBJECTS) $(OBJ_DIR)/test_step3_workers.o | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking test_step3...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step3_workers.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ test_step3 build completed!$(NC)"

# Step 4: Driver 데이터 검증 테스트
$(BIN_DIR)/test_step4: $(ALL_OBJECTS) $(OBJ_DIR)/test_step4_driver_data_validation.o | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking test_step4 (Driver Data Validation)...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step4_driver_data_validation.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ test_step4 (Driver Data Validation) build completed!$(NC)"

# Step 5: DB 통합 검증 테스트
$(BIN_DIR)/test_step5: $(ALL_OBJECTS) $(OBJ_DIR)/test_step5_complete_db_integration_validation.o | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking test_step5...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step5_complete_db_integration_validation.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ test_step5 build completed!$(NC)"

# Step 6: Redis 파이프라인 테스트
$(BIN_DIR)/test_step6: $(ALL_OBJECTS) $(OBJ_DIR)/test_step6_full_pipeline.o | $(BIN_DIR)
	@echo -e "$(PURPLE)🔗 Linking test_step6 (Redis Pipeline)...$(NC)"
	@$(CXX) $(ALL_OBJECTS) $(OBJ_DIR)/test_step6_full_pipeline.o $(TEST_LIBS) $(LIBS) -o $@
	@echo -e "$(GREEN)✅ test_step6 (Redis Pipeline) build completed!$(NC)"

# =============================================================================
# 🚀 메인 타겟들
# =============================================================================

# 개별 테스트 빌드
step3-build: $(BIN_DIR)/test_step3
	@echo -e "$(GREEN)🚀 Step 3 (Workers) build completed!$(NC)"

step4-build: $(BIN_DIR)/test_step4
	@echo -e "$(GREEN)🚀 Step 4 (Driver Data Validation) build completed!$(NC)"

step5-build: $(BIN_DIR)/test_step5
	@echo -e "$(GREEN)🚀 Step 5 (Complete DB Integration) build completed!$(NC)"

step6-build: $(BIN_DIR)/test_step6
	@echo -e "$(GREEN)🚀 Step 6 (Redis Pipeline) build completed!$(NC)"

# 개별 테스트 실행
step3-test: step3-build
	@echo -e "$(PURPLE)🧪 Running Step 3 (Workers) tests...$(NC)"
	@./$(BIN_DIR)/test_step3

step4-test: step4-build
	@echo -e "$(PURPLE)🧪 Running Step 4 (Driver Data Validation) tests...$(NC)"
	@./$(BIN_DIR)/test_step4

step5-test: step5-build
	@echo -e "$(PURPLE)🧪 Running Step 5 (Complete DB Integration) tests...$(NC)"
	@./$(BIN_DIR)/test_step5

step6-test: step6-build
	@echo -e "$(PURPLE)🧪 Running Step 6 (Redis Pipeline) tests...$(NC)"
	@echo -e "$(CYAN)📊 이 테스트는 Redis 서버가 실행 중이어야 합니다$(NC)"
	@./$(BIN_DIR)/test_step6

# 🔥 모든 테스트 빌드
all-tests-build: $(BIN_DIR)/test_step3 $(BIN_DIR)/test_step4 $(BIN_DIR)/test_step5 $(BIN_DIR)/test_step6
	@echo -e "$(GREEN)🎉 All tests build completed (Step 3-6)!$(NC)"

# 🔥 모든 테스트 실행 (순서대로)
all-tests-run: all-tests-build
	@echo -e "$(BLUE)🚀 Running all tests in sequence (Step 3-6)...$(NC)"
	@echo -e "$(YELLOW)======================================$(NC)"
	@echo -e "$(PURPLE)🧪 Step 3: Workers Test$(NC)"
	@./$(BIN_DIR)/test_step3
	@echo -e "$(YELLOW)======================================$(NC)"
	@echo -e "$(PURPLE)🧪 Step 4: Driver Data Validation Test$(NC)"
	@./$(BIN_DIR)/test_step4
	@echo -e "$(YELLOW)======================================$(NC)"
	@echo -e "$(PURPLE)🧪 Step 5: Complete DB Integration Test$(NC)"
	@./$(BIN_DIR)/test_step5
	@echo -e "$(YELLOW)======================================$(NC)"
	@echo -e "$(PURPLE)🧪 Step 6: Redis Pipeline Test$(NC)"
	@./$(BIN_DIR)/test_step6
	@echo -e "$(GREEN)🎉 All tests completed (Step 3-6)!$(NC)"

# 전체 클린 빌드
full-build: clean all-tests-build
	@echo -e "$(GREEN)🏗️  Full build completed!$(NC)"

# 🚀 빠른 테스트 실행 (기존 호환)
test: step3-test

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
	@echo "Step 3 binary: $(if $(wildcard $(BIN_DIR)/test_step3),✅ exists,❌ missing)"
	@echo "Step 4 binary: $(if $(wildcard $(BIN_DIR)/test_step4),✅ exists,❌ missing)"
	@echo "Step 5 binary: $(if $(wildcard $(BIN_DIR)/test_step5),✅ exists,❌ missing)"
	@echo "Step 6 binary: $(if $(wildcard $(BIN_DIR)/test_step6),✅ exists,❌ missing)"
	@echo ""
	@echo -e "$(BLUE)🔧 Libraries:$(NC)"
	@echo "  Modbus: $(if $(MODBUS_LIBS),✅,❌)"
	@echo "  MQTT: $(if $(MQTT_CPP_LIBS),✅,❌)"
	@echo "  BACnet: $(if $(BACNET_LIBS),✅,❌)"
	@echo "  Redis: $(if $(REDIS_LIBS),✅,❌)"

# 🚀 개발용 워크플로우들
dev-step3: step3-build step3-test
dev-step4: step4-build step4-test
dev-step5: step5-build step5-test
dev-step6: step6-build step6-test
dev-all: all-tests-run

# 종속성 확인 (기존과 동일)
check-deps:
	@echo -e "$(BLUE)🔍 라이브러리 의존성 체크$(NC)"
	@echo "  Total sources: $(words $(ALL_SOURCES))"
	@echo "  MQTT: $(if $(MQTT_CPP_LIBS),✅ $(MQTT_CPP_LIBS),❌ not found)"
	@echo "  Modbus: $(if $(MODBUS_LIBS),✅ $(MODBUS_LIBS),❌ not found)"
	@echo "  BACnet: $(if $(BACNET_LIBS),✅ $(BACNET_LIBS),❌ not found)"
	@echo "  Redis: $(if $(REDIS_LIBS),✅ $(REDIS_LIBS),❌ not found)"

# 도움말
help:
	@echo -e "$(BLUE)🚀 PulseOne Fast Build System (with Step 6)$(NC)"
	@echo -e "$(YELLOW)============================================$(NC)"
	@echo ""
	@echo -e "$(GREEN)🔥 개별 테스트 명령어:$(NC)"
	@echo "  make step3-test   - 🧪 Step 3: Workers 테스트"
	@echo "  make step4-test   - 🧪 Step 4: Driver 데이터 검증 테스트"
	@echo "  make step5-test   - 🧪 Step 5: 완전한 DB 통합 테스트"
	@echo "  make step6-test   - 🧪 Step 6: Redis 파이프라인 테스트 (신규!)"
	@echo ""
	@echo -e "$(GREEN)🚀 빠른 빌드 명령어:$(NC)"
	@echo "  make step3-build  - 🚀 Step 3 빌드만"
	@echo "  make step4-build  - 🚀 Step 4 빌드만"
	@echo "  make step5-build  - 🚀 Step 5 빌드만"
	@echo "  make step6-build  - 🚀 Step 6 빌드만 (신규!)"
	@echo "  make all-tests-build - 🚀 모든 테스트 빌드 (Step 3-6)"
	@echo ""
	@echo -e "$(GREEN)🎯 통합 실행:$(NC)"
	@echo "  make all-tests-run - 🎉 모든 테스트 순서대로 실행 (Step 3-6)"
	@echo "  make dev-all      - 🔄 개발 워크플로우 (빌드 + 전체 테스트)"
	@echo ""
	@echo -e "$(GREEN)🏗️  전체 빌드:$(NC)"
	@echo "  make full-build   - 🏗️  전체 클린 빌드"
	@echo "  make clean        - 🧹 모든 빌드 파일 삭제"
	@echo ""
	@echo -e "$(GREEN)📊 상태 확인:$(NC)"
	@echo "  make status       - 📊 빌드 상태 확인"
	@echo "  make check-deps   - 🔍 라이브러리 확인"
	@echo ""
	@echo -e "$(PURPLE)💡 권장 사용법:$(NC)"
	@echo "  1️⃣  첫 빌드: make full-build"
	@echo "  2️⃣  개별 테스트: make step6-test (Redis 파이프라인)"
	@echo "  3️⃣  전체 검증: make all-tests-run"
	@echo "  4️⃣  개발 중: make dev-step6 (빠른 개발)"
	@echo ""
	@echo -e "$(RED)🔥 Step 6 신규 기능:$(NC)"
	@echo "  - Redis 연결 및 데이터 저장 테스트"
	@echo "  - 디바이스 스캔부터 Redis 저장까지 완전한 파이프라인"
	@echo "  - 성능 측정 및 데이터 무결성 검증"

# 🚀 자동 종속성 생성 (고급 기능)
-include $(ALL_OBJECTS:.o=.d)

$(OBJ_DIR)/%.d: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo -e "$(YELLOW)🔗 Generating dependencies for $<$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(OBJ_DIR)/$*.o $< > $@

.PHONY: fast-build full-build test clean status check-deps help \
        step3-build step3-test step4-build step4-test step5-build step5-test \
        step6-build step6-test all-tests-build all-tests-run \
        dev-step3 dev-step4 dev-step5 dev-step6 dev-all