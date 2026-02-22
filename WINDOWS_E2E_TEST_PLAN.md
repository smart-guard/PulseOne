# PulseOne Windows E2E 검증 플랜 (Docker win-test 환경)

> **최종 업데이트**: 2026-02-22 KST  
> **원칙**: 모든 명령어는 `docker exec` 또는 `docker run`을 통해서만 실행. **맥 터미널 직접 실행 금지.**

---

## 📐 테스트 파이프라인

```
[Modbus Simulator]
  ↓ TCP:50502
[win-collector.exe]  ← Wine/Linux
  ↓ Redis PUBLISH (realtime data + alarms:*)
[Redis]
  ↓ SUBSCRIBE
[win-gateway.exe]  ← Wine/Linux
  ↓ payload template 변환
[외부 Export Target (HTTP Echo)]
```

---

## 🐳 컨테이너 맵

| 컨테이너 | 역할 |
|---------|------|
| `pulseone-win-redis` | Redis 7 — 데이터 버스 |
| `pulseone-win-sim-modbus` | Node Modbus Simulator — 포인트 값 제어 |
| `pulseone-win-collector` | collector.exe (Wine) — 수집·알람 평가 |
| `pulseone-win-gateway` | export-gateway.exe (Wine) — 알람 구독·외부 전달 |

---

## 📋 HMI-001 포인트-알람 룰 매핑

| Point ID | 이름 | Modbus Addr | 타입 | 알람 룰 ID | 조건 | 임계값 |
|----------|------|------------|------|-----------|------|--------|
| 1 | WLS.PV | 100 (Coil) | BOOL | 3 | digital on_true | TRUE면 발생 |
| 2 | WLS.SRS | 101 (Coil) | BOOL | 4 | digital on_true | TRUE면 발생 |
| 3 | WLS.SCS | 102 (Coil) | BOOL | 5 | digital on_true | TRUE면 발생 |
| 4 | WLS.SSS | 200 (HR) | UINT16 | 2 | analog HIGH | > 100 |
| 5 | WLS.SBV | 201 (HR) | UINT16 | 6 | analog HIGH | > 200 |

---

## ⚡ 사전 점검

```bash
# 전체 기동
docker compose -f docker/docker-compose.win-test.yml up -d

# 컨테이너 상태 확인
docker compose -f docker/docker-compose.win-test.yml ps

# Collector 5포인트 GOOD 확인
docker logs pulseone-win-collector 2>&1 | grep "Quality: GOOD" | tail -5

# Gateway Redis 연결 확인
docker logs pulseone-win-gateway 2>&1 | grep "Redis.*성공"
```

---

## ✅ Phase 1 — Modbus 데이터 수집 → Redis 저장

**목표**: Collector가 5포인트 모두 `Quality: GOOD`으로 읽고 Redis에 저장하는지 확인.

```bash
# Collector Modbus 읽기 GOOD 확인
docker logs pulseone-win-collector 2>&1 | grep "Quality: GOOD" | tail -5

# Redis 현재값 확인
docker exec pulseone-win-redis redis-cli KEYS "device:*"
docker exec pulseone-win-redis redis-cli GET "device:2:current"
```

**기대값**:
```
WLS.PV  Addr:100 → Value:0 (Quality: GOOD)
WLS.SRS Addr:101 → Value:0 (Quality: GOOD)
WLS.SCS Addr:102 → Value:0 (Quality: GOOD)
WLS.SSS Addr:200 → Value:5 (Quality: GOOD)
WLS.SBV Addr:201 → Value:100 (Quality: GOOD)
```

---

## ✅ Phase 2 — 5포인트 알람 트리거/해제 사이클 (HMI-001)

**목표**: 5개 포인트 각각 알람 발생 → DB 기록(state=active) → 해제(state=cleared) 확인.

### 사전: alarm_occurrences 초기화 (클린 테스트)

```bash
docker run --rm \
  -v /Users/kyungho/Project/PulseOne/data:/data \
  alpine sh -c "apk add sqlite -q && \
  sqlite3 /data/db/pulseone.db 'DELETE FROM alarm_occurrences;' && \
  echo '초기화 완료'"
```

### 사전: 시뮬레이터 값 정상화

```bash
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
c.connectTCP('simulator-modbus',{port:50502}).then(async()=>{
  c.setID(1);
  await c.writeRegister(200,5);    // SSS=5 정상
  await c.writeRegister(201,100);  // SBV=100 정상
  await c.writeCoil(100,false);    // PV=0 정상
  await c.writeCoil(101,false);    // SRS=0 정상
  await c.writeCoil(102,false);    // SCS=0 정상
  console.log('정상화 완료');c.close();
});"
```

---

### Test 1: WLS.SSS (addr=200, analog, 임계값 >100)

```bash
# 트리거: 150 주입
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
c.connectTCP('simulator-modbus',{port:50502}).then(async()=>{
  c.setID(1);await c.writeRegister(200,150);
  console.log('[T1] SSS=150 트리거');c.close();
});"

sleep 4

# DB 확인 (state=active 기대)
docker run --rm -v /Users/kyungho/Project/PulseOne/data:/data \
  alpine sh -c "apk add sqlite -q && sqlite3 /data/db/pulseone.db \
  'SELECT id,rule_id,state,occurrence_time FROM alarm_occurrences ORDER BY id DESC LIMIT 3;'"

# 해제: 5로 복원
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
c.connectTCP('simulator-modbus',{port:50502}).then(async()=>{
  c.setID(1);await c.writeRegister(200,5);
  console.log('[T1] SSS=5 해제');c.close();
});"

sleep 4

# DB 확인 (state=cleared 기대)
docker run --rm -v /Users/kyungho/Project/PulseOne/data:/data \
  alpine sh -c "apk add sqlite -q && sqlite3 /data/db/pulseone.db \
  'SELECT id,rule_id,state,occurrence_time,cleared_time FROM alarm_occurrences ORDER BY id DESC LIMIT 3;'"
```

---

### Test 2: WLS.SBV (addr=201, analog, 임계값 >200)

```bash
# 트리거: 300 주입
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
c.connectTCP('simulator-modbus',{port:50502}).then(async()=>{
  c.setID(1);await c.writeRegister(201,300);
  console.log('[T2] SBV=300 트리거');c.close();
});"

sleep 4

# 해제: 100으로 복원
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
c.connectTCP('simulator-modbus',{port:50502}).then(async()=>{
  c.setID(1);await c.writeRegister(201,100);
  console.log('[T2] SBV=100 해제');c.close();
});"
```

---

### Test 3: WLS.PV (addr=100, Coil/BOOL, digital on_true)

```bash
# 트리거: TRUE
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
c.connectTCP('simulator-modbus',{port:50502}).then(async()=>{
  c.setID(1);await c.writeCoil(100,true);
  console.log('[T3] PV=TRUE 트리거');c.close();
});"

sleep 4

# 해제: FALSE
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
c.connectTCP('simulator-modbus',{port:50502}).then(async()=>{
  c.setID(1);await c.writeCoil(100,false);
  console.log('[T3] PV=FALSE 해제');c.close();
});"
```

---

### Test 4: WLS.SRS (addr=101, Coil/BOOL, digital on_true)

```bash
# 트리거
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
c.connectTCP('simulator-modbus',{port:50502}).then(async()=>{
  c.setID(1);await c.writeCoil(101,true);
  console.log('[T4] SRS=TRUE 트리거');c.close();
});"

sleep 4

# 해제
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
c.connectTCP('simulator-modbus',{port:50502}).then(async()=>{
  c.setID(1);await c.writeCoil(101,false);
  console.log('[T4] SRS=FALSE 해제');c.close();
});"
```

---

### Test 5: WLS.SCS (addr=102, Coil/BOOL, digital on_true)

```bash
# 트리거
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
c.connectTCP('simulator-modbus',{port:50502}).then(async()=>{
  c.setID(1);await c.writeCoil(102,true);
  console.log('[T5] SCS=TRUE 트리거');c.close();
});"

sleep 4

# 해제
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
c.connectTCP('simulator-modbus',{port:50502}).then(async()=>{
  c.setID(1);await c.writeCoil(102,false);
  console.log('[T5] SCS=FALSE 해제');c.close();
});"
```

---

### 전체 알람 사이클 일괄 실행 스크립트

```bash
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');
async function sleep(ms){return new Promise(r=>setTimeout(r,ms));}
async function run(){
  const c=new M();
  await c.connectTCP('simulator-modbus',{port:50502});
  c.setID(1);

  // 초기화
  await c.writeRegister(200,5); await c.writeRegister(201,100);
  await c.writeCoil(100,false); await c.writeCoil(101,false); await c.writeCoil(102,false);
  console.log('[INIT] 정상화'); await sleep(3000);

  // T1: SSS
  await c.writeRegister(200,150); console.log('[T1] SSS=150 트리거'); await sleep(4000);
  await c.writeRegister(200,5);   console.log('[T1] SSS=5 해제');    await sleep(3000);

  // T2: SBV
  await c.writeRegister(201,300); console.log('[T2] SBV=300 트리거'); await sleep(4000);
  await c.writeRegister(201,100); console.log('[T2] SBV=100 해제');   await sleep(3000);

  // T3: PV
  await c.writeCoil(100,true);  console.log('[T3] PV=TRUE 트리거');  await sleep(4000);
  await c.writeCoil(100,false); console.log('[T3] PV=FALSE 해제');   await sleep(3000);

  // T4: SRS
  await c.writeCoil(101,true);  console.log('[T4] SRS=TRUE 트리거'); await sleep(4000);
  await c.writeCoil(101,false); console.log('[T4] SRS=FALSE 해제');  await sleep(3000);

  // T5: SCS
  await c.writeCoil(102,true);  console.log('[T5] SCS=TRUE 트리거'); await sleep(4000);
  await c.writeCoil(102,false); console.log('[T5] SCS=FALSE 해제');  await sleep(2000);

  console.log('[DONE] 5포인트 사이클 완료'); c.close();
}
run().catch(e=>{console.error(e);process.exit(1);});"
```

### 전체 결과 한번에 확인

```bash
docker run --rm -v /Users/kyungho/Project/PulseOne/data:/data \
  alpine sh -c "apk add sqlite -q && sqlite3 /data/db/pulseone.db '
    SELECT ao.id, ar.name, ao.state, ao.occurrence_time, ao.cleared_time
    FROM alarm_occurrences ao
    JOIN alarm_rules ar ON ao.rule_id=ar.id
    ORDER BY ao.id;
  '"
```

---

## ✅ Phase 3 — Export Gateway 알람 구독 → Payload → Echo 수신

**목표**: Gateway가 Redis `alarms:all` 채널 구독 → payload template 적용 → HTTP echo 서버 수신.

### Step 3-1: Echo 서버 기동

```bash
docker run -d --name win-echo-server \
  --network pulseone_win_test_net \
  node:20-slim \
  node -e "
const http=require('http');
http.createServer((req,res)=>{
  let b='';req.on('data',d=>b+=d);
  req.on('end',()=>{
    console.log('[ECHO]',new Date().toISOString(),b.substring(0,400));
    res.writeHead(200);res.end('OK');
  });
}).listen(9999,()=>console.log('[ECHO] :9999 listening'));"
```

### Step 3-2: Export Target + Mapping DB 등록

```bash
docker run --rm -v /Users/kyungho/Project/PulseOne/data:/data \
  alpine sh -c "apk add sqlite -q && sqlite3 /data/db/pulseone.db \"
    INSERT OR REPLACE INTO export_targets
      (id,tenant_id,profile_id,name,target_type,is_enabled,config,export_mode,batch_size,execution_delay_ms,created_at,updated_at)
    VALUES
      (9999,1,3,'win-echo-server','HTTP',1,
       '[{\\\"url\\\":\\\"http://win-echo-server:9999\\\",\\\"method\\\":\\\"POST\\\",\\\"headers\\\":{\\\"Content-Type\\\":\\\"application/json\\\"}}]',
       'on_change',100,0,datetime('now'),datetime('now'));

    INSERT OR IGNORE INTO export_target_mappings
      (target_id,point_id,site_id,target_field_name,is_enabled,created_at)
    VALUES
      (9999,1,1,'WLS.PV',1,datetime('now')),
      (9999,2,1,'WLS.SRS',1,datetime('now')),
      (9999,3,1,'WLS.SCS',1,datetime('now')),
      (9999,4,1,'WLS.SSS',1,datetime('now')),
      (9999,5,1,'WLS.SBV',1,datetime('now'));
    \"
echo '등록 완료'"
```

### Step 3-3: Gateway 재시작 → SSS 알람 트리거 → Echo 수신 확인

```bash
docker restart pulseone-win-gateway && sleep 5

# WLS.SSS 알람 트리거
docker exec pulseone-win-sim-modbus node -e "
const M=require('modbus-serial');const c=new M();
c.connectTCP('simulator-modbus',{port:50502}).then(async()=>{
  c.setID(1);await c.writeRegister(200,150);
  console.log('SSS=150 트리거');c.close();
});"

sleep 4

# Echo 수신 확인
docker logs win-echo-server | tail -10

# Gateway export 로그
docker logs pulseone-win-gateway --since 30s 2>&1 | grep -iE "export|alarm|200|error" | tail -10

# export_logs DB 확인
docker run --rm -v /Users/kyungho/Project/PulseOne/data:/data \
  alpine sh -c "apk add sqlite -q && sqlite3 /data/db/pulseone.db \
  'SELECT id,status,http_status_code,timestamp FROM export_logs ORDER BY id DESC LIMIT 5;'"
```

---

## 📊 테스트 결과 (2026-02-22 실행)

### Phase 1 결과 ✅

| 포인트 | 값 | Quality | Redis |
|--------|-----|---------|-------|
| WLS.PV (100) | 20 | ✅ GOOD | ✅ |
| WLS.SRS (101) | 0 | ✅ GOOD | ✅ |
| WLS.SCS (102) | 0 | ✅ GOOD | ✅ |
| WLS.SSS (200) | 5 | ✅ GOOD | ✅ |
| WLS.SBV (201) | 100 | ✅ GOOD | ✅ |

### Phase 2 결과

| ID | 알람 룰 | 상태 | 발생시각 | 해제시각 |
|----|--------|------|---------|---------|
| 10 | SSS High Alarm | ✅ cleared | 2026-02-22 10:39:14 | 2026-02-22 10:39:19 |
| 11 | WLS.SBV High Alarm | ✅ cleared | 2026-02-22 10:39:22 | 2026-02-22 10:39:27 |
| 12 | WLS.PV Status Alarm | ✅ cleared | 2026-02-22 10:39:30 | 2026-02-22 10:39:35 |
| 13 | WLS.SRS State Change | ✅ cleared | 2026-02-22 10:39:38 | 2026-02-22 10:39:43 |
| 14 | WLS.SCS State Change | ✅ cleared | 2026-02-22 10:39:45 | 2026-02-22 10:39:51 |

> **5포인트 전부 active → cleared 검증 완료** (2026-02-22 KST)

> **주의**: alarm_rules는 alarm_rules 등록 **후** Collector 기동 순서여야 메모리에 로드됨.

### Phase 2 알람 사이클 재현 방법 (컨테이너 재시작 후)

```bash
# 새 룰 로드를 위해 컨테이너 재시작
docker restart pulseone-win-collector && sleep 15

# 알람 사이클 실행
docker exec pulseone-win-sim-modbus node -e "..." # 위 일괄 스크립트 사용

# 결과 확인
docker run --rm -v /Users/kyungho/Project/PulseOne/data:/data \
  alpine sh -c "apk add sqlite -q && sqlite3 /data/db/pulseone.db '
    SELECT ao.id, ar.name, ao.state, ao.occurrence_time, ao.cleared_time
    FROM alarm_occurrences ao JOIN alarm_rules ar ON ao.rule_id=ar.id ORDER BY ao.id;
  '"
```

### Phase 3 결과

| 항목 | 상태 | 비고 |
|------|------|------|
| Gateway Redis 구독 | ✅ | redis:6379 연결 성공 |
| SSS 알람 Redis 수신 | ✅ | `[ALARM_RECEIVE] Point=4 (WLS.SSS)` |
| Echo 서버 Payload 수신 | ✅ | HTTP 200, 36ms |
| export_logs status=success | ✅ | id=44, target_id=9999, code=200 |

**Echo 서버 수신 payload (2026-02-22 10:55:55 KST)**:
```json
{
  "alarm_flag": 1,
  "alarm_status": "WARNING",
  "building_id": 1,
  "point_name": "WLS.SSS",
  "value": 150.0,
  "timestamp": "2026-02-22 10:55:51.949",
  "upload_timestamp": "2026-02-22 10:55:55.217",
  "source": "PulseOne-CSPGateway",
  "version": "2.0"
}
```

---

## 🔧 트러블슈팅

| 증상 | 원인 | 해결 |
|------|------|------|
| `Quality: BAD` 전체 | `config.slave_id` 누락 | DB `config='{"slave_id":1}'` 수정 |
| Redis `localhost` 연결 | `config/redis.env` 하드코딩 | `REDIS_PRIMARY_HOST=redis` |
| `Slave: -1` | `devices.config` 키가 `unit_id` | Docker로 `slave_id`로 교체 |
| alarm_rules 신규룰 미로드 | 시작 시 1회 로드 후 캐싱 | 컨테이너 재시작 필요 |
| Wine Xvfb lock 오류 | stale lock 파일 | `wine-entrypoint.sh`에서 자동 삭제 |
| SQLite `malformed` | 백업 DB index 깨짐 | `REINDEX; VACUUM;` (Docker 내에서) |

---

## 📌 빠른 DB 조회

```bash
# 현재 장치
docker run --rm -v /Users/kyungho/Project/PulseOne/data:/data alpine sh -c \
  "apk add sqlite -q && sqlite3 /data/db/pulseone.db 'SELECT id,name,endpoint,config FROM devices WHERE is_deleted=0;'"

# 알람 룰
docker run --rm -v /Users/kyungho/Project/PulseOne/data:/data alpine sh -c \
  "apk add sqlite -q && sqlite3 /data/db/pulseone.db 'SELECT id,name,target_id,alarm_type,high_limit,trigger_condition FROM alarm_rules WHERE is_enabled=1;'"

# 최신 알람
docker run --rm -v /Users/kyungho/Project/PulseOne/data:/data alpine sh -c \
  "apk add sqlite -q && sqlite3 /data/db/pulseone.db 'SELECT ao.id,ar.name,ao.state,ao.occurrence_time,ao.cleared_time FROM alarm_occurrences ao JOIN alarm_rules ar ON ao.rule_id=ar.id ORDER BY ao.id DESC LIMIT 10;'"
```
