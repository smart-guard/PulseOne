-- =============================================================================
-- backend/lib/database/schemas/02-users-sites.sql
-- 통합 사용자 및 사이트 테이블 (SQLite 버전)
-- =============================================================================

-- 통합 사용자 테이블
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER, -- NULL = 시스템 관리자, 값 있음 = 테넌트 사용자
    
    -- 기본 정보
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(100),
    department VARCHAR(100),
    
    -- 통합 권한 시스템
    role VARCHAR(20) NOT NULL, -- system_admin, company_admin, site_admin, engineer, operator, viewer
    permissions TEXT, -- JSON 배열: ["manage_users", "view_data", "control_devices"]
    
    -- 접근 범위 (테넌트 사용자만)
    site_access TEXT, -- JSON 배열: [1, 2, 3] (접근 가능한 사이트 ID들)
    device_access TEXT, -- JSON 배열: [10, 11, 12] (접근 가능한 디바이스 ID들)
    
    -- 상태 정보
    is_active INTEGER DEFAULT 1,
    last_login DATETIME,
    password_reset_token VARCHAR(255),
    password_reset_expires DATETIME,
    
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);

-- 사용자 세션 테이블
CREATE TABLE IF NOT EXISTS user_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    token_hash VARCHAR(255) NOT NULL,
    ip_address VARCHAR(45),
    user_agent TEXT,
    expires_at DATETIME NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- 사이트 테이블 (공장, 빌딩, 라인 등 통합)
CREATE TABLE IF NOT EXISTS sites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    parent_site_id INTEGER,
    
    -- 사이트 기본 정보
    name VARCHAR(100) NOT NULL,
    code VARCHAR(20) NOT NULL,
    site_type VARCHAR(50) NOT NULL, -- company, factory, office, building, floor, line, area, zone
    description TEXT,
    
    -- 위치 정보
    location VARCHAR(200),
    address TEXT,
    coordinates VARCHAR(100), -- GPS 좌표 "lat,lng"
    timezone VARCHAR(50) DEFAULT 'UTC',
    
    -- 연락처 정보
    manager_name VARCHAR(100),
    manager_email VARCHAR(100),
    manager_phone VARCHAR(20),
    
    -- 운영 정보
    operating_hours VARCHAR(100), -- "08:00-18:00" 또는 "24/7"
    shift_pattern VARCHAR(50), -- "3-shift", "2-shift", "day-only"
    
    -- Edge 서버 정보
    edge_server_id INTEGER,
    
    -- 계층 및 정렬
    hierarchy_level INTEGER DEFAULT 0, -- 0=Company, 1=Factory, 2=Building, 3=Line
    sort_order INTEGER DEFAULT 0,
    
    -- 상태 정보
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (parent_site_id) REFERENCES sites(id) ON DELETE SET NULL,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, code)
);
