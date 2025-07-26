-- =============================================================================
-- backend/lib/database/schemas/03-device-tables.sql
-- 디바이스 및 데이터 포인트 테이블 (SQLite 버전)
-- =============================================================================

-- 디바이스 그룹 테이블
CREATE TABLE IF NOT EXISTS device_groups (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    parent_group_id INTEGER,
    group_type VARCHAR(50) DEFAULT 'functional', -- functional, physical, protocol
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (parent_group_id) REFERENCES device_groups(id) ON DELETE SET NULL
);

-- 드라이버 플러그인 테이블
CREATE TABLE IF NOT EXISTS driver_plugins (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL,
    protocol_type VARCHAR(50) NOT NULL,
    version VARCHAR(20) NOT NULL,
    file_path VARCHAR(255) NOT NULL,
    description TEXT,
    
    -- 플러그인 정보
    author VARCHAR(100),
    api_version INTEGER DEFAULT 1,
    is_enabled INTEGER DEFAULT 1,
    config_schema TEXT, -- JSON 스키마
    
    -- 라이선스 정보
    license_type VARCHAR(20) DEFAULT 'free',
    license_expires DATETIME,
    
    installed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    UNIQUE(protocol_type, version)
);

-- 디바이스 테이블
CREATE TABLE IF NOT EXISTS devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER NOT NULL,
    device_group_id INTEGER,
    edge_server_id INTEGER,
    
    -- 디바이스 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    device_type VARCHAR(50) NOT NULL, -- PLC, HMI, SENSOR, GATEWAY, METER, CONTROLLER
    manufacturer VARCHAR(100),
    model VARCHAR(100),
    serial_number VARCHAR(100),
    
    -- 통신 설정
    protocol_type VARCHAR(50) NOT NULL, -- MODBUS_TCP, MODBUS_RTU, MQTT, BACNET, OPCUA
    endpoint VARCHAR(255) NOT NULL, -- IP:Port 또는 연결 문자열
    config TEXT NOT NULL, -- JSON 형태 프로토콜별 설정
    
    -- 수집 설정
    polling_interval INTEGER DEFAULT 1000,
    timeout INTEGER DEFAULT 3000,
    retry_count INTEGER DEFAULT 3,
    
    -- 상태 정보
    is_enabled INTEGER DEFAULT 1,
    installation_date DATE,
    last_maintenance DATE,
    
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (device_group_id) REFERENCES device_groups(id) ON DELETE SET NULL,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
    FOREIGN KEY (created_by) REFERENCES users(id)
);

-- 디바이스 상태 테이블
CREATE TABLE IF NOT EXISTS device_status (
    device_id INTEGER PRIMARY KEY,
    connection_status VARCHAR(20) NOT NULL DEFAULT 'disconnected',
    last_communication DATETIME,
    error_count INTEGER DEFAULT 0,
    last_error TEXT,
    response_time INTEGER,
    
    -- 추가 진단 정보
    firmware_version VARCHAR(50),
    hardware_info TEXT, -- JSON 형태
    diagnostic_data TEXT, -- JSON 형태
    
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- 데이터 포인트 테이블
CREATE TABLE IF NOT EXISTS data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    
    -- 포인트 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    address INTEGER NOT NULL,
    data_type VARCHAR(20) NOT NULL, -- bool, int16, int32, uint16, uint32, float, double, string
    access_mode VARCHAR(10) DEFAULT 'read', -- read, write, readwrite
    
    -- 엔지니어링 정보
    unit VARCHAR(20),
    scaling_factor DECIMAL(10,4) DEFAULT 1.0,
    scaling_offset DECIMAL(10,4) DEFAULT 0.0,
    min_value DECIMAL(15,4),
    max_value DECIMAL(15,4),
    
    -- 수집 설정
    is_enabled INTEGER DEFAULT 1,
    scan_rate INTEGER, -- 개별 스캔 주기 (디바이스 기본값 오버라이드)
    deadband DECIMAL(10,4) DEFAULT 0,
    
    -- 메타데이터
    config TEXT, -- JSON 형태
    tags TEXT, -- JSON 배열 형태
    
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    UNIQUE(device_id, address)
);

-- 현재값 테이블
CREATE TABLE IF NOT EXISTS current_values (
    point_id INTEGER PRIMARY KEY,
    value DECIMAL(15,4),
    raw_value DECIMAL(15,4),
    string_value TEXT,
    quality VARCHAR(20) DEFAULT 'good', -- good, bad, uncertain, not_connected
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE
);
