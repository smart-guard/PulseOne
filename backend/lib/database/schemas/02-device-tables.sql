-- =============================================================================
-- 02-device-tables.sql - 디바이스 관련 테이블
-- =============================================================================

-- 디바이스 테이블
CREATE TABLE IF NOT EXISTS devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER,
    device_group_id INTEGER,
    edge_server_id INTEGER,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    device_type VARCHAR(50) NOT NULL,
    manufacturer VARCHAR(100),
    model VARCHAR(100),
    serial_number VARCHAR(100),
    protocol_type VARCHAR(50) NOT NULL,
    endpoint VARCHAR(255) NOT NULL,
    config TEXT,
    polling_interval INTEGER DEFAULT 1000,
    timeout INTEGER DEFAULT 3000,
    retry_count INTEGER DEFAULT 3,
    is_enabled INTEGER DEFAULT 1,
    installation_date DATE,
    last_maintenance DATE,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE SET NULL,
    FOREIGN KEY (device_group_id) REFERENCES device_groups(id) ON DELETE SET NULL,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, name)
);

-- 디바이스 상태 테이블
CREATE TABLE IF NOT EXISTS device_status (
    device_id INTEGER PRIMARY KEY,
    connection_status VARCHAR(20) DEFAULT 'disconnected',
    last_communication DATETIME,
    error_count INTEGER DEFAULT 0,
    last_error TEXT,
    response_time INTEGER,
    firmware_version VARCHAR(50),
    hardware_info TEXT,
    diagnostic_data TEXT,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- 디바이스 설정 테이블
CREATE TABLE IF NOT EXISTS device_settings (
    device_id INTEGER PRIMARY KEY,
    polling_interval_ms INTEGER DEFAULT 1000,
    connection_timeout_ms INTEGER DEFAULT 5000,
    max_retry_count INTEGER DEFAULT 3,
    retry_interval_ms INTEGER DEFAULT 5000,
    backoff_time_ms INTEGER DEFAULT 60000,
    keep_alive_enabled INTEGER DEFAULT 1,
    keep_alive_interval_s INTEGER DEFAULT 30,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- 데이터포인트 테이블
CREATE TABLE IF NOT EXISTS data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    device_id INTEGER NOT NULL,
    point_name VARCHAR(100) NOT NULL,
    point_address VARCHAR(100) NOT NULL,
    data_type VARCHAR(20) NOT NULL DEFAULT 'number',
    unit VARCHAR(20),
    scaling_factor REAL DEFAULT 1.0,
    scaling_offset REAL DEFAULT 0.0,
    min_value REAL,
    max_value REAL,
    decimal_places INTEGER DEFAULT 2,
    is_writable INTEGER DEFAULT 0,
    is_enabled INTEGER DEFAULT 1,
    polling_rate INTEGER DEFAULT 1000,
    archive_enabled INTEGER DEFAULT 1,
    archive_interval INTEGER DEFAULT 60000,
    description TEXT,
    tags TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    UNIQUE(device_id, point_address)
);

-- 가상포인트 테이블
CREATE TABLE IF NOT EXISTS virtual_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    scope_type VARCHAR(20) DEFAULT 'tenant',
    site_id INTEGER,
    device_id INTEGER,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    formula TEXT NOT NULL,
    data_type VARCHAR(20) DEFAULT 'float',
    unit VARCHAR(20),
    calculation_interval INTEGER DEFAULT 1000,
    calculation_trigger VARCHAR(20) DEFAULT 'timer',
    is_enabled INTEGER DEFAULT 1,
    category VARCHAR(50) DEFAULT 'calculation',
    tags TEXT,
    execution_type VARCHAR(20) DEFAULT 'javascript',
    cache_duration_ms INTEGER DEFAULT 0,
    error_handling VARCHAR(50) DEFAULT 'return_null',
    last_error TEXT,
    execution_count INTEGER DEFAULT 0,
    avg_execution_time_ms REAL DEFAULT 0,
    last_execution_time DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE SET NULL,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, name)
);

-- 가상포인트 입력값 테이블
CREATE TABLE IF NOT EXISTS virtual_point_inputs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    point_id INTEGER,
    virtual_point_input_id INTEGER,
    alias VARCHAR(100) NOT NULL,
    data_type VARCHAR(20) DEFAULT 'number',
    default_value TEXT,
    is_required INTEGER DEFAULT 1,
    sort_order INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE SET NULL,
    FOREIGN KEY (virtual_point_input_id) REFERENCES virtual_points(id) ON DELETE SET NULL,
    UNIQUE(virtual_point_id, alias)
);