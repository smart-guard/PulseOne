// =============================================================================
// backend/lib/database/queries/ProtocolQueries.js
// DeviceQueries.js 패턴 100% 따라한 프로토콜 쿼리 정의
// =============================================================================

class ProtocolQueries {
    
    // =========================================================================
    // 기본 조회 쿼리들 - DeviceQueries 패턴과 동일
    // =========================================================================
    
    /**
     * 프로토콜 전체 목록 조회 (디바이스 통계 포함)
     */
    static getProtocolsWithDeviceStats() {
        return `
            SELECT 
                p.*,
                COALESCE(device_counts.device_count, 0) as device_count,
                COALESCE(device_counts.enabled_count, 0) as enabled_count,
                COALESCE(device_counts.connected_count, 0) as connected_count
            FROM protocols p
            LEFT JOIN (
                SELECT 
                    d.protocol_id,
                    COUNT(*) as device_count,
                    COUNT(CASE WHEN d.is_enabled = 1 THEN 1 END) as enabled_count,
                    COUNT(CASE WHEN ds.connection_status = 'connected' THEN 1 END) as connected_count
                FROM devices d
                LEFT JOIN device_status ds ON ds.device_id = d.id
                WHERE d.tenant_id = ?
                GROUP BY d.protocol_id
            ) device_counts ON device_counts.protocol_id = p.id
        `;
    }

    /**
     * protocol_type으로 프로토콜 조회
     */
    static findByType() {
        return 'SELECT * FROM protocols WHERE protocol_type = ?';
    }

    /**
     * ID로 프로토콜 조회
     */
    static findById() {
        return 'SELECT * FROM protocols WHERE id = ?';
    }

    /**
     * 카테고리별 프로토콜 조회
     */
    static findByCategory() {
        return 'SELECT * FROM protocols WHERE category = ? ORDER BY display_name ASC';
    }

    /**
     * 활성화된 프로토콜만 조회
     */
    static findActive() {
        return 'SELECT * FROM protocols WHERE is_enabled = 1 ORDER BY display_name ASC';
    }

    /**
     * 전체 프로토콜 조회 (기본)
     */
    static findAll() {
        return 'SELECT * FROM protocols ORDER BY display_name ASC';
    }

    // =========================================================================
    // CRUD 쿼리들 - DeviceQueries 패턴과 동일
    // =========================================================================

    /**
     * 새 프로토콜 생성
     */
    static createProtocol() {
        return `
            INSERT INTO protocols (
                protocol_type, display_name, description, default_port, uses_serial,
                requires_broker, supported_operations, supported_data_types, 
                connection_params, default_polling_interval, default_timeout,
                max_concurrent_connections, category, vendor, standard_reference, 
                is_enabled, is_deprecated, min_firmware_version,
                created_at, updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `;
    }

    /**
     * 프로토콜 정보 수정 (동적 업데이트)
     */
    static updateProtocol() {
        return `
            UPDATE protocols SET 
                display_name = ?, description = ?, default_port = ?,
                default_polling_interval = ?, default_timeout = ?,
                max_concurrent_connections = ?, category = ?, vendor = ?,
                standard_reference = ?, is_enabled = ?, is_deprecated = ?,
                min_firmware_version = ?, updated_at = ?
            WHERE id = ?
        `;
    }

    /**
     * 프로토콜 활성화
     */
    static enableProtocol() {
        return 'UPDATE protocols SET is_enabled = 1, updated_at = ? WHERE id = ?';
    }

    /**
     * 프로토콜 비활성화
     */
    static disableProtocol() {
        return 'UPDATE protocols SET is_enabled = 0, updated_at = ? WHERE id = ?';
    }

    /**
     * 프로토콜 삭제
     */
    static deleteProtocol() {
        return 'DELETE FROM protocols WHERE id = ?';
    }

    // =========================================================================
    // 통계 및 분석 쿼리들 - DeviceQueries 패턴과 동일
    // =========================================================================

    /**
     * 전체 프로토콜 수 조회
     */
    static countTotal() {
        return 'SELECT COUNT(*) as count FROM protocols';
    }

    /**
     * 활성화된 프로토콜 수 조회
     */
    static countEnabled() {
        return 'SELECT COUNT(*) as count FROM protocols WHERE is_enabled = 1';
    }

    /**
     * 사용 중단 예정 프로토콜 수 조회
     */
    static countDeprecated() {
        return 'SELECT COUNT(*) as count FROM protocols WHERE is_deprecated = 1';
    }

    /**
     * 카테고리별 프로토콜 통계
     */
    static getStatsByCategory() {
        return `
            SELECT 
                category,
                COUNT(*) as count,
                ROUND(COUNT(*) * 100.0 / (SELECT COUNT(*) FROM protocols), 2) as percentage
            FROM protocols 
            GROUP BY category 
            ORDER BY count DESC
        `;
    }

    /**
     * 프로토콜별 디바이스 사용 통계 (테넌트별)
     */
    static getUsageStatsByTenant() {
        return `
            SELECT 
                p.id,
                p.protocol_type,
                p.display_name,
                p.category,
                COUNT(d.id) as device_count,
                COUNT(CASE WHEN d.is_enabled = 1 THEN 1 END) as enabled_devices,
                COUNT(CASE WHEN ds.connection_status = 'connected' THEN 1 END) as connected_devices
            FROM protocols p
            LEFT JOIN devices d ON d.protocol_id = p.id AND d.tenant_id = ?
            LEFT JOIN device_status ds ON ds.device_id = d.id
            GROUP BY p.id, p.protocol_type, p.display_name, p.category
            ORDER BY device_count DESC
        `;
    }

    /**
     * 프로토콜 사용량 TOP N
     */
    static getTopUsedProtocols() {
        return `
            SELECT 
                p.protocol_type,
                p.display_name,
                COUNT(d.id) as device_count
            FROM protocols p
            LEFT JOIN devices d ON d.protocol_id = p.id
            WHERE p.is_enabled = 1
            GROUP BY p.protocol_type, p.display_name
            ORDER BY device_count DESC
            LIMIT ?
        `;
    }

    // =========================================================================
    // 검증 및 관계 쿼리들 - DeviceQueries 패턴과 동일
    // =========================================================================

    /**
     * 프로토콜을 사용하는 디바이스 수 조회
     */
    static countDevicesByProtocol() {
        return 'SELECT COUNT(*) as count FROM devices WHERE protocol_id = ?';
    }

    /**
     * 프로토콜을 사용하는 디바이스 목록 (페이징)
     */
    static getDevicesByProtocol() {
        return `
            SELECT d.id, d.name, d.device_type, d.is_enabled,
                   ds.connection_status, ds.last_communication
            FROM devices d
            LEFT JOIN device_status ds ON ds.device_id = d.id
            WHERE d.protocol_id = ?
            ORDER BY d.name
            LIMIT ? OFFSET ?
        `;
    }

    /**
     * 프로토콜 타입 중복 확인
     */
    static checkProtocolTypeExists() {
        return 'SELECT COUNT(*) as count FROM protocols WHERE protocol_type = ? AND id != ?';
    }

    // =========================================================================
    // 고급 검색 및 필터 쿼리들 - DeviceQueries 패턴과 동일
    // =========================================================================

    /**
     * 텍스트 검색 (이름, 설명, 카테고리)
     */
    static searchProtocols() {
        return `
            SELECT * FROM protocols 
            WHERE (display_name LIKE ? OR protocol_type LIKE ? 
                   OR description LIKE ? OR category LIKE ?)
            ORDER BY display_name ASC
        `;
    }

    /**
     * 고급 필터 조건 (동적 쿼리 구성용)
     */
    static addCategoryFilter() {
        return ' AND p.category = ?';
    }

    static addEnabledFilter() {
        return ' AND p.is_enabled = ?';
    }

    static addDeprecatedFilter() {
        return ' AND p.is_deprecated = ?';
    }

    static addPortFilter() {
        return ' AND p.default_port = ?';
    }

    static addSerialFilter() {
        return ' AND p.uses_serial = ?';
    }

    static addBrokerFilter() {
        return ' AND p.requires_broker = ?';
    }

    /**
     * 정렬 옵션
     */
    static addSortBy(sortBy = 'display_name', sortOrder = 'ASC') {
        const validSortColumns = ['id', 'protocol_type', 'display_name', 'category', 'created_at', 'updated_at'];
        const validOrders = ['ASC', 'DESC'];
        
        const column = validSortColumns.includes(sortBy) ? sortBy : 'display_name';
        const order = validOrders.includes(sortOrder.toUpperCase()) ? sortOrder.toUpperCase() : 'ASC';
        
        return ` ORDER BY p.${column} ${order}`;
    }

    /**
     * 페이징 지원
     */
    static addLimit() {
        return ' LIMIT ?';
    }

    static addOffset() {
        return ' OFFSET ?';
    }

    // =========================================================================
    // 현재 시간 헬퍼 (DB별 대응) - DeviceQueries 패턴과 동일
    // =========================================================================
    
    /**
     * DB 타입별 현재 시간 함수
     */
    static getCurrentTimestamp(dbType = 'sqlite') {
        switch (dbType.toLowerCase()) {
        case 'postgresql':
        case 'postgres':
            return 'NOW()';
        case 'sqlite':
        case 'sqlite3':
            return 'datetime(\'now\')';
        case 'mysql':
        case 'mariadb':
            return 'NOW()';
        case 'mssql':
        case 'sqlserver':
            return 'GETDATE()';
        default:
            return 'datetime(\'now\')'; // SQLite 기본값
        }
    }
}

module.exports = ProtocolQueries;