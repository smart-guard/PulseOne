# =============================================================================
# PulseOne Collector 통합 Makefile - Shared 라이브러리 지원 + 자동 디렉토리 생성
# =============================================================================

# 플랫폼 및 성능 설정
NPROC := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += -j$(NPROC) --output-sync=target

# 플랫폼 감지
UNAME := $(shell uname -s)
ifeq ($(UNAME),Linux)
    PLATFORM := Linux
    IS_WINDOWS := 0
    IS_LINUX := 1
else ifeq ($(OS),Windows_NT)
    PLATFORM := Windows
    IS_WINDOWS := 1
    IS_LINUX := 0
else
    PLATFORM := $(UNAME)
    IS_WINDOWS := 0
    IS_LINUX := 0
endif

# Windows 크로스 컴파일 감지
ifeq ($(CROSS_COMPILE_WINDOWS),1)
    PLATFORM := Windows-Cross
    IS_WINDOWS := 1
    IS_LINUX := 0
    CXX = x86_64-w64-mingw32-g++
    MINGW_PREFIX = /usr/x86_64-w64-mingw32
endif

# 동적 라이브러리 감지
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_C := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3c >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_AS := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3as >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_CPP := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqttpp3 >/dev/null 2>&1 && echo "1" || echo "0")
HAS_BACNET_STACK := $(shell [ -f "/usr/local/lib/libbacnet.a" ] && echo "1" || echo "0")
HAS_QUICKJS := $(shell pkg-config --exists quickjs && echo "1" || echo "0")
HAS_HTTPLIB := $(shell echo '\#include <httplib.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_CURL := $(shell pkg-config --exists libcurl && echo "1" || echo "0")

# 컴파일러 설정
ifeq ($(CROSS_COMPILE_WINDOWS),1)
    CXX = x86_64-w64-mingw32-g++
    CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -DWIN32 -D_WIN32_WINNT=0x0601 \
               -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DUNICODE -D_UNICODE
else
    CXX = g++
    CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -DNDEBUG -fPIC \
               -Wno-unused-but-set-variable -Wno-unused-variable \
               -Wno-unused-parameter -fdiagnostics-color=always \
               -march=native -mtune=native -flto -fuse-linker-plugin
endif

# 플랫폼별 설정
ifeq ($(IS_WINDOWS),1)
    CXXFLAGS += -DPULSEONE_WINDOWS=1 -DPULSEONE_LINUX=0
    ifeq ($(CROSS_COMPILE_WINDOWS),1)
        PLATFORM_LIBS = -lws2_32 -lwsock32 -liphlpapi -lrpcrt4 -luuid -lwinmm \
                       -lkernel32 -luser32 -ladvapi32 -lshell32 -lole32 -loleaut32 \
                       -lcrypt32 -lsecur32 -lbcrypt -lncrypt
        LDFLAGS = -L$(MINGW_PREFIX)/lib -static
    else
        CXXFLAGS += -D_WIN32_WINNT=0x0A00 -DWIN32_LEAN_AND_MEAN -DNOMINMAX \
                    -static-libgcc -static-libstdc++
        PLATFORM_LIBS = -lws2_32 -liphlpapi -lkernel32 -luser32
        LDFLAGS = -Wl,--gc-sections -static
    endif
else
    CXXFLAGS += -DPULSEONE_WINDOWS=0 -DPULSEONE_LINUX=1
    PLATFORM_LIBS = 
    LDFLAGS = -Wl,--gc-sections
endif

# 경로 설정
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin
DIST_DIR = dist
TARGET = pulseone-collector

# =============================================================================
# 🔥 Shared 라이브러리 통합 설정
# =============================================================================

# Shared 라이브러리 경로
SHARED_BASE_DIR = /app/core/shared
SHARED_INCLUDE_DIR = $(SHARED_BASE_DIR)/include
SHARED_LIB_DIR = $(SHARED_BASE_DIR)/lib

# Shared 라이브러리 존재 확인
HAS_SHARED := $(shell [ -d "$(SHARED_LIB_DIR)" ] && [ -f "$(SHARED_LIB_DIR)/libpulseone-common.a" ] && echo "1" || echo "0")

# Shared 라이브러리들 (의존성 순서대로)
ifeq ($(HAS_SHARED),1)
    SHARED_LIBS = -L$(SHARED_LIB_DIR) \
                  -lpulseone-alarm \
                  -lpulseone-database \
                  -lpulseone-client \
                  -lpulseone-export \
                  -lpulseone-common
    CXXFLAGS += -DHAS_SHARED_LIBS=1
    
    # Shared include 경로를 최우선으로 설정
    INCLUDES = -I$(SHARED_INCLUDE_DIR) \ -I/usr/local/include/quickjs
               -I$(SHARED_INCLUDE_DIR)/Platform \
               -I$(SHARED_INCLUDE_DIR)/Common \
               -I$(SHARED_INCLUDE_DIR)/Utils \
               -I$(SHARED_INCLUDE_DIR)/Database \
               -I$(SHARED_INCLUDE_DIR)/Database/Entities \
               -I$(SHARED_INCLUDE_DIR)/Database/Repositories \
               -I$(SHARED_INCLUDE_DIR)/Alarm \
               -I$(SHARED_INCLUDE_DIR)/Export \
               -I$(INCLUDE_DIR) \
               -I$(INCLUDE_DIR)/Api \
               -I$(INCLUDE_DIR)/VirtualPoint \
               -I$(INCLUDE_DIR)/Network \
               -I$(INCLUDE_DIR)/Workers \
               -I$(INCLUDE_DIR)/Workers/Base \
               -I$(INCLUDE_DIR)/Workers/Protocol \
               -I$(INCLUDE_DIR)/Workers/Components \
               -I$(INCLUDE_DIR)/Drivers \
               -I$(INCLUDE_DIR)/Drivers/Common \
               -I$(INCLUDE_DIR)/Pipeline \
               -I$(INCLUDE_DIR)/Storage \
               -I$(INCLUDE_DIR)/Client
else
    SHARED_LIBS = 
    # Shared 없을 때는 기존 include 경로 사용
    INCLUDES = -I$(INCLUDE_DIR) \ -I/usr/local/include/quickjs
               -I$(INCLUDE_DIR)/Platform \
               -I$(INCLUDE_DIR)/Api \
               -I$(INCLUDE_DIR)/Alarm \
               -I$(INCLUDE_DIR)/VirtualPoint \
               -I$(INCLUDE_DIR)/Network \
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
               -I$(INCLUDE_DIR)/Storage \
               -I$(INCLUDE_DIR)/Client
endif

# Windows 크로스 컴파일용 추가 경로
ifeq ($(CROSS_COMPILE_WINDOWS),1)
    INCLUDES += -I$(MINGW_PREFIX)/include -I/usr/local/include/quickjs
endif

# =============================================================================
# 라이브러리 설정 (플랫폼별 + Shared 통합)
# =============================================================================

# 기본 라이브러리
ifeq ($(CROSS_COMPILE_WINDOWS),1)
    BASIC_LIBS = -lpthread -lstdc++ -lm -lgcc -lgcc_eh
else
    BASIC_LIBS = -lpthread -lsqlite3 -lm -ldl $(PLATFORM_LIBS)
endif

# 조건부 라이브러리 (플랫폼별)
OPTIONAL_LIBS = 
ifeq ($(CROSS_COMPILE_WINDOWS),1)
    # Windows 크로스 컴파일용 라이브러리
    ifneq ($(wildcard $(MINGW_PREFIX)/lib/libmodbus.a),)
        OPTIONAL_LIBS += -lmodbus
        CXXFLAGS += -DHAVE_MODBUS=1
    endif
    ifneq ($(wildcard $(MINGW_PREFIX)/lib/libpaho-mqttpp3-static.a),)
        OPTIONAL_LIBS += -lpaho-mqttpp3-static
        CXXFLAGS += -DHAVE_MQTT_CPP=1
    endif
    ifneq ($(wildcard $(MINGW_PREFIX)/lib/libpaho-mqtt3a.a),)
        OPTIONAL_LIBS += -lpaho-mqtt3a
        CXXFLAGS += -DHAVE_MQTT_ASYNC=1
    endif
    ifneq ($(wildcard $(MINGW_PREFIX)/lib/libpaho-mqtt3c.a),)
        OPTIONAL_LIBS += -lpaho-mqtt3c
        CXXFLAGS += -DHAVE_MQTT=1
    endif
    ifneq ($(wildcard $(MINGW_PREFIX)/lib/libbacnet.a),)
        OPTIONAL_LIBS += -lbacnet
        CXXFLAGS += -DHAVE_BACNET=1
    endif
    ifneq ($(wildcard $(MINGW_PREFIX)/lib/libquickjs.a),)
        OPTIONAL_LIBS += -lquickjs
        CXXFLAGS += -DHAVE_QUICKJS=1
    endif
    ifneq ($(wildcard $(MINGW_PREFIX)/lib/libsqlite3.a),)
        OPTIONAL_LIBS += -lsqlite3
    endif
    ifneq ($(wildcard $(MINGW_PREFIX)/lib/libhiredis.a),)
        OPTIONAL_LIBS += -lhiredis
        CXXFLAGS += -DHAVE_REDIS=1
    endif
    ifneq ($(wildcard $(MINGW_PREFIX)/include/httplib.h),)
        CXXFLAGS += -DHAVE_HTTPLIB=1
    endif
else
    # Linux 네이티브용 라이브러리
    ifneq ($(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lmodbus >/dev/null 2>&1; echo $$?),0)
        MODBUS_LIBS = 
    else
        MODBUS_LIBS = -lmodbus
        OPTIONAL_LIBS += $(MODBUS_LIBS)
    endif

    ifeq ($(HAS_MQTT_C),1)
        OPTIONAL_LIBS += -lpaho-mqtt3c
    endif
    ifeq ($(HAS_MQTT_AS),1)
        OPTIONAL_LIBS += -lpaho-mqtt3as
    endif
    ifeq ($(HAS_MQTT_CPP),1)
        OPTIONAL_LIBS += -lpaho-mqttpp3
    endif

    ifeq ($(HAS_HIREDIS),1)
        OPTIONAL_LIBS += -lhiredis -lhiredis_ssl
        CXXFLAGS += -DHAS_HIREDIS
    endif

    ifeq ($(HAS_HTTPLIB),1)
        CXXFLAGS += -DHAVE_HTTPLIB=1
    endif

    ifeq ($(HAS_QUICKJS),1)
        QUICKJS_LIBS = $(shell pkg-config --libs quickjs 2>/dev/null || echo "-lquickjs")
        OPTIONAL_LIBS += $(QUICKJS_LIBS)
        CXXFLAGS += -DHAS_QUICKJS=1
    endif

    ifeq ($(HAS_CURL),1)
        OPTIONAL_LIBS += -lcurl
        CXXFLAGS += -DHAS_CURL=1
    endif
endif

# 🔥 최종 라이브러리 링킹 순서 (중요!)
# Shared 라이브러리 → 프로토콜 라이브러리 → 시스템 라이브러리
LIBS = $(SHARED_LIBS) $(OPTIONAL_LIBS) $(BASIC_LIBS) $(PLATFORM_LIBS)

# =============================================================================
# 소스 파일 수집
# =============================================================================

CORE_SOURCES := $(wildcard $(SRC_DIR)/Core/*.cpp)
UTILS_SOURCES := $(wildcard $(SRC_DIR)/Utils/*.cpp)
CONFIG_SOURCES := $(wildcard $(SRC_DIR)/Config/*.cpp)
DATABASE_SOURCES := $(wildcard $(SRC_DIR)/Database/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Entities/*.cpp) \
                   $(wildcard $(SRC_DIR)/Database/Repositories/*.cpp)
CLIENT_SOURCES := $(wildcard $(SRC_DIR)/Client/*.cpp)
WORKERS_SOURCES := $(wildcard $(SRC_DIR)/Workers/Base/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Protocol/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/Components/*.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/WorkerFactory.cpp) \
                  $(wildcard $(SRC_DIR)/Workers/WorkerManager.cpp)
STORAGE_SOURCES := $(wildcard $(SRC_DIR)/Storage/*.cpp)
PIPELINE_SOURCES := $(wildcard $(SRC_DIR)/Pipeline/*.cpp)
ALARM_SOURCES := $(wildcard $(SRC_DIR)/Alarm/*.cpp)
VIRTUALPOINT_SOURCES := $(wildcard $(SRC_DIR)/VirtualPoint/*.cpp)
PLATFORM_SOURCES := $(wildcard $(SRC_DIR)/Platform/*.cpp)

# 조건부 소스 포함
ifeq ($(HAS_HTTPLIB),1)
    NETWORK_SOURCES := $(wildcard $(SRC_DIR)/Network/*.cpp)
    API_SOURCES := $(wildcard $(SRC_DIR)/Api/*.cpp)
else
    NETWORK_SOURCES :=
    API_SOURCES :=
endif

DRIVERS_SOURCES = $(wildcard $(SRC_DIR)/Drivers/Common/*.cpp)
ifneq ($(filter -lmodbus,$(OPTIONAL_LIBS)),)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Modbus/*.cpp)
endif
ifneq ($(filter -lpaho-mqttpp3,$(OPTIONAL_LIBS)),)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Mqtt/*.cpp)
endif
ifeq ($(HAS_BACNET_STACK),1)
    DRIVERS_SOURCES += $(wildcard $(SRC_DIR)/Drivers/Bacnet/*.cpp)
endif

ALL_SOURCES = $(CORE_SOURCES) $(UTILS_SOURCES) $(CONFIG_SOURCES) \
              $(DATABASE_SOURCES) $(CLIENT_SOURCES) $(WORKERS_SOURCES) \
              $(DRIVERS_SOURCES) $(PIPELINE_SOURCES) $(ALARM_SOURCES) \
              $(VIRTUALPOINT_SOURCES) $(NETWORK_SOURCES) $(API_SOURCES) \
              $(PLATFORM_SOURCES) $(STORAGE_SOURCES)

ALL_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ALL_SOURCES))
MAIN_OBJECT = $(BUILD_DIR)/main.o

# 색상 설정
GREEN = \033[0;32m
BLUE = \033[0;34m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m

# =============================================================================
# 🚀 자동 디렉토리 생성 - 핵심 개선사항
# =============================================================================

# 필요한 모든 build 하위 디렉토리
BUILD_SUBDIRS = $(BUILD_DIR)/Core $(BUILD_DIR)/Utils $(BUILD_DIR)/Config \
                $(BUILD_DIR)/Database $(BUILD_DIR)/Database/Entities $(BUILD_DIR)/Database/Repositories \
                $(BUILD_DIR)/Client $(BUILD_DIR)/Workers $(BUILD_DIR)/Workers/Base $(BUILD_DIR)/Workers/Protocol $(BUILD_DIR)/Workers/Components \
                $(BUILD_DIR)/Drivers $(BUILD_DIR)/Drivers/Common $(BUILD_DIR)/Drivers/Modbus $(BUILD_DIR)/Drivers/Mqtt $(BUILD_DIR)/Drivers/Bacnet \
                $(BUILD_DIR)/Pipeline $(BUILD_DIR)/Alarm $(BUILD_DIR)/VirtualPoint \
                $(BUILD_DIR)/Network $(BUILD_DIR)/Api $(BUILD_DIR)/Platform $(BUILD_DIR)/Storage

# 디렉토리 자동 생성 함수
define create_build_dirs
	@mkdir -p $(BUILD_SUBDIRS)
endef

# =============================================================================
# 메인 타겟들
# =============================================================================

.DEFAULT_GOAL := help

# 전체 빌드
all: check-env build-dirs $(TARGET)
	@echo -e "$(GREEN)✅ Build completed for $(PLATFORM)!$(NC)"
	@echo "Binary: $(BIN_DIR)/$(TARGET)"
	@echo "Shared libraries: $(if $(HAS_SHARED),✅ enabled,❌ disabled)"
	@echo "Protocol libraries: $(words $(shell echo $(OPTIONAL_LIBS) | tr ' ' '\n' | sort -u)) unique"

# 디버그 빌드
debug: CXXFLAGS += -DDEBUG -g -O0 -fno-omit-frame-pointer
debug: all

# 릴리즈 빌드  
release: CXXFLAGS += -DNDEBUG -O3 -flto
release: all

# 🚀 디렉토리 생성 타겟 (중요!)
build-dirs: | $(BUILD_DIR) $(BIN_DIR)
	$(call create_build_dirs)

# 기본 디렉토리 생성
$(BUILD_DIR) $(BIN_DIR) $(DIST_DIR):
	@mkdir -p $@

# 메인 실행 파일
$(TARGET): $(ALL_OBJECTS) $(MAIN_OBJECT) | $(BIN_DIR)
	@echo -e "$(BLUE)🔗 Linking binary...$(NC)"
	$(CXX) $(LDFLAGS) -o $(BIN_DIR)/$@ $(ALL_OBJECTS) $(MAIN_OBJECT) $(LIBS)
	@echo -e "$(GREEN)✅ $(TARGET) built successfully!$(NC)"
	@echo "Size: $(shell du -h $(BIN_DIR)/$(TARGET) | cut -f1)"

# 🚀 오브젝트 파일 컴파일 (자동 디렉토리 생성 포함)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling main.cpp"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# 의존성 파일 생성 (자동 디렉토리 생성 포함)
# =============================================================================

# 의존성 파일 생성 규칙
$(BUILD_DIR)/%.d: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(BUILD_DIR)/$*.o $< > $@

# 자동 종속성 포함
-include $(ALL_OBJECTS:.o=.d)

# =============================================================================
# 크로스 컴파일 및 특수 타겟
# =============================================================================

windows-cross:
	@echo -e "$(YELLOW)🔧 Building for Windows (cross-compile)...$(NC)"
	$(MAKE) CROSS_COMPILE_WINDOWS=1 all

# TCP 소켓 오류 자동 수정
fix-tcp:
	@echo -e "$(YELLOW)🔧 Fixing TCP socket issues...$(NC)"
	@if ! grep -q "netinet/tcp.h" src/Workers/Base/TcpBasedWorker.cpp; then \
		sed -i '1i#include <netinet/tcp.h>' src/Workers/Base/TcpBasedWorker.cpp; \
		echo "✅ Added TCP header"; \
	fi
	@sed -i 's/case EWOULDBLOCK: return "Operation would block";/#if EAGAIN != EWOULDBLOCK\n        case EWOULDBLOCK: return "Operation would block";\n#endif/' src/Workers/Base/TcpBasedWorker.cpp
	@echo "✅ Fixed EWOULDBLOCK duplicate case"

# 실행
run: $(TARGET)
	@echo -e "$(GREEN)🚀 Running $(TARGET)...$(NC)"
	@$(BIN_DIR)/$(TARGET)

# =============================================================================
# 유틸리티 및 검증
# =============================================================================

check-env:
	@echo -e "$(BLUE)🔍 Environment check for $(PLATFORM)...$(NC)"
	@echo "Platform: $(PLATFORM)"
	@echo "Shared libs: $(if $(HAS_SHARED),✅ $(SHARED_LIB_DIR),❌ disabled)"
	@echo "Sources: $(words $(ALL_SOURCES)) files"
	@echo "Libraries: $(if $(OPTIONAL_LIBS),✅ $(words $(shell echo $(OPTIONAL_LIBS) | tr ' ' '\n' | sort -u)),❌ minimal)"

status:
	@echo -e "$(BLUE)📊 Build Status$(NC)"
	@echo "Platform: $(PLATFORM)"
	@echo "Target: $(if $(wildcard $(BIN_DIR)/$(TARGET)),✅ $(BIN_DIR)/$(TARGET),❌ not built)"
	@echo "Shared Libraries: $(if $(HAS_SHARED),✅ available,❌ not found)"
	@if [ "$(HAS_SHARED)" = "1" ]; then \
		echo "  - $(SHARED_LIB_DIR)/libpulseone-common.a: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-common.a),✅,❌)"; \
		echo "  - $(SHARED_LIB_DIR)/libpulseone-alarm.a: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-alarm.a),✅,❌)"; \
		echo "  - $(SHARED_LIB_DIR)/libpulseone-database.a: $(if $(wildcard $(SHARED_LIB_DIR)/libpulseone-database.a),✅,❌)"; \
	fi
	@echo "Objects: $(words $(wildcard $(BUILD_DIR)/**/*.o)) / $(words $(ALL_OBJECTS))"

# 🚀 개선된 clean (build 디렉토리 구조는 유지)
clean:
	@echo -e "$(YELLOW)🧹 Cleaning build artifacts...$(NC)"
	@find $(BUILD_DIR) -name "*.o" -delete 2>/dev/null || true
	@find $(BUILD_DIR) -name "*.d" -delete 2>/dev/null || true
	@rm -f $(BIN_DIR)/$(TARGET) 2>/dev/null || true
	@echo "✅ Clean completed (directories preserved)"

# 완전 정리 (디렉토리까지 삭제)
distclean:
	@echo -e "$(RED)🗑️  Complete cleanup...$(NC)"
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(DIST_DIR)

help:
	@echo -e "$(BLUE)🔧 PulseOne Collector Build System$(NC)"
	@echo ""
	@echo -e "$(GREEN)Building:$(NC)"
	@echo "  make all              - Build with shared libraries (if available)"
	@echo "  make debug            - Debug build (-g -O0)"
	@echo "  make release          - Release build (-O3 -flto)"
	@echo "  make windows-cross    - Cross-compile for Windows"
	@echo "  make run              - Build and run"
	@echo ""
	@echo -e "$(GREEN)Maintenance:$(NC)"
	@echo "  make clean            - Clean artifacts (preserve directories)"
	@echo "  make distclean        - Complete cleanup"
	@echo "  make fix-tcp          - Fix TCP socket compilation issues"
	@echo ""
	@echo -e "$(GREEN)Utilities:$(NC)"
	@echo "  make check-env        - Check environment"
	@echo "  make status           - Build status"
	@echo "  make build-dirs       - Create build directories"
	@echo ""
	@echo -e "$(YELLOW)Current Configuration:$(NC)"
	@echo "  Platform: $(PLATFORM)"
	@echo "  Shared Libraries: $(if $(HAS_SHARED),✅ enabled,❌ disabled)"
	@echo "  Cross Compile: $(if $(CROSS_COMPILE_WINDOWS),✅ Windows,❌ Native)"

.PHONY: all debug release clean distclean check-env status windows-cross help build-dirs fix-tcp run