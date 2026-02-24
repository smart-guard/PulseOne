# Supervisor Mode 설계

## 현재 상태

### Collector (`collector/src/main.cpp`)
- `--id=N` → 해당 ID로 실행
- `--id` 없음 → `ResolveCollectorId()`로 빈 슬롯 자동 클레임
- **항상 프로세스 1개만 동작**

### Export Gateway (`export-gateway/src/main.cpp`)
- `--id=N` → 해당 ID로 실행
- `--id` 없음 → `gateway_id = "default"` → ID=0 → DB에 없음 → 실패
- `--list-gateways` → DB에서 active gateway 목록 출력 후 종료

### 공통점
- 둘 다 `edge_servers` 테이블 사용 (`server_type = 'collector' | 'gateway'`)
- 둘 다 `--id=N` 인자 파싱 지원

---

## 목표

- `--id` 없이 실행하면 → **supervisor 모드**
  - DB에서 해당 타입 active 인스턴스 전부 조회
  - 각 ID마다 자식 프로세스 spawn (`자기자신.exe --id=N`)
  - 자식 감시, 죽으면 재시작
  - 주기적 DB 재조회 → 새 인스턴스 추가/삭제된 인스턴스 정리

- `--id=N`으로 실행하면 → **worker 모드** (현재 동작 그대로)

---

## Supervisor 동작 흐름

```
main() 시작
  ├── --id 있음? → worker 모드 (현재 코드, 변경 없음)
  └── --id 없음? → supervisor 모드
       │
       ├── 1. ConfigManager 초기화 (DB 접근용)
       ├── 2. DatabaseManager 초기화
       ├── 3. DB 조회
       │     Collector: SELECT id FROM edge_servers
       │                WHERE server_type='collector' AND status='active' AND is_deleted=0
       │     Gateway:   SELECT id FROM edge_servers
       │                WHERE server_type='gateway' AND status='active' AND is_deleted=0
       │
       ├── 4. 각 ID마다 자식 프로세스 spawn
       │     Windows: CreateProcess("자기경로.exe --id=N --config=...")
       │     Linux:   fork + exec("자기경로 --id=N --config=...")
       │
       ├── 5. 메인 루프 (30초 주기)
       │     ├── 자식 생존 확인 → 비정상 종료 시 재시작
       │     └── 60초 주기로 DB 재조회 → 추가/삭제 반영
       │
       └── 6. 종료 시그널 수신 → 모든 자식 종료 → 종료
```

---

## 수정 파일

### Collector

| 파일 | 변경 |
|------|------|
| `collector/src/main.cpp` | `--id` 없으면 supervisor 모드 진입. 기존 auto-claim 로직 대신 supervisor가 전체 관리 |

### Export Gateway

| 파일 | 변경 |
|------|------|
| `export-gateway/src/main.cpp` | `--id` 없으면 supervisor 모드 진입 (기존 `--list-gateways` 쿼리 재활용) |

### 공통 유틸

| 파일 | 변경 |
|------|------|
| `core/shared/src/Utils/ProcessSupervisor.h/.cpp` [NEW] | spawn/monitor/kill 로직. 크로스 플랫폼 (`CreateProcess` / `fork+exec`) |

### deploy-windows.sh

| 변경 |
|------|
| `--id=6` 하드코딩 제거 (supervisor가 자동으로 처리) |
| WinSW XML에 `<arguments>` 불필요 |

---

## Collector 기존 auto-claim과의 관계

현재 Collector `--id` 없이 실행 → `ResolveCollectorId()` 호출 → 빈 슬롯 클레임

**변경 후:**
- `--id` 없이 실행 → supervisor 모드 → DB에서 active collector 전부 조회 → 각각 `--id=N`으로 spawn
- 각 자식은 `--id=N`으로 실행되므로 `ResolveCollectorId()`의 explicit_id 경로로 동작
- `ResolveCollectorId()`의 auto-claim 로직은 **그대로 유지** (하위 호환)

> 기존에 `--id` 없이 단독 실행하던 환경 → supervisor가 1개만 spawn하면 동일 효과

---

## 작업량

| 항목 | 예상 코드량 |
|------|------------|
| `ProcessSupervisor.h` | ~50줄 (인터페이스) |
| `ProcessSupervisor.cpp` | ~200줄 (spawn, monitor, 크로스 플랫폼) |
| Collector `main.cpp` 수정 | ~30줄 (supervisor 분기) |
| Gateway `main.cpp` 수정 | ~30줄 (supervisor 분기) |
| `deploy-windows.sh` 수정 | ~10줄 (하드코딩 제거) |
| **합계** | ~320줄 |
