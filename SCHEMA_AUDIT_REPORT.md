# PulseOne Schema-Code 전수 조사 보고서
> 기준: C++ SQLQueries.h / ExportSQLQueries.h / ExtendedSQLQueries.h (코드가 진실의 원천)
> 대상: data/sql/schema.sql, data/sql/seed.sql, backend 코드

---

## 🔴 CRITICAL: 불일치 항목 목록

### 1. `data_points.chk_data_type` — 헤더 vs schema.sql 서로 다름

| 위치 | 허용 타입 |
|------|-----------|
| **schema.sql** (실제 DB) | `BOOL, INT8, UINT8, INT16, UINT16, INT32, UINT32, INT64, UINT64, FLOAT32, FLOAT64, STRING, UNKNOWN` |
| **SQLQueries.h** `DataPoint::CREATE_TABLE` | `BOOL, INT8, UINT8, INT16, UINT16, INT32, UINT32, INT64, UINT64, FLOAT, DOUBLE, FLOAT32, FLOAT64, STRING, BINARY, DATETIME, JSON, ARRAY, OBJECT, UNKNOWN` |

**▶ 수정 방향**: schema.sql을 SQLQueries.h 기준으로 확장 — `FLOAT, DOUBLE, BINARY, DATETIME, JSON, ARRAY, OBJECT` 추가

---

### 2. `devices` 테이블 — SQLQueries.h INSERT/SELECT에 없는 컬럼들

`schema.sql`에 있지만 `SQLQueries.h Device::INSERT/SELECT`에 **빠진 컬럼들**:

| 컬럼 | schema.sql 정의 | SQLQueries.h INSERT/SELECT 포함 여부 |
|------|----------------|--------------------------------------|
| `firmware_version` | `VARCHAR(50)` | ❌ 누락 |
| `next_maintenance` | `DATE` | ❌ 누락 |
| `warranty_expires` | `DATE` | ❌ 누락 |
| `is_deleted` | `INTEGER DEFAULT 0` | ❌ 누락 |
| `is_simulation_mode` | `INTEGER DEFAULT 0` | ❌ 누락 |
| `priority` | `INTEGER DEFAULT 100` | ❌ 누락 |
| `tags` | `TEXT` | ❌ 누락 |
| `metadata` | `TEXT` | ❌ 누락 |
| `custom_fields` | `TEXT` | ❌ 누락 |
| `template_device_id` | `INTEGER` | ❌ 누락 |
| `manufacturer_id` | `INTEGER` | ❌ 누락 |
| `model_id` | `INTEGER` | ❌ 누락 |
| `location_description` | `VARCHAR(200)` | ❌ 누락 |

**▶ 수정 방향**: SQLQueries.h Device 네임스페이스 SELECT/INSERT 쿼리에 누락 컬럼 추가, 또는 schema.sql에서 C++이 사용안하는 컬럼 DEFAULT값 보장

> ⚠️ **운영 영향**: C++ collector가 devices 테이블 INSERT 시 `NOT NULL` 없는 컬럼들은 DEFAULT로 채워지므로 즉각적 장애 없음. SELECT 시 누락 컬럼은 entity 매핑에서 기본값 사용.

---

### 3. `alarm_rules.is_deleted` — ExtendedSQLQueries.h vs schema.sql 불일치

| 위치 | is_deleted |
|------|-----------|
| **schema.sql** `alarm_rules` | `is_deleted BOOLEAN DEFAULT 0` ✅ 있음 |
| **ExtendedSQLQueries.h** `AlarmRule::CREATE_TABLE` | ❌ **없음** |
| **ExtendedSQLQueries.h** `AlarmRule::FIND_ENABLED` | `WHERE is_enabled = 1 AND is_deleted = 0` — 조회는 참조 |
| **ExtendedSQLQueries.h** `AlarmRule::FIND_BY_TARGET` | `AND is_deleted = 0` — 조회는 참조 |
| **ExtendedSQLQueries.h** `AlarmRule::COUNT_ENABLED` | `AND is_deleted = 0` — 조회는 참조 |

**▶ 수정 방향**: `ExtendedSQLQueries.h AlarmRule::CREATE_TABLE`에 `is_deleted BOOLEAN DEFAULT 0` 추가

---

### 4. `export_profiles` 테이블 — seed.sql에 존재하지 않는 컬럼 삽입

**schema.sql `export_profiles`** 정의:
```sql
id, name, description, is_enabled, created_at, updated_at, created_by, point_count, last_exported_at
```

**seed.sql** 실제 INSERT (줄 193):
```sql
INSERT INTO export_profiles VALUES(3,'insite 알람셋',NULL,1,'...','...',NULL,0,NULL,'[{...}]');
```
→ 10번째 컬럼 `'[{...}]'` (points JSON) — **schema.sql에 없는 컬럼**

**▶ 수정 방향**: seed.sql에서 해당 컬럼 제거, 또는 schema.sql에 `points TEXT` 컬럼 추가

---

### 5. `AlarmRule::UPDATE` 쿼리 — named placeholder `{var}` 형식 사용

`ExtendedSQLQueries.h AlarmRule::UPDATE`:
```cpp
WHERE id = {id}
SET tenant_id = {tenant_id}, ...
```
SQLite는 `{var}` 방식 불지원. SQLite는 `?` 또는 `:name` 방식만 지원.

**▶ 수정 방향**: `?` 방식으로 변경 필요 (현재 C++ 코드에서 실제 사용 여부 확인 필요)

---

### 6. `AlarmOccurrence::UPDATE` 쿼리 — 동일 문제

`ExtendedSQLQueries.h AlarmOccurrence::UPDATE`:
```cpp
WHERE id = {id}
SET occurrence_time = {occurrence_time}, ...
```
→ 동일하게 `{var}` 방식 사용. SQLite 비호환.

---

### 7. `devices` SELECT 쿼리 — schema.sql에 없는 `device_details` 뷰 참조

`SQLQueries.h Device::FIND_WITH_PROTOCOL_INFO`:
```sql
FROM device_details  -- 뷰
WHERE id = ?
```
`schema.sql`에 `device_details` 뷰 존재 여부 확인 필요 (schema.sql에서 미발견).

---

### 8. `script_library` — `script_templates` 테이블 참조

`ExtendedSQLQueries.h ScriptLibrary::FIND_TEMPLATES`:
```sql
FROM script_templates
```
`schema.sql`에 `script_templates` 테이블 **없음** → 런타임 에러 발생 가능.

---

### 9. `virtual_points.data_type` chk_data_type — schema.sql 허용값 좁음

| 위치 | 허용 data_type |
|------|---------------|
| **schema.sql** `virtual_points` | `'bool', 'int', 'float', 'double', 'string'` (소문자) |
| **seed.sql** `virtual_points` INSERT | `'float'` — OK |
| **ExtendedSQLQueries.h** `VirtualPoint::INSERT` | 제약 없음 |

→ 현재 seed.sql은 OK이나, 코드에서 `'integer'`, `'FLOAT32'` 등 다른 값 삽입 시 에러.

---

### 10. `win-collector` 테스트용 edge_server 부재 (seed.sql)

seed.sql의 edge_servers: id=1(Main Collector), id=2(NY), id=3(Demo), id=4(Test)
- `win-collector` 전용 entry 없음 → 테스트 시 수동 INSERT 필요

**▶ seed.sql에 win-collector entry 추가 필요**:
```sql
INSERT INTO edge_servers VALUES(5,1,'win-collector','collector',NULL,'Windows Test Env',NULL,'127.0.0.1',NULL,NULL,8080,NULL,NULL,NULL,NULL,'active',NULL,NULL,'2.1.0',0.0,0.0,0.0,0,'{}',NULL,1,1,NULL,NULL,0,NULL,100,1000,'all');
```

---

## 🟡 WARNING: 주의 사항

### W1. `devices.chk_device_type` — seed.sql vs schema.sql
- schema.sql: `'PLC','HMI','SENSOR','GATEWAY','METER','CONTROLLER','ROBOT','INVERTER','DRIVE','SWITCH'`
- seed.sql devices: `'HMI'(✅), 'PLC'(✅), 'SENSOR'(✅), 'CONTROLLER'(✅)` — OK

### W2. `DeviceSettings::FIND_ALL` 쿼리 컬럼 수 확인 필요
- schema.sql `device_settings`는 약 25개 컬럼
- SQLQueries.h DeviceSettings 네임스페이스 쿼리 확인 필요

### W3. backend가 `export_profiles`에 `points` JSON 컬럼 직접 쓰는 코드 있는지 확인
- backend 코드에서 `export_profiles` 테이블 컬럼 정의와 seed.sql 불일치

---

## ✅ 정상 일치 항목

| 항목 | 상태 |
|------|------|
| `data_points` 주요 컬럼 (log_enabled, log_interval_ms 등) | ✅ 일치 |
| `alarm_occurrences` 모든 컬럼 | ✅ 일치 |
| `export_targets` 구조 | ✅ 일치 |
| `export_target_mappings` 구조 | ✅ 일치 |
| `payload_templates` 구조 | ✅ 일치 |
| `protocols` 구조 | ✅ 일치 |
| `edge_servers` 주요 컬럼 (status, version, config, subscription_mode) | ✅ schema.sql에 있음 |
| `virtual_points` 주요 컬럼 | ✅ 일치 |
| `alarm_rules` 주요 컬럼 (is_deleted 제외) | ✅ 일치 |

---

## 📋 수정 우선순위

| 우선순위 | 항목 | 파일 |
|---------|------|------|
| 🔴 P1 | `data_points.chk_data_type` schema.sql 확장 | schema.sql |
| 🔴 P1 | `alarm_rules.is_deleted` ExtendedSQLQueries.h CREATE_TABLE 추가 | ExtendedSQLQueries.h |
| 🔴 P1 | win-collector edge_server seed.sql 추가 | seed.sql |
| 🔴 P1 | `export_profiles` seed.sql 10번째 컬럼 제거 | seed.sql |
| 🟡 P2 | `devices` SQLQueries.h SELECT/INSERT 누락 컬럼 추가 | SQLQueries.h |
| 🟡 P2 | `AlarmRule::UPDATE` named placeholder → `?` 변환 | ExtendedSQLQueries.h |
| 🟡 P2 | `script_templates` 테이블 schema.sql 추가 또는 쿼리 제거 | schema.sql / ExtendedSQLQueries.h |
| 🟢 P3 | `device_details` 뷰 schema.sql 추가 확인 | schema.sql |
