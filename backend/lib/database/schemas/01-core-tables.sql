-- =============================================================================
-- backend/lib/database/schemas/01-core-tables.sql
-- 핵심 시스템 테이블 (SQLite 버전) - 2025-08-14 최신 업데이트
-- PulseOne v2.1.0 완전 호환 스키마
-- =============================================================================

-- 스키마 버전 관리 테이블
CREATE TABLE IF NOT EXISTS schema_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    version VARCHAR(20) NOT NULL,
    applied_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    description TEXT
);

-- =============================================================================
-- 테넌트(회사) 테이블 - 멀티테넌트 SaaS 기반
-- =============================================================================
CREATE TABLE IF NOT EXISTS tenants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 🔥 기본 회사 정보
    company_name VARCHAR(100) NOT NULL UNIQUE,
    company_code VARCHAR(20) NOT NULL UNIQUE,
    domain VARCHAR(100) UNIQUE,
    
    -- 🔥 연락처 정보
    contact_name VARCHAR(100),
    contact_email VARCHAR(100),
    contact_phone VARCHAR(20),
    
    -- 🔥 SaaS 구독 정보
    subscription_plan VARCHAR(20) DEFAULT 'starter',      -- starter, professional, enterprise
    subscription_status VARCHAR(20) DEFAULT 'active',     -- active, trial, suspended, cancelled
    max_edge_servers INTEGER DEFAULT 3,
    max_data_points INTEGER DEFAULT 1000,
    max_users INTEGER DEFAULT 5,
    
    -- 🔥 billing 정보
    billing_cycle VARCHAR(20) DEFAULT 'monthly',          -- monthly, yearly
    subscription_start_date DATETIME,
    trial_end_date DATETIME,
    next_billing_date DATETIME,
    
    -- 🔥 상태 및 메타데이터
    is_active INTEGER DEFAULT 1,
    timezone VARCHAR(50) DEFAULT 'UTC',
    currency VARCHAR(3) DEFAULT 'USD',
    language VARCHAR(5) DEFAULT 'en',
    
    -- 🔥 감사 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 🔥 제약조건
    CONSTRAINT chk_subscription_plan CHECK (subscription_plan IN ('starter', 'professional', 'enterprise')),
    CONSTRAINT chk_subscription_status CHECK (subscription_status IN ('active', 'trial', 'suspended', 'cancelled'))
);

-- =============================================================================
-- Edge 서버 등록 테이블 - 분산 아키텍처 지원
-- =============================================================================
CREATE TABLE IF NOT EXISTS edge_servers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 🔥 서버 식별 정보
    server_name VARCHAR(100) NOT NULL,
    factory_name VARCHAR(100),
    location VARCHAR(200),
    
    -- 🔥 네트워크 정보
    ip_address VARCHAR(45),
    mac_address VARCHAR(17),
    hostname VARCHAR(100),
    port INTEGER DEFAULT 8080,
    
    -- 🔥 등록 및 보안
    registration_token VARCHAR(255) UNIQUE,
    activation_code VARCHAR(50),
    api_key VARCHAR(255),
    
    -- 🔥 상태 정보
    status VARCHAR(20) DEFAULT 'pending',                  -- pending, active, inactive, maintenance, error
    last_seen DATETIME,
    last_heartbeat DATETIME,
    version VARCHAR(20),
    
    -- 🔥 성능 정보
    cpu_usage REAL DEFAULT 0.0,
    memory_usage REAL DEFAULT 0.0,
    disk_usage REAL DEFAULT 0.0,
    uptime_seconds INTEGER DEFAULT 0,
    
    -- 🔥 설정 정보
    config TEXT,                                           -- JSON 형태 설정
    capabilities TEXT,                                     -- JSON 배열: 지원 프로토콜
    sync_enabled INTEGER DEFAULT 1,
    auto_update_enabled INTEGER DEFAULT 1,
    
    -- 🔥 메타데이터
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    CONSTRAINT chk_edge_status CHECK (status IN ('pending', 'active', 'inactive', 'maintenance', 'error'))
);

-- =============================================================================
-- 시스템 설정 테이블 - 키-값 저장소
-- =============================================================================
CREATE TABLE IF NOT EXISTS system_settings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 🔥 설정 키-값
    key_name VARCHAR(100) UNIQUE NOT NULL,
    value TEXT NOT NULL,
    description TEXT,
    
    -- 🔥 분류 및 접근성
    category VARCHAR(50) DEFAULT 'general',               -- general, security, performance, protocol
    data_type VARCHAR(20) DEFAULT 'string',               -- string, integer, boolean, json
    is_public INTEGER DEFAULT 0,                          -- 0=관리자만, 1=모든 사용자
    is_readonly INTEGER DEFAULT 0,                        -- 0=수정가능, 1=읽기전용
    
    -- 🔥 검증
    validation_regex VARCHAR(255),                        -- 값 검증용 정규식
    allowed_values TEXT,                                  -- JSON 배열: 허용값 목록
    min_value REAL,                                       -- 숫자형 최솟값
    max_value REAL,                                       -- 숫자형 최댓값
    
    -- 🔥 메타데이터
    updated_by INTEGER,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (updated_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_data_type CHECK (data_type IN ('string', 'integer', 'boolean', 'json', 'float'))
);

-- =============================================================================
-- 인덱스 생성 (성능 최적화)
-- =============================================================================

-- tenants 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_tenants_company_code ON tenants(company_code);
CREATE INDEX IF NOT EXISTS idx_tenants_active ON tenants(is_active);
CREATE INDEX IF NOT EXISTS idx_tenants_subscription ON tenants(subscription_status);
CREATE INDEX IF NOT EXISTS idx_tenants_domain ON tenants(domain);

-- edge_servers 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_edge_servers_tenant ON edge_servers(tenant_id);
CREATE INDEX IF NOT EXISTS idx_edge_servers_status ON edge_servers(status);
CREATE INDEX IF NOT EXISTS idx_edge_servers_last_seen ON edge_servers(last_seen DESC);
CREATE INDEX IF NOT EXISTS idx_edge_servers_token ON edge_servers(registration_token);
CREATE INDEX IF NOT EXISTS idx_edge_servers_hostname ON edge_servers(hostname);

-- system_settings 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_system_settings_category ON system_settings(category);
CREATE INDEX IF NOT EXISTS idx_system_settings_public ON system_settings(is_public);
CREATE INDEX IF NOT EXISTS idx_system_settings_updated ON system_settings(updated_at DESC);

-- =============================================================================
-- 기본 데이터 삽입
-- =============================================================================

-- 스키마 버전 기록
INSERT OR IGNORE INTO schema_versions (version, description) 
VALUES ('2.1.0', 'Complete PulseOne schema with DataPoint struct alignment and JSON protocol params');

-- 기본 시스템 설정값
INSERT OR IGNORE INTO system_settings (key_name, value, description, category, data_type) VALUES 
('system.version', '2.1.0', 'PulseOne system version', 'system', 'string'),
('system.timezone', 'Asia/Seoul', 'Default system timezone', 'system', 'string'),
('system.max_data_retention_days', '365', 'Maximum data retention period', 'system', 'integer'),
('system.log_level', 'INFO', 'Global log level', 'system', 'string'),

-- 데이터포인트 기본 설정
('datapoint.default_log_enabled', 'true', 'Default log_enabled for new data points', 'datapoint', 'boolean'),
('datapoint.default_log_interval_ms', '5000', 'Default log_interval_ms for new data points', 'datapoint', 'integer'),
('datapoint.default_polling_interval_ms', '1000', 'Default polling_interval_ms for new data points', 'datapoint', 'integer'),
('datapoint.default_deadband', '0.0', 'Default deadband for new data points', 'datapoint', 'float'),

-- 프로토콜 기본 설정
('protocol.modbus.default_timeout_ms', '3000', 'Default Modbus timeout', 'protocol', 'integer'),
('protocol.modbus.default_retry_count', '3', 'Default Modbus retry count', 'protocol', 'integer'),
('protocol.mqtt.default_qos', '1', 'Default MQTT QoS level', 'protocol', 'integer'),
('protocol.mqtt.default_keep_alive_s', '60', 'Default MQTT keep alive seconds', 'protocol', 'integer'),
('protocol.bacnet.default_timeout_ms', '5000', 'Default BACnet timeout', 'protocol', 'integer'),

-- 알람 시스템 설정
('alarm.default_severity', 'medium', 'Default alarm severity', 'alarm', 'string'),
('alarm.auto_acknowledge_timeout_min', '60', 'Auto acknowledge timeout in minutes', 'alarm', 'integer'),
('alarm.max_occurrences_per_hour', '100', 'Maximum alarm occurrences per hour', 'alarm', 'integer'),

-- 보안 설정
('security.session_timeout_min', '120', 'User session timeout in minutes', 'security', 'integer'),
('security.password_min_length', '8', 'Minimum password length', 'security', 'integer'),
('security.max_login_attempts', '5', 'Maximum login attempts before lockout', 'security', 'integer'),

-- 성능 설정
('performance.max_concurrent_workers', '10', 'Maximum concurrent protocol workers', 'performance', 'integer'),
('performance.batch_size', '100', 'Default batch processing size', 'performance', 'integer'),
('performance.cache_ttl_seconds', '3600', 'Default cache TTL in seconds', 'performance', 'integer');