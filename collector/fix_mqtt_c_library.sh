#!/bin/bash
# =============================================================================
# MQTT C 라이브러리 올바른 빌드 스크립트
# =============================================================================
set -e
echo "🔧 MQTT C 라이브러리 올바른 빌드..."

MINGW_PREFIX="/usr/x86_64-w64-mingw32"
CROSS_COMPILER="x86_64-w64-mingw32"
TEMP_BUILD="/tmp/mqtt_c_complete"

# 기존 불완전한 라이브러리 제거
rm -f ${MINGW_PREFIX}/lib/libpaho-mqtt3*.a

# 작업 디렉토리 준비
rm -rf $TEMP_BUILD
mkdir -p $TEMP_BUILD
cd $TEMP_BUILD

echo "📦 Paho MQTT C 전체 소스 다운로드..."
git clone --depth 1 --branch v1.3.13 https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c

echo "🛠️ CMake로 완전한 빌드..."
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
    -DCMAKE_C_FLAGS="-O2 -D_WIN32_WINNT=0x0601 -DWIN32_LEAN_AND_MEAN"

echo "🔍 사용 가능한 타겟 확인..."
make help | grep paho || echo "타겟 목록을 직접 확인합니다"

echo "🔨 올바른 타겟으로 빌드..."
# 올바른 타겟명 사용 ('-static' 접미사 제거)
make paho-mqtt3c-static -j$(nproc) || {
    echo "⚠️ paho-mqtt3c-static 실패, 대안 시도..."
    make paho-mqtt3c -j$(nproc) || {
        echo "⚠️ 기본 빌드 시도..."
        make -j$(nproc)
    }
}

# 비동기 버전도 시도 (선택적)
make paho-mqtt3a-static -j$(nproc) 2>/dev/null || {
    echo "⚠️ 비동기 버전 빌드 실패 (선택적이므로 계속 진행)"
}

echo "📋 빌드된 라이브러리 확인..."
echo "빌드 디렉토리의 모든 .a 파일:"
find . -name "*.a" -type f

echo "📦 라이브러리 수동 설치..."
# 디렉토리 생성
mkdir -p ${MINGW_PREFIX}/lib
mkdir -p ${MINGW_PREFIX}/include

# 빌드된 라이브러리 찾기 및 복사
for lib_file in $(find . -name "libpaho-mqtt3*.a" -type f); do
    cp "$lib_file" ${MINGW_PREFIX}/lib/
    echo "✅ 복사됨: $(basename $lib_file)"
done

# 특정 위치에서도 찾기
if [ -f "src/libpaho-mqtt3c-static.a" ]; then
    cp src/libpaho-mqtt3c-static.a ${MINGW_PREFIX}/lib/libpaho-mqtt3c.a
    echo "✅ libpaho-mqtt3c-static.a → libpaho-mqtt3c.a 복사됨"
fi

if [ -f "src/libpaho-mqtt3a-static.a" ]; then
    cp src/libpaho-mqtt3a-static.a ${MINGW_PREFIX}/lib/libpaho-mqtt3a.a
    echo "✅ libpaho-mqtt3a-static.a → libpaho-mqtt3a.a 복사됨"
fi

# 헤더 파일 복사
echo "📋 헤더 파일 복사..."
cp ../src/MQTTClient.h ${MINGW_PREFIX}/include/ 2>/dev/null || echo "⚠️ MQTTClient.h 복사 실패"
cp ../src/MQTTAsync.h ${MINGW_PREFIX}/include/ 2>/dev/null || echo "⚠️ MQTTAsync.h 복사 실패"
cp ../src/MQTTClientPersistence.h ${MINGW_PREFIX}/include/ 2>/dev/null || echo "⚠️ MQTTClientPersistence.h 복사 실패"

# 필수 헤더들을 와일드카드로 복사
cp ../src/MQTT*.h ${MINGW_PREFIX}/include/ 2>/dev/null || echo "⚠️ 일부 헤더 파일 복사 실패"

echo "🔍 최종 검증..."
echo "설치된 라이브러리들:"
ls -la ${MINGW_PREFIX}/lib/libpaho-mqtt3*.a 2>/dev/null || echo "❌ 라이브러리 설치 실패"

echo "설치된 헤더들:"
ls -la ${MINGW_PREFIX}/include/MQTT*.h 2>/dev/null || echo "❌ 헤더 설치 실패"

# 기본 라이브러리가 있는지 확인
if [ -f "${MINGW_PREFIX}/lib/libpaho-mqtt3c.a" ] || [ -f "${MINGW_PREFIX}/lib/libpaho-mqtt3c-static.a" ]; then
    echo "✅ MQTT C 라이브러리 설치 성공!"
    
    # 심볼 확인 (선택적)
    MQTT_LIB=$(ls ${MINGW_PREFIX}/lib/libpaho-mqtt3c*.a | head -1)
    if [ -f "$MQTT_LIB" ]; then
        echo "🔍 심볼 확인 ($(basename $MQTT_LIB)):"
        ${CROSS_COMPILER}-nm "$MQTT_LIB" | grep -c "MQTTClient_connect" 2>/dev/null || echo "0개 MQTTClient_connect"
        ${CROSS_COMPILER}-nm "$MQTT_LIB" | grep -c "MQTTClient_create" 2>/dev/null || echo "0개 MQTTClient_create"
    fi
else
    echo "❌ MQTT C 라이브러리 설치 실패"
    exit 1
fi

# 정리
cd /
rm -rf $TEMP_BUILD

echo "🎉 MQTT C 라이브러리 빌드 및 설치 완료!"
echo ""
echo "다음 단계:"
echo "1. cd /src/collector"
echo "2. make -f Makefile.windows clean"
echo "3. make -f Makefile.windows all"