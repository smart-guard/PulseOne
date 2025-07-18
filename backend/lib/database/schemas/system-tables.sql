-- backend/lib/database/schemas/system-tables.sql
-- 시스템 레벨 테이블 (public 스키마)
-- 모든 테넌트가 공유하는 시스템 설정 및 관리 테이블

-- ===============================================================================
-- PostgreSQL 확장 기능 활성화
-- ===============================================================================
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pg_trgm";
CREATE EXTENSION IF NOT EXISTS "pg_stat_statements";

-- ===============================================================================
-- 시스템 관리자 테이블
-- ===============================================================================
CREATE TABLE IF NOT EXISTS system_admins (
    id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(100),
    is_active {{BOOLEAN}} DEFAULT {{TRUE}},
    last_login {{TIMESTAMP}},
    created_at {{TIMESTAMP}},
    updated_at {{TIMESTAMP}}
);

-- ===============================================================================
-- 테넌트(회사) 테이블
-- ===============================================================================
CREATE TABLE IF NOT EXISTS tenants (
    id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
    company_name VARCHAR(100) NOT NULL UNIQUE,
    company_code VARCHAR(20) NOT NULL UNIQUE, -- 'samsung', 'lg', 'hyundai'
    domain VARCHAR(100) UNIQUE, -- 'samsung.pulseone.com'
    
    -- 연락처 정보
    contact_name VARCHAR(100),
    contact_email VARCHAR(100),
    contact_phone VARCHAR(20),
    
    -- 주소 정보
    address {{TEXT}},
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
    is_active {{BOOLEAN}} DEFAULT {{TRUE}},
    trial_end_date {{TIMESTAMP}},
    created_at {{TIMESTAMP}},
    updated_at {{TIMESTAMP}},
    
    -- 제약 조건
    CONSTRAINT chk_subscription_plan CHECK (subscription_plan IN ('starter', 'professional', 'enterprise')),
    CONSTRAINT chk_subscription_status CHECK (subscription_status IN ('active', 'trial', 'suspended', 'cancelled'))
);

-- ===============================================================================
-- Edge 서버 등록 테이블
-- ===============================================================================
CREATE TABLE IF NOT EXISTS edge_servers (
    id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id {{UUID}} NOT NULL,
    
    -- 서버 식별 정보
    server_name VARCHAR(100) NOT NULL,
    factory_name VARCHAR(100),
    location VARCHAR(200),
    
    -- 네트워크 정보
    ip_address VARCHAR(45), -- INET for PostgreSQL, VARCHAR for others
    mac_address VARCHAR(17),
    hostname VARCHAR(100),
    
    -- 등록 정보
    registration_token VARCHAR(255) UNIQUE,
    activation_code VARCHAR(50),
    
    -- 상태 정보
    status VARCHAR(20) DEFAULT 'pending', -- pending, active, inactive, error
    last_seen {{TIMESTAMP}},
    version VARCHAR(20),
    
    -- 설정 정보
    config {{TEXT}}, -- JSON
    sync_enabled {{BOOLEAN}} DEFAULT {{TRUE}},
    
    created_at {{TIMESTAMP}},
    updated_at {{TIMESTAMP}},
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    UNIQUE(tenant_id, server_name),
    CONSTRAINT chk_edge_status CHECK (status IN ('pending', 'active', 'inactive', 'error'))
);

-- ===============================================================================
-- 사용량 추적 테이블
-- ===============================================================================
CREATE TABLE IF NOT EXISTS usage_metrics (
    id {{AUTO_INCREMENT}},
    tenant_id {{UUID}} NOT NULL,
    edge_server_id {{UUID}},
    
    -- 측정 정보
    metric_type VARCHAR(50) NOT NULL, -- data_points, api_requests, storage_mb
    metric_value BIGINT NOT NULL,
    measurement_date DATE NOT NULL,
    measurement_hour INTEGER, -- 0-23 for hourly tracking
    
    -- 메타데이터
    details {{TEXT}}, -- JSON
    created_at {{TIMESTAMP}},
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE CASCADE,
    UNIQUE(tenant_id, edge_server_id, metric_type, measurement_date, measurement_hour)
);

-- ===============================================================================
-- 과금 내역 테이블
-- ===============================================================================
CREATE TABLE IF NOT EXISTS billing_records (
    id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id {{UUID}} NOT NULL,
    
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
    payment_date {{TIMESTAMP}},
    
    created_at {{TIMESTAMP}},
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    CONSTRAINT chk_billing_status CHECK (status IN ('pending', 'paid', 'overdue', 'cancelled'))
);

-- ===============================================================================
-- 시스템 설정 테이블
-- ===============================================================================
CREATE TABLE IF NOT EXISTS system_settings (
    key VARCHAR(100) PRIMARY KEY,
    value {{TEXT}} NOT NULL, -- JSON
    description {{TEXT}},
    category VARCHAR(50) DEFAULT 'general',
    is_public {{BOOLEAN}} DEFAULT {{FALSE}}, -- 테넌트에서 볼 수 있는지 여부
    updated_by {{UUID}},
    updated_at {{TIMESTAMP}},
    
    FOREIGN KEY (updated_by) REFERENCES system_admins(id) ON DELETE SET NULL
);

-- ===============================================================================
-- 스키마 마이그레이션 추적 (전역)
-- ===============================================================================
CREATE TABLE IF NOT EXISTS schema_migrations (
    id {{AUTO_INCREMENT}},
    version INTEGER UNIQUE NOT NULL,
    name VARCHAR(255) NOT NULL,
    executed_at {{TIMESTAMP}}
);

-- ===============================================================================
-- 드라이버 플러그인 테이블 (시스템 레벨)
-- ===============================================================================
CREATE TABLE IF NOT EXISTS driver_plugins (
    id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
    name VARCHAR(100) NOT NULL,
    protocol_type VARCHAR(50) NOT NULL, -- modbus_tcp, modbus_rtu, bacnet, mqtt, opcua
    version VARCHAR(20) NOT NULL,
    file_path VARCHAR(255) NOT NULL,
    description {{TEXT}},
    author VARCHAR(100),
    api_version INTEGER DEFAULT 1,
    is_enabled {{BOOLEAN}} DEFAULT {{TRUE}},
    config_schema {{TEXT}}, -- JSON Schema for driver configuration
    installed_at {{TIMESTAMP}},
    updated_at {{TIMESTAMP}},
    
    UNIQUE(protocol_type, version)
);

-- ===============================================================================
-- 기본 인덱스 생성
-- ===============================================================================

-- 테넌트 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_tenants_company_code ON tenants(company_code);
CREATE INDEX IF NOT EXISTS idx_tenants_status ON tenants(subscription_status);
CREATE INDEX IF NOT EXISTS idx_tenants_active ON tenants(is_active);

-- Edge 서버 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_edge_servers_tenant ON edge_servers(tenant_id);
CREATE INDEX IF NOT EXISTS idx_edge_servers_status ON edge_servers(status);
CREATE INDEX IF NOT EXISTS idx_edge_servers_last_seen ON edge_servers(last_seen);

-- 사용량 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_usage_metrics_tenant_date ON usage_metrics(tenant_id, measurement_date);
CREATE INDEX IF NOT EXISTS idx_usage_metrics_type ON usage_metrics(metric_type);

-- 과금 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_billing_records_tenant ON billing_records(tenant_id);
CREATE INDEX IF NOT EXISTS idx_billing_records_period ON billing_records(billing_period_start, billing_period_end);
CREATE INDEX IF NOT EXISTS idx_billing_records_status ON billing_records(status);

-- 시스템 설정 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_system_settings_category ON system_settings(category);
CREATE INDEX IF NOT EXISTS idx_system_settings_public ON system_settings(is_public);

-- 드라이버 플러그인 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_driver_plugins_protocol ON driver_plugins(protocol_type);
CREATE INDEX IF NOT EXISTS idx_driver_plugins_enabled ON driver_plugins(is_enabled);