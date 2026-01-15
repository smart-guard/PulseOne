-- =============================================================================
-- Migration: Implement RBAC (Role-Based Access Control) - FIXED
-- =============================================================================

BEGIN TRANSACTION;

-- 1. Create Permissions Table
CREATE TABLE IF NOT EXISTS permissions (
    id VARCHAR(50) PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    category VARCHAR(50),
    resource VARCHAR(50),
    actions TEXT,           -- JSON array: ["read", "write"]
    is_system INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 2. Create Roles Table
CREATE TABLE IF NOT EXISTS roles (
    id VARCHAR(50) PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    is_system INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 3. Create Role-Permissions Mapping Table
CREATE TABLE IF NOT EXISTS role_permissions (
    role_id VARCHAR(50) NOT NULL,
    permission_id VARCHAR(50) NOT NULL,
    assigned_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (role_id, permission_id),
    FOREIGN KEY (role_id) REFERENCES roles(id) ON DELETE CASCADE,
    FOREIGN KEY (permission_id) REFERENCES permissions(id) ON DELETE CASCADE
);

-- 4. Seed Default Permissions
INSERT OR IGNORE INTO permissions (id, name, description, category, resource, actions, is_system) VALUES
('view_dashboard', '대시보드 조회', '메인 대시보드 및 실시간 데이터 조회', '조회', 'dashboard', '["read"]', 1),
('manage_devices', '디바이스 관리', '디바이스 설정, 제어, 추가/삭제', '관리', 'devices', '["create","read","update","delete"]', 1),
('manage_alarms', '알람 관리', '알람 규칙 설정 및 이벤트 처리', '관리', 'alarms', '["create","read","update","delete"]', 1),
('manage_users', '사용자 관리', '사용자 계정 및 권한 관리', '관리', 'users', '["create","read","update","delete"]', 1),
('view_reports', '보고서 조회', '각종 보고서 및 분석 자료 조회', '조회', 'reports', '["read"]', 1),
('export_data', '데이터 내보내기', '시스템 데이터 내보내기', '데이터', 'data', '["export"]', 1),
('system_settings', '시스템 설정', '시스템 전역 설정 관리', '시스템', 'settings', '["read","update"]', 1),
('backup_restore', '백업/복원', '시스템 백업 및 복원', '시스템', 'backup', '["create","restore"]', 1),
('manage_tenants', '고객사 관리', '시스템 관리자 전용 고객사 관리', '시스템', 'tenants', '["create","read","update","delete"]', 1),
('manage_roles', '권한 관리', '역할 및 권한 설정 관리', '시스템', 'permissions', '["create","read","update","delete"]', 1);

-- 5. Seed Default Roles
INSERT OR IGNORE INTO roles (id, name, description, is_system) VALUES
('system_admin', '시스템 관리자', '모든 시스템 기능에 대한 완전한 권한', 1),
('company_admin', '테넌트 관리자', '테넌트 내 모든 관리 권한', 1),
('site_admin', '사이트 관리자', '특정 사이트 내 관리 권한', 1),
('manager', '매니저', '시스템 설정을 제외한 대부분의 관리 권한', 1),
('engineer', '엔지니어', '디바이스 및 알람 관리 권한', 1),
('operator', '운영자', '디바이스 모니터링 및 기본 제어', 1),
('viewer', '조회자', '데이터 조회 및 보고서 확인만 가능', 1);

-- 6. Seed Role Permissions
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) SELECT 'system_admin', id FROM permissions;
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) SELECT 'company_admin', id FROM permissions WHERE id != 'manage_tenants';
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) SELECT 'manager', id FROM permissions WHERE id IN ('view_dashboard', 'manage_devices', 'manage_alarms', 'manage_users', 'view_reports', 'export_data');
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) SELECT 'engineer', id FROM permissions WHERE id IN ('view_dashboard', 'manage_devices', 'manage_alarms', 'view_reports');
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) SELECT 'operator', id FROM permissions WHERE id IN ('view_dashboard', 'manage_devices');
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) SELECT 'viewer', id FROM permissions WHERE id IN ('view_dashboard', 'view_reports');

-- 7. Migrate Users Table
CREATE TABLE users_new (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER,
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(100),
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    phone VARCHAR(20),
    department VARCHAR(100),
    position VARCHAR(100),
    employee_id VARCHAR(50),
    
    role VARCHAR(50) NOT NULL,
    permissions TEXT,
    
    site_access TEXT,
    device_access TEXT,
    data_point_access TEXT,
    
    two_factor_enabled INTEGER DEFAULT 0,
    two_factor_secret VARCHAR(32),
    password_reset_token VARCHAR(255),
    password_reset_expires DATETIME,
    email_verification_token VARCHAR(255),
    email_verified INTEGER DEFAULT 0,
    
    is_active INTEGER DEFAULT 1,
    is_deleted INTEGER DEFAULT 0,  -- Defined here in middle
    is_locked INTEGER DEFAULT 0,
    failed_login_attempts INTEGER DEFAULT 0,
    last_login DATETIME,
    last_password_change DATETIME,
    must_change_password INTEGER DEFAULT 0,
    
    timezone VARCHAR(50) DEFAULT 'UTC',
    language VARCHAR(5) DEFAULT 'en',
    theme VARCHAR(20) DEFAULT 'light',
    notification_preferences TEXT,
    
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_activity DATETIME,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id),
    FOREIGN KEY (role) REFERENCES roles(id) ON DELETE RESTRICT,
    CONSTRAINT chk_theme CHECK (theme IN ('light', 'dark', 'auto'))
);

-- EXPLICIT INSERT to handle column order mismatch (is_deleted is last in source)
INSERT INTO users_new (
    id, tenant_id, username, email, password_hash, full_name, first_name, last_name, phone, department, position, employee_id,
    role, permissions, site_access, device_access, data_point_access,
    two_factor_enabled, two_factor_secret, password_reset_token, password_reset_expires, email_verification_token, email_verified,
    is_active, is_deleted, is_locked, failed_login_attempts, last_login, last_password_change, must_change_password,
    timezone, language, theme, notification_preferences,
    created_by, created_at, updated_at, last_activity
)
SELECT
    id, tenant_id, username, email, password_hash, full_name, first_name, last_name, phone, department, position, employee_id,
    role, permissions, site_access, device_access, data_point_access,
    two_factor_enabled, two_factor_secret, password_reset_token, password_reset_expires, email_verification_token, email_verified,
    is_active, is_deleted, is_locked, failed_login_attempts, last_login, last_password_change, must_change_password,
    timezone, language, theme, notification_preferences,
    created_by, created_at, updated_at, last_activity
FROM users;

DROP TABLE users;
ALTER TABLE users_new RENAME TO users;

-- Recreate Indices
CREATE INDEX IF NOT EXISTS idx_users_tenant ON users(tenant_id);
CREATE INDEX IF NOT EXISTS idx_users_role ON users(role);
CREATE INDEX IF NOT EXISTS idx_users_active ON users(is_active);
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_users_last_login ON users(last_login DESC);
CREATE INDEX IF NOT EXISTS idx_users_employee_id ON users(employee_id);

COMMIT;
