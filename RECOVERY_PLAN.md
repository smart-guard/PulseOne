# PulseOne: Docker Recovery & Verification Plan

도커 재시작 후 다음 순서대로 작업을 진행합니다. 

## 1. 환경 확인 (Connectivity Check)
- [ ] `docker ps` 명령어로 도커 데몬 응답 확인.
- [ ] `export-gateway`, `redis`, `pulseone-db` 컨테이너 상태 확인 ("Up" 상태).

## 2. 안전 빌드 (Safe Build in Container)
호스트가 아닌 **컨테이너 내부**에서 순차적으로 빌드합니다.

### A. Shared Library 빌드
```bash
docker exec export-gateway make clean -C core/shared
docker exec export-gateway make -j1 -C core/shared  # 단일 스레드로 안전하게 빌드
```

### B. Gateway Service 빌드
```bash
docker exec export-gateway make clean -C core/export-gateway
docker exec export-gateway make -j1 -C core/export-gateway # 단일 스레드로 안전하게 빌드
```

## 3. 서비스 재시작 (Restart)
변경된 바이너리를 적용하기 위해 컨테이너를 재시작합니다.
```bash
docker restart export-gateway
```

## 4. 기능 검증 (Verification)

### A. 로그 모니터링 시작
```bash
docker logs -f export-gateway
```

### B. 테스트 #1: 시뮬레이션 알람 (Harvesting & Aliasing 검증)
Redis를 통해 직접 알람을 주입하여 `payload_normalization` 로직이 정상 동작하는지 확인합니다.
```bash
docker exec redis-cli PUBLISH alarms:all '{"site_id": 280, "point_id": 43, "trigger_value": "99.9", "state": "active", "extra_field": "harvest_me"}'
```
**예상 로그:**
- `[ALARM_PARSE]`: `site_id`, `point_id` 등 필드 파싱 확인.
- `[ALARM_ENRICH]`: 매핑 정보 적용 확인.
- `[TRACE-TRANSFORM]`: `extra_field`가 하베스팅되어 변환 컨텍스트에 포함되었는지 확인.

### C. 테스트 #2: 수동 전송 (Manual Export RAW Bypass 검증)
```bash
docker exec redis-cli PUBLISH cmd:gateway:1 '{"command": "MANUAL_EXPORT", "payload": {"target_name": "S3_Target", "raw_payload": {"custom": "data"}}}'
```
**예상 로그:**
- `[TRACE-3-ENRICH]`: Manual Override 활성화 확인.
- `[ALARM_SEND]`: 원본 데이터(`{"custom": "data"}`)가 변형 없이 전송되었는지 확인.

---
**원칙:** 모든 명령어는 호스트가 아닌 Docker Container 내부 또는 Docker CLI를 통해서만 실행합니다.
