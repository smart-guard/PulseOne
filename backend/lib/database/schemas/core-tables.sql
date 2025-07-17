-- backend/lib/database/schemas/core-tables.sql
-- PulseOne 기본 시스템 테이블 (멀티 테넌트 지원)

-- 스키마 버전 관리 테이블
CREATE TABLE IF NOT EXISTS schema_versions (
    id {{AUTO_INCREMENT}},
    version VARCHAR(20) NOT NULL,
    applied_at {{TIMESTAMP}},
    description TEXT
);

-- 시스템 관리자 테이블
CREATE TABLE IF NOT EXISTS system_admins (
    id {{AUTO_INCREMENT}},
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(100),
    is_active {{BOOLEAN}} DEFAULT {{TRUE}},
    last_login {{TIMESTAMP}},
    created_at {{TIMESTAMP}},
    updated_at {{TIMESTAMP}}
);

-- 테넌트(회사) 테이블
CREATE TABLE IF NOT EXISTS tenants (
    id {{AUTO_INCREMENT}},
    company_name VARCHAR(100) NOT NULL UNIQUE,
    company_code VARCHAR(20) NOT NULL UNIQUE,
    domain VARCHAR(100) UNIQUE,
    
    -- 연락처 정보
    contact_name VARCHAR(100),
    contact_email VARCHAR(100),
    contact_phone VARCHAR(20),
    
    -- 구독 정보
    subscription_plan VARCHAR(20) DEFAULT 'starter',
    subscription_status VARCHAR(20) DEFAULT 'active',
    max_edge_servers INTEGER DEFAULT 3,
    max_data_points INTEGER DEFAULT 1000,
    max_users INTEGER DEFAULT 5,
    
    -- 상태 정보
    is_active {{BOOLEAN}} DEFAULT {{TRUE}},
    trial_end_date {{TIMESTAMP}},
    created_at {{TIMESTAMP}},
    updated_at {{TIMESTAMP}}
);

-- Edge 서버 등록 테이블
CREATE TABLE IF NOT EXISTS edge_servers (
    id {{AUTO_INCREMENT}},
    tenant_id INTEGER NOT NULL,
    
    -- 서버 식별 정보
    server_name VARCHAR(100) NOT NULL,
    factory_name VARCHAR(100),
    location VARCHAR(200),
    
    -- 네트워크 정보
    ip_address VARCHAR(45),
    mac_address VARCHAR(17),
    hostname VARCHAR(100),
    
    -- 등록 정보
    registration_token VARCHAR(255) UNIQUE,
    activation_code VARCHAR(50),
    
    -- 상태 정보
    status VARCHAR(20) DEFAULT 'pending',
    last_seen {{TIMESTAMP}},
    version VARCHAR(20),
    
    -- 설정 정보
    config {{TEXT}},
    sync_enabled {{BOOLEAN}} DEFAULT {{TRUE}},
    
    created_at {{TIMESTAMP}},
    updated_at {{TIMESTAMP}},
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);

-- 시스템 설정 테이블
CREATE TABLE IF NOT EXISTS system_settings (
    id {{AUTO_INCREMENT}},
    key_name VARCHAR(100) UNIQUE NOT NULL,
    value {{TEXT}} NOT NULL,
    description TEXT,
    category VARCHAR(50) DEFAULT 'general',
    is_public {{BOOLEAN}} DEFAULT {{FALSE}},
    updated_by INTEGER,
    updated_at {{TIMESTAMP}},
    
    FOREIGN KEY (updated_by) REFERENCES system_admins(id)
);