# scripts/dev-frontend.sh
#!/bin/bash

echo "🚀 PulseOne Frontend 개발 환경 시작..."

# frontend 폴더가 없으면 생성
if [ ! -d "frontend" ]; then
    echo "📁 frontend 폴더 생성 중..."
    mkdir -p frontend/src/{components,pages,hooks,store,services,utils,types}
    mkdir -p frontend/src/components/{common,layout}
    mkdir -p frontend/public
fi

# package.json이 없으면 기본 파일들 생성
if [ ! -f "frontend/package.json" ]; then
    echo "📦 기본 파일들 생성 중..."
    # 여기서 기본 파일들을 생성하는 로직
fi

# Docker Compose로 개발 환경 시작
echo "🐳 Docker 개발 환경 시작..."
docker-compose -f docker-compose.dev.yml up -d frontend

echo "✅ 개발 서버가 시작되었습니다!"
echo "🌐 Frontend: http://localhost:5173"
echo "🔧 Backend API: http://localhost:3000"
echo ""
echo "📋 유용한 명령어들:"
echo "  로그 확인: docker-compose -f docker-compose.dev.yml logs -f frontend"
echo "  컨테이너 접속: docker exec -it pulseone-frontend-dev sh"
echo "  개발 환경 중지: docker-compose -f docker-compose.dev.yml down"

