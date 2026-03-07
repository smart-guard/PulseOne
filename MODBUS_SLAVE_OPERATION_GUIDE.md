# Modbus Slave (Publisher) — 전체 동작 시나리오 가이드

> 작성일: 2026-03-07  
> 대상: 현장 엔지니어 / 시스템 개발자  
> 버전: v1.0 (검증 완료)

---

## 목차

1. [시스템 개요](#1-시스템-개요)
2. [DB 데이터 구조](#2-db-데이터-구조)
3. [엔지니어 설정 시나리오](#3-엔지니어-설정-시나리오)
4. [C++ 서비스 초기화 흐름](#4-c-서비스-초기화-흐름)
5. [Redis 데이터 흐름](#5-redis-데이터-흐름)
6. [클라이언트 접속 시나리오](#6-클라이언트-접속-시나리오)
7. [복수 디바이스 — 멀티 프로세스 시나리오](#7-복수-디바이스--멀티-프로세스-시나리오)
8. [백엔드 역할](#8-백엔드-역할)
9. [프런트엔드 화면별 기능과 부족한 부분](#9-프런트엔드-화면별-기능과-부족한-부분)
10. [장애 복구 시나리오](#10-장애-복구-시나리오)

---

## 1. 시스템 개요

```
[Field Device]  ──Modbus TCP─→  [pulseone-collector]
                                        │ Redis PUBLISH "point:update"
                                        ▼
                                  [Redis :6379]
                                        │ SUBSCRIBE
                              ┌─────────▼─────────┐
                              │ pulseone-modbus-   │  ← 이 서비스
                              │    slave           │
                              └─────────┬──────────┘
                               TCP :502 │
                              ┌─────────▼─────────┐
                              │  SCADA / DCS / HMI │
                              │  (외부 클라이언트)  │
                              └────────────────────┘
```

**핵심**: Collector가 읽어온 값을 실시간으로 Redis에 올리면, Modbus Slave 서비스가 이를 받아 레지스터 메모리에 넣고, 외부 SCADA가 Modbus TCP(FC03/FC01/FC04 등)로 읽어간다.

---

## 2. DB 데이터 구조

### 2.1 `modbus_slave_devices` — 디바이스 설정

| 컬럼           | 타입    | 설명                                      |
| -------------- | ------- | ----------------------------------------- |
| `id`           | INT PK  | 디바이스 식별자                           |
| `tenant_id`    | INT FK  | 테넌트 격리                               |
| `site_id`      | INT FK  | 어느 현장에 속하는지                      |
| `name`         | TEXT    | 엔지니어가 붙이는 이름 (예: "SCADA #1")   |
| `tcp_port`     | INT     | 이 디바이스가 Listen할 TCP 포트 (예: 502) |
| `unit_id`      | INT     | Modbus Unit ID (1~247)                    |
| `max_clients`  | INT     | 동시 접속 허용 클라이언트 수 (기본 10)    |
| `enabled`      | INT 0/1 | Supervisor 자동 시작 여부                 |
| `packet_logging` | INT 0/1 | 패킷 로그 파일 기록 활성화              |

> **UNIQUE(site_id, tcp_port)**: 동일 사이트 내 포트 중복 등록 방지

### 2.2 `modbus_slave_mappings` — 레지스터 매핑

| 컬럼               | 타입    | 설명                                                    |
| ------------------ | ------- | ------------------------------------------------------- |
| `device_id`        | INT FK  | 어느 디바이스에 속하는 매핑인지                         |
| `point_id`         | INT FK  | Collector가 수집하는 DataPoint (data_points.id)          |
| `register_type`    | TEXT    | `HR`(Holding)/`IR`(Input)/`CO`(Coil)/`DI`(Discrete)   |
| `register_address` | INT     | 0-base Modbus 주소 (SCADA에서 읽을 때는 +1 표시)        |
| `data_type`        | TEXT    | `FLOAT32`,`INT32`,`INT16`,`UINT16`,`FLOAT64`,`BOOLEAN` |
| `byte_order`       | TEXT    | `big_endian` / `little_endian`                          |
| `scale_factor`     | REAL    | 레지스터값 = 원본값 × scale_factor + scale_offset       |
| `scale_offset`     | REAL    | 스케일 오프셋                                           |
| `enabled`          | INT 0/1 | 이 매핑만 비활성화 가능                                 |

> **UNIQUE(device_id, register_type, register_address)**: 주소 충돌 방지

### 2.3 `modbus_slave_access_logs` — 통신 이력

| 컬럼             | 설명                                              |
| ---------------- | ------------------------------------------------- |
| `client_ip`      | 접속한 SCADA/HMI의 IP                             |
| `period_start/end` | 이 레코드가 집계하는 시간 구간 (30초 스냅샷)   |
| `total_requests` | 집계 기간 내 총 요청 수                           |
| `fc03_count`     | FC03(Read Holding Registers) 요청 수 (가장 많음)  |
| `avg_response_us` | 평균 응답 시간 (마이크로초)                      |
| `success_rate`   | 0.0~1.0 성공률                                    |
| `is_active`      | 현재 접속 중(1) / 해제됨(0)                       |

---

## 3. 엔지니어 설정 시나리오

### Step 1 — Modbus Slave 디바이스 생성

1. PulseOne 웹 UI → **Modbus Slave** 메뉴
2. **+ Add Device** 클릭
3. 다음 정보 입력:

```
이름:        "서울공장 SCADA 연동"
테넌트:      Seoul Factory
사이트:      Seoul Main Factory
TCP Port:   502          ← SCADA가 연결할 포트
Unit ID:    1            ← 0x01
최대 클라이언트: 5
패킷 로그:  ✔ (활성화 권장 — 디버깅 시)
```

4. **저장** → DB(`modbus_slave_devices`) INSERT  
5. Supervisor가 60초 이내 자동 감지 → Worker 프로세스 자동 시작

### Step 2 — 레지스터 매핑 설정

1. 해당 디바이스 카드의 **Register Mapping** 탭 선택
2. 디바이스 선택 후 **+ Add Point** 클릭
3. 아래 예시처럼 매핑:

| point_id | 포인트명         | 레지스터 타입 | 주소 | 데이터 타입 |
| -------- | ---------------- | ------------- | ---- | ----------- |
| 42       | 온도_1번_센서    | HR            | 100  | FLOAT32     |
| 43       | 압력_1번_센서    | HR            | 102  | FLOAT32     |
| 77       | 밸브_개폐        | CO            | 0    | BOOLEAN     |
| 88       | 유량_m3/h       | HR            | 104  | FLOAT32     |
| 99       | 냉각수_온도      | IR            | 200  | FLOAT32     |

> **주의**: FLOAT32는 2 word 소비 → 주소 100 다음은 102부터 시작.  
> IR은 SCADA에서 FC04로 읽음 (Read-Only).

4. **저장** → DB(`modbus_slave_mappings`) INSERT  
5. C++ Worker가 5분 주기로 자동 재로드 (또는 재시작으로 즉시 적용)

### Step 3 — SCADA 연결 확인

SCADA 소프트웨어에서:
```
PLC IP:   <PulseOne 서버 IP>
Port:     502
Unit ID:  1

Read Holding Registers:
  FC03, start=100, count=2  → FLOAT32 온도 값 (Big-Endian)
  FC03, start=102, count=2  → FLOAT32 압력 값
  FC01, start=0,   count=1  → Coil 밸브 상태
  FC04, start=200, count=2  → Input Register 냉각수 온도
```

---

## 4. C++ 서비스 초기화 흐름

```
pulseone-modbus-slave (Supervisor 모드)
  │
  ├─ QueryActiveDevices()  ← modbus_slave_devices WHERE enabled=1 조회
  │   └─ [id=1, port=502, unit_id=1]
  │   └─ [id=2, port=503, unit_id=2]
  │
  ├─ SpawnChild(device_id=1)  fork/exec --device-id=1
  │     └─  Worker 프로세스 시작
  │         ├─ LoadDeviceFromDb(1)       ← tcp_port, unit_id, max_clients 로드
  │         ├─ DbMappingLoader.Load()   ← modbus_slave_mappings 19개 로드
  │         ├─ RegisterTable 초기화     ← 4×64K word 메모리 (HR/IR/CO/DI)
  │         ├─ RedisSubscriber.Start()  ← SUBSCRIBE point:update
  │         │   └─ point_map 구성 {point_id → RegisterMapping*}
  │         └─ ModbusSlaveServer.Start(port=502, unit_id=1, max_clients=5)
  │             └─ TCP Listen :502
  │
  └─ SpawnChild(device_id=2)  fork/exec --device-id=2
        └─  Worker 프로세스 시작 (동일 흐름, port=503)

Worker 메인 루프 (200ms 틱):
  ├─ [30초 마다] stats.PublishToRedis("modbus:stats:{id}")
  ├─ [30초 마다] client_mgr.PublishToRedis("modbus:clients:{id}")
  └─ [5분 마다]  DbMappingLoader.Reload() → 매핑 변경 시 redis.Restart()

Supervisor 모니터링 루프 (5초 틱):
  ├─ MonitorChildren()       ← waitpid(WNOHANG)으로 크래시 감지 → 자동 재시작
  └─ [60초 마다] QueryActiveDevices() → DB 추가/삭제 감지 → Reconcile()
```

---

## 5. Redis 데이터 흐름

### 5.1 포인트 값 수신 (`point:update` 채널)

```
Collector publishes:
  PUBLISH point:update '{"point_id":42,"value":23.5,"quality":"GOOD","ts":1741234567}'

Worker receives:
  OnMessage(payload) →
    point_id = 42
    value    = 23.5
    it = point_map_[42]  → RegisterMapping{addr=100, type=HR, dtype=FLOAT32}
    WriteValueToTable(m, 23.5):
      raw = 23.5 * scale_factor(1.0) + scale_offset(0.0) = 23.5
      table.SetFloat32(100, 23.5f, big_endian=true)
        → HR[100] = 0x41BC  HR[101] = 0x0000
```

### 5.2 통계/클라이언트 세션 게시 (30초 주기)

```
Redis SETEX modbus:stats:1  120  {
  "total_requests": 1240,
  "total_failures": 2,
  "overall_success_rate": 0.9984,
  "avg_response_us": 312.5,
  "max_response_us": 1820,
  "fc_counters": {"fc03": 1200, "fc01": 40},
  "window_5min":  {"requests": 150, "req_per_min": 30.0, "success_rate": 1.0},
  "window_60min": {"requests": 1240, "req_per_min": 20.7}
}

Redis SETEX modbus:clients:1  120  {
  "total_connections": 3,
  "current_clients": 2,
  "sessions": [
    {"ip":"192.168.1.100","port":49152,"connected_at":"2026-03-07T09:30:00",
     "requests":800,"success_rate":1.0,"fc03_count":800},
    {"ip":"192.168.1.101","port":50020,"connected_at":"2026-03-07T10:00:00",
     "requests":400,"success_rate":0.995}
  ]
}
```

---

## 6. 클라이언트 접속 시나리오

### 6.1 단일 클라이언트 접속 (일반 SCADA)

```
시퀀스:
  SCADA(192.168.1.100) → TCP Connect :502
    → ModbusSlaveServer.ServerLoop():
        newfd = modbus_tcp_accept()
        ip = GetClientIp(newfd)  → "192.168.1.100"
        client_mgr.OnConnect(newfd, "192.168.1.100", port)
          → sessions_[fd] = ClientSession{ip, connected_at}
          → current_clients_++ (현재 1)

  SCADA sends FC03 (Read 2 Holding Registers from addr 100):
    → modbus_receive() → fc=0x03
    → RegisterTable 복사(memcpy) → mb_map
    → modbus_reply() → HR[100~101] (FLOAT32 온도값)
    → stats.Record(0x03, true, 285us)
    → client_mgr.OnRequest(fd, 0x03, true, 285us)

  30초 후:
    → stats.PublishToRedis("modbus:stats:1")
    → client_mgr.PublishToRedis("modbus:clients:1")

  백엔드 /api/modbus-slave/1/stats 호출:
    → Redis GET modbus:stats:1 + modbus:clients:1
    → snapshotAccessLogs() → access_logs DB INSERT
    → 프런트 ClientMonitor 탭에 실시간 표시
```

### 6.2 복수 클라이언트 접속

```
동시 접속 상태 (max_clients=5):
  SCADA-A (192.168.1.100):  800 req, FC03
  SCADA-B (192.168.1.101):  400 req, FC03 + FC06(쓰기 포함)
  HMI-C   (192.168.10.50):  20  req, FC01(코일 읽기)

ClientManager 관리:
  sessions_ = {
    fd=5: {ip="192.168.1.100", total_requests=800, ...},
    fd=6: {ip="192.168.1.101", total_requests=400, fc06_count=12, ...},
    fd=7: {ip="192.168.10.50",  total_requests=20,  fc01_count=20, ...}
  }

SCADA-B가 FC06으로 쓰기 요청:
  → WriteableSingleRegister(addr=200, value=0x1234)
  → RegisterTable.OnExternalRegisterWrite(200, 0x1234)
    → SetHoldingRegister(200, 0x1234)
    → reg_write_cb_(200, 0x1234)  ← 콜백 (역방향 제어 연동 가능)

IP 동일 클라이언트 추가 접속 (과다 접속 시 차단):
  → max_connections_per_ip = 3 (기본)
  → count_from_ip >= 3 → OnConnect() 반환 false → 연결 거부
```

---

## 7. 복수 디바이스 — 멀티 프로세스 시나리오

### 7.1 구성 예시

```
modbus_slave_devices:
  id=1: name="서울공장 SCADA",  port=502, unit_id=1
  id=2: name="부산공장 SCADA",  port=503, unit_id=1
  id=3: name="대전공장 HMI",    port=504, unit_id=1
```

### 7.2 프로세스 트리

```
PID  1: pulseone-modbus-slave  (Supervisor)
PID 13: /app/.../pulseone-modbus-slave --device-id=2  (Worker: port 503)
PID 14: /app/.../pulseone-modbus-slave --device-id=3  (Worker: port 504)
PID  9: /app/.../pulseone-modbus-slave --device-id=1  (Worker: port 502)
```

### 7.3 Redis 키 격리

```
modbus:stats:1    ← device_id=1 통계 (TTL 120초)
modbus:stats:2    ← device_id=2 통계
modbus:stats:3    ← device_id=3 통계
modbus:clients:1  ← device_id=1 클라이언트 세션
modbus:clients:2  ← device_id=2 클라이언트 세션
modbus:clients:3  ← device_id=3 클라이언트 세션
```

### 7.4 신규 디바이스 추가 시 자동 spawn

```
1. 엔지니어가 UI에서 device id=4 (port=505) 등록 → DB INSERT enabled=1
2. Supervisor: 60초 후 QueryActiveDevices() → [1,2,3,4]
3. Reconcile([1,2,3,4]):
   children_에 4 없음 → SpawnChild(4)
   → fork() → execl("--device-id=4")
4. Worker-4 기동: DB에서 설정+매핑 로드 → TCP :505 Listen
```

### 7.5 디바이스 비활성화 시 자동 종료

```
1. 엔지니어가 UI에서 device id=2 '활성화' 토글 OFF → DB UPDATE enabled=0
2. Supervisor: 60초 후 QueryActiveDevices() → [1,3,4]
3. Reconcile([1,3,4]):
   children_에 2 있고 active_ids에 없음 → KillChild(2)
   → SIGTERM → waitpid → SIGKILL
```

---

## 8. 백엔드 역할

### 8.1 API 목록

| Endpoint                           | 방법   | 설명                                   |
| ---------------------------------- | ------ | -------------------------------------- |
| `GET    /api/modbus-slave`         | 목록   | 디바이스 목록 + 프로세스 상태          |
| `POST   /api/modbus-slave`         | 생성   | 새 Slave 디바이스 등록                 |
| `PUT    /api/modbus-slave/:id`     | 수정   | 포트/Unit ID/최대클라이언트 수정       |
| `DELETE /api/modbus-slave/:id`     | 삭제   | 디바이스 + 매핑 + 이력 CASCADE 삭제   |
| `GET    /api/modbus-slave/:id/mappings` | 조회 | 레지스터 매핑 목록                |
| `PUT    /api/modbus-slave/:id/mappings` | 저장 | 매핑 전체 교체 (UPSERT)           |
| `POST   /api/modbus-slave/:id/start`   | 제어 | Worker 수동 시작                  |
| `POST   /api/modbus-slave/:id/stop`    | 제어 | Worker 수동 중지                  |
| `POST   /api/modbus-slave/:id/restart` | 제어 | Worker 수동 재시작                |
| `GET    /api/modbus-slave/:id/stats`   | 통계 | Redis→실시간 통계 + DB 스냅샷    |
| `GET    /api/modbus-slave/access-logs` | 이력 | 통신 이력 페이징 조회 + IP 필터 |
| `GET    /api/modbus-slave/access-logs/stats` | 집계 | 기간별 통계 집계           |
| `GET    /api/modbus-slave/packet-logs` | 파일 | 패킷 로그 파일 목록              |
| `GET    /api/modbus-slave/packet-logs/read` | 파일 | 패킷 로그 파일 tail 읽기    |
| `GET    /api/modbus-slave/reserved-ports` | 유틸 | 시스템 예약 포트 목록 (Docker/Native 환경변수 기반) |
| `GET    /api/modbus-slave/packet-logs/stream` | 스트림 | tail -f SSE 스트리밍 |
| `POST   /api/modbus-slave/block-ip`    | 제어 | 클라이언트 IP 차단 |
| `GET    /api/modbus-slave/blocked-ips` | 제어 | 차단된 IP 목록 조회 |
| `GET    /api/modbus-slave/:id/register-values` | 실시간 | 매핑별 현재 값 조회 (current_values JOIN + 스케일 적용) |

### 8.2 프로세스 상태 감지 (Docker 환경)

Docker multi-container 환경에서는 cross-container ps 불가 → **TCP 포트 접속 시도**로 Running 판별:
```js
await this._checkTcpPort('modbus-slave', dev.tcp_port, 1500ms)
  → true  → process_status = 'running'
  → false → process_status = 'stopped'
```

### 8.3 통계 스냅샷 흐름

```
프런트가 3초마다 GET /api/modbus-slave/:id/stats 폴링
  → 백엔드: Redis GET modbus:stats:{id} (Worker가 30초마다 갱신)
  → 백엔드: Redis GET modbus:clients:{id} (Worker가 30초마다 갱신)
  → 비동기: snapshotAccessLogs() → access_logs DB INSERT
  → 응답: {stats+clients JSON}
```

---

## 9. 프런트엔드 화면별 기능과 부족한 부분

### 9.1 Device Management 탭

**구현 완료**:
- 디바이스 카드 목록 (TCP포트/Unit ID/MAX클라이언트/매핑포인트 수)
- RUNNING/STOPPED 뱃지
- 추가/수정/삭제 모달
- 수동 시작/중지/재시작 버튼
- 현재 접속 클라이언트 수를 카드에 표시 (`current_clients`) ✅
- **포트 충돌 사전 검증** (reserved-ports API + 자동 포트 할당) ✅
- **마지막 요청 시각** (access_logs MAX(recorded_at)로 카드에 표시) ✅

### 9.2 Register Mapping 탭

**구현 완료**:
- 디바이스 선택 → 매핑 테이블 (포인트명/레지스터 타입/주소/데이터타입/스케일/활성화)
- 행 추가/수정/삭제 인라인 편집
- DB 포인트 선택 팝업
- 주소 겹침 시각적 경고 (`FLOAT32`는 2word 소비하므로 addr+1 강조) ✅
- SCADA 관점 주소 표시 (0-base → 1-base 자동 표시, 예: HR100 → 4x0101) ✅
- **JSON/CSV 매핑 Export + JSON Import 버튼** ✅
- **매핑 테이블에 "현재 값" 컬럼** — current_values JOIN, quality 바니 색상 ✅

### 9.3 Client Monitoring 탭

**현재 구현**:
- Worker 상태 (Running/Stopped)
- 요약 카드 4개 (총요청/고유클라이언트/성공률/평균응답시간)
- 접속 클라이언트 테이블 (IP/접속시각/요청수/성공률/응답시간)
- 5분/60분 윈도우 통계
- FC 분포 테이블

**추가하면 좋을 것**:
- 클라이언트별 FC 분포 미니 바 차트
- 응답시간 트렌드 그래프 (시계열)
- 접속 클라이언트 강제 차단(Block IP) 버튼
- 차단된 IP 목록 표시

### 9.4 Communication Log 탭

**구현 완료**:
- 디바이스/기간 필터
- 통계 뱃지 (총요청/고유클라이언트/성공률/평균응답)
- 이력 테이블 (client_ip, 시각, 요청수, 성공률, 응답시간, FC분포)
- 자동 갱신 토글 (15초)
- **성공률 컬럼 색상** (97% 이상: 초록, 90%: 주황, 미만: 빨강) ✅
- **클라이언트 IP 클릭 → 해당 IP 이력 자동 필터링** ✅

### 9.5 Packet Log 탭

**구현 완료**:
- 파일 목록 (날짜별)
- 파일 선택 → 오른쪽 터미널 뷰에 로그 내용 표시
- 파일 목록 갱신 버튼
- 실시간 tail -f 모드 (1초 폴링 스트리밍) ✅
- **검색어 필터** (FC번호, 주소, 클라이언트 IP 실시간 필터링) ✅
- **파일 다운로드 버튼** ✅

---

## 10. 장애 복구 시나리오

### 10.1 Worker 크래시 자동 복구

```
Worker-1 크래시 발생
  → Supervisor.MonitorChildren():
      waitpid(PID, WNOHANG) ≠ 0  → 크래시 감지
      restart_count++ (1회)
      fork() → execl("--device-id=1") → 새 Worker PID 생성
      로그: "[ModbusSupervisor] 디바이스 1 크래시 감지 → 재시작 (1회)"

재시작 한도(MAX_RESTART_COUNT=10) 초과 시:
  → children_ 맵에서 제거
  → 60초 후 Reconcile()에서 DB 재조회
  → DB에 enabled=1 이면 다시 SpawnChild()
  ⇒ 영구 포기 없음 — 항상 복구 시도
```

### 10.2 Redis 장애 시 동작

```
Redis 연결 끊김:
  → RedisSubscriber.SubscribeLoop():
      redisGetReply() → REDIS_ERR (연결 끊김)
      break → reconnect 루프
      5초 대기 후 재연결 시도

재연결 성공 전까지:
  → 레지스터 값 갱신 없음 (SCADA는 마지막 값을 계속 읽음)
  → ModbusSlaveServer는 정상 응답 (마지막 레지스터 값으로)

통계 게시 실패:
  → PublishToRedis() 내부 에러 → 조용히 무시 (no-op)
  → redis 복구 후 자동 재개
```

### 10.3 DB 매핑 변경 즉시 적용 방법

```
방법 A (자동): 5분 대기
  DbMappingLoader.Reload() 주기 = 5분
  → 해시 변경 감지 시 RedisSubscriber 재시작

방법 B (즉시): 프런트 재시작 버튼
  POST /api/modbus-slave/:id/restart
  → Worker 프로세스 중지 → 즉시 재시작
  → DB에서 최신 매핑 로드
  → TCP 재접속까지 약 1~2초 다운타임
```

---

## 부록 A — Modbus 주소 빠른 참조

| 레지스터 타입   | FC Read | FC Write | Modbus 주소 범위 | 용도                        |
| --------------- | ------- | -------- | ---------------- | --------------------------- |
| Coil (CO)       | FC01    | FC05/15  | 0x0000 ~ 0xFFFF  | 디지털 출력 (On/Off)        |
| Discrete Input  | FC02    | —        | 0x0000 ~ 0xFFFF  | 디지털 입력 (Read-Only)     |
| Holding Register| FC03    | FC06/16  | 0x0000 ~ 0xFFFF  | 아날로그 설정/데이터        |
| Input Register  | FC04    | —        | 0x0000 ~ 0xFFFF  | 아날로그 계측 (Read-Only)   |

## 부록 B — 데이터 타입별 레지스터 소비량

| 데이터 타입 | word 수 | 예시 주소 (주소 0 기준) |
| ----------- | ------- | ----------------------- |
| BOOLEAN     | 1 bit   | CO[0]                   |
| INT16/UINT16| 1       | HR[100]                 |
| INT32/UINT32| 2       | HR[100], HR[101]        |
| FLOAT32     | 2       | HR[100]~[101]           |
| FLOAT64     | 4       | HR[100]~[103]           |

> FLOAT32 예: HR[100]=0x41BC, HR[101]=0x0000 → IEEE754 = 23.5

## 부록 C — pymodbus 검증 스크립트

```python
#!/usr/bin/env python3
"""PulseOne Modbus Slave 빠른 검증 스크립트"""
from pymodbus.client import ModbusTcpClient
import struct, sys

HOST = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 502

c = ModbusTcpClient(HOST, port=PORT)
c.connect()

# HR 100~101: FLOAT32 (Big-Endian)
r = c.read_holding_registers(100, 2, unit=1)
if not r.isError():
    val = struct.unpack('>f', struct.pack('>HH', r.registers[0], r.registers[1]))[0]
    print(f"HR[100] FLOAT32 = {val:.4f}")

# CO 0: Coil 상태
r2 = c.read_coils(0, 1, unit=1)
if not r2.isError():
    print(f"CO[0] = {'ON' if r2.bits[0] else 'OFF'}")

# IR 200~201: Input Register FLOAT32
r3 = c.read_input_registers(200, 2, unit=1)
if not r3.isError():
    val3 = struct.unpack('>f', struct.pack('>HH', r3.registers[0], r3.registers[1]))[0]
    print(f"IR[200] FLOAT32 = {val3:.4f}")

c.close()
print("검증 완료")
```

```bash
# 실행
pip install pymodbus
python3 verify_modbus.py 192.168.1.10 502
```

## 부록 D — mbpoll CLI 검증

```bash
# HR 100번부터 10개 읽기 (FLOAT32)
mbpoll -a 1 -r 100 -c 10 -t 4:float 192.168.1.10

# Coil 0번부터 8개 읽기
mbpoll -a 1 -r 0  -c 8  -t 0 192.168.1.10

# Input Register 200번부터 4개 읽기
mbpoll -a 1 -r 200 -c 4 -t 3:float 192.168.1.10
```
