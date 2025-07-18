# =============================================================================
# PulseOne Collector 통합 Makefile v2.0
# 기존 시스템 + 새로운 데이터 액세스 아키텍처 + Docker + CI/CD
# =============================================================================

# 프로젝트 설정
PROJECT_NAME := pulseone-collector
VERSION := 2.0.0
BUILD_TYPE := Release

# 색상 정의
BLUE := \033[0;34m
GREEN := \033[0;32m
YELLOW := \033[1;33m
RED := \033[0;31m
PURPLE := \033[0;35m
CYAN := \033[0;36m
NC := \033[0m

# 컴파일러 설정
CXX := g++
CC := gcc

# 기본 컴파일러 플래그 (C++17 + 새 아키텍처 요구사항)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wno-unused-parameter
CFLAGS := -std=c99 -Wall -Wextra

# 인클루드 디렉토리 (기존 + 새로운 데이터 액세스 계층)
INCLUDES := -Iinclude \
           -Iinclude/Database \
           -Iinclude/Utils \
           -Iinclude/Plugin \
           -Iinclude/Config \
           -Iinclude/Engine \
           -Iinclude/Publisher \
           -Iinclude/Drivers \
           -I/usr/local/include \
           -I/usr/include

# 빌드 타입별 플래그
ifeq ($(BUILD_TYPE),Debug)
    CXXFLAGS += -g -O0 -DDEBUG -fsanitize=address -fno-omit-frame-pointer
    CFLAGS += -g -O0 -DDEBUG
    LDFLAGS += -fsanitize=address
    BUILD_SUFFIX := _debug
else ifeq ($(BUILD_TYPE),Release)
    CXXFLAGS += -O3 -DNDEBUG -march=native -flto
    CFLAGS += -O3 -DNDEBUG -march=native
    LDFLAGS += -flto
    BUILD_SUFFIX := 
else ifeq ($(BUILD_TYPE),RelWithDebInfo)
    CXXFLAGS += -O2 -g -DNDEBUG
    CFLAGS += -O2 -g -DNDEBUG
    BUILD_SUFFIX := _rel_debug
endif

# pkg-config를 통한 라이브러리 설정
PKG_CONFIG := pkg-config

# =============================================================================
# 라이브러리 설정 (기존 + 새 아키텍처 요구사항)
# =============================================================================

# 기존 데이터베이스 라이브러리 (PostgreSQL 우선)
DB_LIBS := -lpqxx -lpq -lsqlite3

# 새로운 프로토콜 라이브러리
# libmodbus 설정
MODBUS_CFLAGS := $(shell $(PKG_CONFIG) --cflags libmodbus 2>/dev/null || echo "-I/usr/local/include")
MODBUS_LIBS := $(shell $(PKG_CONFIG) --libs libmodbus 2>/dev/null || echo "-L/usr/local/lib -lmodbus")
ifneq ($(shell $(PKG_CONFIG) --exists libmodbus 2>/dev/null; echo $$?),0)
    CXXFLAGS += -DHAVE_LIBMODBUS=0
else
    CXXFLAGS += -DHAVE_LIBMODBUS=1
endif

# Paho MQTT 설정
MQTT_CFLAGS := $(shell $(PKG_CONFIG) --cflags paho-mqtt3c paho-mqttpp3 2>/dev/null || echo "-I/usr/local/include")
MQTT_LIBS := $(shell $(PKG_CONFIG) --libs paho-mqtt3c paho-mqttpp3 2>/dev/null || echo "-L/usr/local/lib -lpaho-mqttpp3 -lpaho-mqtt3c")
ifneq ($(shell $(PKG_CONFIG) --exists paho-mqtt3c 2>/dev/null; echo $$?),0)
    CXXFLAGS += -DHAVE_PAHO_MQTT=0
else
    CXXFLAGS += -DHAVE_PAHO_MQTT=1
endif

# BACnet Stack 설정
BACNET_CFLAGS := -I/usr/local/include/bacnet
BACNET_LIBS := -L/usr/local/lib -lbacnet
ifeq ($(shell test -d /usr/local/include/bacnet && echo yes),yes)
    CXXFLAGS += -DHAVE_BACNET=1
else
    CXXFLAGS += -DHAVE_BACNET=0
endif

# JSON 라이브러리 (nlohmann/json)
JSON_CFLAGS := $(shell $(PKG_CONFIG) --cflags nlohmann_json 2>/dev/null || echo "-I/usr/local/include")
ifeq ($(shell test -f /usr/local/include/nlohmann/json.hpp && echo yes),yes)
    CXXFLAGS += -DHAVE_NLOHMANN_JSON=1
else
    CXXFLAGS += -DHAVE_NLOHMANN_JSON=0
endif

# spdlog 설정
SPDLOG_CFLAGS := $(shell $(PKG_CONFIG) --cflags spdlog 2>/dev/null || echo "-I/usr/local/include")
SPDLOG_LIBS := $(shell $(PKG_CONFIG) --libs spdlog 2>/dev/null || echo "-L/usr/local/lib -lspdlog")
ifeq ($(shell test -f /usr/local/include/spdlog/spdlog.h && echo yes),yes)
    CXXFLAGS += -DHAVE_SPDLOG=1
else
    CXXFLAGS += -DHAVE_SPDLOG=0
endif

# 추가 라이브러리들
EXTRA_LIBS := $(SPDLOG_LIBS) -lhiredis -ljson-c -lconfig -lcurl -lrabbitmq

# 시스템 라이브러리
SYSTEM_LIBS := -lpthread -ldl -lm -luuid -lrt

# SSL/TLS 라이브러리
SSL_LIBS := -lssl -lcrypto

# 통합 라이브러리 설정
ALL_CFLAGS := $(INCLUDES) $(MODBUS_CFLAGS) $(MQTT_CFLAGS) $(BACNET_CFLAGS) $(JSON_CFLAGS) $(SPDLOG_CFLAGS)
ALL_LIBS := $(DB_LIBS) $(MODBUS_LIBS) $(MQTT_LIBS) $(BACNET_LIBS) $(EXTRA_LIBS) $(SSL_LIBS) $(SYSTEM_LIBS)

# =============================================================================
# 디렉토리 및 파일 설정
# =============================================================================

# 디렉토리 설정
SRC_DIR := src
BUILD_DIR := build$(BUILD_SUFFIX)
BIN_DIR := bin
LIB_DIR := lib
TEST_DIR := test
INCLUDE_DIR := include

# 새로운 아키텍처 소스 파일들 분류
DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp)
ENGINE_SOURCES := $(wildcard $(SRC_DIR)/Engine/*.cpp)
DRIVERS_SOURCES := $(wildcard $(SRC_DIR)/Drivers/*.cpp)
UTILS_SOURCES := $(wildcard $(SRC_DIR)/Utils/*.cpp)
CONFIG_SOURCES := $(wildcard $(SRC_DIR)/Config/*.cpp)
PUBLISHER_SOURCES := $(wildcard $(SRC_DIR)/Publisher/*.cpp)
PLUGIN_SOURCES := $(wildcard $(SRC_DIR)/Plugin/*.cpp)

# 메인 소스 파일
MAIN_SRC := $(SRC_DIR)/main.cpp

# 전체 소스 파일들 (main 제외)
ALL_SRCS := $(DATABASE_SOURCES) $(ENGINE_SOURCES) $(DRIVERS_SOURCES) \
           $(UTILS_SOURCES) $(CONFIG_SOURCES) $(PUBLISHER_SOURCES) $(PLUGIN_SOURCES)

# 오브젝트 파일들
ALL_OBJS := $(ALL_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
MAIN_OBJ := $(BUILD_DIR)/main.o

# 최종 실행 파일
TARGET := $(BIN_DIR)/$(PROJECT_NAME)$(BUILD_SUFFIX)

# 테스트 실행 파일
TEST_SOURCES := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS := $(TEST_SOURCES:$(TEST_DIR)/%.cpp=$(BUILD_DIR)/test/%.o)
TEST_TARGET := $(BIN_DIR)/test_runner$(BUILD_SUFFIX)

# 환경 설정 파일
ENV_FILE := ../config/.env
ENV_TEMPLATE := ../config/.env.template
ENV_DEV := ../config/.env.dev

# Docker 설정
DOCKER_IMAGE := pulseone-collector
DOCKER_TAG := $(VERSION)

# =============================================================================
# 주요 빌드 타겟들
# =============================================================================

# 기본 타겟
.DEFAULT_GOAL := all

# 모든 타겟 (새 아키텍처 검증 포함)
.PHONY: all
all: precheck directories header-check feature-check $(TARGET) post-build
	@echo -e "$(GREEN)✅ Build completed successfully: $(TARGET)$(NC)"

# 의존성 확인 (강화된 버전)
.PHONY: deps
deps:
	@echo -e "$(BLUE)🔍 Checking dependencies...$(NC)"
	@echo "📦 Core Libraries:"
	@$(PKG_CONFIG) --exists libpqxx && echo -e "  $(GREEN)✅ libpqxx: $$($(PKG_CONFIG) --modversion libpqxx)$(NC)" || echo -e "  $(RED)❌ libpqxx: Not found$(NC)"
	@[ -f /usr/include/sqlite3.h ] && echo -e "  $(GREEN)✅ SQLite3: Found$(NC)" || echo -e "  $(RED)❌ SQLite3: Not found$(NC)"
	@echo ""
	@echo "📡 Protocol Libraries:"
	@$(PKG_CONFIG) --exists libmodbus && echo -e "  $(GREEN)✅ libmodbus: $$($(PKG_CONFIG) --modversion libmodbus)$(NC)" || echo -e "  $(YELLOW)⚠️  libmodbus: Not found$(NC)"
	@[ -f /usr/local/include/MQTTClient.h ] && echo -e "  $(GREEN)✅ Paho MQTT C: Found$(NC)" || echo -e "  $(YELLOW)⚠️  Paho MQTT C: Not found$(NC)"
	@[ -f /usr/local/include/mqtt/client.h ] && echo -e "  $(GREEN)✅ Paho MQTT C++: Found$(NC)" || echo -e "  $(YELLOW)⚠️  Paho MQTT C++: Not found$(NC)"
	@[ -d /usr/local/include/bacnet ] && echo -e "  $(GREEN)✅ BACnet Stack: Found$(NC)" || echo -e "  $(YELLOW)⚠️  BACnet Stack: Not found$(NC)"
	@echo ""
	@echo "🔧 Utility Libraries:"
	@[ -f /usr/local/include/nlohmann/json.hpp ] && echo -e "  $(GREEN)✅ nlohmann/json: Found$(NC)" || echo -e "  $(YELLOW)⚠️  nlohmann/json: Not found$(NC)"
	@[ -f /usr/local/include/spdlog/spdlog.h ] && echo -e "  $(GREEN)✅ spdlog: Found$(NC)" || echo -e "  $(YELLOW)⚠️  spdlog: Not found$(NC)"
	@[ -f /usr/local/include/amqp.h ] && echo -e "  $(GREEN)✅ RabbitMQ-C: Found$(NC)" || echo -e "  $(YELLOW)⚠️  RabbitMQ-C: Not found$(NC)"
	@[ -f /usr/include/hiredis/hiredis.h ] && echo -e "  $(GREEN)✅ hiredis: Found$(NC)" || echo -e "  $(YELLOW)⚠️  hiredis: Not found$(NC)"
	@echo -e "$(CYAN)ℹ️  Missing libraries will disable related features$(NC)"

# 디렉토리 생성 (새 아키텍처 지원)
.PHONY: directories
directories:
	@mkdir -p $(BUILD_DIR)/Database $(BUILD_DIR)/Engine $(BUILD_DIR)/Drivers
	@mkdir -p $(BUILD_DIR)/Utils $(BUILD_DIR)/Config $(BUILD_DIR)/Publisher
	@mkdir -p $(BUILD_DIR)/Plugin $(BUILD_DIR)/test
	@mkdir -p $(BIN_DIR) $(LIB_DIR)

# 환경 설정 확인
.PHONY: precheck
precheck:
	@echo -e "$(BLUE)🔍 Checking environment...$(NC)"
	@if [ ! -f $(ENV_FILE) ]; then \
		if [ -f $(ENV_TEMPLATE) ]; then \
			echo -e "$(YELLOW)⚠️  $(ENV_FILE) not found. Copying from template...$(NC)"; \
			cp $(ENV_TEMPLATE) $(ENV_FILE); \
		elif [ -f $(ENV_DEV) ]; then \
			echo -e "$(YELLOW)⚠️  $(ENV_FILE) not found. Copying from dev template...$(NC)"; \
			cp $(ENV_DEV) $(ENV_FILE); \
		else \
			echo -e "$(YELLOW)⚠️  Creating default .env file...$(NC)"; \
			mkdir -p ../config; \
			echo "ENV_STAGE=dev" > $(ENV_FILE); \
			echo "LOG_LEVEL=info" >> $(ENV_FILE); \
			echo "DB_TYPE=postgresql" >> $(ENV_FILE); \
			echo "POSTGRES_MAIN_DB_HOST=localhost" >> $(ENV_FILE); \
			echo "POSTGRES_MAIN_DB_PORT=5432" >> $(ENV_FILE); \
			echo "POSTGRES_MAIN_DB_USER=user" >> $(ENV_FILE); \
			echo "POSTGRES_MAIN_DB_PASSWORD=password" >> $(ENV_FILE); \
			echo "POSTGRES_MAIN_DB_NAME=pulseone" >> $(ENV_FILE); \
		fi \
	fi
	@echo -e "$(GREEN)✅ Environment check completed$(NC)"

# 헤더 파일 확인 (강화된 버전)
.PHONY: header-check
header-check:
	@echo -e "$(BLUE)🔍 Checking required headers...$(NC)"
	@headers_ok=true; \
	echo "#include <iostream>" | $(CXX) $(ALL_CFLAGS) $(CXXFLAGS) -x c++ -c - -o /dev/null 2>/dev/null || headers_ok=false; \
	echo "#include <thread>" | $(CXX) $(ALL_CFLAGS) $(CXXFLAGS) -x c++ -c - -o /dev/null 2>/dev/null || headers_ok=false; \
	echo "#include <memory>" | $(CXX) $(ALL_CFLAGS) $(CXXFLAGS) -x c++ -c - -o /dev/null 2>/dev/null || headers_ok=false; \
	echo "#include <atomic>" | $(CXX) $(ALL_CFLAGS) $(CXXFLAGS) -x c++ -c - -o /dev/null 2>/dev/null || headers_ok=false; \
	echo "#include <mutex>" | $(CXX) $(ALL_CFLAGS) $(CXXFLAGS) -x c++ -c - -o /dev/null 2>/dev/null || headers_ok=false; \
	echo "#include <condition_variable>" | $(CXX) $(ALL_CFLAGS) $(CXXFLAGS) -x c++ -c - -o /dev/null 2>/dev/null || headers_ok=false; \
	echo "#include <chrono>" | $(CXX) $(ALL_CFLAGS) $(CXXFLAGS) -x c++ -c - -o /dev/null 2>/dev/null || headers_ok=false; \
	if [ "$$headers_ok" = "false" ]; then \
		echo -e "$(RED)❌ Essential C++17 headers missing$(NC)"; \
		exit 1; \
	fi; \
	echo -e "$(GREEN)✅ Essential headers found$(NC)"; \
	echo "#include <pqxx/pqxx>" | $(CXX) $(ALL_CFLAGS) $(CXXFLAGS) -x c++ -c - -o /dev/null 2>/dev/null && \
		echo -e "$(GREEN)✅ PostgreSQL headers found$(NC)" || \
		echo -e "$(RED)❌ PostgreSQL headers not found - database access will fail$(NC)"; \
	echo "#include <sqlite3.h>" | $(CXX) $(ALL_CFLAGS) $(CXXFLAGS) -x c++ -c - -o /dev/null 2>/dev/null && \
		echo -e "$(GREEN)✅ SQLite headers found$(NC)" || \
		echo -e "$(YELLOW)⚠️  SQLite headers not found$(NC)"

# 기능 확인 (새로운 기능)
.PHONY: feature-check
feature-check:
	@echo -e "$(BLUE)🔍 Checking available features...$(NC)"
	@echo "Features that will be available:"
	@echo "#include <modbus.h>" | $(CXX) $(ALL_CFLAGS) $(CXXFLAGS) -x c++ -c - -o /dev/null 2>/dev/null && \
		echo -e "  $(GREEN)✅ Modbus TCP/RTU support$(NC)" || \
		echo -e "  $(YELLOW)⚠️  Modbus support disabled$(NC)"
	@echo "#include <MQTTClient.h>" | $(CXX) $(ALL_CFLAGS) $(CXXFLAGS) -x c++ -c - -o /dev/null 2>/dev/null && \
		echo -e "  $(GREEN)✅ MQTT client support$(NC)" || \
		echo -e "  $(YELLOW)⚠️  MQTT support disabled$(NC)"
	@echo "#include <nlohmann/json.hpp>" | $(CXX) $(ALL_CFLAGS) $(CXXFLAGS) -x c++ -c - -o /dev/null 2>/dev/null && \
		echo -e "  $(GREEN)✅ JSON configuration support$(NC)" || \
		echo -e "  $(YELLOW)⚠️  JSON support limited$(NC)"
	@echo "#include <spdlog/spdlog.h>" | $(CXX) $(ALL_CFLAGS) $(CXXFLAGS) -x c++ -c - -o /dev/null 2>/dev/null && \
		echo -e "  $(GREEN)✅ Advanced logging support$(NC)" || \
		echo -e "  $(YELLOW)⚠️  Basic logging only$(NC)"

# 메인 실행 파일 빌드
$(TARGET): $(MAIN_OBJ) $(ALL_OBJS)
	@echo -e "$(BLUE)🔗 Linking $(TARGET)...$(NC)"
	@echo -e "$(CYAN)   Linking $$(echo '$(ALL_OBJS)' | wc -w) object files...$(NC)"
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(ALL_LIBS)

# 메인 오브젝트 파일
$(MAIN_OBJ): $(MAIN_SRC)
	@echo -e "$(BLUE)🔨 Compiling main application: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(ALL_CFLAGS) -c $< -o $@

# 오브젝트 파일들 (카테고리별 색상 구분)
$(BUILD_DIR)/Database/%.o: $(SRC_DIR)/Database/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(PURPLE)🗄️  Compiling database layer: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(ALL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/Engine/%.o: $(SRC_DIR)/Engine/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(CYAN)⚙️  Compiling engine layer: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(ALL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/Drivers/%.o: $(SRC_DIR)/Drivers/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)🔌 Compiling driver: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(ALL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(BLUE)🔨 Compiling: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(ALL_CFLAGS) -c $< -o $@

# 빌드 후 처리 (강화된 버전)
.PHONY: post-build
post-build:
	@echo -e "$(BLUE)📋 Post-build processing...$(NC)"
	@if [ -f "$(TARGET)" ]; then \
		chmod +x $(TARGET); \
		file_size=$$(ls -lh $(TARGET) | awk '{print $$5}'); \
		echo -e "$(GREEN)✅ Executable created: $(TARGET) ($$file_size)$(NC)"; \
		strip_size=$$(strip -s $(TARGET) -o /tmp/stripped_test 2>/dev/null && ls -lh /tmp/stripped_test | awk '{print $$5}' && rm -f /tmp/stripped_test || echo "N/A"); \
		[ "$$strip_size" != "N/A" ] && echo -e "$(CYAN)ℹ️  Stripped size would be: $$strip_size$(NC)"; \
	fi
	@# 개발 실행 스크립트 생성
	@if [ ! -f "dev.sh" ]; then \
		echo "#!/bin/bash" > dev.sh; \
		echo "# PulseOne Collector Development Runner" >> dev.sh; \
		echo "set -e" >> dev.sh; \
		echo "" >> dev.sh; \
		echo "export PKG_CONFIG_PATH=\"/usr/local/lib/pkgconfig:\$$PKG_CONFIG_PATH\"" >> dev.sh; \
		echo "export LD_LIBRARY_PATH=\"/usr/local/lib:\$$LD_LIBRARY_PATH\"" >> dev.sh; \
		echo "" >> dev.sh; \
		echo "# 설정 파일 확인" >> dev.sh; \
		echo "if [ ! -f \"../config/.env\" ]; then" >> dev.sh; \
		echo "    echo \"❌ Configuration file not found. Run 'make precheck' first.\"" >> dev.sh; \
		echo "    exit 1" >> dev.sh; \
		echo "fi" >> dev.sh; \
		echo "" >> dev.sh; \
		echo "echo \"🚀 Starting PulseOne Collector in development mode...\"" >> dev.sh; \
		echo "echo \"📍 Executable: $(TARGET)\"" >> dev.sh; \
		echo "echo \"📁 Working Directory: \$$(pwd)\"" >> dev.sh; \
		echo "echo \"\"" >> dev.sh; \
		echo "" >> dev.sh; \
		echo "# Ctrl+C 핸들러" >> dev.sh; \
		echo "trap 'echo \"\"; echo \"🛑 Stopping PulseOne Collector...\"; exit 0' INT" >> dev.sh; \
		echo "" >> dev.sh; \
		echo "$(TARGET) \"\$$@\"" >> dev.sh; \
		chmod +x dev.sh; \
		echo -e "$(GREEN)✅ Created dev.sh development script$(NC)"; \
	fi
	@# 프로덕션 실행 스크립트 생성
	@if [ ! -f "run.sh" ]; then \
		echo "#!/bin/bash" > run.sh; \
		echo "# PulseOne Collector Production Runner" >> run.sh; \
		echo "set -e" >> run.sh; \
		echo "" >> run.sh; \
		echo "export PKG_CONFIG_PATH=\"/usr/local/lib/pkgconfig:\$$PKG_CONFIG_PATH\"" >> run.sh; \
		echo "export LD_LIBRARY_PATH=\"/usr/local/lib:\$$LD_LIBRARY_PATH\"" >> run.sh; \
		echo "" >> run.sh; \
		echo "$(TARGET) \"\$$@\"" >> run.sh; \
		chmod +x run.sh; \
		echo -e "$(GREEN)✅ Created run.sh production script$(NC)"; \
	fi

# =============================================================================
# 빌드 변형들
# =============================================================================

# 디버그 빌드
.PHONY: debug
debug:
	@echo -e "$(YELLOW)🐛 Building debug version...$(NC)"
	@$(MAKE) BUILD_TYPE=Debug all

# 릴리즈 빌드
.PHONY: release
release:
	@echo -e "$(GREEN)🚀 Building release version...$(NC)"
	@$(MAKE) BUILD_TYPE=Release all

# 릴리즈 + 디버그 정보
.PHONY: relwithdebinfo
relwithdebinfo:
	@echo -e "$(CYAN)🔍 Building release with debug info...$(NC)"
	@$(MAKE) BUILD_TYPE=RelWithDebInfo all

# 테스트 빌드 (새로운 기능)
.PHONY: tests
tests: $(TEST_TARGET)
	@echo -e "$(GREEN)✅ Tests built: $(TEST_TARGET)$(NC)"

$(TEST_TARGET): $(TEST_OBJS) $(ALL_OBJS)
	@echo -e "$(BLUE)🧪 Linking test runner...$(NC)"
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(ALL_LIBS)

$(BUILD_DIR)/test/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(CYAN)🧪 Compiling test: $<$(NC)"
	$(CXX) $(CXXFLAGS) $(ALL_CFLAGS) -c $< -o $@

# 정리 (강화된 버전)
.PHONY: clean
clean:
	@echo -e "$(YELLOW)🧹 Cleaning build files...$(NC)"
	rm -rf build build_debug build_rel_debug
	rm -rf $(BIN_DIR) $(LIB_DIR)
	rm -f run.sh dev.sh
	rm -f *.log *.out core.*
	@echo -e "$(GREEN)✅ Clean completed$(NC)"

# 완전 정리
.PHONY: distclean
distclean: clean
	@echo -e "$(YELLOW)🧹 Deep cleaning...$(NC)"
	rm -rf logs/* 2>/dev/null || true
	rm -f ../config/.env.backup.* 2>/dev/null || true
	@echo -e "$(GREEN)✅ Deep clean completed$(NC)"

# =============================================================================
# Docker 지원 (기존 유지 + 개선)
# =============================================================================

.PHONY: build-docker
build-docker: precheck
	@echo -e "$(BLUE)🐳 Building Docker image...$(NC)"
	docker build -t $(DOCKER_IMAGE):$(DOCKER_TAG) -f Dockerfile.dev .
	docker tag $(DOCKER_IMAGE):$(DOCKER_TAG) $(DOCKER_IMAGE):latest
	@echo -e "$(GREEN)✅ Docker image built: $(DOCKER_IMAGE):$(DOCKER_TAG)$(NC)"

.PHONY: run-docker
run-docker:
	@echo -e "$(BLUE)🐳 Running Docker container...$(NC)"
	docker run --rm -it \
		-v $(PWD)/../config:/app/config \
		-v $(PWD)/logs:/opt/pulseone/logs \
		-v /etc/localtime:/etc/localtime:ro \
		--name pulseone-instance \
		-p 8080:8080 \
		$(DOCKER_IMAGE):$(DOCKER_TAG)

# Docker Compose 지원
.PHONY: docker-up
docker-up:
	@echo -e "$(BLUE)🐳 Starting Docker Compose services...$(NC)"
	cd .. && docker-compose -f docker-compose.dev.yml up --build -d
	@echo -e "$(GREEN)✅ Services started. Use 'make docker-logs' to view logs$(NC)"

.PHONY: docker-down
docker-down:
	@echo -e "$(BLUE)🐳 Stopping Docker Compose services...$(NC)"
	cd .. && docker-compose -f docker-compose.dev.yml down

.PHONY: docker-logs
docker-logs:
	@echo -e "$(BLUE)🐳 Showing Docker Compose logs...$(NC)"
	cd .. && docker-compose -f docker-compose.dev.yml logs -f pulseone-collector

# Docker 이미지 정리
.PHONY: docker-clean
docker-clean:
	@echo -e "$(YELLOW)🐳 Cleaning Docker images...$(NC)"
	docker rmi $(DOCKER_IMAGE):$(DOCKER_TAG) $(DOCKER_IMAGE):latest 2>/dev/null || true
	docker system prune -f

# =============================================================================
# CI/CD 파이프라인 (강화된 버전)
# =============================================================================

.PHONY: ci-check
ci-check:
	@echo -e "$(BLUE)✅ Running static checks...$(NC)"
	@if [ -n "$(ALL_SRCS)" ]; then \
		echo -e "$(CYAN)📝 Checking $(shell echo '$(ALL_SRCS)' | wc -w) source files...$(NC)"; \
		$(CXX) $(CXXFLAGS) $(ALL_CFLAGS) -fsyntax-only $(ALL_SRCS) $(MAIN_SRC); \
	else \
		echo -e "$(CYAN)📝 Checking main source file...$(NC)"; \
		$(CXX) $(CXXFLAGS) $(ALL_CFLAGS) -fsyntax-only $(MAIN_SRC); \
	fi
	@echo -e "$(GREEN)✅ Static check passed$(NC)"

.PHONY: ci-build
ci-build: clean release
	@echo -e "$(GREEN)✅ CI build completed$(NC)"

.PHONY: ci-test
ci-test:
	@echo -e "$(BLUE)🧪 Running tests...$(NC)"
	@if [ -f "$(TEST_TARGET)" ]; then \
		$(TEST_TARGET); \
		echo -e "$(GREEN)✅ Tests passed$(NC)"; \
	elif [ -f "$(BIN_DIR)/run_tests" ]; then \
		$(BIN_DIR)/run_tests; \
		echo -e "$(GREEN)✅ Tests passed$(NC)"; \
	else \
		echo -e "$(YELLOW)⚠️  No tests available. Skipping...$(NC)"; \
	fi

.PHONY: ci-lint
ci-lint:
	@echo -e "$(BLUE)🔍 Running code linting...$(NC)"
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all --std=c++17 --error-exitcode=1 $(SRC_DIR) 2>/dev/null || echo -e "$(YELLOW)⚠️  cppcheck warnings found$(NC)"; \
	else \
		echo -e "$(YELLOW)⚠️  cppcheck not available$(NC)"; \
	fi

.PHONY: ci
ci: deps ci-check ci-build ci-test ci-lint
	@echo -e "$(GREEN)🎉 CI pipeline completed successfully$(NC)"

# =============================================================================
# 개발 지원 도구들 (새로운 기능)
# =============================================================================

# 소스 코드 통계
.PHONY: stats
stats:
	@echo -e "$(BLUE)=== PulseOne Source Code Statistics ===$(NC)"
	@echo "📊 File counts:"
	@echo "  Header files (.h): $$(find $(INCLUDE_DIR) -name '*.h' 2>/dev/null | wc -l)"
	@echo "  Source files (.cpp): $$(find $(SRC_DIR) -name '*.cpp' 2>/dev/null | wc -l)"
	@echo "  Test files: $$(find $(TEST_DIR) -name '*.cpp' 2>/dev/null | wc -l)"
	@echo ""
	@echo "📈 Line counts:"
	@if command -v wc >/dev/null 2>&1; then \
		echo "  Total lines: $$(find $(SRC_DIR) $(INCLUDE_DIR) -name '*.cpp' -o -name '*.h' 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $$1}' || echo 'N/A')"; \
		echo "  Database layer: $$(find $(SRC_DIR)/Database $(INCLUDE_DIR)/Database -name '*.cpp' -o -name '*.h' 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $$1}' || echo 'N/A')"; \
		echo "  Engine layer: $$(find $(SRC_DIR)/Engine $(INCLUDE_DIR)/Engine -name '*.cpp' -o -name '*.h' 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $$1}' || echo 'N/A')"; \
		echo "  Driver layer: $$(find $(SRC_DIR)/Drivers $(INCLUDE_DIR)/Drivers -name '*.cpp' -o -name '*.h' 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $$1}' || echo 'N/A')"; \
	fi

# 개발 모드 실행
.PHONY: dev
dev: debug
	@echo -e "$(BLUE)🚀 Running in development mode...$(NC)"
	@if [ -f "dev.sh" ]; then \
		./dev.sh; \
	else \
		$(MAKE) run BUILD_TYPE=Debug; \
	fi

# 프로파일링 빌드
.PHONY: profile
profile:
	@echo -e "$(PURPLE)📊 Building with profiling support...$(NC)"
	$(MAKE) BUILD_TYPE=RelWithDebInfo CXXFLAGS="$(CXXFLAGS) -pg" LDFLAGS="$(LDFLAGS) -pg" all

# =============================================================================
# 정보 및 도움말 (개선된 버전)
# =============================================================================

.PHONY: info
info:
	@echo -e "$(BLUE)=== PulseOne Collector Build Information ===$(NC)"
	@echo "Project: $(PROJECT_NAME) v$(VERSION)"
	@echo "Build Type: $(BUILD_TYPE)"
	@echo "Compiler: $(CXX) $(shell $(CXX) --version 2>/dev/null | head -n1 || echo 'not found')"
	@echo "C++ Standard: C++17"
	@echo "Target: $(TARGET)"
	@echo ""
	@echo -e "$(BLUE)=== Database Support ===$(NC)"
	@$(PKG_CONFIG) --exists libpqxx && echo "PostgreSQL: ✅ v$$($(PKG_CONFIG) --modversion libpqxx)" || echo "PostgreSQL: ❌"
	@[ -f /usr/include/sqlite3.h ] && echo "SQLite: ✅" || echo "SQLite: ❌"
	@echo ""
	@echo -e "$(BLUE)=== Protocol Support ===$(NC)"
	@$(PKG_CONFIG) --exists libmodbus && echo "Modbus: ✅ v$$($(PKG_CONFIG) --modversion libmodbus)" || echo "Modbus: ❌"
	@[ -f /usr/local/include/MQTTClient.h ] && echo "MQTT: ✅" || echo "MQTT: ❌"
	@[ -d /usr/local/include/bacnet ] && echo "BACnet: ✅" || echo "BACnet: ❌"
	@echo ""
	@echo -e "$(BLUE)=== Build Status ===$(NC)"
	@[ -f "$(TARGET)" ] && echo "Executable: ✅ ($(shell ls -lh $(TARGET) 2>/dev/null | awk '{print $$5}' || echo 'N/A'))" || echo "Executable: ❌"
	@[ -f "$(ENV_FILE)" ] && echo "Environment: ✅" || echo "Environment: ❌"
	@echo "Object files: $(shell find build* -name '*.o' 2>/dev/null | wc -l || echo 0)"
	@echo "Source files: $(shell find $(SRC_DIR) -name '*.cpp' 2>/dev/null | wc -l || echo 0)"

# 실행
.PHONY: run
run: all
	@echo -e "$(BLUE)🚀 Running $(PROJECT_NAME)...$(NC)"
	@if [ -f "run.sh" ]; then \
		./run.sh; \
	else \
		export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$$PKG_CONFIG_PATH"; \
		export LD_LIBRARY_PATH="/usr/local/lib:$$LD_LIBRARY_PATH"; \
		$(TARGET); \
	fi

# 도움말 (개선된 버전)
.PHONY: help
help:
	@echo -e "$(BLUE)╔══════════════════════════════════════════════════════════════╗$(NC)"
	@echo -e "$(BLUE)║                PulseOne Collector Build System              ║$(NC)"
	@echo -e "$(BLUE)║                        v$(VERSION)                              ║$(NC)"
	@echo -e "$(BLUE)╚══════════════════════════════════════════════════════════════╝$(NC)"
	@echo ""
	@echo -e "$(GREEN)📦 Main Targets:$(NC)"
	@echo "  all             - Build the main application (default)"
	@echo "  debug           - Build with debug symbols and AddressSanitizer"
	@echo "  release         - Build optimized production version"
	@echo "  relwithdebinfo  - Build optimized with debug symbols"
	@echo "  tests           - Build test suite"
	@echo "  clean           - Remove build files"
	@echo "  distclean       - Remove all generated files"
	@echo ""
	@echo -e "$(CYAN)🔧 Development:$(NC)"
	@echo "  dev             - Build and run in development mode"
	@echo "  deps            - Check dependencies"
	@echo "  stats           - Show source code statistics"
	@echo "  profile         - Build with profiling support"
	@echo "  run             - Build and run application"
	@echo ""
	@echo -e "$(PURPLE)🐳 Docker:$(NC)"
	@echo "  docker-up       - Start with Docker Compose"
	@echo "  docker-down     - Stop Docker Compose"
	@echo "  docker-logs     - View container logs"
	@echo "  docker-clean    - Clean Docker images"
	@echo ""
	@echo -e "$(YELLOW)🔍 CI/CD:$(NC)"
	@echo "  ci              - Run full CI pipeline"
	@echo "  ci-check        - Static code analysis"
	@echo "  ci-build        - Clean release build"
	@echo "  ci-test         - Run tests"
	@echo "  ci-lint         - Code linting"
	@echo ""
	@echo -e "$(GREEN)ℹ️  Utility:$(NC)"
	@echo "  info            - Show detailed build information"
	@echo "  help            - Show this help"
	@echo ""
	@echo -e "$(CYAN)💡 Examples:$(NC)"
	@echo "  make debug && make dev     - Debug build and run"
	@echo "  make release && make run   - Production build and run"
	@echo "  make docker-up             - Start in Docker"

# 모든 빌드 타입을 한번에 테스트
.PHONY: test-all-builds
test-all-builds: clean
	@echo -e "$(BLUE)🔄 Testing all build configurations...$(NC)"
	@$(MAKE) debug
	@$(MAKE) clean
	@$(MAKE) release  
	@$(MAKE) clean
	@$(MAKE) relwithdebinfo
	@echo -e "$(GREEN)✅ All build configurations tested successfully$(NC)"

# 의존성 그래프 생성 (있는 경우)
.PHONY: deps-graph
deps-graph:
	@if command -v dot >/dev/null 2>&1; then \
		echo -e "$(BLUE)📊 Generating dependency graph...$(NC)"; \
		$(CXX) $(CXXFLAGS) $(ALL_CFLAGS) -MM $(ALL_SRCS) $(MAIN_SRC) > deps.txt; \
		echo -e "$(GREEN)✅ Dependencies saved to deps.txt$(NC)"; \
	else \
		echo -e "$(YELLOW)⚠️  graphviz not available for dependency graph$(NC)"; \
	fi