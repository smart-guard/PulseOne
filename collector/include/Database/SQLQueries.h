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
// 🎯 기타 공통 쿼리들
// =============================================================================
namespace Common {
    
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
    
} // namespace Common

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

} // namespace SQL
} // namespace Database  
} // namespace PulseOne

#endif // SQL_QUERIES_H