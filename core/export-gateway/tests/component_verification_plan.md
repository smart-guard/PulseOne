# Component Verification Plan - Selective Subscription

이 문서는 Selective Subscription 아키텍처의 핵심 컴포넌트 로직을 E2E 테스트와 별개로 검증하기 위한 계획을 담고 있습니다.

## 1. 개요
전체 시스템 흐름을 확인하는 E2E 테스트 이전에, 개별 컴포넌트(`DynamicTargetManager`, `EventSubscriber`)의 핵심 로직(SQL 쿼리 정확성, Redis 채널 관리 logic)을 독립적으로 검증하여 신뢰성을 확보합니다.

## 2. 테스트 항목 및 방법 (상세)

### 2.1 DynamicTargetManager - C++ 로직 및 DB 연동 검증
*   **목표**: 단순히 SQL뿐만 아니라, `DynamicTargetManager::loadFromDatabase()`가 실행될 때 DB 결과를 `assigned_device_ids_` 멤버 변수에 정확히 담는지 확인합니다.
*   **검증 방법**:
    *   **Unit Test Code**: `DynamicTargetManagerTest.cpp`와 같은 전용 테스트 코드를 작성하여 `setGatewayId()` -> `loadFromDatabase()` 호출 후 `getAssignedDeviceIds()`의 반환값을 검증합니다.
    *   **실제 DB 활용**: 프로젝트 내의 실제 `pulseone.db` 스키마를 사용하여 데이터 간 관계(Target-Point-Device-Profile)가 모두 얽힌 상태에서 쿼리가 도는지 확인합니다.

### 2.2 EventSubscriber - Redis 구독 상태 전이 검증
*   **목표**: 디바이스 목록이 A에서 B로 바뀔 때, Redis에 실제로 `UNSUBSCRIBE A`, `SUBSCRIBE B` 명령이 전달되는지 확인합니다.
*   **검증 방법**:
    *   **Internal State Check**: `getDetailedStats()` API를 통해 현재 구독 중인 채널 목록(`subscribed_channels`)을 JSON으로 확인합니다.
    *   **Log Auditing**: 실행 중인 로그에서 "더 이상 필요 없는 채널 구독 해제", "새로운 디바이스 채널 구독" 메시지가 출력되는지 모니터링합니다.

### 2.3 ExportCoordinator - 통합 흐름 검증
*   **목표**: 타겟 리로드 시 자동으로 구독 업데이트가 유발되는지 확인합니다.
*   **검증 방법**: `target:reload` 이벤트를 Redis로 발행한 뒤, 게이트웨이가 타겟을 다시 읽고 구독 채널을 자동으로 변경하는지 확인합니다.

## 3. 테스트 실행 및 확인 지표

### 3.1 로그 레벨 확인 (Log Auditing)
*   **정상 지표**:
    1.  `✅ 할당된 디바이스 ID 수집 완료: N개` (DynamicTargetManager)
    2.  `Selective Subscription 활성화: N개 디바이스 채널 설정` (ExportCoordinator)
    3.  `새로운 디바이스 채널 구독: device:DEV_001:alarms` (EventSubscriber)

### 3.2 상태 조회 (API/Stats)
*   `EventSubscriber::getDetailedStats()` 호출 결과에 새로 추가된 디바이스 채널들이 포함되어 있어야 함.

## 4. 테스트 준비 사항 (Preparation)

성공적인 컴포넌트 테스트를 위해 아래 사항들을 순서대로 준비해야 합니다.

### 4.1 데이터베이스 시딩 (DB Seeding)
Selective Subscription의 '격리' 성능을 확인하기 위해 다음과 같은 데이터를 준비합니다.
*   **기기 등록**: 최소 2개의 기기(`DEV_001`, `DEV_002`) 등록
*   **프로파일 및 게이트웨이 할당**: 
    - 게이트웨이 `ID 10` → 프로파일 A 할당
    - 프로파일 A → 타겟 T1 포함
    - 타겟 T1 → `DEV_001`의 포인트들만 매핑
*   **결과**: 게이트웨이 10 실행 시 로그에 `DEV_001` 채널은 구독하되, `DEV_002` 채널은 언급되지 않아야 함.

### 4.2 인프라 및 서비스 확인
*   **Redis 서버**: Redis 서버가 실행 중이어야 합니다. (`EventSubscriber` 연동 확인용)
*   **로그 레벨 설정**: `config.json` 또는 환경 설정에서 로그 레벨을 최소 `Info` 이상으로 설정하여 구독 관련 로그가 출력되도록 확인합니다.

### 4.3 빌드 및 실행 준비
*   `core/export-gateway` 디렉토리에서 수정한 코드가 반영되도록 다시 빌드합니다.
*   테스트 시 게이트웨이를 특정 ID로 실행합니다: `./export-gateway --gateway-id 10`
