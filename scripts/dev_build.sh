#!/bin/bash

set -e

# 🔹 실행 환경: 기본값 dev
ENV_STAGE=${1:-dev}
ENV_FILE="./config/.env.${ENV_STAGE}"

echo "🛠️ Local Docker CI/CD - PulseOne 전체 개발 환경 구축 (${ENV_STAGE})"

# 🔸 환경파일 존재 확인
if [ ! -f "$ENV_FILE" ]; then
  echo "❌ 환경파일 없음: $ENV_FILE"
  exit 1
fi
echo "✅ 환경파일 확인 완료: $ENV_FILE"

# 🔹 Docker Compose로 전체 서비스 빌드 및 실행
echo "📦 Docker 개발 컨테이너 빌드 시작..."
ENV_STAGE=$ENV_STAGE docker compose -f docker-compose.dev.yml up -d --build

# 🔹 의존 서비스 준비 대기
echo "⏳ 의존 서비스 대기 중 (Postgres, Redis, RabbitMQ, InfluxDB 등)"
sleep 10

# 🔹 Collector 빌드 및 테스트
echo "🧪 Collector 빌드 및 테스트 실행"
docker exec pulseone-collector-dev make -C /app/collector ci || echo "⚠️ Collector 테스트 실패 (무시됨)"

# 🔹 Backend 유닛 테스트
echo "🧪 Backend 유닛 테스트 실행"
docker exec pulseone-backend-dev bash -c "cd /app/backend && npm test || echo '⚠️ Backend 테스트 실패 (무시됨)'"

echo "✅ 개발 환경 실행 완료!"

tree -I 'node_modules|.git|.DS_Store|*.log|dist|build|venv|__pycache__' > structure.txt