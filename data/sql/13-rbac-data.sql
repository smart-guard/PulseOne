-- =============================================================================
-- data/sql/13-rbac-system.sql
-- RBAC (Role-Based Access Control) 시스템 테이블 및 초기 데이터
-- =============================================================================

-- 1. 권한 테이블 (permissions)
CREATE TABLE IF NOT EXISTS permissions (
    id VARCHAR(50) PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    category VARCHAR(50),
    resource VARCHAR(50),
    actions TEXT,            -- JSON 배열: ["read", "write", "delete"]
    is_system INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 2. 역할 테이블 (roles)
CREATE TABLE IF NOT EXISTS roles (
    id VARCHAR(50) PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    is_system INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 3. 역할-권한 매핑 테이블 (role_permissions)
CREATE TABLE IF NOT EXISTS role_permissions (
    role_id VARCHAR(50),
    permission_id VARCHAR(50),
    PRIMARY KEY (role_id, permission_id),
    FOREIGN KEY (role_id) REFERENCES roles(id) ON DELETE CASCADE,
    FOREIGN KEY (permission_id) REFERENCES permissions(id) ON DELETE CASCADE
);

-- =============================================================================
-- 초기 권한 데이터 삽입
-- =============================================================================
INSERT OR IGNORE INTO permissions (id, name, description, category, resource, actions, is_system) VALUES
('view_dashboard', '대시보드 조회', '시스템 현황 대시보드를 조회할 수 있는 권한', '조회', 'dashboard', '["read"]', 1),
('manage_devices', '디바이스 관리', '디바이스 등록, 수정, 삭제 및 제어 권한', '관리', 'devices', '["read", "write", "delete"]', 1),
('manage_alarms', '알람 관리', '알람 규칙 설정 및 알람 이력 관리 권한', '관리', 'alarms', '["read", "write", "delete"]', 1),
('manage_users', '사용자 관리', '사용자 계정 생성, 수정, 삭제 및 권한 할당 권한', '관리', 'users', '["read", "write", "delete"]', 1),
('view_reports', '보고서 조회', '수집 데이터 보고서 및 통계 자료 조회 권한', '조회', 'reports', '["read"]', 1),
('export_data', '데이터 내보내기', '수집 데이터를 외부 파일(CSV, Excel 등)로 추출하는 권한', '데이터', 'data', '["read", "execute"]', 1),
('system_settings', '시스템 설정', '시스템 환경 설정 및 네트워크 설정 변경 권한', '시스템', 'settings', '["read", "write"]', 1),
('backup_restore', '백업/복원', '데이터베이스 백업 생성 및 시스템 복원 권한', '시스템', 'backup', '["read", "execute"]', 1),
('manage_tenants', '고객사 관리', '테넌트(고객사) 정보 및 라이선스 관리 권한', '시스템', 'tenants', '["read", "write", "delete"]', 1),
('manage_roles', '권한 관리', '시스템 역할(Role) 및 세부 권한 정의 권한', '시스템', 'permissions', '["read", "write", "delete"]', 1);

-- =============================================================================
-- 초기 역할(Role) 데이터 삽입
-- =============================================================================
INSERT OR IGNORE INTO roles (id, name, description, is_system) VALUES
('system_admin', '시스템 관리자', '시스템 전체 리소스 및 모든 테넌트에 대한 전체 제어 권한', 1),
('company_admin', '고객사 관리자', '해당 테넌트(고객사) 내의 모든 리소스 및 사용자 관리 권한', 1),
('manager', '운영 관리자', '현장 운영 및 사용자 관리를 포함한 일반적인 관리 권한', 1),
('engineer', '엔지니어', '디바이스 설정 및 알람 규칙 관리 등 기술적인 운영 권한', 1),
('operator', '운전원', '실시간 모니터링 및 디바이스 상태 확인 권한', 1),
('viewer', '조회자', '데이터 및 보고서 조회만 가능한 제한된 권한', 1);

-- =============================================================================
-- 역할별 권한 매핑 (Seed)
-- =============================================================================

-- 1. system_admin: 모든 권한
INSERT OR IGNORE INTO role_permissions (role_id, permission_id)
SELECT 'system_admin', id FROM permissions;

-- 2. company_admin: manage_tenants 제외한 모든 권한
INSERT OR IGNORE INTO role_permissions (role_id, permission_id)
SELECT 'company_admin', id FROM permissions WHERE id != 'manage_tenants';

-- 3. manager: 주요 운영 및 관리 권한
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) VALUES
('manager', 'view_dashboard'),
('manager', 'manage_devices'),
('manager', 'manage_alarms'),
('manager', 'manage_users'),
('manager', 'view_reports'),
('manager', 'export_data');

-- 4. engineer: 기술 운영 권한
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) VALUES
('engineer', 'view_dashboard'),
('engineer', 'manage_devices'),
('engineer', 'manage_alarms'),
('engineer', 'view_reports');

-- 5. operator: 단순 모니터링 및 조작
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) VALUES
('operator', 'view_dashboard'),
('operator', 'manage_devices');

-- 6. viewer: 단순 조회
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) VALUES
('viewer', 'view_dashboard'),
('viewer', 'view_reports');
