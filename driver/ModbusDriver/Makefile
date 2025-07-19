# =============================================================================
# ModbusDriver Plugin Makefile (향상된 버전)
# 기존 driver/ModbusDriver/Makefile을 이걸로 교체하세요
# =============================================================================

# 컴파일러 및 플래그 설정
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -fPIC
INCLUDES = -I./include -I/usr/local/include

# 플랫폼별 설정
ifeq ($(OS),Windows_NT)
    TARGET = ModbusDriver.dll
    LDFLAGS = -shared -Wl,--out-implib,ModbusDriver.lib
    LIBS = -lmodbus -lws2_32 -lpthread
    RM = del /Q
    MKDIR = mkdir
    SHARED_EXT = dll
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        TARGET = ModbusDriver.so
        LDFLAGS = -shared -fPIC
        LIBS = -lmodbus -lpthread
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

# 소스 파일들 (자동 감지)
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# 기본 타겟
.PHONY: all build clean install test debug release help

all: build

# 빌드 타겟
build: $(PLUGIN_DIR)/$(TARGET)

$(PLUGIN_DIR)/$(TARGET): $(OBJECTS) | $(PLUGIN_DIR)
	@echo "🔗 Linking plugin: $(TARGET)"
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)
	@echo "✅ Plugin built successfully: $@"
	@echo "📁 File info:"
	@ls -la $@
	@file $@
	@echo "📋 Dependencies:"
	@ldd $@ 2>/dev/null | head -5 || echo "N/A"

# 오브젝트 파일 컴파일
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "🔨 Compiling: $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 디렉토리 생성
$(OBJ_DIR):
	$(MKDIR) $(OBJ_DIR)

$(PLUGIN_DIR):
	$(MKDIR) $(PLUGIN_DIR)

$(DIST_DIR):
	$(MKDIR) $(DIST_DIR)

# 디버그 빌드
debug: CXXFLAGS += -g -DDEBUG -O0
debug: build

# 릴리즈 빌드  
release: CXXFLAGS += -O3 -DNDEBUG
release: build

# 설치 (시스템 플러그인 디렉토리로 복사)
install: build | $(DIST_DIR)
	@echo "📦 Installing plugin..."
	cp $(PLUGIN_DIR)/$(TARGET) $(DIST_DIR)/
	@echo "✅ Plugin installed to $(DIST_DIR)/"

# 테스트 빌드
test: build
	@echo "🧪 Testing plugin..."
	@if [ -f "$(PLUGIN_DIR)/$(TARGET)" ]; then \
		echo "✅ Plugin file exists"; \
		echo "📋 Size: $$(stat --format=%s $(PLUGIN_DIR)/$(TARGET) 2>/dev/null || echo 'unknown') bytes"; \
		echo "📋 Symbols:"; \
		nm -D $(PLUGIN_DIR)/$(TARGET) 2>/dev/null | grep -E "(CreateDriver|DestroyDriver|GetPluginInfo)" || echo "No symbols found"; \
		echo "📋 Dependencies:"; \
		ldd $(PLUGIN_DIR)/$(TARGET) 2>/dev/null | grep -E "(modbus|pthread)" || echo "Basic deps only"; \
	else \
		echo "❌ Plugin build failed"; \
		exit 1; \
	fi

# 정리
clean:
	@echo "🧹 Cleaning build files..."
	$(RM) $(OBJ_DIR)/*.o 2>/dev/null || true
	$(RM) $(PLUGIN_DIR)/$(TARGET) 2>/dev/null || true
	@echo "✅ Clean completed"

# 완전 정리
distclean: clean
	@echo "🧹 Deep cleaning..."
	$(RM) -r $(OBJ_DIR) 2>/dev/null || true
	$(RM) -r $(PLUGIN_DIR) 2>/dev/null || true
	$(RM) -r $(DIST_DIR) 2>/dev/null || true

# 의존성 확인
deps:
	@echo "🔍 Checking dependencies..."
	@echo "📦 Required libraries:"
	@pkg-config --exists libmodbus && echo "  ✅ libmodbus: $$(pkg-config --modversion libmodbus)" || echo "  ❌ libmodbus: Not found"
	@echo "📦 Compiler:"
	@$(CXX) --version | head -1
	@echo "📦 Include paths:"
	@echo "  $(INCLUDES)"

# 도움말
help:
	@echo "🛠️  ModbusDriver Plugin Build System"
	@echo ""
	@echo "Targets:"
	@echo "  build    - Build the plugin (default)"
	@echo "  debug    - Build with debug symbols"
	@echo "  release  - Build optimized release"
	@echo "  install  - Install to dist directory"
	@echo "  test     - Build and test plugin"
	@echo "  clean    - Remove build files"
	@echo "  distclean- Remove all generated files"
	@echo "  deps     - Check dependencies"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Platform: $(if $(filter Windows_NT,$(OS)),Windows,$(UNAME_S))"
	@echo "Target: $(TARGET)"
	@echo "Sources: $(words $(SOURCES)) files"

# 상세 정보
info:
	@echo "📋 ModbusDriver Plugin Build Information"
	@echo "=================================="
	@echo "Target: $(TARGET)"
	@echo "Sources: $(SOURCES)"
	@echo "Objects: $(OBJECTS)"
	@echo "Compiler: $(CXX)"
	@echo "Flags: $(CXXFLAGS)"
	@echo "Includes: $(INCLUDES)"
	@echo "Libs: $(LIBS)"
	@echo "Output: $(PLUGIN_DIR)/$(TARGET)"