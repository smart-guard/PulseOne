-- =============================================================================
-- 가상포인트 수정 - 실제 존재하는 포인트 ID 사용
-- 테스트에서 사용하는 실제 포인트: 1,2,3,4,5
-- =============================================================================

BEGIN TRANSACTION;

-- 기존 테스트 가상포인트 정리
DELETE FROM virtual_point_values WHERE virtual_point_id IN (1,2,3,4,5,6,7,8);
DELETE FROM virtual_point_inputs WHERE virtual_point_id IN (1,2,3,4,5,6,7,8);
DELETE FROM virtual_points WHERE id IN (1,2,3,4,5,6,7,8);

-- =============================================================================
-- 🧮 실제 포인트 ID를 사용하는 테스트 가상포인트들
-- =============================================================================

-- 1. Motor Current + Line Speed 합계 (Point 3 + Point 2)
INSERT INTO virtual_points (
    id, tenant_id, scope_type, site_id, device_id,
    name, description, formula, data_type, unit,
    calculation_trigger, calculation_interval, is_enabled,
    category, tags
) VALUES (
    1, 1, 'device', 1, 1,
    'TEST_Motor_Speed_Sum', 'Motor Current + Line Speed 합계', 
    'var current = getPointValue(3) || 0; var speed = getPointValue(2) || 0; return current + speed;',
    'float', 'mixed',
    'onchange', 2000, 1,
    'test', '["test", "simple"]'
);

-- 가상포인트 1 입력 매핑
INSERT INTO virtual_point_inputs (virtual_point_id, variable_name, source_type, source_id) VALUES 
(1, 'current', 'data_point', 3),  -- Motor_Current (실제 존재)
(1, 'speed', 'data_point', 2);    -- Line_Speed (실제 존재)

-- 2. Production Count 2배 (Point 1 * 2)
INSERT INTO virtual_points (
    id, tenant_id, scope_type, site_id, device_id,
    name, description, formula, data_type, unit,
    calculation_trigger, calculation_interval, is_enabled,
    category, tags
) VALUES (
    2, 1, 'device', 1, 1,
    'TEST_Production_Double', 'Production Count 2배',
    'var production = getPointValue(1) || 0; return production * 2;',
    'float', 'count',
    'onchange', 3000, 1,
    'test', '["test", "production"]'
);

-- 가상포인트 2 입력 매핑
INSERT INTO virtual_point_inputs (virtual_point_id, variable_name, source_type, source_id) VALUES 
(2, 'production', 'data_point', 1);  -- Production_Count (실제 존재)

-- 3. Temperature + 10도 (Point 4 + 10)
INSERT INTO virtual_points (
    id, tenant_id, scope_type, site_id, device_id,
    name, description, formula, data_type, unit,
    calculation_trigger, calculation_interval, is_enabled,
    category, tags
) VALUES (
    3, 1, 'device', 1, 1,
    'TEST_Temperature_Plus10', 'Temperature + 10도',
    'var temp = getPointValue(4) || 0; return temp + 10;',
    'float', '°C',
    'onchange', 1000, 1,
    'test', '["test", "temperature"]'
);

-- 가상포인트 3 입력 매핑
INSERT INTO virtual_point_inputs (virtual_point_id, variable_name, source_type, source_id) VALUES 
(3, 'temperature', 'data_point', 4);  -- Temperature (실제 존재)

-- 4. 상수값 테스트 (고정값 100)
INSERT INTO virtual_points (
    id, tenant_id, scope_type, site_id, device_id,
    name, description, formula, data_type, unit,
    calculation_trigger, calculation_interval, is_enabled,
    category, tags
) VALUES (
    4, 1, 'device', 1, 1,
    'TEST_Constant_100', '상수값 100 테스트',
    'return 100;',
    'float', 'fixed',
    'timer', 5000, 1,
    'test', '["test", "constant"]'
);

-- 가상포인트 4는 입력 매핑 없음 (상수값)

-- 5. Emergency Stop 상태 반전 (Point 5 NOT)
INSERT INTO virtual_points (
    id, tenant_id, scope_type, site_id, device_id,
    name, description, formula, data_type, unit,
    calculation_trigger, calculation_interval, is_enabled,
    category, tags
) VALUES (
    5, 1, 'device', 1, 1,
    'TEST_Emergency_Inverted', 'Emergency Stop 상태 반전',
    'var emergency = getPointValue(5) || 0; return emergency > 0 ? 0 : 1;',
    'float', 'bool',
    'onchange', 1000, 1,
    'test', '["test", "digital"]'
);

-- 가상포인트 5 입력 매핑
INSERT INTO virtual_point_inputs (virtual_point_id, variable_name, source_type, source_id) VALUES 
(5, 'emergency', 'data_point', 5);  -- Emergency_Stop (실제 존재)

-- =============================================================================
-- 검증 쿼리
-- =============================================================================

-- 생성된 가상포인트 확인
SELECT 'Created Virtual Points:' as info;
SELECT id, name, description, is_enabled FROM virtual_points WHERE id IN (1,2,3,4,5);

SELECT '' as blank;
SELECT 'Input Mappings:' as info;
SELECT vp.name, vpi.variable_name, vpi.source_type, vpi.source_id 
FROM virtual_points vp 
JOIN virtual_point_inputs vpi ON vp.id = vpi.virtual_point_id 
WHERE vp.id IN (1,2,3,4,5)
ORDER BY vp.id, vpi.variable_name;

COMMIT;

-- 사용법:
-- sqlite3 db/pulseone_test.db < fix_virtual_points.sql