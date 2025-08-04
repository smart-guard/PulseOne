#!/bin/bash

echo "🔍 UnifiedModbusDriver.h 의존성 파일들 확인 중..."

# 1. 필수 의존성 파일들 확인
echo "📋 필수 의존성 파일들:"
echo "================================"

files=(
    "include/Drivers/TestUnifiedDriverBase.h"
    "include/Common/NewTypes.h"
    "include/Common/CompatibilityBridge.h"
)

all_exist=true

for file in "${files[@]}"; do
    if [ -f "$file" ]; then
        echo "✅ $file (존재)"
    else
        echo "❌ $file (없음)"
        all_exist=false
    fi
done

echo ""
echo "📂 Modbus 관련 기존 파일들:"
echo "================================"

modbus_files=(
    "include/Drivers/Modbus/ModbusDriver.h"
    "src/Drivers/Modbus/ModbusDriver.cpp"
)

for file in "${modbus_files[@]}"; do
    if [ -f "$file" ]; then
        echo "✅ $file (기존 파일 - 참고용)"
    else
        echo "❌ $file (없음)"
    fi
done

echo ""
echo "🔧 시스템 라이브러리 확인:"
echo "================================"

# libmodbus 확인
if pkg-config --exists libmodbus; then
    echo "✅ libmodbus 설치됨"
    echo "   버전: $(pkg-config --modversion libmodbus)"
    echo "   경로: $(pkg-config --variable=includedir libmodbus)"
else
    echo "❌ libmodbus 설치 필요"
    echo "   설치 명령: sudo apt-get install libmodbus-dev"
fi

echo ""
if [ "$all_exist" = true ]; then
    echo "🎯 결과: 모든 의존성 파일이 준비되었습니다!"
    echo "📝 다음 단계: UnifiedModbusDriver.h 파일 내용 작성"
else
    echo "⚠️  결과: 일부 의존성 파일이 누락되었습니다."
    echo "📋 먼저 누락된 파일들을 생성해야 합니다."
fi
