-- =============================================================================
-- PulseOne PostgreSQL Initial Schema (Full)
-- =============================================================================

-- Schema Versions
CREATE TABLE IF NOT EXISTS schema_versions (
    id SERIAL PRIMARY KEY,
    version VARCHAR(20) NOT NULL,
    applied_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    description TEXT
);

-- Tenants
CREATE TABLE IF NOT EXISTS tenants (
    id SERIAL PRIMARY KEY,
    company_name VARCHAR(100) NOT NULL UNIQUE,
    company_code VARCHAR(20) NOT NULL UNIQUE,
    domain VARCHAR(100) UNIQUE,
    contact_name VARCHAR(100),
    contact_email VARCHAR(100),
    contact_phone VARCHAR(20),
    subscription_plan VARCHAR(20) DEFAULT 'starter',
    subscription_status VARCHAR(20) DEFAULT 'active',
    max_edge_servers INTEGER DEFAULT 3,
    max_data_points INTEGER DEFAULT 1000,
    max_users INTEGER DEFAULT 5,
    timezone VARCHAR(50) DEFAULT 'UTC',             -- Added
    subscription_start_date TIMESTAMP,              -- Added
    trial_end_date TIMESTAMP,                       -- Added
    is_active INTEGER DEFAULT 1,                    -- Changed to INTEGER
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_deleted INTEGER DEFAULT 0                    -- Changed to INTEGER
);

-- Users
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    tenant_id INTEGER REFERENCES tenants(id) ON DELETE CASCADE,
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(100),
    role VARCHAR(20) NOT NULL,
    is_active INTEGER DEFAULT 1,     -- Changed to INTEGER
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Sites
CREATE TABLE IF NOT EXISTS sites (
    id SERIAL PRIMARY KEY,
    tenant_id INTEGER NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    name VARCHAR(100) NOT NULL,
    code VARCHAR(20) NOT NULL,
    site_type VARCHAR(50) NOT NULL,
    description TEXT,
    location VARCHAR(200),
    is_active INTEGER DEFAULT 1,     -- Changed to INTEGER
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(tenant_id, code)
);

-- Edge Servers
CREATE TABLE IF NOT EXISTS edge_servers (
    id SERIAL PRIMARY KEY,
    tenant_id INTEGER NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    server_name VARCHAR(100) NOT NULL,
    factory_name VARCHAR(100), -- Added for compatibility
    location VARCHAR(200),     -- Added for compatibility
    ip_address VARCHAR(45),
    mac_address VARCHAR(17),
    hostname VARCHAR(100),
    port INTEGER DEFAULT 8080,
    registration_token VARCHAR(255) UNIQUE,
    instance_key VARCHAR(255) UNIQUE,
    activation_code VARCHAR(50),
    status VARCHAR(20) DEFAULT 'pending',
    last_seen TIMESTAMP,
    last_heartbeat TIMESTAMP,
    version VARCHAR(20),
    config TEXT,
    max_devices INTEGER DEFAULT 100,      -- Added
    max_data_points INTEGER DEFAULT 1000, -- Added
    is_enabled INTEGER DEFAULT 1,         -- Changed to INTEGER
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_deleted INTEGER DEFAULT 0,         -- Changed to INTEGER from BOOLEAN
    site_id INTEGER REFERENCES sites(id) ON DELETE SET NULL
);

-- Protocols
CREATE TABLE IF NOT EXISTS protocols (
    id SERIAL PRIMARY KEY,
    protocol_type VARCHAR(50) NOT NULL UNIQUE,
    display_name VARCHAR(100) NOT NULL,
    description TEXT,
    default_port INTEGER,
    uses_serial INTEGER DEFAULT 0,    -- Added based on Repo
    requires_broker INTEGER DEFAULT 0,-- Added based on Repo
    is_enabled INTEGER DEFAULT 1,     -- Changed to INTEGER
    is_deprecated INTEGER DEFAULT 0,  -- Added
    category VARCHAR(50),             -- Added
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Device Groups
CREATE TABLE IF NOT EXISTS device_groups (
    id SERIAL PRIMARY KEY,
    tenant_id INTEGER NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    site_id INTEGER NOT NULL REFERENCES sites(id) ON DELETE CASCADE,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Devices
CREATE TABLE IF NOT EXISTS devices (
    id SERIAL PRIMARY KEY,
    tenant_id INTEGER NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    site_id INTEGER NOT NULL REFERENCES sites(id) ON DELETE CASCADE,
    device_group_id INTEGER REFERENCES device_groups(id) ON DELETE SET NULL,
    edge_server_id INTEGER REFERENCES edge_servers(id) ON DELETE SET NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    device_type VARCHAR(50) NOT NULL,
    manufacturer VARCHAR(100),
    model VARCHAR(100),
    serial_number VARCHAR(100),            -- Added
    protocol_id INTEGER NOT NULL REFERENCES protocols(id) ON DELETE RESTRICT,
    endpoint VARCHAR(255) NOT NULL,
    config TEXT NOT NULL,
    polling_interval INTEGER DEFAULT 1000,
    timeout INTEGER DEFAULT 3000,
    retry_count INTEGER DEFAULT 3,
    is_enabled INTEGER DEFAULT 1,          -- Changed to INTEGER
    is_deleted INTEGER DEFAULT 0,          -- Changed to INTEGER
    is_simulation_mode INTEGER DEFAULT 0,  -- Changed to INTEGER
    tags TEXT,
    metadata TEXT,
    created_by INTEGER,
    installation_date DATE,
    last_maintenance DATE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Device Settings
CREATE TABLE IF NOT EXISTS device_settings (
    device_id INTEGER PRIMARY KEY REFERENCES devices(id) ON DELETE CASCADE,
    
    -- Polling and Timing
    polling_interval_ms INTEGER DEFAULT 1000,
    scan_rate_override INTEGER,
    
    -- Connection and Communication
    connection_timeout_ms INTEGER DEFAULT 10000,
    read_timeout_ms INTEGER DEFAULT 5000,
    write_timeout_ms INTEGER DEFAULT 5000,
    
    -- Retry Policy
    max_retry_count INTEGER DEFAULT 3,
    retry_interval_ms INTEGER DEFAULT 5000,
    backoff_multiplier DECIMAL(3,2) DEFAULT 1.5,
    backoff_time_ms INTEGER DEFAULT 60000,
    max_backoff_time_ms INTEGER DEFAULT 300000,
    
    -- Keep-alive
    keep_alive_enabled INTEGER DEFAULT 1,
    keep_alive_interval_s INTEGER DEFAULT 30,
    keep_alive_timeout_s INTEGER DEFAULT 10,
    
    -- Data Quality
    data_validation_enabled INTEGER DEFAULT 1,
    performance_monitoring_enabled INTEGER DEFAULT 1,
    outlier_detection_enabled INTEGER DEFAULT 0,
    deadband_enabled INTEGER DEFAULT 1,
    
    -- Logging and Diagnostics
    detailed_logging_enabled INTEGER DEFAULT 0,
    diagnostic_mode_enabled INTEGER DEFAULT 0,
    auto_registration_enabled INTEGER DEFAULT 0,
    
    -- Metadata
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_by INTEGER
);

-- Data Points
CREATE TABLE IF NOT EXISTS data_points (
    id SERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    address INTEGER NOT NULL,
    address_string VARCHAR(255),
    data_type VARCHAR(20) NOT NULL DEFAULT 'UNKNOWN',
    access_mode VARCHAR(10) DEFAULT 'read',
    is_enabled INTEGER DEFAULT 1,      -- Changed to INTEGER
    is_writable INTEGER DEFAULT 0,     -- Changed to INTEGER
    unit VARCHAR(50),
    scaling_factor REAL DEFAULT 1.0,
    scaling_offset REAL DEFAULT 0.0,
    log_enabled INTEGER DEFAULT 1,     -- Changed to INTEGER
    log_interval_ms INTEGER DEFAULT 0,
    log_deadband REAL DEFAULT 0.0,
    polling_interval_ms INTEGER DEFAULT 0,
    tags TEXT,
    metadata TEXT,
    protocol_params TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_deleted INTEGER DEFAULT 0,      -- Changed to INTEGER
    UNIQUE(device_id, address)
);

-- Alarm Rules
CREATE TABLE IF NOT EXISTS alarm_rules (
    id SERIAL PRIMARY KEY,
    tenant_id INTEGER NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    target_type VARCHAR(20) NOT NULL,
    target_id INTEGER,
    alarm_type VARCHAR(20) NOT NULL,
    high_limit REAL,
    low_limit REAL,
    deadband REAL DEFAULT 0,
    severity VARCHAR(20) DEFAULT 'medium',
    is_enabled INTEGER DEFAULT 1,      -- Changed to INTEGER
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    category VARCHAR(50),
    tags TEXT
);

-- Alarm Occurrences
CREATE TABLE IF NOT EXISTS alarm_occurrences (
    id SERIAL PRIMARY KEY,
    rule_id INTEGER NOT NULL REFERENCES alarm_rules(id) ON DELETE CASCADE,
    tenant_id INTEGER NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    occurrence_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    trigger_value TEXT,
    alarm_message TEXT,
    severity VARCHAR(20),
    state VARCHAR(20) DEFAULT 'active',
    acknowledged_time TIMESTAMP,
    cleared_time TIMESTAMP,
    device_id INTEGER,
    point_id INTEGER,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- System Settings
CREATE TABLE IF NOT EXISTS system_settings (
    id SERIAL PRIMARY KEY,
    key_name VARCHAR(100) UNIQUE NOT NULL,
    value TEXT NOT NULL,
    description TEXT,
    category VARCHAR(50) DEFAULT 'general',
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- SEED DATA
INSERT INTO tenants (company_name, company_code, domain, subscription_plan)
VALUES ('Test Company', 'TEST01', 'test.com', 'enterprise')
ON CONFLICT (company_code) DO NOTHING;

INSERT INTO users (tenant_id, username, email, password_hash, role)
VALUES (1, 'admin', 'admin@test.com', 'hashed_password', 'system_admin')
ON CONFLICT (username) DO NOTHING;

INSERT INTO sites (tenant_id, name, code, site_type)
VALUES (1, 'Default Site', 'SITE01', 'factory')
ON CONFLICT (tenant_id, code) DO NOTHING;

INSERT INTO protocols (protocol_type, display_name, default_port)
VALUES 
('MODBUS_TCP', 'Modbus TCP', 502),
('MODBUS_RTU', 'Modbus RTU', 0),
('MQTT', 'MQTT', 1883)
ON CONFLICT (protocol_type) DO NOTHING;

INSERT INTO edge_servers (tenant_id, site_id, server_name, hostname, ip_address, instance_key, status, config)
VALUES (1, 1, 'Test Collector', 'docker-collector-1', '127.0.0.1', 'docker-collector-1:12345', 'active', '{}')
ON CONFLICT (instance_key) DO NOTHING;

-- Seed Device
INSERT INTO devices (tenant_id, site_id, edge_server_id, name, device_type, protocol_id, endpoint, config, is_enabled)
VALUES (1, 1, 1, 'Test Device 1', 'PLC', 1, '192.168.1.100:502', '{"slave_id": 1}', 1)
ON CONFLICT DO NOTHING; -- name check optional

-- Seed Data Point
INSERT INTO data_points (device_id, name, address, data_type, access_mode, is_enabled)
VALUES (1, 'Temperature', 40001, 'FLOAT32', 'read', 1)
ON CONFLICT (device_id, address) DO NOTHING;

