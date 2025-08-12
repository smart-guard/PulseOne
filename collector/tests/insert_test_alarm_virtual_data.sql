-- =============================================================================
-- insert_test_alarm_virtual_data.sql
-- PulseOne 알람 시스템 및 가상포인트 테스트 데이터 삽입
-- =============================================================================

BEGIN TRANSACTION;

-- 기존 테스트 데이터 정리 (선택적)
DELETE FROM alarm_occurrences;
DELETE FROM alarm_rules WHERE name LIKE 'TEST_%';
DELETE FROM virtual_point_values WHERE virtual_point_id IN (SELECT id FROM virtual_points WHERE name LIKE 'TEST_%');
DELETE FROM virtual_points WHERE name LIKE 'TEST_%';

-- =============================================================================
-- 🚨 알람 규칙 테스트 데이터
-- =============================================================================

-- 1. 온도 알람 규칙 (PLC Temperature - 포인트 ID: 4)
INSERT INTO alarm_rules (
    tenant_id, name, description, 
    target_type, target_id, alarm_type,
    high_limit, high_high_limit, low_limit, low_low_limit, deadband,
    severity, priority, is_enabled,
    message_template, auto_clear, notification_enabled
) VALUES (
    1, 'TEST_PLC_Temperature_Alarm', 'PLC 온도 모니터링 알람 (테스트용)',
    'data_point', 4, 'analog',
    35.0, 40.0, 15.0, 10.0, 2.0,
    'high', 100, 1,
    '온도 알람: {value}°C (임계값: {limit}°C)', 1, 1
);

-- 2. 모터 전류 알람 규칙 (Motor Current - 포인트 ID: 3)  
INSERT INTO alarm_rules (
    tenant_id, name, description,
    target_type, target_id, alarm_type,
    high_limit, high_high_limit, deadband,
    severity, priority, is_enabled,
    message_template, auto_clear, notification_enabled
) VALUES (
    1, 'TEST_Motor_Current_Alarm', '모터 전류 과부하 알람 (테스트용)',
    'data_point', 3, 'analog', 
    30.0, 35.0, 1.0,
    'critical', 90, 1,
    '모터 과부하: {value}A (한계: {limit}A)', 1, 1
);

-- 3. 비상정지 디지털 알람 (Emergency Stop - 포인트 ID: 5)
INSERT INTO alarm_rules (
    tenant_id, name, description,
    target_type, target_id, alarm_type,
    trigger_condition, severity, priority, is_enabled,
    message_template, auto_clear, notification_enabled
) VALUES (
    1, 'TEST_Emergency_Stop_Alarm', '비상정지 버튼 활성화 알람 (테스트용)',
    'data_point', 5, 'digital',
    'on_true', 'critical', 95, 1,
    '🚨 비상정지 활성화됨!', 0, 1
);

-- 4. Zone1 온도 알람 (RTU Zone1 Temperature - 포인트 ID: 13)
INSERT INTO alarm_rules (
    tenant_id, name, description,
    target_type, target_id, alarm_type,
    high_limit, low_limit, deadband,
    severity, priority, is_enabled,
    message_template, auto_clear, notification_enabled
) VALUES (
    1, 'TEST_Zone1_Temperature_Alarm', 'RTU 구역1 온도 알람 (테스트용)',
    'data_point', 13, 'analog',
    28.0, 18.0, 1.5,
    'medium', 80, 1,
    'Zone1 온도 이상: {value}°C', 1, 1
);

-- 5. 복합 조건 스크립트 알람 (모터 전류 + 온도)
INSERT INTO alarm_rules (
    tenant_id, name, description,
    target_type, target_id, alarm_type,
    condition_script, message_script,
    severity, priority, is_enabled,
    message_template, auto_clear, notification_enabled
) VALUES (
    1, 'TEST_Motor_Overload_Script', '모터 과부하 복합 조건 알람 (테스트용)',
    'data_point', 3, 'script',
    'var current = getPointValue(3); var temp = getPointValue(4); return current > 25 && temp > 30;',
    'return "모터 과부하 위험! 전류: " + getPointValue(3) + "A, 온도: " + getPointValue(4) + "°C";',
    'high', 85, 1,
    '복합 알람: {script_message}', 1, 1
);

-- =============================================================================
-- 🧮 가상포인트 테스트 데이터  
-- =============================================================================

-- 1. RTU 평균 온도 가상포인트 (Zone1, Zone2, Ambient 평균)
INSERT INTO virtual_points (
    tenant_id, scope_type, site_id, device_id, 
    name, description, formula, data_type, unit,
    calculation_trigger, calculation_interval, is_enabled,
    category, tags
) VALUES (
    1, 'device', 1, 12,
    'TEST_Average_Zone_Temperature', 'RTU 구역 평균 온도 (테스트용)',
    'var z1 = getPointValue(13); var z2 = getPointValue(14); var amb = getPointValue(15); if (z1 != null && z2 != null && amb != null) { return (z1 + z2 + amb) / 3; } else { return null; }',
    'float', '°C',
    'onchange', 5000, 1,
    'temperature', '["test", "average", "temperature"]'
);

-- 2. PLC 모터 효율 가상포인트 (전류/속도 비율)
INSERT INTO virtual_points (
    tenant_id, scope_type, site_id, device_id,
    name, description, formula, data_type, unit,
    calculation_trigger, calculation_interval, is_enabled,
    category, tags
) VALUES (
    1, 'device', 1, 1,
    'TEST_Motor_Efficiency', 'PLC 모터 효율 지수 (테스트용)', 
    'var speed = getPointValue(2); var current = getPointValue(3); if (speed != null && current != null && speed > 0) { return current / speed; } else { return 0; }',
    'float', 'A/(m/min)',
    'onchange', 3000, 1,
    'efficiency', '["test", "motor", "efficiency"]'
);

-- 3. PLC 전력 소비 가상포인트 (전류 * 가정 전압)
INSERT INTO virtual_points (
    tenant_id, scope_type, site_id, device_id,
    name, description, formula, data_type, unit,
    calculation_trigger, calculation_interval, is_enabled,
    category, tags  
) VALUES (
    1, 'device', 1, 1,
    'TEST_Power_Consumption', 'PLC 예상 전력 소비 (테스트용)',
    'var current = getPointValue(3); if (current != null) { return current * 220; } else { return 0; }',
    'float', 'W',
    'onchange', 2000, 1,
    'power', '["test", "power", "consumption"]'
);

-- 4. HVAC 유량 효율 가상포인트 (유량/압력 비율)
INSERT INTO virtual_points (
    tenant_id, scope_type, site_id, device_id,
    name, description, formula, data_type, unit,
    calculation_trigger, calculation_interval, is_enabled,
    category, tags
) VALUES (
    1, 'device', 1, 3,
    'TEST_Flow_Efficiency', 'HVAC 유량 효율 (테스트용)',
    'var flow = getPointValue(9); var pressure = getPointValue(8); if (flow != null && pressure != null && pressure > 0) { return flow / pressure; } else { return 0; }',
    'float', '(L/min)/bar',
    'onchange', 4000, 1,
    'efficiency', '["test", "hvac", "flow"]'
);

-- 5. 온도 차이 가상포인트 (최대-최소 온도)
INSERT INTO virtual_points (
    tenant_id, scope_type, site_id, device_id,
    name, description, formula, data_type, unit,
    calculation_trigger, calculation_interval, is_enabled,
    category, tags
) VALUES (
    1, 'device', 1, 12,
    'TEST_Temperature_Variance', 'RTU 온도 편차 (테스트용)',
    'var z1 = getPointValue(13); var z2 = getPointValue(14); var amb = getPointValue(15); if (z1 != null && z2 != null && amb != null) { var max = Math.max(z1, z2, amb); var min = Math.min(z1, z2, amb); return max - min; } else { return null; }',
    'float', '°C',
    'onchange', 6000, 1,
    'analysis', '["test", "variance", "temperature"]'
);

-- =============================================================================
-- 🧪 테스트 현재값 데이터 (알람 발생용)
-- =============================================================================

-- PLC 온도를 알람 임계값 초과로 설정 (35°C 초과 → 37°C)
INSERT OR REPLACE INTO current_values (
    point_id, current_value, value_type, quality_code, quality,
    value_timestamp, last_read_time, updated_at
) VALUES (
    4, '{"value": 37.5}', 'double', 0, 'good',
    datetime('now'), datetime('now'), datetime('now')
);

-- 모터 전류를 정상 범위로 설정 (25A)
INSERT OR REPLACE INTO current_values (
    point_id, current_value, value_type, quality_code, quality,
    value_timestamp, last_read_time, updated_at
) VALUES (
    3, '{"value": 25.0}', 'double', 0, 'good',
    datetime('now'), datetime('now'), datetime('now')
);

-- 라인 속도 설정 (10 m/min)
INSERT OR REPLACE INTO current_values (
    point_id, current_value, value_type, quality_code, quality,
    value_timestamp, last_read_time, updated_at
) VALUES (
    2, '{"value": 10.0}', 'double', 0, 'good',
    datetime('now'), datetime('now'), datetime('now')
);

-- 비상정지를 비활성화로 설정 (false)
INSERT OR REPLACE INTO current_values (
    point_id, current_value, value_type, quality_code, quality,
    value_timestamp, last_read_time, updated_at
) VALUES (
    5, '{"value": 0}', 'double', 0, 'good',
    datetime('now'), datetime('now'), datetime('now')
);

-- RTU Zone1 온도를 정상 범위로 설정 (25°C)
INSERT OR REPLACE INTO current_values (
    point_id, current_value, value_type, quality_code, quality,
    value_timestamp, last_read_time, updated_at
) VALUES (
    13, '{"value": 25.0}', 'double', 0, 'good',
    datetime('now'), datetime('now'), datetime('now')
);

-- RTU Zone2 온도 설정 (26°C)
INSERT OR REPLACE INTO current_values (
    point_id, current_value, value_type, quality_code, quality,
    value_timestamp, last_read_time, updated_at
) VALUES (
    14, '{"value": 26.0}', 'double', 0, 'good',
    datetime('now'), datetime('now'), datetime('now')
);

-- RTU Ambient 온도 설정 (24°C)
INSERT OR REPLACE INTO current_values (
    point_id, current_value, value_type, quality_code, quality,
    value_timestamp, last_read_time, updated_at
) VALUES (
    15, '{"value": 24.0}', 'double', 0, 'good',
    datetime('now'), datetime('now'), datetime('now')
);

-- HVAC 펌프 압력 설정 (5.0 bar)
INSERT OR REPLACE INTO current_values (
    point_id, current_value, value_type, quality_code, quality,
    value_timestamp, last_read_time, updated_at
) VALUES (
    8, '{"value": 5.0}', 'double', 0, 'good',
    datetime('now'), datetime('now'), datetime('now')
);

-- HVAC 유량 설정 (15.0 L/min)
INSERT OR REPLACE INTO current_values (
    point_id, current_value, value_type, quality_code, quality,
    value_timestamp, last_read_time, updated_at
) VALUES (
    9, '{"value": 15.0}', 'double', 0, 'good',
    datetime('now'), datetime('now'), datetime('now')
);

-- =============================================================================
-- 📊 확인 쿼리들 (테스트 완료 후 실행)
-- =============================================================================

-- 생성된 알람 규칙 확인
-- SELECT id, name, target_type, target_id, alarm_type, severity FROM alarm_rules WHERE name LIKE 'TEST_%';

-- 생성된 가상포인트 확인  
-- SELECT id, name, device_id, formula, category FROM virtual_points WHERE name LIKE 'TEST_%';

-- 현재값 확인
-- SELECT cv.point_id, dp.name, cv.current_value, cv.quality FROM current_values cv JOIN data_points dp ON cv.point_id = dp.id WHERE cv.point_id IN (2,3,4,5,8,9,13,14,15);

-- 알람 발생 예상 포인트 (온도 37.5°C > 35°C 임계값)
-- SELECT ar.name, ar.high_limit, cv.current_value FROM alarm_rules ar JOIN current_values cv ON ar.target_id = cv.point_id WHERE ar.target_id = 4;

COMMIT;