PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;

-- ==========================================
-- 1. Schema Definitions
-- ==========================================

CREATE TABLE tenants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    company_name VARCHAR(100) NOT NULL,
    company_code VARCHAR(20) NOT NULL UNIQUE,
    description TEXT,
    domain VARCHAR(100),
    contact_email VARCHAR(100),
    contact_phone VARCHAR(20),
    subscription_plan VARCHAR(20) DEFAULT 'starter',
    subscription_status VARCHAR(20) DEFAULT 'active',
    max_edge_servers INTEGER DEFAULT 3,
    max_data_points INTEGER DEFAULT 1000,
    max_users INTEGER DEFAULT 5,
    billing_cycle VARCHAR(20) DEFAULT 'monthly',
    subscription_start_date DATETIME,
    trial_end_date DATETIME,
    next_billing_date DATETIME,
    is_active INTEGER DEFAULT 1,
    timezone VARCHAR(50) DEFAULT 'UTC',
    currency VARCHAR(3) DEFAULT 'USD',
    language VARCHAR(5) DEFAULT 'en',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_deleted BOOLEAN DEFAULT 0
);

CREATE TABLE sites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    parent_site_id INTEGER,
    name VARCHAR(100) NOT NULL,
    code VARCHAR(20) NOT NULL,
    site_type VARCHAR(50) NOT NULL,
    description TEXT,
    location VARCHAR(200),
    address TEXT,
    coordinates VARCHAR(100),
    postal_code VARCHAR(20),
    country VARCHAR(2),
    city VARCHAR(100),
    state_province VARCHAR(100),
    timezone VARCHAR(50) DEFAULT 'UTC',
    currency VARCHAR(3) DEFAULT 'USD',
    language VARCHAR(5) DEFAULT 'en',
    manager_name VARCHAR(100),
    manager_email VARCHAR(100),
    manager_phone VARCHAR(20),
    emergency_contact VARCHAR(200),
    operating_hours VARCHAR(100),
    shift_pattern VARCHAR(50),
    working_days VARCHAR(20) DEFAULT 'MON-FRI',
    floor_area REAL,
    ceiling_height REAL,
    max_occupancy INTEGER,
    edge_server_id INTEGER,
    hierarchy_level INTEGER DEFAULT 0,
    hierarchy_path VARCHAR(500),
    sort_order INTEGER DEFAULT 0,
    is_active INTEGER DEFAULT 1,
    is_deleted INTEGER DEFAULT 0,
    is_visible INTEGER DEFAULT 1,
    monitoring_enabled INTEGER DEFAULT 1,
    tags TEXT,
    metadata TEXT,
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE edge_servers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    server_name VARCHAR(100) NOT NULL,
    factory_name VARCHAR(100),
    location VARCHAR(200),
    ip_address VARCHAR(45),
    mac_address VARCHAR(17),
    hostname VARCHAR(100),
    port INTEGER DEFAULT 8080,
    registration_token VARCHAR(255) UNIQUE,
    instance_key VARCHAR(255) UNIQUE,
    activation_code VARCHAR(50),
    api_key VARCHAR(255),
    status VARCHAR(20) DEFAULT 'pending',
    last_seen DATETIME,
    last_heartbeat DATETIME,
    version VARCHAR(20),
    cpu_usage REAL DEFAULT 0.0,
    memory_usage REAL DEFAULT 0.0,
    disk_usage REAL DEFAULT 0.0,
    uptime_seconds INTEGER DEFAULT 0,
    config TEXT,
    capabilities TEXT,
    sync_enabled INTEGER DEFAULT 1,
    auto_update_enabled INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_deleted INTEGER DEFAULT 0,
    site_id INTEGER, 
    server_type TEXT DEFAULT 'collector'
);

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
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

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
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

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
    UNIQUE(device_id, address)
);

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
    updated_by INTEGER
);

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
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Payload Templates
CREATE TABLE payload_templates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL UNIQUE,
    system_type VARCHAR(50) NOT NULL,
    description TEXT,
    template_json TEXT NOT NULL,
    is_system_defined BOOLEAN DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
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
    is_deleted BOOLEAN DEFAULT 0,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);

-- Alarm Occurrences
CREATE TABLE alarm_occurrences (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_id INTEGER,
    tenant_id INTEGER NOT NULL,
    occurrence_time DATETIME NOT NULL,
    trigger_value REAL,
    trigger_condition VARCHAR(100),
    alarm_message TEXT,
    severity VARCHAR(20),
    state VARCHAR(20) DEFAULT 'active',
    context_data TEXT,
    source_name VARCHAR(100),
    location VARCHAR(200),
    device_id INTEGER,
    point_id INTEGER,
    category VARCHAR(50),
    tags TEXT,
    cleared_time DATETIME,
    cleared_by INTEGER,
    acknowledged_time DATETIME,
    acknowledged_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (rule_id) REFERENCES alarm_rules(id) ON DELETE SET NULL,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);

-- Device Status
CREATE TABLE device_status (
    device_id INTEGER PRIMARY KEY,
    connection_status VARCHAR(20) NOT NULL DEFAULT 'disconnected',
    last_communication DATETIME,
    connection_established_at DATETIME,
    error_count INTEGER DEFAULT 0,
    last_error TEXT,
    last_error_time DATETIME,
    consecutive_error_count INTEGER DEFAULT 0,
    response_time INTEGER,
    min_response_time INTEGER,
    max_response_time INTEGER,
    throughput_ops_per_sec REAL DEFAULT 0.0,
    total_requests INTEGER DEFAULT 0,
    successful_requests INTEGER DEFAULT 0,
    failed_requests INTEGER DEFAULT 0,
    uptime_percentage REAL DEFAULT 0.0,
    firmware_version VARCHAR(50),
    hardware_info TEXT,
    diagnostic_data TEXT,
    cpu_usage REAL,
    memory_usage REAL,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    CONSTRAINT chk_connection_status CHECK (connection_status IN ('connected', 'disconnected', 'connecting', 'error', 'maintenance'))
);

-- Export Tables
CREATE TABLE export_targets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER,
    name VARCHAR(100) NOT NULL UNIQUE,
    target_type VARCHAR(20) NOT NULL,
    description TEXT,
    config TEXT NOT NULL,
    is_enabled BOOLEAN DEFAULT 1,
    template_id INTEGER,
    export_mode VARCHAR(20) DEFAULT 'on_change',
    export_interval INTEGER DEFAULT 0,
    batch_size INTEGER DEFAULT 100,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE SET NULL,
    FOREIGN KEY (template_id) REFERENCES payload_templates(id) ON DELETE SET NULL
);

CREATE TABLE export_profiles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL UNIQUE,
    description TEXT,
    is_enabled BOOLEAN DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by VARCHAR(50),
    point_count INTEGER DEFAULT 0,
    last_exported_at DATETIME,
    data_points TEXT DEFAULT '[]'
);

CREATE TABLE export_profile_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER NOT NULL,
    point_id INTEGER NOT NULL,
    display_order INTEGER DEFAULT 0,
    display_name VARCHAR(200),
    is_enabled BOOLEAN DEFAULT 1,
    added_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    added_by VARCHAR(50),
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE CASCADE,
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    UNIQUE(profile_id, point_id)
);

CREATE TABLE export_target_mappings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    target_id INTEGER NOT NULL,
    point_id INTEGER,
    site_id INTEGER,
    target_field_name VARCHAR(200),
    target_description VARCHAR(500),
    conversion_config TEXT,
    is_enabled BOOLEAN DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (target_id) REFERENCES export_targets(id) ON DELETE CASCADE,
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE
);

CREATE TABLE export_schedules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER,
    target_id INTEGER NOT NULL,
    schedule_name VARCHAR(100) NOT NULL,
    description TEXT,
    cron_expression VARCHAR(100) NOT NULL,
    timezone VARCHAR(50) DEFAULT 'UTC',
    data_range VARCHAR(20) DEFAULT 'day',
    lookback_periods INTEGER DEFAULT 1,
    is_enabled BOOLEAN DEFAULT 1,
    last_run_at DATETIME,
    last_status VARCHAR(20),
    next_run_at DATETIME,
    total_runs INTEGER DEFAULT 0,
    successful_runs INTEGER DEFAULT 0,
    failed_runs INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE SET NULL,
    FOREIGN KEY (target_id) REFERENCES export_targets(id) ON DELETE CASCADE
);

CREATE TABLE export_gateways (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    ip_address VARCHAR(45),
    port INTEGER DEFAULT 8080,
    status VARCHAR(20) DEFAULT 'pending',
    last_seen DATETIME,
    last_heartbeat DATETIME,
    version VARCHAR(20),
    config TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_deleted INTEGER DEFAULT 0,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);

CREATE TABLE export_profile_assignments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER NOT NULL,
    gateway_id INTEGER NOT NULL,
    is_active INTEGER DEFAULT 1,
    assigned_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE CASCADE,
    FOREIGN KEY (gateway_id) REFERENCES edge_servers(id) ON DELETE CASCADE
);

-- ==========================================
-- 2. Seed Data
-- ==========================================

INSERT INTO tenants (id, company_name, company_code) VALUES (1, 'Test Tenant', 'TEST001');
INSERT INTO sites (id, tenant_id, name, code, site_type) VALUES (1, 1, 'Test Site', 'SITE001', 'factory');

-- Protocols
INSERT INTO protocols (id, protocol_type, display_name) VALUES (1, 'MODBUS_TCP', 'Modbus TCP');

-- Edge Server (Collector)
INSERT INTO edge_servers (id, tenant_id, server_name, status, ip_address, server_type) VALUES (1, 1, 'Main Collector', 'active', '127.0.0.1', 'collector');
-- Edge Server (Export Gateway) - Added for UI Visibility
INSERT INTO edge_servers (id, tenant_id, server_name, status, ip_address, server_type, config) VALUES (2, 1, 'Local Export Gateway', 'active', '127.0.0.1', 'gateway', '{}');

-- Devices: HMI-001 (Modbus TCP)
INSERT INTO devices (id, tenant_id, site_id, edge_server_id, name, device_type, protocol_id, endpoint, is_enabled, config) 
VALUES (2, 1, 1, 1, 'HMI-001', 'HMI', 1, '172.18.0.4:50502', 1, '{"slave_id": 1}');

-- Data Points
-- 2001: Screen Status
INSERT INTO data_points (id, device_id, name, address, data_type, is_enabled) VALUES (6, 2, 'Screen_Status', 2001, 'UINT16', 1);
-- 2002: Active Alarms (Trigger Point)
INSERT INTO data_points (id, device_id, name, address, data_type, is_enabled) VALUES (7, 2, 'Active_Alarms', 2002, 'UINT16', 1);
-- 2003: User Level
INSERT INTO data_points (id, device_id, name, address, data_type, is_enabled) VALUES (8, 2, 'User_Level', 2003, 'UINT16', 1);

-- Device Settings
INSERT INTO device_settings (device_id, polling_interval_ms, connection_timeout_ms) VALUES (2, 1000, 5000);

-- Alarm Rules
INSERT INTO alarm_rules (tenant_id, name, target_type, target_id, alarm_type, high_limit, severity, is_enabled) VALUES (1, 'Test Alarm', 'data_point', 7, 'analog', 0, 'critical', 1);

-- Payload Templates (Restored from user configuration)
INSERT INTO payload_templates (id, name, system_type, description, template_json) VALUES (1, 'Insite Standard', 'insite', 'Insite Standard Format', '{"bd":"{{bd}}","ty":"{{ty}}","nm":"{{nm}}","vl":"{{vl}}","tm":"{{tm}}","st":"{{st}}","al":"{{al}}","des":"{{des}}","il":"{{il}}","xl":"{{xl}}","mi":"{{mi}}","mx":"{{mx}}"}');
INSERT INTO payload_templates (id, name, system_type, description, template_json) VALUES (5, 'instie 알람 템플릿', 'custom', 'Restored Insite Template', '[{"bd":"{{bd}}","ty":"{{ty}}","nm":"{{nm}}","vl":"{{vl}}","il":"{{il}}","xl":"{{xl}}","mi":"{{mi}}","mx":"{{mx}}","tm":"{{tm}}","st":"{{st}}","al":"{{al}}","des":"{{des}}"}]');

INSERT INTO export_gateways (id, tenant_id, name, description, ip_address, status) VALUES (1, 1, 'Local Export Gateway', 'Default Local Gateway', '127.0.0.1', 'active');

INSERT INTO export_profiles (id, name, description, is_enabled) VALUES (1, 'Default Profile', 'Main Export Profile', 1);

-- Export Configuration (Restored)
INSERT INTO export_targets (id, profile_id, name, target_type, config, export_mode) VALUES (1, 1, 'Local MQTT', 'mqtt', '{"host":"rabbitmq","port":1883,"topic":"data/export"}', 'on_change');
INSERT INTO export_targets (id, profile_id, name, target_type, config, export_mode, template_id) VALUES (2, 1, 'insite 알람 타겟', 'http', '{"url":"https://rhnkxbwgb9.execute-api.ap-northeast-2.amazonaws.com/icos5Api/stg/alarm","headers":{"Authorization":"AKIAQKDMIGZZECNAU4X7"},"auth":{"type":"x-api-key","apiKey":"0ia/oVX9l8hXMYOTakpjQ5QwU7usm4g3fm2liemT"}}', 'on_change', 5);
INSERT INTO export_targets (id, profile_id, name, target_type, config, export_mode, template_id) VALUES (3, NULL, 'Local Archive File', 'file', '{"base_path": "/app/logs/export", "file_format": "json"}', 'alarm', 5);
INSERT INTO export_profile_points (profile_id, point_id) VALUES (1, 6), (1, 7), (1, 8);
INSERT INTO export_profile_assignments (profile_id, gateway_id) VALUES (1, 2);

-- Export Override Mappings (User Requirement: BD=280)
INSERT INTO export_target_mappings (target_id, point_id, site_id, target_field_name, is_enabled) VALUES (2, 7, 280, 'Active_Alarms', 1);

COMMIT;
