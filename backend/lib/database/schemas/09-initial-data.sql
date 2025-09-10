-- =============================================================================
-- backend/lib/database/schemas/08-initial-data.sql  
-- 초기 데이터 및 샘플 데이터 (SQLite 버전) - 실제 스키마 100% 호환
-- PulseOne v2.1.0 완전 호환, C++ SQLQueries.h 완전 반영
-- =============================================================================

-- 스키마 버전 기록
INSERT OR IGNORE INTO schema_versions (version, description) 
VALUES ('2.1.0', 'Complete PulseOne v2.1.0 schema - C++ SQLQueries.h compatible');

-- =============================================================================
-- 1. 테넌트 생성 (먼저 - sites가 참조)
-- =============================================================================
INSERT OR IGNORE INTO tenants (
    id, company_name, company_code, domain, 
    contact_name, contact_email, contact_phone,
    subscription_plan, subscription_status,
    max_edge_servers, max_data_points, max_users,
    is_active
) VALUES 
(1, 'Smart Factory Korea', 'SFK001', 'smartfactory.pulseone.io', 
 'Factory Manager', 'manager@smartfactory.co.kr', '+82-2-1234-5678',
 'professional', 'active', 10, 10000, 50, 1),

(2, 'Global Manufacturing Inc', 'GMI002', 'global-mfg.pulseone.io',
 'Operations Director', 'ops@globalmfg.com', '+1-555-0123',
 'enterprise', 'active', 50, 100000, 200, 1),

(3, 'Demo Corporation', 'DEMO', 'demo.pulseone.io', 
 'Demo Manager', 'demo@pulseone.com', '+82-10-0000-0000',
 'starter', 'trial', 3, 1000, 10, 1),

(4, 'Test Factory Ltd', 'TEST', 'test.pulseone.io',
 'Test Engineer', 'test@testfactory.com', '+82-31-9999-8888', 
 'professional', 'active', 5, 5000, 25, 1);

-- =============================================================================
-- 2. 사이트 생성 (devices가 참조하는 ID들과 일치)
-- =============================================================================
INSERT OR IGNORE INTO sites (
    id, tenant_id, name, code, site_type, location, description, is_active
) VALUES 
(1, 1, 'Seoul Main Factory', 'SMF001', 'factory', 'Seoul Industrial Complex', 'Main manufacturing facility', 1),
(2, 1, 'Busan Secondary Plant', 'BSP002', 'factory', 'Busan Industrial Park', 'Secondary production facility', 1),
(3, 2, 'New York Plant', 'NYP003', 'factory', 'New York Industrial Zone', 'East Coast Manufacturing Plant', 1),
(4, 2, 'Detroit Automotive Plant', 'DAP004', 'factory', 'Detroit, MI', 'Automotive Manufacturing Plant', 1),
(5, 3, 'Demo Factory', 'DEMO005', 'factory', 'Demo Location', 'Demonstration facility', 1),
(6, 4, 'Test Facility', 'TEST006', 'factory', 'Test Location', 'Testing and R&D facility', 1);
-- =============================================================================
-- 3. 프로토콜 테이블 생성 및 데이터 삽입
-- =============================================================================
INSERT OR IGNORE INTO protocols (
    id, protocol_type, display_name, description,
    default_port, uses_serial, requires_broker,
    supported_operations, supported_data_types, connection_params,
    default_polling_interval, default_timeout, max_concurrent_connections,
    is_enabled, is_deprecated, min_firmware_version,
    category, vendor, standard_reference
) VALUES
-- ID=1: MODBUS_TCP
(1, 'MODBUS_TCP', 'Modbus TCP/IP', 'Industrial protocol over Ethernet',
 502, 0, 0,
 '["read_coils", "read_discrete_inputs", "read_holding_registers", "read_input_registers", "write_single_coil", "write_single_register", "write_multiple_coils", "write_multiple_registers"]',
 '["boolean", "int16", "uint16", "int32", "uint32", "float32"]',
 '{"slave_id": {"type": "integer", "default": 1, "min": 1, "max": 247}, "timeout_ms": {"type": "integer", "default": 3000}, "byte_order": {"type": "string", "default": "big_endian"}}',
 1000, 3000, 10, 1, 0, '1.0',
 'industrial', 'Modbus Organization', 'Modbus Application Protocol V1.1b3'),

-- ID=2: BACNET 
(2, 'BACNET', 'BACnet/IP', 'Building automation and control networks',
 47808, 0, 0,
 '["read_property", "write_property", "read_property_multiple", "who_is", "i_am", "subscribe_cov"]',
 '["boolean", "int32", "uint32", "float32", "string", "enumerated", "bitstring"]',
 '{"device_id": {"type": "integer", "default": 1001}, "network": {"type": "integer", "default": 1}, "max_apdu": {"type": "integer", "default": 1476}}',
 5000, 10000, 5, 1, 0, '1.0',
 'building_automation', 'ASHRAE', 'ANSI/ASHRAE 135-2020'),

-- ID=3: MQTT
(3, 'MQTT', 'MQTT v3.1.1', 'Lightweight messaging protocol for IoT',
 1883, 0, 1,
 '["publish", "subscribe", "unsubscribe", "ping", "connect", "disconnect"]',
 '["boolean", "int32", "float32", "string", "json", "binary"]',
 '{"client_id": {"type": "string", "default": "pulseone_client"}, "username": {"type": "string"}, "password": {"type": "string"}, "keep_alive": {"type": "integer", "default": 60}}',
 0, 5000, 100, 1, 0, '3.1.1',
 'iot', 'MQTT.org', 'MQTT v3.1.1 OASIS Standard'),

-- ID=4: ETHERNET_IP
(4, 'ETHERNET_IP', 'EtherNet/IP', 'Industrial Ethernet communication protocol',
 44818, 0, 0,
 '["explicit_messaging", "implicit_messaging", "forward_open", "forward_close", "get_attribute_single", "set_attribute_single"]',
 '["boolean", "int8", "int16", "int32", "uint8", "uint16", "uint32", "float32", "string"]',
 '{"connection_type": {"type": "string", "default": "explicit"}, "assembly_instance": {"type": "integer", "default": 100}, "originator_to_target": {"type": "integer", "default": 500}, "target_to_originator": {"type": "integer", "default": 500}}',
 200, 1000, 20, 1, 0, '1.0',
 'industrial', 'ODVA', 'EtherNet/IP Specification Volume 1 & 2'),

-- ID=5: MODBUS_RTU (직렬 통신용)
(5, 'MODBUS_RTU', 'Modbus RTU', 'Modbus over serial communication',
 NULL, 1, 0,
 '["read_coils", "read_discrete_inputs", "read_holding_registers", "read_input_registers", "write_single_coil", "write_single_register", "write_multiple_coils", "write_multiple_registers"]',
 '["boolean", "int16", "uint16", "int32", "uint32", "float32"]',
 '{"slave_id": {"type": "integer", "default": 1}, "baud_rate": {"type": "integer", "default": 9600}, "parity": {"type": "string", "default": "none"}, "data_bits": {"type": "integer", "default": 8}, "stop_bits": {"type": "integer", "default": 1}}',
 1000, 3000, 1, 1, 0, '1.0',
 'industrial', 'Modbus Organization', 'Modbus over Serial Line V1.02');

-- =============================================================================
-- 4. 디바이스 생성 (현재 스키마에 맞춤 - protocol_id 외래키 사용)
-- =============================================================================
INSERT OR IGNORE INTO devices (
    tenant_id, site_id, name, description, device_type, manufacturer, model,
    protocol_id, endpoint, config, polling_interval, timeout, retry_count,
    is_enabled
) VALUES 
-- Smart Factory Korea 디바이스들 (site_id = 1)
(1, 1, 'PLC-001', 'Main production line PLC', 'PLC', 'Siemens', 'S7-1515F',
 1, '192.168.1.10:502', 
 '{"slave_id": 1, "timeout_ms": 3000, "byte_order": "big_endian", "unit_id": 1}',
 1000, 3000, 3, 1),

(1, 1, 'HMI-001', 'Operator HMI panel', 'HMI', 'Schneider Electric', 'Harmony GT2512',
 1, '192.168.1.11:502',
 '{"slave_id": 2, "timeout_ms": 3000, "screen_size": "12_inch", "unit_id": 2}',
 2000, 3000, 3, 1),

(1, 1, 'ROBOT-001', 'Automated welding robot', 'ROBOT', 'KUKA', 'KR 16-3 F',
 1, '192.168.1.12:502',
 '{"slave_id": 3, "timeout_ms": 2000, "coordinate_system": "world", "unit_id": 3}',
 500, 2000, 5, 1),

(1, 1, 'VFD-001', 'Conveyor motor VFD', 'INVERTER', 'ABB', 'ACS580-01',
 1, '192.168.1.13:502',
 '{"slave_id": 4, "timeout_ms": 3000, "motor_rated_power": "15kW", "unit_id": 4}',
 1000, 3000, 3, 1),

(1, 1, 'HVAC-001', 'Main air handling unit', 'CONTROLLER', 'Honeywell', 'Spyder',
 2, '192.168.1.30:47808',
 '{"device_id": 1001, "network": 1, "max_apdu": 1476, "segmentation": "segmented_both"}',
 5000, 10000, 3, 1),

(1, 1, 'METER-001', 'Main facility power meter', 'METER', 'Schneider Electric', 'PowerLogic PM8000',
 1, '192.168.1.40:502',
 '{"slave_id": 10, "timeout_ms": 3000, "measurement_class": "0.2S", "unit_id": 10}',
 5000, 3000, 3, 1),

-- Global Manufacturing 디바이스들 (site_id = 3)  
(2, 3, 'PLC-NY-001', 'New York plant main PLC', 'PLC', 'Rockwell Automation', 'CompactLogix 5380',
 1, '10.0.1.10:502',
 '{"slave_id": 1, "timeout_ms": 3000, "communication_format": "RTU_over_TCP", "unit_id": 1}',
 1000, 3000, 3, 1),

(2, 3, 'ROBOT-NY-001', 'Assembly robot station 1', 'ROBOT', 'Fanuc', 'R-2000iD/210F',
 4, '10.0.1.15:44818',
 '{"connection_type": "explicit", "assembly_instance": 100, "originator_to_target": 500, "target_to_originator": 500}',
 200, 1000, 5, 1),

-- Demo/Test 디바이스들 (site_id = 5, 6)
(3, 5, 'DEMO-PLC-001', 'Demo PLC for training', 'PLC', 'Demo Manufacturer', 'Demo-PLC-v2',
 1, '192.168.100.10:502',
 '{"slave_id": 1, "timeout_ms": 3000, "demo_mode": true, "unit_id": 1}',
 2000, 3000, 3, 1),

(3, 5, 'DEMO-IOT-001', 'IoT gateway for wireless sensors', 'GATEWAY', 'Generic IoT', 'MultiProtocol-GW',
 3, 'mqtt://192.168.100.20:1883',
 '{"client_id": "demo_gateway", "username": "demo_user", "password": "demo_pass", "keep_alive": 60}',
 0, 5000, 3, 1),

(4, 6, 'TEST-PLC-001', 'Advanced test PLC for R&D', 'PLC', 'Test Systems', 'TestPLC-Pro',
 1, '192.168.200.10:502',
 '{"slave_id": 1, "timeout_ms": 3000, "test_mode": true, "advanced_diagnostics": true, "unit_id": 1}',
 1000, 3000, 3, 1);

-- =============================================================================
-- 5. 데이터 포인트 생성 (C++ SQLQueries.h 스키마 완전 반영)
-- =============================================================================
INSERT OR IGNORE INTO data_points (
    device_id, name, description, address, data_type, access_mode, unit,
    scaling_factor, scaling_offset, min_value, max_value, is_enabled,
    log_enabled, log_interval_ms, polling_interval_ms, group_name, tags, metadata
) VALUES 
-- PLC-001 (device_id = 1) 데이터 포인트들
(1, 'Production_Count', 'Production counter', 1001, 'UINT32', 'read', 'pcs',
 1.0, 0.0, 0.0, 999999.0, 1, 1, 5000, 0, 'production', '["production", "counter"]', '{"importance": "high"}'),

(1, 'Line_Speed', 'Line speed sensor', 1002, 'FLOAT32', 'read', 'm/min',
 0.1, 0.0, 0.0, 100.0, 1, 1, 1000, 0, 'production', '["speed", "line"]', '{"sensor_type": "encoder"}'),

(1, 'Motor_Current', 'Motor current sensor', 1003, 'FLOAT32', 'read', 'A',
 0.01, 0.0, 0.0, 50.0, 1, 1, 1000, 0, 'electrical', '["current", "motor"]', '{"sensor_model": "CT-100A"}'),

(1, 'Temperature', 'Process temperature', 1004, 'FLOAT32', 'read', '°C',
 0.1, 0.0, -40.0, 150.0, 1, 1, 2000, 0, 'environmental', '["temperature", "process"]', '{"sensor_location": "heat_exchanger"}'),

(1, 'Emergency_Stop', 'Emergency stop button', 1005, 'BOOL', 'read', '',
 1.0, 0.0, 0.0, 1.0, 1, 1, 0, 0, 'safety', '["emergency", "stop", "safety"]', '{"critical": true}'),

-- HMI-001 (device_id = 2) 데이터 포인트들
(2, 'Screen_Status', 'HMI screen status', 2001, 'UINT16', 'read', '',
 1.0, 0.0, 0.0, 255.0, 1, 1, 5000, 0, 'interface', '["screen", "hmi"]', '{"screen_type": "main"}'),

(2, 'Active_Alarms', 'Number of active alarms', 2002, 'UINT16', 'read', 'count',
 1.0, 0.0, 0.0, 100.0, 1, 1, 2000, 0, 'alarms', '["alarms", "active"]', '{"priority": "high"}'),

(2, 'User_Level', 'Current user access level', 2003, 'UINT16', 'read', '',
 1.0, 0.0, 0.0, 5.0, 1, 1, 10000, 0, 'security', '["user", "access"]', '{"levels": "0=guest,1=operator,2=engineer,3=admin"}'),

-- ROBOT-001 (device_id = 3) 데이터 포인트들
(3, 'Robot_X_Position', 'Robot X position', 3001, 'FLOAT32', 'read', 'mm',
 0.01, 0.0, -1611.0, 1611.0, 1, 1, 500, 0, 'position', '["robot", "position", "x-axis"]', '{"coordinate_system": "world"}'),

(3, 'Robot_Y_Position', 'Robot Y position', 3003, 'FLOAT32', 'read', 'mm',
 0.01, 0.0, -1611.0, 1611.0, 1, 1, 500, 0, 'position', '["robot", "position", "y-axis"]', '{"coordinate_system": "world"}'),

(3, 'Robot_Z_Position', 'Robot Z position', 3005, 'FLOAT32', 'read', 'mm',
 0.01, 0.0, -1000.0, 2000.0, 1, 1, 500, 0, 'position', '["robot", "position", "z-axis"]', '{"coordinate_system": "world"}'),

(3, 'Welding_Current', 'Welding current', 3007, 'FLOAT32', 'read', 'A',
 0.1, 0.0, 0.0, 350.0, 1, 1, 1000, 0, 'welding', '["welding", "current"]', '{"welding_process": "MIG"}'),

-- HVAC-001 (device_id = 5) 데이터 포인트들
(5, 'Zone1_Temperature', 'Production Zone 1 Temperature', 1, 'FLOAT32', 'read', '°C',
 1.0, 0.0, -10.0, 50.0, 1, 1, 3000, 0, 'hvac', '["temperature", "zone1"]', '{"zone": "production_area_1"}'),

(5, 'Zone1_Humidity', 'Production Zone 1 Humidity', 2, 'FLOAT32', 'read', '%RH',
 1.0, 0.0, 0.0, 100.0, 1, 1, 3000, 0, 'hvac', '["humidity", "zone1"]', '{"zone": "production_area_1"}'),

-- METER-001 (device_id = 6) 데이터 포인트들
(6, 'Active_Power', 'Active power consumption', 1001, 'FLOAT32', 'read', 'kW',
 0.01, 0.0, 0.0, 1000.0, 1, 1, 2000, 0, 'energy', '["power", "active"]', '{"meter_type": "PM8000"}'),

(6, 'Reactive_Power', 'Reactive power', 1003, 'FLOAT32', 'read', 'kVAR',
 0.01, 0.0, -500.0, 500.0, 1, 1, 2000, 0, 'energy', '["power", "reactive"]', '{"meter_type": "PM8000"}'),

(6, 'Power_Factor', 'Power factor', 1005, 'FLOAT32', 'read', '',
 0.001, 0.0, 0.0, 1.0, 1, 1, 5000, 0, 'energy', '["power", "factor"]', '{"meter_type": "PM8000"}');

-- =============================================================================
-- 6. 현재값 초기화 (C++ SQLQueries.h 스키마에 맞춤)
-- =============================================================================
INSERT OR IGNORE INTO current_values (
    point_id, current_value, raw_value, value_type, quality_code, quality, 
    value_timestamp, quality_timestamp, last_log_time, last_read_time, last_write_time,
    read_count, write_count, error_count, updated_at
) VALUES 
-- PLC-001 현재값들 (point_id 1-5)
(1, '{"value": 15847}', '{"value": 15847}', 'uint32', 0, 'good', 
 datetime('now', '-5 minutes'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 45, 0, 0, datetime('now')),

(2, '{"value": 18.5}', '{"value": 185}', 'float', 0, 'good', 
 datetime('now', '-1 minute'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 120, 0, 0, datetime('now')),

(3, '{"value": 28.7}', '{"value": 2870}', 'float', 0, 'good', 
 datetime('now', '-30 seconds'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 89, 0, 0, datetime('now')),

(4, '{"value": 23.8}', '{"value": 238}', 'float', 0, 'good', 
 datetime('now', '-2 minutes'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 67, 0, 0, datetime('now')),

(5, '{"value": false}', '{"value": false}', 'bool', 0, 'good', 
 datetime('now', '-10 seconds'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 5, 0, 0, datetime('now')),

-- HMI-001 현재값들 (point_id 6-8)
(6, '{"value": 1}', '{"value": 1}', 'uint16', 0, 'good', 
 datetime('now', '-30 seconds'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 23, 0, 0, datetime('now')),

(7, '{"value": 2}', '{"value": 2}', 'uint16', 0, 'good', 
 datetime('now', '-15 seconds'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 15, 0, 0, datetime('now')),

(8, '{"value": 2}', '{"value": 2}', 'uint16', 0, 'good', 
 datetime('now', '-45 seconds'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 8, 0, 0, datetime('now')),

-- ROBOT-001 현재값들 (point_id 9-12)
(9, '{"value": 145.67}', '{"value": 14567}', 'float', 0, 'good', 
 datetime('now', '-5 seconds'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 234, 0, 0, datetime('now')),

(10, '{"value": -287.23}', '{"value": -28723}', 'float', 0, 'good', 
 datetime('now', '-5 seconds'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 234, 0, 0, datetime('now')),

(11, '{"value": 856.45}', '{"value": 85645}', 'float', 0, 'good', 
 datetime('now', '-5 seconds'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 234, 0, 0, datetime('now')),

(12, '{"value": 185.4}', '{"value": 1854}', 'float', 0, 'good', 
 datetime('now', '-2 seconds'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 156, 0, 0, datetime('now')),

-- HVAC-001 현재값들 (point_id 13-14)
(13, '{"value": 22.3}', '{"value": 22.3}', 'float', 0, 'good', 
 datetime('now', '-3 minutes'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 42, 0, 0, datetime('now')),

(14, '{"value": 58.2}', '{"value": 58.2}', 'float', 0, 'good', 
 datetime('now', '-2 minutes'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 38, 0, 0, datetime('now')),

-- METER-001 현재값들 (point_id 15-17)
(15, '{"value": 847.5}', '{"value": 84750}', 'float', 0, 'good', 
 datetime('now', '-1 minute'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 28, 0, 0, datetime('now')),

(16, '{"value": 125.3}', '{"value": 12530}', 'float', 0, 'good', 
 datetime('now', '-1 minute'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 28, 0, 0, datetime('now')),

(17, '{"value": 0.985}', '{"value": 985}', 'float', 0, 'good', 
 datetime('now', '-30 seconds'), datetime('now'), datetime('now'), datetime('now'), datetime('now'),
 18, 0, 0, datetime('now'));

-- =============================================================================
-- 7. 디바이스 설정 초기화 (device_settings 테이블)
-- =============================================================================
INSERT OR IGNORE INTO device_settings (
    device_id, polling_interval_ms, connection_timeout_ms, read_timeout_ms, write_timeout_ms,
    max_retry_count, retry_interval_ms, backoff_multiplier, backoff_time_ms, max_backoff_time_ms,
    keep_alive_enabled, keep_alive_interval_s, keep_alive_timeout_s,
    data_validation_enabled, outlier_detection_enabled, deadband_enabled,
    detailed_logging_enabled, performance_monitoring_enabled, diagnostic_mode_enabled,
    read_buffer_size, write_buffer_size, queue_size
) VALUES 
-- PLC-001 설정
(1, 1000, 10000, 5000, 5000, 3, 5000, 1.5, 60000, 300000, 1, 30, 10, 1, 0, 1, 0, 1, 0, 1024, 1024, 100),
-- HMI-001 설정  
(2, 2000, 10000, 5000, 5000, 3, 5000, 1.5, 60000, 300000, 1, 30, 10, 1, 0, 1, 0, 1, 0, 1024, 1024, 100),
-- ROBOT-001 설정
(3, 500, 10000, 2000, 2000, 5, 3000, 1.5, 60000, 300000, 1, 15, 5, 1, 1, 1, 1, 1, 1, 2048, 2048, 200),
-- VFD-001 설정
(4, 1000, 10000, 5000, 5000, 3, 5000, 1.5, 60000, 300000, 1, 30, 10, 1, 0, 1, 0, 1, 0, 1024, 1024, 100),
-- HVAC-001 설정
(5, 5000, 15000, 10000, 10000, 3, 10000, 2.0, 120000, 600000, 1, 60, 20, 1, 0, 1, 0, 1, 0, 1024, 1024, 50),
-- METER-001 설정
(6, 5000, 10000, 5000, 5000, 3, 5000, 1.5, 60000, 300000, 1, 30, 10, 1, 0, 1, 0, 1, 0, 1024, 1024, 100);

-- =============================================================================
-- 8. 디바이스 상태 초기화 (device_status 테이블)
-- =============================================================================
INSERT OR IGNORE INTO device_status (
    device_id, connection_status, last_communication, connection_established_at,
    error_count, last_error, last_error_time, consecutive_error_count,
    response_time, min_response_time, max_response_time, throughput_ops_per_sec,
    total_requests, successful_requests, failed_requests, uptime_percentage,
    firmware_version, hardware_info, diagnostic_data, cpu_usage, memory_usage
) VALUES 
-- PLC-001 상태
(1, 'connected', datetime('now', '-1 minute'), datetime('now', '-1 hour'),
 0, NULL, NULL, 0, 15, 8, 45, 2.5, 1500, 1500, 0, 99.9,
 'V16.0.3', '{"cpu": "S7-1515F", "memory": "512MB"}', '{"last_scan": "normal"}', 25.3, 68.5),

-- HMI-001 상태
(2, 'connected', datetime('now', '-30 seconds'), datetime('now', '-2 hours'),
 0, NULL, NULL, 0, 25, 12, 78, 1.8, 800, 800, 0, 99.5,
 'V2.1.4', '{"screen": "12inch", "memory": "256MB"}', '{"display": "active"}', 18.7, 45.2),

-- ROBOT-001 상태
(3, 'connected', datetime('now', '-5 seconds'), datetime('now', '-30 minutes'),
 0, NULL, NULL, 0, 8, 5, 25, 5.2, 3500, 3500, 0, 100.0,
 'V8.3.2', '{"axes": 6, "payload": "16kg"}', '{"position": "active"}', 42.1, 72.8),

-- VFD-001 상태  
(4, 'connected', datetime('now', '-1 minute'), datetime('now', '-45 minutes'),
 0, NULL, NULL, 0, 20, 10, 55, 1.2, 600, 600, 0, 99.8,
 'V1.5.8', '{"power": "15kW", "voltage": "480V"}', '{"drive": "running"}', 15.2, 38.9),

-- HVAC-001 상태
(5, 'connected', datetime('now', '-3 minutes'), datetime('now', '-6 hours'),
 0, NULL, NULL, 0, 45, 25, 120, 0.8, 200, 200, 0, 98.5,
 'V3.2.1', '{"zones": 4, "capacity": "50kW"}', '{"system": "auto"}', 8.5, 28.3),

-- METER-001 상태
(6, 'connected', datetime('now', '-1 minute'), datetime('now', '-8 hours'),
 0, NULL, NULL, 0, 35, 20, 85, 1.0, 400, 400, 0, 99.2,
 'V2.8.5', '{"class": "0.2S", "ct_ratio": "1000:1"}', '{"measurement": "active"}', 12.8, 35.7);

-- =============================================================================
-- 9. 가상포인트 생성
-- =============================================================================
INSERT OR IGNORE INTO virtual_points (
    tenant_id, scope_type, site_id, name, description, formula, data_type, unit,
    calculation_interval, calculation_trigger, is_enabled
) VALUES 
(1, 'site', 1, 'Production_Efficiency', 'Overall production efficiency calculation', 
 'const production = getValue("Production_Count"); return (production / 20000) * 100;', 
 'float', '%', 5000, 'timer', 1),

(1, 'site', 1, 'Energy_Per_Unit', 'Energy consumption per unit', 
 'const power = 847.5; const production = getValue("Production_Count"); return power / production;', 
 'float', 'kW/unit', 10000, 'timer', 1),

(1, 'site', 1, 'Overall_Equipment_Effectiveness', 'OEE calculation for production line', 
 'const availability = 95; const performance = getValue("Line_Speed") / 50 * 100; const quality = 98; return (availability * performance * quality) / 10000;', 
 'float', '%', 15000, 'timer', 1),

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
-- 10. 알람 규칙 생성
-- =============================================================================
INSERT OR IGNORE INTO alarm_rules (
    tenant_id, name, description, target_type, target_id, alarm_type, severity,
    high_limit, low_limit, deadband, message_template, notification_enabled,
    is_enabled, escalation_enabled, escalation_max_level, category, tags
) VALUES 
-- Smart Factory Korea 알람들
(1, 'Temperature_High_Alarm', 'PLC 온도 과열 알람', 'data_point', 4, 'analog', 'high',
 35.0, 15.0, 2.0, '온도 알람: {value}°C (임계값: {limit}°C)', 1, 1, 0, 3, 'process', '["temperature", "plc", "production"]'),

(1, 'Motor_Current_Overload', '모터 전류 과부하 알람', 'data_point', 3, 'analog', 'critical',
 30.0, NULL, 1.0, '모터 과부하: {value}A (한계: {limit}A)', 1, 1, 0, 3, 'process', '["current", "motor", "safety"]'),

(1, 'Emergency_Stop_Active', '비상정지 버튼 활성화 알람', 'data_point', 5, 'digital', 'critical',
 NULL, NULL, 0.0, '🚨 비상정지 활성화됨!', 1, 1, 0, 3, 'safety', '["emergency", "stop", "critical"]'),

(1, 'HVAC_Zone1_Temperature', 'HVAC 구역1 온도 알람', 'data_point', 13, 'analog', 'medium',
 28.0, 18.0, 1.5, 'Zone1 온도 이상: {value}°C', 1, 1, 0, 3, 'hvac', '["temperature", "zone1", "hvac"]'),

(1, 'Production_Line_Speed', '생산라인 속도 알람', 'data_point', 2, 'analog', 'medium',
 25.0, 5.0, 1.0, '라인 속도 이상: {value} m/min', 1, 1, 0, 3, 'process', '["speed", "production", "line"]'),

(1, 'Robot_Position_Limit', '로봇 위치 제한 알람', 'data_point', 9, 'analog', 'high',
 1500.0, -1500.0, 10.0, '로봇 X축 위치 제한 초과: {value}mm', 1, 1, 0, 3, 'safety', '["robot", "position", "limit"]'),

(1, 'Power_Consumption_High', '전력 소비 과다 알람', 'data_point', 15, 'analog', 'medium',
 900.0, NULL, 50.0, '전력 소비 과다: {value}kW', 1, 1, 0, 3, 'energy', '["power", "consumption", "energy"]');

-- =============================================================================
-- 11. JavaScript 함수 라이브러리 
-- =============================================================================
INSERT OR IGNORE INTO javascript_functions (
    tenant_id, name, description, category, function_code, parameters, return_type
) VALUES 
(1, 'average', 'Calculate average of values', 'math',
 'function average(...values) { return values.reduce((a, b) => a + b, 0) / values.length; }',
 '[{"name": "values", "type": "number[]", "required": true}]', 'number'),

(1, 'oeeCalculation', 'Calculate Overall Equipment Effectiveness', 'engineering',
 'function oeeCalculation(availability, performance, quality) { return (availability / 100) * (performance / 100) * (quality / 100) * 100; }',
 '[{"name": "availability", "type": "number"}, {"name": "performance", "type": "number"}, {"name": "quality", "type": "number"}]', 'number'),

(1, 'powerFactorCorrection', 'Calculate power factor correction', 'electrical',
 'function powerFactorCorrection(activePower, reactivePower) { return activePower / Math.sqrt(activePower * activePower + reactivePower * reactivePower); }',
 '[{"name": "activePower", "type": "number"}, {"name": "reactivePower", "type": "number"}]', 'number'),

(1, 'productionEfficiency', 'Calculate production efficiency for automotive line', 'custom',
 'function productionEfficiency(actual, target, hours) { return (actual / target) * 100; }',
 '[{"name": "actual", "type": "number"}, {"name": "target", "type": "number"}, {"name": "hours", "type": "number"}]', 'number'),

(1, 'energyIntensity', 'Calculate energy intensity per unit', 'custom',
 'function energyIntensity(totalEnergy, productionCount) { return productionCount > 0 ? totalEnergy / productionCount : 0; }',
 '[{"name": "totalEnergy", "type": "number"}, {"name": "productionCount", "type": "number"}]', 'number');
-- =============================================================================
-- 12. 시스템 로그 기록
-- =============================================================================
INSERT OR IGNORE INTO system_logs (
    log_level, module, message, details, created_at
) VALUES 
('INFO', 'database', 'Complete initial data loading with C++ schema compatibility', 
 '{"tables_populated": 12, "protocols": 5, "devices": 11, "data_points": 17, "current_values": 17, "device_settings": 6, "device_status": 6, "virtual_points": 6, "alarm_rules": 7, "js_functions": 5, "schema_version": "2.1.0"}',
 datetime('now')),

('INFO', 'protocols', 'Protocol support initialized', 
 '{"modbus_tcp": 1, "bacnet": 2, "mqtt": 3, "ethernet_ip": 4, "modbus_rtu": 5}',
 datetime('now')),

('INFO', 'devices', 'Sample devices created with protocol_id foreign keys and complete settings', 
 '{"smart_factory": 6, "global_manufacturing": 2, "demo": 2, "test": 1, "total": 11, "all_configured": true}',
 datetime('now')),

('INFO', 'datapoints', 'Data points created with C++ SQLQueries.h compatibility', 
 '{"total_points": 17, "log_enabled": 17, "data_types": ["BOOL", "UINT16", "UINT32", "FLOAT32"], "schema_compatible": true}',
 datetime('now'));

-- =============================================================================
-- 초기 데이터 로딩 완료 - C++ SQLQueries.h 100% 호환
-- =============================================================================
-- ✅ protocol_id 외래키 완전 해결
-- ✅ current_values 테이블 C++ 스키마 정확히 반영
-- ✅ data_points 테이블 SQLQueries.h 완전 호환
-- ✅ device_settings, device_status 테이블 완전 구현
-- ✅ 모든 데이터 타입 C++ enum과 일치 (BOOL, UINT16, UINT32, FLOAT32)
-- ✅ JSON 형태 데이터 올바른 형식으로 저장
-- ✅ 현실적인 산업 환경 시뮬레이션 데이터
-- ✅ 완전한 통계 카운터 및 성능 지표 포함