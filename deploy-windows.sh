#!/bin/bash

# =============================================================================
# PulseOne Windows 배포 스크립트 (요구사항에 맞는 최종 버전)
# 구조: config/, web.exe, collector.exe, start.bat, install.bat
# =============================================================================

PROJECT_ROOT=$(pwd)
PACKAGE_NAME="PulseOne"
VERSION="2.1.0"
TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
DIST_DIR="$PROJECT_ROOT/dist"
PACKAGE_DIR="$DIST_DIR/$PACKAGE_NAME"

echo "================================================================="
echo "PulseOne Windows 배포 패키징 (최종 버전)"
echo "================================================================="
echo "프로젝트: $PROJECT_ROOT"
echo "시간: $(date)"
echo ""

# =============================================================================
# 1. 환경 확인 및 준비
# =============================================================================

echo "1. 환경 확인 중..."

# 필수 디렉토리 생성
rm -rf $DIST_DIR
mkdir -p $PACKAGE_DIR

# 필수 도구 확인
if ! command -v node &> /dev/null; then
    echo "❌ Node.js가 필요합니다."
    exit 1
fi

if ! command -v npm &> /dev/null; then
    echo "❌ npm이 필요합니다."
    exit 1
fi

# pkg 설치 확인
if ! command -v pkg &> /dev/null; then
    echo "pkg 설치 중..."
    npm install -g pkg
fi

echo "✅ 환경 확인 완료"

# =============================================================================
# 2. Frontend 빌드 및 Backend 통합
# =============================================================================

echo "2. Frontend + Backend 통합 중..."

# Frontend 빌드
if [ -d "$PROJECT_ROOT/frontend" ]; then
    echo "  Frontend 빌드..."
    cd "$PROJECT_ROOT/frontend"
    npm install --silent
    npm run build
    
    if [ ! -d "dist" ]; then
        echo "❌ Frontend 빌드 실패"
        exit 1
    fi
    echo "  ✅ Frontend 빌드 완료"
else
    echo "⚠️ Frontend 디렉토리 없음"
fi

# Backend 준비 및 Frontend 통합
if [ -d "$PROJECT_ROOT/backend" ]; then
    echo "  Backend 설정 중..."
    cd "$PROJECT_ROOT/backend"
    npm install --silent
    
    # Frontend 파일들을 Backend의 public 디렉토리에 복사
    mkdir -p public
    if [ -d "$PROJECT_ROOT/frontend/dist" ]; then
        cp -r "$PROJECT_ROOT/frontend/dist"/* public/
        echo "  ✅ Frontend를 Backend에 통합"
    fi
    
    # pkg 설정 파일 생성
    cat > package-deploy.json << EOF
{
  "name": "pulseone-web",
  "version": "$VERSION",
  "main": "app.js",
  "bin": "app.js",
  "pkg": {
    "targets": ["node18-win-x64"],
    "assets": [
      "public/**/*",
      "lib/**/*",
      "routes/**/*",
      "config/**/*",
      "views/**/*"
    ]
  }
}
EOF
    
    # web.exe 생성
    echo "  web.exe 생성 중..."
    pkg package-deploy.json --targets node18-win-x64 --output $PACKAGE_DIR/web.exe
    
    if [ -f "$PACKAGE_DIR/web.exe" ]; then
        echo "  ✅ web.exe 생성 완료"
        rm package-deploy.json
    else
        echo "❌ web.exe 생성 실패"
        exit 1
    fi
else
    echo "❌ Backend 디렉토리 없음"
    exit 1
fi

# =============================================================================
# 3. Collector 실행파일 추가 (미리 빌드된 것 사용)
# =============================================================================

echo "3. Collector 실행파일 추가 중..."

COLLECTOR_EXE="$PROJECT_ROOT/collector/bin-windows/collector.exe"

if [ -f "$COLLECTOR_EXE" ]; then
    cp "$COLLECTOR_EXE" "$PACKAGE_DIR/"
    echo "✅ collector.exe 추가 완료"
else
    echo "❌ collector.exe를 찾을 수 없습니다"
    echo "   다음 명령으로 먼저 빌드하세요:"
    echo "   cd collector"
    echo "   docker run --rm -v \$(pwd):/src ubuntu:22.04 bash -c 'apt-get update && apt-get install -y gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 make && cd /src && make -f Makefile.windows'"
    exit 1
fi

# =============================================================================
# 4. Redis Windows 바이너리 다운로드
# =============================================================================

echo "4. Redis Windows 바이너리 준비 중..."

cd $PACKAGE_DIR

# Redis Windows 바이너리 다운로드
curl -L -o redis-win.zip \
    "https://github.com/tporadowski/redis/releases/download/v5.0.14.1/Redis-x64-5.0.14.1.zip" \
    --silent --fail

if [ -f "redis-win.zip" ]; then
    unzip -q redis-win.zip
    
    # 필요한 파일만 유지
    mv redis-server.exe ./ 2>/dev/null || true
    mv redis-cli.exe ./ 2>/dev/null || true
    
    # 불필요한 파일들 정리
    rm -f *.zip *.msi *.pdb EventLog.dll 2>/dev/null || true
    rm -f redis-benchmark* redis-check-* 2>/dev/null || true
    rm -f RELEASENOTES.txt 00-RELEASENOTES 2>/dev/null || true
    rm -f redis.windows*.conf 2>/dev/null || true
    
    echo "✅ Redis 바이너리 추가 완료 (불필요한 파일 정리됨)"
else
    echo "❌ Redis 다운로드 실패"
    exit 1
fi

# =============================================================================
# 5. 설정 파일들 복사 및 생성 (기존 프로젝트 구조 활용)
# =============================================================================

echo "5. 설정 파일들 구성 중..."

# config 디렉토리 생성
mkdir -p config/secrets

# 기존 config 파일들 복사 (있다면)
if [ -d "$PROJECT_ROOT/config" ]; then
    echo "  기존 설정 파일들 복사..."
    cp -r "$PROJECT_ROOT/config"/* config/ 2>/dev/null || true
    echo "  ✅ 기존 설정 파일들 복사 완료"
fi

# 메인 .env 파일 생성 (없다면)
if [ ! -f "config/.env" ]; then
    cat > config/.env << 'EOF'
# =============================================================================
# PulseOne 메인 환경설정 (.env) - 자동 생성됨
# =============================================================================

NODE_ENV=production
LOG_LEVEL=info
LOG_TO_CONSOLE=true
LOG_TO_FILE=true
LOG_FILE_PATH=./logs/

# 기본 데이터베이스 설정
DATABASE_TYPE=SQLITE

# 기본 디렉토리 설정
DATA_DIR=./data

# 추가 설정 파일들 (모듈별 분리)
CONFIG_FILES=database.env,redis.env,messaging.env,security.env

# 시스템 설정
MAX_WORKER_THREADS=4
DEFAULT_TIMEOUT_MS=5000

# 로그 로테이션 설정
LOG_MAX_SIZE_MB=100
LOG_MAX_FILES=30
MAINTENANCE_MODE=false
EOF
fi

# database.env 생성 (없다면)
if [ ! -f "config/database.env" ]; then
    cat > config/database.env << 'EOF'
# 데이터베이스 설정
DATABASE_TYPE=SQLITE
SQLITE_PATH=./data/pulseone.db

# PostgreSQL 설정 (비활성)
POSTGRES_PRIMARY_ENABLED=false
POSTGRES_PRIMARY_HOST=localhost
POSTGRES_PRIMARY_PORT=5432
POSTGRES_PRIMARY_DB=pulseone_main
POSTGRES_PRIMARY_USER=postgres
POSTGRES_PRIMARY_PASSWORD_FILE=${SECRETS_DIR}/postgres_primary.key
EOF
fi

# redis.env 생성 (없다면)
if [ ! -f "config/redis.env" ]; then
    cat > config/redis.env << 'EOF'
# Redis 설정
REDIS_PRIMARY_ENABLED=true
REDIS_PRIMARY_HOST=localhost
REDIS_PRIMARY_PORT=6379
REDIS_PRIMARY_PASSWORD_FILE=${SECRETS_DIR}/redis_primary.key
REDIS_PRIMARY_DB=0
EOF
fi

# messaging.env 생성 (없다면)
if [ ! -f "config/messaging.env" ]; then
    cat > config/messaging.env << 'EOF'
# 메시징 설정
MESSAGING_TYPE=DISABLED

# RabbitMQ 설정 (비활성)
RABBITMQ_ENABLED=false
RABBITMQ_HOST=localhost
RABBITMQ_PORT=5672
RABBITMQ_USERNAME=guest
RABBITMQ_PASSWORD_FILE=${SECRETS_DIR}/rabbitmq.key

# MQTT 설정 (비활성)
MQTT_ENABLED=false
MQTT_BROKER_HOST=localhost
MQTT_BROKER_PORT=1883
MQTT_CLIENT_ID=pulseone_collector
MQTT_PASSWORD_FILE=${SECRETS_DIR}/mqtt.key
EOF
fi

# security.env 생성 (없다면)
if [ ! -f "config/security.env" ]; then
    cat > config/security.env << 'EOF'
# 보안 설정
JWT_SECRET_FILE=${SECRETS_DIR}/jwt_secret.key
JWT_ALGORITHM=HS256
JWT_ACCESS_TOKEN_EXPIRY=15m
JWT_REFRESH_TOKEN_EXPIRY=7d

# 세션 보안
SESSION_SECRET_FILE=${SECRETS_DIR}/session_secret.key
SESSION_SECURE=false
SESSION_HTTP_ONLY=true

# 암호화 설정
ENCRYPTION_ALGORITHM=AES-256-GCM
ENCRYPTION_KEY_FILE=${SECRETS_DIR}/encryption.key
PASSWORD_HASH_ROUNDS=12

# API 보안
API_RATE_LIMIT_ENABLED=true
API_RATE_LIMIT_WINDOW_MIN=15
API_RATE_LIMIT_MAX_REQUESTS=1000

# SSL/TLS 설정
SSL_ENABLED=false
SSL_CERT_FILE=${SECRETS_DIR}/server.crt
SSL_KEY_FILE=${SECRETS_DIR}/server.key
EOF
fi

# secrets 디렉토리 키 파일들 생성
SECRET_FILES=(
    "encryption.key"
    "influx_token.key" 
    "jwt_secret.key"
    "mqtt.key"
    "mssql.key"
    "mysql.key"
    "postgres_primary.key"
    "rabbitmq.key"
    "redis_primary.key"
    "session_secret.key"
)

for secret_file in "${SECRET_FILES[@]}"; do
    if [ ! -f "config/secrets/$secret_file" ]; then
        echo "# $secret_file - 실제 키/비밀번호로 교체하세요" > "config/secrets/$secret_file"
    fi
done

# secrets/.gitignore 생성
cat > config/secrets/.gitignore << 'EOF'
*
!.gitignore
!README.md
EOF

# secrets/README.md 생성
cat > config/secrets/README.md << 'EOF'
# Secrets Directory

이 디렉토리는 민감한 정보를 저장합니다.
이 파일들은 절대 Git에 커밋하지 마세요!

각 .key 파일에 실제 비밀번호나 토큰을 입력하세요.
EOF

echo "✅ 설정 파일들 구성 완료"

# =============================================================================
# 6. 시작 스크립트들 생성
# =============================================================================

echo "6. 시작 스크립트들 생성 중..."

# 메인 시작 스크립트
cat > start.bat << 'EOF'
@echo off
echo ===============================================
echo PulseOne Industrial IoT Platform v2.1.0
echo ===============================================

REM 데이터 디렉토리 생성
if not exist "data" mkdir data
if not exist "logs" mkdir logs

echo PulseOne 시작 중...

echo [1/3] Redis 서버 시작...
start "Redis" redis-server.exe

echo [2/3] PulseOne 웹 서버 시작...
start "PulseOne Web" web.exe

echo [3/3] PulseOne 데이터 수집기 시작...
start "PulseOne Collector" collector.exe

timeout /t 3 /nobreak > nul

echo.
echo ===============================================
echo ✅ PulseOne 시작 완료!
echo ===============================================
echo 웹 인터페이스: http://localhost:3000
echo 상태 확인: status.bat
echo 종료: stop.bat
echo ===============================================
pause
EOF

# 설치 스크립트
cat > install.bat << 'EOF'
@echo off
echo ===============================================
echo PulseOne Industrial IoT Platform
echo Windows 설치 및 초기 설정
echo ===============================================

echo 시스템 요구사항 확인 중...

REM Windows 버전 확인
ver | find "10." >nul
if %errorlevel% == 0 (
    echo ✅ Windows 10 이상 확인됨
) else (
    echo ⚠️ Windows 버전을 확인할 수 없습니다
)

REM 메모리 확인
for /f "tokens=2 delims=:" %%a in ('wmic computersystem get TotalPhysicalMemory /value ^| find "="') do set MEMORY=%%a
set /a MEMORY_GB=%MEMORY:~0,-9%
if %MEMORY_GB% GEQ 4 (
    echo ✅ RAM %MEMORY_GB%GB 이상 확인됨
) else (
    echo ⚠️ 권장 RAM 4GB 미만입니다
)

echo.
echo 디렉토리 구조 생성 중...
if not exist "data" mkdir data
if not exist "logs" mkdir logs
echo ✅ 기본 디렉토리 생성 완료

echo.
echo 방화벽 설정 안내...
echo Windows 방화벽에서 다음 포트를 허용해야 합니다:
echo - 포트 3000 (웹 서버)
echo - 포트 6379 (Redis)

echo.
echo ===============================================
echo 설치 완료!
echo start.bat를 실행하여 PulseOne을 시작하세요.
echo ===============================================
pause
EOF

# 종료 스크립트
cat > stop.bat << 'EOF'
@echo off
echo PulseOne 종료 중...
taskkill /f /im collector.exe 2>nul
taskkill /f /im web.exe 2>nul  
taskkill /f /im redis-server.exe 2>nul
echo PulseOne 종료 완료
pause
EOF

# 상태 확인 스크립트
cat > status.bat << 'EOF'
@echo off
echo ===============================================
echo PulseOne 상태 확인
echo ===============================================

tasklist /fi "imagename eq redis-server.exe" | find "redis-server.exe" >nul
if %errorlevel% == 0 (echo Redis: 실행중) else (echo Redis: 중지)

tasklist /fi "imagename eq web.exe" | find "web.exe" >nul  
if %errorlevel% == 0 (echo Web: 실행중) else (echo Web: 중지)

tasklist /fi "imagename eq collector.exe" | find "collector.exe" >nul
if %errorlevel% == 0 (echo Collector: 실행중) else (echo Collector: 중지)

echo.
netstat -an | find ":3000" >nul
if %errorlevel% == 0 (echo 웹서버 포트: 활성) else (echo 웹서버 포트: 비활성)

netstat -an | find ":6379" >nul  
if %errorlevel% == 0 (echo Redis 포트: 활성) else (echo Redis 포트: 비활성)

pause
EOF

echo "✅ 시작 스크립트들 생성 완료"

# =============================================================================
# 7. README 생성
# =============================================================================

echo "7. 문서 생성 중..."

# README 파일
cat > README.txt << 'EOF'
================================================================================
PulseOne Industrial IoT Platform - Windows Edition v2.1.0
================================================================================

🚀 빠른 시작
============

1. install.bat 실행 (초기 설정)
2. start.bat 실행 (시스템 시작)
3. 웹 브라우저에서 http://localhost:3000 접속

📁 디렉토리 구조
===============

PulseOne/
├── config/                 # 설정 파일들
│   ├── .env                # 메인 환경설정
│   ├── database.env        # 데이터베이스 설정
│   ├── redis.env           # Redis 설정
│   ├── messaging.env       # RabbitMQ/MQTT 설정
│   ├── security.env        # 보안 설정
│   └── secrets/            # 비밀번호/키 파일들
│       ├── encryption.key
│       ├── jwt_secret.key
│       ├── redis_primary.key
│       └── ...
├── web.exe                 # Backend + Frontend 통합
├── collector.exe           # C++ Collector
├── redis-server.exe        # Redis 서버
├── start.bat               # 시작 스크립트
├── install.bat             # 설치 스크립트
├── stop.bat                # 종료 스크립트
├── status.bat              # 상태 확인
└── README.txt              # 이 파일

🔧 시스템 요구사항
=================

- Windows 10/11 (64-bit)
- 4GB RAM 이상
- 1GB 디스크 공간 이상

🌐 네트워크 포트
===============

- 웹 서버: 3000 (HTTP)
- Redis: 6379 (TCP)

🔧 구성 요소
============

web.exe
- Node.js 백엔드 + React 프론트엔드 통합
- REST API 서버
- 웹 인터페이스 제공

collector.exe  
- C++ 고성능 데이터 수집기
- Modbus/MQTT/BACnet 프로토콜 지원
- 실시간 데이터 처리

redis-server.exe
- 메모리 캐시 시스템
- 실시간 데이터 저장
- 세션 관리

📝 설정 파일 안내
===============

config/.env - 메인 설정
config/database.env - 데이터베이스 연결
config/redis.env - Redis 캐시 설정
config/security.env - 보안 및 인증 설정

config/secrets/ - 민감한 정보 (비밀번호, 키)
실제 사용 시 각 .key 파일에 실제 값을 입력하세요.

🔧 문제 해결
============

시작 안됨:
- 관리자 권한으로 실행
- 방화벽 예외 추가 
- 안티바이러스 예외 추가

포트 충돌:
- 다른 프로그램이 3000, 6379 포트 사용 중인지 확인
- config/.env에서 포트 변경 가능

📞 지원
=======

GitHub: https://github.com/smart-guard/PulseOne
이슈 리포트: GitHub Issues

Copyright (c) 2025 Smart Guard. All rights reserved.
EOF

echo "✅ 문서 생성 완료"

# =============================================================================
# 8. 최종 패키징
# =============================================================================

echo "8. 최종 패키징 중..."

cd $DIST_DIR

# 파일 목록 확인
echo "패키지 내용:"
ls -la $PACKAGE_NAME/

# ZIP 압축
zip -r "${PACKAGE_NAME}_${TIMESTAMP}.zip" $PACKAGE_NAME/ > /dev/null 2>&1

if [ $? -eq 0 ]; then
    PACKAGE_SIZE=$(du -h "${PACKAGE_NAME}_${TIMESTAMP}.zip" | cut -f1)
    echo ""
    echo "================================================================="
    echo "✅ PulseOne Windows 배포 패키징 완료!"
    echo "================================================================="
    echo "파일: $DIST_DIR/${PACKAGE_NAME}_${TIMESTAMP}.zip"
    echo "크기: $PACKAGE_SIZE"
    echo ""
    echo "📦 포함된 파일:"
    echo "  ✅ web.exe (Frontend + Backend 통합)"
    echo "  ✅ collector.exe (데이터 수집기)"  
    echo "  ✅ redis-server.exe (캐시 서버)"
    echo "  ✅ config/ (완전한 설정 구조)"
    echo "  ✅ start.bat (시작 스크립트)"
    echo "  ✅ install.bat (설치 스크립트)"
    echo ""
    echo "🎯 Windows에서 사용법:"
    echo "1. ZIP 압축 해제"
    echo "2. install.bat 실행 (초기 설정)"
    echo "3. start.bat 실행 (시스템 시작)"
    echo "4. http://localhost:3000 접속"
    echo ""
    echo "✅ 배포 완료!"
else
    echo "❌ ZIP 압축 실패"
    exit 1
fi