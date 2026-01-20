-- Recreate PulseOne DB with correct schema from SQLQueries.h and ExtendedSQLQueries.h

PRAGMA foreign_keys = OFF;

DROP TABLE IF EXISTS protocols;
DROP TABLE IF EXISTS manufacturers;
DROP TABLE IF EXISTS device_models;
DROP TABLE IF EXISTS device_groups;
DROP TABLE IF EXISTS driver_plugins;
DROP TABLE IF EXISTS devices;
DROP TABLE IF EXISTS device_settings;
DROP TABLE IF EXISTS device_status;
DROP TABLE IF EXISTS data_points;
DROP TABLE IF EXISTS current_values;
DROP TABLE IF EXISTS alarm_rules;
DROP TABLE IF EXISTS alarm_occurrences;
DROP TABLE IF EXISTS virtual_points;
DROP TABLE IF EXISTS virtual_point_inputs;
DROP TABLE IF EXISTS script_library;

-- Protocols
CREATE TABLE protocols (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    protocol_type VARCHAR(50) NOT NULL UNIQUE,
    display_name VARCHAR(100) NOT NULL,
    description TEXT,
    default_port INTEGER,
    uses_serial INTEGER DEFAULT 0,
    requires_broker INTEGER DEFAULT 0,
    supported_operations TEXT,
    supported_data_types TEXT,
    connection_params TEXT,
    default_polling_interval INTEGER DEFAULT 1000,
    default_timeout INTEGER DEFAULT 5000,
    max_concurrent_connections INTEGER DEFAULT 1,
    is_enabled INTEGER DEFAULT 1,
    is_deprecated INTEGER DEFAULT 0,
    min_firmware_version VARCHAR(20),
    category VARCHAR(50),
    vendor VARCHAR(100),
    standard_reference VARCHAR(100),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT chk_category CHECK (category IN ('industrial', 'iot', 'building_automation', 'network', 'web'))
);

-- Manufacturers
CREATE TABLE manufacturers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL UNIQUE,
    description TEXT,
    country VARCHAR(50),
    website VARCHAR(255),
    logo_url VARCHAR(255),
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Device Models
CREATE TABLE device_models (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    manufacturer_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    model_number VARCHAR(100),
    device_type VARCHAR(50),
    description TEXT,
    image_url VARCHAR(255),
    manual_url VARCHAR(255),
    metadata TEXT,
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (manufacturer_id) REFERENCES manufacturers(id) ON DELETE CASCADE,
    UNIQUE(manufacturer_id, name)
);

-- Devices
CREATE TABLE devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER NOT NULL,
    device_group_id INTEGER,
    edge_server_id INTEGER,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    device_type VARCHAR(50) NOT NULL,
    manufacturer VARCHAR(100),
    model VARCHAR(100),
    serial_number VARCHAR(100),
    protocol_id INTEGER NOT NULL,
    endpoint VARCHAR(255) NOT NULL,
    config TEXT NOT NULL,
    polling_interval INTEGER DEFAULT 1000,
    timeout INTEGER DEFAULT 3000,
    retry_count INTEGER DEFAULT 3,
    is_enabled INTEGER DEFAULT 1,
    installation_date DATE,
    last_maintenance DATE,
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (protocol_id) REFERENCES protocols(id) ON DELETE RESTRICT
);

-- Device Settings
CREATE TABLE device_settings (
    device_id INTEGER PRIMARY KEY,
    polling_interval_ms INTEGER DEFAULT 1000,
    scan_rate_override INTEGER,
    connection_timeout_ms INTEGER DEFAULT 10000,
    read_timeout_ms INTEGER DEFAULT 5000,
    write_timeout_ms INTEGER DEFAULT 5000,
    max_retry_count INTEGER DEFAULT 3,
    retry_interval_ms INTEGER DEFAULT 5000,
    backoff_multiplier DECIMAL(3,2) DEFAULT 1.5,
    backoff_time_ms INTEGER DEFAULT 60000,
    max_backoff_time_ms INTEGER DEFAULT 300000,
    keep_alive_enabled INTEGER DEFAULT 1,
    keep_alive_interval_s INTEGER DEFAULT 30,
    keep_alive_timeout_s INTEGER DEFAULT 10,
    data_validation_enabled INTEGER DEFAULT 1,
    performance_monitoring_enabled INTEGER DEFAULT 1,
    outlier_detection_enabled INTEGER DEFAULT 0,
    deadband_enabled INTEGER DEFAULT 1,
    detailed_logging_enabled INTEGER DEFAULT 0,
    diagnostic_mode_enabled INTEGER DEFAULT 0,
    auto_registration_enabled INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_by INTEGER,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- Data Points
CREATE TABLE data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    address INTEGER NOT NULL,
    address_string VARCHAR(255),
    mapping_key VARCHAR(255),
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
    quality_check_enabled INTEGER DEFAULT 1,
    range_check_enabled INTEGER DEFAULT 1,
    rate_of_change_limit REAL DEFAULT 0.0,
    alarm_enabled INTEGER DEFAULT 0,
    alarm_priority VARCHAR(20) DEFAULT 'medium',
    group_name VARCHAR(50),
    tags TEXT,
    metadata TEXT,
    protocol_params TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    UNIQUE(device_id, address)
);

-- Current Values
CREATE TABLE current_values (
    point_id INTEGER PRIMARY KEY,
    current_value TEXT,
    raw_value TEXT,
    previous_value TEXT,
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
    change_count INTEGER DEFAULT 0,
    alarm_state VARCHAR(20) DEFAULT 'normal',
    alarm_active INTEGER DEFAULT 0,
    alarm_acknowledged INTEGER DEFAULT 0,
    source_info TEXT,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE
);

-- Alarm Rules
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
    tags TEXT DEFAULT NULL,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);

-- Virtual Points
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
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- Virtual Point Inputs
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

-- Seed basic data
INSERT INTO protocols (id, protocol_type, display_name, is_enabled) VALUES (1, 'MODBUS_TCP', 'Modbus TCP', 1);
INSERT INTO tenants (id, company_name, company_code) VALUES (1, 'E2E Test Tenant', 'E2E_TENANT');
INSERT INTO sites (id, tenant_id, name, code, site_type) VALUES (1, 1, 'E2E Test Site', 'E2E_SITE', 'factory');

-- E2E Test Device
INSERT INTO devices (id, tenant_id, site_id, name, device_type, protocol_id, endpoint, config, is_enabled) 
VALUES (21, 1, 1, 'E2E_Test_Device_01', 'PLC', 1, '127.0.0.1:502', '{"slave_id": 1}', 1);

-- Device Status
INSERT INTO device_status (device_id, connection_status, last_communication, updated_at)
VALUES (21, 'connected', datetime('now'), datetime('now'));

-- Device Settings (Polling/Reconnection)
INSERT INTO device_settings (device_id, polling_interval_ms, retry_interval_ms, max_retry_count, backoff_time_ms, keep_alive_enabled)
VALUES (21, 5000, 200, 3, 5000, 1);

-- Data Point
INSERT INTO data_points (id, device_id, name, address, data_type, is_enabled) 
VALUES (74, 21, 'Temperature', 1, 'FLOAT32', 1);

-- Virtual Point
INSERT INTO virtual_points (id, tenant_id, scope_type, site_id, device_id, name, formula, calculation_trigger, is_enabled)
VALUES (1001, 1, 'device', 1, 21, 'E2E_Test_VP', 'Temperature * 2', 'timer', 1);

-- Virtual Point Input
INSERT INTO virtual_point_inputs (virtual_point_id, variable_name, source_type, source_id)
VALUES (1001, 'Temperature', 'data_point', 74);

-- Alarm Rule
INSERT INTO alarm_rules (id, tenant_id, name, target_type, target_id, alarm_type, high_limit, is_enabled)
VALUES (1, 1, 'E2E_Test_Alarm', 'virtual_point', 1001, 'threshold', 50.0, 1);

PRAGMA foreign_keys = ON;
