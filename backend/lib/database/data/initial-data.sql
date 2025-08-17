-- =============================================================================
-- initial-data.sql - 초기 샘플 데이터
-- =============================================================================

-- 기본 테넌트 생성
INSERT OR REPLACE INTO tenants (
    id, company_name, company_code, domain, contact_name, contact_email, 
    subscription_plan, subscription_status, max_edge_servers, max_data_points, max_users
) VALUES (
    1, 'Smart Guard Corporation', 'SGC', 'smartguard.local', 'System Admin', 'admin@smartguard.local',
    'professional', 'active', 10, 10000, 50
);

-- Edge 서버 생성
INSERT OR REPLACE INTO edge_servers (
    id, tenant_id, server_name, server_code, hostname, ip_address, port, status, version
) VALUES (
    1, 1, 'Main Edge Server', 'EDGE01', 'localhost', '127.0.0.1', 3000, 'online', '2.1.0'
);

-- 기본 사용자 생성 (admin/admin)
INSERT OR REPLACE INTO users (
    id, tenant_id, username, email, password_hash, full_name, role, is_active
) VALUES (
    1, 1, 'admin', 'admin@smartguard.local', 
    '$2b$10$8K1p/a0dQt3C4qtzNzQgZe7d7o.S4Q8YjSFkjEJQq4zB1YfH5YnS6',
    'System Administrator', 'admin', 1
);

-- 기본 사이트 생성
INSERT OR REPLACE INTO sites (
    id, tenant_id, edge_server_id, site_name, site_code, description, 
    location_city, location_country, timezone
) VALUES (
    1, 1, 1, 'Seoul Main Factory', 'SITE1', 'Main manufacturing facility in Seoul',
    'Seoul', 'South Korea', 'Asia/Seoul'
);

-- 디바이스 그룹 생성
INSERT OR REPLACE INTO device_groups (
    id, tenant_id, site_id, group_name, group_type, description
) VALUES 
(1, 1, 1, 'Production Line A', 'functional', 'Main production line equipment'),
(2, 1, 1, 'HVAC System', 'functional', 'Heating, ventilation, and air conditioning');

-- 샘플 디바이스 생성
INSERT OR REPLACE INTO devices (
    id, tenant_id, site_id, device_group_id, edge_server_id, name, description,
    device_type, manufacturer, model, protocol_type, endpoint, config, 
    polling_interval, is_enabled
) VALUES 
(1, 1, 1, 1, 1, 'PLC-001', 'Main Production PLC', 'PLC', 'Siemens', 'S7-1200', 
 'MODBUS_TCP', '192.168.1.10:502', '{"slave_id":1,"timeout":3000}', 1000, 0),
(2, 1, 1, 1, 1, 'HMI-001', 'Operator Interface Panel', 'HMI', 'Schneider', 'Magelis',
 'MODBUS_TCP', '192.168.1.11:502', '{"slave_id":2,"timeout":3000}', 2000, 1),
(3, 1, 1, 2, 1, 'HVAC-CTRL-001', 'Main HVAC Controller', 'CONTROLLER', 'Honeywell', 'T7350',
 'BACNET', '192.168.1.20', '{"device_id":20,"network":1}', 5000, 1);

-- 디바이스 상태 초기화
INSERT OR REPLACE INTO device_status (device_id, connection_status) VALUES
(1, 'disconnected'), (2, 'connected'), (3, 'connected');

-- 디바이스 설정 초기화
INSERT OR REPLACE INTO device_settings (device_id) VALUES (1), (2), (3);

-- 샘플 데이터포인트 생성
INSERT OR REPLACE INTO data_points (
    id, tenant_id, device_id, point_name, point_address, data_type, unit, is_enabled
) VALUES 
-- PLC-001 데이터포인트
(1, 1, 1, 'Temperature', '40001', 'number', '°C', 1),
(2, 1, 1, 'Pressure', '40002', 'number', 'bar', 1),
(3, 1, 1, 'Flow Rate', '40003', 'number', 'L/min', 1),
(4, 1, 1, 'Level', '40004', 'number', 'mm', 1),
(5, 1, 1, 'Vibration', '40005', 'number', 'Hz', 1),
-- HMI-001 데이터포인트
(6, 1, 2, 'Temperature', '40001', 'number', '°C', 1),
(7, 1, 2, 'Pressure', '40002', 'number', 'bar', 1),
-- HVAC-CTRL-001 데이터포인트
(8, 1, 3, 'Temperature', 'analogValue:1', 'number', '°C', 1),
(9, 1, 3, 'Pressure', 'analogValue:2', 'number', 'bar', 1);

-- 샘플 가상포인트 생성
INSERT OR REPLACE INTO virtual_points (
    id, tenant_id, name, description, formula, data_type, unit,
    calculation_interval, calculation_trigger, is_enabled, category
) VALUES (
    14, 1, 'Test VP Fixed', 'Fixed value test virtual point', '1 + 1', 'float', null,
    1000, 'timer', 1, 'calculation'
);

-- 샘플 알람 룰 생성
INSERT OR REPLACE INTO alarm_rules (
    id, tenant_id, name, description, device_id, data_point_id,
    condition_type, condition_config, severity, message_template, is_enabled
) VALUES (
    1, 1, 'High Temperature Alert', 'Temperature exceeds safe threshold', 1, 1,
    'threshold', '{"operator":">","value":80}', 'major', 
    'High temperature detected: {{value}}°C', 1
);