// ============================================================================
// backend/lib/database/queries/DeviceQueries.js
// 디바이스 관련 SQL 쿼리 모음 (C++ SQLQueries.h 패턴)
// ============================================================================

/**
 * 디바이스 관련 SQL 쿼리 상수들
 * C++의 SQL::Device:: 네임스페이스와 동일한 구조
 */
const DeviceQueries = {
    
    // =========================================================================
    // 🔥 기본 CRUD 쿼리들
    // =========================================================================
    
    /**
     * 모든 디바이스 조회 (JOIN 포함)
     */
    FIND_ALL: `
        SELECT 
            d.*,
            s.name as site_name,
            s.location as site_location,
            t.company_name as tenant_name,
            (SELECT COUNT(*) FROM data_points dp WHERE dp.device_id = d.id) as data_point_count
        FROM devices d
        LEFT JOIN sites s ON d.site_id = s.id
        LEFT JOIN tenants t ON d.tenant_id = t.id
        ORDER BY d.name
    `,

    /**
     * ID로 디바이스 조회
     */
    FIND_BY_ID: `
        SELECT 
            d.*,
            s.name as site_name,
            s.location as site_location,
            t.company_name as tenant_name,
            (SELECT COUNT(*) FROM data_points dp WHERE dp.device_id = d.id) as data_point_count
        FROM devices d
        LEFT JOIN sites s ON d.site_id = s.id
        LEFT JOIN tenants t ON d.tenant_id = t.id
        WHERE d.id = ?
    `,

    /**
     * 테넌트별 디바이스 조회
     */
    FIND_BY_TENANT: `
        SELECT 
            d.*,
            s.name as site_name,
            s.location as site_location,
            (SELECT COUNT(*) FROM data_points dp WHERE dp.device_id = d.id) as data_point_count
        FROM devices d
        LEFT JOIN sites s ON d.site_id = s.id
        WHERE d.tenant_id = ?
        ORDER BY d.name
    `,

    /**
     * 사이트별 디바이스 조회
     */
    FIND_BY_SITE: `
        SELECT 
            d.*,
            s.name as site_name,
            s.location as site_location,
            t.company_name as tenant_name,
            (SELECT COUNT(*) FROM data_points dp WHERE dp.device_id = d.id) as data_point_count
        FROM devices d
        LEFT JOIN sites s ON d.site_id = s.id
        LEFT JOIN tenants t ON d.tenant_id = t.id
        WHERE d.site_id = ?
        ORDER BY d.name
    `,

    /**
     * 프로토콜별 디바이스 조회
     */
    FIND_BY_PROTOCOL: `
        SELECT 
            d.*,
            s.name as site_name,
            s.location as site_location,
            t.company_name as tenant_name,
            (SELECT COUNT(*) FROM data_points dp WHERE dp.device_id = d.id) as data_point_count
        FROM devices d
        LEFT JOIN sites s ON d.site_id = s.id
        LEFT JOIN tenants t ON d.tenant_id = t.id
        WHERE d.protocol = ?
        ORDER BY d.name
    `,

    /**
     * 활성화된 디바이스만 조회
     */
    FIND_ENABLED: `
        SELECT 
            d.*,
            s.name as site_name,
            s.location as site_location,
            t.company_name as tenant_name,
            (SELECT COUNT(*) FROM data_points dp WHERE dp.device_id = d.id) as data_point_count
        FROM devices d
        LEFT JOIN sites s ON d.site_id = s.id
        LEFT JOIN tenants t ON d.tenant_id = t.id
        WHERE d.is_enabled = 1
        ORDER BY d.name
    `,

    /**
     * 연결된 디바이스만 조회
     */
    FIND_CONNECTED: `
        SELECT 
            d.*,
            s.name as site_name,
            s.location as site_location,
            t.company_name as tenant_name,
            (SELECT COUNT(*) FROM data_points dp WHERE dp.device_id = d.id) as data_point_count
        FROM devices d
        LEFT JOIN sites s ON d.site_id = s.id
        LEFT JOIN tenants t ON d.tenant_id = t.id
        WHERE d.is_connected = 1
        ORDER BY d.name
    `,

    /**
     * 디바이스 존재 확인
     */
    EXISTS_BY_ID: `
        SELECT 1 FROM devices WHERE id = ?
    `,

    /**
     * 디바이스 삭제
     */
    DELETE_BY_ID: `
        DELETE FROM devices WHERE id = ?
    `,

    // =========================================================================
    // 🔥 카운트 및 통계 쿼리들
    // =========================================================================

    /**
     * 전체 디바이스 수
     */
    COUNT_ALL: `
        SELECT COUNT(*) as total FROM devices
    `,

    /**
     * 테넌트별 디바이스 수
     */
    COUNT_BY_TENANT: `
        SELECT COUNT(*) as total FROM devices WHERE tenant_id = ?
    `,

    /**
     * 사이트별 디바이스 수
     */
    COUNT_BY_SITE: `
        SELECT COUNT(*) as total FROM devices WHERE site_id = ?
    `,

    /**
     * 프로토콜별 디바이스 수
     */
    COUNT_BY_PROTOCOL: `
        SELECT COUNT(*) as total FROM devices WHERE protocol = ?
    `,

    /**
     * 활성화된 디바이스 수
     */
    COUNT_ENABLED: `
        SELECT COUNT(*) as total FROM devices WHERE is_enabled = 1
    `,

    /**
     * 연결된 디바이스 수
     */
    COUNT_CONNECTED: `
        SELECT COUNT(*) as total FROM devices WHERE is_connected = 1
    `,

    /**
     * 프로토콜별 분포
     */
    GET_PROTOCOL_DISTRIBUTION: `
        SELECT 
            protocol,
            COUNT(*) as count,
            ROUND(COUNT(*) * 100.0 / (SELECT COUNT(*) FROM devices), 2) as percentage
        FROM devices 
        GROUP BY protocol
        ORDER BY count DESC
    `,

    /**
     * 상태별 분포
     */
    GET_STATUS_DISTRIBUTION: `
        SELECT 
            CASE 
                WHEN is_enabled = 1 AND is_connected = 1 THEN 'running'
                WHEN is_enabled = 1 AND is_connected = 0 THEN 'stopped'
                WHEN is_enabled = 0 THEN 'disabled'
                ELSE 'unknown'
            END as status,
            COUNT(*) as count
        FROM devices 
        GROUP BY status
        ORDER BY count DESC
    `,

    // =========================================================================
    // 🔥 필터링용 동적 쿼리 베이스들
    // =========================================================================

    /**
     * 복합 필터링용 베이스 쿼리 (WHERE 조건 없음)
     */
    FIND_WITH_FILTERS_BASE: `
        SELECT 
            d.*,
            s.name as site_name,
            s.location as site_location,
            t.company_name as tenant_name,
            (SELECT COUNT(*) FROM data_points dp WHERE dp.device_id = d.id) as data_point_count
        FROM devices d
        LEFT JOIN sites s ON d.site_id = s.id
        LEFT JOIN tenants t ON d.tenant_id = t.id
        WHERE 1=1
    `,

    /**
     * 카운트용 베이스 쿼리 (WHERE 조건 없음)
     */
    COUNT_WITH_FILTERS_BASE: `
        SELECT COUNT(*) as total 
        FROM devices d 
        WHERE 1=1
    `,

    // =========================================================================
    // 🔥 WHERE 조건 템플릿들 (동적 쿼리 빌딩용)
    // =========================================================================

    CONDITIONS: {
        TENANT_ID: ` AND d.tenant_id = ?`,
        SITE_ID: ` AND d.site_id = ?`,
        DEVICE_TYPE: ` AND d.device_type = ?`,
        PROTOCOL: ` AND d.protocol = ?`,
        MANUFACTURER: ` AND d.manufacturer = ?`,
        IS_ENABLED: ` AND d.is_enabled = ?`,
        IS_CONNECTED: ` AND d.is_connected = ?`,
        SEARCH_NAME: ` AND d.name LIKE ?`,
        SEARCH_DESCRIPTION: ` AND d.description LIKE ?`,
        SEARCH_MANUFACTURER: ` AND d.manufacturer LIKE ?`,
        SEARCH_MODEL: ` AND d.model LIKE ?`,
        SEARCH_ALL: ` AND (d.name LIKE ? OR d.description LIKE ? OR d.manufacturer LIKE ? OR d.model LIKE ?)`,
        DATE_RANGE: ` AND d.created_at BETWEEN ? AND ?`,
        UPDATED_SINCE: ` AND d.updated_at >= ?`
    },

    // =========================================================================
    // 🔥 ORDER BY 템플릿들
    // =========================================================================

    ORDER_BY: {
        NAME_ASC: ` ORDER BY d.name ASC`,
        NAME_DESC: ` ORDER BY d.name DESC`,
        CREATED_ASC: ` ORDER BY d.created_at ASC`,
        CREATED_DESC: ` ORDER BY d.created_at DESC`,
        UPDATED_ASC: ` ORDER BY d.updated_at ASC`,
        UPDATED_DESC: ` ORDER BY d.updated_at DESC`,
        PROTOCOL_ASC: ` ORDER BY d.protocol ASC, d.name ASC`,
        SITE_ASC: ` ORDER BY s.name ASC, d.name ASC`
    },

    // =========================================================================
    // 🔥 LIMIT/OFFSET 템플릿들 (DB별)
    // =========================================================================

    PAGINATION: {
        SQLITE: ` LIMIT ? OFFSET ?`,
        POSTGRES: ` LIMIT ? OFFSET ?`,
        MYSQL: ` LIMIT ?, ?`,
        MSSQL: ` OFFSET ? ROWS FETCH NEXT ? ROWS ONLY`
    }
};

module.exports = DeviceQueries;