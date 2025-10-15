# =============================================================================
# PulseOne Collector - 완전한 Makefile (링크 에러 수정 완료)
# =============================================================================

# 기본 설정
SHELL := /bin/bash
.DEFAULT_GOAL := help
MAKEFLAGS += --warn-undefined-variables --no-builtin-rules

# 플랫폼 감지
UNAME_S := $(shell uname -s)
NPROC := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += -j$(NPROC)

# 플랫폼별 설정
ifeq ($(UNAME_S),Linux)
	PLATFORM := Linux
	IS_WINDOWS := 0
	IS_LINUX := 1
else ifeq ($(OS),Windows_NT)
	PLATFORM := Windows
	IS_WINDOWS := 1
	IS_LINUX := 0
else
	PLATFORM := $(UNAME_S)
	IS_WINDOWS := 0
	IS_LINUX := 0
endif

# Windows 크로스 컴파일 지원
ifeq ($(CROSS_COMPILE_WINDOWS),1)
	PLATFORM := Windows-Cross
	IS_WINDOWS := 1
	IS_LINUX := 0
	CXX := x86_64-w64-mingw32-g++
	MINGW_PREFIX := /usr/x86_64-w64-mingw32
endif

# =============================================================================
# 디렉토리 설정
# =============================================================================

SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build
BIN_DIR := bin
DIST_DIR := dist
TARGET := pulseone-collector

# Shared 라이브러리 경로
SHARED_BASE_DIR := /app/core/shared
SHARED_INCLUDE_DIR := $(SHARED_BASE_DIR)/include
SHARED_LIB_DIR := $(SHARED_BASE_DIR)/lib

# =============================================================================
# 컴파일러 및 플래그 설정
# =============================================================================

# 기본 컴파일러
ifeq ($(CROSS_COMPILE_WINDOWS),1)
	CXX := x86_64-w64-mingw32-g++
else
	CXX := g++
endif

# 공통 컴파일 플래그
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -fPIC
CXXFLAGS += -Wno-unused-parameter -Wno-unused-variable
CXXFLAGS += -fdiagnostics-color=always

# 플랫폼별 플래그
ifeq ($(IS_WINDOWS),1)
	CXXFLAGS += -DPULSEONE_WINDOWS=1 -DPULSEONE_LINUX=0
	CXXFLAGS += -DWIN32 -D_WIN32_WINNT=0x0601
	CXXFLAGS += -DWIN32_LEAN_AND_MEAN -DNOMINMAX
	ifeq ($(CROSS_COMPILE_WINDOWS),1)
		LDFLAGS := -L$(MINGW_PREFIX)/lib -static
	else
		LDFLAGS := -Wl,--gc-sections -static
	endif
else
	CXXFLAGS += -DPULSEONE_WINDOWS=0 -DPULSEONE_LINUX=1
	CXXFLAGS += -march=native -mtune=native
	LDFLAGS := -Wl,--gc-sections
endif

# 디버그/릴리즈 플래그
ifeq ($(DEBUG),1)
	CXXFLAGS += -DDEBUG -g -O0 -fno-omit-frame-pointer
else
	CXXFLAGS += -DNDEBUG -O3 -flto
endif

# =============================================================================
# Include 경로 설정
# =============================================================================

# Shared 라이브러리 Include 확인
HAS_SHARED := $(shell [ -d "$(SHARED_INCLUDE_DIR)" ] && echo "1" || echo "0")

# 기본 Include 경로
INCLUDES := -I$(INCLUDE_DIR)
INCLUDES += -I$(INCLUDE_DIR)/Platform
INCLUDES += -I$(INCLUDE_DIR)/Common
INCLUDES += -I$(INCLUDE_DIR)/Utils
INCLUDES += -I$(INCLUDE_DIR)/Config
INCLUDES += -I$(INCLUDE_DIR)/Database
INCLUDES += -I$(INCLUDE_DIR)/Database/Entities
INCLUDES += -I$(INCLUDE_DIR)/Database/Repositories
INCLUDES += -I$(INCLUDE_DIR)/Client
INCLUDES += -I$(INCLUDE_DIR)/Workers
INCLUDES += -I$(INCLUDE_DIR)/Workers/Base
INCLUDES += -I$(INCLUDE_DIR)/Workers/Protocol
INCLUDES += -I$(INCLUDE_DIR)/Workers/Components
INCLUDES += -I$(INCLUDE_DIR)/Drivers
INCLUDES += -I$(INCLUDE_DIR)/Drivers/Common
INCLUDES += -I$(INCLUDE_DIR)/Pipeline
INCLUDES += -I$(INCLUDE_DIR)/Storage
INCLUDES += -I$(INCLUDE_DIR)/Alarm
INCLUDES += -I$(INCLUDE_DIR)/VirtualPoint
INCLUDES += -I$(INCLUDE_DIR)/Network
INCLUDES += -I$(INCLUDE_DIR)/Api

# Shared 라이브러리 Include (우선순위 높음)
ifeq ($(HAS_SHARED),1)
	INCLUDES := -I$(SHARED_INCLUDE_DIR) $(INCLUDES)
	INCLUDES += -I$(SHARED_INCLUDE_DIR)/Platform
	INCLUDES += -I$(SHARED_INCLUDE_DIR)/Common
	INCLUDES += -I$(SHARED_INCLUDE_DIR)/Utils
	INCLUDES += -I$(SHARED_INCLUDE_DIR)/Database
	INCLUDES += -I$(SHARED_INCLUDE_DIR)/Database/Entities
	INCLUDES += -I$(SHARED_INCLUDE_DIR)/Database/Repositories
	INCLUDES += -I$(SHARED_INCLUDE_DIR)/Alarm
	INCLUDES += -I$(SHARED_INCLUDE_DIR)/Export
endif

# 외부 라이브러리 Include
INCLUDES += -I/usr/local/include
INCLUDES += -I/usr/local/include/quickjs

# Windows 크로스 컴파일용 Include
ifeq ($(CROSS_COMPILE_WINDOWS),1)
	INCLUDES += -I$(MINGW_PREFIX)/include
endif

# =============================================================================
# 라이브러리 감지 및 설정
# =============================================================================

# 동적 라이브러리 감지
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HTTPLIB := $(shell echo '\#include <httplib.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_QUICKJS := $(shell pkg-config --exists quickjs && echo "1" || echo "0")

# 프로토콜 라이브러리 감지
HAS_MODBUS := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lmodbus >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_C := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3c >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_CPP := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqttpp3 >/dev/null 2>&1 && echo "1" || echo "0")
HAS_BACNET := $(shell [ -f "/usr/local/lib/libbacnet.a" ] && echo "1" || echo "0")

# =============================================================================
# 라이브러리 링킹 설정 (✅ 수정: security 추가)
# =============================================================================

# 기본 시스템 라이브러리
ifeq ($(IS_WINDOWS),1)
	BASIC_LIBS := -lpthread -lstdc++ -lm
	PLATFORM_LIBS := -lws2_32 -lwsock32 -liphlpapi -lrpcrt4 -luuid
	PLATFORM_LIBS += -lwinmm -lkernel32 -luser32 -ladvapi32
else
	BASIC_LIBS := -lpthread -lsqlite3 -lm -ldl
	PLATFORM_LIBS :=
endif

# ✅ Shared 라이브러리 (security를 common 앞에 추가)
ifeq ($(HAS_SHARED),1)
	SHARED_LIBS := -L$(SHARED_LIB_DIR)
	SHARED_LIBS += -lpulseone-alarm
	SHARED_LIBS += -lpulseone-database
	SHARED_LIBS += -lpulseone-client
	SHARED_LIBS += -lpulseone-export
	SHARED_LIBS += -lpulseone-security
	SHARED_LIBS += -lpulseone-common
	CXXFLAGS += -DHAS_SHARED_LIBS=1
else
	SHARED_LIBS :=
endif

# 프로토콜 라이브러리
PROTOCOL_LIBS :=
ifeq ($(HAS_MODBUS),1)
	PROTOCOL_LIBS += -lmodbus
	CXXFLAGS += -DHAVE_MODBUS=1
endif

ifeq ($(HAS_MQTT_C),1)
	PROTOCOL_LIBS += -lpaho-mqtt3c
	CXXFLAGS += -DHAVE_MQTT=1
endif

ifeq ($(HAS_MQTT_CPP),1)
	PROTOCOL_LIBS += -lpaho-mqttpp3
	CXXFLAGS += -DHAVE_MQTT_CPP=1
endif

ifeq ($(HAS_BACNET),1)
	PROTOCOL_LIBS += -lbacnet
	CXXFLAGS += -DHAVE_BACNET=1
endif

# 유틸리티 라이브러리
UTILITY_LIBS :=
ifeq ($(HAS_HIREDIS),1)
	UTILITY_LIBS += -lhiredis
	CXXFLAGS += -DHAVE_REDIS=1
endif

ifeq ($(HAS_HTTPLIB),1)
	CXXFLAGS += -DHAVE_HTTPLIB=1
endif

ifeq ($(HAS_QUICKJS),1)
	# QuickJS 라이브러리 위치 자동 감지
	QUICKJS_LIB_PATH := $(shell find /usr/local/lib -name "libquickjs.a" 2>/dev/null | head -1)
	ifneq ($(QUICKJS_LIB_PATH),)
		UTILITY_LIBS += $(QUICKJS_LIB_PATH)
	else
		# pkg-config 시도
		QUICKJS_LIBS := $(shell pkg-config --libs quickjs 2>/dev/null || echo "-lquickjs")
		UTILITY_LIBS += $(QUICKJS_LIBS)
	endif
	CXXFLAGS += -DHAVE_QUICKJS=1
endif

# 최종 라이브러리 링킹 순서
LIBS := $(SHARED_LIBS) $(PROTOCOL_LIBS) $(UTILITY_LIBS) $(BASIC_LIBS) $(PLATFORM_LIBS)

# =============================================================================
# 소스 파일 수집 (✅ 수정: ConfigManager 제외)
# =============================================================================

# Core 소스
CORE_SOURCES := $(wildcard $(SRC_DIR)/Core/*.cpp)

# ✅ Utils 소스 (ConfigManager.cpp 제외 - shared에서 사용)
UTILS_SOURCES := $(filter-out $(SRC_DIR)/Utils/ConfigManager.cpp, $(wildcard $(SRC_DIR)/Utils/*.cpp))

CONFIG_SOURCES := $(wildcard $(SRC_DIR)/Config/*.cpp)
PLATFORM_SOURCES := $(wildcard $(SRC_DIR)/Platform/*.cpp)

# Database 소스
DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp)
DATABASE_SOURCES += $(wildcard $(SRC_DIR)/Database/Entities/*.cpp)
DATABASE_SOURCES += $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)

# Client 소스
CLIENT_SOURCES := $(wildcard $(SRC_DIR)/Client/*.cpp)

# Workers 소스
WORKERS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp)
WORKERS_SOURCES += $(wildcard $(SRC_DIR)/Workers/Protocol/*.cpp)
WORKERS_SOURCES += $(wildcard $(SRC_DIR)/Workers/Components/*.cpp)
WORKERS_SOURCES += $(wildcard $(SRC_DIR)/Workers/WorkerFactory.cpp)
WORKERS_SOURCES += $(wildcard $(SRC_DIR)/Workers/WorkerManager.cpp)

# Pipeline 및 Storage
PIPELINE_SOURCES := $(wildcard $(SRC_DIR)/Pipeline/*.cpp)
STORAGE_SOURCES := $(wildcard $(SRC_DIR)/Storage/*.cpp)

# Alarm 및 VirtualPoint
ALARM_SOURCES := $(wildcard $(SRC_DIR)/Alarm/*.cpp)
VIRTUALPOINT_SOURCES := $(wildcard $(SRC_DIR)/VirtualPoint/*.cpp)

# 드라이버 소스 (조건부 포함)
DRIVERS_SOURCES := $(wildcard $(SRC_DIR)/Drivers/Common/*.cpp)

ifeq ($(HAS_MODBUS),1)
	DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Modbus/*.cpp)
endif

ifeq ($(HAS_MQTT_CPP),1)
	DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Mqtt/*.cpp)
endif

ifeq ($(HAS_BACNET),1)
	DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Bacnet/*.cpp)
endif

# 네트워크 및 API (조건부 포함)
ifeq ($(HAS_HTTPLIB),1)
	NETWORK_SOURCES := $(wildcard $(SRC_DIR)/Network/*.cpp)
	API_SOURCES := $(wildcard $(SRC_DIR)/Api/*.cpp)
else
	NETWORK_SOURCES :=
	API_SOURCES :=
endif

# 전체 소스 파일 목록
ALL_SOURCES := $(CORE_SOURCES) $(UTILS_SOURCES) $(CONFIG_SOURCES)
ALL_SOURCES += $(DATABASE_SOURCES) $(CLIENT_SOURCES) $(WORKERS_SOURCES)
ALL_SOURCES += $(DRIVERS_SOURCES) $(PIPELINE_SOURCES) $(STORAGE_SOURCES)
ALL_SOURCES += $(ALARM_SOURCES) $(VIRTUALPOINT_SOURCES) $(PLATFORM_SOURCES)
ALL_SOURCES += $(NETWORK_SOURCES) $(API_SOURCES)

# 오브젝트 파일
ALL_OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ALL_SOURCES))
MAIN_OBJECT := $(BUILD_DIR)/main.o

# =============================================================================
# 빌드 디렉토리 자동 생성
# =============================================================================

BUILD_SUBDIRS := $(BUILD_DIR)/Core $(BUILD_DIR)/Utils $(BUILD_DIR)/Config
BUILD_SUBDIRS += $(BUILD_DIR)/Database $(BUILD_DIR)/Database/Entities $(BUILD_DIR)/Database/Repositories
BUILD_SUBDIRS += $(BUILD_DIR)/Client $(BUILD_DIR)/Workers $(BUILD_DIR)/Workers/Base
BUILD_SUBDIRS += $(BUILD_DIR)/Workers/Protocol $(BUILD_DIR)/Workers/Components
BUILD_SUBDIRS += $(BUILD_DIR)/Drivers $(BUILD_DIR)/Drivers/Common
BUILD_SUBDIRS += $(BUILD_DIR)/Drivers/Modbus $(BUILD_DIR)/Drivers/Mqtt $(BUILD_DIR)/Drivers/Bacnet
BUILD_SUBDIRS += $(BUILD_DIR)/Pipeline $(BUILD_DIR)/Storage $(BUILD_DIR)/Alarm
BUILD_SUBDIRS += $(BUILD_DIR)/VirtualPoint $(BUILD_DIR)/Network $(BUILD_DIR)/Api $(BUILD_DIR)/Platform

# =============================================================================
# 색상 정의
# =============================================================================

GREEN := \033[0;32m
BLUE := \033[0;34m
YELLOW := \033[1;33m
RED := \033[0;31m
CYAN := \033[0;36m
NC := \033[0m

# =============================================================================
# 메인 타겟들
# =============================================================================

# 전체 빌드
all: check-env create-dirs $(TARGET)
	@echo -e "$(GREEN)✅ Build completed for $(PLATFORM)!$(NC)"
	@echo -e "Binary: $(CYAN)$(BIN_DIR)/$(TARGET)$(NC)"
	@echo -e "Shared libraries: $(if $(HAS_SHARED),$(GREEN)✅ enabled$(NC),$(YELLOW)❌ disabled$(NC))"
	@echo -e "Protocol libraries: $(CYAN)$(words $(shell echo $(PROTOCOL_LIBS) | tr ' ' '\n' | sort -u))$(NC) protocols"

# 디버그 빌드
debug:
	$(MAKE) DEBUG=1 all

# 릴리즈 빌드
release:
	$(MAKE) DEBUG=0 all

# Windows 크로스 컴파일
windows-cross:
	@echo -e "$(YELLOW)🔧 Building for Windows (cross-compile)...$(NC)"
	$(MAKE) CROSS_COMPILE_WINDOWS=1 all

# =============================================================================
# 디렉토리 생성
# =============================================================================

create-dirs: | $(BUILD_DIR) $(BIN_DIR) $(DIST_DIR)
	@mkdir -p $(BUILD_SUBDIRS)

$(BUILD_DIR) $(BIN_DIR) $(DIST_DIR):
	@mkdir -p $@

# =============================================================================
# 컴파일 규칙
# =============================================================================

# 메인 실행 파일
$(TARGET): $(ALL_OBJECTS) $(MAIN_OBJECT) | $(BIN_DIR)
	@echo -e "$(BLUE)🔗 Linking $(TARGET)...$(NC)"
	$(CXX) $(LDFLAGS) -o $(BIN_DIR)/$@ $(ALL_OBJECTS) $(MAIN_OBJECT) $(LIBS)
	@echo -e "$(GREEN)✅ $(TARGET) built successfully!$(NC)"
	@echo -e "Size: $(CYAN)$(shell du -h $(BIN_DIR)/$(TARGET) 2>/dev/null | cut -f1 || echo 'N/A')$(NC)"

# 오브젝트 파일 컴파일
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | create-dirs
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️ Compiling $<$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# main.cpp 특별 처리
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp | create-dirs
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)⚙️ Compiling main.cpp$(NC)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 의존성 생성
# =============================================================================

# 의존성 파일 생성
$(BUILD_DIR)/%.d: $(SRC_DIR)/%.cpp | create-dirs
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(BUILD_DIR)/$*.o $< > $@

# 의존성 파일 포함
-include $(ALL_OBJECTS:.o=.d)

# =============================================================================
# 유틸리티 타겟들
# =============================================================================

# 환경 확인
check-env:
	@echo -e "$(BLUE)🔍 Environment check for $(PLATFORM)...$(NC)"
	@echo -e "Platform: $(CYAN)$(PLATFORM)$(NC)"
	@echo -e "Compiler: $(CYAN)$(CXX)$(NC)"
	@echo -e "Shared libs: $(if $(HAS_SHARED),$(GREEN)✅ $(SHARED_LIB_DIR)$(NC),$(YELLOW)❌ disabled$(NC))"
	@echo -e "Sources: $(CYAN)$(words $(ALL_SOURCES))$(NC) files"
	@echo -e "Protocol support:"
	@echo -e "  Modbus: $(if $(HAS_MODBUS),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  MQTT: $(if $(HAS_MQTT_C),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  MQTT++: $(if $(HAS_MQTT_CPP),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  BACnet: $(if $(HAS_BACNET),$(GREEN)✅$(NC),$(RED)❌$(NC))"
	@echo -e "  QuickJS: $(if $(HAS_QUICKJS),$(GREEN)✅$(NC),$(RED)❌$(NC))"

# 빌드 상태 확인
status:
	@echo -e "$(BLUE)📊 Build Status$(NC)"
	@echo -e "Platform: $(CYAN)$(PLATFORM)$(NC)"
	@echo -e "Target: $(if $(wildcard $(BIN_DIR)/$(TARGET)),$(GREEN)✅ $(BIN_DIR)/$(TARGET)$(NC),$(RED)❌ not built$(NC))"
	@echo -e "Objects: $(CYAN)$(words $(wildcard $(BUILD_DIR)/**/*.o))$(NC) / $(CYAN)$(words $(ALL_OBJECTS))$(NC)"

# 실행
run: $(TARGET)
	@echo -e "$(GREEN)🚀 Running $(TARGET)...$(NC)"
	$(BIN_DIR)/$(TARGET)

# 정리
clean:
	@echo -e "$(YELLOW)🧹 Cleaning build artifacts...$(NC)"
	@find $(BUILD_DIR) -name "*.o" -delete 2>/dev/null || true
	@find $(BUILD_DIR) -name "*.d" -delete 2>/dev/null || true
	@rm -f $(BIN_DIR)/$(TARGET) 2>/dev/null || true
	@echo -e "$(GREEN)✅ Clean completed$(NC)"

# 완전 정리
distclean:
	@echo -e "$(RED)🗑️ Complete cleanup...$(NC)"
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(DIST_DIR)
	@echo -e "$(GREEN)✅ Complete cleanup finished$(NC)"

# TCP 소켓 오류 수정
fix-tcp:
	@echo -e "$(YELLOW)🔧 Fixing TCP socket issues...$(NC)"
	@if [ -f "src/Workers/Base/TcpBasedWorker.cpp" ]; then \
		if ! grep -q "netinet/tcp.h" src/Workers/Base/TcpBasedWorker.cpp; then \
			sed -i '1i#include <netinet/tcp.h>' src/Workers/Base/TcpBasedWorker.cpp; \
			echo -e "$(GREEN)✅ Added TCP header$(NC)"; \
		fi; \
		sed -i 's/case EWOULDBLOCK: return "Operation would block";/#if EAGAIN != EWOULDBLOCK\n        case EWOULDBLOCK: return "Operation would block";\n#endif/' src/Workers/Base/TcpBasedWorker.cpp; \
		echo -e "$(GREEN)✅ Fixed EWOULDBLOCK duplicate case$(NC)"; \
	else \
		echo -e "$(YELLOW)⚠️ TcpBasedWorker.cpp not found$(NC)"; \
	fi

# 도움말
help:
	@echo -e "$(BLUE)🔧 PulseOne Collector Build System$(NC)"
	@echo ""
	@echo -e "$(GREEN)Building:$(NC)"
	@echo "  make all              - Build with all available libraries"
	@echo "  make debug            - Debug build (-g -O0)"
	@echo "  make release          - Release build (-O3 -flto)"
	@echo "  make windows-cross    - Cross-compile for Windows"
	@echo "  make run              - Build and run"
	@echo ""
	@echo -e "$(GREEN)Maintenance:$(NC)"
	@echo "  make clean            - Clean build artifacts"
	@echo "  make distclean        - Complete cleanup"
	@echo "  make fix-tcp          - Fix TCP socket compilation issues"
	@echo ""
	@echo -e "$(GREEN)Utilities:$(NC)"
	@echo "  make check-env        - Check build environment"
	@echo "  make status           - Show build status"
	@echo "  make create-dirs      - Create build directories"
	@echo ""
	@echo -e "$(YELLOW)Current Configuration:$(NC)"
	@echo "  Platform: $(PLATFORM)"
	@echo "  Shared Libraries: $(if $(HAS_SHARED),$(GREEN)✅ enabled$(NC),$(YELLOW)❌ disabled$(NC))"
	@echo "  Cross Compile: $(if $(CROSS_COMPILE_WINDOWS),$(GREEN)✅ Windows$(NC),$(YELLOW)❌ Native$(NC))"

# =============================================================================
# Phony 타겟들
# =============================================================================

.PHONY: all debug release windows-cross clean distclean check-env status run fix-tcp help create-dirs