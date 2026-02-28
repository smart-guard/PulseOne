# 제어 감사 로그 시스템 (Control Audit Log)

> 작성일: 2026-02-28  
> 목적: 모든 포인트 제어 시도·성공·실패의 완전한 기록 및 추적

---

## 1. 개요

### 요구사항

| 항목 | 내용 |
|------|------|
| **누가** | user_id, username (JWT 인증 정보) |
| **어떤 테넌트/사이트** | tenant_id, site_id |
| **어떤 디바이스/포인트** | device_id, point_id, protocol_type, address |
| **무엇을** | old_value → requested_value (기존값 → 새값) |
| **커맨드 전달** | Redis pub/sub 수신자 수, 전달 여부 |
| **실행 결과** | Collector WriteValue() 즉시 결과 |
| **실제 반영** | 다음 폴링 때 current_values 값 비교 |
| **알람 연계** | 제어 후 60초 내 동일 포인트 알람 자동 매칭 |

---

## 2. 핵심 설계: 프로토콜별 확인 단계

> ⚠️ 제어가 실제로 됐는지 아는 시점은 **프로토콜마다 다름**

| 프로토콜 | 즉시 확인 방법 | 결과 타이밍 |
|---------|--------------|------------|
| **Modbus** | FC06/FC05 에코 응답 (TCP 레이어) | **즉시** — WriteValue() 리턴 |
| **BACnet** | WriteProperty 확인 응답 | **즉시** — WriteDataPointValue() 리턴 |
| **OPC-UA** | Write Service StatusCode | **즉시** — WriteDataPoint() 리턴 |
| **HTTP** | HTTP 응답 코드 200/4xx/5xx | **즉시** — WriteValue() 리턴 |
| **MQTT** | Publish-only, 확인 없음 | **다음 폴링** — 구독 토픽 에코 확인 |
| **ROS** | Topic publish-only | **다음 폴링** |
| **BLE** | Write Characteristic (조건부) | **즉시 or 다음 폴링** |

### 3단계 상태 모델

```
[단계 1] 커맨드 전달  (delivery_status)
   pending → delivered      Collector Redis 수신 확인
   pending → no_collector   subscriber_count = 0

[단계 2] 프로토콜 실행  (execution_result)  ← Collector가 WriteValue() 후 즉시 Redis pub
   pending → protocol_success   WriteValue() = true
   pending → protocol_failure   WriteValue() = false
   pending → protocol_async     MQTT/ROS/BLE — 전달만 확인, 결과 불확실
   pending → timeout            10초 내 응답 없음

[단계 3] 값 반영 검증  (verification_result)  ← 다음 폴링 후 Backend 체크
   pending → verified      current_values 값 = requested_value
   pending → unverified    폴링 후 값이 다름 (제어 실제 미반영)
   pending → skipped       동기 프로토콜(Modbus 등) 즉시 성공이면 생략 가능
```

### `final_status` 결정 룰

| 조건 | final_status |
|------|-------------|
| no_collector | **failure** (즉시) |
| protocol_failure | **failure** |
| protocol_success + verified | **success** |
| protocol_success + unverified | **partial** (전송됐지만 값 미반영) |
| protocol_async (MQTT/ROS) | **partial** (전달됐지만 확인 불가) |
| timeout | **timeout** |

---

## 3. DB 스키마

```sql
CREATE TABLE IF NOT EXISTS control_logs (
  id               INTEGER PRIMARY KEY AUTOINCREMENT,
  request_id       TEXT NOT NULL UNIQUE,   -- UUID (end-to-end 추적)

  -- 컨텍스트
  tenant_id        INTEGER,
  site_id          INTEGER,
  user_id          INTEGER,
  username         TEXT,

  -- 대상
  device_id        INTEGER NOT NULL,
  device_name      TEXT,
  protocol_type    TEXT,                   -- MODBUS/MQTT/BACNET/OPCUA/HTTP/BLE/ROS
  point_id         INTEGER NOT NULL,
  point_name       TEXT,
  address          TEXT,                   -- HR:200, CO:100, mqtt/topic 등

  -- 값 변화
  old_value        TEXT,                   -- 제어 전 current_values 값
  requested_value  TEXT NOT NULL,          -- 요청한 새 값

  -- 단계 1: 커맨드 전달
  delivery_status  TEXT DEFAULT 'pending', -- pending/delivered/no_collector
  subscriber_count INTEGER DEFAULT 0,
  delivered_at     DATETIME,

  -- 단계 2: 프로토콜 실행
  execution_result TEXT DEFAULT 'pending', -- pending/protocol_success/protocol_failure/protocol_async/timeout
  execution_error  TEXT,
  executed_at      DATETIME,
  duration_ms      INTEGER,

  -- 단계 3: 값 반영 검증
  verification_result TEXT DEFAULT 'pending', -- pending/verified/unverified/skipped
  verified_value   TEXT,
  verified_at      DATETIME,

  -- 알람 매칭 (제어 후 60초 내 동일 포인트 알람)
  linked_alarm_id  INTEGER,
  alarm_matched_at DATETIME,

  -- UI용 최종 상태
  final_status     TEXT DEFAULT 'pending', -- pending/success/partial/failure/timeout

  requested_at     DATETIME DEFAULT CURRENT_TIMESTAMP,

  FOREIGN KEY (device_id)        REFERENCES devices(id)            ON DELETE SET NULL,
  FOREIGN KEY (point_id)         REFERENCES data_points(id)        ON DELETE SET NULL,
  FOREIGN KEY (user_id)          REFERENCES users(id)              ON DELETE SET NULL,
  FOREIGN KEY (linked_alarm_id)  REFERENCES alarm_occurrences(id)  ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_cl_device  ON control_logs(device_id);
CREATE INDEX IF NOT EXISTS idx_cl_point   ON control_logs(point_id);
CREATE INDEX IF NOT EXISTS idx_cl_user    ON control_logs(user_id);
CREATE INDEX IF NOT EXISTS idx_cl_tenant  ON control_logs(tenant_id);
CREATE INDEX IF NOT EXISTS idx_cl_status  ON control_logs(final_status);
CREATE INDEX IF NOT EXISTS idx_cl_time    ON control_logs(requested_at DESC);
CREATE INDEX IF NOT EXISTS idx_cl_alarm   ON control_logs(linked_alarm_id);
```

---

## 4. 전체 데이터 흐름

```
[Frontend] 값 입력 → 전송 버튼
  │
  ▼ POST /api/devices/:deviceId/data-points/:pointId/write
    Body: { value: "75" }
    Header: Authorization Bearer <JWT>
  │
  ▼ [Backend] routes/data-points.js
    1. JWT → user_id, username 추출
    2. device → tenant_id, site_id 조회
    3. current_values → old_value 조회
    4. access_mode 검증 (read이면 403)
    5. request_id = UUID 생성
    6. control_logs INSERT (result=pending, delivery_status=pending)
    7. Redis PUBLISH cmd:collector:{edge_server_id}
       { command:"write", device_id, point_id, value, request_id }
    8. subscriber_count = 0 → delivery_status=no_collector, final_status=failure
  │
  ▼ [Collector C++] CommandSubscriber.cpp
    수신 즉시:
    - Redis PUBLISH "control:status" { request_id, status:"delivered" }
    
    WriteDataPoint() 실행:
    - Modbus:  libmodbus FC06/FC05 → 에코 응답까지 대기 → 즉시 성공/실패
    - BACnet:  WriteProperty → Confirmed 응답 → 즉시 성공/실패
    - OPCUA:   Write Service → StatusCode → 즉시 성공/실패
    - HTTP:    HTTP PUT/POST → 응답코드 → 즉시 성공/실패
    - MQTT:    MQTT publish → 전달만 → is_async=true
    - ROS/BLE: publish/write → is_async=true
    
    완료 후:
    Redis PUBLISH "control:result" {
      request_id, success, is_async,
      error_message, duration_ms
    }
  │
  ▼ [Backend] Redis Subscriber (control:result)
    - execution_result UPDATE
    - 동기 프로토콜: 10~30초 후 값 검증 Job 등록
    - 비동기(MQTT/ROS): final_status=partial
  │
  ▼ [Background Job] 10~30초 후
    - current_values.current_value == requested_value?
    - verified / unverified 업데이트
  │
  ▼ [Background Job] 60~90초 후
    - alarm_occurrences WHERE point_id=? AND occurrence_time > executed_at  
    - 가장 가까운 알람 → linked_alarm_id 업데이트
```

---

## 5. 구현 파일 목록

### Backend

| 파일 | 작업 | 내용 |
|------|------|------|
| `backend/lib/database/connection.js` | MODIFY | control_logs DDL 자동 생성 |
| `backend/routes/data-points.js` | MODIFY | write API에 로그 INSERT 추가 |
| `backend/routes/control-logs.js` | **NEW** | 조회 API |
| `backend/lib/services/ControlLogService.js` | **NEW** | 로그 CRUD + 검증 Job + 알람 매칭 |
| `backend/lib/startup/SystemInitializer.js` | MODIFY | Redis `control:result` 구독 |

### Collector C++

| 파일 | 작업 | 내용 |
|------|------|------|
| `core/collector/src/Event/CommandSubscriber.cpp` | MODIFY | write 결과 Redis pub, request_id 처리 |

### Frontend

| 파일 | 작업 | 내용 |
|------|------|------|
| `frontend/src/pages/AlarmHistory.tsx` | MODIFY | 제어이력 탭 추가 |
| `frontend/src/api/services/controlLogApi.ts` | **NEW** | 제어이력 조회 API |

---

## 6. 조회 API

```
GET  /api/control-logs
     ?device_id=&point_id=&user_id=&final_status=&protocol_type=
     &from=&to=&page=&limit=

GET  /api/control-logs/:id     (3단계 타이밍 전체 + 알람 상세 포함)
```

---

## 7. UI — 알람이력 페이지 내 제어이력 탭

```
┌──────────────────────────────────────────────────────────┐
│    알람이력    │    제어이력    │                          │
├──────────────────────────────────────────────────────────┤
│ [필터] 디바이스▼  포인트▼  결과▼  기간 [범위]  [검색]   │
├──────┬────────┬──────────┬────────┬──────────┬───────────┤
│ 시간 │ 사용자 │ 디바이스 │ 포인트 │이전→설정값│전달/실행/반영/알람│
├──────┼────────┼──────────┼────────┼──────────┼───────────┤
│10:44 │ admin  │ HMI-001  │WLS.SSS │ 64 → 75  │✅ ✅즉시 ✅ 🔔1│
│10:30 │ kyung  │ MQTT-DEV │WLS.PV  │ 0 → 1   │✅ ⚡전달 🔍 - │
│09:15 │ admin  │ HMI-001  │WLS.SCS │ 1 → 0   │✅ ❌실패 -  - │
└──────┴────────┴──────────┴────────┴──────────┴───────────┘

아이콘 범례:
✅ 성공  ❌ 실패  ⚡ async(MQTT/ROS)  🔍 검증중  🔔 연계알람
```

**알람 연계 동작:** 제어이력 행의 🔔 클릭 → 알람이력 탭으로 전환 + 해당 알람 행 하이라이트

---

## 8. 프로토콜 지원 현황 (WriteDataPoint 구현)

| 프로토콜 | Worker 파일 | 구현 상태 | 즉시 확인 |
|---------|------------|----------|----------|
| Modbus  | ModbusWorker.cpp | ✅ 구현됨 | ✅ FC06 에코 |
| MQTT    | MQTTWorker.cpp   | ✅ 구현됨 | ❌ async |
| BACnet  | BACnetWorker.cpp | ✅ 구현됨 | ✅ Confirmed |
| OPC-UA  | OPCUAWorker.cpp  | ✅ 구현됨 | ✅ StatusCode |
| HTTP    | HttpRestWorker.cpp | ✅ 구현됨 | ✅ HTTP 응답 |
| BLE     | GenericDeviceWorker.cpp | ✅ 공통경로 | ⚠️ 드라이버 의존 |
| ROS     | GenericDeviceWorker.cpp | ✅ 공통경로 | ❌ async |
