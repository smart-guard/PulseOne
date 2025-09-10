#!/bin/bash

# =============================================================================
# 개선된 PulseOne Windows 패키징 스크립트
# 문제점들을 해결한 버전 - macOS에서 Windows 배포판 생성
# =============================================================================

PROJECT_ROOT=$(pwd)
PACKAGE_NAME="PulseOne_Production"
VERSION="2.1.0"
TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
DIST_DIR="$PROJECT_ROOT/dist"
PACKAGE_DIR="$DIST_DIR/$PACKAGE_NAME"

echo "================================================================="
echo "개선된 PulseOne Windows 패키징 시스템"
echo "GitHub: https://github.com/smart-guard/PulseOne.git"
echo "================================================================="

# =============================================================================
# 1. 프로젝트 구조 확인 및 환경 검증
# =============================================================================

echo "1. 프로젝트 구조 확인 중..."

# 필수 디렉토리 확인
REQUIRED_DIRS=("backend" "frontend" "collector")
MISSING_DIRS=()

for dir in "${REQUIRED_DIRS[@]}"; do
    if [ ! -d "$PROJECT_ROOT/$dir" ]; then
        MISSING_DIRS+=("$dir")
    fi
done

if [ ${#MISSING_DIRS[@]} -gt 0 ]; then
    echo "❌ 누락된 디렉토리: ${MISSING_DIRS[*]}"
    exit 1
fi

# Docker 확인 (Collector 빌드용)
if ! command -v docker &> /dev/null; then
    echo "❌ Docker가 필요합니다 (C++ 크로스 컴파일용)"
    echo "   또는 SKIP_COLLECTOR=true 옵션으로 실행하세요"
    if [ "$SKIP_COLLECTOR" != "true" ]; then
        exit 1
    fi
fi

echo "✅ 프로젝트 구조 확인 완료"

# =============================================================================
# 2. 빌드 환경 준비
# =============================================================================

echo "2. 빌드 환경 준비 중..."

rm -rf $DIST_DIR
mkdir -p $PACKAGE_DIR

# Node.js 확인
if ! command -v node &> /dev/null; then
    echo "❌ Node.js가 필요합니다."
    exit 1
fi

# pkg 도구 확인 및 설치
if ! command -v pkg &> /dev/null; then
    echo "📦 pkg 도구 설치 중..."
    npm install -g pkg
    if [ $? -ne 0 ]; then
        echo "❌ pkg 설치 실패"
        exit 1
    fi
fi

echo "✅ 빌드 환경 준비 완료"
echo "  Node.js: $(node --version)"
echo "  pkg: $(pkg --version)"

# =============================================================================
# 3. Frontend 빌드
# =============================================================================

echo "3. Frontend 빌드 중..."

cd "$PROJECT_ROOT/frontend"

if [ ! -f "package.json" ]; then
    echo "❌ frontend/package.json이 없습니다."
    exit 1
fi

echo "  의존성 설치 중..."
npm install --silent

echo "  Frontend 빌드 실행..."
if npm run build; then
    if [ -d "dist" ]; then
        FRONTEND_SIZE=$(du -sh dist | cut -f1)
        echo "  ✅ Frontend 빌드 완료: $FRONTEND_SIZE"
        echo "    파일 수: $(find dist -type f | wc -l) files"
    else
        echo "  ❌ Frontend dist 폴더가 생성되지 않았습니다."
        exit 1
    fi
else
    echo "  ❌ Frontend 빌드 실패"
    exit 1
fi

# =============================================================================
# 4. Backend 패키징 및 Frontend 통합
# =============================================================================

echo "4. Backend 패키징 중..."

cd "$PROJECT_ROOT/backend"

if [ ! -f "package.json" ]; then
    echo "❌ backend/package.json이 없습니다."
    exit 1
fi

if [ ! -f "app.js" ]; then
    echo "❌ backend/app.js가 없습니다."
    exit 1
fi

echo "  Backend 의존성 설치 중..."
npm install --silent

# pkg 설정을 위한 임시 package.json 생성
echo "  pkg 설정 생성 중..."
cat > temp_package.json << EOF
{
  "name": "pulseone-backend",
  "version": "$VERSION",
  "main": "app.js",
  "bin": "app.js",
  "scripts": {
    "start": "node app.js"
  },
  "pkg": {
    "targets": ["node18-win-x64"],
    "assets": [
      "../frontend/dist/**/*",
      "lib/database/schemas/**/*",
      "lib/connection/**/*",
      "scripts/**/*",
      "../config/**/*"
    ],
    "outputPath": "../dist/backend.exe"
  },
  "dependencies": $(cat package.json | jq '.dependencies // {}')
}
EOF

# 🔧 데이터베이스 초기화 스크립트들을 assets에 포함되도록 설정
echo "  데이터베이스 스키마 파일들 확인 중..."
if [ -d "lib/database/schemas" ]; then
    echo "    ✅ 스키마 파일: $(find lib/database/schemas -name "*.sql" | wc -l) files"
fi

if [ -d "scripts" ]; then
    echo "    ✅ 초기화 스크립트: $(find scripts -name "*.js" | wc -l) files"
fi

# Windows용 실행 파일 생성
echo "  Windows 실행 파일 생성 중..."
if pkg temp_package.json --targets node18-win-x64 --output "$PACKAGE_DIR/pulseone-backend.exe"; then
    BACKEND_SIZE=$(du -sh "$PACKAGE_DIR/pulseone-backend.exe" | cut -f1)
    echo "  ✅ Backend 패키징 완료: $BACKEND_SIZE"
else
    echo "  ❌ Backend 패키징 실패"
    exit 1
fi

# 임시 파일 정리
rm -f temp_package.json

# Frontend를 별도로 복사 (실행 파일에 포함 안 된 경우를 위해)
echo "  Frontend 정적 파일 복사 중..."
mkdir -p "$PACKAGE_DIR/frontend"
cp -r "$PROJECT_ROOT/frontend/dist"/* "$PACKAGE_DIR/frontend/"

# =============================================================================
# 5. Collector 빌드 (Docker 기반)
# =============================================================================

if [ "$SKIP_COLLECTOR" = "true" ]; then
    echo "5. Collector 빌드 스킵됨 (SKIP_COLLECTOR=true)"
else
    echo "5. Collector 크로스 컴파일 중..."
    
    cd "$PROJECT_ROOT"
    
    # 🔧 개선된 Dockerfile 생성
    cat > Dockerfile.mingw << 'EOF'
FROM ubuntu:22.04

# 필수 패키지 설치
RUN apt-get update && apt-get install -y \
    gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 \
    make cmake pkg-config \
    curl wget unzip \
    libsqlite3-dev:amd64 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# nlohmann-json 헤더 다운로드
RUN mkdir -p /usr/x86_64-w64-mingw32/include/nlohmann && \
    wget -O /usr/x86_64-w64-mingw32/include/nlohmann/json.hpp \
    https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp

# SQLite Windows 라이브러리
RUN mkdir -p /usr/x86_64-w64-mingw32/lib && \
    wget -O sqlite-autoconf.tar.gz \
    https://www.sqlite.org/2024/sqlite-autoconf-3460000.tar.gz && \
    tar -xzf sqlite-autoconf.tar.gz && \
    cd sqlite-autoconf-* && \
    CC=x86_64-w64-mingw32-gcc ./configure --host=x86_64-w64-mingw32 \
    --prefix=/usr/x86_64-w64-mingw32 && \
    make && make install && \
    cd .. && rm -rf sqlite-autoconf*

WORKDIR /src
EOF
    
    # Docker 이미지 빌드
    echo "  Docker 빌드 환경 생성 중..."
    if docker build -f Dockerfile.mingw -t pulseone-mingw . > /dev/null 2>&1; then
        echo "  ✅ Docker 환경 준비 완료"
        
        # Collector 컴파일
        echo "  Collector 컴파일 중..."
        docker run --rm \
            -v "$(pwd)/collector:/src" \
            -v "$PACKAGE_DIR:/output" \
            pulseone-mingw bash -c "
                cd /src
                make clean > /dev/null 2>&1
                
                # 🔧 Windows 정적 빌드
                if make -f Makefile.windows clean > /dev/null 2>&1 && \
                   make -f Makefile.windows all; then
                    
                    # 생성된 바이너리 찾기
                    find . -name 'collector.exe' -o -name '*.exe' | while read binary; do
                        if [ -f \"\$binary\" ]; then
                            cp \"\$binary\" /output/collector.exe
                            echo \"Collector binary copied: \$binary\"
                            break
                        fi
                    done
                    
                    echo 'Build completed successfully'
                else
                    echo 'Build failed - creating placeholder'
                    echo '#!/bin/bash' > /output/collector-placeholder.txt
                    echo 'echo \"Collector 빌드 실패 - Docker 환경에서 수동 빌드 필요\"' >> /output/collector-placeholder.txt
                fi
            "
        
        if [ -f "$PACKAGE_DIR/collector.exe" ]; then
            COLLECTOR_SIZE=$(du -sh "$PACKAGE_DIR/collector.exe" | cut -f1)
            echo "  ✅ Collector 빌드 성공: $COLLECTOR_SIZE"
        else
            echo "  ⚠️ Collector 빌드 실패 - 패키지에 포함되지 않음"
        fi
    else
        echo "  ❌ Docker 환경 구성 실패"
    fi
    
    # 정리
    rm -f Dockerfile.mingw
fi

# =============================================================================
# 6. 포터블 Node.js 다운로드
# =============================================================================

echo "6. 포터블 Node.js 다운로드 중..."

cd $PACKAGE_DIR

NODE_VERSION="v18.19.0"
NODE_URL="https://nodejs.org/dist/${NODE_VERSION}/node-${NODE_VERSION}-win-x64.zip"

echo "  Node.js ${NODE_VERSION} 다운로드..."
if curl -L -o nodejs.zip "$NODE_URL" --silent --fail; then
    unzip -q nodejs.zip
    mv "node-${NODE_VERSION}-win-x64/node.exe" ./
    rm -rf "node-${NODE_VERSION}-win-x64" nodejs.zip
    echo "  ✅ 포터블 Node.js 준비 완료"
else
    echo "  ❌ Node.js 다운로드 실패"
    exit 1
fi

# =============================================================================
# 7. 데이터베이스 및 캐시 바이너리
# =============================================================================

echo "7. 데이터베이스 바이너리 준비 중..."

# SQLite
echo "  SQLite 다운로드..."
if curl -L -o sqlite-tools.zip \
    "https://sqlite.org/2024/sqlite-tools-win-x64-3460000.zip" \
    --silent --fail; then
    
    unzip -q sqlite-tools.zip
    mv sqlite-tools-*/sqlite3.exe ./ 2>/dev/null || true
    rm -rf sqlite-tools-* sqlite-tools.zip
    echo "  ✅ SQLite 준비 완료"
else
    echo "  ⚠️ SQLite 다운로드 실패"
fi

# Redis
echo "  Redis 다운로드..."
if curl -L -o redis-win.zip \
    "https://github.com/tporadowski/redis/releases/download/v5.0.14.1/Redis-x64-5.0.14.1.zip" \
    --silent --fail; then
    
    unzip -q redis-win.zip
    mv redis-server.exe ./ 2>/dev/null || true
    mv redis-cli.exe ./ 2>/dev/null || true
    rm -f *.zip *.msi *.pdb EventLog.dll 2>/dev/null || true
    rm -f redis-benchmark* redis-check-* RELEASENOTES.txt 00-RELEASENOTES redis.windows*.conf 2>/dev/null || true
    echo "  ✅ Redis 준비 완료"
else
    echo "  ⚠️ Redis 다운로드 실패"
fi

# =============================================================================
# 8. 설정 및 데이터 디렉토리 구성
# =============================================================================

echo "8. 설정 파일 및 디렉토리 구성 중..."

# 기본 디렉토리 생성
mkdir -p config data/db data/backup data/logs data/temp

# 설정 파일 복사
if [ -d "$PROJECT_ROOT/config" ]; then
    cp -r "$PROJECT_ROOT/config"/* ./config/
    echo "  ✅ 기존 설정 파일 복사: $(ls config | wc -l) files"
fi

# 🔧 프로덕션용 기본 설정 생성
cat > config/.env << 'EOF'
# PulseOne Production Environment
NODE_ENV=production
PORT=3000

# Database Configuration
DATABASE_TYPE=SQLITE
SQLITE_PATH=./data/db/pulseone.db

# 🔧 자동 초기화 설정
AUTO_INITIALIZE_ON_START=true
SKIP_IF_INITIALIZED=true
FAIL_ON_INIT_ERROR=false

# Redis Configuration (Optional)
REDIS_ENABLED=false
REDIS_HOST=localhost
REDIS_PORT=6379

# Logging
LOG_LEVEL=info
LOG_TO_FILE=true
LOG_FILE_PATH=./data/logs/pulseone.log

# Frontend Static Files
SERVE_FRONTEND=true
FRONTEND_PATH=./frontend
EOF

echo "  ✅ 프로덕션 설정 파일 생성"

# =============================================================================
# 9. Windows 시작 스크립트 개선
# =============================================================================

echo "9. Windows 시작 스크립트 생성 중..."

# install.bat - 초기 설정
cat > install.bat << 'EOF'
@echo off
echo ================================================================
echo PulseOne Industrial IoT Platform - Installation
echo ================================================================

echo [1/4] Creating directories...
if not exist "data" mkdir data
if not exist "data\db" mkdir "data\db"
if not exist "data\logs" mkdir "data\logs" 
if not exist "data\backup" mkdir "data\backup"
if not exist "data\temp" mkdir "data\temp"
if not exist "config" mkdir config

echo [2/4] Checking files...
if not exist pulseone-backend.exe (
    echo ERROR: pulseone-backend.exe not found!
    pause
    exit /b 1
)

if not exist node.exe (
    echo ERROR: node.exe not found!
    pause
    exit /b 1
)

echo [3/4] Setting permissions...
REM Windows doesn't need explicit permissions for executables

echo [4/4] Database initialization...
echo Database will be automatically initialized on first startup.

echo.
echo ================================================================
echo Installation completed successfully!
echo ================================================================
echo.
echo Next steps:
echo 1. Run 'start.bat' to start PulseOne
echo 2. Open http://localhost:3000 in your browser
echo 3. Check 'data\logs' folder for system logs
echo.
pause
EOF

# start.bat - 시스템 시작
cat > start.bat << 'EOF'
@echo off
setlocal enabledelayedexpansion
title PulseOne Industrial IoT Platform

echo ================================================================
echo PulseOne Industrial IoT Platform v2.1.0
echo ================================================================

REM 디렉토리 생성 (누락된 경우)
if not exist "data\db" mkdir "data\db"
if not exist "data\logs" mkdir "data\logs"

REM 파일 존재 확인
echo [1/5] Checking system files...
if not exist pulseone-backend.exe (
    echo ERROR: pulseone-backend.exe not found!
    echo Please run install.bat first.
    pause
    exit /b 1
)

if not exist config\.env (
    echo ERROR: Configuration file not found!
    echo Please check config\.env file.
    pause
    exit /b 1
)

echo     Backend executable: OK
echo     Configuration: OK
if exist sqlite3.exe echo     SQLite: OK
if exist redis-server.exe echo     Redis: OK
if exist collector.exe echo     Collector: OK

REM 포트 확인 (선택사항)
echo [2/5] Checking port availability...
netstat -an | find "LISTENING" | find ":3000" >nul
if !errorlevel! == 0 (
    echo WARNING: Port 3000 is already in use
    echo PulseOne may fail to start or use alternative port
)

REM 🔧 Redis 시작 (선택사항)
echo [3/5] Starting services...
if exist redis-server.exe (
    echo Starting Redis cache server...
    start /b redis-server.exe --port 6379 --bind 127.0.0.1 --maxmemory 256mb
    timeout /t 2 /nobreak > nul
    echo     Redis: Started
) else (
    echo     Redis: Skipped (not available)
)

REM 🔧 Collector 시작 (선택사항)
if exist collector.exe (
    echo Starting data collector...
    start /b collector.exe
    echo     Collector: Started
) else (
    echo     Collector: Skipped (not available)
)

REM 🔧 백엔드 시스템 시작
echo [4/5] Starting PulseOne backend...
echo.
echo ================================================================
echo Starting PulseOne Backend System...
echo.
echo 🔧 Auto-initialization: Enabled
echo 📊 Database: SQLite (auto-created if needed)
echo 🌐 Web interface will be available at: http://localhost:3000
echo 📁 Logs location: data\logs\
echo.
echo Press Ctrl+C to stop the system
echo ================================================================
echo.

REM 백엔드 실행 (foreground)
pulseone-backend.exe

REM 시스템 종료 시 cleanup
echo.
echo [5/5] Shutting down services...
taskkill /f /im redis-server.exe 2>nul
taskkill /f /im collector.exe 2>nul
echo All services stopped.

echo.
pause
EOF

# stop.bat - 시스템 중지
cat > stop.bat << 'EOF'
@echo off
echo Stopping PulseOne services...

taskkill /f /im pulseone-backend.exe 2>nul
taskkill /f /im redis-server.exe 2>nul  
taskkill /f /im collector.exe 2>nul
taskkill /f /im node.exe 2>nul

echo All PulseOne services stopped.
pause
EOF

echo "  ✅ Windows 배치 파일 생성 완료"

# =============================================================================
# 10. 문서화 및 README
# =============================================================================

echo "10. 문서 생성 중..."

cat > README.md << EOF
# PulseOne Industrial IoT Platform - Windows Production Package

## 🚀 빠른 시작

1. **install.bat** 실행 (관리자 권한 권장)
2. **start.bat** 실행 (시스템 시작)
3. 브라우저에서 **http://localhost:3000** 접속

## 📦 패키지 구성

### 핵심 시스템
- **pulseone-backend.exe**: 완전한 백엔드 서버 (Frontend 포함)
- **node.exe**: 포터블 Node.js 런타임
- **frontend/**: 웹 인터페이스 정적 파일들

### 데이터베이스 & 캐시
- **sqlite3.exe**: SQLite 데이터베이스 엔진 
- **redis-server.exe**: Redis 캐시 서버 (선택사항)
- **data/**: 데이터베이스 및 로그 저장소

### 데이터 수집 (선택사항)
- **collector.exe**: C++ 데이터 수집기 (빌드된 경우)

### 설정 및 제어
- **config/**: 시스템 설정 파일들
- **start.bat**: 시스템 시작 스크립트
- **stop.bat**: 시스템 중지 스크립트
- **install.bat**: 초기 설치 스크립트

## ⚡ 주요 기능

### 자동 초기화
- 🔄 첫 실행 시 데이터베이스 자동 생성
- 📊 시스템 테이블 자동 구성  
- 🏭 기본 설정 및 샘플 데이터 생성

### 웹 기반 관리
- 🌐 완전한 웹 대시보드
- 📱 반응형 인터페이스 (모바일 지원)
- 🔐 사용자 인증 및 권한 관리

### 데이터 관리
- 🗄️ SQLite 내장 데이터베이스 (설정 불필요)
- ⚡ Redis 캐시 (성능 향상)
- 📈 실시간 데이터 처리

## 🔧 시스템 요구사항

- **OS**: Windows 10/11 (64-bit)
- **RAM**: 4GB 이상 권장
- **Storage**: 1GB 이상 (데이터 증가에 따라 추가 필요)
- **Network**: 포트 3000 사용 가능

## 📋 설정 정보

### 기본 설정 (.env)
\`\`\`
DATABASE_TYPE=SQLITE
SQLITE_PATH=./data/db/pulseone.db
AUTO_INITIALIZE_ON_START=true
PORT=3000
\`\`\`

### 포트 사용
- **3000**: 웹 인터페이스 및 API
- **6379**: Redis 캐시 (내부 통신)

### 디렉토리 구조
\`\`\`
PulseOne/
├── data/
│   ├── db/          # SQLite 데이터베이스
│   ├── logs/        # 시스템 로그  
│   └── backup/      # 백업 파일
├── config/          # 설정 파일
└── frontend/        # 웹 인터페이스
\`\`\`

## 🔍 문제 해결

### 포트 충돌
- 다른 프로그램이 3000 포트를 사용 중인 경우
- **해결**: 해당 프로그램 종료 후 재시작

### 데이터베이스 초기화 실패  
- **원인**: 권한 부족 또는 디스크 공간 부족
- **해결**: 관리자 권한으로 실행

### Redis 시작 실패
- **영향**: 캐시 비활성화, 기능은 정상 작동
- **해결**: Redis 포함 버전 확인

## 🏭 빌드 정보

- **빌드 시간**: $(date)
- **버전**: PulseOne v${VERSION}  
- **타겟**: Windows x64
- **Node.js**: 18.19.0 (내장)
- **컴파일러**: GCC MinGW-w64 (Collector)

## 🔗 지원 및 문서

- **GitHub**: https://github.com/smart-guard/PulseOne
- **Issues**: GitHub Issues 탭 활용
- **문서**: 프로젝트 위키 참조

---

**PulseOne Industrial IoT Platform**  
*Complete Windows Production Package*
EOF

echo "  ✅ README 문서 생성 완료"

# =============================================================================
# 11. 최종 패키지 검증 및 압축
# =============================================================================

echo "11. 최종 패키지 검증 중..."

cd $DIST_DIR

# 필수 파일 검증
echo "핵심 파일 검증:"
ESSENTIAL_FILES=(
    "$PACKAGE_NAME/pulseone-backend.exe"
    "$PACKAGE_NAME/node.exe"
    "$PACKAGE_NAME/config/.env"
    "$PACKAGE_NAME/start.bat"
    "$PACKAGE_NAME/install.bat"
)

ALL_ESSENTIAL_PRESENT=true
for file in "${ESSENTIAL_FILES[@]}"; do
    if [ -f "$file" ]; then
        SIZE=$(du -h "$file" | cut -f1)
        echo "  ✅ $(basename "$file"): $SIZE"
    else
        echo "  ❌ $(basename "$file"): MISSING"
        ALL_ESSENTIAL_PRESENT=false
    fi
done

# 선택적 파일 확인
echo ""
echo "선택적 구성요소:"
echo "  🗄️ sqlite3.exe: $(if [ -f "$PACKAGE_NAME/sqlite3.exe" ]; then echo "✅"; else echo "⚠️"; fi)"
echo "  ⚡ redis-server.exe: $(if [ -f "$PACKAGE_NAME/redis-server.exe" ]; then echo "✅"; else echo "⚠️"; fi)"
echo "  🔧 collector.exe: $(if [ -f "$PACKAGE_NAME/collector.exe" ]; then echo "✅"; else echo "⚠️"; fi)"
echo "  🌐 frontend/: $(if [ -d "$PACKAGE_NAME/frontend" ]; then echo "✅ $(ls "$PACKAGE_NAME/frontend" | wc -l) files"; else echo "⚠️"; fi)"

if [ "$ALL_ESSENTIAL_PRESENT" = false ]; then
    echo ""
    echo "❌ 필수 파일이 누락되었습니다. 패키징을 중단합니다."
    exit 1
fi

# ZIP 압축
echo ""
echo "12. 최종 압축 중..."
zip -r "${PACKAGE_NAME}_${TIMESTAMP}.zip" $PACKAGE_NAME/ > /dev/null 2>&1

if [ $? -eq 0 ]; then
    PACKAGE_SIZE=$(du -h "${PACKAGE_NAME}_${TIMESTAMP}.zip" | cut -f1)
    
    echo ""
    echo "================================================================="
    echo "🎉 PulseOne Windows 프로덕션 패키지 완성!"
    echo "================================================================="
    echo "📁 파일: ${PACKAGE_NAME}_${TIMESTAMP}.zip"
    echo "💾 크기: $PACKAGE_SIZE"
    echo ""
    echo "✅ 포함된 구성요소:"
    echo "  🖥️  완전한 백엔드 시스템 (Node.js + 자동초기화)"
    echo "  🌐 웹 인터페이스 (React 기반 대시보드)"  
    echo "  🗄️  SQLite 데이터베이스 (자동 생성)"
    echo "  ⚡ Redis 캐시 시스템 (선택사항)"
    echo "  $(if [ -f "$PACKAGE_NAME/collector.exe" ]; then echo "🔧 데이터 수집기 (C++)"; else echo "⚠️  데이터 수집기 (빌드 실패)"; fi)"
    echo "  📋 완전한 Windows 배치 스크립트"
    echo ""
    echo "🚀 Windows 배포 가이드:"
    echo "1. ZIP을 Windows PC에 다운로드"
    echo "2. 압축 해제 (C:\\PulseOne\\ 권장)" 
    echo "3. install.bat 실행 (관리자 권한 권장)"
    echo "4. start.bat 실행 (시스템 시작)"
    echo "5. 브라우저에서 http://localhost:3000 접속"
    echo ""
    echo "🔧 자동 초기화 기능:"
    echo "  • 첫 실행 시 데이터베이스 자동 생성"
    echo "  • 시스템 테이블 및 기본 설정 자동 구성"
    echo "  • 오류 시에도 시스템 계속 시작 (안정성)"
    echo ""
    echo "✅ 완전한 프로덕션 레디 Windows 패키지 완성!"
    
else
    echo "❌ ZIP 압축 실패"
    exit 1
fi