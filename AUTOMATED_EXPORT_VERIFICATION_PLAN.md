# 🚨 핵심 원칙 (Critical Rules) - 반드시 준수

1.  **전 과정 Docker 내부 실행 (Strict Docker-Only)**
    - 맥(Host)에서 직접 실행하는 것은 **단 하나도 없어야 한다**.
    - DB 접속, 컴파일(make), Node.js 테스트 등 모든 작업은 `docker exec`를 통해 컨테이너 내부에서 수행한다.
    - 로컬 환경 오염 방지 및 배포 환경과의 일치성 보장을 위함.

2.  **코드 수정 전 영향도 분석 (Side-Effect Analysis)**
    - 코드를 수정하기 전에, 해당 변경이 다른 기능에 미칠 영향을 코드 베이스 전체에서 먼저 검증한다.
    - "일단 수정하고 보자"는 접근 금지. 연관된 모듈을 먼저 확인하고 안전함을 확신한 후 작업한다.

3.  **지속적인 결과 보고 (Continuous Reporting)**
    - 각 단계별 테스트 결과를 숨기지 않고 즉시 보여준다.
    - 작업 진행 상황을 계속해서 보고한다.

4.  **사용자 질문 최우선 처리 (Priority on User Communication)**
    - 사용자가 질문하면 **모든 작업을 즉시 중단**하고 대답부터 한다.
    - 작업 중이라도 사용자의 호출이 있으면 1순위로 응답한다.

---

# 자동화된 수출 흐름 검증 계획 (Docker 네이티브)

이 계획은 Modbus 시뮬레이터에서 Export Gateway 대상까지의 엔드 투 엔드 자동화 수출 흐름을 검증하기 위한 단계별 절차입니다. 모든 과정은 **Docker 네이티브 환경**에서만 수행됩니다.

## 1. 전제 조건 (Docker 환경)

- [ ] `docker-compose.yml`이 올바른 데이터베이스 경로를 마운트하고 있는지 확인.
- [ ] `docker-collector-1`, `docker-export-gateway-1`, `docker-redis-1`, `docker-simulator-modbus-1` 컨테이너가 실행 중인지 확인.
    - **참고:** 현재 Collector는 데이터베이스에 `Collector ID 1`이 없어서 시작에 실패하고 있습니다.

## 2. 단계별 검증 절차

### 1단계: Collector 신원(Identity) 수정 (Docker 내부)
Collector가 초기화되려면 `edge_servers` 테이블에 `id=1`인 레코드가 있어야 합니다. **반드시 컨테이너 내부에서** 이를 수정합니다.

**작업:**
1.  `docker-collector-1` 컨테이너 접속 (Host 실행 금지):
    ```bash
    docker exec -it docker-collector-1 /bin/bash
    ```
2.  데이터베이스 경로 확인 (컨테이너 내부): `/app/data/db/pulseone.db`
3.  누락된 `Collector ID 1` 레코드 삽입:
    ```bash
    sqlite3 /app/data/db/pulseone.db "INSERT OR IGNORE INTO edge_servers (id, tenant_id, server_name, factory_name, location, ip_address, port, registration_token, status, last_seen, version, created_at, updated_at) VALUES (1, 1, 'Collector-1', 'Factory-1', 'Seoul', '127.0.0.1', 5000, 'token', 'active', CURRENT_TIMESTAMP, '1.0.0', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP);"
    ```
4.  Collector 컨테이너 재시작:
    ```bash
    docker restart docker-collector-1
    ```

### 2단계: 시뮬레이터 연결 확인
Collector가 시작되면 Modbus 시뮬레이터에 연결되어야 합니다.

**검증:**
1.  Collector 로그 확인:
    ```bash
    docker logs -f docker-collector-1
    ```
    - **성공 기준:** `[ModbusWorker] Connected to 127.0.0.1:502` 또는 유사한 성공 메시지 확인.
    - **예상 문제:** 컨테이너 내부에서 `localhost`로 연결을 시도하면 실패할 수 있음.
    - **해결:** `config/devices.json`이 올바른 호스트명(`simulator-modbus` 또는 `host.docker.internal` (네트워크 모드에 따라))을 가리키는지 확인.

### 3단계: 데이터 수집 확인 (Redis)
Collector는 수집된 원시 값을 Redis에 발행합니다.

**검증:**
1.  `docker-redis-1` 컨테이너 접속:
    ```bash
    docker exec -it docker-redis-1 redis-cli
    ```
2.  값 업데이트 확인:
    ```redis
    # 패턴: T:V:{building_id}:{point_id}
    PSUBSCRIBE T:V:*:*
    ```
    - **성공 기준:** 실시간으로 값이 업데이트되는 메시지가 보여야 함.

### 4단계: 알람 생성 확인
알람 엔진은 값을 처리하고 알람을 생성합니다.

**검증:**
1.  `redis-cli` 내부에서 알람 이벤트 확인:
    ```redis
    # 채널: alarms:all (또는 특정 테넌트 채널)
    SUBSCRIBE alarms:all
    ```
    - **성공 기준:** 알람 상태 변경(Active/Clear)을 설명하는 JSON 메시지 수신.

### 5단계: Export Gateway 구독 확인
Export Gateway는 `alarms:all`을 구독하고 수출을 트리거합니다.

**검증:**
1.  Export Gateway 로그 확인:
    ```bash
    docker logs -f docker-export-gateway-1
    ```
    - **성공 기준:** 로그에 "Received alarm message" 및 "Processing export target..." 확인.

### 6단계: 최종 수출 확인 (S3/HTTP)
**S3 대상:**
- `minio` 버킷(사용 가능한 경우) 또는 매핑된 로컬 `data/export` 디렉토리 확인.
- **성공 기준:** 대상 폴더에 JSON/CSV 파일 생성 확인.

**HTTP 대상:**
- 대상 서버 로그 확인 (Mock API 또는 실제 엔드포인트).
- **성공 기준:** `200 OK` 응답 확인.

## 3. 롤백 및 복구 계획
- DB 삽입 실패 또는 손상 시, `data/db/pulseone.db.bak`에서 복구하거나 `npm run seed`로 재설정.
