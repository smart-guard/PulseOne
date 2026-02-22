# PulseOne 작업 가이드

## ⚡ 지금 당장 할 일

### 1. 실행 중인 빌드 강제 종료
```bash
# 모든 실행 중인 Docker 컨테이너 중지
docker ps -q | xargs docker kill 2>/dev/null || echo "no containers"

# 혹시 남아있는 make 프로세스 정리
pkill -f "make" 2>/dev/null || true
```

---

## 🖥️ 개발 서버 기동 (백엔드/프론트엔드 테스트)

```bash
cd /Users/kyungho/Project/PulseOne

# 전체 개발 스택 기동 (backend + frontend + redis + rabbitmq + influxdb)
docker compose -f docker/docker-compose.yml up -d backend frontend redis rabbitmq

# 기동 확인
docker compose -f docker/docker-compose.yml ps
```

접속:
- 프론트엔드: http://localhost:5173
- 백엔드 API: http://localhost:3000

---

## ✅ Collector 할당 기능 테스트

### 이번에 구현한 것
- 사이트 등록 시 Collector 자동 생성 (테넌트 쿼터 내)
- 사이트 삭제 전 Collector 연결 체크
- Collector 재배정 (연결 장치 0개일 때만 가능)
- 사이트 관리 페이지: Collector 사용/온라인/오프라인 StatCard
- 사이트 상세 모달: Collector 현황 테이블
- 고객사 상세 모달: Collector 할당 현황

### UI 테스트 순서

1. **고객사(Tenant) 페이지** → 고객사 상세 클릭
   - `Collector 현황: N/M 대 사용` 표시 확인

2. **사이트(Site) 관리 페이지**
   - 상단 상태카드에 `Collector 사용`, `온라인 Collector`, `오프라인 Collector` 표시 확인

3. **새 사이트 등록** 클릭
   - 하단 `Collector 설정` 섹션 확인 (이름/설명 입력 가능)
   - 등록 완료 후 → DB에서 edge_servers 자동 생성 확인

4. **쿼터 초과 테스트**
   - 테넌트의 `max_edge_servers` 수만큼 사이트가 이미 있을 때 추가 등록 시도
   - "Collector 할당 한도 초과" 에러 확인

5. **사이트 상세** 클릭
   - 하단 `Collector 현황` 테이블 (이름/온라인상태/연결장치 수) 확인

### API 직접 테스트

```bash
# 로그인 토큰 발급
TOKEN=$(curl -s -X POST http://localhost:3000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123"}' | jq -r '.token')

# Collector 쿼터 현황 (온라인/오프라인 포함)
curl -s -H "Authorization: Bearer $TOKEN" \
  http://localhost:3000/api/collectors/quota/status | jq

# 사이트 1번의 Collector 목록
curl -s -H "Authorization: Bearer $TOKEN" \
  http://localhost:3000/api/collectors/by-site/1 | jq

# Collector 재배정 (site_id 2로 이동)
curl -s -X PATCH -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"site_id": 2}' \
  http://localhost:3000/api/collectors/1/reassign | jq
```

### DB 직접 확인

```bash
docker exec -it docker-backend-1 \
  node -e "
    const db = require('./lib/database/DatabaseFactory').getInstance();
    db.raw('SELECT id, server_name, site_id, status, tenant_id FROM edge_servers WHERE is_deleted=0').then(r => {
      console.table(r);
      process.exit(0);
    });
  "
```

---

## 🪟 Windows 바이너리 테스트 (Wine)

> Windows 빌드 패키지(`dist/`)가 있어야 실행 가능

```bash
# 1. 먼저 개발 스택이 실행 중인지 확인
docker compose -f docker/docker-compose.yml ps

# 2. Windows Collector를 Wine으로 실행
docker compose -f docker/docker-compose.win-test.yml up -d win-collector

# 3. 로그 확인
docker logs pulseone-win-collector -f
```

Windows 전체 테스트는 `dist/deploy-vX.X.X/` 빌드 완료 후 진행.

---

## 📦 Windows 배포 패키지 빌드

```bash
cd /Users/kyungho/Project/PulseOne
SKIP_FRONTEND=true bash deploy-windows.sh
```

완료 후 `dist/deploy-v*/` 폴더 확인:
```
pulseone-collector.exe
pulseone-export-gateway.exe
drivers/
  modbus_driver.dll  bacnet_driver.dll  mqtt_driver.dll
  opcua_driver.dll   ros_driver.dll     ble_driver.dll   httprest_driver.dll
```
