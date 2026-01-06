-- =============================================================================
-- PulseOne Test Data - Protocol v2.0 Schema (protocol_id)
-- =============================================================================

-- 1️⃣ Create protocols table if not exists
CREATE TABLE IF NOT EXISTS protocols (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(50) NOT NULL UNIQUE,
    display_name VARCHAR(100),
    protocol_type VARCHAR(20) NOT NULL, -- 'serial', 'tcp', 'udp', 'mqtt', 'bacnet'
    default_port INTEGER,
    description TEXT,
    is_enabled INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 2️⃣ Insert protocol definitions
INSERT OR IGNORE INTO protocols (id, name, display_name, protocol_type, default_port, description) VALUES
(1, 'MODBUS_TCP', 'Modbus TCP/IP', 'tcp', 502, 'Modbus over TCP/IP'),
(2, 'MODBUS_RTU', 'Modbus RTU', 'serial', NULL, 'Modbus RTU over Serial'),
(3, 'MQTT', 'MQTT Protocol', 'mqtt', 1883, 'MQTT Message Queue'),
(4, 'BACNET', 'BACnet Protocol', 'udp', 47808, 'BACnet/IP Protocol'),
(5, 'OPCUA', 'OPC UA', 'tcp', 4840, 'OPC Unified Architecture');

-- 3️⃣ Insert test devices with protocol_id
INSERT INTO devices (
    tenant_id, site_id, device_group_id, edge_server_id,
    name, description, device_type, manufacturer, model, serial_number,
    protocol_id, endpoint, config,
    is_enabled, installation_date, created_by, created_at, updated_at
) VALUES 
-- Modbus TCP Device
(
    1, 1, 1, 1,
    'PLC-001', 'Production Line PLC', 'PLC', 'Siemens', 'S7-1200', 'SN001',
    1, '192.168.1.10:502', '{"slave_id": 1, "timeout": 3000}',
    1, '2025-01-01', 1, datetime('now'), datetime('now')
),
-- Modbus RTU Device
(
    1, 1, 1, 1,
    'RTU-TEMP-001', 'RTU Temperature Sensor', 'SENSOR', 'Schneider', 'PM8000', 'RTU001',
    2, '/dev/ttyUSB0', '{"slave_id": 5, "baud_rate": 9600, "parity": "N", "data_bits": 8, "stop_bits": 1}',
    1, '2025-01-01', 1, datetime('now'), datetime('now')
),
-- MQTT Device
(
    1, 1, 1, 1,
    'MQTT-GATEWAY-001', 'IoT Gateway', 'GATEWAY', 'Generic', 'ESP32', 'MQTT001',
    3, 'mqtt://192.168.1.20:1883', '{"client_id": "gateway001", "qos": 1}',
    1, '2025-01-01', 1, datetime('now'), datetime('now')
);

-- 4️⃣ Insert data points for devices
INSERT INTO data_points (
    device_id, name, description, address, address_string, data_type,
    access_mode, unit, scaling_factor, scaling_offset,
    min_value, max_value, is_enabled, is_writable, polling_interval_ms,
    log_enabled, log_interval_ms, log_deadband,
    group_name, tags, metadata, protocol_params,
    created_at, updated_at
) VALUES
-- PLC-001 Data Points
(
    (SELECT id FROM devices WHERE name = 'PLC-001'),
    'Production_Count', 'Total Production Count', 1000, '1000', 'UINT32',
    'read', 'pcs', 1.0, 0.0,
    0.0, 999999.0, 1, 0, 1000,
    1, 1000, 1.0,
    'production', '["counter", "production"]', '{}',
    '{"slave_id": 1, "function_code": 3}',
    datetime('now'), datetime('now')
),
(
    (SELECT id FROM devices WHERE name = 'PLC-001'),
    'Motor_Speed', 'Motor RPM', 2000, '2000', 'FLOAT32',
    'read', 'RPM', 1.0, 0.0,
    0.0, 3000.0, 1, 0, 1000,
    1, 1000, 10.0,
    'motor', '["speed", "motor"]', '{}',
    '{"slave_id": 1, "function_code": 3}',
    datetime('now'), datetime('now')
),
-- RTU-TEMP-001 Data Points
(
    (SELECT id FROM devices WHERE name = 'RTU-TEMP-001'),
    'Zone1_Temperature', 'Zone 1 Temperature', 2001, '2001', 'FLOAT32',
    'read', '°C', 0.1, 0.0,
    -50.0, 150.0, 1, 0, 5000,
    1, 5000, 0.1,
    'temperature', '["temperature", "zone1"]', '{}',
    '{"slave_id": 5, "function_code": 3}',
    datetime('now'), datetime('now')
),
-- MQTT-GATEWAY-001 Data Points
(
    (SELECT id FROM devices WHERE name = 'MQTT-GATEWAY-001'),
    'Sensor_Battery', 'Battery Level', 0, 'battery/level', 'FLOAT32',
    'read', '%', 1.0, 0.0,
    0.0, 100.0, 1, 0, 60000,
    1, 60000, 1.0,
    'system', '["battery", "system"]', '{}',
    '{"topic": "sensors/gateway001/battery"}',
    datetime('now'), datetime('now')
);

-- 5️⃣ Verification queries
SELECT '=== Protocols ===' as info;
SELECT * FROM protocols;

SELECT '=== Devices ===' as info;
SELECT id, name, protocol_id, endpoint FROM devices;

SELECT '=== Data Points ===' as info;
SELECT d.name as device, dp.name as point, dp.address, dp.data_type 
FROM devices d 
JOIN data_points dp ON d.id = dp.device_id;
