-- Selective Subscription SQL Verification Script

-- 1. 테이블 생성
CREATE TABLE data_points (
    id INTEGER PRIMARY KEY,
    device_id TEXT NOT NULL
);

CREATE TABLE export_targets (
    id INTEGER PRIMARY KEY,
    profile_id INTEGER,
    is_enabled INTEGER DEFAULT 1
);

CREATE TABLE export_target_mappings (
    id INTEGER PRIMARY KEY,
    target_id INTEGER,
    point_id INTEGER,
    is_enabled INTEGER DEFAULT 1
);

CREATE TABLE export_profile_assignments (
    id INTEGER PRIMARY KEY,
    gateway_id INTEGER,
    profile_id INTEGER
);

-- 2. 테스트 데이터 삽입
-- 디바이스 A (기기 1, 2)
INSERT INTO data_points (id, device_id) VALUES (1, 'DEV_001');
INSERT INTO data_points (id, device_id) VALUES (2, 'DEV_001');
INSERT INTO data_points (id, device_id) VALUES (3, 'DEV_002');

-- 디바이스 B (기기 3) - 다른 게이트웨이용
INSERT INTO data_points (id, device_id) VALUES (4, 'DEV_003');

-- 타겟 설정
INSERT INTO export_targets (id, profile_id, is_enabled) VALUES (101, 1, 1); -- 프로파일 1 소속
INSERT INTO export_targets (id, profile_id, is_enabled) VALUES (102, 2, 1); -- 프로파일 2 소속

-- 매핑 설정
INSERT INTO export_target_mappings (target_id, point_id, is_enabled) VALUES (101, 1, 1); -- DEV_001
INSERT INTO export_target_mappings (target_id, point_id, is_enabled) VALUES (101, 3, 1); -- DEV_002
INSERT INTO export_target_mappings (target_id, point_id, is_enabled) VALUES (102, 4, 1); -- DEV_003

-- 게이트웨이 할당 (GATEWAY_10 -> PROFILE_1)
INSERT INTO export_profile_assignments (gateway_id, profile_id) VALUES (10, 1);

-- 3. 검증 쿼리 실행 (GATEWAY_10 에 할당된 device_id 추출)
-- 예상 결과: DEV_001, DEV_002
SELECT DISTINCT dp.device_id
FROM data_points dp
JOIN export_target_mappings etm ON dp.id = etm.point_id
JOIN export_targets et ON etm.target_id = et.id
JOIN export_profile_assignments epa ON et.profile_id = epa.profile_id
WHERE epa.gateway_id = 10 AND et.is_enabled = 1 AND etm.is_enabled = 1;
