# 스키마 정밀 대조 결과 보고 (코드 vs SQL 파일)

## 개요
현재 `data/sql/schema.sql`은 실제 시스템([백엔드](file:///Users/kyungho/Project/PulseOne/backend/lib/database/repositories/DeviceRepository.js) 및 [콜렉터](file:///Users/kyungho/Project/PulseOne/core/shared/include/Database/Entities/DeviceEntity.h))이 요구하는 물리적 테이블 구조와 **심각하게 분리(Decoupled)**되어 있습니다. 

## 상세 불일치 내역

| 테이블 | 소스코드 요구 사항 (C++/Node.js) | `schema.sql` 상태 | 비고 |
| :--- | :--- | :--- | :--- |
| **devices** | `protocol_instance_id`, `polling_interval`, `timeout`, `retry_count`, `is_deleted` | 줄 끝에 콤마 없이 매달려 있거나 부실하게 패치됨 | 런타임 쿼리 오류 위험 |
| **protocol_instances** | `tenant_id`, `broker_type` | 줄 끝에 수동으로 덧붙여진 형태 | 마이그레이션 미반영 |
| **protocols** | `vendor`, `category`, `default_polling_interval`, `standard_reference` | 낙후된 초기 버전 구조 (확인 필요) | 콜렉터 로딩 시 실패 가능성 |
| **data_points** | `log_enabled`, `log_interval_ms`, `log_deadband` | 2025년 중반 버전에서 멈춤 | 데이터 수집 누락 가능성 |

## 결론
1.  **`schema.sql`은 버려진 유산**입니다. 서비스 구동 시마다 마이그레이션이 수동으로(또는 누락된 채로) 적용된 결과물일 뿐, 단일 진실 공급원(SSOT)이 아닙니다.
2.  **`seed.sql`은 허상**입니다. `Site 280`과 같은 핵심 데이터의 '부모(Site 정의)' 없이 '자식(Mapping)'만 있는 깨진 상태입니다.

## 제안
`data/sql` 폴더를 믿지 말고, **[Migrations](file:///Users/kyungho/Project/PulseOne/backend/migrations)**를 원점에서 모두 돌린 후, 소스코드의 **[Entities](file:///Users/kyungho/Project/PulseOne/core/shared/include/Database/Entities)** 정의를 기준으로 스키마를 덤프하여 정교하게 재구축해야 합니다.
