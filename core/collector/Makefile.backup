# =============================================================================
# PulseOne Collector 최종 프로덕션 Makefile
# 테스트 버전의 라이브러리 감지 + 프로덕션 최적화
# =============================================================================

# 플랫폼 및 성능 설정
NPROC := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += -j$(NPROC) --output-sync=target

# 플랫폼 감지 (테스트 버전과 동일)
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

# 동적 라이브러리 감지 (테스트 버전 활용)
HAS_NLOHMANN_JSON := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_HIREDIS := $(shell echo '\#include <hiredis/hiredis.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_C := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3c >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_AS := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqtt3as >/dev/null 2>&1 && echo "1" || echo "0")
HAS_MQTT_CPP := $(shell echo "int main(){return 0;}" | $(CXX) -x c++ - -lpaho-mqttpp3 >/dev/null 2>&1 && echo "1" || echo "0")
HAS_BACNET_STACK := $(shell [ -f "/usr/local/lib/libbacnet.a" ] && echo "1" || echo "0")
HAS_QUICKJS := $(shell pkg-config --exists quickjs && echo "1" || echo "0")
HAS_HTTPLIB := $(shell echo '\#include <httplib.h>' | $(CXX) -E -x c++ - >/dev/null 2>&1 && echo "1" || echo "0")
HAS_CURL := $(shell pkg-config --exists libcurl && echo "1" || echo "0")

# 컴파일러 설정 - 프로덕션 최적화
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -DNDEBUG -fPIC \
           -Wno-unused-but-set-variable -Wno-unused-variable \
           -Wno-unused-parameter \
           -fdiagnostics-color=always \
           -march=native -mtune=native \
           -flto -fuse-linker-plugin

# 플랫폼별 설정
ifeq ($(IS_WINDOWS),1)
    CXXFLAGS += -DPULSEONE_WINDOWS=1 -DPULSEONE_LINUX=0 \
                -D_WIN32_WINNT=0x0A00 -DWIN32_LEAN_AND_MEAN -DNOMINMAX \
                -static-libgcc -static-libstdc++
    PLATFORM_LIBS = -lws2_32 -liphlpapi -lkernel32 -luser32
    LDFLAGS = -Wl,--gc-sections -static
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

# 모든 모듈 헤더 경로 (테스트 버전과 동일)
INCLUDES = -I$(INCLUDE_DIR) \
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

# 라이브러리 설정 (동적 감지 활용)
BASIC_LIBS = -lpthread -lsqlite3 -lm -ldl $(PLATFORM_LIBS)

# 조건부 라이브러리 (테스트 버전 로직 활용)
OPTIONAL_LIBS = 
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

LIBS = $(BASIC_LIBS) $(OPTIONAL_LIBS)

# 소스 파일 수집 (모든 모듈 포함)
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
ifneq ($(MODBUS_LIBS),)
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
# 메인 타겟들
# =============================================================================

.DEFAULT_GOAL := help

# 디렉토리 생성
$(BUILD_DIR) $(BIN_DIR) $(DIST_DIR):
	@mkdir -p $@
	@mkdir -p $(BUILD_DIR)/Core $(BUILD_DIR)/Utils $(BUILD_DIR)/Config
	@mkdir -p $(BUILD_DIR)/Database/Entities $(BUILD_DIR)/Database/Repositories
	@mkdir -p $(BUILD_DIR)/Client $(BUILD_DIR)/Workers/Base $(BUILD_DIR)/Workers/Protocol $(BUILD_DIR)/Workers/Components
	@mkdir -p $(BUILD_DIR)/Drivers/Common $(BUILD_DIR)/Drivers/Modbus $(BUILD_DIR)/Drivers/Mqtt $(BUILD_DIR)/Drivers/Bacnet
	@mkdir -p $(BUILD_DIR)/Pipeline $(BUILD_DIR)/Alarm $(BUILD_DIR)/VirtualPoint
	@mkdir -p $(BUILD_DIR)/Network $(BUILD_DIR)/Api $(BUILD_DIR)/Platform $(BUILD_DIR)/Storage

# 전체 빌드
all: check-env $(BUILD_DIR) $(BIN_DIR) $(TARGET)
	@echo -e "$(GREEN)✅ Production build completed for $(PLATFORM)!$(NC)"
	@echo "Binary: $(BIN_DIR)/$(TARGET)"
	@echo "Libraries: $(words $(shell echo $(LIBS) | tr ' ' '\n' | sort -u)) unique"

# 메인 실행 파일
$(TARGET): $(ALL_OBJECTS) $(MAIN_OBJECT) | $(BIN_DIR)
	@echo -e "$(BLUE)🔗 Linking production binary...$(NC)"
	$(CXX) $(LDFLAGS) -o $(BIN_DIR)/$@ $(ALL_OBJECTS) $(MAIN_OBJECT) $(LIBS)
	@echo -e "$(GREEN)✅ $(TARGET) built successfully!$(NC)"
	@echo "Size: $(shell du -h $(BIN_DIR)/$(TARGET) | cut -f1)"

# 오브젝트 파일 컴파일
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp | $(BUILD_DIR)
	@echo "Compiling main.cpp"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# Windows 패키징 시스템
# =============================================================================

# Windows용 정적 빌드
windows-static: CXXFLAGS += -static-libgcc -static-libstdc++
windows-static: LDFLAGS += -static
windows-static: clean all
	@echo -e "$(GREEN)✅ Windows static build completed!$(NC)"

# 배포 준비
prepare-dist: windows-static | $(DIST_DIR)
	@echo -e "$(BLUE)📦 Preparing distribution files...$(NC)"
	@cp $(BIN_DIR)/$(TARGET) $(DIST_DIR)/
	@mkdir -p $(DIST_DIR)/config $(DIST_DIR)/data $(DIST_DIR)/logs
	@cp -r config/* $(DIST_DIR)/config/ 2>/dev/null || true
	@echo -e "$(GREEN)✅ Distribution prepared in $(DIST_DIR)/$(NC)"

# Node.js 백엔드 패키징 (pkg 사용)
package-backend:
	@echo -e "$(BLUE)📦 Packaging Node.js backend...$(NC)"
	@cd ../backend && npm install -g pkg
	@cd ../backend && pkg package.json --targets node18-win-x64 --output ../collector/$(DIST_DIR)/pulseone-backend.exe
	@echo -e "$(GREEN)✅ Backend packaged to $(DIST_DIR)/pulseone-backend.exe$(NC)"

# 프론트엔드 빌드
build-frontend:
	@echo -e "$(BLUE)📦 Building frontend...$(NC)"
	@cd ../frontend && npm run build
	@mkdir -p $(DIST_DIR)/web
	@cp -r ../frontend/dist/* $(DIST_DIR)/web/
	@echo -e "$(GREEN)✅ Frontend built to $(DIST_DIR)/web/$(NC)"

# 전체 Windows 패키지 준비
windows-package: prepare-dist package-backend build-frontend
	@echo -e "$(BLUE)📦 Creating complete Windows package...$(NC)"
	@mkdir -p $(DIST_DIR)/scripts
	@echo '@echo off' > $(DIST_DIR)/scripts/install.bat
	@echo 'echo Installing PulseOne...' >> $(DIST_DIR)/scripts/install.bat
	@echo 'xcopy /E /I /Y . "C:\PulseOne\"' >> $(DIST_DIR)/scripts/install.bat
	@echo 'echo Installation completed!' >> $(DIST_DIR)/scripts/install.bat
	@echo -e "$(GREEN)✅ Windows package ready in $(DIST_DIR)/$(NC)"

# NSIS 인스톨러 생성
nsis-installer: windows-package
	@echo -e "$(BLUE)📦 Creating NSIS installer...$(NC)"
	@cat > $(DIST_DIR)/installer.nsi << 'EOF'
!define APPNAME "PulseOne"
!define COMPANYNAME "PulseOne Technologies"
!define DESCRIPTION "Industrial IoT Data Collector"
!define VERSIONMAJOR 2
!define VERSIONMINOR 1
!define VERSIONBUILD 0

Name "${APPNAME}"
InstallDir "$$PROGRAMFILES64\$${APPNAME}"
RequestExecutionLevel admin

Page directory
Page instfiles

Section "MainSection" SEC01
    SetOutPath "$$INSTDIR"
    
    # 실행 파일들
    File "pulseone-collector.exe"
    File "pulseone-backend.exe"
    
    # 웹 파일들
    File /r "web"
    
    # 설정 파일들
    File /r "config"
    
    # 디렉토리 생성
    CreateDirectory "$$INSTDIR\data"
    CreateDirectory "$$INSTDIR\logs"
    
    # 바탕화면 바로가기
    CreateShortCut "$$DESKTOP\PulseOne.lnk" "$$INSTDIR\pulseone-backend.exe"
    
    # 언인스톨러
    WriteUninstaller "$$INSTDIR\uninstall.exe"
    
    # 레지스트리
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$${APPNAME}" \
                     "DisplayName" "$${APPNAME} - $${DESCRIPTION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$${APPNAME}" \
                     "UninstallString" "$$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
    Delete "$$INSTDIR\*.*"
    RMDir /r "$$INSTDIR"
    Delete "$$DESKTOP\PulseOne.lnk"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$${APPNAME}"
SectionEnd
EOF
	@makensis $(DIST_DIR)/installer.nsi 2>/dev/null || echo "NSIS not available - manual installer ready"
	@echo -e "$(GREEN)✅ Installer script created$(NC)"

# =============================================================================
# 유틸리티 및 검증
# =============================================================================

check-env:
	@echo -e "$(BLUE)🔍 Environment check for $(PLATFORM)...$(NC)"
	@echo "Platform: $(PLATFORM)"
	@echo "Sources: $(words $(ALL_SOURCES)) files"
	@echo "Libraries: $(if $(OPTIONAL_LIBS),✅ $(words $(shell echo $(OPTIONAL_LIBS) | tr ' ' '\n' | sort -u)),❌ minimal)"

status:
	@echo -e "$(BLUE)📊 Build Status$(NC)"
	@echo "Platform: $(PLATFORM)"
	@echo "Target: $(if $(wildcard $(BIN_DIR)/$(TARGET)),✅ $(BIN_DIR)/$(TARGET),❌ not built)"
	@echo "Distribution: $(if $(wildcard $(DIST_DIR)),✅ $(DIST_DIR),❌ not prepared)"
	@echo "Objects: $(words $(wildcard $(BUILD_DIR)/**/*.o)) / $(words $(ALL_OBJECTS))"

clean:
	@echo -e "$(YELLOW)🧹 Cleaning...$(NC)"
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(DIST_DIR)

help:
	@echo -e "$(BLUE)🔧 PulseOne Production Build System$(NC)"
	@echo ""
	@echo -e "$(GREEN)Building:$(NC)"
	@echo "  make all              - Production build"
	@echo "  make windows-static   - Windows static build"
	@echo "  make clean            - Clean build files"
	@echo ""
	@echo -e "$(GREEN)Packaging:$(NC)"
	@echo "  make prepare-dist     - Prepare distribution"
	@echo "  make package-backend  - Package Node.js backend"
	@echo "  make build-frontend   - Build React frontend"
	@echo "  make windows-package  - Complete Windows package"
	@echo "  make nsis-installer   - Create NSIS installer"
	@echo ""
	@echo -e "$(GREEN)Utilities:$(NC)"
	@echo "  make check-env        - Check environment"
	@echo "  make status           - Build status"

.PHONY: all clean check-env status windows-static prepare-dist package-backend \
        build-frontend windows-package nsis-installer help

# 자동 종속성
-include $(ALL_OBJECTS:.o=.d)
$(BUILD_DIR)/%.d: $(SRC_DIR)/%.cpp
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(BUILD_DIR)/$*.o $< > $@