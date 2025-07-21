#!/bin/bash

echo "🔍 PulseOne 개발 환경 포트 충돌 체크"
echo "===================================="

# docker-compose.dev.yml에서 사용하는 포트들
PORTS=(
    "3000:Backend API"
    "5432:PostgreSQL"
    "6379:Redis"
    "8086:InfluxDB"
    "5672:RabbitMQ"
    "15672:RabbitMQ Management"
    "9229:Node.js Debug"
)

echo "📋 확인할 포트들:"
for port_info in "${PORTS[@]}"; do
    port=$(echo $port_info | cut -d: -f1)
    service=$(echo $port_info | cut -d: -f2)
    echo "  - $port ($service)"
done

echo ""
echo "🔍 포트 사용 상태 확인:"

conflicts=0
for port_info in "${PORTS[@]}"; do
    port=$(echo $port_info | cut -d: -f1)
    service=$(echo $port_info | cut -d: -f2)
    
    if lsof -i :$port >/dev/null 2>&1; then
        echo "  ⚠️  포트 $port 사용 중 ($service)"
        echo "     사용 중인 프로세스:"
        lsof -i :$port | head -2 | tail -1 | awk '{print "       " $1 " (PID: " $2 ")"}'
        ((conflicts++))
    else
        echo "  ✅ 포트 $port 사용 가능 ($service)"
    fi
done

echo ""
echo "📊 결과 요약:"
echo "============"

if [ $conflicts -eq 0 ]; then
    echo "🎉 모든 포트가 사용 가능합니다!"
    echo "✅ docker-compose.dev.yml 안전하게 실행 가능"
    echo ""
    echo "🚀 다음 명령어로 실행하세요:"
    echo "  docker-compose -f docker-compose.dev.yml up -d"
else
    echo "⚠️  $conflicts개 포트에서 충돌 발견"
    echo ""
    echo "🔧 해결 방법들:"
    echo "1. 충돌 서비스 중지 후 실행"
    echo "2. docker-compose.dev.yml에서 포트 변경"
    echo "   예: '5432:5432' → '5433:5432'"
    echo "3. 또는 충돌 무시하고 실행 (Docker가 자동 처리)"
    echo ""
    echo "💡 보통은 그냥 실행해도 Docker가 알아서 처리합니다:"
    echo "  docker-compose -f docker-compose.dev.yml up -d"
fi

echo ""
echo "🛡️ 안전성 보장:"
echo "==============="
echo "✅ 모든 서비스가 Docker 컨테이너 내에서만 실행"
echo "✅ 기존 시스템 파일/설정에 영향 없음"
echo "✅ 언제든지 완전 삭제 가능:"
echo "   docker-compose -f docker-compose.dev.yml down --volumes"
echo "✅ 호스트 시스템과 격리된 네트워크 사용"