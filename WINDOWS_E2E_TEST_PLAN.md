# PulseOne Windows E2E Verification Plan (Docker-Only)

> **최종 업데이트**: 2026-02-20 15:02 KST  
> **원칙**: 모든 명령어는 `docker exec`를 통해서만 실행. 맥 터미널 직접 접근 금지.

---

## 📋 환경 현황

| 항목 | 상태 | 비고 |
|------|------|------|
| `bin-windows/collector.exe` | ✅ | 2026-02-17 빌드 |
| `bin-windows/export-gateway.exe` | ✅ | **2026-02-20 갱신** (Redis 장애내성, config 배열unwrap 반영) |
| `docker-compose.win-test.yml` | ✅ | **업데이트**: redis/backend/modbus-sim 추가 |
| Phase 0-2 (알람 트리거/KST타임존) | ✅ | 이미 완료 |
| Phase 3 (Export 검증) | 🔧 진행 중 | Echo 서버 타겟 설정 후 재확인 |

---

## 🐛 버그 수정 내역

### [BUG-1] ✅ 타임존 버그 수정 완료
- `TZ=KST-9` (POSIX 형식), Wine 레지스트리 KST 자동 설정
- `datetime('now','localtime')` 적용

### [BUG-2] ✅ Export Gateway Redis 장애 내성
- Redis 없어도 프로세스 종료 안함, 백그라운드 재연결

### [BUG-3] ✅ DB config 배열 unwrap
- `export_targets.config` 컬럼이 `[{...}]` 배열 시 자동 `[0]` unwrap

### [BUG-4] ✅ Windows 매크로 충돌
- `ExportConstants.h` `ERROR` → `ERROR_MSG` (`winerror.h` 충돌 해결)

---

## ✅ Phase 0 - 사전 점검 (완료)

알람 룰 5개 확인 (rule_id 2-6)

---

## ✅ Phase 1 - 타임존 버그 수정 (완료)

- `TZ=KST-9`, wine-entrypoint.sh KST 레지스트리 등록
- 검증: `occurrence_time: 2026-02-20 06:36:02` ✅

---

## ✅ Phase 2 - Modbus 알람 트리거 (완료)

### HMI-001 데이터포인트

| Point ID | 이름 | Modbus 주소 | 알람 룰 |
|----------|------|------------|--------|
| 1 | WLS.PV | 100 | PV Status Alarm |
| 2 | WLS.SRS | 101 | SRS Status Alarm |
| 3 | WLS.SCS | 102 | SCS Status Alarm |
| 4 | WLS.SSS | 200 | SSS High Alarm (>100) |
| 5 | WLS.SBV | 201 | SBV High Alarm (>200) |

### 알람 트리거/해제 명령

```bash
# 트리거: WLS.SSS에 150 주입
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
async function run(){
  await c.connectTCP('simulator-modbus',{port:50502});c.setID(1);
  await c.writeRegister(200,5);
  await new Promise(r=>setTimeout(r,6000));
  await c.writeRegister(200,150);c.close();
}run();"

# 해제
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
async function run(){await c.connectTCP('simulator-modbus',{port:50502});c.setID(1);await c.writeRegister(200,5);c.close();}run();"

# DB 검증
docker exec pulseone-win-backend sqlite3 /app/data/db/pulseone.db \
  "SELECT id, rule_id, occurrence_time, cleared_time, state FROM alarm_occurrences ORDER BY id DESC LIMIT 5;"
```

---

## � Phase 3 - Export Gateway 검증

### Step 3-1: 컨테이너 기동

```bash
docker compose -f docker/docker-compose.win-test.yml up -d
```

### Step 3-2: Echo 서버 타겟 DB 등록

```bash
docker exec pulseone-win-backend sqlite3 /app/data/db/pulseone.db "
INSERT OR IGNORE INTO export_targets (name, target_type, config, is_enabled)
VALUES ('local-echo', 'HTTP',
  '[{\"url\":\"http://backend:9999\",\"method\":\"POST\",\"headers\":{\"Content-Type\":\"application/json\"}}]',
  1);
SELECT id FROM export_targets WHERE name='local-echo';"
```

매핑 등록 (local-echo 타겟 ID 확인 후):
```bash
# TARGET_ID = 위 쿼리 결과 ID
docker exec pulseone-win-backend sqlite3 /app/data/db/pulseone.db "
INSERT OR IGNORE INTO export_target_mappings (target_id, point_id, target_field_name, is_enabled)
  VALUES (TARGET_ID, 1, 'point_1', 1),
         (TARGET_ID, 2, 'point_2', 1),
         (TARGET_ID, 3, 'point_3', 1),
         (TARGET_ID, 4, 'point_4', 1),
         (TARGET_ID, 5, 'point_5', 1);"
```

edge_servers에 gateway 6번 연결:
```bash
docker exec pulseone-win-backend sqlite3 /app/data/db/pulseone.db "
UPDATE edge_servers SET export_gateway_id=6 WHERE id=1;"
```

### Step 3-3: Echo 서버 기동 (backend 컨테이너)

```bash
docker exec -d pulseone-win-backend node -e "
const http=require('http');
http.createServer((req,res)=>{let b='';req.on('data',d=>b+=d);
req.on('end',()=>{console.log('[ECHO]',new Date().toISOString(),b.substring(0,300));res.end('OK');});
}).listen(9999,()=>console.log('[ECHO] listening on 9999'));"
```

### Step 3-4: win-gateway 재시작 후 알람 트리거

```bash
# gateway 재시작 (새 타겟 로드)
docker restart pulseone-win-gateway

# 알람 트리거
docker exec pulseone-win-sim-modbus node -e "..."

# gateway 로그 확인
docker logs pulseone-win-gateway --tail 30 2>&1 | grep -iE "export|alarm|echo|error"

# echo 서버 수신 로그 (backend 컨테이너 로그)
docker logs pulseone-win-backend --tail 20
```

---

## 📊 최종 테스트 결과

| Phase | 항목 | 결과 | 비고 |
|-------|------|------|------|
| 0 | Win Collector 구동 | ✅ | 5개 포인트 수집 |
| 0 | Win Gateway 구동 | ✅ | |
| 0 | 알람 룰 5개 확인 | ✅ | |
| 1 | 타임존 버그 수정 | ✅ | KST-9 |
| 2 | SSS 알람 트리거/해제 | ✅ | |
| 3 | Echo 서버 Export 수신 | 🔧 진행 예정 | |

---

## 🔧 트러블슈팅

- **DB Lock**: `docker compose down` 후 `data/db/*.db-wal`, `*.db-shm` 삭제
- **Wine 오류**: `docker logs pulseone-win-collector 2>&1 | grep -i wine`
- **알람 미발생**: `docker logs pulseone-win-collector --tail 10`
- **타임존**: `TZ=KST-9` 필수 (MinGW `TZ=Asia/Seoul` 미지원)
- **export-gateway.exe 재빌드**: `make CROSS_COMPILE_WINDOWS=1 all -j2` 후 `cp bin-win/export-gateway.exe bin-windows/`
