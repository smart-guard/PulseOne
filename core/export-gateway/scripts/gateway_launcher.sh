#!/bin/bash
# gateway_launcher.sh - PulseOne Export Gateway Production Launcher (DB-Agnostic Version)

# [주의] set -e 제거: --list-gateways 실패 시 스크립트 전체 종료 방지
# 루프 안에서 개별 실패를 처리함

# [환경변수 설정] 배포 환경에 따라 오버라이드 가능
# DB_TYPE, DB_PRIMARY_HOST, DB_PRIMARY_USER 등이 설정되어 있으면 C++ 바이너리가 이를 사용함
BIN_PATH="${BIN_PATH:-./core/export-gateway/bin/export-gateway}"
LOG_DIR="${LOG_DIR:-/app/logs}"
CHECK_INTERVAL="${CHECK_INTERVAL:-5}" # 새로운 게이트웨이 체크 간격 (초)

mkdir -p "$LOG_DIR"

echo "-------------------------------------------------------"
echo "🚀 PulseOne Export Gateway Production Launcher"
echo "🌐 DB-Agnostic Mode (using C++ internal discovery)"
echo "📅 Started at: $(date)"
echo "-------------------------------------------------------"

# 1. 전용 모드 체크 (특정 ID 고정 실행)
if [ ! -z "$GATEWAY_ID" ]; then
    echo "📌 Dedicated Mode: Launching Instance ID $GATEWAY_ID"
    exec "$BIN_PATH" --id "$GATEWAY_ID"
fi

# 2. 자동 감지 및 핫-리로딩(Hot-Reloading) 루프
declare -A RUNNING_PIDS

echo "🔍 Starting Discovery Loop via C++ Binary..."

while true; do
    # C++ 바이너리의 --list-gateways 기능을 사용하여 ID 목록 조회
    # 이 과정에서 C++ 내부의 DatabaseManager가 Pg/Sqlite 등을 자동으로 처리함
    CURRENT_IDS=$($BIN_PATH --list-gateways 2>/dev/null || echo "")

    if [ ! -z "$CURRENT_IDS" ]; then
        # 신규 ID 감지 및 실행
        for GID in $CURRENT_IDS; do
            if [[ "$GID" =~ ^[0-9]+$ ]]; then # 숫자 형태인지 확인
                if [ -z "${RUNNING_PIDS[$GID]}" ] || ! kill -0 "${RUNNING_PIDS[$GID]}" 2>/dev/null; then
                    echo "🎬 [$(date +'%Y-%m-%d %H:%M:%S')] [AUTO] Launching Gateway Instance ID: $GID"
                    "$BIN_PATH" --id "$GID" >> "$LOG_DIR/gateway_$GID.log" 2>&1 &
                    RUNNING_PIDS[$GID]=$!
                    echo "✅ Instance $GID started with PID ${RUNNING_PIDS[$GID]}"
                fi
            fi
        done
    fi

    sleep "$CHECK_INTERVAL"
done

# 컨테이너 종료 시 정리
trap "echo 'Cleaning up processes...'; kill 0" SIGINT SIGTERM
wait
