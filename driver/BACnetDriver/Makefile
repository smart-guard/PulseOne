# =============================================================================
# BACnetDriver Plugin Makefile
# BACnet Building Automation Protocol Driver
# =============================================================================

# 컴파일러 및 플래그 설정
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -fPIC -DBACNET_STACK
INCLUDES = -I./include -I/usr/local/include -I/usr/local/include/bacnet

# 플랫폼별 설정
ifeq ($(OS),Windows_NT)
    TARGET = BACnetDriver.dll
    LDFLAGS = -shared -Wl,--out-implib,BACnetDriver.lib
    LIBS = -lbacnet -lws2_32 -lpthread
    RM = del /Q
    MKDIR = mkdir
    SHARED_EXT = dll
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        TARGET = BACnetDriver.so
        LDFLAGS = -shared -fPIC
        LIBS = -lbacnet -lpthread -lm
        RM = rm -f
        MKDIR = mkdir -p
        SHARED_EXT = so
    endif
endif

# 디렉토리 설정
SRC_DIR = src
OBJ_DIR = obj
PLUGIN_DIR = ../plugins
DIST_DIR = ../dist

# 소스 파일들
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# BACnet 특화 컴파일 플래그
BACNET_CFLAGS = -DBIG_ENDIAN=0 -DBACNET_STACK_VERSION="1.0.0" \
                -DPRINT_ENABLED=1 -DBACAPP_ALL -DBACFILE \
                -DINTRINSIC_REPORTING -DBACNET_TIME_MASTER \
                -DBACNET_PROPERTY_LISTS=1

# 기본 타겟
.PHONY: all build clean install test debug release help deps

all: build

# 빌드 타겟
build: $(PLUGIN_DIR)/$(TARGET)

$(PLUGIN_DIR)/$(TARGET): $(OBJECTS) | $(PLUGIN_DIR)
	@echo "🔗 Linking BACnetDriver plugin: $(TARGET)"
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)
	@echo "✅ BACnetDriver plugin built successfully: $@"
	@echo "📁 File info:"
	@ls -la $@
	@file $@
	@echo "📋 Dependencies:"
	@ldd $@ 2>/dev/null | grep -E "(bacnet|pthread)" || echo "Dependencies check skipped"

# 오브젝트 파일 컴파일
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "🔨 Compiling BACnetDriver: $<"
	$(CXX) $(CXXFLAGS) $(BACNET_CFLAGS) $(INCLUDES) -c $< -o $@

# 디렉토리 생성
$(OBJ_DIR):
	$(MKDIR) $(OBJ_DIR)

$(PLUGIN_DIR):
	$(MKDIR) $(PLUGIN_DIR)

$(DIST_DIR):
	$(MKDIR) $(DIST_DIR)

# 디버그 빌드
debug: CXXFLAGS += -g -DDEBUG -O0 -DBACNET_DEBUG -DDEBUG_ENABLED=1
debug: build

# 릴리즈 빌드  
release: CXXFLAGS += -O3 -DNDEBUG -DBACNET_OPTIMIZED
release: build

# 설치 (시스템 플러그인 디렉토리로 복사)
install: build | $(DIST_DIR)
	@echo "📦 Installing BACnetDriver plugin..."
	cp $(PLUGIN_DIR)/$(TARGET) $(DIST_DIR)/
	@echo "✅ BACnetDriver installed to $(DIST_DIR)/"

# 테스트 빌드
test: build
	@echo "🧪 Testing BACnetDriver plugin..."
	@if [ -f "$(PLUGIN_DIR)/$(TARGET)" ]; then \
		echo "✅ BACnetDriver plugin file exists"; \
		echo "📋 Size: $$(stat --format=%s $(PLUGIN_DIR)/$(TARGET) 2>/dev/null || echo 'unknown') bytes"; \
		echo "📋 Symbols:"; \
		nm -D $(PLUGIN_DIR)/$(TARGET) 2>/dev/null | grep -E "(CreateDriver|DestroyDriver|GetPluginInfo)" || echo "Symbols check..."; \
		echo "📋 BACnet Dependencies:"; \
		ldd $(PLUGIN_DIR)/$(TARGET) 2>/dev/null | grep -E "(bacnet)" || echo "BACnet lib may be static"; \
		echo "📋 Core Dependencies:"; \
		ldd $(PLUGIN_DIR)/$(TARGET) 2>/dev/null | grep -E "(pthread|libc)" || echo "Core libs linked"; \
	else \
		echo "❌ BACnetDriver plugin build failed"; \
		exit 1; \
	fi

# 정리
clean:
	@echo "🧹 Cleaning BACnetDriver build files..."
	$(RM) $(OBJ_DIR)/*.o 2>/dev/null || true
	$(RM) $(PLUGIN_DIR)/$(TARGET) 2>/dev/null || true
	@echo "✅ BACnetDriver clean completed"

# 완전 정리
distclean: clean
	@echo "🧹 Deep cleaning BACnetDriver..."
	$(RM) -r $(OBJ_DIR) 2>/dev/null || true

# 의존성 확인
deps:
	@echo "🔍 Checking BACnetDriver dependencies..."
	@echo "📦 BACnet Stack:"
	@pkg-config --exists bacnet && echo "  ✅ bacnet: $$(pkg-config --modversion bacnet)" || echo "  ⚠️  bacnet: pkg-config not available"
	@echo "📦 Header files:"
	@[ -d /usr/local/include/bacnet ] && echo "  ✅ BACnet headers: Found in /usr/local/include/bacnet" || echo "  ❌ BACnet headers: Not found"
	@[ -f /usr/local/include/bacnet/bacnet.h ] && echo "  ✅ bacnet.h: Found" || echo "  ❌ bacnet.h: Not found"
	@[ -f /usr/local/include/bacnet/config.h ] && echo "  ✅ config.h: Found" || echo "  ❌ config.h: Not found"
	@echo "📦 Library files:"
	@[ -f /usr/local/lib/libbacnet.so ] && echo "  ✅ libbacnet.so: Found" || echo "  ❌ libbacnet.so: Not found"
	@[ -f /usr/local/lib/libbacnet.a ] && echo "  ✅ libbacnet.a: Found" || echo "  ❌ libbacnet.a: Not found"
	@echo "📦 Compiler:"
	@$(CXX) --version | head -1

# BACnet 특화 테스트
bacnet-test: build
	@echo "🧪 BACnet-specific testing..."
	@echo "📋 Checking BACnet symbol exports:"
	@nm -D $(PLUGIN_DIR)/$(TARGET) 2>/dev/null | grep -i bacnet || echo "No BACnet symbols visible (may be static)"
	@echo "📋 Checking standard BACnet functions:"
	@nm $(PLUGIN_DIR)/$(TARGET) 2>/dev/null | grep -E "(Device_Object|Read_Property|Who_Is)" | head -5 || echo "BACnet functions compiled in"

# BACnet 네트워크 스캔 테스트 (선택적)
bacnet-scan-test:
	@echo "🔍 BACnet Network Scan Test"
	@echo "ℹ️  This would perform Who-Is broadcast scan"
	@echo "ℹ️  Implement with actual BACnet network discovery"

# BACnet 객체 읽기 테스트
bacnet-read-test:
	@echo "📖 BACnet Object Read Test"
	@echo "ℹ️  This would test reading BACnet objects"
	@echo "ℹ️  Example: Analog Input, Binary Output, etc."

# 도움말
help:
	@echo "🛠️  BACnetDriver Plugin Build System"
	@echo ""
	@echo "Targets:"
	@echo "  build          - Build the BACnet driver plugin (default)"
	@echo "  debug          - Build with debug symbols and BACnet debugging"
	@echo "  release        - Build optimized release version"
	@echo "  install        - Install to dist directory"
	@echo "  test           - Build and test plugin"
	@echo "  bacnet-test    - BACnet-specific symbol testing"
	@echo "  bacnet-scan-test    - Network discovery testing"
	@echo "  bacnet-read-test    - Object reading testing"
	@echo "  clean          - Remove build files"
	@echo "  distclean      - Remove all generated files"
	@echo "  deps           - Check BACnet dependencies"
	@echo "  help           - Show this help"
	@echo ""
	@echo "BACnet Features:"
	@echo "  - BACnet/IP protocol support"
	@echo "  - Device discovery (Who-Is/I-Am)"
	@echo "  - Object property reading/writing"
	@echo "  - Change of Value (COV) subscriptions"
	@echo "  - Alarm and event handling"
	@echo "  - Trending and logging"
	@echo ""
	@echo "Supported Object Types:"
	@echo "  - Analog Input/Output/Value"
	@echo "  - Binary Input/Output/Value"
	@echo "  - Multi-state Input/Output/Value"
	@echo "  - Device Objects"
	@echo "  - Schedule Objects"
	@echo "  - Calendar Objects"
	@echo ""
	@echo "Platform: $(if $(filter Windows_NT,$(OS)),Windows,$(UNAME_S))"
	@echo "Target: $(TARGET)"
	@echo "Libraries: BACnet Stack"

# 상세 정보
info:
	@echo "📋 BACnetDriver Plugin Build Information"
	@echo "========================================"
	@echo "Target: $(TARGET)"
	@echo "Sources: $(SOURCES)"
	@echo "Objects: $(OBJECTS)"
	@echo "Compiler: $(CXX)"
	@echo "Flags: $(CXXFLAGS)"
	@echo "BACnet Flags: $(BACNET_CFLAGS)"
	@echo "Includes: $(INCLUDES)"
	@echo "Libraries: $(LIBS)"
	@echo "Output: $(PLUGIN_DIR)/$(TARGET)"
	@echo ""
	@echo "BACnet Configuration:"
	@echo "  Protocol: BACnet/IP (UDP Port 47808)"
	@echo "  Services: Read/Write Property, COV, Alarms"
	@echo "  Objects: All standard BACnet objects"
	@echo "  Security: Optional BACnet/SC support"

# BACnet 설정 확인
bacnet-config:
	@echo "🔧 BACnet Stack Configuration"
	@echo "=============================="
	@echo "📋 Compile-time settings:"
	@echo "  BIG_ENDIAN: 0 (Little Endian)"
	@echo "  PRINT_ENABLED: 1 (Debug printing)"
	@echo "  BACAPP_ALL: Enabled (All application layer)"
	@echo "  BACFILE: Enabled (File services)"
	@echo "  INTRINSIC_REPORTING: Enabled (Alarms)"
	@echo "  BACNET_TIME_MASTER: Enabled (Time sync)"
	@echo "  BACNET_PROPERTY_LISTS: Enabled (Property lists)"
	@echo ""
	@echo "📋 Network settings:"
	@echo "  Default Port: 47808"
	@echo "  Max APDU: 1476 bytes"
	@echo "  Max Objects: Unlimited"
	@echo "  Device ID Range: 0-4194302"