-- ===============================================================================
-- PulseOne Database Schema Design
-- PostgreSQL 기반 관계형 데이터베이스 스키마
-- ===============================================================================

-- 확장 기능 활성화
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pg_trgm";

-- ===============================================================================
-- 1. 사용자 및 권한 관리
-- ===============================================================================

-- 사용자 테이블
CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(100),
    role VARCHAR(20) NOT NULL DEFAULT 'operator', -- admin, engineer, operator, viewer
    is_active BOOLEAN DEFAULT true,
    last_login TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 사용자 세션 테이블
CREATE TABLE user_sessions (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    token_hash VARCHAR(255) NOT NULL,
    ip_address INET,
    user_agent TEXT,
    expires_at TIMESTAMP WITH TIME ZONE NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- ===============================================================================
-- 2. 시스템 설정 및 프로젝트 관리
-- ===============================================================================

-- 프로젝트 테이블
CREATE TABLE projects (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name VARCHAR(100) NOT NULL,
    description TEXT,
    config JSONB,
    is_active BOOLEAN DEFAULT true,
    created_by UUID REFERENCES users(id),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 시스템 설정 테이블
CREATE TABLE system_settings (
    key VARCHAR(100) PRIMARY KEY,
    value JSONB NOT NULL,
    description TEXT,
    category VARCHAR(50) DEFAULT 'general',
    updated_by UUID REFERENCES users(id),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- ===============================================================================
-- 3. 드라이버 및 프로토콜 관리
-- ===============================================================================

-- 드라이버 플러그인 테이블
CREATE TABLE driver_plugins (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name VARCHAR(100) NOT NULL,
    protocol_type VARCHAR(50) NOT NULL, -- modbus_tcp, modbus_rtu, bacnet, mqtt, opcua
    version VARCHAR(20) NOT NULL,
    file_path VARCHAR(255) NOT NULL,
    description TEXT,
    author VARCHAR(100),
    api_version INTEGER DEFAULT 1,
    is_enabled BOOLEAN DEFAULT true,
    config_schema JSONB, -- JSON Schema for driver configuration
    installed_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    
    UNIQUE(protocol_type, version)
);

-- ===============================================================================
-- 4. 디바이스 및 연결 관리
-- ===============================================================================

-- 디바이스 그룹 테이블
CREATE TABLE device_groups (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name VARCHAR(100) NOT NULL,
    description TEXT,
    parent_group_id UUID REFERENCES device_groups(id) ON DELETE SET NULL,
    project_id UUID REFERENCES projects(id) ON DELETE CASCADE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 디바이스 테이블
CREATE TABLE devices (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name VARCHAR(100) NOT NULL,
    description TEXT,
    device_type VARCHAR(50), -- PLC, RTU, Sensor, Gateway
    protocol_type VARCHAR(50) NOT NULL, -- modbus_tcp, modbus_rtu, bacnet, mqtt
    endpoint VARCHAR(255) NOT NULL, -- IP:Port, Serial Port, MQTT Topic
    config JSONB NOT NULL, -- Protocol specific configuration
    polling_interval INTEGER DEFAULT 1000, -- milliseconds
    timeout INTEGER DEFAULT 3000, -- milliseconds
    retry_count INTEGER DEFAULT 3,
    is_enabled BOOLEAN DEFAULT true,
    device_group_id UUID REFERENCES device_groups(id) ON DELETE SET NULL,
    project_id UUID REFERENCES projects(id) ON DELETE CASCADE,
    created_by UUID REFERENCES users(id),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    
    -- 인덱스를 위한 제약조건
    CONSTRAINT chk_polling_interval CHECK (polling_interval > 0),
    CONSTRAINT chk_timeout CHECK (timeout > 0)
);

-- 디바이스 상태 테이블 (실시간 상태 - Redis와 동기화)
CREATE TABLE device_status (
    device_id UUID PRIMARY KEY REFERENCES devices(id) ON DELETE CASCADE,
    connection_status VARCHAR(20) NOT NULL DEFAULT 'disconnected', -- connected, disconnected, error
    last_communication TIMESTAMP WITH TIME ZONE,
    error_count INTEGER DEFAULT 0,
    last_error TEXT,
    response_time INTEGER, -- milliseconds
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- ===============================================================================
-- 5. 데이터 포인트 관리
-- ===============================================================================

-- 데이터 포인트 테이블
CREATE TABLE data_points (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    device_id UUID NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    address INTEGER NOT NULL, -- Modbus address, BACnet object instance, etc.
    data_type VARCHAR(20) NOT NULL, -- int16, int32, float, bool, string
    access_mode VARCHAR(10) DEFAULT 'read', -- read, write, readwrite
    unit VARCHAR(20), -- Unit of measurement (°C, %, kW, etc.)
    scaling_factor DECIMAL(10,4) DEFAULT 1.0,
    scaling_offset DECIMAL(10,4) DEFAULT 0.0,
    min_value DECIMAL(15,4),
    max_value DECIMAL(15,4),
    is_enabled BOOLEAN DEFAULT true,
    config JSONB, -- Point-specific configuration
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    
    UNIQUE(device_id, address)
);

-- 현재값 테이블 (Redis와 동기화)
CREATE TABLE current_values (
    point_id UUID PRIMARY KEY REFERENCES data_points(id) ON DELETE CASCADE,
    value DECIMAL(15,4),
    raw_value DECIMAL(15,4),
    quality VARCHAR(20) DEFAULT 'good', -- good, bad, uncertain
    timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- ===============================================================================
-- 6. 가상 포인트 및 수식 관리
-- ===============================================================================

-- 가상 포인트 테이블
CREATE TABLE virtual_points (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name VARCHAR(100) NOT NULL,
    description TEXT,
    formula TEXT NOT NULL, -- Mathematical expression
    data_type VARCHAR(20) NOT NULL DEFAULT 'float',
    unit VARCHAR(20),
    calculation_interval INTEGER DEFAULT 1000, -- milliseconds
    is_enabled BOOLEAN DEFAULT true,
    project_id UUID REFERENCES projects(id) ON DELETE CASCADE,
    created_by UUID REFERENCES users(id),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 가상 포인트 입력 매핑
CREATE TABLE virtual_point_inputs (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    virtual_point_id UUID NOT NULL REFERENCES virtual_points(id) ON DELETE CASCADE,
    variable_name VARCHAR(50) NOT NULL, -- Variable name in formula (A, B, temp1, etc.)
    source_type VARCHAR(20) NOT NULL, -- data_point, virtual_point, constant
    source_id UUID, -- References data_points.id or virtual_points.id
    constant_value DECIMAL(15,4), -- Used when source_type = 'constant'
    
    UNIQUE(virtual_point_id, variable_name)
);

-- 가상 포인트 현재값
CREATE TABLE virtual_point_values (
    virtual_point_id UUID PRIMARY KEY REFERENCES virtual_points(id) ON DELETE CASCADE,
    value DECIMAL(15,4),
    quality VARCHAR(20) DEFAULT 'good',
    last_calculated TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- ===============================================================================
-- 7. 알람 및 이벤트 관리
-- ===============================================================================

-- 알람 설정 테이블
CREATE TABLE alarm_configs (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name VARCHAR(100) NOT NULL,
    description TEXT,
    source_type VARCHAR(20) NOT NULL, -- data_point, virtual_point
    source_id UUID NOT NULL, -- References data_points.id or virtual_points.id
    alarm_type VARCHAR(20) NOT NULL, -- high, low, deviation, discrete
    enabled BOOLEAN DEFAULT true,
    priority VARCHAR(10) DEFAULT 'medium', -- low, medium, high, critical
    
    -- Analog alarm settings
    high_limit DECIMAL(15,4),
    low_limit DECIMAL(15,4),
    deadband DECIMAL(15,4) DEFAULT 0,
    
    -- Discrete alarm settings
    trigger_value DECIMAL(15,4),
    
    -- Message settings
    message_template TEXT NOT NULL,
    auto_acknowledge BOOLEAN DEFAULT false,
    
    project_id UUID REFERENCES projects(id) ON DELETE CASCADE,
    created_by UUID REFERENCES users(id),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 활성 알람 테이블
CREATE TABLE active_alarms (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    alarm_config_id UUID NOT NULL REFERENCES alarm_configs(id) ON DELETE CASCADE,
    triggered_value DECIMAL(15,4),
    message TEXT NOT NULL,
    triggered_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    acknowledged_at TIMESTAMP WITH TIME ZONE,
    acknowledged_by UUID REFERENCES users(id),
    
    -- 인덱스를 위한 필드
    is_active BOOLEAN DEFAULT true
);

-- 알람 히스토리 테이블
CREATE TABLE alarm_history (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    alarm_config_id UUID NOT NULL REFERENCES alarm_configs(id) ON DELETE CASCADE,
    event_type VARCHAR(20) NOT NULL, -- triggered, acknowledged, cleared
    triggered_value DECIMAL(15,4),
    message TEXT,
    event_time TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    user_id UUID REFERENCES users(id)
);

-- ===============================================================================
-- 8. 데이터 히스토리 (PostgreSQL용 - InfluxDB 보완)
-- ===============================================================================

-- 데이터 히스토리 테이블 (파티셔닝 고려)
CREATE TABLE data_history (
    id BIGSERIAL,
    point_id UUID NOT NULL REFERENCES data_points(id) ON DELETE CASCADE,
    value DECIMAL(15,4),
    raw_value DECIMAL(15,4),
    quality VARCHAR(20),
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
    
    PRIMARY KEY (id, timestamp)
) PARTITION BY RANGE (timestamp);

-- 월별 파티션 생성 예시 (자동화 스크립트로 관리)
CREATE TABLE data_history_2025_01 PARTITION OF data_history
    FOR VALUES FROM ('2025-01-01') TO ('2025-02-01');

CREATE TABLE data_history_2025_02 PARTITION OF data_history
    FOR VALUES FROM ('2025-02-01') TO ('2025-03-01');

-- 가상 포인트 히스토리
CREATE TABLE virtual_point_history (
    id BIGSERIAL,
    virtual_point_id UUID NOT NULL REFERENCES virtual_points(id) ON DELETE CASCADE,
    value DECIMAL(15,4),
    quality VARCHAR(20),
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
    
    PRIMARY KEY (id, timestamp)
) PARTITION BY RANGE (timestamp);

-- ===============================================================================
-- 9. 시스템 로그 및 감사
-- ===============================================================================

-- 시스템 로그 테이블
CREATE TABLE system_logs (
    id BIGSERIAL PRIMARY KEY,
    level VARCHAR(10) NOT NULL, -- debug, info, warn, error, fatal
    category VARCHAR(50) NOT NULL, -- collector, backend, driver, etc.
    message TEXT NOT NULL,
    details JSONB,
    source VARCHAR(100), -- Service/component name
    timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 사용자 활동 로그
CREATE TABLE user_activities (
    id BIGSERIAL PRIMARY KEY,
    user_id UUID REFERENCES users(id) ON DELETE SET NULL,
    action VARCHAR(50) NOT NULL, -- login, logout, create, update, delete
    resource_type VARCHAR(50), -- device, virtual_point, alarm, etc.
    resource_id UUID,
    details JSONB,
    ip_address INET,
    user_agent TEXT,
    timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 통신 로그 테이블 (패킷 레벨 로깅)
CREATE TABLE communication_logs (
    id BIGSERIAL PRIMARY KEY,
    device_id UUID REFERENCES devices(id) ON DELETE CASCADE,
    direction VARCHAR(10) NOT NULL, -- request, response
    protocol VARCHAR(20) NOT NULL,
    raw_data BYTEA,
    decoded_data JSONB,
    success BOOLEAN,
    error_message TEXT,
    response_time INTEGER, -- milliseconds
    timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- ===============================================================================
-- 10. 인덱스 생성
-- ===============================================================================

-- 성능 최적화를 위한 인덱스들
CREATE INDEX idx_devices_protocol_enabled ON devices(protocol_type, is_enabled);
CREATE INDEX idx_devices_project ON devices(project_id);
CREATE INDEX idx_data_points_device ON data_points(device_id);
CREATE INDEX idx_data_points_enabled ON data_points(is_enabled);

-- 현재값 조회 최적화
CREATE INDEX idx_current_values_timestamp ON current_values(timestamp DESC);

-- 가상 포인트 관련
CREATE INDEX idx_virtual_points_enabled ON virtual_points(is_enabled);
CREATE INDEX idx_virtual_point_inputs_source ON virtual_point_inputs(source_type, source_id);

-- 알람 관련
CREATE INDEX idx_active_alarms_config ON active_alarms(alarm_config_id);
CREATE INDEX idx_active_alarms_active ON active_alarms(is_active) WHERE is_active = true;
CREATE INDEX idx_alarm_history_config_time ON alarm_history(alarm_config_id, event_time DESC);

-- 히스토리 데이터 조회 최적화
CREATE INDEX idx_data_history_point_time ON data_history(point_id, timestamp DESC);
CREATE INDEX idx_virtual_point_history_point_time ON virtual_point_history(virtual_point_id, timestamp DESC);

-- 로그 테이블 인덱스
CREATE INDEX idx_system_logs_level_time ON system_logs(level, timestamp DESC);
CREATE INDEX idx_system_logs_category_time ON system_logs(category, timestamp DESC);
CREATE INDEX idx_communication_logs_device_time ON communication_logs(device_id, timestamp DESC);

-- 사용자 관련
CREATE INDEX idx_user_sessions_user_expires ON user_sessions(user_id, expires_at);
CREATE INDEX idx_user_activities_user_time ON user_activities(user_id, timestamp DESC);

-- ===============================================================================
-- 11. 트리거 함수들
-- ===============================================================================

-- 업데이트 시간 자동 갱신 함수
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ language 'plpgsql';

-- 자동 업데이트 트리거 생성
CREATE TRIGGER update_users_updated_at BEFORE UPDATE ON users
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_projects_updated_at BEFORE UPDATE ON projects
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_devices_updated_at BEFORE UPDATE ON devices
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_data_points_updated_at BEFORE UPDATE ON data_points
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_virtual_points_updated_at BEFORE UPDATE ON virtual_points
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_alarm_configs_updated_at BEFORE UPDATE ON alarm_configs
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- ===============================================================================
-- 12. 초기 데이터 삽입
-- ===============================================================================

-- 기본 관리자 사용자 생성 (비밀번호: admin123!)
INSERT INTO users (username, email, password_hash, full_name, role) VALUES
('admin', 'admin@pulseone.com', '$2b$10$rOzJKe.TvXj8K0Xv4mK4XOE8K4K4K4K4K4K4K4K4K4K4K4K4K4K4K4', 'System Administrator', 'admin');

-- 기본 프로젝트 생성
INSERT INTO projects (name, description, config) VALUES
('Default Project', 'Default PulseOne project', '{"version": "1.0", "timezone": "UTC"}');

-- 기본 시스템 설정
INSERT INTO system_settings (key, value, description, category) VALUES
('data_retention_days', '365', 'Data retention period in days', 'storage'),
('max_alarm_count', '1000', 'Maximum number of active alarms', 'alarm'),
('default_polling_interval', '1000', 'Default polling interval in milliseconds', 'collection'),
('log_level', '"info"', 'System log level', 'system'),
('enable_packet_logging', 'false', 'Enable communication packet logging', 'logging');

-- 드라이버 플러그인 등록 (예시)
INSERT INTO driver_plugins (name, protocol_type, version, file_path, description, author, config_schema) VALUES
(
    'Modbus TCP Driver',
    'modbus_tcp',
    '1.0.0',
    './plugins/libmodbus_tcp_driver.so',
    'Modbus TCP client driver using libmodbus',
    'PulseOne Team',
    '{
        "type": "object",
        "properties": {
            "slave_id": {"type": "integer", "minimum": 1, "maximum": 247},
            "byte_order": {"type": "string", "enum": ["big_endian", "little_endian"]},
            "word_order": {"type": "string", "enum": ["big_endian", "little_endian"]}
        },
        "required": ["slave_id"]
    }'::jsonb
);

-- ===============================================================================
-- 13. 뷰 생성 (자주 사용되는 조회 최적화)
-- ===============================================================================

-- 디바이스 상태 종합 뷰
CREATE VIEW device_status_summary AS
SELECT 
    d.id,
    d.name,
    d.protocol_type,
    d.endpoint,
    d.is_enabled,
    ds.connection_status,
    ds.last_communication,
    ds.error_count,
    ds.response_time,
    COUNT(dp.id) as point_count,
    COUNT(CASE WHEN dp.is_enabled THEN 1 END) as enabled_point_count
FROM devices d
LEFT JOIN device_status ds ON d.id = ds.device_id
LEFT JOIN data_points dp ON d.id = dp.device_id
GROUP BY d.id, d.name, d.protocol_type, d.endpoint, d.is_enabled,
         ds.connection_status, ds.last_communication, ds.error_count, ds.response_time;

-- 활성 알람 요약 뷰
CREATE VIEW active_alarm_summary AS
SELECT 
    aa.id,
    ac.name as alarm_name,
    ac.priority,
    aa.message,
    aa.triggered_value,
    aa.triggered_at,
    aa.acknowledged_at IS NOT NULL as is_acknowledged,
    CASE 
        WHEN ac.source_type = 'data_point' THEN dp.name
        WHEN ac.source_type = 'virtual_point' THEN vp.name
    END as source_name,
    CASE 
        WHEN ac.source_type = 'data_point' THEN d.name
        ELSE 'Virtual Point'
    END as device_name
FROM active_alarms aa
JOIN alarm_configs ac ON aa.alarm_config_id = ac.id
LEFT JOIN data_points dp ON ac.source_type = 'data_point' AND ac.source_id = dp.id
LEFT JOIN devices d ON dp.device_id = d.id
LEFT JOIN virtual_points vp ON ac.source_type = 'virtual_point' AND ac.source_id = vp.id
WHERE aa.is_active = true
ORDER BY ac.priority DESC, aa.triggered_at DESC;

-- ===============================================================================
-- 14. 데이터베이스 함수들
-- ===============================================================================

-- 디바이스 통계 함수
CREATE OR REPLACE FUNCTION get_device_statistics(device_uuid UUID)
RETURNS TABLE(
    total_points INTEGER,
    enabled_points INTEGER,
    last_update TIMESTAMP WITH TIME ZONE,
    avg_response_time DECIMAL
) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        COUNT(dp.id)::INTEGER as total_points,
        COUNT(CASE WHEN dp.is_enabled THEN 1 END)::INTEGER as enabled_points,
        MAX(cv.timestamp) as last_update,
        AVG(cl.response_time)::DECIMAL as avg_response_time
    FROM data_points dp
    LEFT JOIN current_values cv ON dp.id = cv.point_id
    LEFT JOIN communication_logs cl ON dp.device_id = cl.device_id 
        AND cl.timestamp > CURRENT_TIMESTAMP - INTERVAL '1 hour'
    WHERE dp.device_id = device_uuid;
END;
$$ LANGUAGE plpgsql;

-- 알람 통계 함수
CREATE OR REPLACE FUNCTION get_alarm_statistics()
RETURNS TABLE(
    total_active INTEGER,
    unacknowledged INTEGER,
    critical_count INTEGER,
    high_count INTEGER
) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        COUNT(*)::INTEGER as total_active,
        COUNT(CASE WHEN acknowledged_at IS NULL THEN 1 END)::INTEGER as unacknowledged,
        COUNT(CASE WHEN ac.priority = 'critical' THEN 1 END)::INTEGER as critical_count,
        COUNT(CASE WHEN ac.priority = 'high' THEN 1 END)::INTEGER as high_count
    FROM active_alarms aa
    JOIN alarm_configs ac ON aa.alarm_config_id = ac.id
    WHERE aa.is_active = true;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- PulseOne Multi-Tenant Database Schema
-- PostgreSQL 기반 멀티테넌트 스키마
-- ===============================================================================

-- 확장 기능 활성화
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pg_trgm";
CREATE EXTENSION IF NOT EXISTS "pg_stat_statements";

-- ===============================================================================
-- 시스템 레벨 테이블 (public 스키마)
-- 모든 테넌트가 공유하는 시스템 설정 및 관리 테이블
-- ===============================================================================

-- 시스템 관리자 테이블
CREATE TABLE system_admins (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(100),
    is_active BOOLEAN DEFAULT true,
    last_login TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 테넌트(회사) 테이블
CREATE TABLE tenants (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    company_name VARCHAR(100) NOT NULL UNIQUE,
    company_code VARCHAR(20) NOT NULL UNIQUE, -- 'samsung', 'lg', 'hyundai'
    domain VARCHAR(100) UNIQUE, -- 'samsung.pulseone.com'
    
    -- 연락처 정보
    contact_name VARCHAR(100),
    contact_email VARCHAR(100),
    contact_phone VARCHAR(20),
    
    -- 주소 정보
    address TEXT,
    country VARCHAR(50),
    timezone VARCHAR(50) DEFAULT 'UTC',
    
    -- 구독 정보
    subscription_plan VARCHAR(20) DEFAULT 'starter', -- starter, professional, enterprise
    subscription_status VARCHAR(20) DEFAULT 'active', -- active, suspended, cancelled
    max_edge_servers INTEGER DEFAULT 3,
    max_data_points INTEGER DEFAULT 1000,
    max_users INTEGER DEFAULT 5,
    
    -- 과금 정보
    billing_email VARCHAR(100),
    billing_cycle VARCHAR(10) DEFAULT 'monthly', -- monthly, yearly
    
    -- 상태 정보
    is_active BOOLEAN DEFAULT true,
    trial_end_date TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    
    -- 제약 조건
    CONSTRAINT chk_subscription_plan CHECK (subscription_plan IN ('starter', 'professional', 'enterprise')),
    CONSTRAINT chk_subscription_status CHECK (subscription_status IN ('active', 'trial', 'suspended', 'cancelled'))
);

-- Edge 서버 등록 테이블
CREATE TABLE edge_servers (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    
    -- 서버 식별 정보
    server_name VARCHAR(100) NOT NULL,
    factory_name VARCHAR(100),
    location VARCHAR(200),
    
    -- 네트워크 정보
    ip_address INET,
    mac_address VARCHAR(17),
    hostname VARCHAR(100),
    
    -- 등록 정보
    registration_token VARCHAR(255) UNIQUE,
    activation_code VARCHAR(50),
    
    -- 상태 정보
    status VARCHAR(20) DEFAULT 'pending', -- pending, active, inactive, error
    last_seen TIMESTAMP WITH TIME ZONE,
    version VARCHAR(20),
    
    -- 설정 정보
    config JSONB,
    sync_enabled BOOLEAN DEFAULT true,
    
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    
    UNIQUE(tenant_id, server_name),
    CONSTRAINT chk_status CHECK (status IN ('pending', 'active', 'inactive', 'error'))
);

-- 사용량 추적 테이블
CREATE TABLE usage_metrics (
    id BIGSERIAL PRIMARY KEY,
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    edge_server_id UUID REFERENCES edge_servers(id) ON DELETE CASCADE,
    
    -- 측정 정보
    metric_type VARCHAR(50) NOT NULL, -- data_points, api_requests, storage_mb
    metric_value BIGINT NOT NULL,
    measurement_date DATE NOT NULL,
    measurement_hour INTEGER, -- 0-23 for hourly tracking
    
    -- 메타데이터
    details JSONB,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    
    UNIQUE(tenant_id, edge_server_id, metric_type, measurement_date, measurement_hour)
);

-- 과금 내역 테이블
CREATE TABLE billing_records (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    
    -- 과금 기간
    billing_period_start DATE NOT NULL,
    billing_period_end DATE NOT NULL,
    
    -- 과금 항목들
    base_subscription_fee DECIMAL(10,2) DEFAULT 0,
    edge_server_fee DECIMAL(10,2) DEFAULT 0,
    data_points_fee DECIMAL(10,2) DEFAULT 0,
    api_requests_fee DECIMAL(10,2) DEFAULT 0,
    storage_fee DECIMAL(10,2) DEFAULT 0,
    
    -- 총액
    subtotal DECIMAL(10,2) NOT NULL,
    tax_rate DECIMAL(5,4) DEFAULT 0,
    tax_amount DECIMAL(10,2) DEFAULT 0,
    total_amount DECIMAL(10,2) NOT NULL,
    
    -- 상태
    status VARCHAR(20) DEFAULT 'pending', -- pending, paid, overdue, cancelled
    invoice_number VARCHAR(50) UNIQUE,
    payment_date TIMESTAMP WITH TIME ZONE,
    
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    
    CONSTRAINT chk_billing_status CHECK (status IN ('pending', 'paid', 'overdue', 'cancelled'))
);

-- 시스템 설정 테이블
CREATE TABLE system_settings (
    key VARCHAR(100) PRIMARY KEY,
    value JSONB NOT NULL,
    description TEXT,
    category VARCHAR(50) DEFAULT 'general',
    is_public BOOLEAN DEFAULT false, -- 테넌트에서 볼 수 있는지 여부
    updated_by UUID REFERENCES system_admins(id),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 스키마 마이그레이션 추적 (전역)
CREATE TABLE schema_migrations (
    id SERIAL PRIMARY KEY,
    version INTEGER UNIQUE NOT NULL,
    name VARCHAR(255) NOT NULL,
    executed_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- ===============================================================================
-- 테넌트별 스키마 생성 함수
-- ===============================================================================

-- 테넌트 스키마 생성 함수
CREATE OR REPLACE FUNCTION create_tenant_schema(tenant_code TEXT)
RETURNS VOID AS $$
DECLARE
    schema_name TEXT := 'tenant_' || tenant_code;
BEGIN
    -- 스키마 생성
    EXECUTE format('CREATE SCHEMA IF NOT EXISTS %I', schema_name);
    
    -- 테넌트별 테이블들 생성
    PERFORM create_tenant_tables(schema_name);
    
    -- 인덱스 생성
    PERFORM create_tenant_indexes(schema_name);
    
    -- 트리거 생성
    PERFORM create_tenant_triggers(schema_name);
    
    RAISE NOTICE 'Tenant schema % created successfully', schema_name;
END;
$$ LANGUAGE plpgsql;

-- 테넌트 테이블 생성 함수
CREATE OR REPLACE FUNCTION create_tenant_tables(schema_name TEXT)
RETURNS VOID AS $$
BEGIN
    -- 사용자 테이블
    EXECUTE format('
        CREATE TABLE %I.users (
            id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
            username VARCHAR(50) UNIQUE NOT NULL,
            email VARCHAR(100) UNIQUE NOT NULL,
            password_hash VARCHAR(255) NOT NULL,
            full_name VARCHAR(100),
            role VARCHAR(20) NOT NULL DEFAULT ''engineer'',
            department VARCHAR(100),
            factory_access TEXT[], -- 접근 가능한 공장 목록
            
            -- 권한 설정
            permissions TEXT[] DEFAULT ARRAY[''view_dashboard''],
            
            -- 상태 정보
            is_active BOOLEAN DEFAULT true,
            last_login TIMESTAMP WITH TIME ZONE,
            password_reset_token VARCHAR(255),
            password_reset_expires TIMESTAMP WITH TIME ZONE,
            
            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            
            CONSTRAINT chk_role CHECK (role IN (''company_admin'', ''factory_admin'', ''engineer'', ''operator'', ''viewer''))
        );
    ', schema_name);

    -- 사용자 세션 테이블
    EXECUTE format('
        CREATE TABLE %I.user_sessions (
            id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
            user_id UUID NOT NULL REFERENCES %I.users(id) ON DELETE CASCADE,
            token_hash VARCHAR(255) NOT NULL,
            ip_address INET,
            user_agent TEXT,
            expires_at TIMESTAMP WITH TIME ZONE NOT NULL,
            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
        );
    ', schema_name, schema_name);

    -- 공장 테이블
    EXECUTE format('
        CREATE TABLE %I.factories (
            id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
            name VARCHAR(100) NOT NULL,
            code VARCHAR(20) NOT NULL, -- factory code within company
            description TEXT,
            location VARCHAR(200),
            timezone VARCHAR(50) DEFAULT ''UTC'',
            
            -- 연락처 정보
            manager_name VARCHAR(100),
            manager_email VARCHAR(100),
            manager_phone VARCHAR(20),
            
            -- Edge 서버 정보
            edge_server_id UUID, -- references public.edge_servers(id)
            
            is_active BOOLEAN DEFAULT true,
            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            
            UNIQUE(code)
        );
    ', schema_name);

    -- 디바이스 그룹 테이블
    EXECUTE format('
        CREATE TABLE %I.device_groups (
            id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
            factory_id UUID NOT NULL REFERENCES %I.factories(id) ON DELETE CASCADE,
            name VARCHAR(100) NOT NULL,
            description TEXT,
            parent_group_id UUID REFERENCES %I.device_groups(id) ON DELETE SET NULL,
            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
        );
    ', schema_name, schema_name, schema_name);

    -- 드라이버 플러그인 테이블 (테넌트별)
    EXECUTE format('
        CREATE TABLE %I.driver_plugins (
            id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
            name VARCHAR(100) NOT NULL,
            protocol_type VARCHAR(50) NOT NULL,
            version VARCHAR(20) NOT NULL,
            file_path VARCHAR(255) NOT NULL,
            description TEXT,
            
            -- 플러그인 정보
            author VARCHAR(100),
            api_version INTEGER DEFAULT 1,
            is_enabled BOOLEAN DEFAULT true,
            config_schema JSONB,
            
            -- 라이선스 정보
            license_type VARCHAR(20) DEFAULT ''free'', -- free, commercial
            license_expires TIMESTAMP WITH TIME ZONE,
            
            installed_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            
            UNIQUE(protocol_type, version)
        );
    ', schema_name);

    -- 디바이스 테이블
    EXECUTE format('
        CREATE TABLE %I.devices (
            id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
            factory_id UUID NOT NULL REFERENCES %I.factories(id) ON DELETE CASCADE,
            device_group_id UUID REFERENCES %I.device_groups(id) ON DELETE SET NULL,
            
            -- 디바이스 기본 정보
            name VARCHAR(100) NOT NULL,
            description TEXT,
            device_type VARCHAR(50), -- PLC, RTU, Sensor, Gateway, HMI
            manufacturer VARCHAR(100),
            model VARCHAR(100),
            serial_number VARCHAR(100),
            
            -- 통신 설정
            protocol_type VARCHAR(50) NOT NULL,
            endpoint VARCHAR(255) NOT NULL,
            config JSONB NOT NULL,
            
            -- 수집 설정
            polling_interval INTEGER DEFAULT 1000,
            timeout INTEGER DEFAULT 3000,
            retry_count INTEGER DEFAULT 3,
            
            -- 상태 정보
            is_enabled BOOLEAN DEFAULT true,
            installation_date DATE,
            last_maintenance DATE,
            
            created_by UUID REFERENCES %I.users(id),
            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            
            CONSTRAINT chk_polling_interval CHECK (polling_interval > 0),
            CONSTRAINT chk_timeout CHECK (timeout > 0)
        );
    ', schema_name, schema_name, schema_name, schema_name);

    -- 디바이스 상태 테이블
    EXECUTE format('
        CREATE TABLE %I.device_status (
            device_id UUID PRIMARY KEY REFERENCES %I.devices(id) ON DELETE CASCADE,
            connection_status VARCHAR(20) NOT NULL DEFAULT ''disconnected'',
            last_communication TIMESTAMP WITH TIME ZONE,
            error_count INTEGER DEFAULT 0,
            last_error TEXT,
            response_time INTEGER,
            
            -- 추가 진단 정보
            firmware_version VARCHAR(50),
            hardware_info JSONB,
            diagnostic_data JSONB,
            
            updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            
            CONSTRAINT chk_connection_status CHECK (connection_status IN (''connected'', ''disconnected'', ''error'', ''maintenance''))
        );
    ', schema_name, schema_name);

    -- 데이터 포인트 테이블
    EXECUTE format('
        CREATE TABLE %I.data_points (
            id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
            device_id UUID NOT NULL REFERENCES %I.devices(id) ON DELETE CASCADE,
            
            -- 포인트 기본 정보
            name VARCHAR(100) NOT NULL,
            description TEXT,
            address INTEGER NOT NULL,
            data_type VARCHAR(20) NOT NULL,
            access_mode VARCHAR(10) DEFAULT ''read'',
            
            -- 엔지니어링 정보
            unit VARCHAR(20),
            scaling_factor DECIMAL(10,4) DEFAULT 1.0,
            scaling_offset DECIMAL(10,4) DEFAULT 0.0,
            min_value DECIMAL(15,4),
            max_value DECIMAL(15,4),
            
            -- 수집 설정
            is_enabled BOOLEAN DEFAULT true,
            scan_rate INTEGER, -- override device polling interval
            deadband DECIMAL(10,4) DEFAULT 0,
            
            -- 메타데이터
            config JSONB,
            tags TEXT[],
            
            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            
            UNIQUE(device_id, address),
            CONSTRAINT chk_data_type CHECK (data_type IN (''bool'', ''int16'', ''int32'', ''uint16'', ''uint32'', ''float'', ''double'', ''string'')),
            CONSTRAINT chk_access_mode CHECK (access_mode IN (''read'', ''write'', ''readwrite''))
        );
    ', schema_name, schema_name);

    -- 현재값 테이블
    EXECUTE format('
        CREATE TABLE %I.current_values (
            point_id UUID PRIMARY KEY REFERENCES %I.data_points(id) ON DELETE CASCADE,
            value DECIMAL(15,4),
            raw_value DECIMAL(15,4),
            string_value TEXT,
            quality VARCHAR(20) DEFAULT ''good'',
            timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            
            CONSTRAINT chk_quality CHECK (quality IN (''good'', ''bad'', ''uncertain'', ''not_connected''))
        );
    ', schema_name, schema_name);

    -- 가상 포인트 테이블
    EXECUTE format('
        CREATE TABLE %I.virtual_points (
            id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
            factory_id UUID REFERENCES %I.factories(id) ON DELETE CASCADE,
            
            -- 가상 포인트 정보
            name VARCHAR(100) NOT NULL,
            description TEXT,
            formula TEXT NOT NULL,
            data_type VARCHAR(20) NOT NULL DEFAULT ''float'',
            unit VARCHAR(20),
            
            -- 계산 설정
            calculation_interval INTEGER DEFAULT 1000,
            is_enabled BOOLEAN DEFAULT true,
            
            -- 메타데이터
            category VARCHAR(50),
            tags TEXT[],
            
            created_by UUID REFERENCES %I.users(id),
            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
        );
    ', schema_name, schema_name, schema_name);

    -- 가상 포인트 입력 매핑
    EXECUTE format('
        CREATE TABLE %I.virtual_point_inputs (
            id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
            virtual_point_id UUID NOT NULL REFERENCES %I.virtual_points(id) ON DELETE CASCADE,
            variable_name VARCHAR(50) NOT NULL,
            source_type VARCHAR(20) NOT NULL,
            source_id UUID,
            constant_value DECIMAL(15,4),
            
            UNIQUE(virtual_point_id, variable_name),
            CONSTRAINT chk_source_type CHECK (source_type IN (''data_point'', ''virtual_point'', ''constant''))
        );
    ', schema_name, schema_name);

    -- 가상 포인트 현재값
    EXECUTE format('
        CREATE TABLE %I.virtual_point_values (
            virtual_point_id UUID PRIMARY KEY REFERENCES %I.virtual_points(id) ON DELETE CASCADE,
            value DECIMAL(15,4),
            quality VARCHAR(20) DEFAULT ''good'',
            last_calculated TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            
            CONSTRAINT chk_vp_quality CHECK (quality IN (''good'', ''bad'', ''uncertain''))
        );
    ', schema_name, schema_name);

    -- 알람 설정 테이블
    EXECUTE format('
        CREATE TABLE %I.alarm_configs (
            id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
            factory_id UUID REFERENCES %I.factories(id) ON DELETE CASCADE,
            
            -- 알람 기본 정보
            name VARCHAR(100) NOT NULL,
            description TEXT,
            source_type VARCHAR(20) NOT NULL,
            source_id UUID NOT NULL,
            alarm_type VARCHAR(20) NOT NULL,
            
            -- 알람 설정
            enabled BOOLEAN DEFAULT true,
            priority VARCHAR(10) DEFAULT ''medium'',
            auto_acknowledge BOOLEAN DEFAULT false,
            
            -- 아날로그 알람 설정
            high_limit DECIMAL(15,4),
            low_limit DECIMAL(15,4),
            deadband DECIMAL(15,4) DEFAULT 0,
            
            -- 디지털 알람 설정
            trigger_value DECIMAL(15,4),
            
            -- 메시지 설정
            message_template TEXT NOT NULL,
            
            -- 알림 설정
            email_enabled BOOLEAN DEFAULT false,
            email_recipients TEXT[],
            sms_enabled BOOLEAN DEFAULT false,
            sms_recipients TEXT[],
            
            created_by UUID REFERENCES %I.users(id),
            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            
            CONSTRAINT chk_alarm_source_type CHECK (source_type IN (''data_point'', ''virtual_point'')),
            CONSTRAINT chk_alarm_type CHECK (alarm_type IN (''high'', ''low'', ''deviation'', ''discrete'')),
            CONSTRAINT chk_alarm_priority CHECK (priority IN (''low'', ''medium'', ''high'', ''critical''))
        );
    ', schema_name, schema_name, schema_name);

    -- 활성 알람 테이블
    EXECUTE format('
        CREATE TABLE %I.active_alarms (
            id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
            alarm_config_id UUID NOT NULL REFERENCES %I.alarm_configs(id) ON DELETE CASCADE,
            
            -- 알람 발생 정보
            triggered_value DECIMAL(15,4),
            message TEXT NOT NULL,
            triggered_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            
            -- 알람 응답 정보
            acknowledged_at TIMESTAMP WITH TIME ZONE,
            acknowledged_by UUID REFERENCES %I.users(id),
            acknowledgment_comment TEXT,
            
            -- 상태
            is_active BOOLEAN DEFAULT true
        );
    ', schema_name, schema_name, schema_name);

    -- 알람 히스토리 테이블
    EXECUTE format('
        CREATE TABLE %I.alarm_history (
            id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
            alarm_config_id UUID NOT NULL REFERENCES %I.alarm_configs(id) ON DELETE CASCADE,
            
            -- 이벤트 정보
            event_type VARCHAR(20) NOT NULL,
            triggered_value DECIMAL(15,4),
            message TEXT,
            event_time TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            user_id UUID REFERENCES %I.users(id),
            
            CONSTRAINT chk_event_type CHECK (event_type IN (''triggered'', ''acknowledged'', ''cleared'', ''disabled''))
        );
    ', schema_name, schema_name, schema_name);

    -- 시스템 로그 테이블
    EXECUTE format('
        CREATE TABLE %I.system_logs (
            id BIGSERIAL PRIMARY KEY,
            level VARCHAR(10) NOT NULL,
            category VARCHAR(50) NOT NULL,
            message TEXT NOT NULL,
            details JSONB,
            source VARCHAR(100),
            user_id UUID REFERENCES %I.users(id),
            timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            
            CONSTRAINT chk_log_level CHECK (level IN (''debug'', ''info'', ''warn'', ''error'', ''fatal''))
        );
    ', schema_name, schema_name);

    -- 사용자 활동 로그
    EXECUTE format('
        CREATE TABLE %I.user_activities (
            id BIGSERIAL PRIMARY KEY,
            user_id UUID REFERENCES %I.users(id) ON DELETE SET NULL,
            action VARCHAR(50) NOT NULL,
            resource_type VARCHAR(50),
            resource_id UUID,
            details JSONB,
            ip_address INET,
            user_agent TEXT,
            timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
        );
    ', schema_name, schema_name);

    -- 통신 로그 테이블
    EXECUTE format('
        CREATE TABLE %I.communication_logs (
            id BIGSERIAL PRIMARY KEY,
            device_id UUID REFERENCES %I.devices(id) ON DELETE CASCADE,
            direction VARCHAR(10) NOT NULL,
            protocol VARCHAR(20) NOT NULL,
            raw_data BYTEA,
            decoded_data JSONB,
            success BOOLEAN,
            error_message TEXT,
            response_time INTEGER,
            timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            
            CONSTRAINT chk_direction CHECK (direction IN (''request'', ''response''))
        );
    ', schema_name, schema_name);

    RAISE NOTICE 'All tables created for schema %', schema_name;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- 인덱스 생성 함수
-- ===============================================================================

CREATE OR REPLACE FUNCTION create_tenant_indexes(schema_name TEXT)
RETURNS VOID AS $$
BEGIN
    -- 사용자 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_users_email ON %I.users(email)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_users_role ON %I.users(role)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 디바이스 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_devices_factory ON %I.devices(factory_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_devices_protocol ON %I.devices(protocol_type)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_devices_enabled ON %I.devices(is_enabled)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 데이터 포인트 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_data_points_device ON %I.data_points(device_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_data_points_enabled ON %I.data_points(is_enabled)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 현재값 조회 최적화
    EXECUTE format('CREATE INDEX idx_%s_current_values_timestamp ON %I.current_values(timestamp DESC)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 알람 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_active_alarms_config ON %I.active_alarms(alarm_config_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_active_alarms_active ON %I.active_alarms(is_active) WHERE is_active = true', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 로그 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_system_logs_timestamp ON %I.system_logs(timestamp DESC)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_communication_logs_device_time ON %I.communication_logs(device_id, timestamp DESC)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    RAISE NOTICE 'All indexes created for schema %', schema_name;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- 트리거 생성 함수
-- ===============================================================================

CREATE OR REPLACE FUNCTION create_tenant_triggers(schema_name TEXT)
RETURNS VOID AS $$
BEGIN
    -- 업데이트 시간 자동 갱신 트리거들
    EXECUTE format('
        CREATE TRIGGER update_%s_users_updated_at 
        BEFORE UPDATE ON %I.users
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_factories_updated_at 
        BEFORE UPDATE ON %I.factories
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_devices_updated_at 
        BEFORE UPDATE ON %I.devices
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_data_points_updated_at 
        BEFORE UPDATE ON %I.data_points
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_virtual_points_updated_at 
        BEFORE UPDATE ON %I.virtual_points
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_alarm_configs_updated_at 
        BEFORE UPDATE ON %I.alarm_configs
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    RAISE NOTICE 'All triggers created for schema %', schema_name;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- 공통 함수들
-- ===============================================================================

-- 업데이트 시간 자동 갱신 함수
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ language 'plpgsql';

-- 테넌트 삭제 함수
CREATE OR REPLACE FUNCTION drop_tenant_schema(tenant_code TEXT)
RETURNS VOID AS $$
DECLARE
    schema_name TEXT := 'tenant_' || tenant_code;
BEGIN
    EXECUTE format('DROP SCHEMA IF EXISTS %I CASCADE', schema_name);
    RAISE NOTICE 'Tenant schema % dropped', schema_name;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- 초기 데이터 삽입
-- ===============================================================================

-- 시스템 관리자 생성 (비밀번호: admin123!)
INSERT INTO system_admins (username, email, password_hash, full_name) VALUES
('admin', 'admin@pulseone.com', '$2b$10$rOzJKe.TvXj8K0Xv4mK4XOE8K4K4K4K4K4K4K4K4K4K4K4K4K4K4K4', 'System Administrator'),
('support', 'support@pulseone.com', '$2b$10$rOzJKe.TvXj8K0Xv4mK4XOE8K4K4K4K4K4K4K4K4K4K4K4K4K4K4K4', 'Support Administrator');

-- 기본 시스템 설정
INSERT INTO system_settings (key, value, description, category, is_public) VALUES
('data_retention_days', '365', 'Default data retention period in days', 'storage', true),
('max_edge_servers_starter', '3', 'Maximum edge servers for starter plan', 'billing', false),
('max_edge_servers_professional', '10', 'Maximum edge servers for professional plan', 'billing', false),
('max_edge_servers_enterprise', '-1', 'Maximum edge servers for enterprise plan (-1 = unlimited)', 'billing', false),
('max_data_points_starter', '1000', 'Maximum data points for starter plan', 'billing', false),
('max_data_points_professional', '10000', 'Maximum data points for professional plan', 'billing', false),
('max_data_points_enterprise', '-1', 'Maximum data points for enterprise plan (-1 = unlimited)', 'billing', false),
('default_polling_interval', '1000', 'Default polling interval in milliseconds', 'collection', true),
('system_version', '"1.0.0"', 'Current system version', 'system', true),
('maintenance_mode', 'false', 'System maintenance mode', 'system', false),
('registration_enabled', 'true', 'Allow new tenant registration', 'system', false),
('log_level', '"info"', 'System log level', 'system', false),
('enable_packet_logging', 'false', 'Enable communication packet logging', 'logging', true),
('backup_enabled', 'true', 'Enable automatic backups', 'backup', false),
('backup_retention_days', '30', 'Backup retention period in days', 'backup', false),
('email_notifications_enabled', 'true', 'Enable email notifications', 'notifications', true),
('sms_notifications_enabled', 'false', 'Enable SMS notifications', 'notifications', true);

-- 샘플 테넌트 생성 (개발/테스트용)
INSERT INTO tenants (
    company_name, company_code, domain, 
    contact_name, contact_email, 
    subscription_plan, max_edge_servers, max_data_points, max_users
) VALUES 
('Demo Company', 'demo', 'demo.pulseone.com', 
 'Demo Manager', 'demo@example.com', 
 'professional', 5, 5000, 10),
('Test Corp', 'test', 'test.pulseone.com', 
 'Test Manager', 'test@example.com', 
 'starter', 3, 1000, 5);

-- ===============================================================================
-- 뷰 생성 (시스템 레벨)
-- ===============================================================================

-- 테넌트 요약 뷰
CREATE VIEW tenant_summary AS
SELECT 
    t.id,
    t.company_name,
    t.company_code,
    t.subscription_plan,
    t.subscription_status,
    t.max_edge_servers,
    t.max_data_points,
    t.max_users,
    COUNT(es.id) as active_edge_servers,
    t.created_at,
    CASE 
        WHEN t.trial_end_date IS NOT NULL AND t.trial_end_date < CURRENT_TIMESTAMP 
        THEN true 
        ELSE false 
    END as trial_expired
FROM tenants t
LEFT JOIN edge_servers es ON t.id = es.tenant_id AND es.status = 'active'
GROUP BY t.id, t.company_name, t.company_code, t.subscription_plan, 
         t.subscription_status, t.max_edge_servers, t.max_data_points, 
         t.max_users, t.created_at, t.trial_end_date;

-- Edge 서버 상태 요약 뷰
CREATE VIEW edge_server_summary AS
SELECT 
    es.id,
    es.server_name,
    es.factory_name,
    es.status,
    es.last_seen,
    es.version,
    t.company_name,
    t.company_code,
    CASE 
        WHEN es.last_seen IS NULL THEN 'Never Connected'
        WHEN es.last_seen < CURRENT_TIMESTAMP - INTERVAL '5 minutes' THEN 'Offline'
        ELSE 'Online'
    END as connectivity_status,
    EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - es.last_seen))/60 as minutes_since_last_seen
FROM edge_servers es
JOIN tenants t ON es.tenant_id = t.id
WHERE es.status = 'active';

-- 사용량 요약 뷰 (월별)
CREATE VIEW monthly_usage_summary AS
SELECT 
    um.tenant_id,
    t.company_name,
    DATE_TRUNC('month', um.measurement_date) as month,
    um.metric_type,
    SUM(um.metric_value) as total_usage,
    AVG(um.metric_value) as avg_daily_usage,
    MAX(um.metric_value) as peak_daily_usage
FROM usage_metrics um
JOIN tenants t ON um.tenant_id = t.id
WHERE um.measurement_date >= CURRENT_DATE - INTERVAL '12 months'
GROUP BY um.tenant_id, t.company_name, DATE_TRUNC('month', um.measurement_date), um.metric_type
ORDER BY month DESC, t.company_name, um.metric_type;

-- ===============================================================================
-- 시스템 관리 함수들
-- ===============================================================================

-- 새 테넌트 등록 함수
CREATE OR REPLACE FUNCTION register_new_tenant(
    p_company_name TEXT,
    p_company_code TEXT,
    p_contact_email TEXT,
    p_contact_name TEXT DEFAULT NULL,
    p_subscription_plan TEXT DEFAULT 'starter'
)
RETURNS UUID AS $
DECLARE
    new_tenant_id UUID;
    schema_name TEXT;
BEGIN
    -- 회사 코드 중복 체크
    IF EXISTS (SELECT 1 FROM tenants WHERE company_code = p_company_code) THEN
        RAISE EXCEPTION 'Company code % already exists', p_company_code;
    END IF;
    
    -- 이메일 중복 체크
    IF EXISTS (SELECT 1 FROM tenants WHERE contact_email = p_contact_email) THEN
        RAISE EXCEPTION 'Contact email % already exists', p_contact_email;
    END IF;
    
    -- 테넌트 생성
    INSERT INTO tenants (
        company_name, company_code, domain, 
        contact_name, contact_email, subscription_plan,
        trial_end_date
    ) VALUES (
        p_company_name, p_company_code, 
        p_company_code || '.pulseone.com',
        p_contact_name, p_contact_email, p_subscription_plan,
        CURRENT_TIMESTAMP + INTERVAL '30 days' -- 30일 무료 체험
    ) RETURNING id INTO new_tenant_id;
    
    -- 테넌트 스키마 생성
    PERFORM create_tenant_schema(p_company_code);
    
    -- 기본 관리자 사용자 생성
    schema_name := 'tenant_' || p_company_code;
    EXECUTE format('
        INSERT INTO %I.users (username, email, password_hash, full_name, role, permissions)
        VALUES ($1, $2, $3, $4, $5, $6)
    ', schema_name) 
    USING 
        'admin', 
        p_contact_email, 
        '$2b$10$rOzJKe.TvXj8K0Xv4mK4XOE8K4K4K4K4K4K4K4K4K4K4K4K4K4K4K4', -- 기본 비밀번호: admin123!
        COALESCE(p_contact_name, 'Administrator'),
        'company_admin',
        ARRAY['manage_company', 'manage_factories', 'manage_users', 'view_all_data'];
    
    -- 기본 공장 생성
    EXECUTE format('
        INSERT INTO %I.factories (name, code, description)
        VALUES ($1, $2, $3)
    ', schema_name)
    USING 
        'Main Factory',
        'main',
        'Default factory for ' || p_company_name;
    
    RAISE NOTICE 'New tenant % registered with ID %', p_company_name, new_tenant_id;
    RETURN new_tenant_id;
END;
$ LANGUAGE plpgsql;

-- 테넌트 삭제 함수 (데이터 포함)
CREATE OR REPLACE FUNCTION delete_tenant(p_tenant_id UUID)
RETURNS VOID AS $
DECLARE
    tenant_code TEXT;
    schema_name TEXT;
BEGIN
    -- 테넌트 정보 조회
    SELECT company_code INTO tenant_code 
    FROM tenants 
    WHERE id = p_tenant_id;
    
    IF tenant_code IS NULL THEN
        RAISE EXCEPTION 'Tenant not found: %', p_tenant_id;
    END IF;
    
    schema_name := 'tenant_' || tenant_code;
    
    -- 스키마 삭제
    PERFORM drop_tenant_schema(tenant_code);
    
    -- Edge 서버 정보 삭제
    DELETE FROM edge_servers WHERE tenant_id = p_tenant_id;
    
    -- 사용량 정보 삭제
    DELETE FROM usage_metrics WHERE tenant_id = p_tenant_id;
    
    -- 과금 정보 삭제 (주의: 법적 요구사항에 따라 보관 필요할 수 있음)
    -- DELETE FROM billing_records WHERE tenant_id = p_tenant_id;
    
    -- 테넌트 삭제
    DELETE FROM tenants WHERE id = p_tenant_id;
    
    RAISE NOTICE 'Tenant % deleted completely', tenant_code;
END;
$ LANGUAGE plpgsql;

-- Edge 서버 등록 함수
CREATE OR REPLACE FUNCTION register_edge_server(
    p_tenant_id UUID,
    p_server_name TEXT,
    p_factory_name TEXT DEFAULT NULL,
    p_location TEXT DEFAULT NULL
)
RETURNS TEXT AS $
DECLARE
    registration_token TEXT;
    activation_code TEXT;
BEGIN
    -- 등록 토큰 생성 (UUID 기반)
    registration_token := uuid_generate_v4()::TEXT;
    
    -- 활성화 코드 생성 (6자리 숫자)
    activation_code := LPAD((RANDOM() * 999999)::INTEGER::TEXT, 6, '0');
    
    -- Edge 서버 등록
    INSERT INTO edge_servers (
        tenant_id, server_name, factory_name, location,
        registration_token, activation_code, status
    ) VALUES (
        p_tenant_id, p_server_name, p_factory_name, p_location,
        registration_token, activation_code, 'pending'
    );
    
    RETURN registration_token;
END;
$ LANGUAGE plpgsql;

-- 사용량 기록 함수
CREATE OR REPLACE FUNCTION record_usage(
    p_tenant_id UUID,
    p_edge_server_id UUID DEFAULT NULL,
    p_metric_type TEXT,
    p_metric_value BIGINT,
    p_measurement_date DATE DEFAULT CURRENT_DATE,
    p_measurement_hour INTEGER DEFAULT NULL
)
RETURNS VOID AS $
BEGIN
    INSERT INTO usage_metrics (
        tenant_id, edge_server_id, metric_type, 
        metric_value, measurement_date, measurement_hour
    ) VALUES (
        p_tenant_id, p_edge_server_id, p_metric_type,
        p_metric_value, p_measurement_date, p_measurement_hour
    )
    ON CONFLICT (tenant_id, edge_server_id, metric_type, measurement_date, measurement_hour)
    DO UPDATE SET 
        metric_value = usage_metrics.metric_value + EXCLUDED.metric_value,
        created_at = CURRENT_TIMESTAMP;
END;
$ LANGUAGE plpgsql;

-- 테넌트별 통계 함수
CREATE OR REPLACE FUNCTION get_tenant_statistics(p_tenant_id UUID)
RETURNS TABLE(
    metric_name TEXT,
    metric_value BIGINT,
    last_updated TIMESTAMP WITH TIME ZONE
) AS $
BEGIN
    RETURN QUERY
    SELECT 
        'active_edge_servers'::TEXT,
        COUNT(*)::BIGINT,
        MAX(es.last_seen)
    FROM edge_servers es
    WHERE es.tenant_id = p_tenant_id AND es.status = 'active'
    
    UNION ALL
    
    SELECT 
        'total_data_points_today'::TEXT,
        COALESCE(SUM(um.metric_value), 0)::BIGINT,
        MAX(um.created_at)
    FROM usage_metrics um
    WHERE um.tenant_id = p_tenant_id 
      AND um.metric_type = 'data_points'
      AND um.measurement_date = CURRENT_DATE
    
    UNION ALL
    
    SELECT 
        'api_requests_today'::TEXT,
        COALESCE(SUM(um.metric_value), 0)::BIGINT,
        MAX(um.created_at)
    FROM usage_metrics um
    WHERE um.tenant_id = p_tenant_id 
      AND um.metric_type = 'api_requests'
      AND um.measurement_date = CURRENT_DATE;
END;
$ LANGUAGE plpgsql;

-- ===============================================================================
-- 과금 관련 함수들
-- ===============================================================================

-- 월별 과금 계산 함수
CREATE OR REPLACE FUNCTION calculate_monthly_bill(
    p_tenant_id UUID,
    p_billing_month DATE
)
RETURNS DECIMAL AS $
DECLARE
    tenant_plan TEXT;
    base_fee DECIMAL := 0;
    edge_server_count INTEGER;
    data_points_used BIGINT;
    api_requests_used BIGINT;
    total_fee DECIMAL := 0;
    
    -- 요금표
    starter_base_fee DECIMAL := 99.00;
    professional_base_fee DECIMAL := 299.00;
    enterprise_base_fee DECIMAL := 999.00;
    
    edge_server_fee DECIMAL := 50.00;
    data_points_overage_fee DECIMAL := 0.01; -- per 1000 data points
    api_requests_overage_fee DECIMAL := 0.001; -- per 1000 requests
BEGIN
    -- 테넌트 플랜 조회
    SELECT subscription_plan INTO tenant_plan
    FROM tenants 
    WHERE id = p_tenant_id;
    
    -- 기본 요금 설정
    CASE tenant_plan
        WHEN 'starter' THEN base_fee := starter_base_fee;
        WHEN 'professional' THEN base_fee := professional_base_fee;
        WHEN 'enterprise' THEN base_fee := enterprise_base_fee;
        ELSE base_fee := 0;
    END CASE;
    
    -- Edge 서버 수 계산
    SELECT COUNT(*) INTO edge_server_count
    FROM edge_servers
    WHERE tenant_id = p_tenant_id 
      AND status = 'active'
      AND created_at <= (p_billing_month + INTERVAL '1 month');
    
    -- 사용량 계산
    SELECT 
        COALESCE(SUM(CASE WHEN metric_type = 'data_points' THEN metric_value ELSE 0 END), 0),
        COALESCE(SUM(CASE WHEN metric_type = 'api_requests' THEN metric_value ELSE 0 END), 0)
    INTO data_points_used, api_requests_used
    FROM usage_metrics
    WHERE tenant_id = p_tenant_id
      AND measurement_date >= p_billing_month
      AND measurement_date < (p_billing_month + INTERVAL '1 month');
    
    -- 총 요금 계산
    total_fee := base_fee + (edge_server_count * edge_server_fee);
    
    -- 오버리지 요금 (starter, professional 플랜만)
    IF tenant_plan != 'enterprise' THEN
        DECLARE
            max_data_points INTEGER;
            max_api_requests INTEGER := 10000; -- 기본 API 요청 한도
        BEGIN
            -- 플랜별 데이터 포인트 한도
            IF tenant_plan = 'starter' THEN
                max_data_points := 100000; -- 월 10만 포인트
            ELSE
                max_data_points := 1000000; -- 월 100만 포인트
            END IF;
            
            -- 데이터 포인트 오버리지
            IF data_points_used > max_data_points THEN
                total_fee := total_fee + ((data_points_used - max_data_points) / 1000.0) * data_points_overage_fee;
            END IF;
            
            -- API 요청 오버리지
            IF api_requests_used > max_api_requests THEN
                total_fee := total_fee + ((api_requests_used - max_api_requests) / 1000.0) * api_requests_overage_fee;
            END IF;
        END;
    END IF;
    
    RETURN total_fee;
END;
$ LANGUAGE plpgsql;

-- 과금 레코드 생성 함수
CREATE OR REPLACE FUNCTION generate_monthly_bill(
    p_tenant_id UUID,
    p_billing_month DATE
)
RETURNS UUID AS $
DECLARE
    bill_id UUID;
    total_amount DECIMAL;
    invoice_number TEXT;
BEGIN
    -- 과금 계산
    total_amount := calculate_monthly_bill(p_tenant_id, p_billing_month);
    
    -- 인보이스 번호 생성
    invoice_number := 'INV-' || TO_CHAR(p_billing_month, 'YYYYMM') || '-' || 
                     UPPER(SUBSTRING(p_tenant_id::TEXT, 1, 8));
    
    -- 과금 레코드 생성
    INSERT INTO billing_records (
        tenant_id,
        billing_period_start,
        billing_period_end,
        subtotal,
        total_amount,
        invoice_number,
        status
    ) VALUES (
        p_tenant_id,
        p_billing_month,
        (p_billing_month + INTERVAL '1 month' - INTERVAL '1 day')::DATE,
        total_amount,
        total_amount,
        invoice_number,
        'pending'
    ) RETURNING id INTO bill_id;
    
    RETURN bill_id;
END;
$ LANGUAGE plpgsql;

-- ===============================================================================
-- 데이터 정리 및 유지보수 함수들
-- ===============================================================================

-- 오래된 로그 정리 함수
CREATE OR REPLACE FUNCTION cleanup_old_logs(retention_days INTEGER DEFAULT 90)
RETURNS INTEGER AS $
DECLARE
    deleted_count INTEGER := 0;
    tenant_rec RECORD;
    schema_name TEXT;
BEGIN
    -- 각 테넌트 스키마의 로그 정리
    FOR tenant_rec IN SELECT company_code FROM tenants WHERE is_active = true LOOP
        schema_name := 'tenant_' || tenant_rec.company_code;
        
        -- 시스템 로그 정리
        EXECUTE format('
            DELETE FROM %I.system_logs 
            WHERE timestamp < CURRENT_TIMESTAMP - INTERVAL ''%s days''
        ', schema_name, retention_days);
        
        GET DIAGNOSTICS deleted_count = ROW_COUNT;
        
        -- 통신 로그 정리 (더 짧은 보관 기간)
        EXECUTE format('
            DELETE FROM %I.communication_logs 
            WHERE timestamp < CURRENT_TIMESTAMP - INTERVAL ''%s days''
        ', schema_name, LEAST(retention_days, 30));
        
        -- 사용자 활동 로그 정리
        EXECUTE format('
            DELETE FROM %I.user_activities 
            WHERE timestamp < CURRENT_TIMESTAMP - INTERVAL ''%s days''
        ', schema_name, retention_days);
        
    END LOOP;
    
    RETURN deleted_count;
END;
$ LANGUAGE plpgsql;

-- 테넌트 통계 업데이트 함수
CREATE OR REPLACE FUNCTION update_tenant_statistics()
RETURNS VOID AS $
BEGIN
    -- 각 테넌트의 Edge 서버 상태 업데이트
    UPDATE edge_servers SET status = 'inactive'
    WHERE status = 'active' 
      AND last_seen < CURRENT_TIMESTAMP - INTERVAL '1 hour';
    
    -- 만료된 체험판 상태 업데이트
    UPDATE tenants SET subscription_status = 'trial_expired'
    WHERE subscription_status = 'trial'
      AND trial_end_date < CURRENT_TIMESTAMP;
      
    RAISE NOTICE 'Tenant statistics updated';
END;
$ LANGUAGE plpgsql;

-- ===============================================================================
-- 초기 마이그레이션 기록
-- ===============================================================================

-- 마이그레이션 버전 기록
INSERT INTO schema_migrations (version, name) VALUES
(1, '001_create_system_tables'),
(2, '002_create_tenant_functions'),
(3, '003_create_billing_system'),
(4, '004_create_management_functions');

-- ===============================================================================
-- 사용 예시 및 테스트
-- ===============================================================================

-- 샘플 테넌트의 스키마 생성
SELECT create_tenant_schema('demo');
SELECT create_tenant_schema('test');

-- Edge 서버 등록 예시
DO $
DECLARE
    demo_tenant_id UUID;
    registration_token TEXT;
BEGIN
    -- Demo 테넌트 ID 조회
    SELECT id INTO demo_tenant_id FROM tenants WHERE company_code = 'demo';
    
    -- Edge 서버 등록
    registration_token := register_edge_server(
        demo_tenant_id, 
        'Factory-A-Edge-01', 
        'Factory A', 
        'Seoul, South Korea'
    );
    
    RAISE NOTICE 'Edge server registered with token: %', registration_token;
END $;