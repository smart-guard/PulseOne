-- =============================================================================
-- backend/lib/database/schemas/03-device-tables.sql
-- 디바이스 및 데이터 포인트 테이블 (SQLite 버전) - 2025-08-14 최신 업데이트
-- PulseOne v2.1.0 완전 호환, C++ Struct DataPoint 100% 반영
-- =============================================================================

-- =============================================================================

-- =============================================================================
-- 제조사 및 모델 정보 (공통 참조)
-- =============================================================================
CREATE TABLE IF NOT EXISTS manufacturers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL UNIQUE,
    description TEXT,
    country VARCHAR(50),
    website VARCHAR(255),
    logo_url VARCHAR(255),
    is_active INTEGER DEFAULT 1,
    is_deleted INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS device_models (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    manufacturer_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    model_number VARCHAR(100),
    device_type VARCHAR(50),                             -- PLC, HMI, SENSOR, etc.
    description TEXT,
    image_url VARCHAR(255),
    manual_url VARCHAR(255),
    metadata TEXT,                                       -- JSON 형태
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (manufacturer_id) REFERENCES manufacturers(id) ON DELETE CASCADE,
    UNIQUE(manufacturer_id, name)
);

-- =============================================================================
-- 디바이스 그룹 테이블
-- =============================================================================
CREATE TABLE IF NOT EXISTS device_groups (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER NOT NULL,
    parent_group_id INTEGER,                              -- 계층 그룹 지원
    
    -- 🔥 그룹 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    group_type VARCHAR(50) DEFAULT 'functional',          -- functional, physical, protocol, location
    
    -- 🔥 메타데이터
    tags TEXT,                                           -- JSON 배열
    metadata TEXT,                                       -- JSON 객체
    
    -- 🔥 상태 정보
    is_active INTEGER DEFAULT 1,
    sort_order INTEGER DEFAULT 0,
    
    -- 🔥 감사 정보
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (parent_group_id) REFERENCES device_groups(id) ON DELETE SET NULL,
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_group_type CHECK (group_type IN ('functional', 'physical', 'protocol', 'location'))
);

-- =============================================================================
-- 장치-그룹 다중 배정 테이블 (N:N 관계)
-- =============================================================================
CREATE TABLE IF NOT EXISTS device_group_assignments (
    device_id INTEGER NOT NULL,
    group_id INTEGER NOT NULL,
    is_primary INTEGER DEFAULT 0,                         -- 대표 그룹 여부
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (device_id, group_id),
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    FOREIGN KEY (group_id) REFERENCES device_groups(id) ON DELETE CASCADE
);

-- =============================================================================
-- 드라이버 플러그인 테이블
-- =============================================================================
CREATE TABLE IF NOT EXISTS driver_plugins (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 🔥 플러그인 기본 정보
    name VARCHAR(100) NOT NULL,
    protocol_type VARCHAR(50) NOT NULL,
    version VARCHAR(20) NOT NULL,
    file_path VARCHAR(255) NOT NULL,
    description TEXT,
    
    -- 🔥 플러그인 메타데이터
    author VARCHAR(100),
    website VARCHAR(255),
    documentation_url VARCHAR(255),
    api_version INTEGER DEFAULT 1,
    min_system_version VARCHAR(20),
    
    -- 🔥 설정 스키마
    config_schema TEXT,                                   -- JSON 스키마 정의
    default_config TEXT,                                 -- JSON 기본 설정값
    
    -- 🔥 상태 정보
    is_enabled INTEGER DEFAULT 1,
    is_system INTEGER DEFAULT 0,                         -- 시스템 제공 드라이버
    
    -- 🔥 라이선스 정보
    license_type VARCHAR(20) DEFAULT 'free',             -- free, commercial, trial
    license_expires DATETIME,
    license_key VARCHAR(255),
    
    -- 🔥 성능 정보
    max_concurrent_connections INTEGER DEFAULT 10,
    supported_features TEXT,                             -- JSON 배열
    
    -- 🔥 감사 정보
    installed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 🔥 제약조건
    UNIQUE(protocol_type, version),
    CONSTRAINT chk_license_type CHECK (license_type IN ('free', 'commercial', 'trial'))
);

-- =============================================================================
-- 디바이스 테이블 - 완전한 프로토콜 지원
-- =============================================================================
CREATE TABLE IF NOT EXISTS devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER NOT NULL,
    device_group_id INTEGER,
    edge_server_id INTEGER,
    
    -- 디바이스 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    device_type VARCHAR(50) NOT NULL,                    -- PLC, HMI, SENSOR, GATEWAY, METER, CONTROLLER, ROBOT, INVERTER
    manufacturer VARCHAR(100),
    model VARCHAR(100),
    serial_number VARCHAR(100),
    firmware_version VARCHAR(50),
    
    -- 프로토콜 설정 (외래키 사용)
    protocol_id INTEGER NOT NULL,
    endpoint VARCHAR(255) NOT NULL,                      -- IP:Port 또는 연결 문자열
    config TEXT NOT NULL,                               -- JSON 형태 프로토콜별 설정
    
    -- 기본 수집 설정 (세부 설정은 device_settings 테이블 참조)
    polling_interval INTEGER DEFAULT 1000,               -- 밀리초
    timeout INTEGER DEFAULT 3000,                       -- 밀리초
    retry_count INTEGER DEFAULT 3,
    
    -- 물리적 정보
    location_description VARCHAR(200),
    installation_date DATE,
    last_maintenance DATE,
    next_maintenance DATE,
    warranty_expires DATE,
    
    -- 상태 정보
    is_enabled INTEGER DEFAULT 1,
    is_deleted INTEGER DEFAULT 0,                       -- ⬅️ Added for soft delete
    is_simulation_mode INTEGER DEFAULT 0,               -- 시뮬레이션 모드
    priority INTEGER DEFAULT 100,                       -- 수집 우선순위 (낮을수록 높은 우선순위)
    
    -- 메타데이터
    tags TEXT,                                          -- JSON 배열
    metadata TEXT,                                      -- JSON 객체
    custom_fields TEXT,                                 -- JSON 객체 (사용자 정의 필드)
    
    -- 템플릿 및 제조사 연동
    template_device_id INTEGER,
    manufacturer_id INTEGER,
    model_id INTEGER,
    protocol_instance_id INTEGER,                        -- 프로토콜 인스턴스 참조
    
    -- 감사 정보
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (device_group_id) REFERENCES device_groups(id) ON DELETE SET NULL,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
    FOREIGN KEY (protocol_id) REFERENCES protocols(id) ON DELETE RESTRICT,
    FOREIGN KEY (protocol_instance_id) REFERENCES protocol_instances(id) ON DELETE SET NULL,
    FOREIGN KEY (created_by) REFERENCES users(id),
    FOREIGN KEY (manufacturer_id) REFERENCES manufacturers(id) ON DELETE SET NULL,
    FOREIGN KEY (model_id) REFERENCES device_models(id) ON DELETE SET NULL,
    FOREIGN KEY (template_device_id) REFERENCES template_devices(id) ON DELETE SET NULL,
    
    -- 제약조건
    CONSTRAINT chk_device_type CHECK (device_type IN ('PLC', 'HMI', 'SENSOR', 'GATEWAY', 'METER', 'CONTROLLER', 'ROBOT', 'INVERTER', 'DRIVE', 'SWITCH'))
);

-- =============================================================================
-- 🔥🔥🔥 디바이스 세부 설정 테이블 (고급 통신 설정)
-- =============================================================================
CREATE TABLE IF NOT EXISTS device_settings (
    device_id INTEGER PRIMARY KEY,
    
    -- 🔥 폴링 및 타이밍 설정
    polling_interval_ms INTEGER DEFAULT 1000,
    scan_rate_override INTEGER,                          -- 개별 디바이스 스캔 주기 오버라이드
    scan_group INTEGER DEFAULT 1,                       -- 스캔 그룹 (동시 스캔 제어)
    
    -- 🔥 연결 및 통신 설정
    connection_timeout_ms INTEGER DEFAULT 10000,
    read_timeout_ms INTEGER DEFAULT 5000,
    write_timeout_ms INTEGER DEFAULT 5000,
    inter_frame_delay_ms INTEGER DEFAULT 10,            -- 프레임 간 지연
    
    -- 🔥 재시도 정책
    max_retry_count INTEGER DEFAULT 3,
    retry_interval_ms INTEGER DEFAULT 5000,
    backoff_multiplier DECIMAL(3,2) DEFAULT 1.5,        -- 지수 백오프
    backoff_time_ms INTEGER DEFAULT 60000,
    max_backoff_time_ms INTEGER DEFAULT 300000,         -- 최대 5분
    
    -- 🔥 Keep-alive 설정
    keep_alive_enabled INTEGER DEFAULT 1,
    keep_alive_interval_s INTEGER DEFAULT 30,
    keep_alive_timeout_s INTEGER DEFAULT 10,
    
    -- 🔥 데이터 품질 관리
    data_validation_enabled INTEGER DEFAULT 1,
    outlier_detection_enabled INTEGER DEFAULT 0,
    deadband_enabled INTEGER DEFAULT 1,
    
    -- 🔥 로깅 및 진단
    detailed_logging_enabled INTEGER DEFAULT 0,
    performance_monitoring_enabled INTEGER DEFAULT 1,
    diagnostic_mode_enabled INTEGER DEFAULT 0,
    communication_logging_enabled INTEGER DEFAULT 0,    -- 통신 로그 기록
    
    -- 🔥 버퍼링 설정
    read_buffer_size INTEGER DEFAULT 1024,
    write_buffer_size INTEGER DEFAULT 1024,
    queue_size INTEGER DEFAULT 100,                     -- 명령 큐 크기
    
    -- 🔥 메타데이터
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_by INTEGER,                                 -- 설정을 변경한 사용자
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    FOREIGN KEY (updated_by) REFERENCES users(id)
);

-- =============================================================================
-- 디바이스 상태 테이블
-- =============================================================================
CREATE TABLE IF NOT EXISTS device_status (
    device_id INTEGER PRIMARY KEY,
    
    -- 🔥 연결 상태
    connection_status VARCHAR(20) NOT NULL DEFAULT 'disconnected',  -- connected, disconnected, connecting, error
    last_communication DATETIME,
    connection_established_at DATETIME,
    
    -- 🔥 에러 정보
    error_count INTEGER DEFAULT 0,
    last_error TEXT,
    last_error_time DATETIME,
    consecutive_error_count INTEGER DEFAULT 0,
    
    -- 🔥 성능 정보
    response_time INTEGER,                              -- 평균 응답 시간 (ms)
    min_response_time INTEGER,                          -- 최소 응답 시간 (ms)
    max_response_time INTEGER,                          -- 최대 응답 시간 (ms)
    throughput_ops_per_sec REAL DEFAULT 0.0,           -- 초당 처리 작업 수
    
    -- 🔥 통계 정보
    total_requests INTEGER DEFAULT 0,
    successful_requests INTEGER DEFAULT 0,
    failed_requests INTEGER DEFAULT 0,
    uptime_percentage REAL DEFAULT 0.0,
    
    -- 🔥 디바이스 진단 정보
    firmware_version VARCHAR(50),
    hardware_info TEXT,                                 -- JSON 형태
    diagnostic_data TEXT,                               -- JSON 형태
    cpu_usage REAL,                                     -- 디바이스 CPU 사용률
    memory_usage REAL,                                  -- 디바이스 메모리 사용률
    
    -- 🔥 메타데이터
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    CONSTRAINT chk_connection_status CHECK (connection_status IN ('connected', 'disconnected', 'connecting', 'error', 'maintenance'))
);

-- =============================================================================
-- 🔥🔥🔥 데이터 포인트 테이블 - C++ Struct DataPoint 100% 반영!
-- =============================================================================
CREATE TABLE IF NOT EXISTS data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    
    -- 🔥 기본 식별 정보 (Struct DataPoint와 완전 일치)
    name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 🔥 주소 정보 (Struct DataPoint와 완전 일치)
    address INTEGER NOT NULL,                           -- uint32_t address
    address_string VARCHAR(255),                        -- std::string address_string (별칭)
    
    -- 🔥 데이터 타입 및 접근성 (Struct DataPoint와 완전 일치)
    data_type VARCHAR(20) NOT NULL DEFAULT 'UNKNOWN',   -- std::string data_type
    access_mode VARCHAR(10) DEFAULT 'read',             -- std::string access_mode (read, write, read_write)
    is_enabled INTEGER DEFAULT 1,                      -- bool is_enabled
    is_writable INTEGER DEFAULT 0,                     -- bool is_writable (계산됨)
    
    -- 🔥 엔지니어링 단위 및 스케일링 (Struct DataPoint와 완전 일치)
    unit VARCHAR(50),                                   -- std::string unit
    scaling_factor REAL DEFAULT 1.0,                   -- double scaling_factor
    scaling_offset REAL DEFAULT 0.0,                   -- double scaling_offset
    min_value REAL DEFAULT 0.0,                        -- double min_value
    max_value REAL DEFAULT 0.0,                        -- double max_value
    
    -- 🔥🔥🔥 로깅 및 수집 설정 (SQLQueries.h가 찾던 핵심 컬럼들!)
    log_enabled INTEGER DEFAULT 1,                     -- bool log_enabled ✅
    log_interval_ms INTEGER DEFAULT 0,                 -- uint32_t log_interval_ms ✅
    log_deadband REAL DEFAULT 0.0,                     -- double log_deadband ✅
    polling_interval_ms INTEGER DEFAULT 0,             -- uint32_t polling_interval_ms
    
    -- 🔥 데이터 품질 설정
    quality_check_enabled INTEGER DEFAULT 1,
    range_check_enabled INTEGER DEFAULT 1,
    rate_of_change_limit REAL DEFAULT 0.0,             -- 변화율 제한
    
    -- 🔥🔥🔥 메타데이터 (SQLQueries.h가 찾던 핵심 컬럼들!)
    group_name VARCHAR(50),                             -- std::string group
    tags TEXT,                                          -- std::string tags (JSON 배열) ✅
    metadata TEXT,                                      -- std::string metadata (JSON 객체) ✅
    protocol_params TEXT,                               -- JSON: 프로토콜별 매개변수
    
    -- 🔥 알람 관련 설정
    alarm_enabled INTEGER DEFAULT 0,
    high_alarm_limit REAL,
    low_alarm_limit REAL,
    alarm_deadband REAL DEFAULT 0.0,
    
    -- 🔥 보관 정책
    retention_policy VARCHAR(20) DEFAULT 'standard',    -- standard, extended, minimal, custom
    compression_enabled INTEGER DEFAULT 1,
    
    -- 🔥 시간 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_deleted TINYINT DEFAULT 0,                        -- ⬅️ Match live schema
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    UNIQUE(device_id, address),
    CONSTRAINT chk_data_type CHECK (data_type IN ('BOOL', 'INT8', 'UINT8', 'INT16', 'UINT16', 'INT32', 'UINT32', 'INT64', 'UINT64', 'FLOAT32', 'FLOAT64', 'STRING', 'UNKNOWN')),
    CONSTRAINT chk_access_mode CHECK (access_mode IN ('read', 'write', 'read_write')),
    CONSTRAINT chk_retention_policy CHECK (retention_policy IN ('standard', 'extended', 'minimal', 'custom'))
);

-- =============================================================================
-- 🔥🔥🔥 현재값 테이블 - JSON 기반 DataVariant 저장
-- =============================================================================
CREATE TABLE IF NOT EXISTS current_values (
    point_id INTEGER PRIMARY KEY,
    
    -- 🔥 실제 값 (DataVariant 직렬화)
    current_value TEXT,                                 -- JSON으로 DataVariant 저장 
    raw_value TEXT,                                     -- JSON으로 원시값 저장
    previous_value TEXT,                                -- JSON으로 이전값 저장
    
    -- 🔥 데이터 타입 정보
    value_type VARCHAR(10) DEFAULT 'double',            -- bool, int16, uint16, int32, uint32, float, double, string
    
    -- 🔥 데이터 품질 및 상태
    quality_code INTEGER DEFAULT 0,                    -- DataQuality enum 값
    quality VARCHAR(20) DEFAULT 'not_connected',       -- 텍스트 표현 (good, bad, uncertain, not_connected)
    
    -- 🔥 타임스탬프들
    value_timestamp DATETIME,                          -- 값 변경 시간
    quality_timestamp DATETIME,                        -- 품질 변경 시간  
    last_log_time DATETIME,                            -- 마지막 로깅 시간
    last_read_time DATETIME,                           -- 마지막 읽기 시간
    last_write_time DATETIME,                          -- 마지막 쓰기 시간
    
    -- 🔥 통계 카운터들
    read_count INTEGER DEFAULT 0,                      -- 읽기 횟수
    write_count INTEGER DEFAULT 0,                     -- 쓰기 횟수
    error_count INTEGER DEFAULT 0,                     -- 에러 횟수
    change_count INTEGER DEFAULT 0,                    -- 값 변경 횟수
    
    -- 🔥 알람 상태
    alarm_state VARCHAR(20) DEFAULT 'normal',          -- normal, high, low, critical
    alarm_active INTEGER DEFAULT 0,
    alarm_acknowledged INTEGER DEFAULT 0,
    
    -- 🔥 메타데이터
    source_info TEXT,                                  -- JSON: 값 소스 정보
    
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    CONSTRAINT chk_value_type CHECK (value_type IN ('bool', 'int8', 'uint8', 'int16', 'uint16', 'int32', 'uint32', 'int64', 'uint64', 'float', 'double', 'string')),
    CONSTRAINT chk_quality CHECK (quality IN ('good', 'bad', 'uncertain', 'not_connected', 'device_failure', 'sensor_failure', 'comm_failure', 'out_of_service')),
    CONSTRAINT chk_alarm_state CHECK (alarm_state IN ('normal', 'high', 'low', 'critical', 'warning'))
);

-- =============================================================================
-- 인덱스 생성 (성능 최적화)
-- =============================================================================

-- device_groups 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_device_groups_tenant ON device_groups(tenant_id);
CREATE INDEX IF NOT EXISTS idx_device_groups_site ON device_groups(site_id);
CREATE INDEX IF NOT EXISTS idx_device_groups_parent ON device_groups(parent_group_id);
CREATE INDEX IF NOT EXISTS idx_device_groups_type ON device_groups(group_type);

-- driver_plugins 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_driver_plugins_protocol ON driver_plugins(protocol_type);
CREATE INDEX IF NOT EXISTS idx_driver_plugins_enabled ON driver_plugins(is_enabled);

-- devices 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_devices_tenant ON devices(tenant_id);
CREATE INDEX IF NOT EXISTS idx_devices_site ON devices(site_id);
CREATE INDEX IF NOT EXISTS idx_devices_group ON devices(device_group_id);
CREATE INDEX IF NOT EXISTS idx_devices_edge_server ON devices(edge_server_id);
CREATE INDEX IF NOT EXISTS idx_devices_protocol ON devices(protocol_id);
CREATE INDEX IF NOT EXISTS idx_devices_type ON devices(device_type);
CREATE INDEX IF NOT EXISTS idx_devices_enabled ON devices(is_enabled);
CREATE INDEX IF NOT EXISTS idx_devices_name ON devices(tenant_id, name);

-- device_settings 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_device_settings_device_id ON device_settings(device_id);
CREATE INDEX IF NOT EXISTS idx_device_settings_polling ON device_settings(polling_interval_ms);

-- device_status 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_device_status_connection ON device_status(connection_status);
CREATE INDEX IF NOT EXISTS idx_device_status_last_comm ON device_status(last_communication DESC);

-- data_points 테이블 인덱스 (핵심 성능 최적화)
CREATE INDEX IF NOT EXISTS idx_data_points_device ON data_points(device_id);
CREATE INDEX IF NOT EXISTS idx_data_points_enabled ON data_points(is_enabled);
CREATE INDEX IF NOT EXISTS idx_data_points_address ON data_points(device_id, address);
CREATE INDEX IF NOT EXISTS idx_data_points_type ON data_points(data_type);
CREATE INDEX IF NOT EXISTS idx_data_points_log_enabled ON data_points(log_enabled);
CREATE INDEX IF NOT EXISTS idx_data_points_name ON data_points(device_id, name);
CREATE INDEX IF NOT EXISTS idx_data_points_group ON data_points(group_name);

-- current_values 테이블 인덱스 (실시간 조회 최적화)
CREATE INDEX IF NOT EXISTS idx_current_values_timestamp ON current_values(value_timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_current_values_quality ON current_values(quality_code);
CREATE INDEX IF NOT EXISTS idx_current_values_updated ON current_values(updated_at DESC);
CREATE INDEX IF NOT EXISTS idx_current_values_alarm ON current_values(alarm_active);
CREATE INDEX IF NOT EXISTS idx_current_values_quality_name ON current_values(quality);

-- =============================================================================
-- 디바이스 템플릿 테이블 (운영 디바이스 생성을 위한 참조)
-- =============================================================================
CREATE TABLE IF NOT EXISTS template_devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    model_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,                           -- 템플릿 명칭
    description TEXT,
    protocol_id INTEGER NOT NULL,
    config TEXT NOT NULL,                                -- 기본 프로토콜 설정 (JSON)
    polling_interval INTEGER DEFAULT 1000,
    timeout INTEGER DEFAULT 3000,
    is_public TINYINT DEFAULT 1,                         -- 시스템 공유 여부
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_deleted TINYINT DEFAULT 0,
    FOREIGN KEY (model_id) REFERENCES device_models(id) ON DELETE CASCADE,
    FOREIGN KEY (protocol_id) REFERENCES protocols(id) ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS template_device_settings (
    template_device_id INTEGER PRIMARY KEY,
    polling_interval_ms INTEGER DEFAULT 1000,
    connection_timeout_ms INTEGER DEFAULT 10000,
    read_timeout_ms INTEGER DEFAULT 5000,
    write_timeout_ms INTEGER DEFAULT 5000,
    max_retry_count INTEGER DEFAULT 3,
    FOREIGN KEY (template_device_id) REFERENCES template_devices(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS template_data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    template_device_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    address INTEGER NOT NULL,
    address_string VARCHAR(255),
    data_type VARCHAR(20) NOT NULL,
    access_mode VARCHAR(10) DEFAULT 'read',
    unit VARCHAR(50),
    scaling_factor REAL DEFAULT 1.0,
    is_writable INTEGER DEFAULT 0,
    is_active INTEGER DEFAULT 1,
    sort_order INTEGER DEFAULT 0,
    metadata TEXT, 
    scaling_offset REAL DEFAULT 0.0, 
    protocol_params TEXT, 
    mapping_key VARCHAR(255),                            -- JSON 형태
    FOREIGN KEY (template_device_id) REFERENCES template_devices(id) ON DELETE CASCADE
);

-- 템플릿 인덱스
CREATE INDEX IF NOT EXISTS idx_template_devices_model ON template_devices(model_id);
CREATE INDEX IF NOT EXISTS idx_template_data_points_template ON template_data_points(template_device_id);
CREATE INDEX IF NOT EXISTS idx_manufacturers_name ON manufacturers(name);
CREATE INDEX IF NOT EXISTS idx_device_models_manufacturer ON device_models(manufacturer_id);