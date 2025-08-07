-- =============================================================================
-- backend/lib/database/schemas/03-device-tables.sql
-- 디바이스 및 데이터 포인트 테이블 (SQLite 버전) - 업데이트됨
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
    
    -- 수집 설정 (기본값만, 세부 설정은 device_settings 테이블 참조)
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

-- 🔥 새로 추가: 디바이스 세부 설정 테이블
CREATE TABLE IF NOT EXISTS device_settings (
    device_id INTEGER PRIMARY KEY,
    
    -- 폴링 및 타이밍 설정
    polling_interval_ms INTEGER DEFAULT 1000,
    scan_rate_override INTEGER, -- 개별 디바이스 스캔 주기 오버라이드
    
    -- 연결 및 통신 설정
    connection_timeout_ms INTEGER DEFAULT 10000,
    read_timeout_ms INTEGER DEFAULT 5000,
    write_timeout_ms INTEGER DEFAULT 5000,
    
    -- 재시도 정책
    max_retry_count INTEGER DEFAULT 3,
    retry_interval_ms INTEGER DEFAULT 5000,
    backoff_multiplier DECIMAL(3,2) DEFAULT 1.5, -- 지수 백오프
    backoff_time_ms INTEGER DEFAULT 60000,
    max_backoff_time_ms INTEGER DEFAULT 300000, -- 최대 5분
    
    -- Keep-alive 설정
    keep_alive_enabled INTEGER DEFAULT 1,
    keep_alive_interval_s INTEGER DEFAULT 30,
    keep_alive_timeout_s INTEGER DEFAULT 10,
    
    -- 데이터 품질 관리
    data_validation_enabled INTEGER DEFAULT 1,
    outlier_detection_enabled INTEGER DEFAULT 0,
    deadband_enabled INTEGER DEFAULT 1,
    
    -- 로깅 및 진단
    detailed_logging_enabled INTEGER DEFAULT 0,
    performance_monitoring_enabled INTEGER DEFAULT 1,
    diagnostic_mode_enabled INTEGER DEFAULT 0,
    
    -- 메타데이터
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_by INTEGER, -- 설정을 변경한 사용자
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    FOREIGN KEY (updated_by) REFERENCES users(id)
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

CREATE TABLE IF NOT EXISTS data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    
    -- 🔥 기본 식별 정보 (Struct DataPoint와 일치)
    name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 🔥 주소 정보 (Struct DataPoint와 일치)
    address INTEGER NOT NULL,                    -- uint32_t address
    address_string VARCHAR(255),                 -- std::string address_string
    
    -- 🔥 데이터 타입 및 접근성 (Struct DataPoint와 일치)
    data_type VARCHAR(20) NOT NULL DEFAULT 'UNKNOWN',  -- std::string data_type
    access_mode VARCHAR(10) DEFAULT 'read',             -- std::string access_mode
    is_enabled INTEGER DEFAULT 1,                       -- bool is_enabled
    is_writable INTEGER DEFAULT 0,                      -- bool is_writable (계산됨)
    
    -- 🔥 엔지니어링 단위 및 스케일링 (Struct DataPoint와 일치)
    unit VARCHAR(50),                            -- std::string unit
    scaling_factor REAL DEFAULT 1.0,            -- double scaling_factor
    scaling_offset REAL DEFAULT 0.0,            -- double scaling_offset
    min_value REAL DEFAULT 0.0,                 -- double min_value
    max_value REAL DEFAULT 0.0,                 -- double max_value
    
    -- 🔥🔥🔥 로깅 및 수집 설정 (SQLQueries.h가 찾던 컬럼들!)
    log_enabled INTEGER DEFAULT 1,              -- bool log_enabled ✅
    log_interval_ms INTEGER DEFAULT 0,          -- uint32_t log_interval_ms ✅
    log_deadband REAL DEFAULT 0.0,              -- double log_deadband ✅
    polling_interval_ms INTEGER DEFAULT 0,      -- uint32_t polling_interval_ms
    
    -- 🔥🔥🔥 메타데이터 (SQLQueries.h가 찾던 컬럼들!)
    group_name VARCHAR(50),                      -- std::string group
    tags TEXT,                                   -- std::string tags (JSON 배열) ✅
    metadata TEXT,                               -- std::string metadata (JSON 객체) ✅
    protocol_params TEXT,                        -- JSON for protocol-specific params
    
    -- 🔥 시간 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    UNIQUE(device_id, address)
);

-- 현재값 테이블 (업데이트됨)
CREATE TABLE IF NOT EXISTS current_values (
    point_id INTEGER PRIMARY KEY,
    
    -- 🔥 실제 값 (타입별로 분리하지 않고 통합)
    current_value TEXT,                          -- JSON으로 DataVariant 저장 
    raw_value TEXT,                              -- JSON으로 DataVariant 저장
    value_type VARCHAR(10) DEFAULT 'double',     -- bool, int16, uint16, int32, uint32, float, double, string
    
    -- 🔥 데이터 품질 및 타임스탬프
    quality_code INTEGER DEFAULT 0,             -- DataQuality enum 값
    quality VARCHAR(20) DEFAULT 'not_connected', -- 텍스트 표현
    
    -- 🔥 타임스탬프들
    value_timestamp DATETIME,                   -- 값 변경 시간
    quality_timestamp DATETIME,                 -- 품질 변경 시간  
    last_log_time DATETIME,                     -- 마지막 로깅 시간
    last_read_time DATETIME,                    -- 마지막 읽기 시간
    last_write_time DATETIME,                   -- 마지막 쓰기 시간
    
    -- 🔥 통계 카운터들
    read_count INTEGER DEFAULT 0,               -- 읽기 횟수
    write_count INTEGER DEFAULT 0,              -- 쓰기 횟수
    error_count INTEGER DEFAULT 0,              -- 에러 횟수
    
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE
);

-- =============================================================================
-- 인덱스 생성
-- =============================================================================

-- 기존 인덱스들
CREATE INDEX IF NOT EXISTS idx_devices_tenant ON devices(tenant_id);
CREATE INDEX IF NOT EXISTS idx_devices_site ON devices(site_id);
CREATE INDEX IF NOT EXISTS idx_devices_group ON devices(device_group_id);
CREATE INDEX IF NOT EXISTS idx_devices_edge_server ON devices(edge_server_id);
CREATE INDEX IF NOT EXISTS idx_devices_protocol ON devices(protocol_type);
CREATE INDEX IF NOT EXISTS idx_devices_type ON devices(device_type);
CREATE INDEX IF NOT EXISTS idx_devices_enabled ON devices(is_enabled);

-- 기본 검색 인덱스
CREATE INDEX IF NOT EXISTS idx_data_points_device ON data_points(device_id);
CREATE INDEX IF NOT EXISTS idx_data_points_enabled ON data_points(is_enabled);
CREATE INDEX IF NOT EXISTS idx_data_points_address ON data_points(device_id, address);
CREATE INDEX IF NOT EXISTS idx_data_points_type ON data_points(data_type);
CREATE INDEX IF NOT EXISTS idx_data_points_log_enabled ON data_points(log_enabled);

-- 현재값 인덱스
CREATE INDEX IF NOT EXISTS idx_current_values_timestamp ON current_values(value_timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_current_values_quality ON current_values(quality_code);
CREATE INDEX IF NOT EXISTS idx_current_values_updated ON current_values(updated_at DESC);
-- 🔥 새로운 인덱스들
CREATE INDEX IF NOT EXISTS idx_device_settings_device_id ON device_settings(device_id);
CREATE INDEX IF NOT EXISTS idx_device_settings_polling ON device_settings(polling_interval_ms);

