-- =============================================================================
-- backend/lib/database/data/initial-data-NEW.sql
-- 새 스키마 완전 호환 초기 데이터 (Struct DataPoint 100% 반영)
-- =============================================================================

-- 스키마 버전 기록
INSERT OR IGNORE INTO schema_versions (version, description) 
VALUES ('2.1.0', 'Complete DataPoint struct alignment with JSON protocol params');

-- =============================================================================
-- 데모 테넌트 생성
-- =============================================================================
INSERT OR IGNORE INTO tenants (
    company_name, company_code, domain, 
    contact_name, contact_email, 
    subscription_plan, subscription_status,
    max_edge_servers, max_data_points, max_users
) VALUES 
('Demo Corporation', 'demo', 'demo.pulseone.com', 
 'Demo Manager', 'demo@pulseone.com',
 'professional', 'active', 5, 5000, 20),
('Test Factory', 'test', 'test.pulseone.com',
 'Test Manager', 'test@pulseone.com', 
 'starter', 'trial', 3, 1000, 5),
('Global Industries', 'global', 'global.pulseone.com',
 'Global Manager', 'global@pulseone.com',
 'enterprise', 'active', 10, 10000, 50);

-- =============================================================================
-- 데모 Edge 서버 등록
-- =============================================================================
INSERT OR IGNORE INTO edge_servers (
    tenant_id, server_name, factory_name, location,
    ip_address, hostname, status, version,
    registration_token, activation_code
) VALUES 
(1, 'Edge-Demo-01', 'Seoul Main Factory', 'Building A - Floor 1',
 '192.168.1.100', 'edge-demo-01', 'active', '1.0.0',
 'demo-token-001', '123456'),
(1, 'Edge-Demo-02', 'Seoul Main Factory', 'Building A - Floor 2', 
 '192.168.1.101', 'edge-demo-02', 'active', '1.0.0',
 'demo-token-002', '234567'),
(1, 'Edge-Demo-03', 'Busan Secondary Plant', 'Main Workshop',
 '192.168.2.100', 'edge-demo-03', 'active', '1.0.0',
 'demo-token-003', '345678'),
(2, 'Edge-Test-01', 'Test Facility', 'Test Building',
 '192.168.3.100', 'edge-test-01', 'pending', '1.0.0',
 'test-token-001', '456789'),
(3, 'Edge-Global-01', 'Global Factory', 'Production Hall',
 '192.168.4.100', 'edge-global-01', 'active', '1.0.0',
 'global-token-001', '567890');

-- =============================================================================
-- 통합 사용자 생성
-- =============================================================================

-- 시스템 관리자
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, permissions
) VALUES 
(NULL, 'sysadmin', 'admin@pulseone.com', '$2b$12$SystemAdminPassword123456789012345678901', 'System Administrator', 'system_admin',
 '["manage_all_tenants", "manage_system_settings", "view_all_data"]'),
(NULL, 'devadmin', 'dev@pulseone.com', '$2b$12$DevAdminPassword123456789012345678901234', 'Development Admin', 'system_admin',
 '["manage_all_tenants", "view_all_data", "system_debug"]');

-- Demo Corporation 사용자들
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, department, permissions
) VALUES 
(1, 'demo_admin', 'admin@demo.pulseone.com', '$2b$12$DemoAdminPassword123456789012345678901', 'Demo Admin', 'company_admin', 'IT',
 '["manage_company", "manage_all_sites", "manage_users", "view_all_data"]'),
(1, 'demo_engineer', 'engineer@demo.pulseone.com', '$2b$12$DemoEngineerPassword12345678901234567890', 'Demo Engineer', 'engineer', 'Engineering',
 '["manage_devices", "manage_data_points", "view_data", "manage_alarms"]'),
(1, 'demo_operator', 'operator@demo.pulseone.com', '$2b$12$DemoOperatorPassword12345678901234567890', 'Demo Operator', 'operator', 'Operations',
 '["view_data", "control_devices", "acknowledge_alarms"]');

-- Test Factory 사용자들
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, department, permissions
) VALUES 
(2, 'test_admin', 'admin@test.pulseone.com', '$2b$12$TestAdminPassword123456789012345678901', 'Test Admin', 'company_admin', 'IT',
 '["manage_company", "manage_sites", "manage_users"]'),
(2, 'test_engineer', 'engineer@test.pulseone.com', '$2b$12$TestEngineerPassword123456789012345678', 'Test Engineer', 'engineer', 'R&D',
 '["view_dashboard", "manage_devices", "create_virtual_points"]');

-- Global Industries 사용자
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, department, permissions
) VALUES 
(3, 'global_admin', 'admin@global.pulseone.com', '$2b$12$GlobalAdminPassword123456789012345678901', 'Global Admin', 'company_admin', 'IT',
 '["manage_company", "manage_sites", "manage_users", "view_all_data"]');

-- =============================================================================
-- 데모 사이트 생성 (간소화)
-- =============================================================================
INSERT OR IGNORE INTO sites (
    tenant_id, name, code, site_type, description, location, manager_name, manager_email
) VALUES 
-- Demo Corporation
(1, 'Demo Seoul Factory', 'seoul_factory', 'factory', 'Main manufacturing facility', 'Seoul Industrial Complex', 'Factory Manager', 'factory@demo.com'),
(1, 'Demo Busan Plant', 'busan_plant', 'factory', 'Secondary production plant', 'Busan Industrial Park', 'Plant Manager', 'plant@demo.com'),

-- Test Factory
(2, 'Test Facility', 'test_facility', 'factory', 'R&D and testing facility', 'Incheon Free Zone', 'Test Manager', 'test@test.com'),

-- Global Industries
(3, 'Global Factory NY', 'global_ny', 'factory', 'North America production facility', 'New York Industrial Zone', 'NY Manager', 'ny@global.com');

-- =============================================================================
-- 드라이버 플러그인 생성
-- =============================================================================
INSERT OR IGNORE INTO driver_plugins (
    name, protocol_type, version, file_path, description, author, is_enabled
) VALUES 
('Modbus TCP Driver', 'MODBUS_TCP', '1.0.0', '/drivers/modbus_tcp.so', 'Standard Modbus TCP/IP driver', 'PulseOne Team', 1),
('Modbus RTU Driver', 'MODBUS_RTU', '1.0.0', '/drivers/modbus_rtu.so', 'Modbus RTU serial driver', 'PulseOne Team', 1),
('MQTT Driver', 'MQTT', '1.0.0', '/drivers/mqtt.so', 'MQTT publish/subscribe driver', 'PulseOne Team', 1),
('BACnet Driver', 'BACNET', '1.0.0', '/drivers/bacnet.so', 'BACnet building automation driver', 'PulseOne Team', 1),
('OPC UA Driver', 'OPCUA', '1.0.0', '/drivers/opcua.so', 'OPC Unified Architecture driver', 'PulseOne Team', 0);

-- =============================================================================
-- 디바이스 그룹 생성 
-- =============================================================================
INSERT OR IGNORE INTO device_groups (tenant_id, site_id, name, description) VALUES 
(1, 1, 'Production Equipment', 'Manufacturing line equipment'),
(1, 1, 'Building Systems', 'HVAC, Energy, Safety systems'), 
(2, 3, 'Test Equipment', 'Testing and QA equipment'),
(3, 4, 'Global Production', 'Global production line equipment');

-- =============================================================================
-- 🔥🔥🔥 새 스키마 완전 호환 디바이스 생성
-- =============================================================================
INSERT OR IGNORE INTO devices (
    tenant_id, site_id, device_group_id, edge_server_id,
    name, description, device_type, manufacturer, model, serial_number,
    protocol_type, endpoint, config,
    is_enabled, created_by
) VALUES 
-- Demo Corporation - Seoul Factory
(1, 1, 1, 1, 'PLC-Auto-Line1', 'Automotive line main PLC', 'PLC', 'Siemens', 'S7-1500', 'S7-001', 
 'MODBUS_TCP', '192.168.1.10:502', '{"slave_id": 1, "timeout_ms": 3000}', 1, 3),
(1, 1, 1, 1, 'HMI-Auto-Line1', 'Automotive line HMI', 'HMI', 'Schneider', 'XBTGT7340', 'HMI-001', 
 'MODBUS_TCP', '192.168.1.11:502', '{"slave_id": 2, "timeout_ms": 3000}', 1, 3),
(1, 1, 1, 2, 'Robot-Welder', 'Welding robot controller', 'ROBOT', 'KUKA', 'KR-16', 'KUKA-001', 
 'MODBUS_TCP', '192.168.1.12:502', '{"slave_id": 3, "timeout_ms": 2000}', 1, 3),
(1, 1, 2, 2, 'HVAC-Controller', 'Building HVAC system', 'CONTROLLER', 'Honeywell', 'BACnet-Ctrl', 'HON-001', 
 'BACNET', '192.168.1.30:47808', '{"device_id": 1001, "network": 1}', 1, 3),
(1, 1, 2, 2, 'Energy-Meter', 'Main energy meter', 'METER', 'Schneider', 'PowerLogic', 'PWR-001', 
 'MODBUS_TCP', '192.168.1.40:502', '{"slave_id": 10, "timeout_ms": 3000}', 1, 3),

-- Test Factory  
(2, 3, 3, 4, 'Test-PLC-Main', 'Test line PLC', 'PLC', 'Mitsubishi', 'FX5U', 'MIT-001', 
 'MODBUS_TCP', '192.168.3.10:502', '{"slave_id": 1, "timeout_ms": 3000}', 1, 5),
(2, 3, 3, 4, 'Test-IoT-Gateway', 'IoT sensor gateway', 'GATEWAY', 'Generic', 'MQTT-GW', 'MQTT-001', 
 'MQTT', 'mqtt://192.168.3.20:1883', '{"client_id": "test_gateway", "username": "admin"}', 1, 5),

-- Global Industries
(3, 4, 4, 5, 'Global-PLC-Main', 'Global line main PLC', 'PLC', 'Rockwell', 'CompactLogix', 'RW-001', 
 'MODBUS_TCP', '192.168.4.10:502', '{"slave_id": 1, "timeout_ms": 3000}', 1, 6);

-- =============================================================================
-- 🔥🔥🔥 새 스키마 완전 호환 데이터 포인트 생성 (모든 필드 포함!)
-- =============================================================================

-- PLC-Auto-Line1 데이터 포인트들 (Modbus TCP)
INSERT OR IGNORE INTO data_points (
    device_id, name, description, 
    address, address_string,
    data_type, access_mode, is_enabled, is_writable,
    unit, scaling_factor, scaling_offset, min_value, max_value,
    log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
    group_name, tags, metadata, protocol_params
) VALUES 
(1, 'Production_Count', 'Automotive production count', 
 1001, '40001', 
 'uint32', 'read', 1, 0,
 'pcs', 1.0, 0.0, 0.0, 999999.0,
 1, 5000, 1.0, 1000,
 'production', '["production", "counter", "automotive"]', 
 '{"location": "line_1", "criticality": "high", "maintenance_schedule": "monthly"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 1, "byte_order": "big_endian", "word_order": "high_low"}'),

(1, 'Line_Speed', 'Automotive line speed', 
 1002, '40002', 
 'float', 'read', 1, 0,
 'm/min', 0.1, 0.0, 0.0, 100.0,
 1, 2000, 0.5, 1000,
 'production', '["speed", "automotive", "monitoring"]', 
 '{"location": "line_1", "criticality": "medium", "alarm_enabled": true}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 1, "byte_order": "big_endian", "register_type": "float32"}'),

(1, 'Motor_Current', 'Main motor current', 
 1003, '40003', 
 'float', 'read', 1, 0,
 'A', 0.01, 0.0, 0.0, 50.0,
 1, 1000, 0.1, 500,
 'electrical', '["current", "motor", "electrical"]', 
 '{"location": "motor_1", "criticality": "high", "safety_limit": 45.0}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 1, "byte_order": "big_endian", "register_type": "float32"}'),

(1, 'Temperature', 'Process temperature', 
 1004, '40004', 
 'float', 'read', 1, 0,
 '°C', 0.1, -273.15, -40.0, 150.0,
 1, 3000, 0.2, 1000,
 'environmental', '["temperature", "process", "monitoring"]', 
 '{"location": "process_area", "criticality": "medium", "calibration_date": "2025-01-01"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 1, "byte_order": "big_endian", "register_type": "float32"}'),

(1, 'Emergency_Stop', 'Emergency stop status', 
 1005, '10001', 
 'bool', 'read', 1, 0,
 '', 1.0, 0.0, 0.0, 1.0,
 1, 500, 0.0, 100,
 'safety', '["emergency", "safety", "critical"]', 
 '{"location": "control_panel", "criticality": "critical", "immediate_action": true}',
 '{"protocol": "MODBUS_TCP", "function_code": 1, "slave_id": 1, "byte_order": "big_endian"}'),

(1, 'Start_Button', 'Line start button', 
 1006, '10002', 
 'bool', 'read_write', 1, 1,
 '', 1.0, 0.0, 0.0, 1.0,
 1, 1000, 0.0, 500,
 'control', '["control", "start", "operator"]', 
 '{"location": "control_panel", "criticality": "medium", "operator_required": true}',
 '{"protocol": "MODBUS_TCP", "function_code": 1, "slave_id": 1, "byte_order": "big_endian", "write_function": 5}');

-- HMI-Auto-Line1 데이터 포인트들
INSERT OR IGNORE INTO data_points (
    device_id, name, description, 
    address, address_string,
    data_type, access_mode, is_enabled, is_writable,
    unit, scaling_factor, scaling_offset, min_value, max_value,
    log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
    group_name, tags, metadata, protocol_params
) VALUES 
(2, 'Operator_Screen', 'HMI operator screen status', 
 2001, '40001', 
 'uint16', 'read', 1, 0,
 '', 1.0, 0.0, 0.0, 255.0,
 0, 10000, 1.0, 2000,
 'hmi', '["hmi", "status", "operator"]', 
 '{"screen_id": "main", "criticality": "low"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 2, "byte_order": "big_endian"}'),

(2, 'Alarm_Count', 'Active alarm count', 
 2002, '40002', 
 'uint16', 'read', 1, 0,
 'count', 1.0, 0.0, 0.0, 100.0,
 1, 5000, 1.0, 1000,
 'alarms', '["alarms", "count", "monitoring"]', 
 '{"criticality": "medium", "auto_acknowledge": false}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 2, "byte_order": "big_endian"}');

-- Robot-Welder 데이터 포인트들
INSERT OR IGNORE INTO data_points (
    device_id, name, description, 
    address, address_string,
    data_type, access_mode, is_enabled, is_writable,
    unit, scaling_factor, scaling_offset, min_value, max_value,
    log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
    group_name, tags, metadata, protocol_params
) VALUES 
(3, 'Robot_Position_X', 'Robot X-axis position', 
 3001, '40001', 
 'float', 'read', 1, 0,
 'mm', 0.01, 0.0, -1000.0, 1000.0,
 1, 500, 1.0, 200,
 'robotics', '["position", "robot", "x_axis"]', 
 '{"axis": "X", "criticality": "medium", "coordinate_system": "world"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 3, "byte_order": "big_endian", "register_type": "float32"}'),

(3, 'Robot_Position_Y', 'Robot Y-axis position', 
 3002, '40003', 
 'float', 'read', 1, 0,
 'mm', 0.01, 0.0, -1000.0, 1000.0,
 1, 500, 1.0, 200,
 'robotics', '["position", "robot", "y_axis"]', 
 '{"axis": "Y", "criticality": "medium", "coordinate_system": "world"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 3, "byte_order": "big_endian", "register_type": "float32"}'),

(3, 'Welding_Current', 'Welding current', 
 3003, '40005', 
 'float', 'read', 1, 0,
 'A', 0.1, 0.0, 0.0, 300.0,
 1, 200, 5.0, 100,
 'welding', '["welding", "current", "process"]', 
 '{"process": "arc_welding", "criticality": "high", "safety_limit": 250.0}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 3, "byte_order": "big_endian", "register_type": "float32"}');

-- HVAC-Controller 데이터 포인트들 (BACnet)
INSERT OR IGNORE INTO data_points (
    device_id, name, description, 
    address, address_string,
    data_type, access_mode, is_enabled, is_writable,
    unit, scaling_factor, scaling_offset, min_value, max_value,
    log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
    group_name, tags, metadata, protocol_params
) VALUES 
(4, 'Zone_Temperature', 'HVAC zone temperature', 
 1, 'AI:1', 
 'float', 'read', 1, 0,
 '°C', 1.0, 0.0, 10.0, 40.0,
 1, 10000, 0.5, 5000,
 'hvac', '["hvac", "temperature", "zone"]', 
 '{"zone_id": "zone_1", "criticality": "medium", "comfort_range": "20-26"}',
 '{"protocol": "BACNET_IP", "object_type": 0, "object_instance": 1, "property_id": 85, "array_index": -1}'),

(4, 'Zone_Humidity', 'HVAC zone humidity', 
 2, 'AI:2', 
 'float', 'read', 1, 0,
 '%RH', 1.0, 0.0, 0.0, 100.0,
 1, 10000, 2.0, 5000,
 'hvac', '["hvac", "humidity", "zone"]', 
 '{"zone_id": "zone_1", "criticality": "low", "comfort_range": "40-60"}',
 '{"protocol": "BACNET_IP", "object_type": 0, "object_instance": 2, "property_id": 85, "array_index": -1}'),

(4, 'Damper_Position', 'Air damper position', 
 3, 'AO:1', 
 'float', 'read_write', 1, 1,
 '%', 1.0, 0.0, 0.0, 100.0,
 1, 5000, 1.0, 2000,
 'hvac', '["hvac", "damper", "control"]', 
 '{"zone_id": "zone_1", "criticality": "medium", "control_type": "automatic"}',
 '{"protocol": "BACNET_IP", "object_type": 1, "object_instance": 1, "property_id": 85, "array_index": -1, "priority": 16}');

-- Energy-Meter 데이터 포인트들
INSERT OR IGNORE INTO data_points (
    device_id, name, description, 
    address, address_string,
    data_type, access_mode, is_enabled, is_writable,
    unit, scaling_factor, scaling_offset, min_value, max_value,
    log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
    group_name, tags, metadata, protocol_params
) VALUES 
(5, 'Total_Energy', 'Total energy consumption', 
 5001, '40001', 
 'float', 'read', 1, 0,
 'kWh', 0.01, 0.0, 0.0, 999999.0,
 1, 10000, 1.0, 5000,
 'energy', '["energy", "total", "billing"]', 
 '{"meter_id": "main", "criticality": "high", "billing_purpose": true}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 10, "byte_order": "big_endian", "register_type": "float32"}'),

(5, 'Real_Power', 'Real power consumption', 
 5002, '40003', 
 'float', 'read', 1, 0,
 'kW', 0.1, 0.0, 0.0, 1000.0,
 1, 2000, 0.5, 1000,
 'energy', '["power", "real", "monitoring"]', 
 '{"meter_id": "main", "criticality": "medium", "demand_monitoring": true}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 10, "byte_order": "big_endian", "register_type": "float32"}'),

(5, 'Apparent_Power', 'Apparent power consumption', 
 5003, '40005', 
 'float', 'read', 1, 0,
 'kVA', 0.1, 0.0, 0.0, 1000.0,
 1, 2000, 0.5, 1000,
 'energy', '["power", "apparent", "monitoring"]', 
 '{"meter_id": "main", "criticality": "medium", "power_factor_calc": true}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 10, "byte_order": "big_endian", "register_type": "float32"}');

-- Test-IoT-Gateway 데이터 포인트들 (MQTT)
INSERT OR IGNORE INTO data_points (
    device_id, name, description, 
    address, address_string,
    data_type, access_mode, is_enabled, is_writable,
    unit, scaling_factor, scaling_offset, min_value, max_value,
    log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
    group_name, tags, metadata, protocol_params
) VALUES 
(7, 'IoT_Temperature', 'IoT temperature sensor', 
 0, 'factory/test/temp/sensor1', 
 'float', 'read', 1, 0,
 '°C', 1.0, 0.0, -20.0, 80.0,
 1, 5000, 0.3, 0,
 'iot_sensors', '["iot", "temperature", "wireless"]', 
 '{"sensor_id": "TEMP001", "battery_level": 85, "rssi": -65}',
 '{"protocol": "MQTT", "topic": "factory/test/temp/sensor1", "qos": 1, "retain": false, "message_format": "json", "json_path": "$.temperature"}'),

(7, 'IoT_Humidity', 'IoT humidity sensor', 
 0, 'factory/test/humidity/sensor1', 
 'float', 'read', 1, 0,
 '%RH', 1.0, 0.0, 0.0, 100.0,
 1, 5000, 1.0, 0,
 'iot_sensors', '["iot", "humidity", "wireless"]', 
 '{"sensor_id": "HUM001", "battery_level": 92, "rssi": -58}',
 '{"protocol": "MQTT", "topic": "factory/test/humidity/sensor1", "qos": 1, "retain": false, "message_format": "json", "json_path": "$.humidity"}'),

(7, 'IoT_Vibration', 'IoT vibration sensor', 
 0, 'factory/test/vibration/sensor1', 
 'float', 'read', 1, 0,
 'g', 0.001, 0.0, 0.0, 10.0,
 1, 1000, 0.01, 0,
 'iot_sensors', '["iot", "vibration", "condition_monitoring"]', 
 '{"sensor_id": "VIB001", "battery_level": 78, "sampling_rate": 1000}',
 '{"protocol": "MQTT", "topic": "factory/test/vibration/sensor1", "qos": 2, "retain": false, "message_format": "json", "json_path": "$.rms_acceleration"}');

-- =============================================================================
-- 🔥🔥🔥 현재값 초기화 (JSON 방식 적용)
-- =============================================================================
INSERT OR IGNORE INTO current_values (
    point_id, current_value, value_type, raw_value, 
    quality_code, quality, value_timestamp, last_read_time,
    read_count, write_count, error_count
) VALUES 
-- PLC-Auto-Line1 현재값들
(1, '{"value": 1234}', 'uint32', '{"value": 1234}', 1, 'good', datetime('now'), datetime('now'), 125, 0, 0),
(2, '{"value": 15.5}', 'float', '{"value": 155}', 1, 'good', datetime('now'), datetime('now'), 234, 0, 0),
(3, '{"value": 12.34}', 'float', '{"value": 1234}', 1, 'good', datetime('now'), datetime('now'), 189, 0, 0),
(4, '{"value": 23.5}', 'float', '{"value": 235}', 1, 'good', datetime('now'), datetime('now'), 156, 0, 0),
(5, '{"value": false}', 'bool', '{"value": 0}', 1, 'good', datetime('now'), datetime('now'), 345, 0, 0),
(6, '{"value": false}', 'bool', '{"value": 0}', 1, 'good', datetime('now'), datetime('now'), 67, 2, 0),

-- HMI-Auto-Line1 현재값들
(7, '{"value": 1}', 'uint16', '{"value": 1}', 1, 'good', datetime('now'), datetime('now'), 45, 0, 0),
(8, '{"value": 3}', 'uint16', '{"value": 3}', 1, 'good', datetime('now'), datetime('now'), 78, 0, 0),

-- Robot-Welder 현재값들
(9, '{"value": 125.67}', 'float', '{"value": 12567}', 1, 'good', datetime('now'), datetime('now'), 456, 0, 0),
(10, '{"value": -87.23}', 'float', '{"value": -8723}', 1, 'good', datetime('now'), datetime('now'), 445, 0, 0),
(11, '{"value": 185.4}', 'float', '{"value": 1854}', 1, 'good', datetime('now'), datetime('now'), 234, 0, 0),

-- HVAC-Controller 현재값들
(12, '{"value": 22.3}', 'float', '{"value": 22.3}', 1, 'good', datetime('now'), datetime('now'), 89, 0, 0),
(13, '{"value": 45.7}', 'float', '{"value": 45.7}', 1, 'good', datetime('now'), datetime('now'), 92, 0, 0),
(14, '{"value": 65.0}', 'float', '{"value": 65.0}', 1, 'good', datetime('now'), datetime('now'), 34, 8, 0),

-- Energy-Meter 현재값들  
(15, '{"value": 2750.5}', 'float', '{"value": 275050}', 1, 'good', datetime('now'), datetime('now'), 167, 0, 0),
(16, '{"value": 275.0}', 'float', '{"value": 2750}', 1, 'good', datetime('now'), datetime('now'), 234, 0, 0),
(17, '{"value": 290.5}', 'float', '{"value": 2905}', 1, 'good', datetime('now'), datetime('now'), 234, 0, 0),

-- IoT 센서들 현재값들
(18, '{"value": 24.8}', 'float', '{"value": 24.8}', 1, 'good', datetime('now'), datetime('now'), 345, 0, 0),
(19, '{"value": 58.2}', 'float', '{"value": 58.2}', 1, 'good', datetime('now'), datetime('now'), 298, 0, 0),
(20, '{"value": 0.025}', 'float', '{"value": 25}', 1, 'good', datetime('now'), datetime('now'), 567, 0, 0);

-- =============================================================================
-- 가상포인트 생성 (간소화)
-- =============================================================================
INSERT OR IGNORE INTO virtual_points (
    tenant_id, scope_type, name, description, formula, unit, category, created_by
) VALUES 
(1, 'tenant', 'Total_Production_Rate', 'Company-wide production rate', 
 'production_count / line_speed', 'pcs/min', 'production', 3),
(1, 'tenant', 'Energy_Efficiency', 'Overall energy efficiency',
 '(real_power / apparent_power) * 100', '%', 'efficiency', 3);

-- =============================================================================
-- 시스템 설정 기본값
-- =============================================================================
INSERT OR IGNORE INTO system_settings (key_name, value, description, category, updated_by) VALUES 
('system.version', '2.1.0', 'PulseOne system version', 'system', 1),
('system.timezone', 'Asia/Seoul', 'Default system timezone', 'system', 1),
('system.max_data_retention_days', '365', 'Maximum data retention period', 'system', 1),
('datapoint.default_log_enabled', 'true', 'Default log_enabled for new data points', 'datapoint', 1),
('datapoint.default_log_interval_ms', '5000', 'Default log_interval_ms for new data points', 'datapoint', 1),
('datapoint.default_polling_interval_ms', '1000', 'Default polling_interval_ms for new data points', 'datapoint', 1),
('protocol.modbus.default_timeout_ms', '3000', 'Default Modbus timeout', 'protocol', 1),
('protocol.modbus.default_retry_count', '3', 'Default Modbus retry count', 'protocol', 1),
('protocol.mqtt.default_qos', '1', 'Default MQTT QoS level', 'protocol', 1),
('protocol.bacnet.default_timeout_ms', '5000', 'Default BACnet timeout', 'protocol', 1);