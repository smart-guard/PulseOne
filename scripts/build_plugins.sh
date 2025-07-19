#!/bin/bash
# =============================================================================
# scripts/build_plugins.sh
# 플러그인 빌드 스크립트 (컨테이너 시작시 실행)
# =============================================================================

set -e

echo "🚀 Building PulseOne plugins..."

# 1. 플러그인 설정 (한번만 실행)
if [ ! -f /app/.plugins_configured ]; then
    echo "⚙️ Configuring plugins for the first time..."
    
    PLUGIN_DIR="/app/driver/ModbusDriver"
    COLLECTOR_DIR="/app/collector"
    
    # 디렉토리 생성
    mkdir -p "$PLUGIN_DIR/include/Drivers"
    mkdir -p "$PLUGIN_DIR/include/Utils" 
    mkdir -p "$PLUGIN_DIR/include/Database"
    mkdir -p "$PLUGIN_DIR/include/Config"
    mkdir -p "$PLUGIN_DIR/src"
    mkdir -p "$PLUGIN_DIR/obj"
    mkdir -p "/app/driver/plugins"
    
    # 헤더 파일들 심볼릭 링크
    ln -sf "$COLLECTOR_DIR/include/Drivers/CommonTypes.h" "$PLUGIN_DIR/include/Drivers/" 2>/dev/null || true
    ln -sf "$COLLECTOR_DIR/include/Drivers/IProtocolDriver.h" "$PLUGIN_DIR/include/Drivers/" 2>/dev/null || true
    ln -sf "$COLLECTOR_DIR/include/Drivers/DriverFactory.h" "$PLUGIN_DIR/include/Drivers/" 2>/dev/null || true
    ln -sf "$COLLECTOR_DIR/include/Drivers/ModbusDriver.h" "$PLUGIN_DIR/include/Drivers/" 2>/dev/null || true
    ln -sf "$COLLECTOR_DIR/include/Drivers/DriverLogger.h" "$PLUGIN_DIR/include/Drivers/" 2>/dev/null || true
    ln -sf "$COLLECTOR_DIR/include/Utils/LogManager.h" "$PLUGIN_DIR/include/Utils/" 2>/dev/null || true
    ln -sf "$COLLECTOR_DIR/include/Utils/LogLevels.h" "$PLUGIN_DIR/include/Utils/" 2>/dev/null || true
    ln -sf "$COLLECTOR_DIR/include/Database/DatabaseManager.h" "$PLUGIN_DIR/include/Database/" 2>/dev/null || true
    ln -sf "$COLLECTOR_DIR/include/Config/ConfigManager.h" "$PLUGIN_DIR/include/Config/" 2>/dev/null || true
    
    # 소스 파일들 복사
    if [ -f "$COLLECTOR_DIR/src/Drivers/ModbusDriver.cpp" ]; then
        cp "$COLLECTOR_DIR/src/Drivers/ModbusDriver.cpp" "$PLUGIN_DIR/src/" 2>/dev/null || true
        echo "✅ ModbusDriver.cpp copied"
    fi
    
    if [ -f "$COLLECTOR_DIR/src/Drivers/DriverLogger.cpp" ]; then
        cp "$COLLECTOR_DIR/src/Drivers/DriverLogger.cpp" "$PLUGIN_DIR/src/" 2>/dev/null || true
        echo "✅ DriverLogger.cpp copied"
    fi
    
    # 플러그인 엔트리 포인트 생성 (핵심!)
    cat > "$PLUGIN_DIR/src/plugin_entry.cpp" << 'EOF'
// =============================================================================
// ModbusDriver Plugin Entry Point
// =============================================================================

#include "Drivers/ModbusDriver.h"
#include "Drivers/IProtocolDriver.h"
#include <memory>

using namespace PulseOne::Drivers;

extern "C" {

struct PluginInfo {
    const char* name;
    const char* version;
    const char* protocol;
    const char* description;
};

#ifdef _WIN32
    #define PLUGIN_EXPORT __declspec(dllexport)
#else
    #define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

PLUGIN_EXPORT PluginInfo* GetPluginInfo() {
    static PluginInfo info = {
        "ModbusDriver",
        "1.0.0", 
        "modbus_tcp",
        "Modbus TCP/RTU Protocol Driver for PulseOne"
    };
    return &info;
}

PLUGIN_EXPORT IProtocolDriver* CreateDriver() {
    try {
        return new ModbusDriver();
    } catch (...) {
        return nullptr;
    }
}

PLUGIN_EXPORT void DestroyDriver(IProtocolDriver* driver) {
    if (driver) {
        delete driver;
    }
}

PLUGIN_EXPORT bool InitializePlugin() { return true; }
PLUGIN_EXPORT void CleanupPlugin() {}

} // extern "C"
EOF
    
    # Makefile 생성
    cat > "$PLUGIN_DIR/Makefile" << 'EOF'
# =============================================================================
# ModbusDriver Plugin Makefile
# =============================================================================

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -fPIC
INCLUDES = -I./include -I/usr/local/include

TARGET = ModbusDriver.so
LDFLAGS = -shared -fPIC
LIBS = -lmodbus -lpthread
RM = rm -f
MKDIR = mkdir -p

SRC_DIR = src
OBJ_DIR = obj
PLUGIN_DIR = ../plugins

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

.PHONY: all build clean

all: build

build: $(PLUGIN_DIR)/$(TARGET)

$(PLUGIN_DIR)/$(TARGET): $(OBJECTS) | $(PLUGIN_DIR)
	@echo "🔗 Linking plugin: $(TARGET)"
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)
	@echo "✅ Plugin built: $@"
	@ls -la $@
	@file $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "🔨 Compiling: $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR):
	$(MKDIR) $(OBJ_DIR)

$(PLUGIN_DIR):
	$(MKDIR) $(PLUGIN_DIR)

clean:
	@echo "🧹 Cleaning..."
	$(RM) $(OBJ_DIR)/*.o
	$(RM) $(PLUGIN_DIR)/$(TARGET)

test: build
	@echo "🧪 Testing plugin load..."
	@if [ -f $(PLUGIN_DIR)/$(TARGET) ]; then \
		echo "✅ Plugin file exists"; \
		ldd $(PLUGIN_DIR)/$(TARGET) | head -10; \
		nm -D $(PLUGIN_DIR)/$(TARGET) | grep -E "(CreateDriver|DestroyDriver)" || true; \
	fi
EOF
    
    touch /app/.plugins_configured
    echo "✅ Plugins configured"
else
    echo "✅ Plugins already configured"
fi

# 2. ModbusDriver 빌드
echo "🔨 Building ModbusDriver..."
cd /app/driver/ModbusDriver

# 의존성 확인
if ! pkg-config --exists libmodbus; then
    echo "❌ libmodbus not found!"
    exit 1
fi

# 빌드 실행
make clean 2>/dev/null || true
make build

# 3. 결과 확인
echo "📦 Plugin build results:"
if [ -f "../plugins/ModbusDriver.so" ]; then
    echo "✅ ModbusDriver.so created successfully"
    ls -la ../plugins/ModbusDriver.so
    echo "📋 Symbols check:"
    nm -D ../plugins/ModbusDriver.so | grep -E "(CreateDriver|DestroyDriver|GetPluginInfo)" | head -5 || true
else
    echo "❌ Plugin build failed"
    exit 1
fi

echo "🎉 Plugin build completed!"
echo "📁 Available plugins:"
ls -la /app/driver/plugins/ 2>/dev/null || echo "No plugins found"