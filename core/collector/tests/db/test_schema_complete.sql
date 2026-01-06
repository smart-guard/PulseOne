-- Unified Test Schema and Data for PulseOne Collector Tests (Corrected V3)

-- =============================================================================
-- 1. Clean up
-- =============================================================================
DROP TABLE IF EXISTS alarm_occurrences;
DROP TABLE IF EXISTS alarm_rules;
DROP TABLE IF EXISTS current_values;
DROP TABLE IF EXISTS data_points;
DROP TABLE IF EXISTS device_settings;
DROP TABLE IF EXISTS devices;
DROP TABLE IF EXISTS protocols;
DROP TABLE IF EXISTS sites;
DROP TABLE IF EXISTS tenants;
DROP TABLE IF EXISTS virtual_point_values;
DROP TABLE IF EXISTS virtual_point_inputs;
DROP TABLE IF EXISTS virtual_points;

-- =============================================================================
-- 2. Create Tables
-- =============================================================================

-- Protocols Table (Complete Schema matching ProtocolRepository)
CREATE TABLE protocols (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(50) NOT NULL UNIQUE,
    display_name VARCHAR(100),
    protocol_type VARCHAR(20) NOT NULL,
    default_port INTEGER,
    description TEXT,
    is_enabled INTEGER DEFAULT 1,
    
    -- Extended Columns
    uses_serial INTEGER DEFAULT 0,
    requires_broker INTEGER DEFAULT 0,
    supported_operations TEXT,
    supported_data_types TEXT,
    connection_params TEXT,
    default_config TEXT,
    security_level VARCHAR(20),
    max_connections INTEGER,
    
    -- Mapped in Repository
    default_polling_interval INTEGER DEFAULT 1000,
    default_timeout INTEGER DEFAULT 3000,
    max_concurrent_connections INTEGER DEFAULT 10,
    is_deprecated INTEGER DEFAULT 0,
    
    min_firmware_version VARCHAR(20),
    category VARCHAR(50),
    vendor VARCHAR(100),
    standard_reference VARCHAR(100),

    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Device Settings Table (Matching DeviceSettingsRepository)
CREATE TABLE device_settings (
    device_id INTEGER PRIMARY KEY,
    polling_interval_ms INTEGER,
    scan_rate_override INTEGER,
    connection_timeout_ms INTEGER,
    read_timeout_ms INTEGER,
    write_timeout_ms INTEGER,
    max_retry_count INTEGER,
    retry_interval_ms INTEGER,
    backoff_multiplier REAL,
    backoff_time_ms INTEGER,
    max_backoff_time_ms INTEGER,
    keep_alive_enabled INTEGER,
    keep_alive_interval_s INTEGER,
    keep_alive_timeout_s INTEGER,
    data_validation_enabled INTEGER,
    outlier_detection_enabled INTEGER,
    deadband_enabled INTEGER,
    detailed_logging_enabled INTEGER,
    performance_monitoring_enabled INTEGER,
    diagnostic_mode_enabled INTEGER,
    
    -- Added based on error logs
    updated_by INTEGER DEFAULT 1,
    
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Devices Table (Matching DeviceRepository)
CREATE TABLE devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER,
    site_id INTEGER,
    device_group_id INTEGER,
    edge_server_id INTEGER,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    device_type VARCHAR(50),
    manufacturer VARCHAR(100),
    model VARCHAR(100),
    serial_number VARCHAR(100),
    protocol_id INTEGER NOT NULL,
    endpoint VARCHAR(255),
    config TEXT,
    
    -- Columns expected by DeviceRepository
    polling_interval INTEGER DEFAULT 1000,
    timeout INTEGER DEFAULT 3000,
    retry_count INTEGER DEFAULT 3,
    
    is_enabled INTEGER DEFAULT 1,
    installation_date DATETIME,
    last_maintenance DATETIME,
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (protocol_id) REFERENCES protocols(id)
);

-- Data Points Table
CREATE TABLE data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    address INTEGER NOT NULL,
    address_string VARCHAR(255),
    data_type VARCHAR(20) NOT NULL DEFAULT 'UNKNOWN',
    access_mode VARCHAR(10) DEFAULT 'read',
    is_enabled INTEGER DEFAULT 1,
    is_writable INTEGER DEFAULT 0,
    unit VARCHAR(50),
    scaling_factor REAL DEFAULT 1.0,
    scaling_offset REAL DEFAULT 0.0,
    min_value REAL DEFAULT 0.0,
    max_value REAL DEFAULT 0.0,
    log_enabled INTEGER DEFAULT 1,
    log_interval_ms INTEGER DEFAULT 0,
    log_deadband REAL DEFAULT 0.0,
    polling_interval_ms INTEGER DEFAULT 0,
    group_name VARCHAR(50),
    tags TEXT,
    metadata TEXT,
    protocol_params TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    UNIQUE(device_id, address)
);

-- Current Values Table
CREATE TABLE current_values (
    point_id INTEGER PRIMARY KEY,
    current_value TEXT,
    raw_value TEXT,
    value_type VARCHAR(10) DEFAULT 'double',
    quality_code INTEGER DEFAULT 0,
    quality VARCHAR(20) DEFAULT 'not_connected',
    value_timestamp DATETIME,
    quality_timestamp DATETIME,
    last_log_time DATETIME,
    last_read_time DATETIME,
    last_write_time DATETIME,
    read_count INTEGER DEFAULT 0,
    write_count INTEGER DEFAULT 0,
    error_count INTEGER DEFAULT 0,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE
);

-- Alarm Rules Table
CREATE TABLE alarm_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    target_type VARCHAR(20) NOT NULL,
    target_id INTEGER,
    target_group VARCHAR(100),
    alarm_type VARCHAR(20) NOT NULL,
    high_high_limit REAL,
    high_limit REAL,
    low_limit REAL,
    low_low_limit REAL,
    deadband REAL DEFAULT 0,
    rate_of_change REAL DEFAULT 0,
    trigger_condition VARCHAR(20),
    condition_script TEXT,
    message_script TEXT,
    message_config TEXT,
    message_template TEXT,
    severity VARCHAR(20) DEFAULT 'medium',
    priority INTEGER DEFAULT 100,
    auto_acknowledge INTEGER DEFAULT 0,
    acknowledge_timeout_min INTEGER DEFAULT 0,
    auto_clear INTEGER DEFAULT 1,
    suppression_rules TEXT,
    notification_enabled INTEGER DEFAULT 1,
    notification_delay_sec INTEGER DEFAULT 0,
    notification_repeat_interval_min INTEGER DEFAULT 0,
    notification_channels TEXT,
    notification_recipients TEXT,
    is_enabled INTEGER DEFAULT 1,
    is_latched INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    template_id INTEGER,
    rule_group VARCHAR(36),
    created_by_template INTEGER DEFAULT 0,
    last_template_update DATETIME,
    escalation_enabled INTEGER DEFAULT 0,
    escalation_max_level INTEGER DEFAULT 3,
    escalation_rules TEXT DEFAULT NULL,
    category VARCHAR(50) DEFAULT NULL,
    tags TEXT DEFAULT NULL
);

-- Alarm Occurrences Table
CREATE TABLE alarm_occurrences (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_id INTEGER NOT NULL,
    tenant_id INTEGER NOT NULL,
    occurrence_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    trigger_value TEXT,
    trigger_condition TEXT,
    alarm_message TEXT,
    severity VARCHAR(20),
    state VARCHAR(20) DEFAULT 'active',
    acknowledged_time DATETIME,
    acknowledged_by INTEGER,
    acknowledge_comment TEXT,
    cleared_time DATETIME,
    cleared_value TEXT,
    clear_comment TEXT,
    cleared_by INTEGER,
    notification_sent INTEGER DEFAULT 0,
    notification_time DATETIME,
    notification_count INTEGER DEFAULT 0,
    notification_result TEXT,
    context_data TEXT,
    source_name VARCHAR(100),
    location VARCHAR(200),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    device_id INTEGER,
    point_id INTEGER,
    category VARCHAR(50) DEFAULT NULL,
    tags TEXT DEFAULT NULL,
    FOREIGN KEY (rule_id) REFERENCES alarm_rules(id) ON DELETE CASCADE
);

-- Virtual Points Tables (Matching virtual_points_core.sql)
CREATE TABLE virtual_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    scope_type VARCHAR(20) NOT NULL DEFAULT 'tenant',
    site_id INTEGER,
    device_id INTEGER,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    formula TEXT NOT NULL,
    data_type VARCHAR(20) NOT NULL DEFAULT 'float',
    unit VARCHAR(20),
    calculation_interval INTEGER DEFAULT 1000,
    calculation_trigger VARCHAR(20) DEFAULT 'timer',
    is_enabled INTEGER DEFAULT 1,
    category VARCHAR(50),
    tags TEXT,
    execution_type VARCHAR(20) DEFAULT 'javascript',
    dependencies TEXT,
    cache_duration_ms INTEGER DEFAULT 0,
    error_handling VARCHAR(20) DEFAULT 'return_null',
    last_error TEXT,
    execution_count INTEGER DEFAULT 0,
    avg_execution_time_ms REAL DEFAULT 0.0,
    last_execution_time DATETIME,
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);

CREATE TABLE virtual_point_inputs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    variable_name VARCHAR(50) NOT NULL,
    source_type VARCHAR(20) NOT NULL,
    source_id INTEGER,
    constant_value REAL,
    source_formula TEXT,
    data_processing VARCHAR(20) DEFAULT 'current',
    time_window_seconds INTEGER,
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    UNIQUE(virtual_point_id, variable_name)
);

CREATE TABLE virtual_point_values (
    virtual_point_id INTEGER PRIMARY KEY,
    value REAL,
    string_value TEXT,
    quality VARCHAR(20) DEFAULT 'good',
    last_calculated DATETIME DEFAULT CURRENT_TIMESTAMP,
    calculation_error TEXT,
    input_values TEXT,
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE
);

-- =============================================================================
-- 3. Insert Data
-- =============================================================================

-- Insert Protocols
INSERT INTO protocols (
    id, name, display_name, protocol_type, default_port, description,
    uses_serial, requires_broker, supported_operations, supported_data_types, connection_params,
    category, default_polling_interval, default_timeout, max_concurrent_connections, is_deprecated
) VALUES
(1, 'MODBUS_TCP', 'Modbus TCP/IP', 'tcp', 502, 'Modbus over TCP/IP',
 0, 0, '["read", "write"]', '["UINT16", "INT16", "UINT32", "INT32", "FLOAT32"]', '{}',
 'industrial', 1000, 3000, 20, 0),

(2, 'MODBUS_RTU', 'Modbus RTU', 'serial', NULL, 'Modbus RTU over Serial',
 1, 0, '["read", "write"]', '["UINT16", "INT16", "UINT32", "INT32", "FLOAT32"]', 
 '{"baud_rate": 9600, "parity": "N", "data_bits": 8, "stop_bits": 1}',
 'industrial', 1000, 3000, 1, 0),

(3, 'MQTT', 'MQTT Protocol', 'mqtt', 1883, 'MQTT Message Queue',
 0, 1, '["publish", "subscribe"]', '["STRING", "JSON", "BINARY"]', 
 '{"qos": 1, "clean_session": true}',
 'iot', 0, 5000, 50, 0),
 
(4, 'BACNET', 'BACnet Protocol', 'udp', 47808, 'BACnet/IP Protocol',
 0, 0, '["read", "write", "subscribe"]', '["ANALOG", "BINARY", "MULTISTATE"]', '{}',
 'building_automation', 2000, 5000, 20, 0),

(5, 'OPCUA', 'OPC UA', 'tcp', 4840, 'OPC Unified Architecture',
 0, 0, '["read", "write", "subscribe"]', '["BOOLEAN", "INT32", "FLOAT", "STRING"]', '{}',
 'industrial', 1000, 5000, 20, 0);

-- Insert Devices
INSERT INTO devices (
    id, tenant_id, site_id, device_group_id, edge_server_id,
    name, description, device_type, manufacturer, model, serial_number,
    protocol_id, endpoint, config,
    polling_interval, timeout, retry_count,
    is_enabled, installation_date, last_maintenance, created_by, created_at, updated_at
) VALUES 
-- Modbus TCP Device
(1, 1, 1, 1, 1, 'PLC-001', 'Production Line PLC', 'PLC', 'Siemens', 'S7-1200', 'SN001',
 1, '192.168.1.10:502', '{"slave_id": 1}',
 1000, 3000, 3,
 1, '2025-01-01', datetime('now'), 1, datetime('now'), datetime('now')),
 
-- Modbus RTU Device
(2, 1, 1, 1, 1, 'RTU-TEMP-001', 'RTU Temperature Sensor', 'SENSOR', 'Schneider', 'PM8000', 'RTU001',
 2, '/dev/ttyUSB0', '{"slave_id": 5, "baud_rate": 9600, "parity": "N", "data_bits": 8, "stop_bits": 1}',
 5000, 5000, 5,
 1, '2025-01-01', datetime('now'), 1, datetime('now'), datetime('now')),
 
-- MQTT Device
(3, 1, 1, 1, 1, 'MQTT-GATEWAY-001', 'IoT Gateway', 'GATEWAY', 'Generic', 'ESP32', 'MQTT001',
 3, 'mqtt://192.168.1.20:1883', '{"client_id": "gateway001", "qos": 1}',
 10000, 30000, 3,
 1, '2025-01-01', datetime('now'), 1, datetime('now'), datetime('now'));

-- Insert Device Settings
INSERT INTO device_settings (
    device_id, polling_interval_ms, scan_rate_override,
    connection_timeout_ms, read_timeout_ms, write_timeout_ms,
    max_retry_count, retry_interval_ms, backoff_multiplier, backoff_time_ms, max_backoff_time_ms,
    keep_alive_enabled, keep_alive_interval_s, keep_alive_timeout_s,
    data_validation_enabled, outlier_detection_enabled, deadband_enabled,
    detailed_logging_enabled, performance_monitoring_enabled, diagnostic_mode_enabled,
    updated_by, created_at, updated_at
) VALUES
(1, 1000, NULL, 5000, 3000, 3000, 3, 2000, 1.5, 30000, 180000, 1, 30, 10, 1, 1, 1, 0, 1, 0, 1, datetime('now'), datetime('now')),
(2, 5000, NULL, 10000, 5000, 5000, 5, 5000, 2.0, 60000, 300000, 1, 60, 15, 1, 1, 1, 1, 1, 0, 1, datetime('now'), datetime('now')),
(3, 10000, NULL, 30000, 30000, 30000, 3, 10000, 1.5, 60000, 300000, 1, 60, 20, 1, 0, 1, 0, 1, 0, 1, datetime('now'), datetime('now'));

-- Insert Data Points
INSERT INTO data_points (
    device_id, name, description, address, address_string, data_type,
    access_mode, unit, scaling_factor, scaling_offset,
    min_value, max_value, is_enabled, is_writable, polling_interval_ms,
    log_enabled, log_interval_ms, log_deadband,
    group_name, tags, metadata, protocol_params,
    created_at, updated_at
) VALUES
-- PLC-001
(1, 'Production_Count', 'Total Production Count', 1000, '1000', 'UINT32',
 'read', 'pcs', 1.0, 0.0, 0.0, 999999.0, 1, 0, 1000,
 1, 1000, 1.0, 'production', '["counter", "production"]', '{}', '{"slave_id": 1, "function_code": 3}',
 datetime('now'), datetime('now')),
(1, 'Motor_Speed', 'Motor RPM', 2000, '2000', 'FLOAT32',
 'read', 'RPM', 1.0, 0.0, 0.0, 3000.0, 1, 0, 1000,
 1, 1000, 10.0, 'motor', '["speed", "motor"]', '{}', '{"slave_id": 1, "function_code": 3}',
 datetime('now'), datetime('now')),
-- RTU-TEMP-001
(2, 'Zone1_Temperature', 'Zone 1 Temperature', 2001, '2001', 'FLOAT32',
 'read', '°C', 0.1, 0.0, -50.0, 150.0, 1, 0, 5000,
 1, 5000, 0.1, 'temperature', '["temperature", "zone1"]', '{}', '{"slave_id": 5, "function_code": 3}',
 datetime('now'), datetime('now')),
-- MQTT-GATEWAY-001
(3, 'Sensor_Battery', 'Battery Level', 0, 'battery/level', 'FLOAT32',
 'read', '%', 1.0, 0.0, 0.0, 100.0, 1, 0, 60000,
 1, 60000, 1.0, 'system', '["battery", "system"]', '{}', '{"topic": "sensors/gateway001/battery"}',
 datetime('now'), datetime('now'));

-- Insert Alarms for Step 5
INSERT INTO alarm_rules (
    id, tenant_id, name, description, target_type, target_id, alarm_type, 
    high_limit, severity, is_enabled
) VALUES (
    1, 1, 'High Temperature Alarm', 'Zone 1 Temp > 40', 'point', 255, 'threshold', 
    40.0, 'HIGH', 1
);

INSERT INTO alarm_occurrences (
    id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition, 
    alarm_message, severity, state, source_name, device_id, point_id
) VALUES (
    1, 1, 1, datetime('now', '-10 minutes'), '42.5', '>', 
    'Zone 1 High Temperature detected (42.5 > 40.0)', 'HIGH', 'active', 'TemperatureSensor', 2, 255
);
