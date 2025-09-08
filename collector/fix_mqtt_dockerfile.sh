#!/bin/bash
# =============================================================================
# MQTT C++ 라이브러리 완전 재빌드 (Docker 내부에서 실행)
# =============================================================================

set -e

echo "🔧 MQTT C++ 라이브러리 완전 재빌드 시작..."

MINGW_PREFIX="/usr/x86_64-w64-mingw32"
CROSS_COMPILER="x86_64-w64-mingw32"
TEMP_BUILD="/tmp/mqtt_complete_rebuild"

# 기존 불완전한 라이브러리 제거
rm -f ${MINGW_PREFIX}/lib/libpaho-mqttpp3.a
rm -rf ${MINGW_PREFIX}/include/mqtt

# 작업 디렉토리 준비
rm -rf $TEMP_BUILD
mkdir -p $TEMP_BUILD
cd $TEMP_BUILD

echo "📦 Paho MQTT C++ 전체 소스 다운로드..."
git clone --depth 1 --branch v1.3.2 https://github.com/eclipse/paho.mqtt.cpp.git
cd paho.mqtt.cpp

echo "🛠️ CMake로 완전한 빌드 설정..."
mkdir -p build
cd build

# CMake 크로스 컴파일 설정
cmake .. \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=${CROSS_COMPILER}-gcc \
    -DCMAKE_CXX_COMPILER=${CROSS_COMPILER}-g++ \
    -DCMAKE_RC_COMPILER=${CROSS_COMPILER}-windres \
    -DCMAKE_INSTALL_PREFIX=${MINGW_PREFIX} \
    -DPAHO_WITH_SSL=FALSE \
    -DPAHO_BUILD_STATIC=TRUE \
    -DPAHO_BUILD_SHARED=FALSE \
    -DPAHO_BUILD_DOCUMENTATION=FALSE \
    -DPAHO_BUILD_SAMPLES=FALSE \
    -DPAHO_BUILD_TESTS=FALSE \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=11 \
    -DCMAKE_PREFIX_PATH=${MINGW_PREFIX} \
    -DCMAKE_FIND_ROOT_PATH=${MINGW_PREFIX} \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_CXX_FLAGS="-std=c++11 -D_WIN32_WINNT=0x0601 -DWIN32_LEAN_AND_MEAN"

echo "🔨 전체 라이브러리 빌드..."
make -j$(nproc)

echo "📋 설치..."
make install

# 라이브러리 확인
echo "🔍 빌드 검증..."
if [ -f "${MINGW_PREFIX}/lib/libpaho-mqttpp3.a" ]; then
    echo "✅ libpaho-mqttpp3.a 생성됨 ($(du -h ${MINGW_PREFIX}/lib/libpaho-mqttpp3.a | cut -f1))"
else
    echo "❌ libpaho-mqttpp3.a 생성 실패"
    exit 1
fi

if [ -d "${MINGW_PREFIX}/include/mqtt" ]; then
    echo "✅ MQTT C++ 헤더 설치됨 ($(find ${MINGW_PREFIX}/include/mqtt -name "*.h" | wc -l)개 파일)"
else
    echo "❌ MQTT C++ 헤더 설치 실패"
    exit 1
fi

# 심볼 확인
echo "🔍 라이브러리 심볼 확인..."
${CROSS_COMPILER}-nm ${MINGW_PREFIX}/lib/libpaho-mqttpp3.a | grep -c "connect_options" || echo "0개 connect_options 심볼"
${CROSS_COMPILER}-nm ${MINGW_PREFIX}/lib/libpaho-mqttpp3.a | grep -c "properties" || echo "0개 properties 심볼"

# 정리
cd /
rm -rf $TEMP_BUILD

echo "🎉 MQTT C++ 라이브러리 완전 재빌드 완료!"
echo ""
echo "이제 PulseOne을 다시 빌드하세요:"
echo "make -f Makefile.windows clean"
echo "make -f Makefile.windows all"