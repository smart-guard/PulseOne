#!/bin/bash
# modbus_launcher.sh - PulseOne Modbus Slave Production Launcher
# export-gateway/scripts/gateway_launcher.sh 와 동일한 패턴

BIN_DIR="${BIN_DIR:-/app/core/modbus-slave}"
BIN_PATH="${BIN_PATH:-$BIN_DIR/bin/pulseone-modbus-slave}"
LOG_DIR="${LOG_DIR:-/app/logs}"

mkdir -p "$LOG_DIR"

echo "-------------------------------------------------------"
echo "🔌 PulseOne Modbus Slave Launcher"
echo "📅 Started at: $(date)"
echo "-------------------------------------------------------"

# 1. 바이너리 없으면 빌드
if [ ! -f "$BIN_PATH" ]; then
    echo "⚙️  바이너리 없음 — 빌드 시작..."
    cd "$BIN_DIR" && make -j$(nproc) 2>&1 | tee "$LOG_DIR/modbus_slave_build.log"
    if [ $? -ne 0 ]; then
        echo "❌ 빌드 실패. 로그: $LOG_DIR/modbus_slave_build.log"
        exit 1
    fi
    echo "✅ 빌드 완료"
fi

chmod +x "$BIN_PATH"

echo "🚀 Modbus Slave Supervisor 시작 (DB 자동 감지)"
echo "   Binary : $BIN_PATH"
echo "   SQLITE : ${SQLITE_PATH:-미설정}"
echo "-------------------------------------------------------"

# 2. Supervisor 모드로 실행 (인자 없음 = Supervisor)
exec "$BIN_PATH"
