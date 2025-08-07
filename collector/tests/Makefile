# =============================================================================
# collector/tests/Makefile - 에러 디버그 개선 버전
# 컴파일 에러를 정확히 보기 위한 수정
# =============================================================================

# 동일한 설정
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -DPULSEONE_DEBUG_MODE -O0
LDFLAGS = 

# 경로 설정
PARENT_DIR = ..
INCLUDE_DIR = $(PARENT_DIR)/include
SRC_DIR = $(PARENT_DIR)/src
BIN_DIR = ./bin
DATA_DIR = ./data

# Include 경로
INCLUDES = -I$(INCLUDE_DIR)

# 라이브러리 체크
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_BACNET_STACK := $(shell pkg-config --exists bacnet && echo "1" || echo "0")

# 플래그 적용
ifeq ($(HAS_NLOHMANN_JSON),1)
    CXXFLAGS += -DHAS_NLOHMANN_JSON
endif

ifeq ($(HAS_HIREDIS),1)
    CXXFLAGS += -DHAS_HIREDIS
endif

ifeq ($(HAS_BACNET_STACK), 1)
    CXXFLAGS += -DHAS_BACNET_STACK=1
else
    CXXFLAGS += -DHAS_BACNET_STACK=0
endif

# 🔥 수정: 라이브러리 단계별 적용 (에러 방지)
BASIC_LIBS = -lpthread -lsqlite3
OPTIONAL_LIBS = 

# hiredis 체크
ifeq ($(HAS_HIREDIS),1)
    OPTIONAL_LIBS += -lhiredis
endif

# 다른 라이브러리들도 선택적으로
ifneq ($(shell pkg-config --exists libmodbus 2>/dev/null; echo $$?),0)
    $(warning ⚠️  libmodbus not found - some features may be disabled)
else
    OPTIONAL_LIBS += $(shell pkg-config --libs libmodbus)
endif

# PostgreSQL (선택적)
ifneq ($(shell pkg-config --exists libpqxx 2>/dev/null; echo $$?),0)
    $(warning ⚠️  libpqxx not found - PostgreSQL features disabled)
else
    OPTIONAL_LIBS += -lpqxx -lpq
endif

# MQTT (선택적)
ifneq ($(shell pkg-config --exists paho-mqtt3c 2>/dev/null; echo $$?),0)
    $(warning ⚠️  paho-mqtt3c not found - MQTT features disabled)
else
    OPTIONAL_LIBS += -lpaho-mqtt3c
endif

# 🔥 수정: 단계별 라이브러리 적용
LIBS = $(BASIC_LIBS) $(OPTIONAL_LIBS)
TEST_LIBS = -lgtest -lgtest_main -pthread

# 색상
GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m

# =============================================================================
# 🔥 중요: 소스 파일 그룹 정의 (안전하게)
# =============================================================================

# 기본 필수 소스들
UTILS_SOURCES := $(SRC_DIR)/Utils/LogManager.cpp 
CONFIG_SOURCES := $(SRC_DIR)/Utils/ConfigManager.cpp

# LogLevelManager는 선택적으로 추가 (존재하는 경우만)
ifeq ($(shell test -f "$(SRC_DIR)/Utils/LogLevelManager.cpp" && echo "1" || echo "0"),1)
    UTILS_SOURCES += $(SRC_DIR)/Utils/LogLevelManager.cpp
endif

# 데이터베이스 소스들 (존재하는 것만)
DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Entities/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)

# 클라이언트 소스들
CLIENT_SOURCES := $(wildcard $(SRC_DIR)/Client/*.cpp)

# 🔥 Driver 소스들 (존재하는 것만 안전하게)
DRIVERS_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Common/*.cpp) \
                  $(wildcard $(SRC_DIR)/Drivers/Modbus/*.cpp) \
                  $(wildcard $(SRC_DIR)/Drivers/Mqtt/*.cpp) \
                  $(wildcard $(SRC_DIR)/Drivers/Bacnet/*.cpp)

# 🔥 Pipeline 소스들 (존재하는 것만 안전하게)
PIPELINE_SOURCES := $(wildcard $(SRC_DIR)/Pipeline/*.cpp)

# Workers 소스들 (존재하는 것만 안전하게)
WORKERS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Protocol/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Components/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/WorkerFactory.cpp)

# 🔥 수정: 단계별 소스 (점진적 확장)
STEP1_SOURCES = $(UTILS_SOURCES) $(CONFIG_SOURCES)
STEP2_SOURCES = $(STEP1_SOURCES) $(DATABASE_SOURCES) $(CLIENT_SOURCES)
STEP3_SOURCES = $(STEP2_SOURCES) $(WORKERS_SOURCES)
# Driver와 Pipeline은 필요한 경우에만 추가
ifneq ($(DRIVERS_SOURCES),)
    STEP3_SOURCES += $(DRIVERS_SOURCES)
endif
ifneq ($(PIPELINE_SOURCES),)
    STEP3_SOURCES += $(PIPELINE_SOURCES)
endif

# =============================================================================
# 🔥 디버그용 타겟 (컴파일 에러 확인)
# =============================================================================

# 디렉토리 생성
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Step3 디버그 컴파일 (상세 에러 메시지)
debug-step3: $(BIN_DIR)
	@echo -e "$(BLUE)🔍 Step3 디버그 컴파일 시작$(NC)"
	@echo -e "$(YELLOW)================================$(NC)"
	@echo ""
	@echo -e "$(BLUE)📋 소스 파일 체크:$(NC)"
	@echo "  UTILS_SOURCES: $(words $(UTILS_SOURCES))개"
	@echo "  CONFIG_SOURCES: $(words $(CONFIG_SOURCES))개"
	@echo "  DATABASE_SOURCES: $(words $(DATABASE_SOURCES))개"
	@echo "  CLIENT_SOURCES: $(words $(CLIENT_SOURCES))개"
	@echo "  WORKERS_SOURCES: $(words $(WORKERS_SOURCES))개"
	@echo "  DRIVERS_SOURCES: $(words $(DRIVERS_SOURCES))개"
	@echo "  PIPELINE_SOURCES: $(words $(PIPELINE_SOURCES))개"
	@echo "  총 STEP3_SOURCES: $(words $(STEP3_SOURCES))개"
	@echo ""
	@echo -e "$(BLUE)🔧 컴파일 플래그:$(NC)"
	@echo "  CXX: $(CXX)"
	@echo "  CXXFLAGS: $(CXXFLAGS)"
	@echo "  INCLUDES: $(INCLUDES)"
	@echo "  LIBS: $(LIBS)"
	@echo "  TEST_LIBS: $(TEST_LIBS)"
	@echo ""
	@if [ ! -f "test_step3_workers.cpp" ]; then \
		echo -e "$(RED)❌ test_step3_workers.cpp 파일이 없습니다!$(NC)"; \
		echo -e "$(YELLOW)   현재 디렉토리의 파일들:$(NC)"; \
		ls -la *.cpp 2>/dev/null || echo -e "$(RED)   .cpp 파일이 전혀 없음$(NC)"; \
		exit 1; \
	fi
	@echo -e "$(GREEN)✅ test_step3_workers.cpp 파일 존재$(NC)"
	@echo ""
	@echo -e "$(BLUE)🚀 컴파일 명령어:$(NC)"
	@echo "$(CXX) $(CXXFLAGS) $(INCLUDES) test_step3_workers.cpp $(STEP3_SOURCES) $(TEST_LIBS) $(LIBS) -o $(BIN_DIR)/test_step3"
	@echo ""
	@echo -e "$(BLUE)🔨 컴파일 시작... (에러 메시지 전체 표시)$(NC)"
	@echo -e "$(YELLOW)=========================================$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		test_step3_workers.cpp \
		$(STEP3_SOURCES) \
		$(TEST_LIBS) $(LIBS) -o $(BIN_DIR)/test_step3 || \
	(echo -e "$(RED)❌ 컴파일 실패!$(NC)" && \
	 echo -e "$(YELLOW)위의 에러 메시지를 확인하세요$(NC)" && \
	 exit 1)
	@echo -e "$(GREEN)✅ Step3 컴파일 성공!$(NC)"

# 🔥 Step3 초간단 버전 (최소한의 소스만)
step3-minimal: $(BIN_DIR)
	@echo -e "$(BLUE)🔧 Step3 최소 버전 컴파일$(NC)"
	@echo -e "$(YELLOW)========================$(NC)"
	@echo "최소 소스만 사용: UTILS + CONFIG + DATABASE"
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		test_step3_workers.cpp \
		$(UTILS_SOURCES) $(CONFIG_SOURCES) $(DATABASE_SOURCES) \
		$(TEST_LIBS) $(BASIC_LIBS) -o $(BIN_DIR)/test_step3_minimal || \
	(echo -e "$(RED)❌ 최소 버전도 실패!$(NC)" && exit 1)
	@echo -e "$(GREEN)✅ 최소 버전 컴파일 성공!$(NC)"

# 🔥 헤더 파일 체크
check-headers:
	@echo -e "$(BLUE)🔍 필수 헤더 파일 체크$(NC)"
	@echo -e "$(YELLOW)========================$(NC)"
	@echo -e "$(BLUE)Utils 헤더:$(NC)"
	@ls -la $(INCLUDE_DIR)/Utils/*.h 2>/dev/null || echo -e "$(RED)❌ Utils 헤더 없음$(NC)"
	@echo ""
	@echo -e "$(BLUE)Database 헤더:$(NC)"
	@ls -la $(INCLUDE_DIR)/Database/*.h 2>/dev/null || echo -e "$(RED)❌ Database 헤더 없음$(NC)"
	@echo ""
	@echo -e "$(BLUE)Workers 헤더:$(NC)"
	@ls -la $(INCLUDE_DIR)/Workers/*/*.h 2>/dev/null | head -5 || echo -e "$(RED)❌ Workers 헤더 없음$(NC)"

# 🔥 개별 소스 파일 컴파일 테스트
test-individual-sources:
	@echo -e "$(BLUE)🔧 개별 소스 컴파일 테스트$(NC)"
	@echo -e "$(YELLOW)============================$(NC)"
	@echo -e "$(BLUE)1. LogManager.cpp 컴파일 테스트$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $(SRC_DIR)/Utils/LogManager.cpp -o /tmp/test_logmanager.o || echo -e "$(RED)❌ LogManager 컴파일 실패$(NC)"
	@echo -e "$(BLUE)2. ConfigManager.cpp 컴파일 테스트$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $(SRC_DIR)/Utils/ConfigManager.cpp -o /tmp/test_configmanager.o || echo -e "$(RED)❌ ConfigManager 컴파일 실패$(NC)"
	@echo -e "$(BLUE)3. DatabaseManager.cpp 컴파일 테스트$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $(SRC_DIR)/Database/DatabaseManager.cpp -o /tmp/test_dbmanager.o 2>&1 || echo -e "$(RED)❌ DatabaseManager 컴파일 실패$(NC)"
	@echo -e "$(GREEN)✅ 개별 소스 컴파일 테스트 완료$(NC)"

# 🔥 test_step3_workers.cpp 파일 자체 컴파일 테스트
test-step3-file-only:
	@echo -e "$(BLUE)🔧 test_step3_workers.cpp 단독 컴파일 테스트$(NC)"
	@echo -e "$(YELLOW)===========================================$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c test_step3_workers.cpp -o /tmp/test_step3_workers.o 2>&1 || \
	(echo -e "$(RED)❌ test_step3_workers.cpp 컴파일 실패!$(NC)" && \
	 echo -e "$(YELLOW)헤더 include 문제 또는 문법 오류$(NC)" && \
	 exit 1)
	@echo -e "$(GREEN)✅ test_step3_workers.cpp 단독 컴파일 성공$(NC)"

# =============================================================================
# 기존 타겟들 (디버그 개선)
# =============================================================================

# 🔥 수정된 Step3 타겟
step3: $(BIN_DIR)/test_step3
	@echo -e "$(GREEN)✅ 3단계: DeviceWorker 생성 테스트 빌드 완료$(NC)"

$(BIN_DIR)/test_step3: test_step3_workers.cpp $(STEP3_SOURCES) | $(BIN_DIR)
	@echo -e "$(BLUE)🔧 Step3 컴파일: DeviceWorker 생성$(NC)"
	@echo -e "$(YELLOW)Sources: $(words $(STEP3_SOURCES))개 파일$(NC)"
	@if [ ! -f "test_step3_workers.cpp" ]; then \
		echo -e "$(RED)❌ test_step3_workers.cpp 파일이 필요합니다$(NC)"; \
		exit 1; \
	fi
	@echo -e "$(BLUE)컴파일 시작...$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		test_step3_workers.cpp \
		$(STEP3_SOURCES) \
		$(TEST_LIBS) $(LIBS) -o $@

# =============================================================================
# 진단 및 도움말
# =============================================================================

# 전체 시스템 진단
diagnose:
	@echo -e "$(BLUE)🏥 PulseOne 테스트 시스템 진단$(NC)"
	@echo -e "$(YELLOW)===============================$(NC)"
	@echo ""
	@echo -e "$(BLUE)1️⃣ 환경 체크:$(NC)"
	@which g++ || echo -e "$(RED)❌ g++ not found$(NC)"
	@which pkg-config || echo -e "$(RED)❌ pkg-config not found$(NC)"
	@echo ""
	@echo -e "$(BLUE)2️⃣ 디렉토리 구조:$(NC)"
	@ls -la $(PARENT_DIR) | grep -E "(include|src)" || echo -e "$(RED)❌ include/src 디렉토리 없음$(NC)"
	@echo ""
	@echo -e "$(BLUE)3️⃣ 테스트 파일들:$(NC)"
	@ls -la test_*.cpp 2>/dev/null || echo -e "$(RED)❌ 테스트 파일 없음$(NC)"
	@echo ""
	@echo -e "$(BLUE)4️⃣ 라이브러리 체크:$(NC)"
	@ldconfig -p | grep -E "(gtest|sqlite3)" || echo -e "$(YELLOW)⚠️  일부 라이브러리 없음$(NC)"

# 도움말 (디버그 명령어 추가)
help:
	@echo -e "$(BLUE)🔧 PulseOne Collector 테스트 시스템$(NC)"
	@echo -e "$(YELLOW)=======================================$(NC)"
	@echo ""
	@echo -e "$(GREEN)🐛 디버그 명령어들:$(NC)"
	@echo "  debug-step3           - Step3 상세 디버그 컴파일"
	@echo "  step3-minimal         - Step3 최소 버전 컴파일"
	@echo "  check-headers         - 필수 헤더 파일 체크"
	@echo "  test-individual-sources - 개별 소스 컴파일 테스트"
	@echo "  test-step3-file-only  - test_step3_workers.cpp만 컴파일 테스트"
	@echo "  diagnose              - 전체 시스템 진단"
	@echo ""
	@echo -e "$(GREEN)💡 권장 디버그 순서:$(NC)"
	@echo -e "  $(YELLOW)1. make diagnose$(NC)           # 전체 환경 체크"
	@echo -e "  $(YELLOW)2. make check-headers$(NC)      # 헤더 파일 확인"
	@echo -e "  $(YELLOW)3. make test-step3-file-only$(NC) # 테스트 파일 자체 문제 체크"
	@echo -e "  $(YELLOW)4. make debug-step3$(NC)        # 상세 컴파일 에러 확인"
	@echo -e "  $(YELLOW)5. make step3-minimal$(NC)      # 최소 버전으로 시도"

.DEFAULT_GOAL := help