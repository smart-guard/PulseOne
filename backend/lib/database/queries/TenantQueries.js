// =============================================================================
// backend/lib/database/queries/TenantQueries.js
// Tenant 관련 SQL 쿼리 모음 - 스키마와 완전 일치
// =============================================================================

/**
 * @class TenantQueries
 * @description Tenant 테이블 관련 모든 SQL 쿼리 정의
 * 
 * 🔥 실제 스키마 구조:
 * - id INTEGER PRIMARY KEY AUTOINCREMENT
 * - company_name VARCHAR(100) NOT NULL UNIQUE
 * - company_code VARCHAR(20) NOT NULL UNIQUE
 * - domain VARCHAR(100) UNIQUE
 * - contact_name VARCHAR(100)
 * - contact_email VARCHAR(100)
 * - contact_phone VARCHAR(20)
 * - subscription_plan VARCHAR(20) DEFAULT 'starter'
 * - subscription_status VARCHAR(20) DEFAULT 'active'
 * - max_edge_servers INTEGER DEFAULT 3
 * - max_data_points INTEGER DEFAULT 1000
 * - max_users INTEGER DEFAULT 5
 * - is_active INTEGER DEFAULT 1
 * - trial_end_date DATETIME
 * - created_at DATETIME DEFAULT CURRENT_TIMESTAMP
 * - updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
 */
class TenantQueries {
    // ==========================================================================
    // 기본 조회 쿼리들
    // ==========================================================================

    /**
     * 모든 테넌트 조회
     */
    static findAll() {
        return `
            SELECT 
                id, company_name, company_code, domain,
                contact_name, contact_email, contact_phone,
                subscription_plan, subscription_status,
                max_edge_servers, max_data_points, max_users,
                is_active, trial_end_date,
                created_at, updated_at
            FROM tenants 
            ORDER BY company_name
        `;
    }

    /**
     * ID로 테넌트 조회
     */
    static findById() {
        return `
            SELECT 
                id, company_name, company_code, domain,
                contact_name, contact_email, contact_phone,
                subscription_plan, subscription_status,
                max_edge_servers, max_data_points, max_users,
                is_active, trial_end_date,
                created_at, updated_at
            FROM tenants 
            WHERE id = ?
        `;
    }

    // ==========================================================================
    // 도메인/이름 기반 검색 쿼리들
    // ==========================================================================

    /**
     * 도메인으로 테넌트 조회
     */
    static findByDomain() {
        return `
            SELECT 
                id, company_name, company_code, domain,
                contact_name, contact_email, contact_phone,
                subscription_plan, subscription_status,
                max_edge_servers, max_data_points, max_users,
                is_active, trial_end_date,
                created_at, updated_at
            FROM tenants 
            WHERE domain = ?
        `;
    }

    /**
     * 회사명으로 테넌트 조회
     */
    static findByCompanyName() {
        return `
            SELECT 
                id, company_name, company_code, domain,
                contact_name, contact_email, contact_phone,
                subscription_plan, subscription_status,
                max_edge_servers, max_data_points, max_users,
                is_active, trial_end_date,
                created_at, updated_at
            FROM tenants 
            WHERE company_name = ?
        `;
    }

    /**
     * 회사 코드로 테넌트 조회
     */
    static findByCompanyCode() {
        return `
            SELECT 
                id, company_name, company_code, domain,
                contact_name, contact_email, contact_phone,
                subscription_plan, subscription_status,
                max_edge_servers, max_data_points, max_users,
                is_active, trial_end_date,
                created_at, updated_at
            FROM tenants 
            WHERE company_code = ?
        `;
    }

    // ==========================================================================
    // 상태별 조회 쿼리들
    // ==========================================================================

    /**
     * 구독 상태별 테넌트 조회
     */
    static findBySubscriptionStatus() {
        return `
            SELECT 
                id, company_name, company_code, domain,
                contact_name, contact_email, contact_phone,
                subscription_plan, subscription_status,
                max_edge_servers, max_data_points, max_users,
                is_active, trial_end_date,
                created_at, updated_at
            FROM tenants 
            WHERE subscription_status = ?
            ORDER BY company_name
        `;
    }

    /**
     * 활성 테넌트 조회
     */
    static findActiveTenants() {
        return `
            SELECT 
                id, company_name, company_code, domain,
                contact_name, contact_email, contact_phone,
                subscription_plan, subscription_status,
                max_edge_servers, max_data_points, max_users,
                is_active, trial_end_date,
                created_at, updated_at
            FROM tenants 
            WHERE is_active = 1 AND subscription_status = 'active'
            ORDER BY company_name
        `;
    }

    /**
     * 만료된 테넌트 조회
     */
    static findExpiredTenants() {
        return `
            SELECT 
                id, company_name, company_code, domain,
                contact_name, contact_email, contact_phone,
                subscription_plan, subscription_status,
                max_edge_servers, max_data_points, max_users,
                is_active, trial_end_date,
                created_at, updated_at
            FROM tenants 
            WHERE trial_end_date IS NOT NULL 
              AND trial_end_date < datetime('now')
            ORDER BY trial_end_date DESC
        `;
    }

    /**
     * 트라이얼 테넌트 조회
     */
    static findTrialTenants() {
        return `
            SELECT 
                id, company_name, company_code, domain,
                contact_name, contact_email, contact_phone,
                subscription_plan, subscription_status,
                max_edge_servers, max_data_points, max_users,
                is_active, trial_end_date,
                created_at, updated_at
            FROM tenants 
            WHERE subscription_status = 'trial'
              AND trial_end_date IS NOT NULL
              AND trial_end_date > datetime('now')
            ORDER BY trial_end_date ASC
        `;
    }

    /**
     * 비활성 테넌트 조회
     */
    static findInactiveTenants() {
        return `
            SELECT 
                id, company_name, company_code, domain,
                contact_name, contact_email, contact_phone,
                subscription_plan, subscription_status,
                max_edge_servers, max_data_points, max_users,
                is_active, trial_end_date,
                created_at, updated_at
            FROM tenants 
            WHERE is_active = 0 OR subscription_status IN ('suspended', 'inactive')
            ORDER BY updated_at DESC
        `;
    }

    // ==========================================================================
    // CRUD 연산 쿼리들
    // ==========================================================================

    /**
     * 새 테넌트 생성
     */
    static create() {
        return `
            INSERT INTO tenants (
                company_name, company_code, domain,
                contact_name, contact_email, contact_phone,
                subscription_plan, subscription_status,
                max_edge_servers, max_data_points, max_users,
                is_active, trial_end_date
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `;
    }

    /**
     * 테넌트 정보 수정
     */
    static update() {
        return `
            UPDATE tenants SET
                company_name = ?, company_code = ?, domain = ?,
                contact_name = ?, contact_email = ?, contact_phone = ?,
                subscription_plan = ?, subscription_status = ?,
                max_edge_servers = ?, max_data_points = ?, max_users = ?,
                is_active = ?, trial_end_date = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
        `;
    }

    /**
     * 테넌트 삭제
     */
    static delete() {
        return `DELETE FROM tenants WHERE id = ?`;
    }

    /**
     * 테넌트 비활성화 (소프트 삭제)
     */
    static deactivate() {
        return `
            UPDATE tenants SET 
                is_active = 0, 
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    // ==========================================================================
    // 구독 관리 쿼리들
    // ==========================================================================

    /**
     * 구독 상태 업데이트
     */
    static updateSubscriptionStatus() {
        return `
            UPDATE tenants SET 
                subscription_status = ?, 
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    /**
     * 구독 연장 (날짜 업데이트)
     */
    static extendSubscription() {
        return `
            UPDATE tenants SET 
                trial_end_date = datetime(COALESCE(trial_end_date, 'now'), '+' || ? || ' days'),
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    /**
     * 구독 플랜 변경
     */
    static updateSubscriptionPlan() {
        return `
            UPDATE tenants SET 
                subscription_plan = ?,
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    /**
     * 사용량 제한 업데이트
     */
    static updateLimits() {
        return `
            UPDATE tenants SET 
                max_edge_servers = ?,
                max_data_points = ?,
                max_users = ?,
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    /**
     * 활성화/비활성화 설정
     */
    static setActive() {
        return `
            UPDATE tenants SET 
                is_active = ?, 
                updated_at = CURRENT_TIMESTAMP 
            WHERE id = ?
        `;
    }

    // ==========================================================================
    // 유효성 검증 쿼리들
    // ==========================================================================

    /**
     * 도메인 중복 확인
     */
    static isDomainTaken() {
        return `
            SELECT COUNT(*) as count 
            FROM tenants 
            WHERE domain = ?
        `;
    }

    /**
     * 도메인 중복 확인 (특정 ID 제외)
     */
    static isDomainTakenExcluding() {
        return `
            SELECT COUNT(*) as count 
            FROM tenants 
            WHERE domain = ? AND id != ?
        `;
    }

    /**
     * 회사명 중복 확인
     */
    static isCompanyNameTaken() {
        return `
            SELECT COUNT(*) as count 
            FROM tenants 
            WHERE company_name = ?
        `;
    }

    /**
     * 회사명 중복 확인 (특정 ID 제외)
     */
    static isCompanyNameTakenExcluding() {
        return `
            SELECT COUNT(*) as count 
            FROM tenants 
            WHERE company_name = ? AND id != ?
        `;
    }

    /**
     * 회사 코드 중복 확인
     */
    static isCompanyCodeTaken() {
        return `
            SELECT COUNT(*) as count 
            FROM tenants 
            WHERE company_code = ?
        `;
    }

    /**
     * 회사 코드 중복 확인 (특정 ID 제외)
     */
    static isCompanyCodeTakenExcluding() {
        return `
            SELECT COUNT(*) as count 
            FROM tenants 
            WHERE company_code = ? AND id != ?
        `;
    }

    // ==========================================================================
    // 통계 및 집계 쿼리들
    // ==========================================================================

    /**
     * 전체 테넌트 수 조회
     */
    static getTotalCount() {
        return `
            SELECT COUNT(*) as count 
            FROM tenants
        `;
    }

    /**
     * 활성 테넌트 수 조회
     */
    static getActiveCount() {
        return `
            SELECT COUNT(*) as count 
            FROM tenants 
            WHERE is_active = 1
        `;
    }

    /**
     * 구독 상태별 통계
     */
    static getSubscriptionStatusStats() {
        return `
            SELECT 
                subscription_status,
                COUNT(*) as total_count,
                SUM(CASE WHEN is_active = 1 THEN 1 ELSE 0 END) as active_count
            FROM tenants 
            GROUP BY subscription_status
            ORDER BY subscription_status
        `;
    }

    /**
     * 구독 플랜별 통계
     */
    static getSubscriptionPlanStats() {
        return `
            SELECT 
                subscription_plan,
                COUNT(*) as total_count,
                SUM(CASE WHEN is_active = 1 THEN 1 ELSE 0 END) as active_count,
                AVG(max_data_points) as avg_data_points,
                AVG(max_users) as avg_users
            FROM tenants 
            GROUP BY subscription_plan
            ORDER BY subscription_plan
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
                subscription_plan
            FROM tenants 
            WHERE created_at >= datetime('now', '-12 months')
            GROUP BY strftime('%Y-%m', created_at), subscription_plan
            ORDER BY month DESC, subscription_plan
        `;
    }

    /**
     * 만료 예정 테넌트 통계
     */
    static getExpiringTenantsStats() {
        return `
            SELECT 
                CASE 
                    WHEN trial_end_date <= datetime('now', '+7 days') THEN '7_days'
                    WHEN trial_end_date <= datetime('now', '+30 days') THEN '30_days'
                    ELSE 'later'
                END as expiry_period,
                COUNT(*) as count
            FROM tenants 
            WHERE subscription_status = 'trial' 
              AND trial_end_date IS NOT NULL
              AND trial_end_date > datetime('now')
            GROUP BY expiry_period
        `;
    }

    // ==========================================================================
    // 고급 검색 쿼리들
    // ==========================================================================

    /**
     * 복합 조건 검색
     */
    static searchTenants() {
        return `
            SELECT 
                id, company_name, company_code, domain,
                contact_name, contact_email, contact_phone,
                subscription_plan, subscription_status,
                max_edge_servers, max_data_points, max_users,
                is_active, trial_end_date,
                created_at, updated_at
            FROM tenants 
            WHERE (? IS NULL OR company_name LIKE ?)
              AND (? IS NULL OR company_code LIKE ?)
              AND (? IS NULL OR subscription_status = ?)
              AND (? IS NULL OR subscription_plan = ?)
              AND (? IS NULL OR is_active = ?)
            ORDER BY company_name
        `;
    }

    /**
     * 최근 생성된 테넌트들 조회
     */
    static findRecentTenants() {
        return `
            SELECT 
                id, company_name, company_code, domain,
                subscription_plan, subscription_status,
                is_active, created_at
            FROM tenants 
            ORDER BY created_at DESC
            LIMIT ?
        `;
    }

    /**
     * 사용량이 많은 테넌트들 조회
     */
    static findHighUsageTenants() {
        return `
            SELECT 
                t.id, t.company_name, t.company_code,
                t.max_data_points, t.max_users, t.subscription_plan,
                COUNT(DISTINCT d.id) as device_count,
                COUNT(DISTINCT u.id) as user_count
            FROM tenants t
            LEFT JOIN devices d ON t.id = d.tenant_id
            LEFT JOIN users u ON t.id = u.tenant_id
            WHERE t.is_active = 1
            GROUP BY t.id, t.company_name, t.company_code, t.max_data_points, t.max_users, t.subscription_plan
            HAVING device_count > ? OR user_count > ?
            ORDER BY device_count DESC, user_count DESC
        `;
    }

    // ==========================================================================
    // 관계형 데이터 조회
    // ==========================================================================

    /**
     * 테넌트와 관련된 사용량 정보 포함 조회
     */
    static findWithUsageStats() {
        return `
            SELECT 
                t.id, t.company_name, t.company_code, t.subscription_plan,
                t.max_edge_servers, t.max_data_points, t.max_users,
                t.is_active, t.created_at,
                COUNT(DISTINCT es.id) as current_edge_servers,
                COUNT(DISTINCT d.id) as current_devices,
                COUNT(DISTINCT dp.id) as current_data_points,
                COUNT(DISTINCT u.id) as current_users
            FROM tenants t
            LEFT JOIN edge_servers es ON t.id = es.tenant_id
            LEFT JOIN devices d ON t.id = d.tenant_id
            LEFT JOIN data_points dp ON d.id = dp.device_id
            LEFT JOIN users u ON t.id = u.tenant_id
            WHERE t.id = ?
            GROUP BY t.id, t.company_name, t.company_code, t.subscription_plan, 
                     t.max_edge_servers, t.max_data_points, t.max_users, 
                     t.is_active, t.created_at
        `;
    }
}

module.exports = TenantQueries;