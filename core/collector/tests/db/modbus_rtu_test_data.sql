-- =============================================================================
-- 🧹 ModbusRTU 테스트 데이터 정리 및 재삽입 SQL
-- PulseOne 시스템 - 실제 스키마에 맞춰 수정됨
-- =============================================================================

-- 1️⃣ 기존 잘못 들어간 RTU 데이터 삭제 (중복 제거)
DELETE FROM devices WHERE name LIKE 'RTU-%' AND protocol_type = 'MODBUS_RTU';

-- 2️⃣ ModbusRTU 디바이스 추가 (실제 스키마 기준)
INSERT INTO devices (
    tenant_id, site_id, device_group_id, edge_server_id,
    name, description, device_type, manufacturer, model, serial_number,
    protocol_type, endpoint, config,
    is_enabled, installation_date, created_by, created_at, updated_at
) VALUES 
-- 🔧 RTU 디바이스 #1: 시리얼 온도 센서
(
    1, 1, 1, 1,
    'RTU-TEMP-001', 'RTU Temperature Sensor Array', 'SENSOR', 'Schneider Electric', 'PM8000', 'RTU001',
    'MODBUS_RTU', '/dev/ttyUSB0', '{"slave_id": 5, "baud_rate": 9600, "parity": "N", "data_bits": 8, "stop_bits": 1, "timeout": 2000}',
    1, '2025-01-01', 1, datetime('now'), datetime('now')
),
-- 🔧 RTU 디바이스 #2: 시리얼 압력 센서  
(
    1, 1, 1, 1,
    'RTU-PRESSURE-001', 'RTU Pressure Monitor', 'SENSOR', 'ABB', 'ACS580', 'RTU002', 
    'MODBUS_RTU', '/dev/ttyUSB1', '{"slave_id": 8, "baud_rate": 19200, "parity": "E", "data_bits": 8, "stop_bits": 1, "frame_delay_ms": 100, "timeout": 3000}',
    1, '2025-01-01', 1, datetime('now'), datetime('now')
),
-- 🔧 RTU 디바이스 #3: 시리얼 유량계
(
    1, 1, 1, 1,
    'RTU-FLOW-001', 'RTU Flow Meter', 'FLOWMETER', 'Yokogawa', 'ADMAG AXF', 'RTU003',
    'MODBUS_RTU', '/dev/ttyRS485', '{"slave_id": 12, "baud_rate": 38400, "parity": "O", "data_bits": 7, "stop_bits": 2, "timeout": 1500}',
    1, '2025-01-01', 1, datetime('now'), datetime('now')
);

-- 3️⃣ ModbusRTU 디바이스 설정 추가 (실제 스키마: device_settings)
INSERT INTO device_settings (
    device_id, polling_interval_ms, connection_timeout_ms, max_retry_count,
    retry_interval_ms, backoff_time_ms, keep_alive_enabled, keep_alive_interval_s,
    updated_at
) VALUES
-- RTU-TEMP-001 설정 (느린 폴링)
(
    (SELECT id FROM devices WHERE name = 'RTU-TEMP-001'),
    5000, 15000, 5, 10000, 60000, 1, 30, datetime('now')
),
-- RTU-PRESSURE-001 설정 (중간 폴링)
(
    (SELECT id FROM devices WHERE name = 'RTU-PRESSURE-001'),
    3000, 12000, 3, 8000, 45000, 1, 25, datetime('now')
),
-- RTU-FLOW-001 설정 (빠른 폴링)
(
    (SELECT id FROM devices WHERE name = 'RTU-FLOW-001'),
    2000, 10000, 4, 6000, 30000, 1, 20, datetime('now')
);

-- 4️⃣ ModbusRTU DataPoints 추가 (실제 스키마에 맞춤)
INSERT INTO data_points (
    device_id, name, description, address, address_string, data_type,
    access_mode, unit, scaling_factor, scaling_offset,
    min_value, max_value, is_enabled, is_writable, polling_interval_ms,
    log_enabled, log_interval_ms, log_deadband,
    group_name, tags, metadata, protocol_params,
    created_at, updated_at
) VALUES

-- 🌡️ RTU-TEMP-001 DataPoints (온도 센서 배열)
(
    (SELECT id FROM devices WHERE name = 'RTU-TEMP-001'),
    'Zone1_Temperature', 'Production Zone 1 Temperature', 2001, '2001', 'FLOAT32',
    'read', '°C', 0.1, 0.0,
    -50.0, 150.0, 1, 0, 5000,
    1, 5000, 0.1,
    'temperature_sensors', '["temperature", "zone1", "production"]', 
    '{"sensor_type": "PT100", "location": "Zone1", "calibration_date": "2025-01-01"}',
    '{"slave_id": 5, "function_code": 3, "protocol": "MODBUS_RTU", "byte_order": "big_endian"}',
    datetime('now'), datetime('now')
),
(
    (SELECT id FROM devices WHERE name = 'RTU-TEMP-001'),
    'Zone2_Temperature', 'Production Zone 2 Temperature', 2002, '2002', 'FLOAT32',
    'read', '°C', 0.1, 0.0,
    -50.0, 150.0, 1, 0, 5000,
    1, 5000, 0.1,
    'temperature_sensors', '["temperature", "zone2", "production"]',
    '{"sensor_type": "PT100", "location": "Zone2", "calibration_date": "2025-01-01"}',
    '{"slave_id": 5, "function_code": 3, "protocol": "MODBUS_RTU", "byte_order": "big_endian"}',
    datetime('now'), datetime('now')
),
(
    (SELECT id FROM devices WHERE name = 'RTU-TEMP-001'),
    'Ambient_Temperature', 'Ambient Temperature', 2003, '2003', 'FLOAT32',
    'read', '°C', 0.1, 0.0,
    -30.0, 80.0, 1, 0, 5000,
    1, 5000, 0.2,
    'temperature_sensors', '["temperature", "ambient"]',
    '{"sensor_type": "RTD", "location": "Ambient", "calibration_date": "2025-01-01"}',
    '{"slave_id": 5, "function_code": 3, "protocol": "MODBUS_RTU", "byte_order": "big_endian"}',
    datetime('now'), datetime('now')
),

-- 💨 RTU-PRESSURE-001 DataPoints (압력 모니터)
(
    (SELECT id FROM devices WHERE name = 'RTU-PRESSURE-001'),
    'Inlet_Pressure', 'System Inlet Pressure', 3001, '3001', 'FLOAT32',
    'read', 'bar', 0.01, 0.0,
    0.0, 16.0, 1, 0, 3000,
    1, 3000, 0.01,
    'pressure_sensors', '["pressure", "inlet", "system"]',
    '{"sensor_type": "4-20mA", "range": "0-16bar", "accuracy": "0.1%"}',
    '{"slave_id": 8, "function_code": 3, "protocol": "MODBUS_RTU", "byte_order": "big_endian"}',
    datetime('now'), datetime('now')
),
(
    (SELECT id FROM devices WHERE name = 'RTU-PRESSURE-001'),
    'Outlet_Pressure', 'System Outlet Pressure', 3002, '3002', 'FLOAT32',
    'read', 'bar', 0.01, 0.0,
    0.0, 16.0, 1, 0, 3000,
    1, 3000, 0.01,
    'pressure_sensors', '["pressure", "outlet", "system"]',
    '{"sensor_type": "4-20mA", "range": "0-16bar", "accuracy": "0.1%"}',
    '{"slave_id": 8, "function_code": 3, "protocol": "MODBUS_RTU", "byte_order": "big_endian"}',
    datetime('now'), datetime('now')
),

-- 🌊 RTU-FLOW-001 DataPoints (유량계)
(
    (SELECT id FROM devices WHERE name = 'RTU-FLOW-001'),
    'Instantaneous_Flow', 'Real-time Flow Rate', 4001, '4001', 'FLOAT32',
    'read', 'L/min', 0.001, 0.0,
    0.0, 1000.0, 1, 0, 2000,
    1, 2000, 0.1,
    'flow_sensors', '["flow", "realtime", "liquid"]',
    '{"sensor_type": "electromagnetic", "pipe_diameter": "50mm", "accuracy": "0.2%"}',
    '{"slave_id": 12, "function_code": 3, "protocol": "MODBUS_RTU", "byte_order": "big_endian"}',
    datetime('now'), datetime('now')
),
(
    (SELECT id FROM devices WHERE name = 'RTU-FLOW-001'),
    'Total_Volume', 'Cumulative Volume', 4002, '4002', 'UINT32',
    'read', 'L', 1.0, 0.0,
    0.0, 999999999.0, 1, 0, 2000,
    1, 10000, 1.0,
    'flow_sensors', '["flow", "totalizer", "cumulative"]',
    '{"counter_type": "totalizer", "reset_capability": false}',
    '{"slave_id": 12, "function_code": 3, "protocol": "MODBUS_RTU", "byte_order": "big_endian", "register_count": 2}',
    datetime('now'), datetime('now')
);

-- 5️⃣ ModbusRTU CurrentValues 추가 (실제 스키마: current_values)
INSERT INTO current_values (
    point_id, current_value, raw_value, value_type,
    quality_code, quality, value_timestamp, quality_timestamp,
    last_log_time, last_read_time, 
    read_count, write_count, error_count,
    updated_at
) VALUES

-- RTU-TEMP-001 CurrentValues
(
    (SELECT id FROM data_points WHERE name = 'Zone1_Temperature'),
    '{"value": 45.3}', '{"value": 453}', 'double',
    1, 'good', datetime('now'), datetime('now'),
    datetime('now'), datetime('now'),
    456, 0, 2, datetime('now')
),
(
    (SELECT id FROM data_points WHERE name = 'Zone2_Temperature'),
    '{"value": 42.8}', '{"value": 428}', 'double',
    1, 'good', datetime('now'), datetime('now'),
    datetime('now'), datetime('now'),
    455, 0, 1, datetime('now')
),
(
    (SELECT id FROM data_points WHERE name = 'Ambient_Temperature'),
    '{"value": 24.1}', '{"value": 241}', 'double',
    1, 'good', datetime('now'), datetime('now'),
    datetime('now'), datetime('now'),
    458, 0, 0, datetime('now')
),

-- RTU-PRESSURE-001 CurrentValues
(
    (SELECT id FROM data_points WHERE name = 'Inlet_Pressure'),
    '{"value": 8.45}', '{"value": 845}', 'double',
    1, 'good', datetime('now'), datetime('now'),
    datetime('now'), datetime('now'),
    234, 0, 3, datetime('now')
),
(
    (SELECT id FROM data_points WHERE name = 'Outlet_Pressure'),
    '{"value": 7.92}', '{"value": 792}', 'double',
    1, 'good', datetime('now'), datetime('now'),
    datetime('now'), datetime('now'),
    233, 0, 2, datetime('now')
),

-- RTU-FLOW-001 CurrentValues  
(
    (SELECT id FROM data_points WHERE name = 'Instantaneous_Flow'),
    '{"value": 156.789}', '{"value": 156789}', 'double',
    1, 'good', datetime('now'), datetime('now'),
    datetime('now'), datetime('now'),
    789, 0, 5, datetime('now')
),
(
    (SELECT id FROM data_points WHERE name = 'Total_Volume'),
    '{"value": 1234567}', '{"value": 1234567}', 'uint32',
    1, 'good', datetime('now'), datetime('now'),
    datetime('now'), datetime('now'),
    788, 0, 1, datetime('now')
);

-- =============================================================================
-- 🔍 검증 쿼리들
-- =============================================================================

-- 6️⃣ 추가된 ModbusRTU 디바이스 확인
SELECT 
    id, name, protocol_type, endpoint, config
FROM devices 
WHERE protocol_type = 'MODBUS_RTU'
ORDER BY name;

-- 7️⃣ ModbusRTU DataPoints 확인
SELECT 
    d.name as device_name,
    dp.name as point_name,
    dp.address,
    dp.data_type,
    dp.unit,
    dp.protocol_params
FROM devices d
JOIN data_points dp ON d.id = dp.device_id
WHERE d.protocol_type = 'MODBUS_RTU'
ORDER BY d.name, dp.address;

-- 8️⃣ ModbusRTU CurrentValues 확인 (실제 스키마 기준)
SELECT 
    d.name as device_name,
    dp.name as point_name,
    cv.current_value,
    cv.quality_code,
    cv.read_count
FROM devices d
JOIN data_points dp ON d.id = dp.device_id
JOIN current_values cv ON dp.id = cv.point_id
WHERE d.protocol_type = 'MODBUS_RTU'
ORDER BY d.name, dp.name;

-- 9️⃣ 전체 프로토콜 통계
SELECT 
    protocol_type,
    COUNT(*) as device_count,
    COUNT(CASE WHEN is_enabled = 1 THEN 1 END) as enabled_count
FROM devices 
GROUP BY protocol_type
ORDER BY protocol_type;

-- 🔟 RTU 디바이스별 설정 확인
SELECT 
    d.name as device_name,
    ds.polling_interval_ms,
    ds.connection_timeout_ms,
    ds.max_retry_count
FROM devices d
JOIN device_settings ds ON d.id = ds.device_id
WHERE d.protocol_type = 'MODBUS_RTU'
ORDER BY d.name;