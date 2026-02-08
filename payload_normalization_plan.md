# PulseOne: Payload Normalization & Identity Resolution Plan (v3.2.0)

This document outlines the evidence-based plan to align the Export Gateway with the Collector's descriptive data format and implement a robust, zero-assumption template engine.

## 1. System Context & Evidence (Confirmed via Code)

### A. Collector Output 규격
`core/collector/include/Storage/BackendFormat.h`의 `AlarmEventData` 구조체 확인 결과:
- **사용 키:** `site_id`, `point_id`, `rule_id`, `trigger_value`, `timestamp`, `state`, `severity`.
- **확인 사항:** 컬렉터는 더 이상 `bd`, `vl` 등의 축약형 키를 사용하지 않으며, 모든 데이터를 서술형(Descriptive)으로 송신함.

### B. 게이트웨이 소유권(Ownership) 로직
`core/export-gateway/src/CSP/DynamicTargetLoader.cpp` 확인 결과:
- **메커니즘:** 게이트웨이 기동 시 `export_profile_assignments` 조인을 통해 자신이 담당한 프로파일과 해당 포인트들의 `device_id` 목록을 추출함.
- **구독 방식:** Redis에서 `device:<id>:alarms` 채널을 **선택적 구독(Selective Subscription)**하여, 자신이 "소유"한 디바이스의 알람만 처리함.

### C. 수동 전송(Manual Export) 경로
`core/export-gateway/src/CSP/ExportCoordinator.cpp` 확인 결과:
- **동작:** Redis의 `point:<id>:latest`에서 최신값을 읽어오며, 자동 알람 전송과 동일한 정규화(Normalization) 로직을 공유함.

---

## 2. 세부 실행 계획

### [Phase 1] 내부 모델 정규화 (Internal Sanitization)
- **대상:** `core/export-gateway/include/Gateway/Model/AlarmMessage.h`
- **변경:** 모든 내부 멤버 변수명을 낡은 축약형(`bd`, `vl`, `nm`)에서 서술형(`site_id`, `measured_value`, `point_name`)으로 변경.
- **목적:** 코드 읽기에서의 혼선을 제거하고 컬렉터 규격과 1:1로 일치시킴.

### [Phase 2] 서술형 키 우선 파싱 (Descriptive Parsing)
- **대상:** `AlarmMessage::from_json`
- **변경:** `site_id`, `trigger_value`, `point_id` 등 컬렉터가 실제로 보내는 키를 최우선적으로 파싱하도록 로직 재정렬.
- **대상:** `AlarmMessage::to_json`
- **변경:** 템플릿이 없을 경우 출력되는 기본 JSON 규격을 서술형 키로 고정.

### [Phase 3] Zero-Assumption & UI Alignment
- **대상:** `core/export-gateway/src/Transform/PayloadTransformer.cpp`
- **변경:**
    1. **Harvesting:** 입력된 원본 JSON(`extra_info`)의 모든 키를 자동으로 토큰화.
    2. **UI Alignment:** 현재 UI(TemplateManagementTab.tsx)는 이미 `{{site_id}}`, `{{measured_value}}` 등을 사용 중임. 따라서 백엔드의 새로운 서술형 멤버들을 해당 이름의 토큰과 1:1 매핑.
    3. **Legacy Aliasing (중요):** 기존에 생성된 템플릿 소급 적용을 위해 `{{bd}}` -> `site_id`, `{{vl}}` -> `measured_value`와 같은 별칭(Alias)을 토큰 풀에 명시적으로 추가.
- **효과:** 신규 UI와 기존 템플릿이 모두 오차 없이 동작함.

### [Phase 4] 4단계 추적 로그 (Traceability)
검증을 위해 다음과 같은 4단계 Tracing Log를 구현:
1. `[ALARM_RECEIVE]`: Redis에서 수신한 원본 JSON 데이터.
2. `[ALARM_PARSE]`: 정규화가 완료된 내부 `AlarmMessage` 객체 상태.
3. `[ALARM_ENRICH]`: DB 매핑 조회 결과 (Mapped Name, Scale, Offset 적용 등).
4. `[ALARM_SEND]`: 타겟 핸들러로 전달되는 최종 전송 페이로드.

---

## 3. 검증 계획 (Verification)

> [!IMPORTANT]
> **DB 및 시스템 접근 원칙:**
> - 모든 DB 검증은 `docker exec -it PulseOne-DB sqlite3 /data/database.db`를 통해서만 수행.
> - 로컬 터미널 및 직접 파일 접근 절대 금지.

### A. 시뮬레이션 알람 테스트
- **명령:** `docker exec redis-cli PUBLISH alarms:all '{"site_id": 280, "point_id": 43, "trigger_value": "99.9", "state": "active"}'`
- **확인:** 게이트웨이 로그의 4단계 추적 정보와 타겟 전송 여부 확인.

### B. 수동 전송 테스트
- **명령:** `docker exec redis-cli PUBLISH cmd:gateway:12 '{"command": "MANUAL_EXPORT", "payload": {"point_id": 43}}'`
- **확인:** `export_logs` 테이블에 `sent_payload` 및 `gateway_id`가 정확히 기록되었는지 확인.
