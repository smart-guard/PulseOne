#!/bin/bash
# PulseOne 빠른 개발 환경 시작

set -e

# 색상 정의
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}🚀 PulseOne 빠른 개발 환경 시작${NC}"

# 옵션 파싱
AUTO_BUILD=true
AUTO_RUN=false
CLEAN_BUILD=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-build)
            AUTO_BUILD=false
            shift
            ;;
        --auto-run)
            AUTO_RUN=true
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        -h|--help)
            echo "사용법: $0 [옵션]"
            echo "옵션:"
            echo "  --no-build    자동 빌드 비활성화"
            echo "  --auto-run    빌드 후 자동 실행"
            echo "  --clean       완전 정리 후 시작"
            echo "  -h, --help    도움말 표시"
            exit 0
            ;;
        *)
            echo "알 수 없는 옵션: $1"
            exit 1
            ;;
    esac
done

# 완전 정리 (선택적)
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}🧹 완전 정리 중...${NC}"
    docker-compose -f docker-compose.dev.yml down --volumes --remove-orphans
    docker system prune -f
fi

# 환경 변수 설정
export AUTO_BUILD=$AUTO_BUILD
export AUTO_RUN=$AUTO_RUN

# 개발 환경 시작
echo -e "${BLUE}🐳 Docker 개발 환경 시작...${NC}"
docker-compose -f docker-compose.dev.yml up -d

# 상태 확인
echo -e "${BLUE}📊 서비스 상태 확인...${NC}"
sleep 5
docker-compose -f docker-compose.dev.yml ps

# 접속 정보 출력
echo ""
echo -e "${GREEN}✅ 개발 환경 시작 완료!${NC}"
echo ""
echo "🔗 접속 정보:"
echo "  Collector 컨테이너: docker exec -it pulseone-collector-dev bash"
echo "  Backend API: http://localhost:3000"
echo "  InfluxDB: http://localhost:8086"
echo "  RabbitMQ 관리: http://localhost:15672"
echo ""
echo "🔧 유용한 명령어들:"
echo "  docker-compose -f docker-compose.dev.yml logs -f collector"
echo "  docker-compose -f docker-compose.dev.yml down"
echo ""

# 자동 빌드가 활성화된 경우 빌드 로그 표시
if [ "$AUTO_BUILD" = true ]; then
    echo -e "${BLUE}📋 빌드 로그 모니터링 (Ctrl+C로 중단):${NC}"
    docker-compose -f docker-compose.dev.yml logs -f collector
fi