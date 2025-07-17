-- backend/lib/database/schemas/device-tables.sql
-- PulseOne 디바이스 및 데이터 포인트 테이블 (다중 프로토콜 지원)

-- 디바이스 그룹 테이블
CREATE TABLE IF NOT EXISTS device_groups (
    id {{AUTO_INCREMENT}},
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    parent_group_id INTEGER,
    created_at {{TIMESTAMP}},
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (parent_group_id) REFERENCES device_groups(id) ON DELETE SET NULL
);

-- 드라이버 플러그인 테이블
CREATE TABLE IF NOT EXISTS driver_plugins (
    id {{AUTO_INCREMENT}},
    name VARCHAR(100) NOT NULL,
    protocol_type VARCHAR(50) NOT NULL,
    version VARCHAR(20) NOT NULL,
    file_path VARCHAR(255) NOT NULL,
    description TEXT,
    
    -- 플러그인 정보
    author VARCHAR(100),
    api_version INTEGER DEFAULT 1,
    is_enabled {{BOOLEAN}} DEFAULT {{TRUE}},
    config_schema {{TEXT}},
    
    -- 라이선스 정보
    license_type VARCHAR(20) DEFAULT 'free',
    license_expires {{TIMESTAMP}},
    
    installed_at {{TIMESTAMP}},
    updated_at {{TIMESTAMP}},
    
    UNIQUE(protocol_type, version)
);

-- 디바이스 테이블 (다중 프로토콜 지원)
CREATE TABLE IF NOT EXISTS devices (
    id {{AUTO_INCREMENT}},
    tenant_id INTEGER NOT NULL,
    edge_server_id INTEGER,
    device_group_id INTEGER,
    
    -- 디바이스 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    device_type VARCHAR(50),
    manufacturer VARCHAR(100),
    model VARCHAR(100),
    serial_number VARCHAR(100),
    
    -- 통신 설정 (프로토콜별 공통)
    protocol_type VARCHAR(50) NOT NULL,
    endpoint VARCHAR(255) NOT NULL,
    config {{TEXT}} NOT NULL,
    
    -- 수집 설정
    polling_interval INTEGER DEFAULT 1000,
    timeout INTEGER DEFAULT 3000,
    retry_count INTEGER DEFAULT 3,
    
    -- 상태 정보
    is_enabled {{BOOLEAN}} DEFAULT {{TRUE}},
    installation_date DATE,
    last_maintenance DATE,
    
    created_by INTEGER,
    created_at {{TIMESTAMP}},
    updated_at {{TIMESTAMP}},
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
    FOREIGN KEY (device_group_id) REFERENCES device_groups(id) ON DELETE SET NULL
);

-- 디바이스 상태 테이블
CREATE TABLE IF NOT EXISTS device_status (
    device_id INTEGER PRIMARY KEY,
    connection_status VARCHAR(20) NOT NULL DEFAULT 'disconnected',
    last_communication {{TIMESTAMP}},
    error_count INTEGER DEFAULT 0,
    last_error TEXT,
    response_time INTEGER,
    
    -- 추가 진단 정보
    firmware_version VARCHAR(50),
    hardware_info {{TEXT}},
    diagnostic_data {{TEXT}},
    
    updated_at {{TIMESTAMP}},
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- 데이터 포인트 테이블 (프로토콜별 공통 구조)
CREATE TABLE IF NOT EXISTS data_points (
    id {{AUTO_INCREMENT}},
    device_id INTEGER NOT NULL,
    
    -- 포인트 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    address INTEGER NOT NULL,
    data_type VARCHAR(20) NOT NULL,
    access_mode VARCHAR(10) DEFAULT 'read',
    
    -- 엔지니어링 정보
    unit VARCHAR(20),
    scaling_factor DECIMAL(10,4) DEFAULT 1.0,
    scaling_offset DECIMAL(10,4) DEFAULT 0.0,
    min_value DECIMAL(15,4),
    max_value DECIMAL(15,4),
    
    -- 수집 설정
    is_enabled {{BOOLEAN}} DEFAULT {{TRUE}},
    scan_rate INTEGER,
    deadband DECIMAL(10,4) DEFAULT 0,
    
    -- 메타데이터
    config {{TEXT}},
    tags {{TEXT}},
    
    created_at {{TIMESTAMP}},
    updated_at {{TIMESTAMP}},
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    UNIQUE(device_id, address)
);

-- 현재값 테이블
CREATE TABLE IF NOT EXISTS current_values (
    point_id INTEGER PRIMARY KEY,
    value DECIMAL(15,4),
    raw_value DECIMAL(15,4),
    string_value TEXT,
    quality VARCHAR(20) DEFAULT 'good',
    timestamp {{TIMESTAMP}},
    
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE
);

-- 가상 포인트 테이블
CREATE TABLE IF NOT EXISTS virtual_points (
    id {{AUTO_INCREMENT}},
    tenant_id INTEGER NOT NULL,
    
    -- 가상 포인트 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    formula TEXT NOT NULL,
    data_type VARCHAR(20) NOT NULL DEFAULT 'float',
    unit VARCHAR(20),
    
    -- 계산 설정
    calculation_interval INTEGER DEFAULT 1000,
    is_enabled {{BOOLEAN}} DEFAULT {{TRUE}},
    
    -- 메타데이터
    category VARCHAR(50),
    tags {{TEXT}},
    
    created_by INTEGER,
    created_at {{TIMESTAMP}},
    updated_at {{TIMESTAMP}},
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);

-- 가상 포인트 입력 매핑
CREATE TABLE IF NOT EXISTS virtual_point_inputs (
    id {{AUTO_INCREMENT}},
    virtual_point_id INTEGER NOT NULL,
    variable_name VARCHAR(50) NOT NULL,
    source_type VARCHAR(20) NOT NULL,
    source_id INTEGER,
    constant_value DECIMAL(15,4),
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    UNIQUE(virtual_point_id, variable_name)
);