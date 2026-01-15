-- Seed default roles directly via CLI
BEGIN TRANSACTION;

INSERT OR IGNORE INTO roles (id, name, description, is_system) VALUES
('system_admin', '시스템 관리자', '모든 시스템 기능에 대한 완전한 권한', 1),
('company_admin', '테넌트 관리자', '테넌트 내 모든 관리 권한', 1),
('site_admin', '사이트 관리자', '특정 사이트 내 관리 권한', 1),
('manager', '매니저', '시스템 설정을 제외한 대부분의 관리 권한', 1),
('engineer', '엔지니어', '디바이스 및 알람 관리 권한', 1),
('operator', '운영자', '디바이스 모니터링 및 기본 제어', 1),
('viewer', '조회자', '데이터 조회 및 보고서 확인만 가능', 1);

-- Ensure permissions exist just in case
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

-- Map Permissions
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) SELECT 'system_admin', id FROM permissions;
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) SELECT 'company_admin', id FROM permissions WHERE id != 'manage_tenants';
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) SELECT 'manager', id FROM permissions WHERE id IN ('view_dashboard', 'manage_devices', 'manage_alarms', 'manage_users', 'view_reports', 'export_data');
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) SELECT 'engineer', id FROM permissions WHERE id IN ('view_dashboard', 'manage_devices', 'manage_alarms', 'view_reports');
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) SELECT 'operator', id FROM permissions WHERE id IN ('view_dashboard', 'manage_devices');
INSERT OR IGNORE INTO role_permissions (role_id, permission_id) SELECT 'viewer', id FROM permissions WHERE id IN ('view_dashboard', 'view_reports');

COMMIT;
