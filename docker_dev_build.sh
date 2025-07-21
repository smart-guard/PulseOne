#!/bin/bash

# PulseOne Docker 기반 개발 빌드 스크립트
echo "🐳 PulseOne Docker 기반 개발 빌드"
echo "================================="

# 색상 정의
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# 1단계: 환경 확인
echo -e "${BLUE}1️⃣ 환경 확인${NC}"
if ! command -v docker &> /dev/null; then
    echo -e "${RED}❌ Docker가 설치되지 않았습니다.${NC}"
    exit 1
fi

if ! command -v docker-compose &> /dev/null; then
    echo -e "${RED}❌ Docker Compose가 설치되지 않았습니다.${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Docker 환경 확인 완료${NC}"

# 2단계: 개발 컨테이너 빌드
echo ""
echo -e "${BLUE}2️⃣ Collector 개발 컨테이너 빌드${NC}"
echo "모든 라이브러리가 컨테이너 안에서 설치됩니다..."

# collector 개발 이미지 빌드
docker build -t pulseone-collector-dev -f collector/Dockerfile.dev collector/

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ 개발 컨테이너 빌드 성공${NC}"
else
    echo -e "${RED}❌ 개발 컨테이너 빌드 실패${NC}"
    exit 1
fi

# 3단계: 컨테이너 내에서 빌드 및 테스트
echo ""
echo -e "${BLUE}3️⃣ 컨테이너 내에서 C++ 프로젝트 빌드${NC}"

# 현재 소스를 컨테이너에 마운트하여 빌드
docker run --rm \
    -v "$(pwd)/collector:/app/collector" \
    -w /app/collector \
    pulseone-collector-dev \
    bash -c "
echo '🔍 컨테이너 내 환경 확인';
echo '================================';
echo '컴파일러 버전:';
g++ --version | head -1;
echo '';
echo '설치된 라이브러리들:';
pkg-config --list-all | grep -E '(modbus|paho|redis|postgres)' || echo '  (pkg-config에서 확인 안됨)';
echo '';

echo '📁 소스 파일 확인:';
ls -la include/Workers/Base/ 2>/dev/null || echo '  Workers/Base 폴더 없음';
ls -la src/Workers/Base/ 2>/dev/null || echo '  src/Workers/Base 폴더 없음';
echo '';

echo '🔨 BaseDeviceWorker 컴파일 테스트:';
if [ -f 'src/Workers/Base/BaseDeviceWorker.cpp' ]; then
    echo '  ✅ BaseDeviceWorker.cpp 존재';
    echo '  🔧 컴파일 시도...';
    if g++ -std=c++17 -Iinclude -c src/Workers/Base/BaseDeviceWorker.cpp -o /tmp/test.o 2>compile_error.log; then
        echo '  ✅ BaseDeviceWorker 컴파일 성공!';
        rm -f /tmp/test.o;
    else
        echo '  ❌ BaseDeviceWorker 컴파일 실패:';
        head -10 compile_error.log 2>/dev/null || echo '    (오류 로그 없음)';
    fi;
else
    echo '  ❌ BaseDeviceWorker.cpp 파일 없음';
fi;
echo '';

echo '🔨 전체 프로젝트 빌드 시도:';
if make clean >/dev/null 2>&1; then
    echo '  ✅ make clean 성공';
else
    echo '  ⚠️ make clean 실패 (무시)';
fi;

if make debug 2>build_error.log; then
    echo '  🎉 전체 빌드 성공!';
    if [ -f 'bin/pulseone_collector' ] || [ -f 'pulseone_collector' ]; then
        echo '  ✅ 실행 파일 생성됨';
        ls -la *pulseone_collector* 2>/dev/null || echo '    (실행 파일 확인 실패)';
    fi;
else
    echo '  ❌ 전체 빌드 실패';
    echo '  📄 오류 내용 (최근 15줄):';
    tail -15 build_error.log 2>/dev/null || echo '    (오류 로그 없음)';
fi;
"

# 4단계: 결과 확인
echo ""
echo -e "${BLUE}4️⃣ 빌드 결과 확인${NC}"

# 빌드된 파일이 있는지 확인
if [ -f "collector/bin/pulseone_collector" ] || [ -f "collector/pulseone_collector" ]; then
    echo -e "${GREEN}🎉 빌드 성공! 실행 파일이 생성되었습니다.${NC}"
    echo ""
    echo "📋 다음 단계:"
    echo "  1. 실행 테스트: docker run --rm -v \$(pwd)/collector:/app/collector pulseone-collector-dev /app/collector/pulseone_collector --help"
    echo "  2. 개발 컨테이너 접속: docker run -it --rm -v \$(pwd)/collector:/app/collector pulseone-collector-dev bash"
    echo "  3. 전체 개발 환경: ./scripts/dev_build.sh 또는 ./scripts/quick-dev.sh"
else
    echo -e "${YELLOW}⚠️ 빌드가 완전하지 않습니다.${NC}"
    echo ""
    echo "🔧 문제 해결 방법:"
    echo "  1. 컨테이너 접속하여 수동 확인:"
    echo "     docker run -it --rm -v \$(pwd)/collector:/app/collector pulseone-collector-dev bash"
    echo "  2. 오류 로그 확인:"
    echo "     cat collector/build_error.log"
    echo "  3. Include 경로 수정 필요할 수 있음"
fi

echo ""
echo -e "${BLUE}💡 유용한 Docker 명령어들:${NC}"
echo "  • 개발 컨테이너 접속: docker run -it --rm -v \$(pwd)/collector:/app/collector pulseone-collector-dev bash"
echo "  • 빠른 컴파일 테스트: docker run --rm -v \$(pwd)/collector:/app/collector pulseone-collector-dev make -C /app/collector syntax-check"
echo "  • 전체 서비스 시작: docker-compose -f docker-compose.dev.yml up -d"