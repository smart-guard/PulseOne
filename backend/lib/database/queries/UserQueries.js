// =============================================================================
// backend/lib/database/queries/UserQueries.js
// User 관련 SQL 쿼리 모음 - 스키마와 완전 일치
// =============================================================================

/**
 * @class UserQueries
 * @description User 테이블 관련 모든 SQL 쿼리 정의
 * 
 * 🔥 실제 스키마 구조:
 * - id INTEGER PRIMARY KEY AUTOINCREMENT
 * - tenant_id INTEGER (NULL = 시스템 관리자)
 * - username VARCHAR(50) UNIQUE NOT NULL
 * - email VARCHAR(100) UNIQUE NOT NULL
 * - password_hash VARCHAR(255) NOT NULL
 * - full_name VARCHAR(100)
 * - department VARCHAR(100)
 * - role VARCHAR(20) NOT NULL (system_admin, company_admin, site_admin, engineer, operator, viewer)
 * - permissions TEXT (JSON 배열)
 * - site_access TEXT (JSON 배열)
 * - device_access TEXT (JSON 배열)
 * - is_active INTEGER DEFAULT 1
 * - last_login DATETIME
 * - password_reset_token VARCHAR(255)
 * - password_reset_expires DATETIME
 * - created_at DATETIME DEFAULT CURRENT_TIMESTAMP
 * - updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
 */
class UserQueries {
    // ==========================================================================
    // 기본 조회 쿼리들
    // ==========================================================================

    /**
     * 모든 사용자 조회
     */
    static findAll() {
        return `
            SELECT 
                id, tenant_id, username, email, full_name, department,
                role, permissions, site_access, device_access,
                is_active, last_login, created_at, updated_at
            FROM users 
            WHERE is_deleted = 0
            ORDER BY username
        `;
    }

    /**
     * ID로 사용자 조회
     */
    static findById() {
        return `
            SELECT 
                id, tenant_id, username, email, full_name, department,
                role, permissions, site_access, device_access,
                is_active, last_login, created_at, updated_at
            FROM users 
            WHERE id = ? AND is_deleted = 0
        `;
    }

    // ==========================================================================
    // 인증 관련 조회 쿼리들
    // ==========================================================================

    /**
     * 사용자명으로 조회 (로그인용)
     */
    static findByUsername() {
        return `
            SELECT 
                id, tenant_id, username, email, password_hash, full_name, department,
                role, permissions, site_access, device_access,
                is_active, last_login, created_at, updated_at
            FROM users 
            WHERE username = ? AND is_deleted = 0
        `;
    }

    /**
     * 이메일로 조회
     */
    static findByEmail() {
        return `
            SELECT 
                id, tenant_id, username, email, password_hash, full_name, department,
                role, permissions, site_access, device_access,
                is_active, last_login, created_at, updated_at
            FROM users 
            WHERE email = ? AND is_deleted = 0
        `;
    }

    /**
     * 로그인용 인증 정보 조회 (비밀번호 포함)
     */
    static findForAuthentication() {
        return `
            SELECT 
                id, tenant_id, username, email, password_hash, 
                role, is_active, last_login
            FROM users 
            WHERE (username = ? OR email = ?) AND is_active = 1 AND is_deleted = 0
        `;
    }

    // ==========================================================================
    // 테넌트/역할별 조회 쿼리들
    // ==========================================================================

    /**
     * 테넌트별 사용자 조회
     */
    static findByTenant() {
        return `
            SELECT 
                id, tenant_id, username, email, full_name, department,
                role, permissions, site_access, device_access,
                is_active, last_login, created_at, updated_at
            FROM users 
            WHERE tenant_id = ? AND is_deleted = 0
            ORDER BY username
        `;
    }

    /**
     * 역할별 사용자 조회
     */
    static findByRole() {
        return `
            SELECT 
                id, tenant_id, username, email, full_name, department,
                role, permissions, site_access, device_access,
                is_active, last_login, created_at, updated_at
            FROM users 
            WHERE role = ? AND is_deleted = 0
            ORDER BY username
        `;
    }

    /**
     * 활성 사용자 조회
     */
    static findActiveUsers() {
        return `
            SELECT 
                id, tenant_id, username, email, full_name, department,
                role, permissions, site_access, device_access,
                is_active, last_login, created_at, updated_at
            FROM users 
            WHERE is_active = 1 AND is_deleted = 0
            ORDER BY username
        `;
    }

    /**
     * 테넌트별 활성 사용자 조회
     */
    static findActiveUsersByTenant() {
        return `
            SELECT 
                id, tenant_id, username, email, full_name, department,
                role, permissions, site_access, device_access,
                is_active, last_login, created_at, updated_at
            FROM users 
            WHERE tenant_id = ? AND is_active = 1 AND is_deleted = 0
            ORDER BY username
        `;
    }

    /**
     * 시스템 관리자 조회
     */
    static findSystemAdmins() {
        return `
            SELECT 
                id, tenant_id, username, email, full_name, department,
                role, permissions, is_active, last_login, created_at, updated_at
            FROM users 
            WHERE role = 'system_admin' AND is_active = 1 AND is_deleted = 0
            ORDER BY username
        `;
    }

    // ==========================================================================
    // CRUD 연산 쿼리들
    // ==========================================================================

    /**
     * 새 사용자 생성
     */
    static create() {
        return `
            INSERT INTO users (
                tenant_id, username, email, password_hash, full_name, department,
                role, permissions, site_access, device_access, is_active
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `;
    }

    /**
     * 사용자 정보 수정
     */
    static update() {
        return `
            UPDATE users SET
                username = ?, email = ?, full_name = ?, department = ?,
                role = ?, permissions = ?, site_access = ?, device_access = ?,
                is_active = ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
        `;
    }

    /**
     * 사용자 삭제
     */
    static delete() {
        return 'DELETE FROM users WHERE id = ?';
    }

    /**
     * 사용자 비활성화 (소프트 삭제)
     */
    static deactivate() {
        return `
            UPDATE users SET 
                is_active = 0, 
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    // ==========================================================================
    // 인증 및 세션 관리 쿼리들
    // ==========================================================================

    /**
     * 비밀번호 업데이트
     */
    static updatePassword() {
        return `
            UPDATE users SET 
                password_hash = ?, 
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    /**
     * 마지막 로그인 시간 업데이트
     */
    static updateLastLogin() {
        return `
            UPDATE users SET 
                last_login = CURRENT_TIMESTAMP, 
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    /**
     * 비밀번호 재설정 토큰 설정
     */
    static setPasswordResetToken() {
        return `
            UPDATE users SET 
                password_reset_token = ?, 
                password_reset_expires = ?, 
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    /**
     * 비밀번호 재설정 토큰으로 조회
     */
    static findByPasswordResetToken() {
        return `
            SELECT 
                id, username, email, password_reset_expires
            FROM users 
            WHERE password_reset_token = ? 
              AND password_reset_expires > CURRENT_TIMESTAMP
              AND is_active = 1
        `;
    }

    /**
     * 비밀번호 재설정 토큰 클리어
     */
    static clearPasswordResetToken() {
        return `
            UPDATE users SET 
                password_reset_token = NULL, 
                password_reset_expires = NULL, 
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    // ==========================================================================
    // 권한 관리 쿼리들
    // ==========================================================================

    /**
     * 사용자 권한 업데이트
     */
    static updatePermissions() {
        return `
            UPDATE users SET 
                permissions = ?, 
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    /**
     * 사용자 역할 업데이트
     */
    static updateRole() {
        return `
            UPDATE users SET 
                role = ?, 
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    /**
     * 사이트 접근 권한 업데이트
     */
    static updateSiteAccess() {
        return `
            UPDATE users SET 
                site_access = ?, 
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    /**
     * 디바이스 접근 권한 업데이트
     */
    static updateDeviceAccess() {
        return `
            UPDATE users SET 
                device_access = ?, 
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    /**
     * 활성화/비활성화 설정
     */
    static setActive() {
        return `
            UPDATE users SET 
                is_active = ?, 
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    // ==========================================================================
    // 유효성 검증 쿼리들
    // ==========================================================================

    /**
     * 사용자명 중복 확인
     */
    static isUsernameTaken() {
        return `
            SELECT COUNT(*) as count 
            FROM users 
            WHERE username = ?
        `;
    }

    /**
     * 사용자명 중복 확인 (특정 ID 제외)
     */
    static isUsernameTakenExcluding() {
        return `
            SELECT COUNT(*) as count 
            FROM users 
            WHERE username = ? AND id != ?
        `;
    }

    /**
     * 이메일 중복 확인
     */
    static isEmailTaken() {
        return `
            SELECT COUNT(*) as count 
            FROM users 
            WHERE email = ?
        `;
    }

    /**
     * 이메일 중복 확인 (특정 ID 제외)
     */
    static isEmailTakenExcluding() {
        return `
            SELECT COUNT(*) as count 
            FROM users 
            WHERE email = ? AND id != ?
        `;
    }

    // ==========================================================================
    // 통계 및 집계 쿼리들
    // ==========================================================================

    /**
     * 전체 사용자 수 조회
     */
    static getTotalCount() {
        return `
            SELECT COUNT(*) as count 
            FROM users
            WHERE is_deleted = 0
        `;
    }

    /**
     * 활성 사용자 수 조회
     */
    static getActiveCount() {
        return `
            SELECT COUNT(*) as count 
            FROM users 
            WHERE is_active = 1 AND is_deleted = 0
        `;
    }

    /**
     * 테넌트별 사용자 수 조회
     */
    static countByTenant() {
        return `
            SELECT COUNT(*) as count 
            FROM users 
            WHERE tenant_id = ? AND is_deleted = 0
        `;
    }

    /**
     * 역할별 사용자 통계
     */
    static getStatsByRole() {
        return `
            SELECT 
                role,
                COUNT(*) as total_count,
                SUM(CASE WHEN is_active = 1 THEN 1 ELSE 0 END) as active_count
            FROM users 
            WHERE is_deleted = 0
            GROUP BY role
            ORDER BY role
        `;
    }

    /**
     * 테넌트별 역할 통계
     */
    static getStatsByRoleAndTenant() {
        return `
            SELECT 
                role,
                COUNT(*) as total_count,
                SUM(CASE WHEN is_active = 1 THEN 1 ELSE 0 END) as active_count
            FROM users 
            WHERE tenant_id = ? AND is_deleted = 0
            GROUP BY role
            ORDER BY role
        `;
    }

    /**
     * 월별 가입 통계
     */
    static getMonthlySignupStats() {
        return `
            SELECT 
                strftime('%Y-%m', created_at) as month,
                COUNT(*) as signup_count,
                role
            FROM users 
            WHERE created_at >= datetime('now', '-12 months')
            GROUP BY strftime('%Y-%m', created_at), role
            ORDER BY month DESC, role
        `;
    }

    /**
     * 최근 로그인 통계
     */
    static getRecentLoginStats() {
        return `
            SELECT 
                COUNT(CASE WHEN last_login >= datetime('now', '-1 day') THEN 1 END) as today_logins,
                COUNT(CASE WHEN last_login >= datetime('now', '-7 days') THEN 1 END) as week_logins,
                COUNT(CASE WHEN last_login >= datetime('now', '-30 days') THEN 1 END) as month_logins,
                COUNT(CASE WHEN last_login IS NULL THEN 1 END) as never_logged_in
            FROM users 
            WHERE is_active = 1 AND is_deleted = 0
        `;
    }

    // ==========================================================================
    // 고급 검색 쿼리들
    // ==========================================================================

    /**
     * 복합 조건 검색
     */
    static searchUsers() {
        return `
            SELECT 
                id, tenant_id, username, email, full_name, department,
                role, is_active, last_login, created_at, updated_at
            FROM users 
            WHERE (? IS NULL OR username LIKE ?)
              AND (? IS NULL OR email LIKE ?)
              AND (? IS NULL OR full_name LIKE ?)
              AND (? IS NULL OR role = ?)
              AND (? IS NULL OR tenant_id = ?)
              AND (? IS NULL OR is_active = ?)
              AND is_deleted = 0
            ORDER BY username
        `;
    }

    /**
     * 최근 생성된 사용자들 조회
     */
    static findRecentUsers() {
        return `
            SELECT 
                id, username, email, full_name, role, 
                is_active, created_at
            FROM users 
            WHERE is_deleted = 0
            ORDER BY created_at DESC
            LIMIT ?
        `;
    }

    /**
     * 특정 권한을 가진 사용자들 조회
     */
    static findByPermission() {
        return `
            SELECT 
                id, tenant_id, username, email, full_name, 
                role, permissions, is_active
            FROM users 
            WHERE permissions LIKE ? AND is_active = 1 AND is_deleted = 0
            ORDER BY username
        `;
    }

    /**
     * 오랫동안 로그인하지 않은 사용자들 조회
     */
    static findInactiveUsers() {
        return `
            SELECT 
                id, username, email, full_name, role, 
                last_login, created_at
            FROM users 
            WHERE is_active = 1 AND is_deleted = 0
              AND (last_login IS NULL OR last_login < datetime('now', '-? days'))
            ORDER BY last_login ASC NULLS FIRST
        `;
    }

    // ==========================================================================
    // 관계형 데이터 조회
    // ==========================================================================

    /**
     * 사용자와 세션 정보 함께 조회
     */
    static findWithSessions() {
        return `
            SELECT 
                u.id, u.username, u.email, u.full_name, u.role, u.is_active,
                u.last_login, u.created_at,
                COUNT(s.id) as active_sessions,
                MAX(s.created_at) as latest_session
            FROM users u
            LEFT JOIN user_sessions s ON u.id = s.user_id AND s.expires_at > CURRENT_TIMESTAMP
            WHERE u.id = ?
            GROUP BY u.id, u.username, u.email, u.full_name, u.role, u.is_active,
                     u.last_login, u.created_at
        `;
    }

    /**
     * 사용자 통계 요약 조회
     */
    static getUserStatistics() {
        return `
            SELECT 
                COUNT(*) as total_users,
                SUM(CASE WHEN is_active = 1 AND is_deleted = 0 THEN 1 ELSE 0 END) as active_users,
                SUM(CASE WHEN is_deleted = 1 THEN 1 ELSE 0 END) as deleted_users,
                SUM(CASE WHEN role = 'company_admin' AND is_deleted = 0 THEN 1 ELSE 0 END) as admin_users,
                SUM(CASE WHEN last_login >= datetime('now', '-24 hours') AND is_deleted = 0 THEN 1 ELSE 0 END) as active_today
            FROM users
            WHERE tenant_id = ?
        `;
    }
}

module.exports = UserQueries;