# PulseOne E2E 통합 테스트 마스터 플랜 (Master Plan)

본 문서는 PulseOne 시스템의 데이터 수집, 가공, 알람, 내보내기(Export) 전 과정을 검증하기 위한 통합 테스트 지침입니다.

## 🚨 최우선 지침: Docker 환경 내 실행 (Mandatory)
- 모든 테스트 명령어는 **반드시 Docker 컨테이너 내부** 또는 `docker exec`를 통해서만 실행합니다.
- 맥(Mac) 로컬 터미널에서의 직접 실행을 절대 금지합니다.

---

## 📋 통합 테스트 단계 (Phases)

| 단계 | 명칭 | 주요 검증 항목 |
| :--- | :--- | :--- |
| **Phase 1** | **Modbus 데이터 수집** | 장치 연결, Redis 실시간 데이터 유입 확인 |
| **Phase 2** | **Modbus 알람 파이프라인** | 임계값 트리거, DB 기록, Redis 상태 및 UI 배지 동기화 |
| **Phase 3** | **Export Gateway 연동** | 알람 이벤트 수신, S3/HTTP 최종 업로드 및 페이로드 정규화 |
| **Phase 4** | **MQTT 고급 기능** | 자동 탐색(Auto-Discovery), 파일 분할 전송(Chunking), 알람 연동 |
| **Phase 5** | **재연결 및 백오프** | 통신 단절 시 자동 복구, DB 설정값 기반 재시도 및 대기 검증 |

---

## 1. 전제 조건 및 환경 준비
- **데이터베이스**: `/app/data/db/pulseone.db` (Collector 컨테이너 기준)
- **컨테이너 자산**: `docker-collector-1`, `docker-backend-1`, `docker-redis-1`, `docker-simulator-modbus-1`, `docker-export-gateway-1`
- **사전 점검**: `cd docker && docker compose ps`를 통해 모든 서비스 `Up` 상태 확인.

---

## 2. [Phase 1] Modbus 데이터 수집 검증

### 2.1. 장치 등록 및 연결 확인
1.  **UI 작업**: Modbus 장치 등록 (IP: `simulator-modbus`, Port: 50502).
2.  **연결 로그**:
    ```bash
    docker logs docker-collector-1 | grep "Modbus"
    ```
    - `Connected to modbus device` 메시지 확인.

### 2.2. Redis 실시간 데이터 검증
1.  **데이터 유입 조회**:
    ```bash
    # 장치 ID 2번의 특정 포인트(WLS.SSS) 조회
    docker exec docker-redis-1 redis-cli -n 0 GET "device:2:WLS.SSS"
    ```
    - 수치가 주기적으로 변하는지 확인.

---

## 3. [Phase 2] Modbus 알람 발생 및 UI 동기화

### 3.1. 알람 정책 설정
- 웹 UI '알람 설정'에서 `WLS.SSS > 100` 조건으로 정책 생성.

### 3.2. 알람 트리거 및 전파 확인
1.  **시뮬레이터 값 강제 조작**:
    ```bash
    # 임계치 100을 초과하는 150 주입
    docker exec docker-simulator-modbus-1 node /app/backend/scripts/force_modbus.js 200 150 register
    ```
2.  **DB 상태 검사**:
    ```bash
    docker exec docker-collector-1 sqlite3 /app/data/db/pulseone.db "SELECT id, status, point_id FROM alarm_occurrences ORDER BY id DESC LIMIT 1;"
    ```
    - `status='active'` 확인.
3.  **UI 실시간 동기화**: 대시보드 알람 숫자와 사이드바 배지가 즉시 업데이트되는지 확인.

---

## 4. [Phase 3] Export Gateway 연동 검증

### 4.1. 내보내기 타겟 설정
- 알람 이벤트를 S3 또는 HTTP 타겟(예: ID 19)으로 내보내도록 매핑 설정.

### 4.2. 최종 업로드 성공 여부
1.  **로그 모니터링**:
    ```bash
    docker logs -f docker-export-gateway-1
    ```
    - `S3 Upload Success` 또는 `HTTP 200 OK` 기록 확인.
    - 페이로드 내에 포인트 이름, 사이트 ID 등 메타데이터가 정상 포함되었는지 검사.

---

## 5. [Phase 4] MQTT 자동 탐색 및 파일 전송

### 5.1. 자동 탐색 (Auto-Discovery)
1.  **MQTT 시뮬레이터 가동**:
    ```bash
    docker exec docker-backend-1 node scripts/mqtt_simulator.js
    ```
2.  **데이터 동기화 확인**:
    - `data_points` 테이블에 새로운 토픽 기반 포인트 자동 생성 확인.
    - Redis `device:*:current`에 데이터 유입 확인.

### 5.2. 파일 분할 전송 및 재조합 (Advanced)
1.  **청크 시뮬레이터 실행**:
    ```bash
    docker exec docker-backend-1 node scripts/mqtt_chunked_simulator.js
    ```
2.  **검증**:
    - Collector 로그: `File reassembly complete` 확인.
    - Export Gateway: 재조합된 파일이 S3로 최종 업로드되는지 확인.

### 5.3. MQTT 알람 파이프라인 (New)
1.  **알람 정책 설정**:
    - `vfd/auto_point > 100` 조건으로 정책 생성.
2.  **데이터 트리거**:
    ```bash
    docker exec -e MQTT_BROKER_URL=mqtt://rabbitmq:1883 docker-backend-1 node -e "const mqtt = require('mqtt'); const client = mqtt.connect(process.env.MQTT_BROKER_URL); client.on('connect', () => { client.publish('vfd/auto_point', JSON.stringify({value: 200, timestamp: Date.now()}), {qos:1}, () => { client.end(); }); });"
    ```
3.  **검증**:
    - `alarm_occurrences` 테이블에 MQTT 기반 알람 생성 확인.
    - Export Gateway를 통해 최종 목적지로 전송되는지 확인.

---

## 6. [Phase 5] 재연결 및 백오프(Reconnection & Backoff) 검증

### 6.1. 재연결 설정 로드 확인
1.  **로그 확인**:
    ```bash
    docker logs docker-collector-1 | grep "Initial Reconnection Settings"
    ```
    - 각 드라이버별로 DB의 `retry_count`, `retry_interval` 등이 정상 로드되었는지 확인.

### 6.2. 통신 단절 시뮬레이션
1.  **시뮬레이터 중지**: `docker stop docker-simulator-modbus-1`
2.  **로그 확인**:
    - `재연결 프로세스 시작` 메시지 확인.
    - 설정된 간격(`retry_interval`)대로 재시도가 발생하는지 확인.
3.  **백오프(Cool-down) 검증**:
    - 최대 재시도 횟수(`retry_count`) 도달 후 `Cool-down` 상태로 진입하는지 확인.
4.  **복구 확인**:
    - `docker start docker-simulator-modbus-1` 실행 후 자동으로 통신이 복구되고 데이터 수집이 재개되는지 확인.

---

## 7. 최종 체크리스트
- [ ] 모든 작업이 `docker exec`를 통해 수행되었는가?
- [ ] 수집된 데이터가 Redis에 즉시 반영되는가?
- [ ] 알람 발생 후 UI와 사이드바가 즉시 동기화되는가?
- [x] Phase 4: MQTT Advanced Features <!-- id: 28 -->
    - [x] Auto-Discovery verification <!-- id: 30 -->
    - [ ] MQTT Alarm Pipeline verification <!-- id: 31 -->
- [x] Phase 5: Reconnection & Backoff <!-- id: 29 -->
    - [ ] 통신 단절 시 설정된 Reconnection/Backoff 정책에 따라 자동 복구되는가?
