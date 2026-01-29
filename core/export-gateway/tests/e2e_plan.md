# Export Gateway E2E 테스트 계획서 (상세 가이드)

본 문서는 Docker 환경에서 PulseOne의 엔드-투-엔드(E2E) 데이터 엑스포트 파이프라인을 검증하기 위한 상세 절차와 기술적 근거를 제공합니다.

## 1. 환경 및 아키텍처 개요

테스트를 진행하기 전, 관여하는 컴포넌트들과 이들의 통신 경로를 이해하는 것이 중요합니다.

*   **Modbus 시뮬레이터 (`docker-simulator-modbus-1`)**: 물리적인 PLC 하드웨어 역할을 합니다. 수집기가 읽어갈 레지스터 데이터를 유지합니다.
*   **수집기 (Collector, `docker-collector-1`)**: 데이터 수집 엔진입니다. Modbus TCP를 통해 시뮬레이터를 폴링하고, 데이터를 알람 룰에 따라 평가한 뒤 Redis와 SQLite에 저장합니다.
*   **Redis (`docker-redis-1`)**: 고속 이벤트 버스입니다. 수집기(발행자)와 Export Gateway(구독자) 사이의 실시간 통신을 가능하게 합니다.
*   **Export Gateway (`pulseone-export-gateway`)**: 데이터 전송 엔진입니다. Redis의 이벤트를 리스닝하고, 페이로드 변환(템플릿)을 적용한 뒤 외부 타겟(HTTP/S3)으로 데이터를 전송합니다.

| 리소스 | 값 | 기술적 근거 (Rationale) |
| :--- | :--- | :--- |
| **시뮬레이터 IP** | `172.18.0.10` | 컨테이너 간 통신을 위한 내부 브릿지 IP입니다. |
| **게이트웨이 ID** | `6` | 격리(Isolation) 및 할당 로직을 테스트하기 위해 지정된 인스턴스 ID입니다. |
| **데이터베이스** | `./data/db/pulseone.db` | 모든 컨테이너가 동일한 설정을 공유할 수 있도록 마운트된 공유 볼륨입니다. |

---

## 2. 1단계: Atomic Reset (초기화 프로토콜)

검증을 위해서는 명확한 **상태 변화**(예: 정상 -> 알람)를 관찰해야 합니다. 시스템이 이미 알람 상태라면, 엣지 감지(Edge-detection) 로직이 트리거되지 않을 수 있습니다.

```bash
# 1. 초기화 중 데이터 충돌을 방지하기 위해 서비스 중지
docker stop docker-collector-1 pulseone-export-gateway

# 2. 휘발성 데이터 및 최근 이력 삭제
# - alarm_occurrences: 이전 사고 기록 삭제
# - current_values: 수집기의 메모리를 '정상(Normal)' 상태로 리셋
# - export_logs: 이번 테스트의 로그만 명확히 보이도록 이력 삭제
docker exec docker-backend-1 sqlite3 /app/data/db/pulseone.db \
"DELETE FROM alarm_occurrences; \
 UPDATE current_values SET current_value = '{\"value\":0.0}', alarm_state = 'normal', alarm_active = 0; \
 DELETE FROM export_logs;"
```

---

## 3. 2단계: 설정 (테스트 시나리오 구축)

데이터베이스에 직접 테스트 시나리오를 주입합니다. 이는 UI 버그의 영향을 배제하고 정밀한 테스트를 보장합니다.

### 3.1 리소스 등록 (SQL)
```sql
-- 디바이스(PLC) 등록
-- 수집기가 어디를 폴링해야 할지 지정합니다. edge_server_id=1은 docker-collector-1에 매핑됩니다.
INSERT OR REPLACE INTO devices (id, tenant_id, site_id, name, device_type, protocol_id, endpoint, edge_server_id, is_enabled, config)
VALUES (200, 1, 1, 'E2E-PLC', 'PLC', 1, '172.18.0.10:50502', 1, 1, '{"slave_id": 1}');

-- 데이터 포인트(센서) 등록
-- 주소 100은 시뮬레이터의 Coil 100번에 해당합니다.
INSERT OR REPLACE INTO data_points (id, device_id, name, address, data_type, is_enabled, alarm_enabled)
VALUES (200, 200, 'E2E.ALARM', 100, 'BOOL', 1, 1);

-- 알람 룰(트리거) 등록
-- digital/on_true: 포인트 값이 1.0(True)이 되는 즉시 발생합니다.
INSERT OR REPLACE INTO alarm_rules (id, tenant_id, name, point_id, rule_type, condition, severity, is_enabled)
VALUES (200, 1, 'E2E_ALARM_RULE', 200, 'digital', 'on_true', 'critical', 1);

-- Export 타겟 매핑
-- 포인트를 기존 HTTP(18) 및 S3(19) 타겟과 연결합니다.
-- target_field_name: 전송될 JSON 페이로드에서 사용될 키 이름입니다.
INSERT OR REPLACE INTO export_target_mappings (target_id, point_id, site_id, target_field_name, is_enabled)
VALUES 
(18, 200, 1, 'ENVS_TEST.E2E_POINT', 1),
(19, 200, 1, 'ENVS_TEST.E2E_POINT', 1);
```

---

## 4. 3단계: 실행 및 모니터링 (감사 추적)

"데이터 포인트의 생애 주기"를 추적하기 위해 로그를 순차적으로 모니터링합니다.

### 1단계: 서비스 시작 및 안정화
```bash
docker start docker-collector-1 pulseone-export-gateway
```
*핵심: 수집기가 시뮬레이터에 연결되어 0.0 값을 정상적으로 폴링하는지 확인합니다.*

### 2단계: 외부 트리거 (결함 시뮬레이션)
```bash
docker exec docker-simulator-modbus-1 node scripts/force_modbus.js 100 1
```
*동작: 가상 센서 값을 0에서 1로 변경합니다.*

### 3단계: 파이프라인 체인 감사
1.  **수집기 획득 (Collector Acquisition)**: 
    로그 확인: `Read Point 200 (E2E.ALARM) -> Value: true`.
    *검증: 수집기가 값의 변화를 실제로 감지했는가?*
2.  **알람 엔진 (Alarm Engine)**: 
    로그 확인: `eval.should_trigger: 1` 및 `Publishing alarm event to Redis...`.
    *검증: 위반 사항을 정확히 판단했는가?*
3.  **Redis 전송 (Redis Dispatch)**: 
    확인: `docker exec docker-redis-1 redis-cli MONITOR`.
    로그 확인: `PUBLISH "alarms:all"`.
    *검증: 이벤트가 네트워크 상에 게시되었는가?*
4.  **게이트웨이 수신 (Gateway Reception)**: 
    로그 확인: `[INFO] Received alarm event for rule_id: 200`.
    *검증: 게이트웨이가 이벤트를 수신했는가?*
5.  **타겟 전송 (Target Dispatch)**: 
    로그 확인: `Target result: [HTTP Target] success=1`.
    *검증: 데이터가 최종 목적지에 도달했는가?*

---

## 5. 4단계: 데이터 내용 검증 (최준 감사)

마지막으로 전송된 데이터의 **내용**이 정확한지 확인합니다.

1.  **타임스탬프**: `tm` 필드가 KST 기준 `YYYY-MM-DD HH:mm:ss` 형식인지 확인합니다.
2.  **식별 정보**: `bd` (사이트 ID)가 `1`과 일치하는지 확인합니다.
3.  **매핑 정보**: 필드 이름이 `ENVS_TEST.E2E_POINT`로 변환되었는지 확인합니다.
4.  **지속성**: DB에서 최종 전송 로그를 확인합니다.
    ```sql
    SELECT * FROM export_logs ORDER BY id DESC LIMIT 1;
    ```
