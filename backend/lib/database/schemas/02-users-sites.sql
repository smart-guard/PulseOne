-- =============================================================================
-- backend/lib/database/schemas/02-users-sites.sql
-- 통합 사용자 및 사이트 테이블 (SQLite 버전) - 2025-08-14 최신 업데이트
-- PulseOne v2.1.0 멀티테넌트 & 계층구조 완전 지원
-- =============================================================================

-- =============================================================================
-- 통합 사용자 테이블 - 시스템 관리자 + 테넌트 사용자 통합
-- =============================================================================
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER,                                    -- NULL = 시스템 관리자, 값 있음 = 테넌트 사용자
    
    -- 🔥 기본 인증 정보
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    
    -- 🔥 개인 정보
    full_name VARCHAR(100),
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    phone VARCHAR(20),
    department VARCHAR(100),
    position VARCHAR(100),
    employee_id VARCHAR(50),
    
    -- 🔥 통합 권한 시스템
    role VARCHAR(20) NOT NULL,                            -- system_admin, company_admin, site_admin, engineer, operator, viewer
    permissions TEXT,                                     -- JSON 배열: ["manage_users", "view_data", "control_devices"]
    
    -- 🔥 접근 범위 제어 (테넌트 사용자만)
    site_access TEXT,                                     -- JSON 배열: [1, 2, 3] (접근 가능한 사이트 ID들)
    device_access TEXT,                                   -- JSON 배열: [10, 11, 12] (접근 가능한 디바이스 ID들)
    data_point_access TEXT,                               -- JSON 배열: [100, 101, 102] (접근 가능한 데이터포인트 ID들)
    
    -- 🔥 보안 설정
    two_factor_enabled INTEGER DEFAULT 0,
    two_factor_secret VARCHAR(32),
    password_reset_token VARCHAR(255),
    password_reset_expires DATETIME,
    email_verification_token VARCHAR(255),
    email_verified INTEGER DEFAULT 0,
    
    -- 🔥 계정 상태
    is_active INTEGER DEFAULT 1,
    is_locked INTEGER DEFAULT 0,
    failed_login_attempts INTEGER DEFAULT 0,
    last_login DATETIME,
    last_password_change DATETIME,
    must_change_password INTEGER DEFAULT 0,
    
    -- 🔥 사용자 설정
    timezone VARCHAR(50) DEFAULT 'UTC',
    language VARCHAR(5) DEFAULT 'en',
    theme VARCHAR(20) DEFAULT 'light',                    -- light, dark, auto
    notification_preferences TEXT,                        -- JSON 객체
    
    -- 🔥 감사 정보
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_activity DATETIME,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_role CHECK (role IN ('system_admin', 'company_admin', 'site_admin', 'engineer', 'operator', 'viewer')),
    CONSTRAINT chk_theme CHECK (theme IN ('light', 'dark', 'auto'))
);

-- =============================================================================
-- 사용자 세션 관리 테이블
-- =============================================================================
CREATE TABLE IF NOT EXISTS user_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    
    -- 🔥 세션 정보
    token_hash VARCHAR(255) NOT NULL,
    refresh_token_hash VARCHAR(255),
    session_id VARCHAR(100) UNIQUE NOT NULL,
    
    -- 🔥 접속 정보
    ip_address VARCHAR(45),
    user_agent TEXT,
    device_fingerprint VARCHAR(255),
    
    -- 🔥 위치 정보 (선택)
    country VARCHAR(2),
    city VARCHAR(100),
    
    -- 🔥 세션 상태
    is_active INTEGER DEFAULT 1,
    expires_at DATETIME NOT NULL,
    last_used DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 🔥 메타데이터
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- =============================================================================
-- 사이트 테이블 - 완전한 계층구조 지원
-- =============================================================================
CREATE TABLE IF NOT EXISTS sites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    parent_site_id INTEGER,                               -- 계층 구조 지원
    
    -- 🔥 사이트 기본 정보
    name VARCHAR(100) NOT NULL,
    code VARCHAR(20) NOT NULL,                            -- 테넌트 내 고유 코드
    site_type VARCHAR(50) NOT NULL,                       -- company, factory, office, building, floor, line, area, zone
    description TEXT,
    
    -- 🔥 위치 정보
    location VARCHAR(200),
    address TEXT,
    coordinates VARCHAR(100),                             -- GPS 좌표 "lat,lng"
    postal_code VARCHAR(20),
    country VARCHAR(2),                                   -- ISO 3166-1 alpha-2
    city VARCHAR(100),
    state_province VARCHAR(100),
    
    -- 🔥 시간대 및 지역 설정
    timezone VARCHAR(50) DEFAULT 'UTC',
    currency VARCHAR(3) DEFAULT 'USD',
    language VARCHAR(5) DEFAULT 'en',
    
    -- 🔥 연락처 정보
    manager_name VARCHAR(100),
    manager_email VARCHAR(100),
    manager_phone VARCHAR(20),
    emergency_contact VARCHAR(200),
    
    -- 🔥 운영 정보
    operating_hours VARCHAR(100),                         -- "08:00-18:00" 또는 "24/7"
    shift_pattern VARCHAR(50),                            -- "3-shift", "2-shift", "day-only"
    working_days VARCHAR(20) DEFAULT 'MON-FRI',           -- "MON-FRI", "MON-SAT", "24/7"
    
    -- 🔥 시설 정보
    floor_area REAL,                                      -- 면적 (평방미터)
    ceiling_height REAL,                                  -- 천장 높이 (미터)
    max_occupancy INTEGER,                                -- 최대 수용 인원
    
    -- 🔥 Edge 서버 연결
    edge_server_id INTEGER,
    
    -- 🔥 계층 및 정렬
    hierarchy_level INTEGER DEFAULT 0,                   -- 0=Company, 1=Factory, 2=Building, 3=Line, etc.
    hierarchy_path VARCHAR(500),                         -- "/1/2/3" 형태 경로
    sort_order INTEGER DEFAULT 0,
    
    -- 🔥 상태 및 설정
    is_active INTEGER DEFAULT 1,
    is_visible INTEGER DEFAULT 1,                        -- 사용자에게 표시 여부
    monitoring_enabled INTEGER DEFAULT 1,                -- 모니터링 활성화
    
    -- 🔥 메타데이터
    tags TEXT,                                           -- JSON 배열
    metadata TEXT,                                       -- JSON 객체 (추가 속성)
    
    -- 🔥 감사 정보
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (parent_site_id) REFERENCES sites(id) ON DELETE SET NULL,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    UNIQUE(tenant_id, code),
    CONSTRAINT chk_site_type CHECK (site_type IN ('company', 'factory', 'office', 'building', 'floor', 'line', 'area', 'zone', 'room'))
);

-- =============================================================================
-- 사용자 즐겨찾기 테이블
-- =============================================================================
CREATE TABLE IF NOT EXISTS user_favorites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    
    -- 🔥 즐겨찾기 대상
    target_type VARCHAR(20) NOT NULL,                    -- 'site', 'device', 'data_point', 'alarm', 'dashboard'
    target_id INTEGER NOT NULL,
    
    -- 🔥 메타데이터
    display_name VARCHAR(100),
    notes TEXT,
    sort_order INTEGER DEFAULT 0,
    
    -- 🔥 감사 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    UNIQUE(user_id, target_type, target_id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_target_type CHECK (target_type IN ('site', 'device', 'data_point', 'alarm', 'dashboard', 'virtual_point'))
);

-- =============================================================================
-- 사용자 알림 설정 테이블
-- =============================================================================
CREATE TABLE IF NOT EXISTS user_notification_settings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    
    -- 🔥 알림 채널 설정
    email_enabled INTEGER DEFAULT 1,
    sms_enabled INTEGER DEFAULT 0,
    push_enabled INTEGER DEFAULT 1,
    slack_enabled INTEGER DEFAULT 0,
    teams_enabled INTEGER DEFAULT 0,
    
    -- 🔥 알림 타입별 설정
    alarm_notifications INTEGER DEFAULT 1,
    system_notifications INTEGER DEFAULT 1,
    maintenance_notifications INTEGER DEFAULT 1,
    report_notifications INTEGER DEFAULT 0,
    
    -- 🔥 알림 시간 설정
    quiet_hours_start TIME,                              -- 알림 차단 시작 시간
    quiet_hours_end TIME,                                -- 알림 차단 종료 시간
    weekend_notifications INTEGER DEFAULT 0,             -- 주말 알림 허용
    
    -- 🔥 연락처 정보
    sms_phone VARCHAR(20),
    slack_webhook_url VARCHAR(255),
    teams_webhook_url VARCHAR(255),
    
    -- 🔥 메타데이터
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    UNIQUE(user_id)
);

-- =============================================================================
-- 인덱스 생성 (성능 최적화)
-- =============================================================================

-- users 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_users_tenant ON users(tenant_id);
CREATE INDEX IF NOT EXISTS idx_users_role ON users(role);
CREATE INDEX IF NOT EXISTS idx_users_active ON users(is_active);
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_users_last_login ON users(last_login DESC);
CREATE INDEX IF NOT EXISTS idx_users_employee_id ON users(employee_id);

-- user_sessions 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_user_sessions_user ON user_sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_user_sessions_token ON user_sessions(token_hash);
CREATE INDEX IF NOT EXISTS idx_user_sessions_expires ON user_sessions(expires_at);
CREATE INDEX IF NOT EXISTS idx_user_sessions_active ON user_sessions(is_active);

-- sites 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_sites_tenant ON sites(tenant_id);
CREATE INDEX IF NOT EXISTS idx_sites_parent ON sites(parent_site_id);
CREATE INDEX IF NOT EXISTS idx_sites_type ON sites(site_type);
CREATE INDEX IF NOT EXISTS idx_sites_hierarchy ON sites(hierarchy_level);
CREATE INDEX IF NOT EXISTS idx_sites_active ON sites(is_active);
CREATE INDEX IF NOT EXISTS idx_sites_code ON sites(tenant_id, code);
CREATE INDEX IF NOT EXISTS idx_sites_edge_server ON sites(edge_server_id);
CREATE INDEX IF NOT EXISTS idx_sites_hierarchy_path ON sites(hierarchy_path);

-- user_favorites 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_user_favorites_user ON user_favorites(user_id);
CREATE INDEX IF NOT EXISTS idx_user_favorites_target ON user_favorites(target_type, target_id);
CREATE INDEX IF NOT EXISTS idx_user_favorites_sort ON user_favorites(user_id, sort_order);

-- user_notification_settings 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_user_notification_user ON user_notification_settings(user_id);