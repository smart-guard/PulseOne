# =============================================================================
# PulseOne Collector 통합 Makefile (기존 + 새 라이브러리 지원)
# 기존 시스템 + 새로운 드라이버 시스템 + Docker + CI/CD
# =============================================================================

# 프로젝트 설정
PROJECT_NAME := pulseone-collector
VERSION := 1.0.0
BUILD_TYPE := Release

# 색상 정의
BLUE := \033[0;34m
GREEN := \033[0;32m
YELLOW := \033[1;33m
RED := \033[0;31m
NC := \033[0m

# 컴파일러 설정
CXX := g++
CC := gcc

# 기본 컴파일러 플래그 (기존 + 확장)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic
CFLAGS := -std=c99 -Wall -Wextra

# 인클루드 디렉토리 (기존 + 새로운 드라이버)
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
    CXXFLAGS += -g -O0 -DDEBUG -fsanitize=address
    CFLAGS += -g -O0 -DDEBUG
    LDFLAGS += -fsanitize=address
else ifeq ($(BUILD_TYPE),Release)
    CXXFLAGS += -O3 -DNDEBUG -march=native
    CFLAGS += -O3 -DNDEBUG
else ifeq ($(BUILD_TYPE),RelWithDebInfo)
    CXXFLAGS += -O2 -g -DNDEBUG
    CFLAGS += -O2 -g -DNDEBUG
endif

# pkg-config를 통한 라이브러리 설정
PKG_CONFIG := pkg-config

# 기존 데이터베이스 라이브러리
DB_LIBS := -lpqxx -lpq -lsqlite3

# 새로운 프로토콜 라이브러리
# libmodbus 설정
MODBUS_CFLAGS := $(shell $(PKG_CONFIG) --cflags libmodbus 2>/dev/null || echo "-I/usr/local/include")
MODBUS_LIBS := $(shell $(PKG_CONFIG) --libs libmodbus 2>/dev/null || echo "-L/usr/local/lib -lmodbus")

# Paho MQTT 설정
MQTT_CFLAGS := $(shell $(PKG_CONFIG) --cflags paho-mqtt3c paho-mqttpp3 2>/dev/null || echo "-I/usr/local/include")
MQTT_LIBS := $(shell $(PKG_CONFIG) --libs paho-mqtt3c paho-mqttpp3 2>/dev/null || echo "-L/usr/local/lib -lpaho-mqttpp3 -lpaho-mqtt3c")

# BACnet Stack 설정
BACNET_CFLAGS := -I/usr/local/include/bacnet
BACNET_LIBS := -L/usr/local/lib -lbacnet

# 추가 라이브러리들 (새로 설치된 것들)
EXTRA_LIBS := -lspdlog -lhiredis -ljson-c -lconfig -lcurl -lrabbitmq

# 시스템 라이브러리
SYSTEM_LIBS := -lpthread -ldl -lm -luuid

# SSL/TLS 라이브러리
SSL_LIBS := -lssl -lcrypto

# 통합 라이브러리 설정
ALL_CFLAGS := $(INCLUDES) $(MODBUS_CFLAGS) $(MQTT_CFLAGS) $(BACNET_CFLAGS)
ALL_LIBS := $(DB_LIBS) $(MODBUS_LIBS) $(MQTT_LIBS) $(BACNET_LIBS) $(EXTRA_LIBS) $(SSL_LIBS) $(SYSTEM_LIBS)

# 디렉토리 설정
SRC_DIR := src
BUILD_DIR := build
BIN_DIR := bin
LIB_DIR := lib
TEST_DIR := tests

# 소스 파일 검색 (기존 방식 유지 + 확장)
MAIN_SRC := $(SRC_DIR)/main.cpp
ALL_SRCS := $(shell find $(SRC_DIR) -name '*.cpp' -not -name 'main.cpp' 2>/dev/null || echo "")
ALL_OBJS := $(ALL_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
MAIN_OBJ := $(BUILD_DIR)/main.o

# 최종 실행 파일
TARGET := $(BIN_DIR)/$(PROJECT_NAME)

# 환경 설정 파일
ENV_FILE := ../config/.env
ENV_TEMPLATE := ../config/.env.template
ENV_DEV := ../config/.env.dev

# Docker 설정
DOCKER_IMAGE := pulseone-collector
DOCKER_TAG := latest

# =============================================================================
# 주요 빌드 타겟들
# =============================================================================

# 기본 타겟
.DEFAULT_GOAL := all

# 모든 타겟 (기존 방식 + 새로운 검증)
.PHONY: all
all: precheck directories header-check $(TARGET) post-build
	@echo -e "$(GREEN)✅ Build completed successfully: $(TARGET)$(NC)"

# 의존성 확인 (Dockerfile에서 이미 설치되었으므로 상태만 확인)
.PHONY: deps
deps:
	@echo -e "$(BLUE)🔍 Checking dependencies...$(NC)"
	@echo "📦 Library Status:"
	@$(PKG_CONFIG) --exists libmodbus && echo "  ✅ libmodbus: $$($(PKG_CONFIG) --modversion libmodbus)" || echo "  ❌ libmodbus: Not found"
	@[ -f /usr/local/include/MQTTClient.h ] && echo "  ✅ Paho MQTT C: Found" || echo "  ❌ Paho MQTT C: Not found"
	@[ -f /usr/local/include/mqtt/client.h ] && echo "  ✅ Paho MQTT C++: Found" || echo "  ❌ Paho MQTT C++: Not found"
	@[ -d /usr/local/include/bacnet ] && echo "  ✅ BACnet Stack: Found" || echo "  ❌ BACnet Stack: Not found"
	@[ -f /usr/local/include/amqp.h ] && echo "  ✅ RabbitMQ-C: Found" || echo "  ❌ RabbitMQ-C: Not found"
	@[ -f /usr/local/include/nlohmann/json.hpp ] && echo "  ✅ nlohmann/json: Found" || echo "  ❌ nlohmann/json: Not found"
	@[ -f /usr/local/include/spdlog/spdlog.h ] && echo "  ✅ spdlog: Found" || echo "  ❌ spdlog: Not found"
	@echo "✅ All dependencies are already installed in the container!"

# 디렉토리 생성
.PHONY: directories
directories:
	@mkdir -p $(BUILD_DIR)/Database $(BUILD_DIR)/Utils $(BUILD_DIR)/Plugin
	@mkdir -p $(BUILD_DIR)/Config $(BUILD_DIR)/Engine $(BUILD_DIR)/Publisher
	@mkdir -p $(BUILD_DIR)/Drivers $(BUILD_DIR)/tests
	@mkdir -p $(BIN_DIR) $(LIB_DIR)

# 환경 설정 확인 (기존 방식 확장)
.PHONY: precheck
precheck:
	@echo -e "$(BLUE)🔍 Checking environment...$(NC)"
	@if [ ! -f $(ENV_FILE) ]; then \
		if [ -f $(ENV_TEMPLATE) ]; then \
			echo -e "$(YELLOW)⚠️ $(ENV_FILE) not found. Copying from template...$(NC)"; \
			cp $(ENV_TEMPLATE) $(ENV_FILE); \
		elif [ -f $(ENV_DEV) ]; then \
			echo -e "$(YELLOW)⚠️ $(ENV_FILE) not found. Copying from dev template...$(NC)"; \
			cp $(ENV_DEV) $(ENV_FILE); \
		else \
			echo -e "$(YELLOW)⚠️ Creating default .env file...$(NC)"; \
			mkdir -p ../config; \
			echo "ENV_STAGE=dev" > $(ENV_FILE); \
			echo "LOG_LEVEL=info" >> $(ENV_FILE); \
		fi \
	fi
	@echo -e "$(GREEN)✅ Environment check completed$(NC)"

# 헤더 파일 확인 (새로운 기능)
.PHONY: header-check
header-check:
	@echo -e "$(BLUE)🔍 Checking required headers...$(NC)"
	@headers_ok=true; \
	echo "#include <iostream>" | $(CXX) $(ALL_CFLAGS) -x c++ -c - -o /dev/null 2>/dev/null || headers_ok=false; \
	echo "#include <thread>" | $(CXX) $(ALL_CFLAGS) -x c++ -c - -o /dev/null 2>/dev/null || headers_ok=false; \
	echo "#include <memory>" | $(CXX) $(ALL_CFLAGS) -x c++ -c - -o /dev/null 2>/dev/null || headers_ok=false; \
	if [ "$$headers_ok" = "false" ]; then \
		echo -e "$(RED)❌ Essential headers missing$(NC)"; \
		exit 1; \
	fi; \
	echo "#include <modbus.h>" | $(CXX) $(ALL_CFLAGS) -x c++ -c - -o /dev/null 2>/dev/null && \
		echo -e "$(GREEN)✅ Modbus headers found$(NC)" || \
		echo -e "$(YELLOW)⚠️ Modbus headers not found - support may be limited$(NC)"; \
	echo "#include <MQTTClient.h>" | $(CXX) $(ALL_CFLAGS) -x c++ -c - -o /dev/null 2>/dev/null && \
		echo -e "$(GREEN)✅ MQTT headers found$(NC)" || \
		echo -e "$(YELLOW)⚠️ MQTT headers not found - support may be limited$(NC)"

# 메인 실행 파일 빌드
$(TARGET): $(MAIN_OBJ) $(ALL_OBJS)
	@echo -e "$(BLUE)🔗 Linking $(TARGET)...$(NC)"
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(ALL_LIBS)

# 메인 오브젝트 파일
$(MAIN_OBJ): $(MAIN_SRC)
	@echo -e "$(BLUE)🔨 Compiling $<...$(NC)"
	$(CXX) $(CXXFLAGS) $(ALL_CFLAGS) -c $< -o $@

# 일반 오브젝트 파일들 (기존 패턴 확장)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(BLUE)🔨 Compiling $<...$(NC)"
	$(CXX) $(CXXFLAGS) $(ALL_CFLAGS) -c $< -o $@

# 빌드 후 처리 (새로운 기능)
.PHONY: post-build
post-build:
	@echo -e "$(BLUE)📋 Post-build processing...$(NC)"
	@if [ -f "$(TARGET)" ]; then \
		chmod +x $(TARGET); \
		file_size=$$(ls -lh $(TARGET) | awk '{print $$5}'); \
		echo -e "$(GREEN)✅ Executable created: $(TARGET) ($$file_size)$(NC)"; \
	fi
	@# 실행 스크립트 생성
	@if [ ! -f "run.sh" ]; then \
		echo "#!/bin/bash" > run.sh; \
		echo "export PKG_CONFIG_PATH=\"/usr/local/lib/pkgconfig:\$$PKG_CONFIG_PATH\"" >> run.sh; \
		echo "export LD_LIBRARY_PATH=\"/usr/local/lib:\$$LD_LIBRARY_PATH\"" >> run.sh; \
		echo "$(TARGET) \"\$$@\"" >> run.sh; \
		chmod +x run.sh; \
		echo -e "$(GREEN)✅ Created run.sh script$(NC)"; \
	fi

# =============================================================================
# 빌드 변형들
# =============================================================================

# 디버그 빌드
.PHONY: debug
debug:
	@$(MAKE) BUILD_TYPE=Debug all

# 릴리즈 빌드
.PHONY: release
release:
	@$(MAKE) BUILD_TYPE=Release all

# 정리
.PHONY: clean
clean:
	@echo -e "$(YELLOW)🧹 Cleaning build files...$(NC)"
	rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)
	rm -f run.sh dev.sh
	@echo -e "$(GREEN)✅ Clean completed$(NC)"

# =============================================================================
# 기존 Docker 및 CI/CD 부분은 그대로 유지
# =============================================================================

.PHONY: build-docker
build-docker: precheck
	@echo -e "$(BLUE)🐳 Building Docker image...$(NC)"
	docker build -t $(DOCKER_IMAGE):$(DOCKER_TAG) -f Dockerfile.dev .
	@echo -e "$(GREEN)✅ Docker image built: $(DOCKER_IMAGE):$(DOCKER_TAG)$(NC)"

.PHONY: run-docker
run-docker:
	@echo -e "$(BLUE)🐳 Running Docker container...$(NC)"
	docker run --rm -it \
		-v $(PWD)/../config:/app/config \
		-v $(PWD)/logrotate:/etc/logrotate.d \
		-v $(PWD)/logs:/opt/pulseone/logs \
		-v /etc/localtime:/etc/localtime:ro \
		--name pulseone-instance \
		$(DOCKER_IMAGE):$(DOCKER_TAG)

# Docker Compose 지원
.PHONY: docker-up
docker-up:
	@echo -e "$(BLUE)🐳 Starting Docker Compose services...$(NC)"
	cd .. && docker-compose -f docker-compose.dev.yml up --build

.PHONY: docker-down
docker-down:
	@echo -e "$(BLUE)🐳 Stopping Docker Compose services...$(NC)"
	cd .. && docker-compose -f docker-compose.dev.yml down

# Docker 이미지 정리
.PHONY: docker-clean
docker-clean:
	@echo -e "$(YELLOW)🐳 Cleaning Docker images...$(NC)"
	docker rmi $(DOCKER_IMAGE):$(DOCKER_TAG) 2>/dev/null || true
	docker system prune -f

# =============================================================================
# CI/CD 파이프라인 (기존 유지)
# =============================================================================

.PHONY: ci-check
ci-check:
	@echo -e "$(BLUE)✅ Running static checks...$(NC)"
	@if [ -n "$(ALL_SRCS)" ]; then \
		$(CXX) $(CXXFLAGS) $(ALL_CFLAGS) -fsyntax-only $(ALL_SRCS) $(MAIN_SRC); \
	else \
		$(CXX) $(CXXFLAGS) $(ALL_CFLAGS) -fsyntax-only $(MAIN_SRC); \
	fi
	@echo -e "$(GREEN)✅ Static check passed$(NC)"

.PHONY: ci-build
ci-build: clean all
	@echo -e "$(GREEN)✅ CI build completed$(NC)"

.PHONY: ci-test
ci-test:
	@echo -e "$(BLUE)🧪 Running tests...$(NC)"
	@if [ -f "$(BIN_DIR)/run_tests" ]; then \
		$(BIN_DIR)/run_tests; \
		echo -e "$(GREEN)✅ Tests passed$(NC)"; \
	else \
		echo -e "$(YELLOW)⚠️ No collector unit tests defined. Skipping...$(NC)"; \
	fi

.PHONY: ci
ci: ci-check ci-build ci-test
	@echo -e "$(GREEN)🎉 CI pipeline completed successfully$(NC)"

# =============================================================================
# 정보 및 도움말 (기존 유지하되 업데이트)
# =============================================================================

.PHONY: info
info:
	@echo -e "$(BLUE)=== PulseOne Collector Build Information ===$(NC)"
	@echo "Project: $(PROJECT_NAME) v$(VERSION)"
	@echo "Build Type: $(BUILD_TYPE)"
	@echo "Compiler: $(CXX) $(shell $(CXX) --version 2>/dev/null | head -n1 || echo 'not found')"
	@echo "C++ Standard: C++17"
	@echo ""
	@echo -e "$(BLUE)=== Library Information ===$(NC)"
	@echo "Database: PostgreSQL + SQLite"
	@echo "Modbus: $(shell $(PKG_CONFIG) --modversion libmodbus 2>/dev/null || echo 'manual installation')"
	@echo "MQTT: $(shell $(PKG_CONFIG) --modversion paho-mqtt3c 2>/dev/null || echo 'manual installation')"
	@echo "BACnet: Manual installation"
	@echo "Logging: spdlog"
	@echo "JSON: nlohmann/json"
	@echo "Redis: hiredis"
	@echo ""
	@echo -e "$(BLUE)=== Build Status ===$(NC)"
	@[ -f "$(TARGET)" ] && echo "Executable: Found ($(shell ls -lh $(TARGET) 2>/dev/null | awk '{print $$5}' || echo 'N/A'))" || echo "Executable: Not built"
	@[ -f "$(ENV_FILE)" ] && echo "Environment: Configured" || echo "Environment: Not configured"
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

# 도움말
.PHONY: help
help:
	@echo -e "$(BLUE)PulseOne Collector Build System$(NC)"
	@echo "Usage: make [target]"
	@echo ""
	@echo "Main targets:"
	@echo "  all             - Build the main application (default)"
	@echo "  debug           - Build with debug symbols"
	@echo "  release         - Build optimized version"
	@echo "  deps            - Check dependencies"
	@echo "  run             - Build and run application"
	@echo "  clean           - Remove build files"
	@echo ""
	@echo "Docker targets:"
	@echo "  docker-up       - Start with Docker Compose"
	@echo "  docker-down     - Stop Docker Compose"
	@echo "  docker-clean    - Clean Docker images"
	@echo ""
	@echo "CI/CD targets:"
	@echo "  ci              - Run CI pipeline"
	@echo "  ci-check        - Static code analysis"
	@echo "  ci-build        - Clean build"
	@echo "  ci-test         - Run tests"
	@echo ""
	@echo "Utility targets:"
	@echo "  info            - Show build information"
	@echo "  help            - Show this help"