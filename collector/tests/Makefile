# =============================================================================
# collector/tests/Makefile - Step3 완전 수정 버전
# 중복 제거 + 누락 소스 추가 + 링킹 에러 해결
# =============================================================================

# 원본 Makefile과 동일한 설정 사용
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -DPULSEONE_DEBUG_MODE -O0
LDFLAGS = 

# 경로 설정 (tests 폴더 기준)
PARENT_DIR = ..
INCLUDE_DIR = $(PARENT_DIR)/include
SRC_DIR = $(PARENT_DIR)/src
BIN_DIR = ./bin
DATA_DIR = ./data

# Include 경로 (원본과 동일)
INCLUDES = -I$(INCLUDE_DIR)

# =============================================================================
# 원본 Makefile의 라이브러리 체크 그대로 사용
# =============================================================================

HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_BACNET_STACK := $(shell pkg-config --exists bacnet && echo "1" || echo "0")

# 원본 Makefile의 플래그 적용
ifeq ($(HAS_NLOHMANN_JSON),1)
    CXXFLAGS += -DHAS_NLOHMANN_JSON
endif

ifeq ($(HAS_HIREDIS),1)
    CXXFLAGS += -DHAS_HIREDIS
else
    $(warning ⚠️  hiredis not found - Redis functionality will be limited)
    $(warning    Install with: sudo apt-get install libhiredis-dev)
endif

ifeq ($(HAS_BACNET_STACK), 1)
    CXXFLAGS += -DHAS_BACNET_STACK=1
else
    CXXFLAGS += -DHAS_BACNET_STACK=0
endif

# 원본 Makefile의 라이브러리들 (테스트용으로 필요한 것만)
LIBS = -lpthread \
       $(shell pkg-config --libs libmodbus 2>/dev/null || echo "-lmodbus") \
       -lpqxx -lpq -lsqlite3 \
       -lpaho-mqtt3c -lpaho-mqttpp3 \
       -lbacnet -lmysqlclient \
       -lhiredis -lhiredis_ssl

# 테스트 전용 라이브러리
TEST_LIBS = -lgtest -lgtest_main -pthread

# 원본 Makefile의 색상 정의 그대로 사용
GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m

# =============================================================================
# 🔧 수정: 소스 파일 그룹 정의 (중복 제거 + 누락 추가)
# =============================================================================

# 기본 유틸리티 소스들 (중복 없이 한 번만 정의)
UTILS_SOURCES := $(SRC_DIR)/Utils/LogManager.cpp $(SRC_DIR)/Utils/LogLevelManager.cpp
CONFIG_SOURCES := $(SRC_DIR)/Utils/ConfigManager.cpp

# 데이터베이스 관련 소스들
DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Entities/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)

# 클라이언트 소스들
CLIENT_SOURCES := $(wildcard $(SRC_DIR)/Client/*.cpp)

# 🔧 추가: Driver 소스들 (누락되었던 부분)
DRIVERS_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Common/*.cpp) \
                  $(wildcard $(SRC_DIR)/Drivers/Modbus/*.cpp) \
                  $(wildcard $(SRC_DIR)/Drivers/Mqtt/*.cpp) \
                  $(wildcard $(SRC_DIR)/Drivers/Bacnet/*.cpp)

# 🔧 추가: Pipeline 소스들 (누락되었던 부분)
PIPELINE_SOURCES := $(wildcard $(SRC_DIR)/Pipeline/*.cpp)

# Workers 소스들
WORKERS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Protocol/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Components/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/WorkerFactory.cpp)

# 🔧 수정: 테스트 단계별 필요 소스들 (중복 제거)
STEP1_SOURCES = $(UTILS_SOURCES) $(CONFIG_SOURCES)
STEP2_SOURCES = $(STEP1_SOURCES) $(DATABASE_SOURCES) $(CLIENT_SOURCES)
STEP3_SOURCES = $(STEP2_SOURCES) $(WORKERS_SOURCES) $(DRIVERS_SOURCES) $(PIPELINE_SOURCES)
STEP4_SOURCES = $(STEP3_SOURCES)
STEP5_SOURCES = $(STEP4_SOURCES)

# =============================================================================
# 메인 타겟 정의
# =============================================================================

.PHONY: all clean clean-all clean-testdb step1 step2 step3 step4 step5 check-deps setup-testdb check-testdb reset-testdb

# 기본 타겟
all: check-deps setup-testdb step1
	@echo -e "$(GREEN)🎉 테스트 빌드 완료!$(NC)"

# =============================================================================
# 의존성 및 환경 체크 (원본 Makefile 스타일)
# =============================================================================

check-deps:
	@echo -e "$(BLUE)🔍 PulseOne Collector Dependencies Check$(NC)"
	@echo -e "$(YELLOW)===============================================$(NC)"
	@echo -e "$(BLUE)📋 컴파일러 정보:$(NC)"
	@echo "  CXX: $(CXX)"
	@$(CXX) --version | head -1
	@echo ""
	@echo -e "$(BLUE)📚 필수 라이브러리 체크:$(NC)"
	@if [ "$(HAS_NLOHMANN_JSON)" = "1" ]; then \
		echo -e "  $(GREEN)✅ nlohmann/json$(NC)"; \
	else \
		echo -e "  $(YELLOW)⚠️  nlohmann/json (optional)$(NC)"; \
	fi
	@if [ "$(HAS_HIREDIS)" = "1" ]; then \
		echo -e "  $(GREEN)✅ hiredis$(NC)"; \
	else \
		echo -e "  $(RED)❌ hiredis$(NC)"; \
		echo -e "     Install: sudo apt-get install libhiredis-dev"; \
	fi
	@pkg-config --exists libmodbus 2>/dev/null && echo -e "  $(GREEN)✅ libmodbus: $$(pkg-config --modversion libmodbus)$(NC)" || echo -e "  $(RED)❌ libmodbus$(NC)"
	@pkg-config --exists paho-mqtt3c 2>/dev/null && echo -e "  $(GREEN)✅ paho-mqtt3c$(NC)" || echo -e "  $(YELLOW)⚠️  paho-mqtt3c$(NC)"
	@pkg-config --exists bacnet 2>/dev/null && echo -e "  $(GREEN)✅ bacnet$(NC)" || echo -e "  $(YELLOW)⚠️  bacnet$(NC)"
	@echo ""
	@echo -e "$(BLUE)🧪 테스트 라이브러리 체크:$(NC)"
	@ldconfig -p | grep -q "libgtest" && echo -e "  $(GREEN)✅ Google Test$(NC)" || echo -e "  $(YELLOW)⚠️  Google Test (install: sudo apt-get install libgtest-dev)$(NC)"
	@ldconfig -p | grep -q "libsqlite3" && echo -e "  $(GREEN)✅ SQLite3$(NC)" || echo -e "  $(RED)❌ SQLite3$(NC)"

# 🔧 추가: 소스 파일 상세 체크
check-sources:
	@echo -e "$(BLUE)🔍 소스 파일 체크$(NC)"
	@echo -e "$(YELLOW)Utils sources: $(words $(UTILS_SOURCES))개$(NC)"
	@echo -e "$(YELLOW)Config sources: $(words $(CONFIG_SOURCES))개$(NC)"
	@echo -e "$(YELLOW)Database sources: $(words $(DATABASE_SOURCES))개$(NC)"
	@echo -e "$(YELLOW)Client sources: $(words $(CLIENT_SOURCES))개$(NC)"
	@echo -e "$(YELLOW)Workers sources: $(words $(WORKERS_SOURCES))개$(NC)"
	@echo -e "$(YELLOW)Drivers sources: $(words $(DRIVERS_SOURCES))개$(NC)"
	@echo -e "$(YELLOW)Pipeline sources: $(words $(PIPELINE_SOURCES))개$(NC)"

# 디렉토리 생성
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(DATA_DIR):
	@mkdir -p $(DATA_DIR)

directories: $(BIN_DIR) $(DATA_DIR)

# =============================================================================
# 테스트 DB 관리 (SQLite 기반)
# =============================================================================

# 테스트 DB 생성 (기존 코드 활용)
setup-testdb: $(BIN_DIR)
	@echo -e "$(BLUE)🗄️  실제 테스트 환경 구성$(NC)"
	@echo -e "$(YELLOW)============================$(NC)"
	@echo -e "$(BLUE)📁 디렉토리 생성 중...$(NC)"
	@mkdir -p db logs backup config
	@if [ -f "db/pulseone_test.db" ]; then \
		echo -e "$(YELLOW)⚠️  기존 DB 발견 - 백업 생성$(NC)"; \
		mv db/pulseone_test.db backup/pulseone_test_backup_$$(date +%Y%m%d_%H%M%S).db; \
	fi
	@echo -e "$(BLUE)📋 테스트 DB 생성: db/pulseone_test.db$(NC)"
	@if [ ! -f "setup_test_db.sql" ]; then \
		echo -e "$(RED)❌ setup_test_db.sql 파일이 없습니다!$(NC)"; \
		echo -e "$(YELLOW)   './setup_test_environment.sh'를 먼저 실행하거나$(NC)"; \
		echo -e "$(YELLOW)   artifacts에서 SQL 파일을 저장해주세요.$(NC)"; \
		exit 1; \
	fi
	@sqlite3 db/pulseone_test.db < setup_test_db.sql
	@echo -e "$(GREEN)✅ 테스트 DB 생성 완료: db/pulseone_test.db$(NC)"
	@echo -e "$(BLUE)📊 ConfigManager가 이 DB를 사용하도록 설정됨$(NC)"

# 테스트 DB 상태 확인
check-testdb:
	@echo -e "$(BLUE)🔍 테스트 환경 상태 확인$(NC)"
	@echo -e "$(YELLOW)========================$(NC)"
	@echo -e "$(BLUE)📁 디렉토리 구조:$(NC)"
	@ls -la . | grep -E "(config|db|logs|backup)" || echo -e "$(YELLOW)   일부 디렉토리 없음$(NC)"
	@echo ""
	@echo -e "$(BLUE)⚙️  설정 파일들:$(NC)"
	@if [ -f "config/.env" ]; then \
		echo -e "   $(GREEN)✅ config/.env$(NC)"; \
	else \
		echo -e "   $(RED)❌ config/.env$(NC)"; \
	fi
	@if [ -f "config/database.env" ]; then \
		echo -e "   $(GREEN)✅ config/database.env$(NC)"; \
	else \
		echo -e "   $(RED)❌ config/database.env$(NC)"; \
	fi
	@if [ -f "config/redis.env" ]; then \
		echo -e "   $(GREEN)✅ config/redis.env$(NC)"; \
	else \
		echo -e "   $(RED)❌ config/redis.env$(NC)"; \
	fi
	@echo ""
	@if [ ! -f "db/pulseone_test.db" ]; then \
		echo -e "$(RED)❌ 테스트 DB가 없습니다: db/pulseone_test.db$(NC)"; \
		echo -e "$(YELLOW)   'make setup-testdb' 또는 './setup_test_environment.sh'를 실행하세요$(NC)"; \
		exit 1; \
	fi
	@echo -e "$(GREEN)✅ 테스트 DB 파일: db/pulseone_test.db$(NC)"
	@echo -e "$(BLUE)📊 DB 테이블 및 데이터:$(NC)"
	@sqlite3 db/pulseone_test.db "SELECT printf('   %-15s: %d개', 'tenants', COUNT(*)) FROM tenants UNION ALL SELECT printf('   %-15s: %d개', 'sites', COUNT(*)) FROM sites UNION ALL SELECT printf('   %-15s: %d개', 'devices', COUNT(*)) FROM devices UNION ALL SELECT printf('   %-15s: %d개', 'data_points', COUNT(*)) FROM data_points;"

# 테스트 DB 초기화
reset-testdb: setup-testdb
	@echo -e "$(GREEN)✅ 테스트 DB 초기화 완료$(NC)"

# 테스트 DB 정리
clean-testdb:
	@echo -e "$(YELLOW)🧹 테스트 DB 정리$(NC)"
	@rm -f $(BIN_DIR)/test.db $(BIN_DIR)/test.db.backup.*
	@echo -e "$(GREEN)✅ 테스트 DB 정리 완료$(NC)"

# =============================================================================
# 🔧 수정: 5단계 테스트들 (중복 제거된 소스 사용)
# =============================================================================

# 1단계: ✅ 설정파일 읽기
step1: $(BIN_DIR)/test_step1
	@echo -e "$(GREEN)✅ 1단계: 설정파일 읽기 테스트 빌드 완료$(NC)"

$(BIN_DIR)/test_step1: test_step1_config.cpp $(STEP1_SOURCES) | $(BIN_DIR)
	@echo -e "$(BLUE)🔧 Step1 컴파일: 설정파일 읽기$(NC)"
	@echo -e "$(YELLOW)   Sources: $(words $(STEP1_SOURCES))개 파일$(NC)"
	@if [ ! -f "test_step1_config.cpp" ]; then \
		echo -e "$(RED)❌ test_step1_config.cpp 파일이 필요합니다$(NC)"; \
		exit 1; \
	fi
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		test_step1_config.cpp $(STEP1_SOURCES) \
		$(TEST_LIBS) $(LIBS) -o $@

# 2단계: ✅ DB 접근하여 테이블들 읽기
step2: $(BIN_DIR)/test_step2
	@echo -e "$(GREEN)✅ 2단계: DB 접근 테스트 빌드 완료$(NC)"

$(BIN_DIR)/test_step2: test_step2_database.cpp $(STEP2_SOURCES) | $(BIN_DIR)
	@echo -e "$(BLUE)🔧 Step2 컴파일: DB 접근$(NC)"
	@echo -e "$(YELLOW)   Sources: $(words $(STEP2_SOURCES))개 파일$(NC)"
	@if [ ! -f "test_step2_database.cpp" ]; then \
		echo -e "$(RED)❌ test_step2_database.cpp 파일이 필요합니다$(NC)"; \
		exit 1; \
	fi
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		test_step2_database.cpp $(STEP2_SOURCES) \
		$(TEST_LIBS) $(LIBS) -o $@

# 🔧 수정: 3단계 - 중복 제거 및 누락 소스 추가
step3: $(BIN_DIR)/test_step3
	@echo -e "$(GREEN)✅ 3단계: DeviceWorker 생성 테스트 빌드 완료$(NC)"

$(BIN_DIR)/test_step3: test_step3_workers.cpp $(STEP3_SOURCES) | $(BIN_DIR)
	@echo -e "$(BLUE)🔧 Step3 컴파일: DeviceWorker 생성 (완전 수정)$(NC)"
	@echo -e "$(YELLOW)   Sources: $(words $(STEP3_SOURCES))개 파일$(NC)"
	@echo -e "$(YELLOW)   Including: Utils($(words $(UTILS_SOURCES))), Config($(words $(CONFIG_SOURCES))), Database($(words $(DATABASE_SOURCES))), Workers($(words $(WORKERS_SOURCES))), Drivers($(words $(DRIVERS_SOURCES))), Pipeline($(words $(PIPELINE_SOURCES)))$(NC)"
	@if [ ! -f "test_step3_workers.cpp" ]; then \
		echo -e "$(RED)❌ test_step3_workers.cpp 파일이 필요합니다$(NC)"; \
		exit 1; \
	fi
	@# 중복 제거된 소스로 컴파일
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		test_step3_workers.cpp \
		$(STEP3_SOURCES) \
		$(TEST_LIBS) $(LIBS) -o $@

# 4단계: ✅ 드라이버 초기화 시뮬레이션
step4: $(BIN_DIR)/test_step4
	@echo -e "$(GREEN)✅ 4단계: 드라이버 초기화 테스트 빌드 완료$(NC)"

$(BIN_DIR)/test_step4: test_step4_drivers.cpp $(STEP4_SOURCES) | $(BIN_DIR)
	@echo -e "$(BLUE)🔧 Step4 컴파일: 드라이버 초기화$(NC)"
	@echo -e "$(YELLOW)   Sources: $(words $(STEP4_SOURCES))개 파일$(NC)"
	@if [ ! -f "test_step4_drivers.cpp" ]; then \
		echo -e "$(RED)❌ test_step4_drivers.cpp 파일이 필요합니다$(NC)"; \
		exit 1; \
	fi
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		test_step4_drivers.cpp $(STEP4_SOURCES) \
		$(TEST_LIBS) $(LIBS) -o $@

# 5단계: ✅ 데이터 Redis 저장 시뮬레이션
step5: $(BIN_DIR)/test_step5
	@echo -e "$(GREEN)✅ 5단계: Redis 저장 테스트 빌드 완료$(NC)"

$(BIN_DIR)/test_step5: test_step5_redis.cpp $(STEP5_SOURCES) | $(BIN_DIR)
	@echo -e "$(BLUE)🔧 Step5 컴파일: Redis 저장$(NC)"
	@echo -e "$(YELLOW)   Sources: $(words $(STEP5_SOURCES))개 파일$(NC)"
	@if [ ! -f "test_step5_redis.cpp" ]; then \
		echo -e "$(RED)❌ test_step5_redis.cpp 파일이 필요합니다$(NC)"; \
		exit 1; \
	fi
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		test_step5_redis.cpp $(STEP5_SOURCES) \
		$(TEST_LIBS) $(LIBS) -o $@

# =============================================================================
# 실행 타겟들 (원본 Makefile의 run 스타일)
# =============================================================================

run-step1: step1 check-testdb
	@echo -e "$(BLUE)🚀 1단계 실행: 설정파일 읽기$(NC)"
	@echo -e "$(YELLOW)================================$(NC)"
	cd $(BIN_DIR) && ./test_step1

run-step2: step2 check-testdb
	@echo -e "$(BLUE)🚀 2단계 실행: DB 접근$(NC)"
	@echo -e "$(YELLOW)========================$(NC)"
	cd $(BIN_DIR) && ./test_step2

run-step3: step3 check-testdb
	@echo -e "$(BLUE)🚀 3단계 실행: DeviceWorker 생성 (완전 수정)$(NC)"
	@echo -e "$(YELLOW)===========================================$(NC)"
	@echo -e "$(GREEN)✅ 중복 제거 + 모든 Driver 및 Pipeline 소스 포함됨$(NC)"
	cd $(BIN_DIR) && ./test_step3

run-step4: step4 check-testdb
	@echo -e "$(BLUE)🚀 4단계 실행: 드라이버 초기화$(NC)"
	@echo -e "$(YELLOW)==============================$(NC)"
	cd $(BIN_DIR) && ./test_step4

run-step5: step5 check-testdb
	@echo -e "$(BLUE)🚀 5단계 실행: Redis 저장$(NC)"
	@echo -e "$(YELLOW)==========================$(NC)"
	cd $(BIN_DIR) && ./test_step5

# 전체 5단계 플로우 실행
run-all: step1 step2 step3 step4 step5 check-testdb
	@echo -e "$(BLUE)🚀 PulseOne Collector 전체 테스트 플로우$(NC)"
	@echo -e "$(YELLOW)=========================================$(NC)"
	@echo -e "$(GREEN)1단계: 설정파일 읽기$(NC)"
	cd $(BIN_DIR) && ./test_step1 && echo ""
	@echo -e "$(GREEN)2단계: DB 접근하여 테이블들 읽기$(NC)"
	cd $(BIN_DIR) && ./test_step2 && echo ""
	@echo -e "$(GREEN)3단계: 프로토콜별 DeviceWorker 생성$(NC)"
	cd $(BIN_DIR) && ./test_step3 && echo ""
	@echo -e "$(GREEN)4단계: 드라이버 초기화 시뮬레이션$(NC)"
	cd $(BIN_DIR) && ./test_step4 && echo ""
	@echo -e "$(GREEN)5단계: 데이터 Redis 저장 시뮬레이션$(NC)"
	cd $(BIN_DIR) && ./test_step5 && echo ""
	@echo -e "$(GREEN)🎉 전체 5단계 테스트 플로우 완료!$(NC)"

# =============================================================================
# 🔧 추가: 디버그 및 진단 도구들
# =============================================================================

# 디버그: Step3에서 사용되는 소스 확인
debug-step3-sources:
	@echo -e "$(BLUE)🔍 Step3 소스 파일 분석$(NC)"
	@echo -e "$(YELLOW)========================$(NC)"
	@echo -e "$(BLUE)Total STEP3_SOURCES: $(words $(STEP3_SOURCES))개$(NC)"
	@echo ""
	@echo -e "$(BLUE)UTILS_SOURCES ($(words $(UTILS_SOURCES))개):$(NC)"
	@echo "$(UTILS_SOURCES)" | tr ' ' '\n'
	@echo ""
	@echo -e "$(BLUE)CONFIG_SOURCES ($(words $(CONFIG_SOURCES))개):$(NC)"
	@echo "$(CONFIG_SOURCES)" | tr ' ' '\n'
	@echo ""
	@echo -e "$(BLUE)DRIVERS_SOURCES ($(words $(DRIVERS_SOURCES))개):$(NC)"
	@echo "$(DRIVERS_SOURCES)" | tr ' ' '\n'
	@echo ""
	@echo -e "$(BLUE)PIPELINE_SOURCES ($(words $(PIPELINE_SOURCES))개):$(NC)"
	@echo "$(PIPELINE_SOURCES)" | tr ' ' '\n'
	@echo ""
	@echo -e "$(BLUE)WORKERS_SOURCES ($(words $(WORKERS_SOURCES))개):$(NC)"
	@echo "$(WORKERS_SOURCES)" | tr ' ' '\n' | head -10

# 실제 파일 존재 여부 체크
check-step3-files:
	@echo -e "$(BLUE)🔍 Step3 필수 파일 존재 여부 체크$(NC)"
	@echo -e "$(YELLOW)==================================$(NC)"
	@echo -e "$(BLUE)Driver 구현 파일들:$(NC)"
	@ls -la $(SRC_DIR)/Drivers/*/*.cpp 2>/dev/null | head -10 || echo -e "$(RED)❌ Driver 소스 파일 없음$(NC)"
	@echo ""
	@echo -e "$(BLUE)Pipeline 구현 파일들:$(NC)"
	@ls -la $(SRC_DIR)/Pipeline/*.cpp 2>/dev/null || echo -e "$(RED)❌ Pipeline 소스 파일 없음$(NC)"
	@echo ""
	@echo -e "$(BLUE)Workers 구현 파일들:$(NC)"
	@ls -la $(SRC_DIR)/Workers/Protocol/*.cpp 2>/dev/null | head -5 || echo -e "$(RED)❌ Workers 소스 파일 없음$(NC)"

# 중복 파일 체크
check-duplicates:
	@echo -e "$(BLUE)🔍 중복 소스 파일 체크$(NC)"
	@echo -e "$(YELLOW)=====================$(NC)"
	@echo "$(STEP3_SOURCES)" | tr ' ' '\n' | sort | uniq -d | while read duplicate; do \
		if [ -n "$$duplicate" ]; then \
			echo -e "$(RED)❌ 중복 파일: $$duplicate$(NC)"; \
		fi; \
	done
	@echo -e "$(GREEN)✅ 중복 체크 완료$(NC)"

.PHONY: debug-step3-sources check-step3-files check-duplicates

# =============================================================================
# 통합 실행 타겟들
# =============================================================================

# 빠른 테스트 (1단계만) - DB 설정 포함
quick: clean check-deps setup-testdb step1 run-step1
	@echo -e "$(GREEN)🎉 빠른 테스트 완료$(NC)"

# 전체 테스트 플로우 - DB 설정 포함
full: clean check-deps check-sources setup-testdb run-all
	@echo -e "$(GREEN)🎉 전체 테스트 플로우 완료$(NC)"

# 개발자용 빠른 빌드 체크
build-only: check-deps step1 step2 step3 step4 step5
	@echo -e "$(GREEN)🎉 모든 단계 빌드 완료 (실행 없음)$(NC)"

# =============================================================================
# 유틸리티 및 정리
# =============================================================================

clean:
	@echo -e "$(YELLOW)🧹 빌드 파일 정리$(NC)"
	@rm -rf $(BIN_DIR)
	@echo -e "$(GREEN)✅ 빌드 정리 완료$(NC)"

clean-all: clean clean-testdb
	@echo -e "$(GREEN)✅ 전체 정리 완료 (빌드 파일 + 테스트 DB)$(NC)"

# 디버그 정보 출력
debug-info:
	@echo -e "$(BLUE)🔧 PulseOne 테스트 시스템 디버그 정보$(NC)"
	@echo -e "$(YELLOW)========================================$(NC)"
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "INCLUDES: $(INCLUDES)"
	@echo "LIBS: $(LIBS)"
	@echo "HAS_NLOHMANN_JSON: $(HAS_NLOHMANN_JSON)"
	@echo "HAS_HIREDIS: $(HAS_HIREDIS)"
	@echo "HAS_BACNET_STACK: $(HAS_BACNET_STACK)"
	@echo ""
	@echo "Source counts:"
	@echo "  UTILS_SOURCES: $(words $(UTILS_SOURCES))"
	@echo "  CONFIG_SOURCES: $(words $(CONFIG_SOURCES))"
	@echo "  DATABASE_SOURCES: $(words $(DATABASE_SOURCES))"
	@echo "  CLIENT_SOURCES: $(words $(CLIENT_SOURCES))"
	@echo "  WORKERS_SOURCES: $(words $(WORKERS_SOURCES))"
	@echo "  DRIVERS_SOURCES: $(words $(DRIVERS_SOURCES))"
	@echo "  PIPELINE_SOURCES: $(words $(PIPELINE_SOURCES))"

# 도움말
help:
	@echo -e "$(BLUE)🔧 PulseOne Collector 테스트 시스템 (완전 수정)$(NC)"
	@echo -e "$(YELLOW)===============================================$(NC)"
	@echo ""
	@echo -e "$(GREEN)🗄️  테스트 DB 관리:$(NC)"
	@echo "  setup-testdb         - SQLite 테스트 DB 생성 (5개 devices, 16개 data_points)"
	@echo "  check-testdb         - 테스트 DB 상태 및 데이터 확인"
	@echo "  reset-testdb         - 테스트 DB 초기화"
	@echo "  clean-testdb         - 테스트 DB 삭제"
	@echo ""
	@echo -e "$(GREEN)🧪 5단계 테스트 플로우:$(NC)"
	@echo "  step1 / run-step1    - 설정파일 읽기"
	@echo "  step2 / run-step2    - DB 접근하여 테이블들 읽기"
	@echo "  step3 / run-step3    - 프로토콜별 DeviceWorker 생성 (완전 수정)"
	@echo "  step4 / run-step4    - 드라이버 초기화 시뮬레이션"
	@echo "  step5 / run-step5    - 데이터 Redis 저장 시뮬레이션"
	@echo ""
	@echo -e "$(GREEN)🚀 통합 실행:$(NC)"
	@echo "  run-all              - 전체 5단계 플로우 실행"
	@echo "  quick                - DB 설정 + 1단계 빠른 테스트"
	@echo "  full                 - 환경체크 + DB 설정 + 전체 플로우"
	@echo "  build-only           - 모든 단계 빌드 (실행 없음)"
	@echo ""
	@echo -e "$(GREEN)🔧 개발 도구:$(NC)"
	@echo "  check-deps           - 라이브러리 의존성 체크"
	@echo "  check-sources        - 소스 파일 존재 여부 체크"
	@echo "  debug-step3-sources  - Step3 소스 파일 상세 분석"
	@echo "  check-step3-files    - Step3 필수 파일 존재 여부 체크"
	@echo "  check-duplicates     - 중복 소스 파일 체크"
	@echo "  debug-info           - 컴파일 설정 및 환경 정보 출력"
	@echo "  clean                - 빌드 파일 정리"
	@echo "  clean-all            - 빌드 파일 + 테스트 DB 정리"
	@echo ""
	@echo -e "$(GREEN)💡 사용 예시:$(NC)"
	@echo -e "  $(YELLOW)make debug-step3-sources$(NC)  # Step3 소스 분석"
	@echo -e "  $(YELLOW)make check-duplicates$(NC)     # 중복 파일 체크"
	@echo -e "  $(YELLOW)make step3$(NC)                # Step3만 빌드"
	@echo -e "  $(YELLOW)make run-step3$(NC)            # Step3 실행"

# =============================================================================
# 특수 타겟들
# =============================================================================

# SQLite 테스트 도구  
sqlite-shell:
	@echo -e "$(BLUE)🗄️  SQLite 셸 실행 - 테스트 DB$(NC)"
	@echo -e "$(YELLOW)종료: .quit$(NC)"
	@sqlite3 db/pulseone_test.db

# 기본 타겟 체인
.DEFAULT_GOAL := help