-- =============================================================================
-- backend/lib/database/schemas/08-initial-data.sql
-- 초기 데이터 및 샘플 데이터 (SQLite 버전) - 2025-08-14 최신 업데이트
-- PulseOne v2.1.0 완전 호환, Struct DataPoint 100% 반영 샘플 데이터
-- =============================================================================

-- 스키마 버전 기록
INSERT OR IGNORE INTO schema_versions (version, description) 
VALUES ('2.1.0', 'Complete PulseOne v2.1.0 schema with DataPoint struct alignment and JSON protocol params');

-- =============================================================================
-- 🔥🔥🔥 데모 테넌트 생성 (실제 운영 환경 시뮬레이션)
-- =============================================================================
INSERT OR IGNORE INTO tenants (
    company_name, company_code, domain, 
    contact_name, contact_email, contact_phone,
    subscription_plan, subscription_status,
    max_edge_servers, max_data_points, max_users,
    billing_cycle, subscription_start_date, currency, language, timezone
) VALUES 
('Smart Factory Korea', 'SFK001', 'smartfactory.pulseone.io', 
 'Factory Manager', 'manager@smartfactory.co.kr', '+82-2-1234-5678',
 'professional', 'active', 10, 10000, 50,
 'yearly', '2025-01-01 00:00:00', 'KRW', 'ko', 'Asia/Seoul'),

('Global Manufacturing Inc', 'GMI002', 'global-mfg.pulseone.io',
 'Operations Director', 'ops@globalmfg.com', '+1-555-0123',
 'enterprise', 'active', 50, 100000, 200,
 'monthly', '2024-06-01 00:00:00', 'USD', 'en', 'America/New_York'),

('Demo Corporation', 'DEMO', 'demo.pulseone.io', 
 'Demo Manager', 'demo@pulseone.com', '+82-10-0000-0000',
 'starter', 'trial', 3, 1000, 10,
 'monthly', '2025-08-01 00:00:00', 'USD', 'en', 'UTC'),

('Test Factory Ltd', 'TEST', 'test.pulseone.io',
 'Test Engineer', 'test@testfactory.com', '+82-31-9999-8888', 
 'professional', 'active', 5, 5000, 25,
 'monthly', '2025-07-01 00:00:00', 'KRW', 'ko', 'Asia/Seoul');

-- =============================================================================
-- 🔥🔥🔥 Edge 서버 등록 (분산 아키텍처)
-- =============================================================================
INSERT OR IGNORE INTO edge_servers (
    tenant_id, server_name, factory_name, location,
    ip_address, hostname, mac_address, port,
    registration_token, activation_code, api_key,
    status, version, capabilities, config
) VALUES 
-- Smart Factory Korea Edge 서버들
(1, 'Edge-SFK-Seoul-01', 'Seoul Main Factory', 'Building A - Floor 1',
 '192.168.1.100', 'edge-sfk-seoul-01.local', '00:11:22:33:44:55', 8080,
 'sfk-token-seoul-001', 'SFK001', 'sfk-api-key-001',
 'active', '2.1.0', '["MODBUS_TCP", "MODBUS_RTU", "MQTT", "BACNET"]', 
 '{"max_devices": 100, "log_level": "INFO", "data_retention_days": 30}'),

(1, 'Edge-SFK-Busan-01', 'Busan Secondary Plant', 'Production Hall B',
 '192.168.2.100', 'edge-sfk-busan-01.local', '00:11:22:33:44:66', 8080,
 'sfk-token-busan-001', 'SFK002', 'sfk-api-key-002',
 'active', '2.1.0', '["MODBUS_TCP", "MQTT", "OPCUA"]',
 '{"max_devices": 50, "log_level": "WARN", "data_retention_days": 30}'),

-- Global Manufacturing Edge 서버들  
(2, 'Edge-GMI-NewYork-01', 'New York Plant', 'Manufacturing Floor 1',
 '10.0.1.100', 'edge-gmi-ny-01.global', '00:AA:BB:CC:DD:01', 8080,
 'gmi-token-ny-001', 'GMI001', 'gmi-api-key-001',
 'active', '2.1.0', '["MODBUS_TCP", "ETHERNET_IP", "OPCUA"]',
 '{"max_devices": 200, "log_level": "INFO", "data_retention_days": 90}'),

(2, 'Edge-GMI-Detroit-01', 'Detroit Automotive Plant', 'Assembly Line 1-3',
 '10.0.2.100', 'edge-gmi-detroit-01.global', '00:AA:BB:CC:DD:02', 8080,
 'gmi-token-detroit-001', 'GMI002', 'gmi-api-key-002',
 'active', '2.1.0', '["MODBUS_TCP", "ETHERNET_IP", "PROFINET"]',
 '{"max_devices": 150, "log_level": "DEBUG", "data_retention_days": 60}'),

-- Demo/Test Edge 서버들
(3, 'Edge-Demo-01', 'Demo Factory', 'Demo Building',
 '192.168.100.100', 'edge-demo-01.local', '00:DE:MO:01:02:03', 8080,
 'demo-token-001', 'DEMO001', 'demo-api-key-001',
 'active', '2.1.0', '["MODBUS_TCP", "MODBUS_RTU", "MQTT", "BACNET"]',
 '{"max_devices": 20, "log_level": "DEBUG", "data_retention_days": 7}'),

(4, 'Edge-Test-01', 'Test Facility', 'Test Lab',
 '192.168.200.100', 'edge-test-01.local', '00:TE:ST:01:02:03', 8080,
 'test-token-001', 'TEST001', 'test-api-key-001',
 'active', '2.1.0', '["MODBUS_TCP", "MQTT", "BACNET", "OPCUA"]',
 '{"max_devices": 10, "log_level": "DEBUG", "data_retention_days": 3}');

-- =============================================================================
-- 🔥🔥🔥 통합 사용자 생성 (시스템 관리자 + 테넌트 사용자)
-- =============================================================================

-- 시스템 관리자들
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, permissions,
    timezone, language, is_active, email_verified, two_factor_enabled
) VALUES 
(NULL, 'sysadmin', 'admin@pulseone.com', '$2b$12$SystemAdminPasswordHash123456789012345678901234567890', 
 'System Administrator', 'system_admin', 
 '["manage_all_tenants", "manage_system_settings", "view_all_data", "manage_edge_servers", "system_diagnostics"]',
 'UTC', 'en', 1, 1, 1),

(NULL, 'devadmin', 'dev@pulseone.com', '$2b$12$DevAdminPasswordHash1234567890123456789012345678901234567890', 
 'Development Admin', 'system_admin',
 '["manage_all_tenants", "view_all_data", "system_debug", "manage_schemas", "performance_monitoring"]',
 'Asia/Seoul', 'ko', 1, 1, 0);

-- Smart Factory Korea 사용자들
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, first_name, last_name,
    role, department, position, employee_id, permissions,
    timezone, language, phone, is_active, email_verified
) VALUES 
(1, 'sfk_admin', 'admin@smartfactory.co.kr', '$2b$12$SFKAdminPasswordHash123456789012345678901234567890', 
 '김관리자', '관리자', '김', 'company_admin', 'IT', 'IT Manager', 'SFK001',
 '["manage_company", "manage_all_sites", "manage_users", "view_all_data", "manage_alarms"]',
 'Asia/Seoul', 'ko', '+82-10-1234-5678', 1, 1),

(1, 'sfk_engineer', 'engineer@smartfactory.co.kr', '$2b$12$SFKEngineerPasswordHash12345678901234567890123456789012', 
 '박엔지니어', '엔지니어', '박', 'engineer', 'Engineering', 'Senior Engineer', 'SFK002',
 '["manage_devices", "manage_data_points", "view_data", "manage_alarms", "create_virtual_points"]',
 'Asia/Seoul', 'ko', '+82-10-2345-6789', 1, 1),

(1, 'sfk_operator', 'operator@smartfactory.co.kr', '$2b$12$SFKOperatorPasswordHash12345678901234567890123456789012', 
 '이운영자', '운영자', '이', 'operator', 'Operations', 'Line Operator', 'SFK003',
 '["view_data", "control_devices", "acknowledge_alarms", "view_dashboard"]',
 'Asia/Seoul', 'ko', '+82-10-3456-7890', 1, 1);

-- Global Manufacturing 사용자들
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, department, position, permissions,
    timezone, language, is_active, email_verified
) VALUES 
(2, 'gmi_admin', 'admin@globalmfg.com', '$2b$12$GMIAdminPasswordHash123456789012345678901234567890', 
 'John Smith', 'company_admin', 'IT', 'IT Director',
 '["manage_company", "manage_all_sites", "manage_users", "view_all_data", "financial_reports"]',
 'America/New_York', 'en', 1, 1),

(2, 'gmi_engineer', 'engineer@globalmfg.com', '$2b$12$GMIEngineerPasswordHash12345678901234567890123456789012', 
 'Sarah Johnson', 'engineer', 'Engineering', 'Process Engineer',
 '["manage_devices", "manage_data_points", "view_data", "manage_alarms", "create_reports"]',
 'America/New_York', 'en', 1, 1);

-- Demo/Test 사용자들
INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, permissions,
    timezone, language, is_active, email_verified
) VALUES 
(3, 'demo_user', 'demo@pulseone.com', '$2b$12$DemoPasswordHash1234567890123456789012345678901234567890', 
 'Demo User', 'company_admin', 
 '["manage_company", "manage_sites", "manage_users", "view_all_data"]',
 'UTC', 'en', 1, 1),

(4, 'test_user', 'test@testfactory.com', '$2b$12$TestPasswordHash1234567890123456789012345678901234567890', 
 'Test User', 'engineer',
 '["manage_devices", "manage_data_points", "view_data", "create_virtual_points"]',
 'Asia/Seoul', 'ko', 1, 1);

-- =============================================================================
-- 🔥🔥🔥 사이트 생성 (계층적 구조)
-- =============================================================================
INSERT OR IGNORE INTO sites (
    tenant_id, parent_site_id, name, code, site_type, description, location, address,
    coordinates, timezone, manager_name, manager_email, operating_hours, 
    edge_server_id, hierarchy_level, is_active, created_by
) VALUES 
-- Smart Factory Korea 사이트 계층
(1, NULL, 'Smart Factory Korea HQ', 'SFK-HQ', 'company', 'Company Headquarters', 'Seoul, Korea', 
 'Seoul Digital Media City, Mapo-gu, Seoul', '37.5759,126.8955', 'Asia/Seoul',
 'Kim Manager', 'manager@smartfactory.co.kr', '24/7', NULL, 0, 1, 3),

(1, 1, 'Seoul Main Factory', 'SFK-SEOUL', 'factory', 'Main manufacturing facility', 'Seoul Industrial Complex',
 'Seoul Digital Media City Complex A', '37.5759,126.8955', 'Asia/Seoul',
 'Park Engineer', 'seoul@smartfactory.co.kr', '06:00-22:00', 1, 1, 1, 3),

(1, 2, 'Building A', 'SFK-SEOUL-A', 'building', 'Manufacturing Building A', 'Building A',
 'Complex A Building A', '37.5759,126.8955', 'Asia/Seoul',
 'Lee Supervisor', 'buildingA@smartfactory.co.kr', '06:00-22:00', 1, 2, 1, 3),

(1, 3, 'Production Line 1', 'SFK-SEOUL-A-L1', 'line', 'Automotive Parts Production Line 1', 'Line 1',
 'Building A Floor 1', '37.5759,126.8955', 'Asia/Seoul',
 'Choi Operator', 'line1@smartfactory.co.kr', '06:00-22:00', 1, 3, 1, 3),

(1, 1, 'Busan Secondary Plant', 'SFK-BUSAN', 'factory', 'Secondary production facility', 'Busan Industrial Park',
 'Busan Industrial Complex B', '35.1796,129.0756', 'Asia/Seoul',
 'Busan Manager', 'busan@smartfactory.co.kr', '08:00-20:00', 2, 1, 1, 3),

-- Global Manufacturing 사이트 계층
(2, NULL, 'Global Manufacturing HQ', 'GMI-HQ', 'company', 'Global Headquarters', 'New York, USA',
 '123 Manufacturing Ave, New York, NY', '40.7128,-74.0060', 'America/New_York',
 'John Smith', 'hq@globalmfg.com', '24/7', NULL, 0, 1, 5),

(2, 6, 'New York Plant', 'GMI-NY', 'factory', 'East Coast Manufacturing Plant', 'New York Industrial Zone',
 '456 Factory Blvd, New York, NY', '40.7128,-74.0060', 'America/New_York',
 'Sarah Johnson', 'ny@globalmfg.com', '06:00-22:00', 3, 1, 1, 5),

(2, 6, 'Detroit Automotive Plant', 'GMI-DETROIT', 'factory', 'Automotive Manufacturing Plant', 'Detroit, MI',
 '789 Auto Drive, Detroit, MI', '42.3314,-83.0458', 'America/Detroit',
 'Mike Wilson', 'detroit@globalmfg.com', '24/7', 4, 1, 1, 5),

-- Demo/Test 사이트들
(3, NULL, 'Demo Factory', 'DEMO-FACTORY', 'factory', 'Demonstration facility', 'Demo Location',
 'Demo Address', '0.0,0.0', 'UTC',
 'Demo Manager', 'demo@pulseone.com', '24/7', 5, 0, 1, 7),

(4, NULL, 'Test Facility', 'TEST-FACILITY', 'factory', 'Testing and R&D facility', 'Test Location',
 'Test Address', '0.0,0.0', 'Asia/Seoul',
 'Test Manager', 'test@testfactory.com', '09:00-18:00', 6, 0, 1, 8);

-- =============================================================================
-- 🔥🔥🔥 드라이버 플러그인 생성
-- =============================================================================
INSERT OR IGNORE INTO driver_plugins (
    name, protocol_type, version, file_path, description, author,
    config_schema, default_config, is_enabled, is_system, 
    max_concurrent_connections, supported_features
) VALUES 
('Modbus TCP Driver', 'MODBUS_TCP', '2.1.0', '/drivers/modbus_tcp.so', 
 'Standard Modbus TCP/IP protocol driver', 'PulseOne Team',
 '{"timeout_ms": {"type": "integer", "default": 3000}, "slave_id": {"type": "integer", "default": 1}}',
 '{"timeout_ms": 3000, "slave_id": 1, "max_retries": 3}', 1, 1, 50,
 '["read_holding_registers", "read_input_registers", "read_coils", "read_discrete_inputs", "write_single_coil", "write_single_register"]'),

('Modbus RTU Driver', 'MODBUS_RTU', '2.1.0', '/drivers/modbus_rtu.so',
 'Modbus RTU serial communication driver', 'PulseOne Team',
 '{"baudrate": {"type": "integer", "default": 9600}, "parity": {"type": "string", "default": "none"}}',
 '{"baudrate": 9600, "parity": "none", "stop_bits": 1, "data_bits": 8}', 1, 1, 10,
 '["read_holding_registers", "read_input_registers", "read_coils", "read_discrete_inputs", "write_single_coil", "write_single_register"]'),

('MQTT Driver', 'MQTT', '2.1.0', '/drivers/mqtt.so',
 'MQTT publish/subscribe protocol driver', 'PulseOne Team',
 '{"qos": {"type": "integer", "default": 1}, "keep_alive": {"type": "integer", "default": 60}}',
 '{"qos": 1, "keep_alive": 60, "clean_session": true}', 1, 1, 100,
 '["publish", "subscribe", "qos_0", "qos_1", "qos_2", "retained_messages"]'),

('BACnet Driver', 'BACNET', '2.1.0', '/drivers/bacnet.so',
 'BACnet building automation protocol driver', 'PulseOne Team',
 '{"device_id": {"type": "integer", "default": 1001}, "network": {"type": "integer", "default": 1}}',
 '{"device_id": 1001, "network": 1, "port": 47808}', 1, 1, 20,
 '["read_property", "write_property", "cov_subscription", "object_discovery"]'),

('OPC UA Driver', 'OPCUA', '2.1.0', '/drivers/opcua.so',
 'OPC Unified Architecture protocol driver', 'PulseOne Team',
 '{"security_mode": {"type": "string", "default": "none"}, "session_timeout": {"type": "integer", "default": 60000}}',
 '{"security_mode": "none", "session_timeout": 60000, "subscription_interval": 1000}', 0, 1, 30,
 '["read", "write", "subscribe", "browse", "method_call", "security_modes"]');

-- =============================================================================
-- 🔥🔥🔥 디바이스 그룹 생성 
-- =============================================================================
INSERT OR IGNORE INTO device_groups (
    tenant_id, site_id, name, description, group_type, 
    tags, metadata, created_by
) VALUES 
-- Smart Factory Korea 그룹들
(1, 4, 'Production Line A Equipment', 'All equipment on production line A', 'functional',
 '["production", "line_a", "automotive"]', 
 '{"line_speed": "variable", "product_type": "automotive_parts", "shift_pattern": "3_shift"}', 3),

(1, 4, 'HVAC & Building Systems', 'Building automation and HVAC systems', 'functional',
 '["hvac", "building", "automation"]',
 '{"control_type": "automatic", "energy_efficiency": "high", "maintenance_schedule": "monthly"}', 3),

(1, 4, 'Energy Monitoring', 'Power and energy monitoring devices', 'functional',
 '["energy", "power", "monitoring", "billing"]',
 '{"billing_purpose": true, "report_frequency": "daily", "alarm_threshold": 1000}', 3),

-- Global Manufacturing 그룹들
(2, 7, 'Assembly Line Equipment', 'Main assembly line machinery', 'functional',
 '["assembly", "production", "manufacturing"]',
 '{"line_type": "assembly", "automation_level": "high", "capacity": "500_units_per_hour"}', 5),

(2, 8, 'Automotive Production', 'Automotive manufacturing equipment', 'functional',
 '["automotive", "production", "quality"]',
 '{"quality_standards": ["ISO9001", "TS16949"], "production_capacity": "1000_vehicles_per_day"}', 5),

-- Demo/Test 그룹들
(3, 9, 'Demo Equipment', 'Demonstration equipment for testing', 'functional',
 '["demo", "test", "showcase"]',
 '{"demo_purpose": true, "reset_daily": true}', 7),

(4, 10, 'Test Equipment', 'R&D testing equipment', 'functional',
 '["test", "rnd", "development"]',
 '{"test_purpose": true, "experimental": true}', 8);

-- =============================================================================
-- 🔥🔥🔥 새 스키마 완전 호환 디바이스 생성 (실제 산업 환경 시뮬레이션)
-- =============================================================================
INSERT OR IGNORE INTO devices (
    tenant_id, site_id, device_group_id, edge_server_id,
    name, description, device_type, manufacturer, model, serial_number, firmware_version,
    protocol_type, endpoint, config,
    polling_interval, timeout, retry_count, priority,
    tags, metadata, custom_fields, is_enabled, created_by
) VALUES 
-- Smart Factory Korea - Production Line A Equipment
(1, 4, 1, 1, 'PLC-AutoLine-Main', 'Main PLC for automotive production line', 'PLC', 
 'Siemens', 'S7-1515F', 'S7F-2025-001', 'V2.9.4',
 'MODBUS_TCP', '192.168.1.10:502', 
 '{"slave_id": 1, "timeout_ms": 3000, "byte_order": "big_endian", "word_order": "high_low"}',
 1000, 3000, 3, 10,
 '["production", "plc", "main_controller", "safety"]', 
 '{"safety_rating": "SIL3", "io_count": 512, "memory_size": "8MB", "communication_ports": ["MODBUS_TCP", "PROFINET"]}',
 '{"purchase_date": "2025-01-15", "warranty_expires": "2028-01-15", "maintenance_contract": "PREMIUM"}', 1, 3),

(1, 4, 1, 1, 'HMI-AutoLine-Operator', 'Operator HMI panel for line control', 'HMI',
 'Schneider Electric', 'Harmony GT2512', 'GT-2025-002', 'V1.2.3',
 'MODBUS_TCP', '192.168.1.11:502',
 '{"slave_id": 2, "timeout_ms": 3000, "screen_size": "12_inch", "touch_type": "capacitive"}',
 2000, 3000, 3, 20,
 '["hmi", "operator_interface", "touchscreen"]',
 '{"screen_resolution": "1024x768", "brightness": "1000_nits", "operating_temp": "-10_to_60C"}',
 '{"installation_height": "1.5m", "operator_training_required": true}', 1, 3),

(1, 4, 1, 1, 'Robot-Welder-01', 'Automated welding robot #1', 'ROBOT',
 'KUKA', 'KR 16-3 F', 'KUKA-2025-001', 'V8.7.2',
 'MODBUS_TCP', '192.168.1.12:502',
 '{"slave_id": 3, "timeout_ms": 2000, "coordinate_system": "world", "tool_frame": "welding_gun"}',
 500, 2000, 5, 5,
 '["robot", "welding", "automation", "precision"]',
 '{"payload": "16kg", "reach": "1611mm", "repeatability": "±0.05mm", "axes": 6}',
 '{"welding_process": "MIG", "wire_feeder": "external", "safety_zone": "3m_radius"}', 1, 3),

(1, 4, 1, 1, 'VFD-ConveyorMotor', 'Variable frequency drive for conveyor motor', 'INVERTER',
 'ABB', 'ACS580-01', 'ABB-VFD-001', 'V1.8.1',
 'MODBUS_TCP', '192.168.1.13:502',
 '{"slave_id": 4, "timeout_ms": 3000, "motor_rated_power": "15kW", "frequency_range": "0-60Hz"}',
 1000, 3000, 3, 30,
 '["vfd", "motor_control", "conveyor", "speed_control"]',
 '{"motor_voltage": "400V", "motor_current": "30A", "efficiency": "97%", "protection_class": "IP21"}',
 '{"motor_manufacturer": "Siemens", "motor_model": "1LA7 163-4AA91", "belt_type": "timing_belt"}', 1, 3),

-- Smart Factory Korea - HVAC & Building Systems
(1, 4, 2, 1, 'HVAC-AHU-Main', 'Main air handling unit controller', 'CONTROLLER',
 'Honeywell', 'Spyder Programmable', 'HON-AHU-001', 'V2.1.0',
 'BACNET', '192.168.1.30:47808',
 '{"device_id": 1001, "network": 1, "max_apdu": 1476, "segmentation": "both"}',
 5000, 10000, 3, 50,
 '["hvac", "ahu", "building_automation", "air_quality"]',
 '{"air_flow_capacity": "10000_cfm", "filter_type": "HEPA", "heat_recovery": "enthalpy_wheel"}',
 '{"location": "roof_top", "installation_date": "2025-02-01", "filter_change_interval": "quarterly"}', 1, 3),

(1, 4, 3, 1, 'PowerMeter-Main', 'Main facility power meter', 'METER',
 'Schneider Electric', 'PowerLogic PM8000', 'PM8-2025-001', 'V3.2.1',
 'MODBUS_TCP', '192.168.1.40:502',
 '{"slave_id": 10, "timeout_ms": 3000, "measurement_class": "0.2S", "frequency": "50Hz"}',
 5000, 3000, 3, 40,
 '["power_meter", "energy_monitoring", "billing", "main_feed"]',
 '{"voltage_rating": "400V", "current_rating": "5000A", "power_rating": "3.5MW", "accuracy": "±0.2%"}',
 '{"meter_location": "main_electrical_room", "ct_ratio": "5000:5", "pt_ratio": "400:110"}', 1, 3),

-- Global Manufacturing - New York Plant
(2, 7, 4, 3, 'PLC-Assembly-Main', 'Main assembly line PLC', 'PLC',
 'Rockwell Automation', 'CompactLogix 5380', 'RW-CL5380-001', 'V33.11',
 'MODBUS_TCP', '10.0.1.10:502',
 '{"slave_id": 1, "timeout_ms": 3000, "communication_format": "RTU_over_TCP"}',
 1000, 3000, 3, 10,
 '["plc", "assembly", "main_controller", "usa"]',
 '{"processor": "5380", "memory": "32MB", "io_capacity": "256_points", "ethernet_ports": 2}',
 '{"programming_software": "Studio_5000", "hmi_integration": "FactoryTalk", "safety_rating": "SIL2"}', 1, 5),

(2, 7, 4, 3, 'Robot-Assembly-01', 'Assembly robot station 1', 'ROBOT',
 'Fanuc', 'R-2000iD/210F', 'FANUC-R2000-001', 'V9.40P',
 'ETHERNET_IP', '10.0.1.15:44818',
 '{"connection_type": "explicit", "assembly_instance": 100, "config_instance": 101}',
 200, 1000, 5, 5,
 '["robot", "assembly", "fanuc", "high_payload"]',
 '{"payload": "210kg", "reach": "2655mm", "repeatability": "±0.08mm", "axes": 6}',
 '{"end_effector": "pneumatic_gripper", "vision_system": "integrated", "collision_detection": "advanced"}', 1, 5),

-- Demo Equipment
(3, 9, 6, 5, 'Demo-PLC-01', 'Demo PLC for training and showcase', 'PLC',
 'Demo Manufacturer', 'Demo-PLC-v2', 'DEMO-001', 'v2.1.0',
 'MODBUS_TCP', '192.168.100.10:502',
 '{"slave_id": 1, "timeout_ms": 3000, "demo_mode": true}',
 2000, 3000, 3, 100,
 '["demo", "training", "showcase"]',
 '{"demo_purpose": true, "simulation_capable": true, "training_scenarios": 10}',
 '{"reset_schedule": "daily_midnight", "demo_scenarios": ["basic_io", "analog_scaling", "alarms"]}', 1, 7),

(3, 9, 6, 5, 'Demo-IoT-Gateway', 'IoT gateway for wireless sensors', 'GATEWAY',
 'Generic IoT', 'MultiProtocol-GW', 'IOT-GW-001', 'v1.5.0',
 'MQTT', 'mqtt://192.168.100.20:1883',
 '{"client_id": "demo_gateway", "username": "demo_user", "password": "demo_pass", "keep_alive": 60}',
 0, 5000, 3, 50,
 '["iot", "gateway", "wireless", "mqtt"]',
 '{"supported_protocols": ["LoRaWAN", "Zigbee", "WiFi"], "max_sensors": 100, "battery_backup": "8_hours"}',
 '{"sensor_types": ["temperature", "humidity", "pressure", "vibration"], "range": "1km_outdoor"}', 1, 7),

-- Test Equipment
(4, 10, 7, 6, 'Test-PLC-Advanced', 'Advanced test PLC for R&D', 'PLC',
 'Test Systems', 'TestPLC-Pro', 'TEST-PLC-001', 'v3.0.0',
 'MODBUS_TCP', '192.168.200.10:502',
 '{"slave_id": 1, "timeout_ms": 3000, "test_mode": true, "logging_enabled": true}',
 1000, 3000, 3, 10,
 '["test", "rnd", "advanced", "development"]',
 '{"test_purpose": true, "firmware_upgradeable": true, "debug_capabilities": "full"}',
 '{"test_protocols": ["MODBUS_TCP", "MODBUS_RTU", "MQTT"], "development_features": ["script_engine", "data_logger"]}', 1, 8);

-- =============================================================================
-- 🔥🔥🔥 디바이스 설정 생성 (각 디바이스의 고급 통신 설정)
-- =============================================================================
INSERT OR IGNORE INTO device_settings (
    device_id, polling_interval_ms, connection_timeout_ms, read_timeout_ms, write_timeout_ms,
    max_retry_count, retry_interval_ms, backoff_multiplier, max_backoff_time_ms,
    keep_alive_enabled, keep_alive_interval_s, keep_alive_timeout_s,
    data_validation_enabled, detailed_logging_enabled, performance_monitoring_enabled,
    read_buffer_size, write_buffer_size, queue_size, updated_by
) VALUES 
-- PLC 설정 (고성능, 낮은 지연)
(1, 1000, 3000, 2000, 3000, 3, 1000, 1.5, 60000, 1, 30, 10, 1, 0, 1, 2048, 1024, 100, 3),
(7, 1000, 3000, 2000, 3000, 3, 1000, 1.5, 60000, 1, 30, 10, 1, 1, 1, 2048, 1024, 100, 5),
(9, 2000, 5000, 3000, 4000, 3, 2000, 1.5, 120000, 1, 60, 15, 1, 1, 1, 1024, 512, 50, 7),
(11, 1000, 3000, 2000, 3000, 5, 1000, 2.0, 60000, 1, 30, 10, 1, 1, 1, 2048, 1024, 200, 8),

-- HMI 설정 (중간 성능)
(2, 2000, 5000, 3000, 4000, 3, 2000, 1.5, 120000, 1, 60, 15, 1, 0, 1, 1024, 512, 50, 3),

-- 로봇 설정 (고성능, 빠른 응답)
(3, 500, 2000, 1000, 2000, 5, 500, 2.0, 30000, 1, 15, 5, 1, 1, 1, 4096, 2048, 200, 3),
(8, 200, 1000, 500, 1000, 5, 200, 2.0, 15000, 1, 10, 3, 1, 1, 1, 8192, 4096, 500, 5),

-- VFD 설정 (표준 성능)
(4, 1000, 3000, 2000, 3000, 3, 1000, 1.5, 60000, 1, 30, 10, 1, 0, 1, 1024, 512, 100, 3),

-- HVAC 설정 (낮은 빈도, 에너지 효율)
(5, 5000, 10000, 5000, 8000, 3, 5000, 1.5, 300000, 1, 120, 30, 1, 0, 1, 512, 256, 25, 3),

-- 전력 측정기 설정 (정확성 중시)
(6, 5000, 8000, 5000, 6000, 3, 3000, 1.5, 180000, 1, 60, 20, 1, 0, 1, 1024, 512, 50, 3),

-- IoT Gateway 설정 (MQTT, 비동기)
(10, 0, 10000, 5000, 5000, 3, 5000, 1.5, 300000, 1, 60, 20, 1, 0, 1, 2048, 1024, 100, 7);

-- =============================================================================
-- 🔥🔥🔥 새 스키마 완전 호환 데이터 포인트 생성 (모든 필드 포함!)
-- =============================================================================

-- PLC-AutoLine-Main 데이터 포인트들 (Modbus TCP)
INSERT OR IGNORE INTO data_points (
    device_id, name, description, 
    address, address_string,
    data_type, access_mode, is_enabled, is_writable,
    unit, scaling_factor, scaling_offset, min_value, max_value,
    log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
    group_name, tags, metadata, protocol_params,
    alarm_enabled, high_alarm_limit, low_alarm_limit, alarm_deadband
) VALUES 
(1, 'Production_Count', 'Automotive parts production counter', 
 1001, '40001', 
 'UINT32', 'read', 1, 0,
 'pcs', 1.0, 0.0, 0.0, 999999.0,
 1, 5000, 1.0, 1000,
 'production', '["production", "counter", "automotive", "kpi"]', 
 '{"location": "line_1", "criticality": "high", "maintenance_schedule": "monthly", "shift_reset": false}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 1, "byte_order": "big_endian", "word_order": "high_low", "register_type": "holding"}',
 0, NULL, NULL, 0.0),

(1, 'Line_Speed', 'Production line conveyor speed', 
 1002, '40002', 
 'FLOAT32', 'read', 1, 0,
 'm/min', 0.1, 0.0, 0.0, 100.0,
 1, 2000, 0.5, 1000,
 'production', '["speed", "conveyor", "monitoring", "efficiency"]', 
 '{"location": "conveyor_motor", "criticality": "medium", "optimal_range": "15-25", "efficiency_impact": "high"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 1, "byte_order": "big_endian", "register_type": "float32", "scaling_required": true}',
 1, 30.0, 5.0, 1.0),

(1, 'Motor_Current', 'Main conveyor motor current', 
 1003, '40003', 
 'FLOAT32', 'read', 1, 0,
 'A', 0.01, 0.0, 0.0, 50.0,
 1, 1000, 0.1, 500,
 'electrical', '["current", "motor", "electrical", "protection"]', 
 '{"motor_rating": "15kW", "rated_current": "30A", "overload_threshold": "45A", "safety_critical": true}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 1, "byte_order": "big_endian", "register_type": "float32", "safety_monitoring": true}',
 1, 45.0, NULL, 2.0),

(1, 'Process_Temperature', 'Process area ambient temperature', 
 1004, '40004', 
 'FLOAT32', 'read', 1, 0,
 '°C', 0.1, -273.15, -40.0, 150.0,
 1, 3000, 0.2, 1000,
 'environmental', '["temperature", "process", "environmental", "comfort"]', 
 '{"sensor_type": "PT100", "location": "process_area", "criticality": "medium", "calibration_date": "2025-01-01"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 1, "byte_order": "big_endian", "register_type": "float32", "temperature_compensation": true}',
 1, 40.0, 0.0, 1.0),

(1, 'Emergency_Stop', 'Main emergency stop button status', 
 1005, '10001', 
 'BOOL', 'read', 1, 0,
 '', 1.0, 0.0, 0.0, 1.0,
 1, 500, 0.0, 100,
 'safety', '["emergency", "safety", "critical", "shutdown"]', 
 '{"safety_category": "category_4", "location": "main_panel", "criticality": "critical", "immediate_action": true}',
 '{"protocol": "MODBUS_TCP", "function_code": 1, "slave_id": 1, "byte_order": "big_endian", "safety_certified": true}',
 1, NULL, NULL, 0.0),

(1, 'Start_Command', 'Production line start command', 
 1006, '00001', 
 'BOOL', 'read_write', 1, 1,
 '', 1.0, 0.0, 0.0, 1.0,
 1, 1000, 0.0, 500,
 'control', '["control", "start", "operator", "command"]', 
 '{"control_type": "momentary", "location": "operator_panel", "authorization_required": true, "audit_trail": true}',
 '{"protocol": "MODBUS_TCP", "function_code": 1, "slave_id": 1, "byte_order": "big_endian", "write_function": 5, "confirmation_required": true}',
 0, NULL, NULL, 0.0),

(1, 'Cycle_Time', 'Production cycle time per unit', 
 1007, '40005', 
 'FLOAT32', 'read', 1, 0,
 's', 0.1, 0.0, 0.0, 300.0,
 1, 2000, 1.0, 1000,
 'production', '["cycle_time", "efficiency", "kpi", "timing"]', 
 '{"target_cycle_time": "45s", "world_class": "30s", "current_efficiency": "85%", "improvement_potential": "15%"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 1, "byte_order": "big_endian", "register_type": "float32", "statistical_tracking": true}',
 1, 60.0, NULL, 2.0);

-- HMI-AutoLine-Operator 데이터 포인트들
INSERT OR IGNORE INTO data_points (
    device_id, name, description, 
    address, address_string,
    data_type, access_mode, is_enabled, is_writable,
    unit, scaling_factor, scaling_offset, min_value, max_value,
    log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
    group_name, tags, metadata, protocol_params
) VALUES 
(2, 'Screen_Status', 'HMI screen current status', 
 2001, '40001', 
 'UINT16', 'read', 1, 0,
 '', 1.0, 0.0, 0.0, 255.0,
 0, 10000, 1.0, 2000,
 'hmi', '["hmi", "status", "operator", "interface"]', 
 '{"screen_count": 10, "current_screen": "main_overview", "user_level": "operator"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 2, "byte_order": "big_endian", "screen_mapping": "custom"}'),

(2, 'Active_Alarms', 'Number of active alarms on HMI', 
 2002, '40002', 
 'UINT16', 'read', 1, 0,
 'count', 1.0, 0.0, 0.0, 100.0,
 1, 5000, 1.0, 1000,
 'alarms', '["alarms", "count", "monitoring", "hmi"]', 
 '{"alarm_categories": ["critical", "high", "medium", "low"], "auto_acknowledge": false, "alarm_log_size": 1000}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 2, "byte_order": "big_endian", "alarm_integration": true}'),

(2, 'User_Level', 'Current user access level', 
 2003, '40003', 
 'UINT16', 'read', 1, 0,
 '', 1.0, 0.0, 0.0, 5.0,
 1, 30000, 1.0, 5000,
 'security', '["user", "access", "security", "authorization"]', 
 '{"levels": {"0": "viewer", "1": "operator", "2": "engineer", "3": "supervisor", "4": "admin"}}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 2, "byte_order": "big_endian", "security_tracking": true}');

-- Robot-Welder-01 데이터 포인트들
INSERT OR IGNORE INTO data_points (
    device_id, name, description, 
    address, address_string,
    data_type, access_mode, is_enabled, is_writable,
    unit, scaling_factor, scaling_offset, min_value, max_value,
    log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
    group_name, tags, metadata, protocol_params,
    alarm_enabled, high_alarm_limit, low_alarm_limit, alarm_deadband
) VALUES 
(3, 'Robot_X_Position', 'Robot TCP X-axis position', 
 3001, '40001', 
 'FLOAT32', 'read', 1, 0,
 'mm', 0.01, 0.0, -1611.0, 1611.0,
 1, 200, 1.0, 100,
 'position', '["position", "robot", "x_axis", "tcp"]', 
 '{"coordinate_system": "world", "reference_frame": "base", "accuracy": "±0.05mm", "measurement_system": "absolute"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 3, "byte_order": "big_endian", "register_type": "float32", "high_precision": true}',
 1, 1600.0, -1600.0, 5.0),

(3, 'Robot_Y_Position', 'Robot TCP Y-axis position', 
 3003, '40003', 
 'FLOAT32', 'read', 1, 0,
 'mm', 0.01, 0.0, -1611.0, 1611.0,
 1, 200, 1.0, 100,
 'position', '["position", "robot", "y_axis", "tcp"]', 
 '{"coordinate_system": "world", "reference_frame": "base", "accuracy": "±0.05mm", "measurement_system": "absolute"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 3, "byte_order": "big_endian", "register_type": "float32", "high_precision": true}',
 1, 1600.0, -1600.0, 5.0),

(3, 'Robot_Z_Position', 'Robot TCP Z-axis position', 
 3005, '40005', 
 'FLOAT32', 'read', 1, 0,
 'mm', 0.01, 0.0, -1000.0, 2000.0,
 1, 200, 1.0, 100,
 'position', '["position", "robot", "z_axis", "tcp"]', 
 '{"coordinate_system": "world", "reference_frame": "base", "accuracy": "±0.05mm", "measurement_system": "absolute"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 3, "byte_order": "big_endian", "register_type": "float32", "high_precision": true}',
 1, 1950.0, -950.0, 5.0),

(3, 'Welding_Current', 'Welding process current', 
 3007, '40007', 
 'FLOAT32', 'read', 1, 0,
 'A', 0.1, 0.0, 0.0, 350.0,
 1, 100, 2.0, 50,
 'welding', '["welding", "current", "process", "quality"]', 
 '{"welding_process": "MIG", "material": "steel", "wire_diameter": "1.2mm", "shielding_gas": "Ar/CO2"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 3, "byte_order": "big_endian", "register_type": "float32", "process_monitoring": true}',
 1, 300.0, 50.0, 10.0),

(3, 'Welding_Voltage', 'Welding process voltage', 
 3009, '40009', 
 'FLOAT32', 'read', 1, 0,
 'V', 0.1, 0.0, 0.0, 50.0,
 1, 100, 0.5, 50,
 'welding', '["welding", "voltage", "process", "quality"]', 
 '{"welding_process": "MIG", "voltage_type": "arc_voltage", "measurement_point": "torch_tip"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 3, "byte_order": "big_endian", "register_type": "float32", "process_monitoring": true}',
 1, 35.0, 15.0, 2.0),

(3, 'Robot_Status', 'Robot operational status', 
 3011, '40011', 
 'UINT16', 'read', 1, 0,
 '', 1.0, 0.0, 0.0, 10.0,
 1, 1000, 1.0, 500,
 'status', '["status", "robot", "operational", "state"]', 
 '{"states": {"0": "stopped", "1": "manual", "2": "auto", "3": "error", "4": "maintenance"}}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 3, "byte_order": "big_endian", "status_monitoring": true}',
 1, NULL, NULL, 0.0);

-- HVAC-AHU-Main 데이터 포인트들 (BACnet)
INSERT OR IGNORE INTO data_points (
    device_id, name, description, 
    address, address_string,
    data_type, access_mode, is_enabled, is_writable,
    unit, scaling_factor, scaling_offset, min_value, max_value,
    log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
    group_name, tags, metadata, protocol_params,
    alarm_enabled, high_alarm_limit, low_alarm_limit, alarm_deadband
) VALUES 
(5, 'Supply_Air_Temperature', 'AHU supply air temperature', 
 1, 'AI:1', 
 'FLOAT32', 'read', 1, 0,
 '°C', 1.0, 0.0, -10.0, 50.0,
 1, 10000, 0.5, 5000,
 'hvac', '["hvac", "temperature", "supply_air", "comfort"]', 
 '{"sensor_type": "RTD", "location": "supply_duct", "control_loop": "temperature", "setpoint": "22°C"}',
 '{"protocol": "BACNET_IP", "object_type": 0, "object_instance": 1, "property_id": 85, "array_index": -1, "cov_enabled": true}',
 1, 30.0, 10.0, 2.0),

(5, 'Return_Air_Temperature', 'AHU return air temperature', 
 2, 'AI:2', 
 'FLOAT32', 'read', 1, 0,
 '°C', 1.0, 0.0, -10.0, 50.0,
 1, 10000, 0.5, 5000,
 'hvac', '["hvac", "temperature", "return_air", "monitoring"]', 
 '{"sensor_type": "RTD", "location": "return_duct", "differential_control": true}',
 '{"protocol": "BACNET_IP", "object_type": 0, "object_instance": 2, "property_id": 85, "array_index": -1, "cov_enabled": true}',
 1, 35.0, 5.0, 2.0),

(5, 'Supply_Air_Flow', 'AHU supply air flow rate', 
 3, 'AI:3', 
 'FLOAT32', 'read', 1, 0,
 'cfm', 1.0, 0.0, 0.0, 12000.0,
 1, 5000, 100.0, 5000,
 'hvac', '["hvac", "airflow", "supply", "volume"]', 
 '{"measurement_type": "differential_pressure", "design_flow": "10000_cfm", "minimum_flow": "3000_cfm"}',
 '{"protocol": "BACNET_IP", "object_type": 0, "object_instance": 3, "property_id": 85, "array_index": -1, "cov_enabled": false}',
 1, 11000.0, 2000.0, 500.0),

(5, 'Outside_Air_Damper', 'Outside air damper position', 
 4, 'AO:1', 
 'FLOAT32', 'read_write', 1, 1,
 '%', 1.0, 0.0, 0.0, 100.0,
 1, 5000, 2.0, 5000,
 'hvac', '["hvac", "damper", "outside_air", "control"]', 
 '{"control_type": "modulating", "minimum_position": "15%", "economizer_enabled": true}',
 '{"protocol": "BACNET_IP", "object_type": 1, "object_instance": 1, "property_id": 85, "array_index": -1, "priority": 16, "cov_enabled": true}',
 0, NULL, NULL, 0.0);

-- PowerMeter-Main 데이터 포인트들
INSERT OR IGNORE INTO data_points (
    device_id, name, description, 
    address, address_string,
    data_type, access_mode, is_enabled, is_writable,
    unit, scaling_factor, scaling_offset, min_value, max_value,
    log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
    group_name, tags, metadata, protocol_params,
    alarm_enabled, high_alarm_limit, low_alarm_limit
) VALUES 
(6, 'Total_Active_Power', 'Total facility active power consumption', 
 5001, '40001', 
 'FLOAT32', 'read', 1, 0,
 'kW', 1.0, 0.0, 0.0, 4000.0,
 1, 5000, 5.0, 2000,
 'energy', '["energy", "power", "active", "billing"]', 
 '{"measurement_class": "0.2S", "billing_purpose": true, "demand_monitoring": true, "peak_tracking": true}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 10, "byte_order": "big_endian", "register_type": "float32", "energy_calculation": true}',
 1, 3500.0, NULL),

(6, 'Total_Energy_Consumed', 'Cumulative energy consumption', 
 5003, '40003', 
 'FLOAT32', 'read', 1, 0,
 'kWh', 0.01, 0.0, 0.0, 999999999.0,
 1, 15000, 1.0, 5000,
 'energy', '["energy", "consumption", "cumulative", "billing"]', 
 '{"meter_type": "revenue_grade", "accuracy": "±0.2%", "billing_integration": true, "reset_capability": false}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 10, "byte_order": "big_endian", "register_type": "float32", "high_precision": true}',
 0, NULL, NULL),

(6, 'Power_Factor', 'Overall facility power factor', 
 5005, '40005', 
 'FLOAT32', 'read', 1, 0,
 '', 0.001, 0.0, 0.0, 1.0,
 1, 10000, 0.01, 5000,
 'energy', '["power_factor", "efficiency", "quality", "monitoring"]', 
 '{"target_pf": "0.95", "penalty_threshold": "0.85", "correction_available": true}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 10, "byte_order": "big_endian", "register_type": "float32", "power_quality": true}',
 1, NULL, 0.8);

-- Demo IoT Gateway 데이터 포인트들 (MQTT)
INSERT OR IGNORE INTO data_points (
    device_id, name, description, 
    address, address_string,
    data_type, access_mode, is_enabled, is_writable,
    unit, scaling_factor, scaling_offset, min_value, max_value,
    log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
    group_name, tags, metadata, protocol_params
) VALUES 
(10, 'Wireless_Temperature_01', 'Wireless temperature sensor #1', 
 0, 'factory/demo/sensors/temp01', 
 'FLOAT32', 'read', 1, 0,
 '°C', 1.0, 0.0, -20.0, 80.0,
 1, 5000, 0.3, 0,
 'iot_sensors', '["iot", "temperature", "wireless", "demo"]', 
 '{"sensor_id": "TEMP_001", "battery_level": 85, "rssi": -65, "last_seen": "2025-08-14T10:30:00Z"}',
 '{"protocol": "MQTT", "topic": "factory/demo/sensors/temp01", "qos": 1, "retain": false, "message_format": "json", "json_path": "$.temperature"}'),

(10, 'Wireless_Humidity_01', 'Wireless humidity sensor #1', 
 0, 'factory/demo/sensors/hum01', 
 'FLOAT32', 'read', 1, 0,
 '%RH', 1.0, 0.0, 0.0, 100.0,
 1, 5000, 1.0, 0,
 'iot_sensors', '["iot", "humidity", "wireless", "demo"]', 
 '{"sensor_id": "HUM_001", "battery_level": 92, "rssi": -58, "sensor_type": "SHT30"}',
 '{"protocol": "MQTT", "topic": "factory/demo/sensors/hum01", "qos": 1, "retain": false, "message_format": "json", "json_path": "$.humidity"}'),

(10, 'Wireless_Vibration_01', 'Wireless vibration sensor #1', 
 0, 'factory/demo/sensors/vib01', 
 'FLOAT32', 'read', 1, 0,
 'g', 0.001, 0.0, 0.0, 20.0,
 1, 1000, 0.01, 0,
 'iot_sensors', '["iot", "vibration", "condition_monitoring", "demo"]', 
 '{"sensor_id": "VIB_001", "battery_level": 78, "sampling_rate": 1000, "measurement_type": "RMS_acceleration"}',
 '{"protocol": "MQTT", "topic": "factory/demo/sensors/vib01", "qos": 2, "retain": false, "message_format": "json", "json_path": "$.rms_acceleration"}'),

(10, 'Gateway_Status', 'IoT gateway connection status', 
 0, 'factory/demo/gateway/status', 
 'UINT16', 'read', 1, 0,
 '', 1.0, 0.0, 0.0, 10.0,
 1, 30000, 1.0, 0,
 'gateway', '["gateway", "status", "connectivity", "monitoring"]', 
 '{"connected_sensors": 25, "total_capacity": 100, "uptime": "99.5%", "firmware_version": "v1.5.0"}',
 '{"protocol": "MQTT", "topic": "factory/demo/gateway/status", "qos": 1, "retain": true, "message_format": "json", "json_path": "$.status"}');

-- =============================================================================
-- 🔥🔥🔥 현재값 초기화 (JSON 방식, 실제 센서 값 시뮬레이션)
-- =============================================================================
INSERT OR IGNORE INTO current_values (
    point_id, current_value, value_type, raw_value, 
    quality_code, quality, value_timestamp, last_read_time,
    read_count, write_count, error_count, change_count,
    alarm_state, alarm_active
) VALUES 
-- PLC-AutoLine-Main 현재값들 (생산 라인 데이터)
(1, '{"value": 15847}', 'uint32', '{"value": 15847}', 1, 'good', datetime('now', '-5 minutes'), datetime('now'), 2856, 0, 0, 425, 'normal', 0),
(2, '{"value": 18.5}', 'float', '{"value": 185}', 1, 'good', datetime('now', '-1 minute'), datetime('now'), 1456, 0, 0, 89, 'normal', 0),
(3, '{"value": 28.7}', 'float', '{"value": 2870}', 1, 'good', datetime('now', '-30 seconds'), datetime('now'), 7234, 0, 0, 156, 'normal', 0),
(4, '{"value": 23.8}', 'float', '{"value": 238}', 1, 'good', datetime('now', '-2 minutes'), datetime('now'), 856, 0, 0, 34, 'normal', 0),
(5, '{"value": false}', 'bool', '{"value": 0}', 1, 'good', datetime('now', '-10 seconds'), datetime('now'), 12456, 0, 0, 2, 'normal', 0),
(6, '{"value": false}', 'bool', '{"value": 0}', 1, 'good', datetime('now', '-5 seconds'), datetime('now'), 2145, 15, 0, 45, 'normal', 0),
(7, '{"value": 42.3}', 'float', '{"value": 423}', 1, 'good', datetime('now', '-1 minute'), datetime('now'), 1234, 0, 0, 67, 'normal', 0),

-- HMI 현재값들
(8, '{"value": 1}', 'uint16', '{"value": 1}', 1, 'good', datetime('now', '-30 seconds'), datetime('now'), 156, 0, 0, 12, 'normal', 0),
(9, '{"value": 2}', 'uint16', '{"value": 2}', 1, 'good', datetime('now', '-15 seconds'), datetime('now'), 456, 0, 0, 8, 'normal', 0),
(10, '{"value": 2}', 'uint16', '{"value": 2}', 1, 'good', datetime('now', '-45 seconds'), datetime('now'), 234, 0, 0, 3, 'normal', 0),

-- Robot 현재값들 (정밀 위치 데이터)
(11, '{"value": 145.67}', 'float', '{"value": 14567}', 1, 'good', datetime('now', '-5 seconds'), datetime('now'), 18456, 0, 0, 1567, 'normal', 0),
(12, '{"value": -287.23}', 'float', '{"value": -28723}', 1, 'good', datetime('now', '-5 seconds'), datetime('now'), 18456, 0, 0, 1567, 'normal', 0),
(13, '{"value": 856.45}', 'float', '{"value": 85645}', 1, 'good', datetime('now', '-5 seconds'), datetime('now'), 18456, 0, 0, 1567, 'normal', 0),
(14, '{"value": 185.4}', 'float', '{"value": 1854}', 1, 'good', datetime('now', '-2 seconds'), datetime('now'), 45678, 0, 0, 2345, 'normal', 0),
(15, '{"value": 24.8}', 'float', '{"value": 248}', 1, 'good', datetime('now', '-2 seconds'), datetime('now'), 45678, 0, 0, 2345, 'normal', 0),
(16, '{"value": 2}', 'uint16', '{"value": 2}', 1, 'good', datetime('now', '-10 seconds'), datetime('now'), 8567, 0, 0, 234, 'normal', 0),

-- HVAC 현재값들
(17, '{"value": 22.3}', 'float', '{"value": 22.3}', 1, 'good', datetime('now', '-3 minutes'), datetime('now'), 234, 0, 0, 15, 'normal', 0),
(18, '{"value": 25.1}', 'float', '{"value": 25.1}', 1, 'good', datetime('now', '-3 minutes'), datetime('now'), 234, 0, 0, 12, 'normal', 0),
(19, '{"value": 8750.0}', 'float', '{"value": 8750.0}', 1, 'good', datetime('now', '-2 minutes'), datetime('now'), 456, 0, 0, 23, 'normal', 0),
(20, '{"value": 35.5}', 'float', '{"value": 35.5}', 1, 'good', datetime('now', '-1 minute'), datetime('now'), 123, 8, 0, 45, 'normal', 0),

-- 전력 측정기 현재값들  
(21, '{"value": 2847.5}', 'float', '{"value": 2847.5}', 1, 'good', datetime('now', '-30 seconds'), datetime('now'), 567, 0, 0, 34, 'normal', 0),
(22, '{"value": 1458962.75}', 'float', '{"value": 145896275}', 1, 'good', datetime('now', '-5 minutes'), datetime('now'), 289, 0, 0, 1, 'normal', 0),
(23, '{"value": 0.892}', 'float', '{"value": 892}', 1, 'good', datetime('now', '-1 minute'), datetime('now'), 345, 0, 0, 12, 'warning', 1),

-- IoT 센서들 현재값들 (시뮬레이션된 무선 센서 데이터)
(24, '{"value": 24.8}', 'float', '{"value": 24.8}', 1, 'good', datetime('now', '-2 minutes'), datetime('now'), 456, 0, 0, 23, 'normal', 0),
(25, '{"value": 58.2}', 'float', '{"value": 58.2}', 1, 'good', datetime('now', '-2 minutes'), datetime('now'), 445, 0, 0, 18, 'normal', 0),
(26, '{"value": 0.025}', 'float', '{"value": 25}', 1, 'good', datetime('now', '-30 seconds'), datetime('now'), 1234, 0, 0, 67, 'normal', 0),
(27, '{"value": 1}', 'uint16', '{"value": 1}', 1, 'good', datetime('now', '-1 minute'), datetime('now'), 89, 0, 0, 5, 'normal', 0);

-- =============================================================================
-- 🔥🔥🔥 샘플 가상포인트 생성 (JavaScript 계산)
-- =============================================================================
INSERT OR IGNORE INTO virtual_points (
    tenant_id, scope_type, site_id, name, description, formula, data_type, unit, category,
    calculation_interval, calculation_trigger, execution_type,
    tags, metadata, created_by
) VALUES 
(1, 'site', 4, 'Production_Efficiency', 'Overall production line efficiency calculation', 
 'const actualProduction = getValue("Production_Count"); const targetProduction = 20000; return (actualProduction / targetProduction) * 100;', 
 'float', '%', 'production',
 5000, 'timer', 'javascript',
 '["efficiency", "kpi", "production", "oee"]', 
 '{"target_oee": 85, "world_class": 95, "calculation_method": "actual_vs_target"}', 3),

(1, 'site', 4, 'Energy_Cost_Per_Unit', 'Energy cost per produced unit', 
 'const power = getValue("Total_Active_Power"); const production = getValue("Production_Count"); const costPerKwh = 0.12; return (power * costPerKwh) / production;', 
 'float', '$/unit', 'energy',
 10000, 'timer', 'javascript',
 '["energy", "cost", "efficiency", "sustainability"]', 
 '{"cost_currency": "USD", "rate_per_kwh": 0.12, "target_cost_per_unit": 0.05}', 3),

(1, 'site', 4, 'Overall_Equipment_Effectiveness', 'OEE calculation based on availability, performance, quality', 
 'const availability = 0.95; const performance = getValue("Line_Speed") / 25.0; const quality = 0.98; return availability * performance * quality * 100;', 
 'float', '%', 'production',
 5000, 'timer', 'javascript',
 '["oee", "kpi", "manufacturing", "performance"]', 
 '{"target_oee": 85, "availability_target": 95, "performance_target": 100, "quality_target": 98}', 3),

-- Global Manufacturing 가상포인트
(2, 'site', 7, 'Assembly_Line_Throughput', 'Assembly line throughput calculation', 
 'const cycleTime = 45; const efficiency = 0.85; return (3600 / cycleTime) * efficiency;', 
 'float', 'units/hour', 'production',
 5000, 'timer', 'javascript',
 '["throughput", "assembly", "performance"]', 
 '{"target_throughput": 72, "cycle_time_target": 45}', 5),

-- Demo 가상포인트
(3, 'site', 9, 'Demo_Performance_Index', 'Demo performance calculation for training', 
 'return Math.sin(Date.now() / 10000) * 50 + 75;', 
 'float', '%', 'demo',
 2000, 'timer', 'javascript',
 '["demo", "training", "simulation"]', 
 '{"demo_purpose": true, "pattern": "sine_wave", "base_value": 75, "amplitude": 50}', 7);

-- =============================================================================
-- 🔥🔥🔥 가상포인트 입력 매핑
-- =============================================================================
INSERT OR IGNORE INTO virtual_point_inputs (
    virtual_point_id, variable_name, source_type, source_id, description
) VALUES 
-- Production_Efficiency 입력
(1, 'production_count', 'data_point', 1, 'Current production count'),
(1, 'line_speed', 'data_point', 2, 'Current line speed'),

-- Energy_Cost_Per_Unit 입력
(2, 'total_power', 'data_point', 21, 'Total active power consumption'),
(2, 'production_count', 'data_point', 1, 'Current production count'),

-- OEE 입력
(3, 'line_speed', 'data_point', 2, 'Current line speed'),
(3, 'cycle_time', 'data_point', 7, 'Current cycle time');

-- =============================================================================
-- 🔥🔥🔥 샘플 알람 규칙 생성
-- =============================================================================
INSERT OR IGNORE INTO alarm_rules (
    tenant_id, name, description, target_type, target_id, alarm_type, severity,
    high_limit, low_limit, deadband, message_template, notification_enabled,
    is_enabled, created_by
) VALUES 
-- Smart Factory Korea 알람들
(1, 'Motor Current High', 'Conveyor motor current too high - potential overload', 'data_point', 3, 'analog', 'high',
 40.0, NULL, 2.0, 'Motor current is {value}A, exceeding limit of {threshold}A', 1, 1, 3),

(1, 'Production Line Speed Low', 'Production line speed below minimum threshold', 'data_point', 2, 'analog', 'medium',
 NULL, 10.0, 1.0, 'Line speed is {value}m/min, below minimum of {threshold}m/min', 1, 1, 3),

(1, 'Emergency Stop Activated', 'Emergency stop button has been activated', 'data_point', 5, 'digital', 'critical',
 NULL, NULL, 0.0, 'EMERGENCY STOP ACTIVATED - Line shutdown required', 1, 1, 3),

(1, 'HVAC Supply Temperature High', 'Supply air temperature exceeding comfort range', 'data_point', 17, 'analog', 'medium',
 28.0, NULL, 1.0, 'Supply air temperature {value}°C exceeds comfort limit', 1, 1, 3),

(1, 'Power Factor Low', 'Facility power factor below penalty threshold', 'data_point', 23, 'analog', 'low',
 NULL, 0.85, 0.02, 'Power factor {value} below penalty threshold of {threshold}', 1, 1, 3),

-- Global Manufacturing 알람들
(2, 'Robot Position Limit', 'Robot approaching workspace limits', 'data_point', 11, 'analog', 'medium',
 1500.0, -1500.0, 10.0, 'Robot X position {value}mm approaching workspace limit', 1, 1, 5),

-- Demo 알람들 (교육용)
(3, 'Demo Temperature High', 'Demo temperature sensor high reading', 'data_point', 24, 'analog', 'low',
 35.0, NULL, 1.0, 'Demo temperature {value}°C exceeds {threshold}°C', 1, 1, 7);

-- =============================================================================
-- 🔥🔥🔥 스크립트 라이브러리 초기 함수들
-- =============================================================================
INSERT OR IGNORE INTO script_library (
    tenant_id, category, name, display_name, description, script_code, 
    parameters, return_type, tags, example_usage, is_system, is_active
) VALUES 
-- 시스템 제공 수학 함수들
(0, 'math', 'average', 'Average Calculator', 'Calculate average of multiple values',
 'function average(...values) { return values.reduce((a, b) => a + b, 0) / values.length; }',
 '[{"name": "values", "type": "number[]", "required": true, "description": "Array of numbers to average"}]',
 'number', '["math", "statistics", "average"]', 
 'average(10, 20, 30) // Returns 20', 1, 1),

(0, 'math', 'movingAverage', 'Moving Average', 'Calculate moving average over time window',
 'function movingAverage(currentValue, previousAverage, windowSize) { return ((previousAverage * (windowSize - 1)) + currentValue) / windowSize; }',
 '[{"name": "currentValue", "type": "number", "required": true}, {"name": "previousAverage", "type": "number", "required": true}, {"name": "windowSize", "type": "number", "required": true}]',
 'number', '["math", "filtering", "smoothing"]',
 'movingAverage(25.5, 24.8, 10) // Returns smoothed value', 1, 1),

(0, 'engineering', 'oeeCalculation', 'OEE Calculator', 'Calculate Overall Equipment Effectiveness',
 'function oeeCalculation(availability, performance, quality) { return (availability / 100) * (performance / 100) * (quality / 100) * 100; }',
 '[{"name": "availability", "type": "number", "required": true, "description": "Availability percentage"}, {"name": "performance", "type": "number", "required": true, "description": "Performance percentage"}, {"name": "quality", "type": "number", "required": true, "description": "Quality percentage"}]',
 'number', '["oee", "manufacturing", "kpi"]',
 'oeeCalculation(95, 87, 98) // Returns OEE percentage', 1, 1),

(0, 'conversion', 'celsiusToFahrenheit', 'Temperature Conversion', 'Convert Celsius to Fahrenheit',
 'function celsiusToFahrenheit(celsius) { return (celsius * 9/5) + 32; }',
 '[{"name": "celsius", "type": "number", "required": true, "description": "Temperature in Celsius"}]',
 'number', '["conversion", "temperature"]',
 'celsiusToFahrenheit(25) // Returns 77', 1, 1),

-- Smart Factory Korea 전용 함수들
(1, 'custom', 'productionEfficiency', 'Production Efficiency', 'Calculate production efficiency for automotive line',
 'function productionEfficiency(actualCount, targetCount, operatingHours) { const efficiency = (actualCount / targetCount) * 100; const hourlyRate = actualCount / operatingHours; return { efficiency: efficiency, hourlyRate: hourlyRate }; }',
 '[{"name": "actualCount", "type": "number", "required": true}, {"name": "targetCount", "type": "number", "required": true}, {"name": "operatingHours", "type": "number", "required": true}]',
 'object', '["production", "efficiency", "automotive"]',
 'productionEfficiency(850, 1000, 8) // Returns efficiency metrics', 0, 1);

-- =============================================================================
-- 🔥🔥🔥 사용자 알림 설정
-- =============================================================================
INSERT OR IGNORE INTO user_notification_settings (
    user_id, email_enabled, sms_enabled, push_enabled,
    alarm_notifications, system_notifications, maintenance_notifications,
    quiet_hours_start, quiet_hours_end, weekend_notifications
) VALUES 
(3, 1, 0, 1, 1, 1, 1, '22:00', '06:00', 0),  -- sfk_admin
(4, 1, 1, 1, 1, 1, 0, '23:00', '07:00', 0),  -- sfk_engineer  
(5, 1, 0, 1, 1, 0, 0, '22:00', '06:00', 0),  -- sfk_operator
(6, 1, 0, 1, 1, 1, 1, '18:00', '08:00', 1),  -- gmi_admin
(7, 1, 1, 1, 1, 1, 0, '19:00', '07:00', 0);  -- gmi_engineer

-- =============================================================================
-- 🔥🔥🔥 초기 성능 로그 (시스템 상태)
-- =============================================================================
INSERT OR IGNORE INTO performance_logs (
    metric_category, metric_name, metric_value, metric_unit,
    hostname, component, details, timestamp
) VALUES 
-- 시스템 성능 메트릭
('system', 'cpu_usage', 25.5, '%', 'edge-sfk-seoul-01.local', 'collector', 
 '{"cores": 4, "load_avg": [1.2, 1.1, 1.0]}', datetime('now', '-5 minutes')),
('system', 'memory_usage', 68.2, '%', 'edge-sfk-seoul-01.local', 'collector',
 '{"total_mb": 8192, "used_mb": 5588, "available_mb": 2604}', datetime('now', '-5 minutes')),
('system', 'disk_usage', 45.8, '%', 'edge-sfk-seoul-01.local', 'collector',
 '{"total_gb": 500, "used_gb": 229, "available_gb": 271}', datetime('now', '-5 minutes')),

-- 데이터베이스 성능
('database', 'query_response_time', 12.5, 'ms', 'edge-sfk-seoul-01.local', 'database',
 '{"query_type": "select", "table": "current_values", "rows_affected": 1500}', datetime('now', '-3 minutes')),
('database', 'connection_count', 15, 'count', 'edge-sfk-seoul-01.local', 'database',
 '{"max_connections": 100, "active_connections": 15}', datetime('now', '-3 minutes')),

-- 네트워크 성능
('network', 'modbus_response_time', 45.2, 'ms', 'edge-sfk-seoul-01.local', 'collector',
 '{"protocol": "MODBUS_TCP", "device_count": 6, "success_rate": 99.2}', datetime('now', '-2 minutes')),
('network', 'mqtt_message_rate', 125.5, 'msg/sec', 'edge-sfk-seoul-01.local', 'collector',
 '{"broker": "192.168.100.20", "subscriptions": 4, "qos_distribution": {"0": 20, "1": 75, "2": 5}}', datetime('now', '-2 minutes'));

-- =============================================================================
-- 📝 초기 데이터 로딩 완료 로그
-- =============================================================================
INSERT OR IGNORE INTO system_logs (
    log_level, module, message, details, created_at
) VALUES 
('INFO', 'database', 'Initial data loading completed successfully', 
 '{"tables_populated": 15, "sample_devices": 11, "sample_points": 27, "virtual_points": 5, "alarm_rules": 7}',
 datetime('now'));

-- =============================================================================
-- 🎉 초기 데이터 로딩 완료!
-- =============================================================================
-- 이 파일을 실행하면 다음이 생성됩니다:
-- ✅ 4개 테넌트 (Smart Factory Korea, Global Manufacturing, Demo, Test)
-- ✅ 6개 Edge 서버 (분산 아키텍처)
-- ✅ 8명 사용자 (시스템 관리자 + 테넌트 사용자들)
-- ✅ 10개 사이트 (계층적 구조)
-- ✅ 5개 드라이버 플러그인
-- ✅ 7개 디바이스 그룹
-- ✅ 11개 실제 산업용 디바이스 (PLC, HMI, 로봇, VFD, HVAC, 전력측정기 등)
-- ✅ 27개 데이터 포인트 (모든 새 컬럼 포함)
-- ✅ 27개 현재값 (실시간 시뮬레이션 데이터)
-- ✅ 5개 가상포인트 (JavaScript 계산)
-- ✅ 7개 알람 규칙 (실제 산업 시나리오)
-- ✅ 6개 스크립트 라이브러리 함수
-- ✅ 사용자 알림 설정
-- ✅ 성능 모니터링 로그
-- 
-- 🚀 이제 PulseOne v2.1.0이 완전히 작동할 준비가 되었습니다!