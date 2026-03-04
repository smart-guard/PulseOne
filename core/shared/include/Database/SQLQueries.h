// =============================================================================
// collector/include/Database/SQLQueries.h
// 🎯 SQL 쿼리 상수 중앙 관리 - 현재 스키마에 완전 일치 (2025년 8월 업데이트)
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
const std::string GET_CURRENT_TIMESTAMP =
    "SELECT datetime('now', 'localtime') as timestamp";

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
// 🎯 DeviceRepository 쿼리들 - 현재 스키마 완전 반영
// =============================================================================
namespace Device {

// 🔥🔥🔥 중요: protocol_id 사용, protocol_type 제거
const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id, protocol_instance_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_id, endpoint, config, polling_interval, timeout, retry_count,
            is_enabled, installation_date, last_maintenance, 
            created_by, created_at, updated_at
        FROM devices 
        ORDER BY id
    )";

const std::string FIND_BY_ID = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id, protocol_instance_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_id, endpoint, config, polling_interval, timeout, retry_count,
            is_enabled, installation_date, last_maintenance, 
            created_by, created_at, updated_at
        FROM devices 
        WHERE id = ?
    )";

// 🔥🔥🔥 프로토콜 조회 - protocol_id 사용
const std::string FIND_BY_PROTOCOL_ID = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id, protocol_instance_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_id, endpoint, config, polling_interval, timeout, retry_count,
            is_enabled, installation_date, last_maintenance, 
            created_by, created_at, updated_at
        FROM devices 
        WHERE protocol_id = ? AND is_enabled = 1
        ORDER BY name
    )";

// 🔥 프로토콜 타입별 조회 (JOIN 필요)
const std::string FIND_BY_PROTOCOL_TYPE = R"(
        SELECT 
            d.id, d.tenant_id, d.site_id, d.device_group_id, d.edge_server_id, d.protocol_instance_id,
            d.name, d.description, d.device_type, d.manufacturer, d.model, d.serial_number,
            d.protocol_id, d.endpoint, d.config, d.polling_interval, d.timeout, d.retry_count,
            d.is_enabled, d.installation_date, d.last_maintenance, 
            d.created_by, d.created_at, d.updated_at
        FROM devices d
        JOIN protocols p ON d.protocol_id = p.id
        WHERE p.protocol_type = ? AND d.is_enabled = 1
        ORDER BY d.name
    )";

const std::string FIND_BY_TENANT = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id, protocol_instance_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_id, endpoint, config, polling_interval, timeout, retry_count,
            is_enabled, installation_date, last_maintenance, 
            created_by, created_at, updated_at
        FROM devices 
        WHERE tenant_id = ?
        ORDER BY name
    )";

const std::string FIND_BY_SITE = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id, protocol_instance_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_id, endpoint, config, polling_interval, timeout, retry_count,
            is_enabled, installation_date, last_maintenance, 
            created_by, created_at, updated_at
        FROM devices 
        WHERE site_id = ?
        ORDER BY name
    )";

// 🔥 에지 서버별 조회 (컬렉터 필터링용)
const std::string FIND_BY_EDGE_SERVER = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id, protocol_instance_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_id, endpoint, config, polling_interval, timeout, retry_count,
            is_enabled, installation_date, last_maintenance, 
            created_by, created_at, updated_at
        FROM devices 
        WHERE edge_server_id = ? AND is_enabled = 1
        ORDER BY name
    )";

const std::string FIND_ENABLED = R"(
        SELECT 
            d.id, d.tenant_id, d.site_id, d.device_group_id, d.edge_server_id, d.protocol_instance_id,
            d.name, d.description, d.device_type, d.manufacturer, d.model, d.serial_number,
            d.protocol_id, d.endpoint, d.config, d.polling_interval, d.timeout, d.retry_count,
            d.is_enabled, d.installation_date, d.last_maintenance, 
            d.created_by, d.created_at, d.updated_at
        FROM devices d
        LEFT JOIN protocols p ON d.protocol_id = p.id
        WHERE d.is_enabled = 1
        ORDER BY p.protocol_type, d.name
    )";

const std::string FIND_DISABLED = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id, protocol_instance_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_id, endpoint, config, polling_interval, timeout, retry_count,
            is_enabled, installation_date, last_maintenance, 
            created_by, created_at, updated_at
        FROM devices 
        WHERE is_enabled = 0
        ORDER BY updated_at DESC
    )";

// 🔥🔥🔥 INSERT - 현재 스키마 필드들
const std::string INSERT = R"(
        INSERT INTO devices (
            tenant_id, site_id, device_group_id, edge_server_id, protocol_instance_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_id, endpoint, config, polling_interval, timeout, retry_count,
            is_enabled, installation_date, last_maintenance, 
            created_by, created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

// 🔥🔥🔥 UPDATE - 현재 스키마 필드들
const std::string UPDATE = R"(
        UPDATE devices SET 
            tenant_id = ?, site_id = ?, device_group_id = ?, edge_server_id = ?, protocol_instance_id = ?,
            name = ?, description = ?, device_type = ?, manufacturer = ?, model = ?, 
            serial_number = ?, protocol_id = ?, endpoint = ?, config = ?, 
            polling_interval = ?, timeout = ?, retry_count = ?,
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

const std::string UPDATE_PROTOCOL = R"(
        UPDATE devices 
        SET protocol_id = ?, updated_at = ?
        WHERE id = ?
    )";

const std::string DELETE_BY_ID = "DELETE FROM devices WHERE id = ?";

const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM devices WHERE id = ?";

const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM devices";

const std::string COUNT_ENABLED =
    "SELECT COUNT(*) as count FROM devices WHERE is_enabled = 1";

const std::string COUNT_BY_PROTOCOL_ID =
    "SELECT COUNT(*) as count FROM devices WHERE protocol_id = ?";

// 🔥 프로토콜 분포 - JOIN 사용
const std::string GET_PROTOCOL_DISTRIBUTION = R"(
        SELECT p.protocol_type, p.display_name, COUNT(d.id) as count 
        FROM protocols p
        LEFT JOIN devices d ON p.id = d.protocol_id
        GROUP BY p.id, p.protocol_type, p.display_name
        ORDER BY count DESC
    )";

const std::string GET_LAST_INSERT_ID = "SELECT last_insert_rowid() as id";

// 🔥🔥🔥 테이블 생성 - 현재 스키마 완전 반영
const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS devices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER NOT NULL,
            site_id INTEGER NOT NULL,
            device_group_id INTEGER,
            edge_server_id INTEGER,
            protocol_instance_id INTEGER,
            
            -- 디바이스 기본 정보
            name VARCHAR(100) NOT NULL,
            description TEXT,
            device_type VARCHAR(50) NOT NULL,
            manufacturer VARCHAR(100),
            model VARCHAR(100),
            serial_number VARCHAR(100),
            
            -- 프로토콜 설정 (외래키로 변경!)
            protocol_id INTEGER NOT NULL,
            endpoint VARCHAR(255) NOT NULL,
            config TEXT NOT NULL,
            
            -- 수집 설정
            polling_interval INTEGER DEFAULT 1000,
            timeout INTEGER DEFAULT 3000,
            retry_count INTEGER DEFAULT 3,
            
            -- 상태 정보
            is_enabled INTEGER DEFAULT 1,
            installation_date DATE,
            last_maintenance DATE,
            
            created_by INTEGER,
            created_at DATETIME DEFAULT (datetime('now', 'localtime')),
            updated_at DATETIME DEFAULT (datetime('now', 'localtime')),
            
            -- 외래키 제약조건
            FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
            FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
            FOREIGN KEY (device_group_id) REFERENCES device_groups(id) ON DELETE SET NULL,
            FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
            FOREIGN KEY (protocol_id) REFERENCES protocols(id) ON DELETE RESTRICT,
            FOREIGN KEY (protocol_instance_id) REFERENCES protocol_instances(id) ON DELETE SET NULL
        )
    )";

// 🔥 device_details 뷰 조회
const std::string FIND_WITH_PROTOCOL_INFO = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id, protocol_instance_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_id, endpoint, config, polling_interval, timeout, retry_count,
            is_enabled, installation_date, last_maintenance, 
            created_by, created_at, updated_at,
            protocol_type, protocol_display_name, protocol_category
        FROM device_details 
        ORDER BY name
    )";

const std::string FIND_BY_ID_WITH_PROTOCOL_INFO = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id, protocol_instance_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_id, endpoint, config, polling_interval, timeout, retry_count,
            is_enabled, installation_date, last_maintenance, 
            created_by, created_at, updated_at,
            protocol_type, protocol_display_name, protocol_category
        FROM device_details 
        WHERE id = ?
    )";

} // namespace Device

// =============================================================================
// 🎯 DataPointRepository 쿼리들 - 현재 스키마 완전 일치
// =============================================================================
namespace DataPoint {

// 🔥🔥🔥 FIND_ALL - 현재 스키마의 모든 필드 포함
const std::string FIND_ALL = R"(
        SELECT 
            -- 기본 식별 정보
            id, device_id, name, description, 
            
            -- 주소 정보  
            address, address_string, mapping_key,
            
            -- 데이터 타입 및 접근성
            data_type, access_mode, is_enabled, is_writable,
            
            -- 엔지니어링 단위 및 스케일링
            unit, scaling_factor, scaling_offset, min_value, max_value,
            
            -- 🔥 로깅 및 수집 설정
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            
            -- 🔥 품질 및 알람 설정
            quality_check_enabled, range_check_enabled, rate_of_change_limit,
            alarm_enabled, alarm_priority,
            
            -- 🔥 메타데이터
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
            address, address_string, mapping_key,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            quality_check_enabled, range_check_enabled, rate_of_change_limit,
            alarm_enabled, alarm_priority,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        FROM data_points 
        WHERE id = ?
    )";

const std::string FIND_BY_DEVICE_ID = R"(
        SELECT 
            id, device_id, name, description, 
            address, address_string, mapping_key,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            quality_check_enabled, range_check_enabled, rate_of_change_limit,
            alarm_enabled, alarm_priority,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        FROM data_points 
        WHERE device_id = ?
        ORDER BY address
    )";

const std::string FIND_BY_DEVICE_ID_ENABLED = R"(
        SELECT 
            id, device_id, name, description, 
            address, address_string, mapping_key,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            quality_check_enabled, range_check_enabled, rate_of_change_limit,
            alarm_enabled, alarm_priority,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        FROM data_points 
        WHERE device_id = ? AND is_enabled = 1
        ORDER BY address
    )";

const std::string FIND_BY_DEVICE_AND_ADDRESS = R"(
        SELECT 
            id, device_id, name, description, 
            address, address_string, mapping_key,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            quality_check_enabled, range_check_enabled, rate_of_change_limit,
            alarm_enabled, alarm_priority,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        FROM data_points 
        WHERE device_id = ? AND address = ?
        ORDER BY created_at DESC
        LIMIT 1
    )";

// 🔥🔥🔥 INSERT - 현재 스키마의 모든 필드
const std::string INSERT = R"(
        INSERT INTO data_points (
            device_id, name, description, 
            address, address_string, mapping_key,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            quality_check_enabled, range_check_enabled, rate_of_change_limit,
            alarm_enabled, alarm_priority,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

// 🔥🔥🔥 UPDATE - 현재 스키마의 모든 필드
const std::string UPDATE = R"(
        UPDATE data_points SET 
            device_id = ?, name = ?, description = ?, 
            address = ?, address_string = ?, mapping_key = ?,
            data_type = ?, access_mode = ?, is_enabled = ?, is_writable = ?,
            unit = ?, scaling_factor = ?, scaling_offset = ?, min_value = ?, max_value = ?,
            log_enabled = ?, log_interval_ms = ?, log_deadband = ?, polling_interval_ms = ?,
            quality_check_enabled = ?, range_check_enabled = ?, rate_of_change_limit = ?,
            alarm_enabled = ?, alarm_priority = ?,
            group_name = ?, tags = ?, metadata = ?, protocol_params = ?,
            updated_at = ?
        WHERE id = ?
    )";

// 기본 CRUD 작업
const std::string DELETE_BY_ID = "DELETE FROM data_points WHERE id = ?";
const std::string DELETE_BY_DEVICE_ID =
    "DELETE FROM data_points WHERE device_id = ?";
const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM data_points WHERE id = ?";
const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM data_points";
const std::string COUNT_BY_DEVICE =
    "SELECT COUNT(*) as count FROM data_points WHERE device_id = ?";

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

// 🔥🔥🔥 CREATE_TABLE - 현재 스키마 완전 반영
const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS data_points (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id INTEGER NOT NULL,
            
            -- 🔥 기본 식별 정보
            name VARCHAR(100) NOT NULL,
            description TEXT,
            
            -- 🔥 주소 정보
            address INTEGER NOT NULL,
            address_string VARCHAR(255),
            mapping_key VARCHAR(255),
            
            -- 🔥 데이터 타입 및 접근성
            data_type VARCHAR(20) NOT NULL DEFAULT 'UNKNOWN',
            access_mode VARCHAR(10) DEFAULT 'read',
            is_enabled INTEGER DEFAULT 1,
            is_writable INTEGER DEFAULT 0,
            
            -- 🔥 엔지니어링 단위 및 스케일링
            unit VARCHAR(50),
            scaling_factor REAL DEFAULT 1.0,
            scaling_offset REAL DEFAULT 0.0,
            min_value REAL DEFAULT 0.0,
            max_value REAL DEFAULT 0.0,
            
            -- 🔥🔥🔥 로깅 및 수집 설정
            log_enabled INTEGER DEFAULT 1,
            log_interval_ms INTEGER DEFAULT 0,
            log_deadband REAL DEFAULT 0.0,
            polling_interval_ms INTEGER DEFAULT 0,
            
            -- 🔥🔥🔥 품질 및 알람 설정 (추가됨)
            quality_check_enabled INTEGER DEFAULT 1,
            range_check_enabled INTEGER DEFAULT 1,
            rate_of_change_limit REAL DEFAULT 0.0,
            alarm_enabled INTEGER DEFAULT 0,
            alarm_priority VARCHAR(20) DEFAULT 'medium',
            
            -- 🔥🔥🔥 메타데이터
            group_name VARCHAR(50),
            tags TEXT,
            metadata TEXT,
            protocol_params TEXT,
            
            -- 🔥 시간 정보
            created_at DATETIME DEFAULT (datetime('now', 'localtime')),
            updated_at DATETIME DEFAULT (datetime('now', 'localtime')),
            
            FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
            UNIQUE(device_id, address),
            CONSTRAINT chk_data_type CHECK (data_type IN ('BOOL', 'INT8', 'UINT8', 'INT16', 'UINT16', 'INT32', 'UINT32', 'INT64', 'UINT64', 'FLOAT', 'DOUBLE', 'FLOAT32', 'FLOAT64', 'STRING', 'BINARY', 'DATETIME', 'JSON', 'ARRAY', 'OBJECT', 'UNKNOWN'))
        )
    )";

// 🔥🔥🔥 실시간 값 조회 (current_values 테이블과 JOIN)
const std::string FIND_WITH_CURRENT_VALUES = R"(
        SELECT 
            -- data_points 모든 필드
            dp.id, dp.device_id, dp.name, dp.description, 
            dp.address, dp.address_string, dp.mapping_key,
            dp.data_type, dp.access_mode, dp.is_enabled, dp.is_writable,
            dp.unit, dp.scaling_factor, dp.scaling_offset, dp.min_value, dp.max_value,
            dp.log_enabled, dp.log_interval_ms, dp.log_deadband, dp.polling_interval_ms,
            dp.quality_check_enabled, dp.range_check_enabled, dp.rate_of_change_limit,
            dp.alarm_enabled, dp.alarm_priority,
            dp.group_name, dp.tags, dp.metadata, dp.protocol_params,
            dp.created_at, dp.updated_at,
            
            -- current_values 테이블의 실시간 데이터
            cv.current_value, cv.raw_value, cv.value_type,
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

// 추가 쿼리들
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

const std::string DELETE_BY_DEVICE_IDS = R"(
        DELETE FROM data_points 
        WHERE device_id IN (%s)
    )";

const std::string UPSERT = R"(
        INSERT OR REPLACE INTO data_points (
            id, device_id, name, description, address, address_string,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

const std::string UPDATE_BASIC_INFO = R"(
        UPDATE data_points SET 
            name = ?, description = ?, unit = ?, 
            scaling_factor = ?, scaling_offset = ?,
            min_value = ?, max_value = ?,
            updated_at = ?
        WHERE id = ?
    )";

const std::string UPDATE_STATUS = R"(
        UPDATE data_points SET 
            is_enabled = ?, updated_at = ?
        WHERE id = ?
    )";

const std::string UPDATE_LOG_CONFIG = R"(
        UPDATE data_points SET 
            log_enabled = ?, log_interval_ms = ?, log_deadband = ?,
            updated_at = ?
        WHERE id = ?
    )";

const std::string BULK_UPDATE_STATUS = R"(
        UPDATE data_points SET 
            is_enabled = ?, updated_at = ?
        WHERE id IN (%s)
    )";

// 통계 관련
const std::string COUNT_ENABLED =
    "SELECT COUNT(*) as count FROM data_points WHERE is_enabled = 1";
const std::string COUNT_DISABLED =
    "SELECT COUNT(*) as count FROM data_points WHERE is_enabled = 0";
const std::string COUNT_WRITABLE =
    "SELECT COUNT(*) as count FROM data_points WHERE is_writable = 1";
const std::string COUNT_LOG_ENABLED =
    "SELECT COUNT(*) as count FROM data_points WHERE log_enabled = 1";

const std::string GET_STATS_BY_DEVICE = R"(
        SELECT 
            device_id,
            COUNT(*) as total_count,
            COUNT(CASE WHEN is_enabled = 1 THEN 1 END) as enabled_count,
            COUNT(CASE WHEN is_writable = 1 THEN 1 END) as writable_count,
            COUNT(CASE WHEN log_enabled = 1 THEN 1 END) as log_enabled_count
        FROM data_points 
        GROUP BY device_id
        ORDER BY device_id
    )";

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

const std::string EXPORT_FOR_BACKUP = R"(
        SELECT 
            device_id, name, description, address, address_string, mapping_key,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            group_name, tags, metadata, protocol_params
        FROM data_points 
        WHERE device_id IN (%s)
        ORDER BY device_id, address
    )";

const std::string COPY_TO_DEVICE = R"(
        INSERT INTO data_points (
            device_id, name, description, address, address_string, mapping_key,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        )
        SELECT 
            ? as device_id, name, description, address, address_string, mapping_key,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            group_name, tags, metadata, protocol_params,
            datetime('now', 'localtime') as created_at, datetime('now', 'localtime') as updated_at
        FROM data_points 
        WHERE device_id = ?
    )";

const std::string FIND_LAST_CREATED_BY_DEVICE_ADDRESS = R"(
        SELECT id FROM data_points 
        WHERE device_id = ? AND address = ?
        ORDER BY created_at DESC LIMIT 1
    )";

const std::string FIND_WRITABLE_POINTS = R"(
        SELECT 
            id, device_id, name, description, address, address_string,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        FROM data_points 
        WHERE is_writable = 1 AND is_enabled = 1
        ORDER BY device_id, address
    )";

const std::string FIND_BY_DATA_TYPE = R"(
        SELECT 
            id, device_id, name, description, address, address_string,
            data_type, access_mode, is_enabled, is_writable,
            unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
            group_name, tags, metadata, protocol_params,
            created_at, updated_at
        FROM data_points 
        WHERE data_type = ? AND is_enabled = 1
        ORDER BY device_id, address
    )";

} // namespace DataPoint

// =============================================================================
// 🎯 DeviceSettingsRepository 쿼리들 - 현재 스키마 완전 반영
// =============================================================================
namespace DeviceSettings {

const std::string FIND_ALL = R"(
        SELECT 
            device_id, polling_interval_ms, scan_rate_override, 
            connection_timeout_ms, read_timeout_ms, write_timeout_ms,
            max_retry_count, retry_interval_ms, backoff_multiplier,
            backoff_time_ms, max_backoff_time_ms,
            keep_alive_enabled, keep_alive_interval_s, keep_alive_timeout_s,
            data_validation_enabled, outlier_detection_enabled, deadband_enabled,
            detailed_logging_enabled, performance_monitoring_enabled, diagnostic_mode_enabled, auto_registration_enabled,
            created_at, updated_at, updated_by
        FROM device_settings 
        ORDER BY device_id
    )";

const std::string FIND_BY_ID = R"(
        SELECT 
            device_id, polling_interval_ms, scan_rate_override, 
            connection_timeout_ms, read_timeout_ms, write_timeout_ms,
            max_retry_count, retry_interval_ms, backoff_multiplier,
            backoff_time_ms, max_backoff_time_ms,
            keep_alive_enabled, keep_alive_interval_s, keep_alive_timeout_s,
            data_validation_enabled, outlier_detection_enabled, deadband_enabled,
            detailed_logging_enabled, performance_monitoring_enabled, diagnostic_mode_enabled, auto_registration_enabled,
            created_at, updated_at, updated_by
        FROM device_settings 
        WHERE device_id = ?
    )";

const std::string FIND_BY_PROTOCOL = R"(
        SELECT 
            ds.device_id, ds.polling_interval_ms, ds.scan_rate_override, 
            ds.connection_timeout_ms, ds.read_timeout_ms, ds.write_timeout_ms,
            ds.max_retry_count, ds.retry_interval_ms, ds.backoff_multiplier,
            ds.backoff_time_ms, ds.max_backoff_time_ms,
            ds.keep_alive_enabled, ds.keep_alive_interval_s, ds.keep_alive_timeout_s,
            ds.data_validation_enabled, ds.outlier_detection_enabled, ds.deadband_enabled,
            ds.detailed_logging_enabled, ds.performance_monitoring_enabled, ds.diagnostic_mode_enabled, ds.auto_registration_enabled,
            ds.created_at, ds.updated_at, ds.updated_by
        FROM device_settings ds
        INNER JOIN devices d ON ds.device_id = d.id
        INNER JOIN protocols p ON d.protocol_id = p.id
        WHERE p.protocol_type = ?
        ORDER BY ds.device_id
    )";

const std::string FIND_ACTIVE_DEVICES = R"(
        SELECT 
            ds.device_id, ds.polling_interval_ms, ds.scan_rate_override, 
            ds.connection_timeout_ms, ds.read_timeout_ms, ds.write_timeout_ms,
            ds.max_retry_count, ds.retry_interval_ms, ds.backoff_multiplier,
            ds.backoff_time_ms, ds.max_backoff_time_ms,
            ds.keep_alive_enabled, ds.keep_alive_interval_s, ds.keep_alive_timeout_s,
            ds.data_validation_enabled, ds.outlier_detection_enabled, ds.deadband_enabled,
            ds.detailed_logging_enabled, ds.performance_monitoring_enabled, ds.diagnostic_mode_enabled, ds.auto_registration_enabled,
            ds.created_at, ds.updated_at, ds.updated_by
        FROM device_settings ds
        INNER JOIN devices d ON ds.device_id = d.id
        WHERE d.is_enabled = 1
        ORDER BY ds.polling_interval_ms, ds.device_id
    )";

// 🔥🔥🔥 UPSERT - 현재 스키마의 모든 필드
const std::string UPSERT = R"(
        INSERT OR REPLACE INTO device_settings (
            device_id, polling_interval_ms, scan_rate_override, 
            connection_timeout_ms, read_timeout_ms, write_timeout_ms,
            max_retry_count, retry_interval_ms, backoff_multiplier,
            backoff_time_ms, max_backoff_time_ms,
            keep_alive_enabled, keep_alive_interval_s, keep_alive_timeout_s,
            data_validation_enabled, outlier_detection_enabled, deadband_enabled,
            detailed_logging_enabled, performance_monitoring_enabled, diagnostic_mode_enabled, auto_registration_enabled,
            created_at, updated_at, updated_by
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
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

const std::string UPDATE_AUTO_REGISTRATION_ENABLED = R"(
        UPDATE device_settings 
        SET auto_registration_enabled = ?, updated_at = ?
        WHERE device_id = ?
    )";

const std::string DELETE_BY_ID =
    "DELETE FROM device_settings WHERE device_id = ?";

const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM device_settings WHERE device_id = ?";

const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM device_settings";

const std::string GET_POLLING_INTERVAL_DISTRIBUTION = R"(
        SELECT polling_interval_ms, COUNT(*) as count 
        FROM device_settings 
        GROUP BY polling_interval_ms
        ORDER BY polling_interval_ms
    )";

// 🔥🔥🔥 CREATE_TABLE - schema.sql과 완전 동기화 (2025-08-26 업데이트)
const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS device_settings (
            device_id INTEGER PRIMARY KEY,
            
            -- 폴링 및 타이밍 설정
            polling_interval_ms INTEGER DEFAULT 1000,
            scan_rate_override INTEGER, -- 개별 디바이스 스캔 주기 오버라이드
            scan_group INTEGER DEFAULT 1, -- 스캔 그룹 (동시 스캔 제어)
            
            -- 연결 및 통신 설정
            connection_timeout_ms INTEGER DEFAULT 10000,
            read_timeout_ms INTEGER DEFAULT 5000,
            write_timeout_ms INTEGER DEFAULT 5000,
            inter_frame_delay_ms INTEGER DEFAULT 10, -- 프레임 간 지연
            
            -- 재시도 정책
            max_retry_count INTEGER DEFAULT 3,
            retry_interval_ms INTEGER DEFAULT 5000,
            backoff_multiplier DECIMAL(3,2) DEFAULT 1.5, -- 지수 백오프
            backoff_time_ms INTEGER DEFAULT 60000,
            max_backoff_time_ms INTEGER DEFAULT 300000, -- 최대 5분
            
            -- Keep-alive 설정
            keep_alive_enabled INTEGER DEFAULT 1,
            keep_alive_interval_s INTEGER DEFAULT 30,
            keep_alive_timeout_s INTEGER DEFAULT 10,
            
            -- 데이터 품질 관리
            data_validation_enabled INTEGER DEFAULT 1,
            performance_monitoring_enabled INTEGER DEFAULT 1,
            outlier_detection_enabled INTEGER DEFAULT 0,
            deadband_enabled INTEGER DEFAULT 1,
            
            -- 로깅 및 진단
            detailed_logging_enabled INTEGER DEFAULT 0,
            diagnostic_mode_enabled INTEGER DEFAULT 0,
            communication_logging_enabled INTEGER DEFAULT 0, -- 통신 로그 기록
            auto_registration_enabled INTEGER DEFAULT 0,
            
            -- 버퍼링 설정
            read_buffer_size INTEGER DEFAULT 1024,
            write_buffer_size INTEGER DEFAULT 1024,
            queue_size INTEGER DEFAULT 100, -- 명령 큐 크기
            
            -- 메타데이터
            created_at DATETIME DEFAULT (datetime('now', 'localtime')),
            updated_at DATETIME DEFAULT (datetime('now', 'localtime')),
            updated_by INTEGER,
            
            FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
        )
    )";

} // namespace DeviceSettings

// =============================================================================
// 🎯 CurrentValueRepository 쿼리들 - 현재 스키마 완전 반영
// =============================================================================
namespace CurrentValue {

// 🔥🔥🔥 CREATE_TABLE - 현재 스키마 완전 반영
const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS current_values (
            point_id INTEGER PRIMARY KEY,
            -- 🔥 실제 값 (DataVariant 직렬화)
            current_value TEXT, -- JSON으로 DataVariant 저장
            raw_value TEXT, -- JSON으로 원시값 저장
            previous_value TEXT, -- JSON으로 이전값 저장
            -- 🔥 데이터 타입 정보
            value_type VARCHAR(10) DEFAULT 'double', -- bool, int16, uint16, int32, uint32, float, double, string
            -- 🔥 데이터 품질 및 상태
            quality_code INTEGER DEFAULT 0, -- DataQuality enum 값
            quality VARCHAR(20) DEFAULT 'not_connected', -- 텍스트 표현 (good, bad, uncertain, not_connected)
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
            change_count INTEGER DEFAULT 0, -- 값 변경 횟수
            -- 🔥 알람 상태
            alarm_state VARCHAR(20) DEFAULT 'normal', -- normal, high, low, critical
            alarm_active INTEGER DEFAULT 0,
            alarm_acknowledged INTEGER DEFAULT 0,
            -- 🔥 메타데이터
            source_info TEXT, -- JSON: 값 소스 정보
            updated_at DATETIME DEFAULT (datetime('now', 'localtime')),
            
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

const std::string DELETE_BY_ID =
    "DELETE FROM current_values WHERE point_id = ?";

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

const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM current_values WHERE point_id = ?";

const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM current_values";

const std::string COUNT_BY_QUALITY =
    "SELECT COUNT(*) as count FROM current_values WHERE quality = ?";

const std::string COUNT_BY_QUALITY_CODE =
    "SELECT COUNT(*) as count FROM current_values WHERE quality_code = ?";

const std::string GET_LATEST_VALUES = R"(
        SELECT 
            cv.point_id, dp.name as point_name, cv.current_value, cv.quality, cv.value_timestamp,
            d.name as device_name, p.protocol_type
        FROM current_values cv
        JOIN data_points dp ON cv.point_id = dp.id
        JOIN devices d ON dp.device_id = d.id
        JOIN protocols p ON d.protocol_id = p.id
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
// 🎯 ProtocolRepository 쿼리들 - 새로 추가된 protocols 테이블
// =============================================================================
namespace Protocol {
// 기본 CRUD 쿼리들 (기존)
const std::string FIND_ALL = R"(
        SELECT 
            id, protocol_type, display_name, description,
            default_port, uses_serial, requires_broker,
            supported_operations, supported_data_types, connection_params,
            default_polling_interval, default_timeout, max_concurrent_connections,
            is_enabled, is_deprecated, min_firmware_version,
            category, vendor, standard_reference,
            created_at, updated_at
        FROM protocols 
        ORDER BY display_name
    )";

const std::string FIND_BY_ID = R"(
        SELECT 
            id, protocol_type, display_name, description,
            default_port, uses_serial, requires_broker,
            supported_operations, supported_data_types, connection_params,
            default_polling_interval, default_timeout, max_concurrent_connections,
            is_enabled, is_deprecated, min_firmware_version,
            category, vendor, standard_reference,
            created_at, updated_at
        FROM protocols 
        WHERE id = ?
    )";

// 🔥 누락된 상수들 추가
const std::string FIND_BY_TYPE = R"(
        SELECT 
            id, protocol_type, display_name, description,
            default_port, uses_serial, requires_broker,
            supported_operations, supported_data_types, connection_params,
            default_polling_interval, default_timeout, max_concurrent_connections,
            is_enabled, is_deprecated, min_firmware_version,
            category, vendor, standard_reference,
            created_at, updated_at
        FROM protocols 
        WHERE protocol_type = ?
    )";

const std::string FIND_ACTIVE = R"(
        SELECT 
            id, protocol_type, display_name, description,
            default_port, uses_serial, requires_broker,
            supported_operations, supported_data_types, connection_params,
            default_polling_interval, default_timeout, max_concurrent_connections,
            is_enabled, is_deprecated, min_firmware_version,
            category, vendor, standard_reference,
            created_at, updated_at
        FROM protocols 
        WHERE is_enabled = 1
        ORDER BY category, display_name
    )";

const std::string FIND_SERIAL = R"(
        SELECT 
            id, protocol_type, display_name, description,
            default_port, uses_serial, requires_broker,
            supported_operations, supported_data_types, connection_params,
            default_polling_interval, default_timeout, max_concurrent_connections,
            is_enabled, is_deprecated, min_firmware_version,
            category, vendor, standard_reference,
            created_at, updated_at
        FROM protocols 
        WHERE uses_serial = 1 AND is_enabled = 1
        ORDER BY display_name
    )";

const std::string FIND_BROKER_REQUIRED = R"(
        SELECT 
            id, protocol_type, display_name, description,
            default_port, uses_serial, requires_broker,
            supported_operations, supported_data_types, connection_params,
            default_polling_interval, default_timeout, max_concurrent_connections,
            is_enabled, is_deprecated, min_firmware_version,
            category, vendor, standard_reference,
            created_at, updated_at
        FROM protocols 
        WHERE requires_broker = 1 AND is_enabled = 1
        ORDER BY display_name
    )";

const std::string FIND_BY_PORT = R"(
        SELECT 
            id, protocol_type, display_name, description,
            default_port, uses_serial, requires_broker,
            supported_operations, supported_data_types, connection_params,
            default_polling_interval, default_timeout, max_concurrent_connections,
            is_enabled, is_deprecated, min_firmware_version,
            category, vendor, standard_reference,
            created_at, updated_at
        FROM protocols 
        WHERE default_port = ? AND is_enabled = 1
        ORDER BY display_name
    )";

const std::string GET_CATEGORY_DISTRIBUTION = R"(
        SELECT 
            category,
            COUNT(*) as count
        FROM protocols 
        WHERE is_enabled = 1
        GROUP BY category
        ORDER BY count DESC, category
    )";

const std::string FIND_DEPRECATED = R"(
        SELECT 
            id, protocol_type, display_name, description,
            default_port, uses_serial, requires_broker,
            supported_operations, supported_data_types, connection_params,
            default_polling_interval, default_timeout, max_concurrent_connections,
            is_enabled, is_deprecated, min_firmware_version,
            category, vendor, standard_reference,
            created_at, updated_at
        FROM protocols 
        WHERE is_deprecated = 1
        ORDER BY display_name
    )";

const std::string GET_API_LIST = R"(
        SELECT 
            protocol_type,
            display_name,
            description
        FROM protocols 
        WHERE is_enabled = 1 AND is_deprecated = 0
        ORDER BY display_name
    )";

// 기존 쿼리들 (이미 있던 것들)
const std::string FIND_ENABLED = R"(
        SELECT 
            id, protocol_type, display_name, description,
            default_port, uses_serial, requires_broker,
            supported_operations, supported_data_types, connection_params,
            default_polling_interval, default_timeout, max_concurrent_connections,
            is_enabled, is_deprecated, min_firmware_version,
            category, vendor, standard_reference,
            created_at, updated_at
        FROM protocols 
        WHERE is_enabled = 1 AND is_deprecated = 0
        ORDER BY category, display_name
    )";

const std::string FIND_BY_CATEGORY = R"(
        SELECT 
            id, protocol_type, display_name, description,
            default_port, uses_serial, requires_broker,
            supported_operations, supported_data_types, connection_params,
            default_polling_interval, default_timeout, max_concurrent_connections,
            is_enabled, is_deprecated, min_firmware_version,
            category, vendor, standard_reference,
            created_at, updated_at
        FROM protocols 
        WHERE category = ? AND is_enabled = 1
        ORDER BY display_name
    )";

const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS protocols (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            
            -- 기본 정보
            protocol_type VARCHAR(50) NOT NULL UNIQUE,
            display_name VARCHAR(100) NOT NULL,
            description TEXT,
            
            -- 네트워크 정보
            default_port INTEGER,
            uses_serial INTEGER DEFAULT 0,
            requires_broker INTEGER DEFAULT 0,
            
            -- 기능 지원 정보 (JSON)
            supported_operations TEXT,
            supported_data_types TEXT,
            connection_params TEXT,
            
            -- 설정 정보
            default_polling_interval INTEGER DEFAULT 1000,
            default_timeout INTEGER DEFAULT 5000,
            max_concurrent_connections INTEGER DEFAULT 1,
            
            -- 상태 정보
            is_enabled INTEGER DEFAULT 1,
            is_deprecated INTEGER DEFAULT 0,
            min_firmware_version VARCHAR(20),
            
            -- 분류 정보
            category VARCHAR(50),
            vendor VARCHAR(100),
            standard_reference VARCHAR(100),
            
            -- 메타데이터
            created_at DATETIME DEFAULT (datetime('now', 'localtime')),
            updated_at DATETIME DEFAULT (datetime('now', 'localtime')),
            
            -- 제약조건
            CONSTRAINT chk_category CHECK (category IN ('industrial', 'iot', 'building_automation', 'network', 'web'))
        )
    )";

const std::string INSERT = R"(
        INSERT INTO protocols (
            protocol_type, display_name, description,
            default_port, uses_serial, requires_broker,
            supported_operations, supported_data_types, connection_params,
            default_polling_interval, default_timeout, max_concurrent_connections,
            is_enabled, is_deprecated, min_firmware_version,
            category, vendor, standard_reference,
            created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

const std::string UPDATE = R"(
        UPDATE protocols SET 
            protocol_type = ?, display_name = ?, description = ?,
            default_port = ?, uses_serial = ?, requires_broker = ?,
            supported_operations = ?, supported_data_types = ?, connection_params = ?,
            default_polling_interval = ?, default_timeout = ?, max_concurrent_connections = ?,
            is_enabled = ?, is_deprecated = ?, min_firmware_version = ?,
            category = ?, vendor = ?, standard_reference = ?,
            updated_at = ?
        WHERE id = ?
    )";

const std::string DELETE_BY_ID = "DELETE FROM protocols WHERE id = ?";

const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM protocols WHERE id = ?";

const std::string EXISTS_BY_TYPE =
    "SELECT COUNT(*) as count FROM protocols WHERE protocol_type = ?";

const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM protocols";

const std::string COUNT_ENABLED =
    "SELECT COUNT(*) as count FROM protocols WHERE is_enabled = 1";

const std::string GET_CATEGORIES = R"(
        SELECT DISTINCT category 
        FROM protocols 
        WHERE category IS NOT NULL AND is_enabled = 1
        ORDER BY category
    )";

const std::string GET_LAST_INSERT_ID = "SELECT last_insert_rowid() as id";

} // namespace Protocol

// =============================================================================
// 🎯 동적 쿼리 빌더 헬퍼들
// =============================================================================
namespace QueryBuilder {

/**
 * @brief WHERE 절 생성 헬퍼
 * @param conditions 조건들 (column=value 형태)
 * @return WHERE 절 문자열
 */
inline std::string buildWhereClause(
    const std::vector<std::pair<std::string, std::string>> &conditions) {
  if (conditions.empty())
    return "";

  std::string where_clause = " WHERE ";
  for (size_t i = 0; i < conditions.size(); ++i) {
    if (i > 0)
      where_clause += " AND ";
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
inline std::string buildOrderByClause(const std::string &column,
                                      const std::string &direction = "ASC") {
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
    return " LIMIT " + std::to_string(limit) + " OFFSET " +
           std::to_string(offset);
  } else {
    return " LIMIT " + std::to_string(limit);
  }
}

/**
 * @brief IN 절용 플레이스홀더 생성
 * @param count 항목 개수
 * @return IN 절 플레이스홀더 문자열 (?,?,?)
 */
inline std::string buildInPlaceholders(int count) {
  if (count <= 0)
    return "";

  std::string placeholders = "?";
  for (int i = 1; i < count; ++i) {
    placeholders += ",?";
  }
  return placeholders;
}

} // namespace QueryBuilder

// =============================================================================
// 🎯 EdgeServerRepository 쿼리들 - 컬렉터 아이덴티티 관리
// =============================================================================
namespace EdgeServer {
const std::string FIND_ALL = R"(
        SELECT id, tenant_id, server_name as name, factory_name, location,
               ip_address, port, registration_token, status,
               last_seen, version, created_at, updated_at, subscription_mode
        FROM edge_servers
        WHERE is_deleted = 0
        ORDER BY id
    )";

const std::string FIND_BY_ID = R"(
        SELECT id, tenant_id, server_name as name, factory_name, location,
               ip_address, port, registration_token, status,
               last_seen, version, created_at, updated_at, subscription_mode,
               config
        FROM edge_servers
        WHERE id = ?
    )";

const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS edge_servers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER NOT NULL,
            name VARCHAR(100) NOT NULL,
            description TEXT,
            ip_address VARCHAR(45),
            port INTEGER DEFAULT 50051,
            is_enabled INTEGER DEFAULT 1,
            max_devices INTEGER DEFAULT 100,
            max_data_points INTEGER DEFAULT 1000,
            created_at DATETIME DEFAULT (datetime('now', 'localtime')),
            updated_at DATETIME DEFAULT (datetime('now', 'localtime')),
            FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
        )
    )";
} // namespace EdgeServer

// =============================================================================
// 🗓️ DeviceSchedule — device_schedules 테이블 CRUD (DeviceScheduleRepository)
// =============================================================================
namespace DeviceSchedule {

const std::string SELECT_COLUMNS =
    "SELECT id, device_id, point_id, schedule_type, schedule_data, "
    "is_synced, last_sync_time, created_at, updated_at "
    "FROM device_schedules";

const std::string FIND_ALL = SELECT_COLUMNS;

const std::string FIND_PENDING_SYNC = SELECT_COLUMNS + " WHERE is_synced = 0";

// 파라미터 포함 쿼리 (id/device_id 값으로 조립)
inline std::string FIND_BY_ID(int id) {
  return SELECT_COLUMNS + " WHERE id = " + std::to_string(id);
}

inline std::string FIND_BY_DEVICE_ID(int device_id) {
  return SELECT_COLUMNS + " WHERE device_id = " + std::to_string(device_id);
}

inline std::string FIND_PENDING_BY_DEVICE(int device_id) {
  return SELECT_COLUMNS +
         " WHERE is_synced = 0 AND device_id = " + std::to_string(device_id);
}

// device_id, point_id(nullable), schedule_type, schedule_data, is_synced
inline std::string INSERT(int device_id, const std::string &point_id_or_null,
                          const std::string &schedule_type,
                          const std::string &schedule_data, bool is_synced) {
  return "INSERT INTO device_schedules "
         "(device_id, point_id, schedule_type, schedule_data, is_synced) "
         "VALUES (" +
         std::to_string(device_id) + ", " + point_id_or_null + ", '" +
         schedule_type + "', '" + schedule_data + "', " +
         (is_synced ? "1" : "0") + ")";
}

inline std::string UPDATE(int id, int device_id,
                          const std::string &point_id_or_null,
                          const std::string &schedule_type,
                          const std::string &schedule_data, bool is_synced) {
  return "UPDATE device_schedules SET "
         "device_id = " +
         std::to_string(device_id) + ", point_id = " + point_id_or_null +
         ", schedule_type = '" + schedule_type + "', schedule_data = '" +
         schedule_data + "', is_synced = " + (is_synced ? "1" : "0") +
         ", updated_at = (datetime('now', 'localtime')) "
         "WHERE id = " +
         std::to_string(id);
}

inline std::string DELETE_BY_ID(int id) {
  return "DELETE FROM device_schedules WHERE id = " + std::to_string(id);
}

// is_synced = 1, last_sync_time = now 업데이트
inline std::string MARK_SYNCED(int id) {
  return "UPDATE device_schedules SET is_synced = 1, "
         "last_sync_time = (datetime('now', 'localtime')) "
         "WHERE id = " +
         std::to_string(id);
}

} // namespace DeviceSchedule

// =============================================================================
// ⚙️ SystemSettings — system_settings 테이블 CRUD (SystemSettingsRepository)
// =============================================================================
namespace SystemSettings {

const std::string SELECT_COLUMNS =
    "SELECT id, key_name, value, category FROM system_settings";

const std::string FIND_ALL = SELECT_COLUMNS + " ORDER BY key_name ASC";

inline std::string FIND_BY_ID(int id) {
  return SELECT_COLUMNS + " WHERE id = " + std::to_string(id);
}

inline std::string FIND_BY_KEY(const std::string &key) {
  return SELECT_COLUMNS + " WHERE key_name = '" + key + "'";
}

inline std::string COUNT_BY_ID(int id) {
  return "SELECT COUNT(*) as count FROM system_settings WHERE id = " +
         std::to_string(id);
}

// 🔧 NOW() → datetime('now', 'localtime') — adaptQuery()가 DB별 자동 변환
// (SQLite: datetime, PG: NOW(), MySQL: NOW() 등으로 각각 치환됨)
inline std::string UPDATE_VALUE(const std::string &key,
                                const std::string &value) {
  return "UPDATE system_settings SET value = '" + value +
         "', updated_at = (datetime('now', 'localtime')) "
         "WHERE key_name = '" +
         key + "'";
}

inline std::string INSERT_VALUE(const std::string &key,
                                const std::string &value) {
  return "INSERT INTO system_settings (key_name, value, updated_at) "
         "VALUES ('" +
         key + "', '" + value + "', (datetime('now', 'localtime')))";
}

inline std::string DELETE_BY_ID(int id) {
  return "DELETE FROM system_settings WHERE id = " + std::to_string(id);
}

} // namespace SystemSettings

} // namespace SQL
} // namespace Database
} // namespace PulseOne

#endif // SQL_QUERIES_H
