# scripts/dev-setup.sh
#!/bin/bash

echo "🛠️  PulseOne 개발 환경 초기 설정..."

# 환경변수 파일 생성
if [ ! -f ".env" ]; then
    echo "📝 .env 파일 생성 중..."
    cat > .env << EOF
# Database
POSTGRES_DB=pulseone_dev
POSTGRES_USER=postgres
POSTGRES_PASSWORD=postgres123

# Redis
REDIS_PASSWORD=redis123

# InfluxDB
INFLUX_USERNAME=admin
INFLUX_PASSWORD=influx123
INFLUX_ORG=pulseone
INFLUX_BUCKET=telemetry_dev
INFLUX_TOKEN=dev-token-123

# RabbitMQ
RABBITMQ_USER=admin
RABBITMQ_PASSWORD=rabbitmq123

# Development
NODE_ENV=development
LOG_LEVEL=debug
EOF
    echo "✅ .env 파일이 생성되었습니다."
fi

# Frontend 환경변수 파일
if [ ! -f "frontend/.env" ]; then
    echo "📝 frontend/.env 파일 생성 중..."
    cat > frontend/.env << EOF
VITE_API_BASE_URL=http://localhost:3000/api
VITE_WEBSOCKET_URL=ws://localhost:3000
VITE_APP_TITLE=PulseOne Dashboard
VITE_DEBUG_MODE=true
EOF
    echo "✅ frontend/.env 파일이 생성되었습니다."
fi

# Docker 네트워크 생성
echo "🌐 Docker 네트워크 설정 중..."
docker network create pulseone-dev-network 2>/dev/null || true

echo "🎉 개발 환경 설정이 완료되었습니다!"
echo ""
echo "다음 단계:"
echo "1. 개발 환경 시작: ./scripts/dev-frontend.sh"
echo "2. 전체 시스템 시작: docker-compose -f docker-compose.dev.yml up -d"