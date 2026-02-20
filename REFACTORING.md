# Export Gateway 리팩터링 계획 및 진행 상황

> 작성: 2026-02-20  
> 대상: `core/export-gateway/`  
> 목표: 코드 구조 개선 + SSL 수정 포함 재빌드 + E2E 테스트

---

## ⚠️ 핵심 제약: 크로스 플랫폼 필수

export-gateway는 아래 **3가지 환경** 모두에서 동일하게 동작해야 합니다:

| 환경 | 실행 방식 | 비고 |
|------|-----------|------|
| **Linux 네이티브** | systemd 데몬 | ARM/x86_64 |
| **Windows 네이티브** | WinSW 서비스 (.exe) | MinGW 크로스컴파일 |
| **Docker 컨테이너** | docker compose | Linux 기반 |

### 코딩 규칙 (위반 금지)
- OS 분기: `#ifdef _WIN32` / `#ifndef _WIN32` 으로만 처리
- 경로: 절대경로 하드코딩 금지, `ConfigManager` 통해 런타임 결정
- 플랫폼 매크로: 기존 `CROSS_COMPILE_WINDOWS=1`, `PULSEONE_LINUX=1` 체계 유지
- SSL: WIN32 빌드에서만 CA 번들 탐색 또는 VERIFYPEER 완화 허용 (`HttpClient.cpp` 기적용)

---

## 배경 설명

### 왜 리팩터링이 필요한가?

현재 `export-gateway`에는 **같은 역할을 하는 코드가 두 곳에 존재합니다:**

```
CSP/ 폴더 (옛날 방식)              Gateway/ 폴더 (새 방식)
────────────────────────           ───────────────────────
ExportCoordinator.cpp              GatewayService.cpp
DynamicTargetManager.cpp           TargetRunner.cpp / TargetRegistry.cpp
CSP/AlarmMessage.h                 Gateway/Model/AlarmMessage.h
CSP/ITargetHandler.h               Gateway/Target/ITargetHandler.h
```

`main.cpp`가 두 구조를 동시에 사용하면서 **ExportCoordinator 안에 GatewayService가 들어있는** 이상한 중첩 구조가 됩니다.

**"God Class"란?** — 하나의 파일/클래스가 너무 많은 역할을 혼자 담당하는 것
- `ExportCoordinator.cpp`: 1731줄, Redis 구독·이벤트 파싱·타겟 관리·로그·스케줄·명령 처리 전부
- `DynamicTargetManager.cpp`: 1439줄, DB 로드·Redis 연결·알람 전송·파일 전송·Circuit Breaker 전부

---

## 리팩터링 범위 및 3단계 계획

### Phase 1: 클린업 (즉시, 리스크 없음)
- [ ] `CSP/AlarmMessage.cpp.bak` 삭제
- [ ] `CSP/FileTargetHandler.cpp.bak` 삭제
- [ ] `CSP/HttpTargetHandler.cpp.bak` 삭제
- [ ] `CSP/MqttTargetHandler.cpp.bak` 삭제
- [ ] `CSP/S3TargetHandler.cpp.bak` 삭제
- [ ] `include/CSP/` 헤더들을 `include/Gateway/` 헤더 참조로 교체 (typedef/alias)
- [ ] `main.cpp` 내 테스트 함수 5개 → `tests/manual/` 로 이동

### Phase 2: 내부 클래스 파일 분리 (중간)
> `ExportCoordinator.cpp` 안에 직접 정의된 3개 클래스를 별도 파일로 분리

- [ ] `ScheduleEventHandler` → `src/Event/ScheduleEventHandler.cpp` 분리
- [ ] `ConfigEventHandler` → `src/Event/ConfigEventHandler.cpp` 분리
- [ ] `CommandEventHandler` → `src/Event/CommandEventHandler.cpp` 분리
- [ ] `ExportCoordinator.cpp` 에서 분리된 클래스 include 로 교체
- [ ] 빌드 검증 (Linux 네이티브)

### Phase 3: 이중 아키텍처 통합 (핵심)
> CSP/ExportCoordinator의 역할을 Gateway/GatewayService로 흡수

- [ ] `DynamicTargetManager::sendAlarmToTargets` 로직을 `TargetRunner`로 완전 이전
  - 현재 TargetRunner와 기능 중복이 존재
- [ ] `ExportCoordinator::start()` 의 초기화 흐름을 `main.cpp`으로 올림
  - GatewayService가 직접 DynamicTargetManager를 받도록 변경
- [ ] ExportCoordinator를 얇은 "부트스트래퍼"로만 남기거나 제거
- [ ] `CSP` 네임스페이스 타입들을 `Gateway` 네임스페이스 타입으로 전환
- [ ] 빌드 검증 (Linux + Windows 크로스컴파일)

---

## SSL 수정 사항 (이미 완료)

`core/shared/src/Client/HttpClient.cpp` 수정됨:
- WIN32 빌드 시 Linux CA 번들 자동 탐지 후 `CURLOPT_CAINFO` 설정
- CA 번들 없으면 Wine 환경을 위해 `CURLOPT_SSL_VERIFYPEER=0` 폴백

---

## 최종 빌드 및 테스트

- [ ] Phase 1~3 완료 후 Linux 빌드 검증
```bash
docker compose -f docker-compose.dev.yml build export-gateway
docker compose -f docker-compose.dev.yml restart export-gateway
```
- [ ] Windows 크로스컴파일 빌드
```bash
docker run --rm -v "$(pwd)/core:/src/core" pulseone-windows-builder bash -c \
  "cd /src/core/shared && make CROSS_COMPILE_WINDOWS=1 client -j2 && \
   cd /src/core/export-gateway && make CROSS_COMPILE_WINDOWS=1 -j2"
```
- [ ] Wine E2E 테스트: alarms:all → HTTP target → HTTP 200
- [ ] Wine E2E 테스트: alarms:all → S3 target → HTTPS 성공 (SSL 수정 검증)

---

## 진행 현황

| Phase | 상태 | 비고 |
|-------|------|------|
| Phase 1: 클린업 | 🔲 대기 | |
| Phase 2: 클래스 분리 | 🔲 대기 | |
| Phase 3: 아키텍처 통합 | 🔲 대기 | |
| 빌드 검증 | 🔲 대기 | |
| E2E 테스트 | 🔲 대기 | |

> 세션이 끊기면 이 파일을 열고 "진행 현황" 표를 보고 이어서 진행하세요.
