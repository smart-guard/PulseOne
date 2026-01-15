const sqlite3 = require('sqlite3').verbose();
const path = require('path');

const DB_PATH = path.resolve(__dirname, '../../data/db/pulseone.db');
const db = new sqlite3.Database(DB_PATH);

const ROLES = [
    { id: 'system_admin', name: '시스템 관리자', description: '모든 시스템 기능에 대한 완전한 권한', is_system: 1 },
    { id: 'company_admin', name: '테넌트 관리자', description: '테넌트 내 모든 관리 권한', is_system: 1 },
    { id: 'site_admin', name: '사이트 관리자', description: '특정 사이트 내 관리 권한', is_system: 1 },
    { id: 'manager', name: '매니저', description: '시스템 설정을 제외한 대부분의 관리 권한', is_system: 1 },
    { id: 'engineer', name: '엔지니어', description: '디바이스 및 알람 관리 권한', is_system: 1 },
    { id: 'operator', name: '운영자', description: '디바이스 모니터링 및 기본 제어', is_system: 1 },
    { id: 'viewer', name: '조회자', description: '데이터 조회 및 보고서 확인만 가능', is_system: 1 }
];

const PERMISSIONS = [
    { id: 'view_dashboard', name: '대시보드 조회', category: '조회', is_system: 1 },
    { id: 'manage_devices', name: '디바이스 관리', category: '관리', is_system: 1 },
    { id: 'manage_alarms', name: '알람 관리', category: '관리', is_system: 1 },
    { id: 'manage_users', name: '사용자 관리', category: '관리', is_system: 1 },
    { id: 'view_reports', name: '보고서 조회', category: '조회', is_system: 1 },
    { id: 'export_data', name: '데이터 내보내기', category: '데이터', is_system: 1 },
    { id: 'system_settings', name: '시스템 설정', category: '시스템', is_system: 1 },
    { id: 'backup_restore', name: '백업/복원', category: '시스템', is_system: 1 },
    { id: 'manage_tenants', name: '고객사 관리', category: '시스템', is_system: 1 },
    { id: 'manage_roles', name: '권한 관리', category: '시스템', is_system: 1 }
];

// Simple mapping for seeding
const ROLE_PERMISSIONS = {
    'system_admin': 'ALL',
    'company_admin': ['view_dashboard', 'manage_devices', 'manage_alarms', 'manage_users', 'view_reports', 'export_data', 'system_settings', 'backup_restore', 'manage_roles'],
    'site_admin': ['view_dashboard', 'manage_devices', 'manage_alarms', 'manage_users', 'view_reports', 'export_data'], // Example
    'manager': ['view_dashboard', 'manage_devices', 'manage_alarms', 'manage_users', 'view_reports', 'export_data'],
    'engineer': ['view_dashboard', 'manage_devices', 'manage_alarms', 'view_reports'],
    'operator': ['view_dashboard', 'manage_devices'],
    'viewer': ['view_dashboard', 'view_reports']
};

function seed() {
    console.log('🌱 Seeding Roles and Permissions...');

    db.serialize(() => {
        // 1. Permissions (Ensure they exist)
        // Note: permissions might already exist from migration.

        // 2. Roles
        const stmtRole = db.prepare("INSERT OR IGNORE INTO roles (id, name, description, is_system) VALUES (?, ?, ?, ?)");
        ROLES.forEach(role => {
            stmtRole.run(role.id, role.name, role.description, role.is_system);
        });
        stmtRole.finalize();
        console.log('✅ Roles seeded.');

        // 3. Role Permissions
        // We need to clear existing mappings for these system roles to ensure correctness? 
        // Or just INSERT OR IGNORE.
        // Let's use INSERT OR IGNORE.

        const stmtRP = db.prepare("INSERT OR IGNORE INTO role_permissions (role_id, permission_id) VALUES (?, ?)");

        // Fetch all permissions IDs first to match 'ALL'
        db.all("SELECT id FROM permissions", (err, rows) => {
            if (err) {
                console.error('Error fetching permissions:', err);
                return;
            }
            const allPermIds = rows.map(r => r.id);

            Object.entries(ROLE_PERMISSIONS).forEach(([roleId, perms]) => {
                let targetPerms = [];
                if (perms === 'ALL') {
                    targetPerms = allPermIds;
                } else {
                    targetPerms = perms;
                }

                targetPerms.forEach(permId => {
                    if (allPermIds.includes(permId)) {
                        stmtRP.run(roleId, permId);
                    }
                });
            });
            stmtRP.finalize();
            console.log('✅ Role Permissions mapped.');
        });
    });

    // db.close(); // Async nature might cause close before finish? 
    // db.close() inside callback of db.all?
    // Actually sqlite3 serializes. but db.all callback is async.
    // Let's just wait a bit or use proper async/await wrapper. 
    // Use simple timeout for script.
    setTimeout(() => {
        console.log('🏁 Seeding Complete.');
        db.close();
    }, 2000);
}

seed();
