#!/bin/bash

set -e  # 오류 발생 시 즉시 종료

echo "🚀 PulseOne Collector 설치 시작..."

# 🔐 1. 루트 권한 확인
if [ "$EUID" -ne 0 ]; then
  echo "❌ 이 스크립트는 root 권한으로 실행되어야 합니다."
  echo "🔧 sudo ./install_collector.sh"
  exit 1
fi

# 📁 2. 로그 디렉토리 생성
mkdir -p /opt/pulseone/logs
mkdir -p /opt/pulseone/logs/packets
echo "📁 로그 디렉토리 생성 완료 (/opt/pulseone/logs)"

# 🔁 3. logrotate 설정 배포
LOGROTATE_DST="/etc/logrotate.d/pulseone_collector"
LOGROTATE_SRC="./logrotate/pulseone_collector"

if [ -f "$LOGROTATE_DST" ]; then
  echo "⚠️ logrotate 설정이 이미 존재합니다: $LOGROTATE_DST"
else
  cp "$LOGROTATE_SRC" "$LOGROTATE_DST"
  chmod 644 "$LOGROTATE_DST"
  echo "✅ logrotate 설정 배포 완료: $LOGROTATE_DST"
fi

# 📦 4. SQLite 파일 초기 생성 (선택)
SQLITE_DB="./config/db.sqlite"
if [ ! -f "$SQLITE_DB" ]; then
  echo "🪶 SQLite 초기 DB 생성 중: $SQLITE_DB"
  mkdir -p ./config
  touch "$SQLITE_DB"
  chmod 644 "$SQLITE_DB"
else
  echo "ℹ️ SQLite DB 파일 이미 존재함: $SQLITE_DB"
fi

# 🔑 5. 권한 확인
chown -R $(whoami):$(whoami) ./logs ./config

# ✅ 완료 메시지
echo "✅ PulseOne Collector 설치가 완료되었습니다!"
echo "📦 logs → /opt/pulseone/logs"
echo "🔄 logrotate 설정 → $LOGROTATE_DST"
echo "🧩 SQLite DB → $SQLITE_DB"
