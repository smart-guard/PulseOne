-- =============================================================================
-- backend/lib/database/data/initial-data.sql
-- 초기 데이터 삽입 (SQLite 버전) - 수정됨
-- =============================================================================

-- 스키마 버전 기록
INSERT OR IGNORE INTO schema_versions (version, description) 
VALUES ('2.0.0', 'Unified users and flexible virtual points architecture');

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
 'professional', 'active',
 5, 5000, 20),
('Test Factory', 'test', 'test.pulseone.com',
 'Test Manager', 'test@pulseone.com', 
 'starter', 'trial',
 3, 1000, 5),
('Global Industries', 'global', 'global.pulseone.com',
 'Global Manager', 'global@pulseone.com',
 'enterprise', 'active',
 10, 10000, 50);

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
-- 통합 사용자 생성 (system_admin + tenant users)
-- =============================================================================

-- 시스템 관리자 (tenant_id = NULL)
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, permissions
) VALUES 
(NULL, 'sysadmin', 'admin@pulseone.com', '$2b$12$SystemAdminPassword123456789012345678901', 'System Administrator', 'system_admin',
 '["manage_all_tenants", "manage_system_settings", "view_all_data", "manage_system_users"]'),
(NULL, 'devadmin', 'dev@pulseone.com', '$2b$12$DevAdminPassword123456789012345678901234', 'Development Admin', 'system_admin',
 '["manage_all_tenants", "view_all_data", "system_debug"]');

-- Demo Corporation 사용자들
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, department,
    permissions, site_access, device_access
) VALUES 
(1, 'demo_admin', 'admin@demo.pulseone.com', '$2b$12$DemoAdminPassword123456789012345678901', 'Demo Admin', 'company_admin', 'IT',
 '["manage_company", "manage_all_sites", "manage_users", "view_all_data"]', NULL, NULL),
(1, 'seoul_manager', 'seoul@demo.pulseone.com', '$2b$12$SeoulManagerPassword123456789012345678', 'Seoul Factory Manager', 'site_admin', 'Operations',
 '["manage_site", "manage_devices", "manage_alarms", "view_site_data"]', '[4,9,10,15,16,17]', NULL),
(1, 'line1_engineer', 'line1@demo.pulseone.com', '$2b$12$Line1EngineerPassword123456789012345678', 'Line 1 Engineer', 'engineer', 'Engineering',
 '["manage_devices", "create_virtual_points", "manage_alarms", "view_data"]', '[15]', '[1,2,3]'),
(1, 'operator1', 'operator1@demo.pulseone.com', '$2b$12$Operator1Password123456789012345678901', 'Production Operator 1', 'operator', 'Operations',
 '["view_data", "control_devices", "acknowledge_alarms"]', '[15,16]', '[1,2,4,5]'),
(1, 'maintenance', 'maintenance@demo.pulseone.com', '$2b$12$MaintenancePassword123456789012345678901', 'Maintenance Technician', 'engineer', 'Maintenance',
 '["view_data", "manage_devices", "view_diagnostics"]', '[4,5]', NULL);

-- Test Factory 사용자들
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, department,
    permissions, site_access
) VALUES 
(2, 'test_admin', 'admin@test.pulseone.com', '$2b$12$TestAdminPassword123456789012345678901', 'Test Admin', 'company_admin', 'IT',
 '["manage_company", "manage_sites", "manage_users"]', '[7,13,18]'),
(2, 'test_engineer', 'engineer@test.pulseone.com', '$2b$12$TestEngineerPassword123456789012345678', 'Test Engineer', 'engineer', 'R&D',
 '["view_dashboard", "manage_devices", "create_virtual_points"]', '[18]');

-- Global Industries 사용자들
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, department,
    permissions, site_access
) VALUES 
(3, 'global_admin', 'admin@global.pulseone.com', '$2b$12$GlobalAdminPassword123456789012345678901', 'Global Admin', 'company_admin', 'IT',
 '["manage_company", "manage_sites", "manage_users", "view_all_data"]', '[8,14,19]');

-- =============================================================================
-- 데모 사이트 생성 (계층 구조)
-- =============================================================================
INSERT OR IGNORE INTO sites (
    tenant_id, parent_site_id, name, code, site_type, description, 
    location, manager_name, manager_email, hierarchy_level, sort_order, edge_server_id
) VALUES 
-- 🏢 Level 0: Company
(1, NULL, 'Demo Corporation', 'demo_corp', 'company', 'Demo Corporation Headquarters', 'Seoul, South Korea', 'Demo CEO', 'ceo@demo.com', 0, 1, NULL),
(2, NULL, 'Test Industries', 'test_corp', 'company', 'Test Industries Headquarters', 'Incheon, South Korea', 'Test CEO', 'ceo@test.com', 0, 1, NULL),
(3, NULL, 'Global Industries', 'global_corp', 'company', 'Global Industries Headquarters', 'New York, USA', 'Global CEO', 'ceo@global.com', 0, 1, NULL),

-- 🏭 Level 1: Factories & Offices  
(1, 1, 'Seoul Main Factory', 'seoul_factory', 'factory', 'Main manufacturing facility', 'Seoul Industrial Complex', 'Factory Manager', 'factory@demo.com', 1, 1, 1),
(1, 1, 'Busan Secondary Plant', 'busan_plant', 'factory', 'Secondary production plant', 'Busan Industrial Park', 'Plant Manager', 'plant@demo.com', 1, 2, 3),
(1, 1, 'Seoul HQ Office', 'seoul_office', 'office', 'Corporate headquarters office', 'Seoul CBD', 'Office Manager', 'office@demo.com', 1, 3, NULL),
(2, 2, 'Incheon Test Facility', 'incheon_test', 'factory', 'R&D and testing facility', 'Incheon Free Zone', 'Test Manager', 'test@test.com', 1, 1, 4),
(3, 3, 'Global Factory NY', 'global_ny', 'factory', 'North America production facility', 'New York Industrial Zone', 'NY Manager', 'ny@global.com', 1, 1, 5),

-- 🏗️ Level 2: Buildings
(1, 4, 'Production Building A', 'bldg_a', 'building', 'Main production building', 'Building A', 'Building Manager A', 'bldga@demo.com', 2, 1, 1),
(1, 4, 'Assembly Building B', 'bldg_b', 'building', 'Final assembly building', 'Building B', 'Building Manager B', 'bldgb@demo.com', 2, 2, 2),
(1, 5, 'Busan Main Workshop', 'busan_workshop', 'workshop', 'Main production workshop', 'Workshop 1', 'Workshop Manager', 'workshop@demo.com', 2, 1, 3),
(1, 6, 'IT Data Center', 'seoul_dc', 'datacenter', 'Corporate data center', 'Basement Level', 'IT Manager', 'it@demo.com', 2, 1, NULL),
(2, 7, 'Test Building', 'test_bldg', 'building', 'Main testing building', 'Test Building 1', 'Test Building Manager', 'testbldg@test.com', 2, 1, 4),
(3, 8, 'Production Hall', 'global_hall', 'building', 'Main production hall', 'Production Hall A', 'Hall Manager', 'hall@global.com', 2, 1, 5),

-- 🔧 Level 3: Lines & Areas
(1, 9, 'Automotive Line 1', 'auto_line1', 'line', 'Automotive parts production line', 'Line 1', 'Line 1 Supervisor', 'line1@demo.com', 3, 1, 1),
(1, 9, 'Electronics Line 2', 'elec_line2', 'line', 'Electronics assembly line', 'Line 2', 'Line 2 Supervisor', 'line2@demo.com', 3, 2, 1),
(1, 10, 'Final Assembly Area', 'final_assembly', 'area', 'Final product assembly area', 'Assembly Floor', 'Assembly Supervisor', 'assembly@demo.com', 3, 1, 2),
(1, 11, 'Quality Control Zone', 'qc_zone', 'zone', 'Quality control and testing zone', 'QC Area', 'QC Manager', 'qc@demo.com', 3, 1, 3),
(2, 13, 'Test Line Alpha', 'test_alpha', 'line', 'Alpha testing line', 'Test Line A', 'Alpha Supervisor', 'alpha@test.com', 3, 1, 4),
(3, 14, 'Global Production Line', 'global_line1', 'line', 'Main production line', 'Global Line 1', 'Global Supervisor', 'line@global.com', 3, 1, 5);

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
-- 데모 디바이스 그룹 생성
-- =============================================================================
INSERT OR IGNORE INTO device_groups (tenant_id, site_id, name, description, group_type) VALUES 
-- Demo Corporation 그룹들
(1, 15, 'Production Equipment', 'Automotive line production equipment', 'functional'), -- Automotive Line 1
(1, 16, 'Electronics Equipment', 'Electronics line equipment', 'functional'), -- Electronics Line 2
(1, 9, 'Building Utilities', 'HVAC, Power, Water systems', 'functional'), -- Building A
(1, 17, 'Assembly Equipment', 'Final assembly line equipment', 'functional'), -- Final Assembly Area
(1, 18, 'QC Equipment', 'Quality control and testing equipment', 'functional'), -- QC Zone

-- Test Factory 그룹들
(2, 19, 'Test Equipment', 'Testing and QA equipment', 'functional'), -- Test Line Alpha

-- Global Industries 그룹들
(3, 20, 'Global Production', 'Global production line equipment', 'functional'); -- Global Production Line

-- =============================================================================
-- 데모 디바이스 생성
-- =============================================================================
INSERT OR IGNORE INTO devices (
    tenant_id, site_id, device_group_id, edge_server_id,
    name, description, device_type, manufacturer, model, serial_number,
    protocol_type, endpoint, config,
    polling_interval, timeout, is_enabled, created_by
) VALUES 
-- Demo Corporation - Automotive Line 1
(1, 15, 1, 1, 'PLC-Auto-Line1', 'Automotive line main PLC', 'PLC', 'Siemens', 'S7-1500', 'S7-1500-001', 
 'MODBUS_TCP', '192.168.1.10:502', '{"slave_id": 1, "byte_order": "big_endian"}', 1000, 3000, 1, 3),
(1, 15, 1, 1, 'HMI-Auto-Line1', 'Automotive line operator interface', 'HMI', 'Schneider', 'XBTGT7340', 'HMI-001', 
 'MODBUS_TCP', '192.168.1.11:502', '{"slave_id": 2, "byte_order": "big_endian"}', 2000, 3000, 1, 3),
(1, 15, 1, 1, 'Robot-Auto-Line1', 'Automotive line welding robot', 'ROBOT', 'KUKA', 'KR-16', 'KUKA-001', 
 'MODBUS_TCP', '192.168.1.12:502', '{"slave_id": 3, "byte_order": "big_endian"}', 500, 2000, 1, 3),

-- Demo Corporation - Electronics Line 2
(1, 16, 2, 1, 'PLC-Elec-Line2', 'Electronics line main PLC', 'PLC', 'Allen-Bradley', 'ControlLogix', 'AB-001', 
 'MODBUS_TCP', '192.168.1.20:502', '{"slave_id": 4, "byte_order": "big_endian"}', 1000, 3000, 1, 3),
(1, 16, 2, 1, 'Vision-Elec-Line2', 'Electronics line vision system', 'SENSOR', 'Cognex', 'In-Sight 7000', 'COG-001', 
 'MODBUS_TCP', '192.168.1.21:502', '{"slave_id": 5, "byte_order": "big_endian"}', 2000, 3000, 1, 3),

-- Demo Corporation - Building Utilities
(1, 9, 3, 2, 'HVAC-BuildingA', 'Building A HVAC controller', 'CONTROLLER', 'Honeywell', 'BACnet-Controller', 'HON-001', 
 'BACNET', '192.168.1.30:47808', '{"device_id": 1001, "network": 1}', 5000, 5000, 1, 3),
(1, 9, 3, 2, 'Energy-Meter-Main', 'Building A main energy meter', 'METER', 'Schneider', 'PowerLogic', 'PWR-001', 
 'MODBUS_TCP', '192.168.1.40:502', '{"slave_id": 10, "byte_order": "big_endian"}', 5000, 3000, 1, 3),

-- Demo Corporation - Assembly Area
(1, 17, 4, 2, 'Conveyor-Assembly', 'Assembly line conveyor controller', 'CONTROLLER', 'SEW', 'MoviPLC', 'SEW-001', 
 'MODBUS_TCP', '192.168.2.10:502', '{"slave_id": 6, "byte_order": "big_endian"}', 1000, 3000, 1, 3),
(1, 17, 4, 2, 'Barcode-Scanner', 'Assembly line barcode scanner', 'SENSOR', 'Keyence', 'SR-1000', 'KEY-001', 
 'MODBUS_TCP', '192.168.2.11:502', '{"slave_id": 7, "byte_order": "big_endian"}', 500, 2000, 1, 3),

-- Test Factory
(2, 19, 6, 4, 'Test-PLC-01', 'Test line main PLC', 'PLC', 'Mitsubishi', 'FX5U', 'MIT-001', 
 'MODBUS_TCP', '192.168.3.10:502', '{"slave_id": 1, "byte_order": "big_endian"}', 1000, 3000, 1, 8),
(2, 19, 6, 4, 'Test-Analyzer', 'Product quality analyzer', 'SENSOR', 'Agilent', 'U2702A', 'AGL-001', 
 'MODBUS_TCP', '192.168.3.11:502', '{"slave_id": 2, "byte_order": "big_endian"}', 2000, 3000, 1, 8),

-- Global Industries
(3, 20, 7, 5, 'Global-PLC-Main', 'Global line main PLC', 'PLC', 'Rockwell', 'CompactLogix', 'RW-001', 
 'MODBUS_TCP', '192.168.4.10:502', '{"slave_id": 1, "byte_order": "big_endian"}', 1000, 3000, 1, 10);

-- =============================================================================
-- 데모 데이터 포인트 생성 (주요 디바이스들)
-- =============================================================================
INSERT OR IGNORE INTO data_points (
    device_id, name, description, address, data_type, access_mode,
    unit, scaling_factor, min_value, max_value, is_enabled
) VALUES 
-- PLC-Auto-Line1 데이터 포인트
(1, 'Production_Count', 'Automotive production count', 1001, 'uint32', 'read', 'pcs', 1.0, 0, 999999, 1),
(1, 'Line_Speed', 'Automotive line speed', 1002, 'float', 'read', 'm/min', 0.1, 0, 100, 1),
(1, 'Motor_Current', 'Main motor current', 1003, 'float', 'read', 'A', 0.01, 0, 50, 1),
(1, 'Temperature', 'Process temperature', 1004, 'float', 'read', '°C', 0.1, -40, 150, 1),
(1, 'Emergency_Stop', 'Emergency stop status', 1005, 'bool', 'read', '', 1.0, 0, 1, 1),

-- PLC-Elec-Line2 데이터 포인트
(4, 'Electronics_Count', 'Electronics production count', 2001, 'uint32', 'read', 'pcs', 1.0, 0, 999999, 1),
(4, 'Conveyor_Speed', 'Electronics line conveyor speed', 2002, 'float', 'read', 'm/min', 0.1, 0, 50, 1),
(4, 'Power_Consumption', 'Line power consumption', 2003, 'float', 'read', 'kW', 0.1, 0, 500, 1),

-- Energy-Meter-Main 데이터 포인트
(7, 'Total_Energy', 'Building total energy consumption', 3001, 'float', 'read', 'kWh', 0.01, 0, 999999, 1),
(7, 'Real_Power', 'Real power consumption', 3002, 'float', 'read', 'kW', 0.1, 0, 1000, 1),
(7, 'Apparent_Power', 'Apparent power consumption', 3003, 'float', 'read', 'kVA', 0.1, 0, 1000, 1);

-- =============================================================================
-- 개선된 가상포인트 생성 (테넌트/사이트/디바이스별)
-- =============================================================================

-- 1. 테넌트 전체 가상포인트 (Demo Corporation 전체)
INSERT OR IGNORE INTO virtual_points (
    tenant_id, scope_type, site_id, device_id, 
    name, description, formula, unit, category, created_by
) VALUES 
(1, 'tenant', NULL, NULL,
 'Total_Company_Production', 'Demo Corporation total production across all sites', 
 'auto_production + electronics_production', 'pcs', 'production', 3),
(1, 'tenant', NULL, NULL,
 'Company_Energy_Efficiency', 'Overall company energy efficiency',
 '(total_production / total_energy) * 100', 'pcs/kWh', 'efficiency', 3);

-- 2. 사이트별 가상포인트 (Seoul Factory)
INSERT OR IGNORE INTO virtual_points (
    tenant_id, scope_type, site_id, device_id,
    name, description, formula, unit, category, created_by
) VALUES 
(1, 'site', 4, NULL,
 'Seoul_Factory_Efficiency', 'Seoul factory overall efficiency',
 '(actual_production / target_production) * 100', '%', 'efficiency', 3),
(1, 'site', 4, NULL,
 'Seoul_Power_Factor', 'Seoul factory average power factor',
 'total_real_power / total_apparent_power', '', 'electrical', 3);

-- 3. 디바이스별 가상포인트 (Energy Meter 계산)
INSERT OR IGNORE INTO virtual_points (
    tenant_id, scope_type, site_id, device_id,
    name, description, formula, unit, category, created_by
) VALUES 
(1, 'device', 9, 7,
 'Power_Factor', 'Building A power factor calculation',
 'real_power / apparent_power', '', 'electrical', 3),
(1, 'device', 15, 1,
 'Production_Efficiency', 'Automotive line production efficiency',
 '(production_count / target_count) * 100', '%', 'efficiency', 4);

-- =============================================================================
-- 가상포인트 입력 매핑
-- =============================================================================
INSERT OR IGNORE INTO virtual_point_inputs (
    virtual_point_id, variable_name, source_type, source_id, data_processing, constant_value
) VALUES 
-- 테넌트 전체 생산량을 위한 입력들
(1, 'auto_production', 'data_point', 1, 'current', NULL), -- Production_Count from Auto Line
(1, 'electronics_production', 'data_point', 5, 'current', NULL), -- Electronics_Count from Elec Line

-- 테넌트 에너지 효율성을 위한 입력들  
(2, 'total_production', 'virtual_point', 1, 'current', NULL), -- 위에서 만든 총 생산량
(2, 'total_energy', 'data_point', 8, 'current', NULL), -- Total_Energy from meter

-- Seoul Factory 효율성을 위한 입력들
(3, 'actual_production', 'virtual_point', 1, 'current', NULL), -- 실제 생산량
(3, 'target_production', 'constant', NULL, 'current', 2000), -- 목표 생산량 (상수)

-- Seoul Factory 역률을 위한 입력들
(4, 'total_real_power', 'data_point', 9, 'current', NULL), -- Real_Power
(4, 'total_apparent_power', 'data_point', 10, 'current', NULL), -- Apparent_Power

-- 디바이스별 역률 계산
(5, 'real_power', 'data_point', 9, 'current', NULL), -- Real_Power
(5, 'apparent_power', 'data_point', 10, 'current', NULL), -- Apparent_Power

-- 생산 효율성 계산
(6, 'production_count', 'data_point', 1, 'current', NULL), -- Production_Count
(6, 'target_count', 'constant', NULL, 'current', 1000); -- 목표 생산량 (상수)

-- =============================================================================
-- 데모 현재값 초기화
-- =============================================================================
INSERT OR IGNORE INTO current_values (point_id, value, raw_value, quality) VALUES 
(1, 1234, 1234, 'good'),    -- Auto Production_Count
(2, 15.5, 155, 'good'),     -- Auto Line_Speed
(3, 12.34, 1234, 'good'),   -- Auto Motor_Current
(4, 23.5, 235, 'good'),     -- Auto Temperature
(5, 0, 0, 'good'),          -- Auto Emergency_Stop
(6, 856, 856, 'good'),      -- Electronics_Count
(7, 8.2, 82, 'good'),       -- Electronics Conveyor_Speed
(8, 145.5, 1455, 'good'),   -- Electronics Power_Consumption
(9, 2750.5, 275050, 'good'), -- Total_Energy
(10, 275.0, 2750, 'good'),  -- Real_Power
(11, 290.5, 2905, 'good');  -- Apparent_Power

-- =============================================================================
-- 시스템 설정 기본값
-- =============================================================================
INSERT OR IGNORE INTO system_settings (key_name, value, description, category, updated_by) VALUES 
('system.version', '2.0.0', 'PulseOne system version', 'system', 1),
('system.timezone', 'UTC', 'Default system timezone', 'system', 1),
('system.max_data_retention_days', '365', 'Maximum data retention period', 'system', 1),
('system.backup_enabled', 'true', 'Enable automatic backup', 'system', 1),
('system.backup_interval_hours', '24', 'Backup interval in hours', 'system', 1),
('ui.refresh_interval_ms', '5000', 'UI refresh interval', 'ui', 1),
('ui.max_chart_points', '1000', 'Maximum points in charts', 'ui', 1),
('security.session_timeout_minutes', '60', 'User session timeout', 'security', 1),
('security.password_min_length', '8', 'Minimum password length', 'security', 1),
('security.max_login_attempts', '5', 'Maximum login attempts', 'security', 1),
('alerts.email_enabled', 'false', 'Enable email alerts', 'alerts', 1),
('alerts.sms_enabled', 'false', 'Enable SMS alerts', 'alerts', 1),
('virtual_points.calculation_enabled', 'true', 'Enable virtual point calculations', 'virtual_points', 1),
('virtual_points.default_interval_ms', '5000', 'Default calculation interval', 'virtual_points', 1);