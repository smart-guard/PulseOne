-- =============================================================================
-- backend/lib/database/schemas/08-initial-data.sql
-- 초기 데이터 및 샘플 데이터 (SQLite 버전) - 2025-08-21 최신 업데이트
-- PulseOne v2.1.0 완전 호환, 현재 DB 스키마와 100% 일치
-- =============================================================================

-- 스키마 버전 기록
INSERT OR IGNORE INTO schema_versions (version, description) 
VALUES ('2.1.0', 'Complete PulseOne v2.1.0 schema with updated alarm system');

-- =============================================================================
-- 테넌트 생성
-- =============================================================================
INSERT OR IGNORE INTO tenants (
    company_name, company_code, domain, 
    contact_name, contact_email, contact_phone,
    subscription_plan, subscription_status,
    max_edge_servers, max_data_points, max_users,
    is_active
) VALUES 
('Smart Factory Korea', 'SFK001', 'smartfactory.pulseone.io', 
 'Factory Manager', 'manager@smartfactory.co.kr', '+82-2-1234-5678',
 'professional', 'active', 10, 10000, 50, 1),

('Global Manufacturing Inc', 'GMI002', 'global-mfg.pulseone.io',
 'Operations Director', 'ops@globalmfg.com', '+1-555-0123',
 'enterprise', 'active', 50, 100000, 200, 1),

('Demo Corporation', 'DEMO', 'demo.pulseone.io', 
 'Demo Manager', 'demo@pulseone.com', '+82-10-0000-0000',
 'starter', 'trial', 3, 1000, 10, 1),

('Test Factory Ltd', 'TEST', 'test.pulseone.io',
 'Test Engineer', 'test@testfactory.com', '+82-31-9999-8888', 
 'professional', 'active', 5, 5000, 25, 1);

-- =============================================================================
-- 사이트 생성
-- =============================================================================
INSERT OR IGNORE INTO sites (
    tenant_id, name, location, description, is_active
) VALUES 
(1, 'Seoul Main Factory', 'Seoul Industrial Complex', 'Main manufacturing facility', 1),
(1, 'Busan Secondary Plant', 'Busan Industrial Park', 'Secondary production facility', 1),
(2, 'New York Plant', 'New York Industrial Zone', 'East Coast Manufacturing Plant', 1),
(2, 'Detroit Automotive Plant', 'Detroit, MI', 'Automotive Manufacturing Plant', 1),
(3, 'Demo Factory', 'Demo Location', 'Demonstration facility', 1),
(4, 'Test Facility', 'Test Location', 'Testing and R&D facility', 1);

-- =============================================================================
-- 디바이스 생성 (현재 스키마에 맞춤)
-- =============================================================================
INSERT OR IGNORE INTO devices (
    tenant_id, site_id, name, description, device_type, manufacturer, model,
    protocol_type, endpoint, config, polling_interval, timeout, retry_count,
    is_enabled
) VALUES 
-- Smart Factory Korea 디바이스들
(1, 1, 'PLC-001', 'Main production line PLC', 'PLC', 'Siemens', 'S7-1515F',
 'MODBUS_TCP', '192.168.1.10:502', 
 '{"slave_id": 1, "timeout_ms": 3000, "byte_order": "big_endian"}',
 1000, 3000, 3, 1),

(1, 1, 'HMI-001', 'Operator HMI panel', 'HMI', 'Schneider Electric', 'Harmony GT2512',
 'MODBUS_TCP', '192.168.1.11:502',
 '{"slave_id": 2, "timeout_ms": 3000, "screen_size": "12_inch"}',
 2000, 3000, 3, 1),

(1, 1, 'ROBOT-001', 'Automated welding robot', 'ROBOT', 'KUKA', 'KR 16-3 F',
 'MODBUS_TCP', '192.168.1.12:502',
 '{"slave_id": 3, "timeout_ms": 2000, "coordinate_system": "world"}',
 500, 2000, 5, 1),

(1, 1, 'VFD-001', 'Conveyor motor VFD', 'INVERTER', 'ABB', 'ACS580-01',
 'MODBUS_TCP', '192.168.1.13:502',
 '{"slave_id": 4, "timeout_ms": 3000, "motor_rated_power": "15kW"}',
 1000, 3000, 3, 1),

(1, 1, 'HVAC-001', 'Main air handling unit', 'CONTROLLER', 'Honeywell', 'Spyder',
 'BACNET', '192.168.1.30:47808',
 '{"device_id": 1001, "network": 1, "max_apdu": 1476}',
 5000, 10000, 3, 1),

(1, 1, 'METER-001', 'Main facility power meter', 'METER', 'Schneider Electric', 'PowerLogic PM8000',
 'MODBUS_TCP', '192.168.1.40:502',
 '{"slave_id": 10, "timeout_ms": 3000, "measurement_class": "0.2S"}',
 5000, 3000, 3, 1),

-- Global Manufacturing 디바이스들
(2, 3, 'PLC-NY-001', 'New York plant main PLC', 'PLC', 'Rockwell Automation', 'CompactLogix 5380',
 'MODBUS_TCP', '10.0.1.10:502',
 '{"slave_id": 1, "timeout_ms": 3000, "communication_format": "RTU_over_TCP"}',
 1000, 3000, 3, 1),

(2, 3, 'ROBOT-NY-001', 'Assembly robot station 1', 'ROBOT', 'Fanuc', 'R-2000iD/210F',
 'ETHERNET_IP', '10.0.1.15:44818',
 '{"connection_type": "explicit", "assembly_instance": 100}',
 200, 1000, 5, 1),

-- Demo/Test 디바이스들
(3, 5, 'DEMO-PLC-001', 'Demo PLC for training', 'PLC', 'Demo Manufacturer', 'Demo-PLC-v2',
 'MODBUS_TCP', '192.168.100.10:502',
 '{"slave_id": 1, "timeout_ms": 3000, "demo_mode": true}',
 2000, 3000, 3, 1),

(3, 5, 'DEMO-IOT-001', 'IoT gateway for wireless sensors', 'GATEWAY', 'Generic IoT', 'MultiProtocol-GW',
 'MQTT', 'mqtt://192.168.100.20:1883',
 '{"client_id": "demo_gateway", "username": "demo_user", "keep_alive": 60}',
 0, 5000, 3, 1),

(4, 6, 'TEST-PLC-001', 'Advanced test PLC for R&D', 'PLC', 'Test Systems', 'TestPLC-Pro',
 'MODBUS_TCP', '192.168.200.10:502',
 '{"slave_id": 1, "timeout_ms": 3000, "test_mode": true}',
 1000, 3000, 3, 1);

-- =============================================================================
-- 데이터 포인트 생성 (현재 스키마에 맞춤)
-- =============================================================================
INSERT OR IGNORE INTO data_points (
    device_id, name, description, address, data_type, access_mode, unit,
    scaling_factor, scaling_offset, min_value, max_value, is_enabled
) VALUES 
-- PLC-001 데이터 포인트들
(1, 'Production_Count', 'Production counter', 1001, 'uint32', 'read', 'pcs',
 1.0, 0.0, 0.0, 999999.0, 1),
(1, 'Line_Speed', 'Line speed sensor', 1002, 'float', 'read', 'm/min',
 0.1, 0.0, 0.0, 100.0, 1),
(1, 'Motor_Current', 'Motor current sensor', 1003, 'float', 'read', 'A',
 0.01, 0.0, 0.0, 50.0, 1),
(1, 'Temperature', 'Process temperature', 1004, 'float', 'read', '°C',
 0.1, 0.0, -40.0, 150.0, 1),
(1, 'Emergency_Stop', 'Emergency stop button', 1005, 'bool', 'read', '',
 1.0, 0.0, 0.0, 1.0, 1),

-- HMI-001 데이터 포인트들
(2, 'Screen_Status', 'HMI screen status', 2001, 'uint16', 'read', '',
 1.0, 0.0, 0.0, 255.0, 1),
(2, 'Active_Alarms', 'Number of active alarms', 2002, 'uint16', 'read', 'count',
 1.0, 0.0, 0.0, 100.0, 1),
(2, 'User_Level', 'Current user access level', 2003, 'uint16', 'read', '',
 1.0, 0.0, 0.0, 5.0, 1),

-- ROBOT-001 데이터 포인트들
(3, 'Robot_X_Position', 'Robot X position', 3001, 'float', 'read', 'mm',
 0.01, 0.0, -1611.0, 1611.0, 1),
(3, 'Robot_Y_Position', 'Robot Y position', 3003, 'float', 'read', 'mm',
 0.01, 0.0, -1611.0, 1611.0, 1),
(3, 'Robot_Z_Position', 'Robot Z position', 3005, 'float', 'read', 'mm',
 0.01, 0.0, -1000.0, 2000.0, 1),
(3, 'Welding_Current', 'Welding current', 3007, 'float', 'read', 'A',
 0.1, 0.0, 0.0, 350.0, 1),

-- HVAC-001 데이터 포인트들
(5, 'Zone1_Temperature', 'Production Zone 1 Temperature', 1, 'FLOAT32', 'read', '°C',
 1.0, 0.0, -10.0, 50.0, 1),
(5, 'Zone1_Humidity', 'Production Zone 1 Humidity', 2, 'FLOAT32', 'read', '%RH',
 1.0, 0.0, 0.0, 100.0, 1);

-- =============================================================================
-- 현재값 초기화
-- =============================================================================
INSERT OR IGNORE INTO current_values (
    point_id, value, raw_value, string_value, quality, timestamp, updated_at
) VALUES 
-- PLC-001 현재값들
(1, 15847.0, 15847.0, NULL, 'good', datetime('now', '-5 minutes'), datetime('now')),
(2, 18.5, 185.0, NULL, 'good', datetime('now', '-1 minute'), datetime('now')),
(3, 28.7, 2870.0, NULL, 'good', datetime('now', '-30 seconds'), datetime('now')),
(4, 23.8, 238.0, NULL, 'good', datetime('now', '-2 minutes'), datetime('now')),
(5, 0.0, 0.0, 'false', 'good', datetime('now', '-10 seconds'), datetime('now')),

-- HMI-001 현재값들
(6, 1.0, 1.0, NULL, 'good', datetime('now', '-30 seconds'), datetime('now')),
(7, 2.0, 2.0, NULL, 'good', datetime('now', '-15 seconds'), datetime('now')),
(8, 2.0, 2.0, NULL, 'good', datetime('now', '-45 seconds'), datetime('now')),

-- ROBOT-001 현재값들
(9, 145.67, 14567.0, NULL, 'good', datetime('now', '-5 seconds'), datetime('now')),
(10, -287.23, -28723.0, NULL, 'good', datetime('now', '-5 seconds'), datetime('now')),
(11, 856.45, 85645.0, NULL, 'good', datetime('now', '-5 seconds'), datetime('now')),
(12, 185.4, 1854.0, NULL, 'good', datetime('now', '-2 seconds'), datetime('now')),

-- HVAC-001 현재값들
(13, 22.3, 22.3, NULL, 'good', datetime('now', '-3 minutes'), datetime('now')),
(14, 58.2, 58.2, NULL, 'good', datetime('now', '-2 minutes'), datetime('now'));

-- =============================================================================
-- 가상포인트 생성 (현재 스키마에 맞춘 간단한 구조)
-- =============================================================================
INSERT OR IGNORE INTO virtual_points (
    tenant_id, scope_type, site_id, name, description, formula, data_type, unit,
    calculation_interval, calculation_trigger, is_enabled
) VALUES 
(1, 'site', 1, 'Production_Efficiency', 'Overall production efficiency calculation', 
 'const production = getValue("Production_Count"); return (production / 20000) * 100;', 
 'float', '%', 5000, 'timer', 1),

(1, 'site', 1, 'Energy_Per_Unit', 'Energy consumption per unit', 
 'const power = 2847.5; const production = getValue("Production_Count"); return power / production;', 
 'float', 'kW/unit', 10000, 'timer', 1),

(2, 'site', 3, 'Assembly_Throughput', 'Assembly line throughput', 
 'const cycleTime = 45; return 3600 / cycleTime;', 
 'float', 'units/hour', 5000, 'timer', 1),

(3, 'site', 5, 'Demo_Performance', 'Demo performance index', 
 'return Math.sin(Date.now() / 10000) * 50 + 75;', 
 'float', '%', 2000, 'timer', 1),

(4, 'site', 6, 'Test_Metric', 'Test calculation metric', 
 'return Math.random() * 100;', 
 'float', '%', 3000, 'timer', 1);

-- =============================================================================
-- 알람 규칙 생성 (현재 스키마 구조에 맞춤)
-- =============================================================================
INSERT OR IGNORE INTO alarm_rules (
    tenant_id, name, description, target_type, target_id, alarm_type, severity,
    high_limit, low_limit, deadband, message_template, notification_enabled,
    is_enabled, escalation_enabled, escalation_max_level
) VALUES 
-- Smart Factory Korea 알람들
(1, 'TEST_PLC_Temperature_Alarm', 'PLC 온도 모니터링 알람 (테스트용)', 'data_point', 4, 'analog', 'high',
 35.0, 15.0, 2.0, '온도 알람: {value}°C (임계값: {limit}°C)', 1, 1, 0, 3),

(1, 'TEST_Motor_Current_Alarm', '모터 전류 과부하 알람 (테스트용)', 'data_point', 3, 'analog', 'critical',
 30.0, NULL, 1.0, '모터 과부하: {value}A (한계: {limit}A)', 1, 1, 0, 3),

(1, 'TEST_Emergency_Stop_Alarm', '비상정지 버튼 활성화 알람 (테스트용)', 'data_point', 5, 'digital', 'critical',
 NULL, NULL, 0.0, '🚨 비상정지 활성화됨!', 1, 1, 0, 3),

(1, 'TEST_Zone1_Temperature_Alarm', 'RTU 구역1 온도 알람 (테스트용)', 'data_point', 13, 'analog', 'medium',
 28.0, 18.0, 1.5, 'Zone1 온도 이상: {value}°C', 1, 1, 0, 3),

-- 추가 테스트 알람들
(1, 'High Temperature Alert', 'Temperature exceeds 80 degrees', 'data_point', 1, 'analog', 'high',
 80.0, NULL, 2.0, 'Temperature alarm: {{value}}°C > {{limit}}°C', 1, 1, 0, 3),

(1, 'Test Temperature Alarm FIXED', '', 'data_point', 999, 'analog', 'high',
 85.0, NULL, 0.0, 'Test Temperature Alarm FIXED alarm triggered', 1, 1, 0, 3),

(1, '고온 경고 표준_1', '온도 센서용 고온 임계값 경고 (자동 생성)', 'data_point', 1, 'analog', 'high',
 85.0, NULL, 2.0, '{device_name} {point_name}이 {threshold}°C를 초과했습니다 (현재: {value}°C)', 1, 1, 0, 3),

(1, '고온 경고 표준_2', '온도 센서용 고온 임계값 경고 (자동 생성)', 'data_point', 2, 'analog', 'high',
 80.0, NULL, 2.0, '{device_name} {point_name}이 {threshold}°C를 초과했습니다 (현재: {value}°C)', 1, 1, 0, 3),

(1, '고온 경고 표준_3', '온도 센서용 고온 임계값 경고 (자동 생성)', 'data_point', 3, 'analog', 'high',
 90.0, NULL, 2.0, '{device_name} {point_name}이 {threshold}°C를 초과했습니다 (현재: {value}°C)', 1, 1, 0, 3),

(1, '테스트 온도 알람', '테스트용 온도 임계값 알람', 'data_point', 1, 'analog', 'medium',
 85.0, 10.0, 2.5, '온도 알람: {value}°C', 1, 1, 0, 3);

-- =============================================================================
-- 알람 규칙 템플릿 생성
-- =============================================================================
INSERT OR IGNORE INTO alarm_rule_templates (
    tenant_id, name, description, category, condition_type, condition_template,
    default_config, severity, message_template, applicable_data_types,
    notification_enabled, is_active, is_system_template
) VALUES 
(1, '온도 센서 고온 알람', '온도 센서용 고온 임계값 알람 템플릿', 'temperature',
 'threshold', '> {high_limit}°C', 
 '{"high_limit": 80, "deadband": 2}', 'high',
 '{device_name} 온도가 {value}°C로 임계값 {threshold}°C를 초과했습니다',
 '["temperature", "float", "analog"]', 1, 1, 0),

(1, '전류 과부하 알람', '전류 센서용 과부하 알람 템플릿', 'electrical',
 'threshold', '> {high_limit}A',
 '{"high_limit": 30, "deadband": 1}', 'critical',
 '{device_name} 전류가 {value}A로 과부하 상태입니다',
 '["current", "float", "analog"]', 1, 1, 0),

(1, '디지털 입력 알람', '디지털 입력 상태 변화 알람 템플릿', 'digital',
 'boolean', 'on_true',
 '{"trigger_condition": "on_true"}', 'high',
 '{device_name} {point_name} 활성화됨',
 '["bool", "digital", "binary"]', 1, 1, 0);

-- =============================================================================
-- JavaScript 함수 라이브러리
-- =============================================================================
INSERT OR IGNORE INTO javascript_functions (
    tenant_id, name, description, category, function_code, parameters, return_type, is_system
) VALUES 
(0, 'average', 'Calculate average of values', 'math',
 'function average(...values) { return values.reduce((a, b) => a + b, 0) / values.length; }',
 '[{"name": "values", "type": "number[]", "required": true}]', 'number', 1),

(0, 'oeeCalculation', 'Calculate Overall Equipment Effectiveness', 'engineering',
 'function oeeCalculation(availability, performance, quality) { return (availability / 100) * (performance / 100) * (quality / 100) * 100; }',
 '[{"name": "availability", "type": "number"}, {"name": "performance", "type": "number"}, {"name": "quality", "type": "number"}]', 'number', 1),

(1, 'productionEfficiency', 'Calculate production efficiency for automotive line', 'custom',
 'function productionEfficiency(actual, target, hours) { return (actual / target) * 100; }',
 '[{"name": "actual", "type": "number"}, {"name": "target", "type": "number"}, {"name": "hours", "type": "number"}]', 'number', 0);

-- =============================================================================
-- 초기 데이터 로딩 완료 로그
-- =============================================================================
INSERT OR IGNORE INTO system_logs (
    log_level, module, message, details, created_at
) VALUES 
('INFO', 'database', 'Updated initial data loading completed successfully', 
 '{"tables_populated": 12, "devices": 11, "data_points": 14, "virtual_points": 5, "alarm_rules": 10, "templates": 3}',
 datetime('now'));

-- =============================================================================
-- 초기 데이터 로딩 완료
-- =============================================================================
-- 이 업데이트된 파일은 현재 데이터베이스 스키마와 완전히 일치합니다:
-- ✅ target_type, target_id 구조 사용
-- ✅ escalation_enabled, escalation_max_level 컬럼 포함
-- ✅ 실제 존재하는 테이블들만 사용
-- ✅ 백엔드 API와 호환되는 데이터 구조