# =============================================================================
# Export Gateway Unit Tests Makefile (Complete Version - Paho MQTT C++ 지원)
# =============================================================================

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g

# =============================================================================
# 디렉토리 구조
# =============================================================================

UNIT_TEST_DIR = $(shell pwd)
EXPORT_GATEWAY_DIR = $(shell cd ../.. && pwd)
PROJECT_ROOT = $(shell dirname $(shell dirname $(EXPORT_GATEWAY_DIR)))
SHARED_DIR = $(PROJECT_ROOT)/core/shared

INCLUDE_DIR = $(EXPORT_GATEWAY_DIR)/include
SHARED_INCLUDE_DIR = $(SHARED_DIR)/include
SHARED_LIB_DIR = $(SHARED_DIR)/lib

BIN_DIR = $(EXPORT_GATEWAY_DIR)/bin/tests
BUILD_DIR = build

# =============================================================================
# Include 경로
# =============================================================================

INCLUDES = -I$(INCLUDE_DIR)
INCLUDES += -I$(SHARED_INCLUDE_DIR)
INCLUDES += -I$(SHARED_INCLUDE_DIR)/Platform
INCLUDES += -I$(SHARED_INCLUDE_DIR)/Common
INCLUDES += -I$(SHARED_INCLUDE_DIR)/Utils
INCLUDES += -I$(SHARED_INCLUDE_DIR)/Database
INCLUDES += -I$(SHARED_INCLUDE_DIR)/Database/Entities
INCLUDES += -I$(SHARED_INCLUDE_DIR)/Database/Repositories
INCLUDES += -I$(SHARED_INCLUDE_DIR)/Alarm
INCLUDES += -I$(SHARED_INCLUDE_DIR)/Export
INCLUDES += -I$(SHARED_INCLUDE_DIR)/Client
INCLUDES += -I$(SHARED_INCLUDE_DIR)/Security
INCLUDES += -I$(SHARED_INCLUDE_DIR)/Data
INCLUDES += -I$(PROJECT_ROOT)/core/collector/include
INCLUDES += -I/usr/include/hiredis
INCLUDES += -I/usr/local/include

# =============================================================================
# 라이브러리 감지
# =============================================================================

HAS_SHARED := $(shell [ -d "$(SHARED_LIB_DIR)" ] && [ -f "$(SHARED_LIB_DIR)/libpulseone-common.a" ] && echo "1" || echo "0")
HAS_HIREDIS := $(shell pkg-config --exists hiredis 2>/dev/null && echo "1" || echo "0")
ifeq ($(HAS_HIREDIS),0)
    HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
endif
HAS_CURL := $(shell pkg-config --exists libcurl 2>/dev/null && echo "1" || echo "0")
ifeq ($(HAS_CURL),0)
    HAS_CURL := $(shell echo '\#include <curl/curl.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
endif
HAS_OPENSSL := $(shell pkg-config --exists openssl 2>/dev/null && echo "1" || echo "0")

# Paho MQTT C++ 라이브러리 감지
HAS_PAHO_MQTT := $(shell [ -f /usr/include/mqtt/async_client.h ] && echo "1" || echo "0")

# =============================================================================
# 링크 라이브러리
# =============================================================================

SHARED_LIBS =
ifeq ($(HAS_SHARED),1)
    CXXFLAGS += -DHAS_SHARED_LIBS=1
    SHARED_LIBS = -L$(SHARED_LIB_DIR)
    SHARED_LIBS += -lpulseone-alarm
    SHARED_LIBS += -lpulseone-export
    SHARED_LIBS += -lpulseone-data
    SHARED_LIBS += -lpulseone-database
    SHARED_LIBS += -lpulseone-client
    SHARED_LIBS += -lpulseone-common
    SHARED_LIBS += -lpulseone-security
endif

UTILITY_LIBS =

ifeq ($(HAS_HIREDIS),1)
    HIREDIS_LIBS := $(shell pkg-config --libs hiredis 2>/dev/null)
    ifneq ($(HIREDIS_LIBS),)
        UTILITY_LIBS += $(HIREDIS_LIBS)
    else
        UTILITY_LIBS += -lhiredis
    endif
    CXXFLAGS += -DHAVE_REDIS=1
endif

ifeq ($(HAS_CURL),1)
    CURL_LIBS := $(shell pkg-config --libs libcurl 2>/dev/null)
    ifneq ($(CURL_LIBS),)
        UTILITY_LIBS += $(CURL_LIBS)
    else
        UTILITY_LIBS += -lcurl
    endif
    CXXFLAGS += -DHAS_CURL=1
endif

ifeq ($(HAS_OPENSSL),1)
    OPENSSL_LIBS := $(shell pkg-config --libs openssl 2>/dev/null)
    ifneq ($(OPENSSL_LIBS),)
        UTILITY_LIBS += $(OPENSSL_LIBS)
    else
        UTILITY_LIBS += -lssl -lcrypto
    endif
    CXXFLAGS += -DHAS_OPENSSL=1
endif

# Paho MQTT C++ 라이브러리 링크
ifeq ($(HAS_PAHO_MQTT),1)
    CXXFLAGS += -DHAS_PAHO_MQTT
    UTILITY_LIBS += -lpaho-mqttpp3 -lpaho-mqtt3a
endif

BASIC_LIBS = -lpthread -lm -ldl -lsqlite3

LDFLAGS = $(SHARED_LIBS) $(UTILITY_LIBS) $(BASIC_LIBS)

# =============================================================================
# 색상
# =============================================================================

GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m

# =============================================================================
# Export Gateway 소스 파일
# =============================================================================

EVENT_SRC_DIR = $(EXPORT_GATEWAY_DIR)/src/Event
EVENT_SOURCES = $(wildcard $(EVENT_SRC_DIR)/*.cpp)
EVENT_OBJECTS = $(EVENT_SOURCES:$(EXPORT_GATEWAY_DIR)/src/%.cpp=$(BUILD_DIR)/%.o)

SCHEDULE_SRC_DIR = $(EXPORT_GATEWAY_DIR)/src/Schedule
SCHEDULE_SOURCES = $(wildcard $(SCHEDULE_SRC_DIR)/*.cpp)
SCHEDULE_OBJECTS = $(SCHEDULE_SOURCES:$(EXPORT_GATEWAY_DIR)/src/%.cpp=$(BUILD_DIR)/%.o)

TRANSFORM_SRC_DIR = $(EXPORT_GATEWAY_DIR)/src/Transform
TRANSFORM_SOURCES = $(wildcard $(TRANSFORM_SRC_DIR)/*.cpp)
TRANSFORM_OBJECTS = $(TRANSFORM_SOURCES:$(EXPORT_GATEWAY_DIR)/src/%.cpp=$(BUILD_DIR)/%.o)

CSP_SRC_DIR = $(EXPORT_GATEWAY_DIR)/src/CSP
CSP_SOURCES = $(wildcard $(CSP_SRC_DIR)/*.cpp)
CSP_OBJECTS = $(CSP_SOURCES:$(EXPORT_GATEWAY_DIR)/src/%.cpp=$(BUILD_DIR)/%.o)

EXPORT_SRC_DIR = $(EXPORT_GATEWAY_DIR)/src/Export
EXPORT_SOURCES = $(wildcard $(EXPORT_SRC_DIR)/*.cpp)
EXPORT_OBJECTS = $(EXPORT_SOURCES:$(EXPORT_GATEWAY_DIR)/src/%.cpp=$(BUILD_DIR)/%.o)

UTILS_SRC_DIR = $(EXPORT_GATEWAY_DIR)/src/Utils
UTILS_SOURCES = $(wildcard $(UTILS_SRC_DIR)/*.cpp)
UTILS_OBJECTS = $(UTILS_SOURCES:$(EXPORT_GATEWAY_DIR)/src/%.cpp=$(BUILD_DIR)/%.o)

ALL_GATEWAY_OBJECTS = $(EVENT_OBJECTS) $(SCHEDULE_OBJECTS) $(TRANSFORM_OBJECTS) $(CSP_OBJECTS) $(EXPORT_OBJECTS) $(UTILS_OBJECTS)

# =============================================================================
# 테스트 바이너리 목록 (존재하는 것만 빌드)
# =============================================================================

TEST_BINARIES = 
ifneq ($(wildcard test_circuit_breaker.cpp),)
    TEST_BINARIES += $(BIN_DIR)/test_circuit_breaker
endif
ifneq ($(wildcard test_http_handler.cpp),)
    TEST_BINARIES += $(BIN_DIR)/test_http_handler
endif
ifneq ($(wildcard test_dynamic_target.cpp),)
    TEST_BINARIES += $(BIN_DIR)/test_dynamic_target
endif
ifneq ($(wildcard test_file_handler.cpp),)
    TEST_BINARIES += $(BIN_DIR)/test_file_handler
endif
ifneq ($(wildcard test_s3_handler.cpp),)
    TEST_BINARIES += $(BIN_DIR)/test_s3_handler
endif
ifneq ($(wildcard test_mqtt_handler.cpp),)
    TEST_BINARIES += $(BIN_DIR)/test_mqtt_handler
endif
ifneq ($(wildcard test_export_modes.cpp),)
    TEST_BINARIES += $(BIN_DIR)/test_export_modes
endif

# =============================================================================
# 타겟
# =============================================================================

.PHONY: all clean test test-circuit test-http test-dynamic test-file test-s3 test-mqtt test-export-modes check-libs help

all: check-libs $(TEST_BINARIES)
	@echo -e "$(GREEN)✅ All available unit tests built$(NC)"

$(BIN_DIR) $(BUILD_DIR):
	@mkdir -p $@
	@mkdir -p $(BUILD_DIR)/Event
	@mkdir -p $(BUILD_DIR)/Schedule
	@mkdir -p $(BUILD_DIR)/Transform
	@mkdir -p $(BUILD_DIR)/CSP
	@mkdir -p $(BUILD_DIR)/Export
	@mkdir -p $(BUILD_DIR)/Utils

# =============================================================================
# 오브젝트 파일 컴파일
# =============================================================================

$(BUILD_DIR)/%.o: $(EXPORT_GATEWAY_DIR)/src/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️  Compiling $<$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 개별 테스트 빌드 규칙
# =============================================================================

$(BIN_DIR)/test_circuit_breaker: test_circuit_breaker.cpp $(ALL_GATEWAY_OBJECTS) | $(BIN_DIR)
	@echo -e "$(BLUE)🔨 Building Circuit Breaker test$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) test_circuit_breaker.cpp $(ALL_GATEWAY_OBJECTS) -o $@ $(LDFLAGS)
	@echo -e "$(GREEN)✅ Built: $@$(NC)"

$(BIN_DIR)/test_http_handler: test_http_handler.cpp $(ALL_GATEWAY_OBJECTS) | $(BIN_DIR)
	@echo -e "$(BLUE)🔨 Building HTTP Handler test$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) test_http_handler.cpp $(ALL_GATEWAY_OBJECTS) -o $@ $(LDFLAGS)
	@echo -e "$(GREEN)✅ Built: $@$(NC)"

$(BIN_DIR)/test_dynamic_target: test_dynamic_target.cpp $(ALL_GATEWAY_OBJECTS) | $(BIN_DIR)
	@echo -e "$(BLUE)🔨 Building DynamicTargetManager test$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) test_dynamic_target.cpp $(ALL_GATEWAY_OBJECTS) -o $@ $(LDFLAGS)
	@echo -e "$(GREEN)✅ Built: $@$(NC)"

$(BIN_DIR)/test_file_handler: test_file_handler.cpp $(ALL_GATEWAY_OBJECTS) | $(BIN_DIR)
	@echo -e "$(BLUE)🔨 Building FileTargetHandler test$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) test_file_handler.cpp $(ALL_GATEWAY_OBJECTS) -o $@ $(LDFLAGS)
	@echo -e "$(GREEN)✅ Built: $@$(NC)"

$(BIN_DIR)/test_s3_handler: test_s3_handler.cpp $(ALL_GATEWAY_OBJECTS) | $(BIN_DIR)
	@echo -e "$(BLUE)🔨 Building S3TargetHandler test$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) test_s3_handler.cpp $(ALL_GATEWAY_OBJECTS) -o $@ $(LDFLAGS)
	@echo -e "$(GREEN)✅ Built: $@$(NC)"

$(BIN_DIR)/test_mqtt_handler: test_mqtt_handler.cpp $(ALL_GATEWAY_OBJECTS) | $(BIN_DIR)
	@echo -e "$(BLUE)🔨 Building MqttTargetHandler test (Paho MQTT: $(HAS_PAHO_MQTT))$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) test_mqtt_handler.cpp $(ALL_GATEWAY_OBJECTS) -o $@ $(LDFLAGS)
	@echo -e "$(GREEN)✅ Built: $@$(NC)"

$(BIN_DIR)/test_export_modes: test_export_modes.cpp $(ALL_GATEWAY_OBJECTS) | $(BIN_DIR)
	@echo -e "$(BLUE)🔨 Building Export Modes test$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) test_export_modes.cpp $(ALL_GATEWAY_OBJECTS) -o $@ $(LDFLAGS)
	@echo -e "$(GREEN)✅ Built: $@$(NC)"

# =============================================================================
# 개별 테스트 실행 규칙 (명시적 경로)
# =============================================================================

test-circuit: $(BIN_DIR)/test_circuit_breaker
	@echo -e "$(BLUE)🧪 Running Circuit Breaker tests$(NC)"
	@$(BIN_DIR)/test_circuit_breaker

test-http: $(BIN_DIR)/test_http_handler
	@echo -e "$(BLUE)🧪 Running HTTP Handler tests$(NC)"
	@$(BIN_DIR)/test_http_handler

test-dynamic: $(BIN_DIR)/test_dynamic_target
	@echo -e "$(BLUE)🧪 Running DynamicTargetManager tests$(NC)"
	@$(BIN_DIR)/test_dynamic_target

test-file: $(BIN_DIR)/test_file_handler
	@echo -e "$(BLUE)🧪 Running FileTargetHandler tests$(NC)"
	@$(BIN_DIR)/test_file_handler

test-s3: $(BIN_DIR)/test_s3_handler
	@echo -e "$(BLUE)🧪 Running S3TargetHandler tests$(NC)"
	@$(BIN_DIR)/test_s3_handler

test-mqtt: $(BIN_DIR)/test_mqtt_handler
	@echo -e "$(BLUE)🧪 Running MqttTargetHandler tests$(NC)"
	@$(BIN_DIR)/test_mqtt_handler

test-export-modes: $(BIN_DIR)/test_export_modes
	@echo -e "$(BLUE)🧪 Running Export Modes tests$(NC)"
	@$(BIN_DIR)/test_export_modes

# =============================================================================
# 전체 테스트 실행
# =============================================================================

test: all
	@echo -e "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo -e "$(BLUE)🧪 Running ALL Unit Tests$(NC)"
	@echo -e "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo ""
	@[ -f $(BIN_DIR)/test_circuit_breaker ] && { echo -e "$(YELLOW)▶ Circuit Breaker Tests$(NC)"; $(BIN_DIR)/test_circuit_breaker || true; echo ""; } || true
	@[ -f $(BIN_DIR)/test_http_handler ] && { echo -e "$(YELLOW)▶ HTTP Handler Tests$(NC)"; $(BIN_DIR)/test_http_handler || true; echo ""; } || true
	@[ -f $(BIN_DIR)/test_dynamic_target ] && { echo -e "$(YELLOW)▶ DynamicTargetManager Tests$(NC)"; $(BIN_DIR)/test_dynamic_target || true; echo ""; } || true
	@[ -f $(BIN_DIR)/test_file_handler ] && { echo -e "$(YELLOW)▶ FileTargetHandler Tests$(NC)"; $(BIN_DIR)/test_file_handler || true; echo ""; } || true
	@[ -f $(BIN_DIR)/test_s3_handler ] && { echo -e "$(YELLOW)▶ S3TargetHandler Tests$(NC)"; $(BIN_DIR)/test_s3_handler || true; echo ""; } || true
	@[ -f $(BIN_DIR)/test_mqtt_handler ] && { echo -e "$(YELLOW)▶ MqttTargetHandler Tests$(NC)"; $(BIN_DIR)/test_mqtt_handler || true; echo ""; } || true
	@[ -f $(BIN_DIR)/test_export_modes ] && { echo -e "$(YELLOW)▶ Export Modes Tests$(NC)"; $(BIN_DIR)/test_export_modes || true; echo ""; } || true
	@echo -e "$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo -e "$(GREEN)✅ All tests completed$(NC)"

# =============================================================================
# 라이브러리 확인
# =============================================================================

check-libs:
	@echo -e "$(BLUE)🔍 Checking PulseOne Shared Libraries$(NC)"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "Shared lib directory: $(SHARED_LIB_DIR)"
	@echo "HAS_SHARED: $(HAS_SHARED)"
	@echo "HAS_HIREDIS: $(HAS_HIREDIS)"
	@echo "HAS_CURL: $(HAS_CURL)"
	@echo "HAS_OPENSSL: $(HAS_OPENSSL)"
	@echo "HAS_PAHO_MQTT: $(HAS_PAHO_MQTT)"
	@echo ""
ifeq ($(HAS_SHARED),1)
	@echo "Shared libraries:"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-security.a" ] && echo "  libpulseone-security: ✅" || echo "  libpulseone-security: ❌"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-common.a" ] && echo "  libpulseone-common: ✅" || echo "  libpulseone-common: ❌"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-database.a" ] && echo "  libpulseone-database: ✅" || echo "  libpulseone-database: ❌"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-client.a" ] && echo "  libpulseone-client: ✅" || echo "  libpulseone-client: ❌"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-data.a" ] && echo "  libpulseone-data: ✅" || echo "  libpulseone-data: ❌"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-export.a" ] && echo "  libpulseone-export: ✅" || echo "  libpulseone-export: ❌"
	@[ -f "$(SHARED_LIB_DIR)/libpulseone-alarm.a" ] && echo "  libpulseone-alarm: ✅" || echo "  libpulseone-alarm: ❌"
	@echo -e "$(GREEN)✅ Shared libraries available$(NC)"
else
	@echo -e "$(RED)❌ Shared libraries not found!$(NC)"
	@echo -e "$(YELLOW)Build them first: cd $(SHARED_DIR) && make$(NC)"
endif
ifeq ($(HAS_PAHO_MQTT),1)
	@echo -e "$(GREEN)✅ Paho MQTT C++ available$(NC)"
else
	@echo -e "$(YELLOW)⚠️  Paho MQTT not found - MQTT tests will run in Mock mode$(NC)"
	@echo -e "$(YELLOW)   Install: apt install libpaho-mqtt-dev libpaho-mqttpp-dev$(NC)"
endif
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# =============================================================================
# 정리
# =============================================================================

clean:
	@echo -e "$(YELLOW)🧹 Cleaning unit tests$(NC)"
	@rm -rf $(BUILD_DIR)
	@rm -f $(BIN_DIR)/test_*
	@echo -e "$(GREEN)✅ Clean completed$(NC)"

# =============================================================================
# 도움말
# =============================================================================

help:
	@echo "Export Gateway Unit Tests (Paho MQTT C++ Support)"
	@echo ""
	@echo "Commands:"
	@echo "  make                 - Build all available tests"
	@echo "  make test            - Run all available tests"
	@echo "  make test-circuit    - Run Circuit Breaker tests"
	@echo "  make test-http       - Run HTTP Handler tests"
	@echo "  make test-dynamic    - Run DynamicTargetManager tests"
	@echo "  make test-file       - Run FileTargetHandler tests"
	@echo "  make test-s3         - Run S3TargetHandler tests"
	@echo "  make test-mqtt       - Run MqttTargetHandler tests"
	@echo "  make test-export-modes - Run Export Modes tests"
	@echo "  make check-libs      - Check shared libraries"
	@echo "  make clean           - Clean build files"
	@echo "  make help            - Show this help"
	@echo ""
	@echo "Library Status:"
	@echo "  HAS_SHARED: $(HAS_SHARED)"
	@echo "  HAS_HIREDIS: $(HAS_HIREDIS)"
	@echo "  HAS_CURL: $(HAS_CURL)"
	@echo "  HAS_OPENSSL: $(HAS_OPENSSL)"
	@echo "  HAS_PAHO_MQTT: $(HAS_PAHO_MQTT)"
	@echo ""
	@echo "Available Tests:"
	@[ -f test_circuit_breaker.cpp ] && echo "  ✅ test_circuit_breaker.cpp" || echo "  ❌ test_circuit_breaker.cpp"
	@[ -f test_http_handler.cpp ] && echo "  ✅ test_http_handler.cpp" || echo "  ❌ test_http_handler.cpp"
	@[ -f test_dynamic_target.cpp ] && echo "  ✅ test_dynamic_target.cpp" || echo "  ❌ test_dynamic_target.cpp"
	@[ -f test_file_handler.cpp ] && echo "  ✅ test_file_handler.cpp" || echo "  ❌ test_file_handler.cpp"
	@[ -f test_s3_handler.cpp ] && echo "  ✅ test_s3_handler.cpp" || echo "  ❌ test_s3_handler.cpp"
	@[ -f test_mqtt_handler.cpp ] && echo "  ✅ test_mqtt_handler.cpp" || echo "  ❌ test_mqtt_handler.cpp"
	@[ -f test_export_modes.cpp ] && echo "  ✅ test_export_modes.cpp" || echo "  ❌ test_export_modes.cpp"