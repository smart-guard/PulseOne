# scripts/prod-build.sh  
#!/bin/bash

echo "🔨 PulseOne 운영 빌드 시작..."

# 운영용 Docker Compose 빌드
docker-compose build --no-cache

echo "✅ 운영 빌드 완료!"
echo "🚀 시작하려면: docker-compose up -d"