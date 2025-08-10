// =============================================================================
// collector/include/Database/SQLQueries.h
// 🎯 SQL 쿼리 상수 중앙 관리 - 모든 쿼리를 한 곳에서 관리 (완전판)
// =============================================================================

#ifndef SQL_QUERIES_H
#define SQL_QUERIES_H

#include <string>

namespace PulseOne {
namespace Database {
namespace SQL {


    // =============================================================================
// 🎯 기타 공통 쿼리들
// =============================================================================
namespace Common {
    
    // 🔥 CHECK_TABLE_EXISTS - 기존 패턴과 일치 (이름 반환)
    const std::string CHECK_TABLE_EXISTS = R"(
        SELECT name FROM sqlite_master 
        WHERE type='table' AND name = ?
    )";
    
    const std::string GET_TABLE_INFO = "PRAGMA table_info(?)";
    
    const std::string GET_FOREIGN_KEYS = "PRAGMA foreign_key_list(?)";
    
    const std::string ENABLE_FOREIGN_KEYS = "PRAGMA foreign_keys = ON";
    
    const std::string DISABLE_FOREIGN_KEYS = "PRAGMA foreign_keys = OFF";
    
    const std::string BEGIN_TRANSACTION = "BEGIN TRANSACTION";
    
    const std::string COMMIT_TRANSACTION = "COMMIT";
    
    const std::string ROLLBACK_TRANSACTION = "ROLLBACK";
    
    // 🔥 마지막 삽입 ID 조회 (SQLite)
    const std::string GET_LAST_INSERT_ID = "SELECT last_insert_rowid() as id";
    
    // 🔥 PostgreSQL용 (필요시 사용)
    const std::string GET_LAST_INSERT_ID_POSTGRES = "SELECT lastval() as id";
    
    // 🔥 MySQL용 (필요시 사용)  
    const std::string GET_LAST_INSERT_ID_MYSQL = "SELECT LAST_INSERT_ID() as id";
    
    // 🔥 현재 시간 조회
    const std::string GET_CURRENT_TIMESTAMP = "SELECT datetime('now') as timestamp";
    
    // 🔥 데이터베이스 정보 조회
    const std::string GET_DATABASE_VERSION = "SELECT sqlite_version() as version";
    
    // 🔥🔥🔥 별도의 카운트 전용 쿼리 (필요한 경우)
    const std::string COUNT_TABLES = R"(
        SELECT COUNT(*) as count 
        FROM sqlite_master 
        WHERE type='table' AND name=?
    )";
    
} // namespace Common


// =============================================================================
// 🎯 DeviceRepository 쿼리들 (완전판)
// =============================================================================
namespace Device {
    
    const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        ORDER BY id
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        WHERE id = ?
    )";
    
    const std::string FIND_BY_PROTOCOL = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        WHERE protocol_type = ? AND is_enabled = 1
        ORDER BY name
    )";
    
    const std::string FIND_BY_TENANT = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        WHERE tenant_id = ?
        ORDER BY name
    )";
    
    const std::string FIND_BY_SITE = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        WHERE site_id = ?
        ORDER BY name
    )";
    
    const std::string FIND_ENABLED = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        WHERE is_enabled = 1
        ORDER BY protocol_type, name
    )";
    
    const std::string FIND_DISABLED = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        WHERE is_enabled = 0
        ORDER BY updated_at DESC
    )";
    
    const std::string INSERT = R"(
        INSERT INTO devices (
            tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE = R"(
        UPDATE devices SET 
            tenant_id = ?, site_id = ?, device_group_id = ?, edge_server_id = ?,
            name = ?, description = ?, device_type = ?, manufacturer = ?, model = ?, 
            serial_number = ?, protocol_type = ?, endpoint = ?, config = ?, 
            is_enabled = ?, installation_date = ?, last_maintenance = ?, 
            updated_at = ?
        WHERE id = ?
    )";
    
    const std::string UPDATE_STATUS = R"(
        UPDATE devices 
        SET is_enabled = ?, updated_at = ?
        WHERE id = ?
    )";
    
    const std::string UPDATE_ENDPOINT = R"(
        UPDATE devices 
        SET endpoint = ?, updated_at = ?
        WHERE id = ?
    )";
    
    const std::string UPDATE_CONFIG = R"(
        UPDATE devices 
        SET config = ?, updated_at = ?
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM devices WHERE id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM devices WHERE id = ?";
    
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM devices";
    
    const std::string COUNT_ENABLED = "SELECT COUNT(*) as count FROM devices WHERE is_enabled = 1";
    
    const std::string COUNT_BY_PROTOCOL = "SELECT COUNT(*) as count FROM devices WHERE protocol_type = ?";
    
    const std::string GET_PROTOCOL_DISTRIBUTION = R"(
        SELECT protocol_type, COUNT(*) as count 
        FROM devices 
        GROUP BY protocol_type
        ORDER BY count DESC
    )";
    
    const std::string GET_LAST_INSERT_ID = "SELECT last_insert_rowid() as id";
    
    // 테이블 생성
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS devices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER NOT NULL,
            site_id INTEGER NOT NULL,
            device_group_id INTEGER,
            edge_server_id INTEGER,
            
            -- 디바이스 기본 정보
            name VARCHAR(100) NOT NULL,
            description TEXT,
            device_type VARCHAR(50) NOT NULL,
            manufacturer VARCHAR(100),
            model VARCHAR(100),
            serial_number VARCHAR(100),
            
            -- 통신 설정
            protocol_type VARCHAR(50) NOT NULL,
            endpoint VARCHAR(255) NOT NULL,
            config TEXT NOT NULL,
            
            -- 상태 정보
            is_enabled INTEGER DEFAULT 1,
            installation_date DATE,
            last_maintenance DATE,
            
            created_by INTEGER,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
} // namespace Device

// =============================================================================
// 🎯 DataPointRepository 쿼리들
// =============================================================================
namespace DataPoint {
    
    // 🔥🔥🔥 FIND_ALL - 모든 필드 포함 (Struct DataPoint 완전 일치)
    const std::string FIND_ALL = R"(
        SELECT 
            -- 기본 식별 정보
            id, device_id, name, description, 
            
            -- 주소 정보  
            address, address_string,
            
            -- 데이터 타입 및 접근성
            data_type, access_mode, is_enabled, is_writable,
            
            -- 엔지니어링 단위 및 스케일링
            unit, scaling_factor, scaling_offset, min_value, max_value,
            
            -- 🔥 로깅 및 수집 설정 (SQLQueries.h가 찾던 컬럼들!)
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            
            -- 🔥 메타데이터 (SQLQueries.h가 찾던 컬럼들!)
            group_name, tags, metadata, protocol_params,
            
            -- 시간 정보
            created_at, updated_at
        FROM data_points 
        ORDER BY device_id, address
    )";
    
    // 🔥🔥🔥 FIND_BY_ID - 모든 필드 포함
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, device_id, name, description, 
            address, address_string,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        FROM data_points 
        WHERE id = ?
    )";
    
    // 🔥 FIND_BY_DEVICE_ID - 디바이스별 조회
    const std::string FIND_BY_DEVICE_ID = R"(
        SELECT 
            id, device_id, name, description, 
            address, address_string,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        FROM data_points 
        WHERE device_id = ?
        ORDER BY address
    )";
    
    // 🔥 FIND_BY_DEVICE_ID_ENABLED - 활성화된 포인트만
    const std::string FIND_BY_DEVICE_ID_ENABLED = R"(
        SELECT 
            id, device_id, name, description, 
            address, address_string,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        FROM data_points 
        WHERE device_id = ? AND is_enabled = 1
        ORDER BY address
    )";
    
    // 🔥 FIND_BY_DEVICE_AND_ADDRESS - 유니크 검색
    const std::string FIND_BY_DEVICE_AND_ADDRESS = R"(
        SELECT 
            id, device_id, name, description, 
            address, address_string,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        FROM data_points 
        WHERE device_id = ? AND address = ?
        ORDER BY created_at DESC
        LIMIT 1
    )";
    
    // 🔥🔥🔥 INSERT - 모든 필드 삽입
    const std::string INSERT = R"(
        INSERT INTO data_points (
            device_id, name, description, 
            address, address_string,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    // 🔥🔥🔥 UPDATE - 모든 필드 업데이트  
    const std::string UPDATE = R"(
        UPDATE data_points SET 
            device_id = ?, name = ?, description = ?, 
            address = ?, address_string = ?,
            data_type = ?, access_mode = ?, is_enabled = ?, is_writable = ?,
            unit = ?, scaling_factor = ?, scaling_offset = ?, min_value = ?, max_value = ?,
            log_enabled = ?, log_interval_ms = ?, log_deadband = ?, polling_interval_ms = ?,
            group_name = ?, tags = ?, metadata = ?, protocol_params = ?,
            updated_at = ?
        WHERE id = ?
    )";
    
    // 기본 CRUD 작업
    const std::string DELETE_BY_ID = "DELETE FROM data_points WHERE id = ?";
    const std::string DELETE_BY_DEVICE_ID = "DELETE FROM data_points WHERE device_id = ?";
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM data_points WHERE id = ?";
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM data_points";
    const std::string COUNT_BY_DEVICE = "SELECT COUNT(*) as count FROM data_points WHERE device_id = ?";
    
    // 통계 쿼리들
    const std::string GET_COUNT_BY_DEVICE = R"(
        SELECT device_id, COUNT(*) as count 
        FROM data_points 
        GROUP BY device_id
        ORDER BY device_id
    )";
    
    const std::string GET_COUNT_BY_DATA_TYPE = R"(
        SELECT data_type, COUNT(*) as count 
        FROM data_points 
        GROUP BY data_type
        ORDER BY count DESC
    )";
    
    const std::string GET_LAST_INSERT_ID = "SELECT last_insert_rowid() as id";
    
    // 🔥🔥🔥 CREATE_TABLE - Struct DataPoint 완전 반영
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS data_points (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id INTEGER NOT NULL,
            
            -- 🔥 기본 식별 정보 (Struct DataPoint와 일치)
            name VARCHAR(100) NOT NULL,
            description TEXT,
            
            -- 🔥 주소 정보 (Struct DataPoint와 일치)
            address INTEGER NOT NULL,                    -- uint32_t address
            address_string VARCHAR(255),                 -- std::string address_string
            
            -- 🔥 데이터 타입 및 접근성 (Struct DataPoint와 일치)
            data_type VARCHAR(20) NOT NULL DEFAULT 'UNKNOWN',  -- std::string data_type
            access_mode VARCHAR(10) DEFAULT 'read',             -- std::string access_mode
            is_enabled INTEGER DEFAULT 1,                       -- bool is_enabled
            is_writable INTEGER DEFAULT 0,                      -- bool is_writable
            
            -- 🔥 엔지니어링 단위 및 스케일링 (Struct DataPoint와 일치)
            unit VARCHAR(50),                            -- std::string unit
            scaling_factor REAL DEFAULT 1.0,            -- double scaling_factor
            scaling_offset REAL DEFAULT 0.0,            -- double scaling_offset
            min_value REAL DEFAULT 0.0,                 -- double min_value
            max_value REAL DEFAULT 0.0,                 -- double max_value
            
            -- 🔥🔥🔥 로깅 및 수집 설정 (중요! 이전에 없던 컬럼들)
            log_enabled INTEGER DEFAULT 1,              -- bool log_enabled ✅
            log_interval_ms INTEGER DEFAULT 0,          -- uint32_t log_interval_ms ✅
            log_deadband REAL DEFAULT 0.0,              -- double log_deadband ✅
            polling_interval_ms INTEGER DEFAULT 0,      -- uint32_t polling_interval_ms
            
            -- 🔥🔥🔥 메타데이터 (중요! 이전에 없던 컬럼들)
            group_name VARCHAR(50),                      -- std::string group
            tags TEXT,                                   -- std::string tags (JSON 배열) ✅
            metadata TEXT,                               -- std::string metadata (JSON 객체) ✅
            protocol_params TEXT,                        -- map<string,string> protocol_params (JSON)
            
            -- 🔥 시간 정보
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
            UNIQUE(device_id, address)
        )
    )";
    
    // 🔥🔥🔥 실시간 값 조회 (current_values 테이블과 JOIN)
    const std::string FIND_WITH_CURRENT_VALUES = R"(
        SELECT 
            -- data_points 모든 필드
            dp.id, dp.device_id, dp.name, dp.description, 
            dp.address, dp.address_string,
            dp.data_type, dp.access_mode, dp.is_enabled, dp.is_writable,
            dp.unit, dp.scaling_factor, dp.scaling_offset, dp.min_value, dp.max_value,
            dp.log_enabled, dp.log_interval_ms, dp.log_deadband, dp.polling_interval_ms,
            dp.group_name, dp.tags, dp.metadata, dp.protocol_params,
            dp.created_at, dp.updated_at,
            
            -- current_values 테이블의 실시간 데이터
            cv.current_value_bool, cv.current_value_int16, cv.current_value_uint16,
            cv.current_value_int32, cv.current_value_uint32, cv.current_value_float,
            cv.current_value_double, cv.current_value_string, cv.active_value_type,
            cv.raw_value_float, cv.active_raw_type,
            cv.quality_code, cv.quality,
            cv.value_timestamp, cv.quality_timestamp, cv.last_log_time,
            cv.last_read_time, cv.last_write_time,
            cv.read_count, cv.write_count, cv.error_count
        FROM data_points dp
        LEFT JOIN current_values cv ON dp.id = cv.point_id
        WHERE dp.device_id = ?
        ORDER BY dp.address
    )";
    
    // 🔥 로깅 활성화된 포인트들만 조회
    const std::string FIND_LOG_ENABLED = R"(
        SELECT 
            id, device_id, name, address, data_type,
            log_enabled, log_interval_ms, log_deadband,
            unit, scaling_factor, scaling_offset
        FROM data_points 
        WHERE log_enabled = 1 AND is_enabled = 1
        ORDER BY device_id, address
    )";

    // 🔥🔥🔥 누락된 쿼리 1: FIND_BY_TAG (findByTag 메서드 사용)
    const std::string FIND_BY_TAG = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE tags LIKE ?
        ORDER BY device_id, address
    )";
    
    // 🔥🔥🔥 누락된 쿼리 2: FIND_DISABLED (findDisabledPoints 메서드 사용)
    const std::string FIND_DISABLED = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE is_enabled = 0
        ORDER BY device_id, address
    )";
    
    // 🔥🔥🔥 누락된 쿼리 3: DELETE_BY_DEVICE_IDS (deleteByDeviceIds 메서드 사용)
    const std::string DELETE_BY_DEVICE_IDS = R"(
        DELETE FROM data_points 
        WHERE device_id IN (%s)
    )"; // %s는 런타임에 IN 절로 대체됨
    
    // 🔥🔥🔥 누락된 쿼리 4: BULK_INSERT (saveBulk 메서드 사용)
    const std::string BULK_INSERT = R"(
        INSERT INTO data_points (
            device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        ) VALUES %s
    )"; // %s는 런타임에 VALUES 절들로 대체됨
    
    // 🔥🔥🔥 누락된 쿼리 5: UPSERT (upsert 메서드 사용)
    const std::string UPSERT = R"(
        INSERT OR REPLACE INTO data_points (
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    // 🔥🔥🔥 누락된 쿼리 7: UPDATE_BASIC_INFO (updateBasicInfo 메서드 사용)
    const std::string UPDATE_BASIC_INFO = R"(
        UPDATE data_points SET 
            name = ?, description = ?, unit = ?, 
            scaling_factor = ?, scaling_offset = ?,
            min_value = ?, max_value = ?,
            updated_at = ?
        WHERE id = ?
    )";
    
    // 🔥🔥🔥 누락된 쿼리 8: UPDATE_STATUS (updateStatus 메서드 사용)  
    const std::string UPDATE_STATUS = R"(
        UPDATE data_points SET 
            is_enabled = ?, updated_at = ?
        WHERE id = ?
    )";
    
    // 🔥🔥🔥 누락된 쿼리 9: UPDATE_LOG_CONFIG (updateLogConfig 메서드 사용)
    const std::string UPDATE_LOG_CONFIG = R"(
        UPDATE data_points SET 
            log_enabled = ?, log_interval_ms = ?, log_deadband = ?,
            updated_at = ?
        WHERE id = ?
    )";
    
    // 🔥🔥🔥 누락된 쿼리 10: BULK_UPDATE_STATUS (bulkUpdateStatus 메서드 사용)
    const std::string BULK_UPDATE_STATUS = R"(
        UPDATE data_points SET 
            is_enabled = ?, updated_at = ?
        WHERE id IN (%s)
    )"; // %s는 런타임에 ID 목록으로 대체됨
    
    // 🔥🔥🔥 누락된 쿼리 11: COUNT_BY_CONDITIONS (countByConditions에서 확장 사용)
    const std::string COUNT_ENABLED = "SELECT COUNT(*) as count FROM data_points WHERE is_enabled = 1";
    const std::string COUNT_DISABLED = "SELECT COUNT(*) as count FROM data_points WHERE is_enabled = 0";
    const std::string COUNT_WRITABLE = "SELECT COUNT(*) as count FROM data_points WHERE access_mode IN ('write', 'readwrite')";
    const std::string COUNT_LOG_ENABLED = "SELECT COUNT(*) as count FROM data_points WHERE log_enabled = 1";
    
    // 🔥🔥🔥 누락된 쿼리 12: 통계 관련 쿼리들
    const std::string GET_STATS_BY_DEVICE = R"(
        SELECT 
            device_id,
            COUNT(*) as total_count,
            COUNT(CASE WHEN is_enabled = 1 THEN 1 END) as enabled_count,
            COUNT(CASE WHEN access_mode IN ('write', 'readwrite') THEN 1 END) as writable_count,
            COUNT(CASE WHEN log_enabled = 1 THEN 1 END) as log_enabled_count
        FROM data_points 
        GROUP BY device_id
        ORDER BY device_id
    )";
    
    // 🔥🔥🔥 누락된 쿼리 13: 최근 생성/수정 조회
    const std::string FIND_RECENTLY_CREATED = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE created_at >= datetime('now', '-? days')
        ORDER BY created_at DESC
    )";
    
    const std::string FIND_RECENTLY_UPDATED = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE updated_at >= datetime('now', '-? days')
        ORDER BY updated_at DESC
    )";
    
    // 🔥🔥🔥 누락된 쿼리 14: 검증 및 중복 체크
    const std::string CHECK_DUPLICATE_ADDRESS = R"(
        SELECT COUNT(*) as count 
        FROM data_points 
        WHERE device_id = ? AND address = ? AND id != ?
    )";
    
    const std::string FIND_DUPLICATE_NAMES = R"(
        SELECT name, COUNT(*) as count
        FROM data_points 
        WHERE device_id = ?
        GROUP BY name 
        HAVING COUNT(*) > 1
    )";
    
    // 🔥🔥🔥 누락된 쿼리 15: 페이징 및 정렬
    const std::string FIND_WITH_PAGINATION = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        %s  -- WHERE 절 (조건부)
        ORDER BY %s %s  -- ORDER BY 컬럼, ASC/DESC
        LIMIT ? OFFSET ?
    )";
    
    // 🔥🔥🔥 누락된 쿼리 16: 고급 필터링
    const std::string FIND_BY_NAME_PATTERN = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE name LIKE ? AND is_enabled = 1
        ORDER BY device_id, address
    )";
    
    const std::string FIND_BY_ADDRESS_RANGE = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE device_id = ? AND address BETWEEN ? AND ?
        ORDER BY address
    )";
    
    // 🔥🔥🔥 누락된 쿼리 17: 백업 및 복구 관련
    const std::string EXPORT_FOR_BACKUP = R"(
        SELECT 
            device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata
        FROM data_points 
        WHERE device_id IN (%s)
        ORDER BY device_id, address
    )";
    
    // 🔥🔥🔥 누락된 쿼리 18: 디바이스 이전/복사
    const std::string COPY_TO_DEVICE = R"(
        INSERT INTO data_points (
            device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        )
        SELECT 
            ? as device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            datetime('now') as created_at, datetime('now') as updated_at
        FROM data_points 
        WHERE device_id = ?
    )";

    const std::string FIND_LAST_CREATED_BY_DEVICE_ADDRESS = R"(
        SELECT id FROM data_points 
        WHERE device_id = ? AND address = ?
        ORDER BY created_at DESC LIMIT 1
    )";
    
    const std::string FIND_WRITABLE_POINTS = R"(
        SELECT * FROM data_points 
        WHERE is_writable = 1 AND is_enabled = 1
        ORDER BY device_id, address
    )";
    
    const std::string FIND_BY_DATA_TYPE = R"(
        SELECT * FROM data_points 
        WHERE data_type = ? AND is_enabled = 1
        ORDER BY device_id, address
    )";        
    
} // namespace DataPoint


// =============================================================================
// 🎯 DeviceSettingsRepository 쿼리들 (완전판)
// =============================================================================
namespace DeviceSettings {
    
    const std::string FIND_ALL = R"(
        SELECT 
            device_id, polling_interval_ms, connection_timeout_ms, max_retry_count,
            retry_interval_ms, backoff_time_ms, keep_alive_enabled, keep_alive_interval_s,
            scan_rate_override, read_timeout_ms, write_timeout_ms, backoff_multiplier,
            max_backoff_time_ms, keep_alive_timeout_s, data_validation_enabled,
            performance_monitoring_enabled, diagnostic_mode_enabled, updated_at
        FROM device_settings 
        ORDER BY device_id
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            device_id, polling_interval_ms, connection_timeout_ms, max_retry_count,
            retry_interval_ms, backoff_time_ms, keep_alive_enabled, keep_alive_interval_s,
            scan_rate_override, read_timeout_ms, write_timeout_ms, backoff_multiplier,
            max_backoff_time_ms, keep_alive_timeout_s, data_validation_enabled,
            performance_monitoring_enabled, diagnostic_mode_enabled, updated_at
        FROM device_settings 
        WHERE device_id = ?
    )";
    
    const std::string FIND_BY_PROTOCOL = R"(
        SELECT 
            ds.device_id, ds.polling_interval_ms, ds.connection_timeout_ms, ds.max_retry_count,
            ds.retry_interval_ms, ds.backoff_time_ms, ds.keep_alive_enabled, ds.keep_alive_interval_s,
            ds.scan_rate_override, ds.read_timeout_ms, ds.write_timeout_ms, ds.backoff_multiplier,
            ds.max_backoff_time_ms, ds.keep_alive_timeout_s, ds.data_validation_enabled,
            ds.performance_monitoring_enabled, ds.diagnostic_mode_enabled, ds.updated_at
        FROM device_settings ds
        INNER JOIN devices d ON ds.device_id = d.id
        WHERE d.protocol_type = ?
        ORDER BY ds.device_id
    )";
    
    const std::string FIND_ACTIVE_DEVICES = R"(
        SELECT 
            ds.device_id, ds.polling_interval_ms, ds.connection_timeout_ms, ds.max_retry_count,
            ds.retry_interval_ms, ds.backoff_time_ms, ds.keep_alive_enabled, ds.keep_alive_interval_s,
            ds.scan_rate_override, ds.read_timeout_ms, ds.write_timeout_ms, ds.backoff_multiplier,
            ds.max_backoff_time_ms, ds.keep_alive_timeout_s, ds.data_validation_enabled,
            ds.performance_monitoring_enabled, ds.diagnostic_mode_enabled, ds.updated_at
        FROM device_settings ds
        INNER JOIN devices d ON ds.device_id = d.id
        WHERE d.is_enabled = 1
        ORDER BY ds.polling_interval_ms, ds.device_id
    )";
    
    const std::string UPSERT = R"(
        INSERT OR REPLACE INTO device_settings (
            device_id, polling_interval_ms, connection_timeout_ms, max_retry_count,
            retry_interval_ms, backoff_time_ms, keep_alive_enabled, keep_alive_interval_s,
            scan_rate_override, read_timeout_ms, write_timeout_ms, backoff_multiplier,
            max_backoff_time_ms, keep_alive_timeout_s, data_validation_enabled,
            performance_monitoring_enabled, diagnostic_mode_enabled, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE_POLLING_INTERVAL = R"(
        UPDATE device_settings 
        SET polling_interval_ms = ?, updated_at = ?
        WHERE device_id = ?
    )";
    
    const std::string UPDATE_CONNECTION_TIMEOUT = R"(
        UPDATE device_settings 
        SET connection_timeout_ms = ?, updated_at = ?
        WHERE device_id = ?
    )";
    
    const std::string UPDATE_RETRY_SETTINGS = R"(
        UPDATE device_settings 
        SET max_retry_count = ?, retry_interval_ms = ?, updated_at = ?
        WHERE device_id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM device_settings WHERE device_id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM device_settings WHERE device_id = ?";
    
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM device_settings";
    
    const std::string GET_POLLING_INTERVAL_DISTRIBUTION = R"(
        SELECT polling_interval_ms, COUNT(*) as count 
        FROM device_settings 
        GROUP BY polling_interval_ms
        ORDER BY polling_interval_ms
    )";
    
    // 테이블 생성
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS device_settings (
            device_id INTEGER PRIMARY KEY,
            
            -- 기본 통신 설정
            polling_interval_ms INTEGER DEFAULT 1000,
            connection_timeout_ms INTEGER DEFAULT 10000,
            max_retry_count INTEGER DEFAULT 3,
            retry_interval_ms INTEGER DEFAULT 5000,
            backoff_time_ms INTEGER DEFAULT 60000,
            keep_alive_enabled INTEGER DEFAULT 1,
            keep_alive_interval_s INTEGER DEFAULT 30,
            
            -- 고급 설정
            scan_rate_override INTEGER,
            read_timeout_ms INTEGER DEFAULT 5000,
            write_timeout_ms INTEGER DEFAULT 5000,
            backoff_multiplier DECIMAL(3,2) DEFAULT 1.5,
            max_backoff_time_ms INTEGER DEFAULT 300000,
            keep_alive_timeout_s INTEGER DEFAULT 10,
            data_validation_enabled INTEGER DEFAULT 1,
            performance_monitoring_enabled INTEGER DEFAULT 1,
            diagnostic_mode_enabled INTEGER DEFAULT 0,
            
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
} // namespace DeviceSettings

// =============================================================================
// 🎯 CurrentValueRepository 쿼리들  
// =============================================================================
namespace CurrentValue {

    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS current_values (
            point_id INTEGER PRIMARY KEY,
            -- 🔥 실제 값 (타입별로 분리하지 않고 통합)
            current_value TEXT, -- JSON으로 DataVariant 저장
            raw_value TEXT, -- JSON으로 DataVariant 저장
            value_type VARCHAR(10) DEFAULT 'double', -- bool, int16, uint16, int32, uint32, float, double, string
            -- 🔥 데이터 품질 및 타임스탬프
            quality_code INTEGER DEFAULT 0, -- DataQuality enum 값
            quality VARCHAR(20) DEFAULT 'not_connected', -- 텍스트 표현
            -- 🔥 타임스탬프들
            value_timestamp DATETIME, -- 값 변경 시간
            quality_timestamp DATETIME, -- 품질 변경 시간
            last_log_time DATETIME, -- 마지막 로깅 시간
            last_read_time DATETIME, -- 마지막 읽기 시간
            last_write_time DATETIME, -- 마지막 쓰기 시간
            -- 🔥 통계 카운터들
            read_count INTEGER DEFAULT 0, -- 읽기 횟수
            write_count INTEGER DEFAULT 0, -- 쓰기 횟수
            error_count INTEGER DEFAULT 0, -- 에러 횟수
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE
        )
    )";

    const std::string FIND_ALL = R"(
        SELECT 
            point_id, current_value, raw_value, value_type,
            quality_code, quality,
            value_timestamp, quality_timestamp, last_log_time,
            last_read_time, last_write_time,
            read_count, write_count, error_count, updated_at
        FROM current_values 
        ORDER BY point_id
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            point_id, current_value, raw_value, value_type,
            quality_code, quality,
            value_timestamp, quality_timestamp, last_log_time,
            last_read_time, last_write_time,
            read_count, write_count, error_count, updated_at
        FROM current_values 
        WHERE point_id = ?
    )";
    
    const std::string FIND_BY_DEVICE_ID = R"(
        SELECT 
            cv.point_id, cv.current_value, cv.raw_value, cv.value_type,
            cv.quality_code, cv.quality,
            cv.value_timestamp, cv.quality_timestamp, cv.last_log_time,
            cv.last_read_time, cv.last_write_time,
            cv.read_count, cv.write_count, cv.error_count, cv.updated_at
        FROM current_values cv
        JOIN data_points dp ON cv.point_id = dp.id
        WHERE dp.device_id = ?
        ORDER BY dp.address
    )";

    const std::string FIND_BY_IDS = R"(
        SELECT 
            point_id, current_value, raw_value, value_type,
            quality_code, quality,
            value_timestamp, quality_timestamp, last_log_time,
            last_read_time, last_write_time,
            read_count, write_count, error_count, updated_at
        FROM current_values 
        WHERE point_id IN (%s)
        ORDER BY point_id
    )";
    
    const std::string FIND_BY_QUALITY_CODE = R"(
        SELECT 
            point_id, current_value, raw_value, value_type,
            quality_code, quality,
            value_timestamp, quality_timestamp, last_log_time,
            last_read_time, last_write_time,
            read_count, write_count, error_count, updated_at
        FROM current_values 
        WHERE quality_code = ?
        ORDER BY updated_at DESC
    )";

    const std::string FIND_BY_QUALITY = R"(
        SELECT 
            point_id, current_value, raw_value, value_type,
            quality_code, quality,
            value_timestamp, quality_timestamp, last_log_time,
            last_read_time, last_write_time,
            read_count, write_count, error_count, updated_at
        FROM current_values 
        WHERE quality = ?
        ORDER BY updated_at DESC
    )";

    const std::string FIND_STALE_VALUES = R"(
        SELECT 
            point_id, current_value, raw_value, value_type,
            quality_code, quality,
            value_timestamp, quality_timestamp, last_log_time,
            last_read_time, last_write_time,
            read_count, write_count, error_count, updated_at
        FROM current_values 
        WHERE updated_at < ?
        ORDER BY updated_at ASC
    )";

    const std::string FIND_BAD_QUALITY_VALUES = R"(
        SELECT 
            point_id, current_value, raw_value, value_type,
            quality_code, quality,
            value_timestamp, quality_timestamp, last_log_time,
            last_read_time, last_write_time,
            read_count, write_count, error_count, updated_at
        FROM current_values 
        WHERE quality IN ('bad', 'uncertain', 'not_connected')
        ORDER BY updated_at DESC
    )";

    const std::string UPSERT = R"(
        INSERT OR REPLACE INTO current_values (
            point_id, current_value, raw_value, value_type,
            quality_code, quality,
            value_timestamp, quality_timestamp, last_log_time,
            last_read_time, last_write_time,
            read_count, write_count, error_count, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string INSERT = R"(
        INSERT INTO current_values (
            point_id, current_value, raw_value, value_type,
            quality_code, quality,
            value_timestamp, quality_timestamp, last_log_time,
            last_read_time, last_write_time,
            read_count, write_count, error_count, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    const std::string UPDATE = R"(
        UPDATE current_values SET 
            current_value = ?, raw_value = ?, value_type = ?,
            quality_code = ?, quality = ?,
            value_timestamp = ?, quality_timestamp = ?, last_log_time = ?,
            last_read_time = ?, last_write_time = ?,
            read_count = ?, write_count = ?, error_count = ?, updated_at = ?
        WHERE point_id = ?
    )";

    const std::string UPDATE_VALUE = R"(
        UPDATE current_values SET 
            current_value = ?, raw_value = ?, value_type = ?,
            value_timestamp = ?, updated_at = ?
        WHERE point_id = ?
    )";

    const std::string UPDATE_QUALITY = R"(
        UPDATE current_values SET 
            quality_code = ?, quality = ?, quality_timestamp = ?, updated_at = ?
        WHERE point_id = ?
    )";

    const std::string UPDATE_STATISTICS = R"(
        UPDATE current_values SET 
            read_count = ?, write_count = ?, error_count = ?, updated_at = ?
        WHERE point_id = ?
    )";

    const std::string INCREMENT_READ_COUNT = R"(
        UPDATE current_values SET 
            read_count = read_count + 1, last_read_time = ?, updated_at = ?
        WHERE point_id = ?
    )";

    const std::string INCREMENT_WRITE_COUNT = R"(
        UPDATE current_values SET 
            write_count = write_count + 1, last_write_time = ?, updated_at = ?
        WHERE point_id = ?
    )";

    const std::string INCREMENT_ERROR_COUNT = R"(
        UPDATE current_values SET 
            error_count = error_count + 1, updated_at = ?
        WHERE point_id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM current_values WHERE point_id = ?";
    
    const std::string DELETE_BY_DEVICE_ID = R"(
        DELETE FROM current_values 
        WHERE point_id IN (
            SELECT id FROM data_points WHERE device_id = ?
        )
    )";

    const std::string DELETE_STALE_VALUES = R"(
        DELETE FROM current_values 
        WHERE updated_at < ?
    )";

    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM current_values WHERE point_id = ?";

    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM current_values";

    const std::string COUNT_BY_QUALITY = "SELECT COUNT(*) as count FROM current_values WHERE quality = ?";

    const std::string COUNT_BY_QUALITY_CODE = "SELECT COUNT(*) as count FROM current_values WHERE quality_code = ?";

    const std::string GET_LATEST_VALUES = R"(
        SELECT 
            cv.point_id, dp.name as point_name, cv.current_value, cv.quality, cv.value_timestamp,
            d.name as device_name, d.protocol_type
        FROM current_values cv
        JOIN data_points dp ON cv.point_id = dp.id
        JOIN devices d ON dp.device_id = d.id
        WHERE cv.value_timestamp >= ?
        ORDER BY cv.value_timestamp DESC
    )";

    const std::string GET_QUALITY_DISTRIBUTION = R"(
        SELECT quality, COUNT(*) as count 
        FROM current_values 
        GROUP BY quality
        ORDER BY count DESC
    )";

    const std::string GET_VALUE_TYPE_DISTRIBUTION = R"(
        SELECT value_type, COUNT(*) as count 
        FROM current_values 
        GROUP BY value_type
        ORDER BY count DESC
    )";

    const std::string GET_STATISTICS_SUMMARY = R"(
        SELECT 
            COUNT(*) as total_count,
            AVG(read_count) as avg_reads,
            AVG(write_count) as avg_writes,
            AVG(error_count) as avg_errors,
            MAX(read_count) as max_reads,
            MAX(write_count) as max_writes,
            MAX(error_count) as max_errors
        FROM current_values
    )";

    // 🔥 기존 테스트와 호환성을 위한 별칭들
    const std::string FIND_BY_POINT_ID = FIND_BY_ID;
    const std::string UPSERT_VALUE = UPSERT;
    
} // namespace CurrentValue



// =============================================================================
// 🎯 동적 쿼리 빌더 헬퍼들
// =============================================================================
namespace QueryBuilder {
    
    /**
     * @brief WHERE 절 생성 헬퍼
     * @param conditions 조건들 (column=value 형태)
     * @return WHERE 절 문자열
     */
    inline std::string buildWhereClause(const std::vector<std::pair<std::string, std::string>>& conditions) {
        if (conditions.empty()) return "";
        
        std::string where_clause = " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) where_clause += " AND ";
            where_clause += conditions[i].first + " = '" + conditions[i].second + "'";
        }
        return where_clause;
    }
    
    /**
     * @brief ORDER BY 절 생성 헬퍼
     * @param column 정렬할 컬럼
     * @param direction ASC 또는 DESC
     * @return ORDER BY 절 문자열
     */
    inline std::string buildOrderByClause(const std::string& column, const std::string& direction = "ASC") {
        return " ORDER BY " + column + " " + direction;
    }
    
    /**
     * @brief LIMIT 절 생성 헬퍼
     * @param limit 제한 수
     * @param offset 오프셋 (기본값: 0)
     * @return LIMIT 절 문자열
     */
    inline std::string buildLimitClause(int limit, int offset = 0) {
        if (offset > 0) {
            return " LIMIT " + std::to_string(limit) + " OFFSET " + std::to_string(offset);
        } else {
            return " LIMIT " + std::to_string(limit);
        }
    }
    
} // namespace QueryBuilder

namespace VirtualPoint {
    
    // 🔥 FIND_ALL - 모든 가상포인트 조회 (Device::FIND_ALL 패턴)
    const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, is_enabled,
            category, tags, scope_type,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE is_enabled = 1
        ORDER BY tenant_id, name
    )";
    
    // 🔥 FIND_BY_ID - ID로 조회 (Device::FIND_BY_ID 패턴)
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, is_enabled,
            category, tags, scope_type,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE id = ?
    )";
    
    // 🔥 INSERT - 새 가상포인트 생성 (Device 패턴)
    const std::string INSERT = R"(
        INSERT INTO virtual_points (
            tenant_id, site_id, device_id, name, description, formula,
            data_type, unit, calculation_interval, calculation_trigger,
            is_enabled, category, tags, scope_type, created_by
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    // 🔥 UPDATE - 기존 가상포인트 수정 (Device 패턴)
    const std::string UPDATE = R"(
        UPDATE virtual_points SET 
            tenant_id = ?, site_id = ?, device_id = ?, name = ?, 
            description = ?, formula = ?, data_type = ?, unit = ?,
            calculation_interval = ?, calculation_trigger = ?,
            is_enabled = ?, category = ?, tags = ?, scope_type = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    // 🔥 DELETE_BY_ID - ID로 삭제 (Device 패턴)
    const std::string DELETE_BY_ID = "DELETE FROM virtual_points WHERE id = ?";
    
    // 🔥 EXISTS_BY_ID - 존재 여부 확인 (Device 패턴)
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM virtual_points WHERE id = ?";
    
    // 🔥 COUNT_ALL - 전체 개수 (Device 패턴)
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM virtual_points";
    
    // 🔥 COUNT_ENABLED - 활성화된 개수 (Device 패턴)
    const std::string COUNT_ENABLED = "SELECT COUNT(*) as count FROM virtual_points WHERE is_enabled = 1";
    
    // =======================================================================
    // VirtualPoint 전용 쿼리들
    // =======================================================================
    
    // 🔥 FIND_BY_TENANT - 테넌트별 조회
    const std::string FIND_BY_TENANT = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, is_enabled,
            category, tags, scope_type,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE tenant_id = ? AND is_enabled = 1
        ORDER BY name
    )";
    
    // 🔥 FIND_BY_SCOPE - 범위별 조회 (tenant/site/device)
    const std::string FIND_BY_SCOPE = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, is_enabled,
            category, tags, scope_type,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE scope_type = ? AND is_enabled = 1
        ORDER BY name
    )";
    
    // 🔥 FIND_ACTIVE_FOR_CALCULATION - 계산할 활성 가상포인트들
    const std::string FIND_ACTIVE_FOR_CALCULATION = R"(
        SELECT 
            id, name, formula, calculation_interval, calculation_trigger
        FROM virtual_points 
        WHERE is_enabled = 1 AND calculation_trigger = 'timer'
        ORDER BY calculation_interval
    )";
    
    // 🔥 FIND_INPUTS_BY_VP_ID - 가상포인트의 입력 매핑
    const std::string FIND_INPUTS_BY_VP_ID = R"(
        SELECT 
            variable_name, source_type, source_id, expression
        FROM virtual_point_inputs 
        WHERE virtual_point_id = ?
        ORDER BY variable_name
    )";
    
    // 🔥 GET_LAST_INSERT_ID - 마지막 삽입 ID (Device 패턴)
    const std::string GET_LAST_INSERT_ID = "SELECT last_insert_rowid() as id";
    
    // =======================================================================
    // 테이블 생성 (Device::CREATE_TABLE 패턴)
    // =======================================================================
    
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS virtual_points (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER NOT NULL,
            site_id INTEGER,
            device_id INTEGER,
            
            -- 가상포인트 기본 정보
            name VARCHAR(100) NOT NULL,
            description TEXT,
            formula TEXT NOT NULL,
            data_type VARCHAR(20) NOT NULL DEFAULT 'float',
            unit VARCHAR(20),
            
            -- 계산 설정
            calculation_interval INTEGER DEFAULT 1000,
            calculation_trigger VARCHAR(20) DEFAULT 'timer',
            is_enabled INTEGER DEFAULT 1,
            
            -- 메타데이터
            category VARCHAR(50),
            tags TEXT,
            scope_type VARCHAR(20) NOT NULL DEFAULT 'tenant',
            
            created_by INTEGER,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            -- 제약조건
            CONSTRAINT chk_scope_type CHECK (scope_type IN ('tenant', 'site', 'device'))
        )
    )";
    
    // 🔥 가상포인트 입력 매핑 테이블
    const std::string CREATE_INPUTS_TABLE = R"(
        CREATE TABLE IF NOT EXISTS virtual_point_inputs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            virtual_point_id INTEGER NOT NULL,
            variable_name VARCHAR(50) NOT NULL,
            source_type VARCHAR(20) NOT NULL,
            source_id INTEGER,
            expression TEXT,
            
            FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
            CONSTRAINT chk_source_type CHECK (source_type IN ('data_point', 'virtual_point', 'constant'))
        )
    )";
    
    // 🔥 가상포인트 실행 로그 테이블
    const std::string CREATE_EXECUTION_LOG_TABLE = R"(
        CREATE TABLE IF NOT EXISTS virtual_point_execution_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            virtual_point_id INTEGER NOT NULL,
            calculated_value REAL,
            execution_time_ms INTEGER,
            status VARCHAR(20),
            error_message TEXT,
            executed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE
        )
    )";
    
} // namespace VirtualPoint


// =============================================================================
// 🎯 AlarmRule 관련 쿼리들 (alarm_rules 테이블)
// =============================================================================
namespace AlarmRule {
    
    // 🔥🔥🔥 FIND_ALL - 모든 알람 규칙 조회 (우선순위, 중요도 순)
    const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_at, updated_at, created_by
        FROM alarm_rules 
        ORDER BY priority DESC, severity, name
    )";
    
    // 🔥🔥🔥 FIND_BY_ID - 특정 알람 규칙 조회
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_at, updated_at, created_by
        FROM alarm_rules 
        WHERE id = ?
    )";
    
    // 🔥🔥🔥 FIND_BY_TARGET - 특정 대상에 대한 알람 규칙들
    const std::string FIND_BY_TARGET = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_at, updated_at, created_by
        FROM alarm_rules 
        WHERE target_type = ? AND target_id = ?
        ORDER BY priority DESC, severity
    )";
    
    // 🔥🔥🔥 FIND_BY_TENANT - 테넌트별 알람 규칙들
    const std::string FIND_BY_TENANT = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_at, updated_at, created_by
        FROM alarm_rules 
        WHERE tenant_id = ?
        ORDER BY priority DESC, severity, name
    )";
    
    // 🔥🔥🔥 FIND_ENABLED - 활성화된 알람 규칙들만
    const std::string FIND_ENABLED = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_at, updated_at, created_by
        FROM alarm_rules 
        WHERE is_enabled = 1
        ORDER BY priority DESC, severity
    )";
    
    // 🔥🔥🔥 FIND_BY_SEVERITY - 중요도별 알람 규칙들
    const std::string FIND_BY_SEVERITY = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_at, updated_at, created_by
        FROM alarm_rules 
        WHERE severity = ?
        ORDER BY priority DESC, name
    )";
    
    // 🔥🔥🔥 FIND_BY_ALARM_TYPE - 알람 타입별 (analog, digital, script)
    const std::string FIND_BY_ALARM_TYPE = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_at, updated_at, created_by
        FROM alarm_rules 
        WHERE alarm_type = ?
        ORDER BY priority DESC, severity
    )";
    
    // 🔥🔥🔥 INSERT - 새 알람 규칙 생성
    const std::string INSERT = R"(
        INSERT INTO alarm_rules (
            tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_by, created_at, updated_at
        ) VALUES (
            ?, ?, ?, ?, ?, ?,
            ?, ?, ?, ?, ?,
            ?, ?, ?, ?,
            ?, ?, ?, ?, ?,
            ?, ?, ?, ?,
            ?, ?, ?,
            ?, ?, ?, ?,
            ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP
        )
    )";
    
    // 🔥🔥🔥 UPDATE - 알람 규칙 업데이트
    const std::string UPDATE = R"(
        UPDATE alarm_rules SET 
            tenant_id = ?, name = ?, description = ?, target_type = ?, target_id = ?, target_group = ?,
            alarm_type = ?, high_high_limit = ?, high_limit = ?, low_limit = ?, low_low_limit = ?,
            deadband = ?, rate_of_change = ?, trigger_condition = ?, condition_script = ?,
            message_script = ?, message_config = ?, message_template = ?, severity = ?, priority = ?,
            auto_acknowledge = ?, acknowledge_timeout_min = ?, auto_clear = ?, suppression_rules = ?,
            notification_enabled = ?, notification_delay_sec = ?, notification_repeat_interval_min = ?,
            notification_channels = ?, notification_recipients = ?, is_enabled = ?, is_latched = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    // 🔥🔥🔥 UPDATE_STATUS - 알람 규칙 활성화/비활성화
    const std::string UPDATE_STATUS = R"(
        UPDATE alarm_rules SET 
            is_enabled = ?, updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    // 🔥🔥🔥 기본 CRUD 및 유틸리티
    const std::string DELETE_BY_ID = "DELETE FROM alarm_rules WHERE id = ?";
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM alarm_rules WHERE id = ?";
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM alarm_rules";
    const std::string COUNT_ENABLED = "SELECT COUNT(*) as count FROM alarm_rules WHERE is_enabled = 1";
    const std::string COUNT_BY_SEVERITY = "SELECT COUNT(*) as count FROM alarm_rules WHERE severity = ?";
    const std::string COUNT_BY_TARGET = "SELECT COUNT(*) as count FROM alarm_rules WHERE target_type = ? AND target_id = ?";
    const std::string GET_LAST_INSERT_ID = "SELECT last_insert_rowid() as id";
    
    // 🔥🔥🔥 통계 쿼리들
    const std::string GET_SEVERITY_DISTRIBUTION = R"(
        SELECT severity, COUNT(*) as count 
        FROM alarm_rules 
        GROUP BY severity
        ORDER BY 
            CASE severity 
                WHEN 'critical' THEN 1
                WHEN 'high' THEN 2  
                WHEN 'medium' THEN 3
                WHEN 'low' THEN 4
                WHEN 'info' THEN 5
                ELSE 6
            END
    )";
    
    const std::string GET_TYPE_DISTRIBUTION = R"(
        SELECT alarm_type, COUNT(*) as count 
        FROM alarm_rules 
        GROUP BY alarm_type
        ORDER BY count DESC
    )";
    
    // 🔥🔥🔥 CREATE_TABLE - alarm_rules 테이블 생성
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS alarm_rules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER NOT NULL,
            name VARCHAR(100) NOT NULL,
            description TEXT,
            
            -- 대상 정보
            target_type VARCHAR(20) NOT NULL,  -- 'data_point', 'virtual_point', 'group'
            target_id INTEGER,
            target_group VARCHAR(100),
            
            -- 알람 타입
            alarm_type VARCHAR(20) NOT NULL,  -- 'analog', 'digital', 'script'
            
            -- 아날로그 알람 설정
            high_high_limit REAL,
            high_limit REAL,
            low_limit REAL,
            low_low_limit REAL,
            deadband REAL DEFAULT 0,
            rate_of_change REAL DEFAULT 0,
            
            -- 디지털 알람 설정
            trigger_condition VARCHAR(20),  -- 'on_true', 'on_false', 'on_change', 'on_rising', 'on_falling'
            
            -- 스크립트 기반 알람
            condition_script TEXT,
            message_script TEXT,
            
            -- 메시지 커스터마이징
            message_config TEXT,  -- JSON 형태
            message_template TEXT,
            
            -- 우선순위
            severity VARCHAR(20) DEFAULT 'medium',  -- 'critical', 'high', 'medium', 'low', 'info'
            priority INTEGER DEFAULT 100,
            
            -- 자동 처리
            auto_acknowledge INTEGER DEFAULT 0,
            acknowledge_timeout_min INTEGER DEFAULT 0,
            auto_clear INTEGER DEFAULT 1,
            
            -- 억제 규칙
            suppression_rules TEXT,  -- JSON 형태
            
            -- 알림 설정
            notification_enabled INTEGER DEFAULT 1,
            notification_delay_sec INTEGER DEFAULT 0,
            notification_repeat_interval_min INTEGER DEFAULT 0,
            notification_channels TEXT,  -- JSON 배열
            notification_recipients TEXT,  -- JSON 배열
            
            -- 상태
            is_enabled INTEGER DEFAULT 1,
            is_latched INTEGER DEFAULT 0,
            
            -- 타임스탬프
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            created_by INTEGER,
            
            FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
            FOREIGN KEY (created_by) REFERENCES users(id)
        )
    )";
    
} // namespace AlarmRule

namespace AlarmOccurrence {
    
    const std::string FIND_ALL = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by, acknowledge_comment,
            cleared_time, cleared_value, clear_comment, notification_sent, notification_time,
            notification_count, notification_result, context_data, source_name, location,
            created_at, updated_at
        FROM alarm_occurrences 
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by, acknowledge_comment,
            cleared_time, cleared_value, clear_comment, notification_sent, notification_time,
            notification_count, notification_result, context_data, source_name, location,
            created_at, updated_at
        FROM alarm_occurrences 
        WHERE id = ?
    )";
    
    const std::string FIND_BY_RULE_ID = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by, acknowledge_comment,
            cleared_time, cleared_value, clear_comment, notification_sent, notification_time,
            notification_count, notification_result, context_data, source_name, location,
            created_at, updated_at
        FROM alarm_occurrences 
        WHERE rule_id = ?
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_BY_TENANT_ID = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by, acknowledge_comment,
            cleared_time, cleared_value, clear_comment, notification_sent, notification_time,
            notification_count, notification_result, context_data, source_name, location,
            created_at, updated_at
        FROM alarm_occurrences 
        WHERE tenant_id = ?
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_ACTIVE = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by, acknowledge_comment,
            cleared_time, cleared_value, clear_comment, notification_sent, notification_time,
            notification_count, notification_result, context_data, source_name, location,
            created_at, updated_at
        FROM alarm_occurrences 
        WHERE state = 'active'
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_BY_SEVERITY = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by, acknowledge_comment,
            cleared_time, cleared_value, clear_comment, notification_sent, notification_time,
            notification_count, notification_result, context_data, source_name, location,
            created_at, updated_at
        FROM alarm_occurrences 
        WHERE severity = ?
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_BY_STATE = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by, acknowledge_comment,
            cleared_time, cleared_value, clear_comment, notification_sent, notification_time,
            notification_count, notification_result, context_data, source_name, location,
            created_at, updated_at
        FROM alarm_occurrences 
        WHERE state = ?
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_UNACKNOWLEDGED = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by, acknowledge_comment,
            cleared_time, cleared_value, clear_comment, notification_sent, notification_time,
            notification_count, notification_result, context_data, source_name, location,
            created_at, updated_at
        FROM alarm_occurrences 
        WHERE acknowledged_time IS NULL AND state != 'cleared'
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_PENDING_NOTIFICATION = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by, acknowledge_comment,
            cleared_time, cleared_value, clear_comment, notification_sent, notification_time,
            notification_count, notification_result, context_data, source_name, location,
            created_at, updated_at
        FROM alarm_occurrences 
        WHERE notification_sent = 0 OR notification_sent IS NULL
        ORDER BY occurrence_time DESC
    )";
    
    // INSERT 문
    const std::string INSERT = R"(
        INSERT INTO alarm_occurrences (
            rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledge_comment, cleared_value,
            clear_comment, notification_sent, notification_count, notification_result,
            context_data, source_name, location
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    // UPDATE 문
    const std::string UPDATE = R"(
        UPDATE alarm_occurrences SET
            rule_id = ?, tenant_id = ?, occurrence_time = ?, trigger_value = ?, trigger_condition = ?,
            alarm_message = ?, severity = ?, state = ?, acknowledge_comment = ?, cleared_value = ?,
            clear_comment = ?, notification_sent = ?, notification_count = ?, notification_result = ?,
            context_data = ?, source_name = ?, location = ?, acknowledged_time = ?, acknowledged_by = ?,
            cleared_time = ?, notification_time = ?, updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM alarm_occurrences WHERE id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM alarm_occurrences WHERE id = ?";
    
    // 통계 쿼리들
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM alarm_occurrences";
    
    const std::string COUNT_ACTIVE = "SELECT COUNT(*) as count FROM alarm_occurrences WHERE state = 'active'";
    
    const std::string COUNT_BY_RULE_ID = "SELECT COUNT(*) as count FROM alarm_occurrences WHERE rule_id = ?";
    
    const std::string COUNT_BY_TENANT_ID = "SELECT COUNT(*) as count FROM alarm_occurrences WHERE tenant_id = ?";
    
    const std::string COUNT_BY_SEVERITY = "SELECT COUNT(*) as count FROM alarm_occurrences WHERE severity = ?";
    
    const std::string GET_LAST_INSERT_ID = "SELECT last_insert_rowid() as id";
    
    // 테이블 생성 (Device::CREATE_TABLE 패턴)
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS alarm_occurrences (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            rule_id INTEGER NOT NULL,
            tenant_id INTEGER NOT NULL,
            
            -- 발생 정보
            occurrence_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
            trigger_value TEXT,
            trigger_condition TEXT,
            alarm_message TEXT NOT NULL,
            severity TEXT NOT NULL DEFAULT 'medium',
            state TEXT NOT NULL DEFAULT 'active',
            
            -- Acknowledge 정보
            acknowledged_time TIMESTAMP NULL,
            acknowledged_by INTEGER NULL,
            acknowledge_comment TEXT,
            
            -- Clear 정보
            cleared_time TIMESTAMP NULL,
            cleared_value TEXT,
            clear_comment TEXT,
            
            -- 알림 정보
            notification_sent BOOLEAN DEFAULT 0,
            notification_time TIMESTAMP NULL,
            notification_count INTEGER DEFAULT 0,
            notification_result TEXT,
            
            -- 추가 컨텍스트
            context_data TEXT,
            source_name TEXT,
            location TEXT,
            
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            
            -- 제약조건
            CONSTRAINT chk_severity CHECK (severity IN ('low', 'medium', 'high', 'critical')),
            CONSTRAINT chk_state CHECK (state IN ('active', 'acknowledged', 'cleared'))
        )
    )";
    
} // namespace AlarmOccurrence

} // namespace SQL
} // namespace Database  
} // namespace PulseOne

#endif // SQL_QUERIES_H