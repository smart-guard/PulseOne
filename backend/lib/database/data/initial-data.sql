-- =============================================================================
-- backend/lib/database/data/initial-data.sql
-- 초기 데이터 삽입 (SQLite 버전)
-- =============================================================================

-- 스키마 버전 기록
INSERT OR IGNORE INTO schema_versions (version, description) 
VALUES ('2.0.0', 'Unified users and flexible virtual points architecture');

-- 데모 테넌트 생성
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
 'starter', 'trial', 3, 1000, 5);

-- 시스템 관리자 (tenant_id = NULL)
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, permissions
) VALUES 
(NULL, 'sysadmin', 'admin@pulseone.com', '$2b$12$SystemAdminPassword123456789012345678901', 'System Administrator', 'system_admin',
 '["manage_all_tenants", "manage_system_settings", "view_all_data"]');

-- Demo Corporation 사용자들
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, department, permissions
) VALUES 
(1, 'demo_admin', 'admin@demo.pulseone.com', '$2b$12$DemoAdminPassword123456789012345678901', 'Demo Admin', 'company_admin', 'IT',
 '["manage_company", "manage_all_sites", "manage_users", "view_all_data"]'),
(1, 'demo_engineer', 'engineer@demo.pulseone.com', '$2b$12$DemoEngineerPassword12345678901234567890', 'Demo Engineer', 'engineer', 'Engineering',
 '["manage_devices", "manage_data_points", "view_data", "manage_alarms"]'),
(1, 'demo_operator', 'operator@demo.pulseone.com', '$2b$12$DemoOperatorPassword12345678901234567890', 'Demo Operator', 'operator', 'Operations',
 '["view_data", "acknowledge_alarms", "control_devices"]');

-- 시스템 설정 기본값
INSERT OR IGNORE INTO system_settings (key_name, value, description, category, updated_by) VALUES 
('system.version', '2.0.0', 'PulseOne system version', 'system', 1),
('system.timezone', 'UTC', 'Default system timezone', 'system', 1),
('virtual_points.calculation_enabled', 'true', 'Enable virtual point calculations', 'virtual_points', 1),
('alarms.notification_enabled', 'true', 'Enable alarm notifications', 'alarms', 1),
('data_collection.default_interval', '1000', 'Default data collection interval (ms)', 'data_collection', 1);

-- Demo Corporation 사이트 구조
INSERT OR IGNORE INTO sites (
    tenant_id, parent_site_id, name, code, site_type, description, 
    location, hierarchy_level, sort_order
) VALUES 
-- 회사 레벨 (0단계)
(1, NULL, 'Demo Corporation', 'DEMO_CORP', 'company', 'Demo Corporation Headquarters', 'Seoul, Korea', 0, 1),

-- 공장 레벨 (1단계)
(1, 1, 'Seoul Factory', 'SEOUL_FAC', 'factory', 'Main manufacturing facility', 'Seoul, Gangnam-gu', 1, 1),
(1, 1, 'Busan Factory', 'BUSAN_FAC', 'factory', 'Secondary manufacturing facility', 'Busan, Haeundae-gu', 1, 2),

-- 빌딩 레벨 (2단계)
(1, 2, 'Production Building A', 'PROD_A', 'building', 'Main production building', 'Seoul Factory Complex', 2, 1),
(1, 2, 'Production Building B', 'PROD_B', 'building', 'Secondary production building', 'Seoul Factory Complex', 2, 2),
(1, 2, 'Warehouse Building', 'WAREHOUSE', 'building', 'Storage and logistics', 'Seoul Factory Complex', 2, 3),

-- 라인 레벨 (3단계)
(1, 4, 'Production Line 1', 'LINE_1', 'line', 'High-speed assembly line', 'Building A, Floor 1', 3, 1),
(1, 4, 'Production Line 2', 'LINE_2', 'line', 'Quality control line', 'Building A, Floor 1', 3, 2),
(1, 5, 'Production Line 3', 'LINE_3', 'line', 'Packaging line', 'Building B, Floor 1', 3, 3);

-- 드라이버 플러그인 등록
INSERT OR IGNORE INTO driver_plugins (
    name, protocol_type, version, file_path, description, author, is_enabled
) VALUES 
('Modbus TCP Driver', 'MODBUS_TCP', '1.0.0', '/plugins/modbus_tcp.so', 'Standard Modbus TCP/IP driver', 'PulseOne Team', 1),
('Modbus RTU Driver', 'MODBUS_RTU', '1.0.0', '/plugins/modbus_rtu.so', 'Modbus RTU over serial driver', 'PulseOne Team', 1),
('MQTT Client Driver', 'MQTT', '1.0.0', '/plugins/mqtt_client.so', 'MQTT 3.1.1 client driver', 'PulseOne Team', 1),
('BACnet Driver', 'BACNET', '1.0.0', '/plugins/bacnet.so', 'BACnet/IP protocol driver', 'PulseOne Team', 1),
('OPC UA Client', 'OPCUA', '1.0.0', '/plugins/opcua_client.so', 'OPC UA client driver', 'PulseOne Team', 1);

-- 샘플 디바이스 그룹
INSERT OR IGNORE INTO device_groups (
    tenant_id, site_id, name, description, group_type
) VALUES 
(1, 7, 'PLC Controllers', 'Programmable Logic Controllers', 'functional'),
(1, 7, 'Temperature Sensors', 'Temperature monitoring devices', 'functional'),
(1, 7, 'Energy Meters', 'Power consumption monitoring', 'functional'),
(1, 8, 'Quality Control', 'Quality assurance equipment', 'functional'),
(1, 9, 'Packaging Equipment', 'Packaging line devices', 'functional');

-- 샘플 디바이스
INSERT OR IGNORE INTO devices (
    tenant_id, site_id, device_group_id, name, description, device_type,
    manufacturer, model, protocol_type, endpoint, config, polling_interval, created_by
) VALUES 
-- Line 1 devices
(1, 7, 1, 'PLC-LINE1-001', 'Main PLC for Production Line 1', 'PLC',
 'Siemens', 'S7-1200', 'MODBUS_TCP', '192.168.1.100:502', '{"slave_id": 1, "timeout": 3000}', 1000, 2),
 
(1, 7, 2, 'TEMP-LINE1-001', 'Oven Temperature Sensor', 'SENSOR',
 'Honeywell', 'PT100', 'MODBUS_TCP', '192.168.1.101:502', '{"slave_id": 2, "timeout": 3000}', 2000, 2),
 
(1, 7, 3, 'METER-LINE1-001', 'Line 1 Energy Meter', 'METER',
 'Schneider', 'PM5560', 'MODBUS_TCP', '192.168.1.102:502', '{"slave_id": 3, "timeout": 3000}', 5000, 2),

-- Line 2 devices  
(1, 8, 1, 'PLC-LINE2-001', 'Main PLC for Production Line 2', 'PLC',
 'Allen-Bradley', 'CompactLogix', 'MODBUS_TCP', '192.168.1.110:502', '{"slave_id": 4, "timeout": 3000}', 1000, 2),
 
(1, 8, 4, 'QC-CAMERA-001', 'Quality Control Camera System', 'SENSOR',
 'Cognex', 'In-Sight 7000', 'MODBUS_TCP', '192.168.1.111:502', '{"slave_id": 5, "timeout": 5000}', 1000, 2);

-- 샘플 데이터 포인트
INSERT OR IGNORE INTO data_points (
    device_id, name, description, address, data_type, unit, access_mode, is_enabled
) VALUES 
-- PLC-LINE1-001 포인트들
(1, 'Production_Count', 'Current production count', 1001, 'uint32', 'pcs', 'read', 1),
(1, 'Line_Speed', 'Production line speed', 1002, 'float', 'pcs/min', 'read', 1),
(1, 'Motor_Status', 'Main motor running status', 1003, 'bool', '', 'read', 1),
(1, 'Emergency_Stop', 'Emergency stop button status', 1004, 'bool', '', 'read', 1),
(1, 'Target_Speed', 'Target production speed', 1005, 'float', 'pcs/min', 'readwrite', 1),

-- TEMP-LINE1-001 포인트들
(2, 'Oven_Temperature', 'Current oven temperature', 2001, 'float', '°C', 'read', 1),
(2, 'Temperature_Setpoint', 'Temperature setpoint', 2002, 'float', '°C', 'readwrite', 1),
(2, 'Heater_Status', 'Heater on/off status', 2003, 'bool', '', 'read', 1),

-- METER-LINE1-001 포인트들
(3, 'Total_Power', 'Total power consumption', 3001, 'float', 'kW', 'read', 1),
(3, 'Voltage_L1', 'Line 1 voltage', 3002, 'float', 'V', 'read', 1),
(3, 'Current_L1', 'Line 1 current', 3003, 'float', 'A', 'read', 1),

-- PLC-LINE2-001 포인트들
(4, 'Production_Count_L2', 'Line 2 production count', 4001, 'uint32', 'pcs', 'read', 1),
(4, 'Line_Speed_L2', 'Line 2 speed', 4002, 'float', 'pcs/min', 'read', 1),

-- QC-CAMERA-001 포인트들
(5, 'Pass_Count', 'Quality pass count', 5001, 'uint32', 'pcs', 'read', 1),
(5, 'Fail_Count', 'Quality fail count', 5002, 'uint32', 'pcs', 'read', 1),
(5, 'Pass_Rate', 'Quality pass rate', 5003, 'float', '%', 'read', 1);

-- 샘플 가상 포인트
INSERT OR IGNORE INTO virtual_points (
    tenant_id, scope_type, site_id, name, description, formula, 
    data_type, unit, calculation_interval, created_by
) VALUES 
(1, 'site', 7, 'Total_Production_L1', 'Total production for Line 1', 
 'return inputs.Production_Count || 0;', 'uint32', 'pcs', 1000, 2),
 
(1, 'site', 7, 'Energy_Efficiency', 'Energy efficiency calculation',
 'const production = inputs.Production_Count || 0; const power = inputs.Total_Power || 1; return production / power;',
 'float', 'pcs/kWh', 5000, 2),
 
(1, 'tenant', NULL, 'Overall_Production', 'Company-wide total production',
 'return (inputs.Production_L1 || 0) + (inputs.Production_L2 || 0);',
 'uint32', 'pcs', 2000, 2);

-- 가상 포인트 입력 매핑
INSERT OR IGNORE INTO virtual_point_inputs (
    virtual_point_id, variable_name, source_type, source_id
) VALUES 
-- Total_Production_L1 입력
(1, 'Production_Count', 'data_point', 1),

-- Energy_Efficiency 입력
(2, 'Production_Count', 'data_point', 1),
(2, 'Total_Power', 'data_point', 9),

-- Overall_Production 입력  
(3, 'Production_L1', 'virtual_point', 1),
(3, 'Production_L2', 'data_point', 12);

-- 샘플 알람 정의
INSERT OR IGNORE INTO alarm_definitions (
    tenant_id, site_id, data_point_id, alarm_name, description,
    severity, condition_type, threshold_value, message_template
) VALUES 
(1, 7, 6, 'High Temperature Alarm', 'Oven temperature too high', 'high', 'threshold', 250.0,
 'Oven temperature has exceeded {value}°C (limit: {threshold}°C)'),
 
(1, 7, 9, 'High Power Consumption', 'Power consumption alarm', 'medium', 'threshold', 100.0,
 'Power consumption is {value}kW, exceeding limit of {threshold}kW'),
 
(1, 8, 15, 'Low Quality Rate', 'Quality pass rate too low', 'high', 'threshold', 95.0,
 'Quality pass rate has dropped to {value}% (minimum: {threshold}%)');
