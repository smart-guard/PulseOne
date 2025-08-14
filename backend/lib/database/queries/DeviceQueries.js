// =============================================================================
// backend/lib/database/DeviceQueries.js
// 디바이스 관리 통합 SQL 쿼리 모음 - 스키마와 완전 일치하도록 수정됨
// =============================================================================

class DeviceQueries {
  // =============================================================================
  // 디바이스 기본 조회 쿼리들
  // =============================================================================

  // 디바이스 목록 조회 (모든 관련 정보 포함)
  static getDevicesWithAllInfo() {
    return `
      SELECT 
        d.id,
        d.tenant_id,
        d.site_id,
        d.device_group_id,
        d.edge_server_id,
        d.name,
        d.description,
        d.device_type,
        d.manufacturer,
        d.model,
        d.serial_number,
        d.protocol_type,
        d.endpoint,
        d.config,
        d.polling_interval,
        d.timeout,
        d.retry_count,
        d.is_enabled,
        d.installation_date,
        d.last_maintenance,
        d.created_at,
        d.updated_at,
        
        -- 디바이스 설정
        ds.polling_interval_ms,
        ds.connection_timeout_ms,
        ds.max_retry_count,
        ds.retry_interval_ms,
        ds.backoff_time_ms,
        ds.keep_alive_enabled,
        ds.keep_alive_interval_s,
        ds.updated_at as settings_updated_at,
        
        -- 🔥 디바이스 상태 (실제 스키마와 일치하도록 수정)
        dst.connection_status,  -- status -> connection_status로 수정
        dst.last_communication, -- last_seen -> last_communication으로 수정
        dst.error_count,        -- 새로 추가
        dst.last_error,
        dst.response_time,
        dst.firmware_version,
        dst.hardware_info,
        dst.diagnostic_data,
        dst.updated_at as status_updated_at,
        
        -- 사이트 정보
        s.name as site_name,
        s.code as site_code,
        
        -- 그룹 정보
        dg.name as group_name,
        dg.group_type,
        
        -- 통계
        COUNT(dp.id) as data_point_count,
        SUM(CASE WHEN dp.is_enabled = 1 THEN 1 ELSE 0 END) as enabled_point_count
        
      FROM devices d
      LEFT JOIN device_settings ds ON d.id = ds.device_id
      LEFT JOIN device_status dst ON d.id = dst.device_id
      LEFT JOIN sites s ON d.site_id = s.id
      LEFT JOIN device_groups dg ON d.device_group_id = dg.id
      LEFT JOIN data_points dp ON d.id = dp.device_id
      WHERE 1=1
    `;
  }

  // 필터 조건들
  static addTenantFilter() {
    return ` AND d.tenant_id = ?`;
  }

  static addSiteFilter() {
    return ` AND d.site_id = ?`;
  }

  static addDeviceGroupFilter() {
    return ` AND d.device_group_id = ?`;
  }

  static addProtocolTypeFilter() {
    return ` AND d.protocol_type = ?`;
  }

  static addDeviceTypeFilter() {
    return ` AND d.device_type = ?`;
  }

  static addEnabledFilter() {
    return ` AND d.is_enabled = ?`;
  }

  static addDeviceIdFilter() {
    return ` AND d.id = ?`;
  }

  // 🔥 수정: connection_status 필드로 수정
  static addConnectionStatusFilter() {
    return ` AND dst.connection_status = ?`;
  }

  static addSearchFilter() {
    return ` AND (d.name LIKE ? OR d.description LIKE ? OR d.manufacturer LIKE ? OR d.model LIKE ?)`;
  }

  // 그룹화 및 정렬
  static getGroupByAndOrder() {
    return ` GROUP BY d.id ORDER BY d.name`;
  }

  static addLimit() {
    return ` LIMIT ?`;
  }

  // =============================================================================
  // 디바이스 CRUD 쿼리들
  // =============================================================================

  // 디바이스 생성
  static createDevice() {
    return `
      INSERT INTO devices (
        tenant_id, site_id, device_group_id, edge_server_id,
        name, description, device_type, manufacturer, model, serial_number,
        protocol_type, endpoint, config, polling_interval, timeout, retry_count,
        is_enabled, installation_date, created_by
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `;
  }

  // 디바이스 업데이트
  static updateDevice() {
    return `
      UPDATE devices SET
        name = ?, description = ?, device_type = ?, manufacturer = ?, model = ?,
        serial_number = ?, protocol_type = ?, endpoint = ?, config = ?,
        polling_interval = ?, timeout = ?, retry_count = ?, is_enabled = ?,
        installation_date = ?, last_maintenance = ?, updated_at = CURRENT_TIMESTAMP
      WHERE id = ?
    `;
  }

  // 디바이스 삭제
  static deleteDevice() {
    return `DELETE FROM devices WHERE id = ?`;
  }

  // =============================================================================
  // 디바이스 설정 쿼리들
  // =============================================================================

  // 디바이스 설정 UPSERT
  static upsertDeviceSettings() {
    return `
      INSERT OR REPLACE INTO device_settings (
        device_id, polling_interval_ms, connection_timeout_ms,
        max_retry_count, retry_interval_ms, backoff_time_ms,
        keep_alive_enabled, keep_alive_interval_s, updated_at
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
    `;
  }

  // 디바이스 설정 조회
  static getDeviceSettings() {
    return `SELECT * FROM device_settings WHERE device_id = ?`;
  }

  // 디바이스 설정 삭제
  static deleteDeviceSettings() {
    return `DELETE FROM device_settings WHERE device_id = ?`;
  }

  // 설정이 없는 디바이스들에 기본 설정 생성
  static createDefaultSettings() {
    return `
      INSERT INTO device_settings (device_id)
      SELECT id FROM devices 
      WHERE tenant_id = ? AND id NOT IN (SELECT device_id FROM device_settings)
    `;
  }

  // =============================================================================
  // 🔥 디바이스 상태 쿼리들 (스키마와 일치하도록 수정)
  // =============================================================================

  // 디바이스 상태 UPSERT
  static upsertDeviceStatus() {
    return `
      INSERT OR REPLACE INTO device_status (
        device_id, connection_status, last_communication, error_count, last_error, 
        response_time, firmware_version, hardware_info, diagnostic_data, updated_at
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
    `;
  }

  // 디바이스 상태 조회
  static getDeviceStatus() {
    return `SELECT * FROM device_status WHERE device_id = ?`;
  }

  // 🔥 수정: connection_status = 'connected'로 온라인 디바이스 카운트
  static getOnlineDeviceCount() {
    return `
      SELECT COUNT(*) as online_count
      FROM device_status dst
      INNER JOIN devices d ON dst.device_id = d.id
      WHERE d.tenant_id = ? AND dst.connection_status = 'connected'
    `;
  }

  // =============================================================================
  // 데이터 포인트 쿼리들
  // =============================================================================

  // 디바이스의 데이터 포인트 목록
  static getDataPointsByDevice() {
    return `
      SELECT 
        dp.id,
        dp.device_id,
        dp.name,
        dp.description,
        dp.address,
        dp.address_string,
        dp.data_type,
        dp.access_mode,
        dp.is_enabled,
        dp.is_writable,
        dp.unit,
        dp.scaling_factor,
        dp.scaling_offset,
        dp.min_value,
        dp.max_value,
        dp.log_enabled,
        dp.log_interval_ms,
        dp.log_deadband,
        dp.polling_interval_ms,
        dp.group_name,
        dp.tags,
        dp.metadata,
        dp.protocol_params,
        dp.created_at,
        dp.updated_at,
        
        -- 현재값 정보
        cv.current_value,
        cv.raw_value,
        cv.value_type,
        cv.quality_code,
        cv.quality,
        cv.value_timestamp,
        cv.quality_timestamp,
        cv.last_log_time,
        cv.last_read_time,
        cv.last_write_time,
        cv.read_count,
        cv.write_count,
        cv.error_count,
        cv.updated_at as value_updated_at
        
      FROM data_points dp
      LEFT JOIN current_values cv ON dp.id = cv.point_id
      WHERE dp.device_id = ?
      ORDER BY dp.address
    `;
  }

  // 데이터 포인트 생성
  static createDataPoint() {
    return `
      INSERT INTO data_points (
        device_id, name, description, address, address_string,
        data_type, access_mode, is_enabled, is_writable,
        unit, scaling_factor, scaling_offset, min_value, max_value,
        log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
        group_name, tags, metadata, protocol_params
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `;
  }

  // 데이터 포인트 업데이트
  static updateDataPoint() {
    return `
      UPDATE data_points SET
        name = ?, description = ?, address = ?, address_string = ?,
        data_type = ?, access_mode = ?, is_enabled = ?, is_writable = ?,
        unit = ?, scaling_factor = ?, scaling_offset = ?, min_value = ?, max_value = ?,
        log_enabled = ?, log_interval_ms = ?, log_deadband = ?, polling_interval_ms = ?,
        group_name = ?, tags = ?, metadata = ?, protocol_params = ?,
        updated_at = CURRENT_TIMESTAMP
      WHERE id = ?
    `;
  }

  // 데이터 포인트 삭제
  static deleteDataPoint() {
    return `DELETE FROM data_points WHERE id = ?`;
  }

  // 디바이스의 모든 데이터 포인트 삭제
  static deleteAllDataPointsByDevice() {
    return `DELETE FROM data_points WHERE device_id = ?`;
  }

  // =============================================================================
  // 현재값 쿼리들
  // =============================================================================

  // 현재값 UPSERT
  static upsertCurrentValue() {
    return `
      INSERT OR REPLACE INTO current_values (
        point_id, current_value, raw_value, value_type,
        quality_code, quality, value_timestamp, quality_timestamp,
        last_log_time, last_read_time, last_write_time,
        read_count, write_count, error_count, updated_at
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
    `;
  }

  // 현재값 조회
  static getCurrentValue() {
    return `SELECT * FROM current_values WHERE point_id = ?`;
  }

  // 디바이스의 모든 현재값 조회
  static getCurrentValuesByDevice() {
    return `
      SELECT cv.*, dp.name as point_name, dp.unit
      FROM current_values cv
      INNER JOIN data_points dp ON cv.point_id = dp.id
      WHERE dp.device_id = ?
      ORDER BY dp.address
    `;
  }

  // 품질별 데이터 포인트 수 조회
  static getDataPointCountByQuality() {
    return `
      SELECT 
        cv.quality,
        COUNT(*) as count
      FROM current_values cv
      INNER JOIN data_points dp ON cv.point_id = dp.id
      WHERE dp.device_id = ?
      GROUP BY cv.quality
    `;
  }

  // =============================================================================
  // 🔥 통계 및 모니터링 쿼리들 (connection_status로 수정)
  // =============================================================================

  // 프로토콜별 디바이스 수
  static getDeviceCountByProtocol() {
    return `
      SELECT 
        protocol_type,
        COUNT(*) as total_count,
        SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_count,
        SUM(CASE WHEN dst.connection_status = 'connected' THEN 1 ELSE 0 END) as connected_count
      FROM devices d
      LEFT JOIN device_status dst ON d.id = dst.device_id
      WHERE d.tenant_id = ?
      GROUP BY protocol_type
      ORDER BY total_count DESC
    `;
  }

  // 사이트별 디바이스 통계
  static getDeviceStatsBySite() {
    return `
      SELECT 
        s.id as site_id,
        s.name as site_name,
        s.code as site_code,
        COUNT(d.id) as device_count,
        SUM(CASE WHEN d.is_enabled = 1 THEN 1 ELSE 0 END) as enabled_count,
        SUM(CASE WHEN dst.connection_status = 'connected' THEN 1 ELSE 0 END) as connected_count,
        SUM(CASE WHEN dst.connection_status = 'disconnected' THEN 1 ELSE 0 END) as disconnected_count,
        SUM(CASE WHEN dst.connection_status = 'error' THEN 1 ELSE 0 END) as error_count
      FROM sites s
      LEFT JOIN devices d ON s.id = d.site_id
      LEFT JOIN device_status dst ON d.id = dst.device_id
      WHERE s.tenant_id = ?
      GROUP BY s.id, s.name, s.code
      ORDER BY s.name
    `;
  }

  // 디바이스 타입별 통계
  static getDeviceCountByType() {
    return `
      SELECT 
        device_type,
        COUNT(*) as count,
        SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_count
      FROM devices
      WHERE tenant_id = ?
      GROUP BY device_type
      ORDER BY count DESC
    `;
  }

  // 전체 시스템 상태 요약
  static getSystemStatusSummary() {
    return `
      SELECT 
        COUNT(d.id) as total_devices,
        SUM(CASE WHEN d.is_enabled = 1 THEN 1 ELSE 0 END) as enabled_devices,
        SUM(CASE WHEN dst.connection_status = 'connected' THEN 1 ELSE 0 END) as connected_devices,
        SUM(CASE WHEN dst.connection_status = 'disconnected' THEN 1 ELSE 0 END) as disconnected_devices,
        SUM(CASE WHEN dst.connection_status = 'error' THEN 1 ELSE 0 END) as error_devices,
        COUNT(DISTINCT d.protocol_type) as protocol_types,
        COUNT(DISTINCT d.site_id) as sites_with_devices,
        COUNT(dp.id) as total_data_points,
        SUM(CASE WHEN dp.is_enabled = 1 THEN 1 ELSE 0 END) as enabled_data_points
      FROM devices d
      LEFT JOIN device_status dst ON d.id = dst.device_id
      LEFT JOIN data_points dp ON d.id = dp.device_id
      WHERE d.tenant_id = ?
    `;
  }

  // 🔥 수정: 최근 활동한 디바이스 목록 (last_communication 사용)
  static getRecentActiveDevices() {
    return `
      SELECT 
        d.id,
        d.name,
        d.protocol_type,
        dst.connection_status,
        dst.last_communication,
        dst.response_time
      FROM devices d
      INNER JOIN device_status dst ON d.id = dst.device_id
      WHERE d.tenant_id = ? AND dst.last_communication IS NOT NULL
      ORDER BY dst.last_communication DESC
      LIMIT ?
    `;
  }

  // 🔥 수정: 오류가 있는 디바이스 목록
  static getDevicesWithErrors() {
    return `
      SELECT 
        d.id,
        d.name,
        d.protocol_type,
        dst.connection_status,
        dst.last_error,
        dst.error_count,
        dst.last_communication
      FROM devices d
      INNER JOIN device_status dst ON d.id = dst.device_id
      WHERE d.tenant_id = ? AND (dst.connection_status = 'error' OR dst.last_error IS NOT NULL OR dst.error_count > 0)
      ORDER BY dst.updated_at DESC
    `;
  }

  // 응답 시간 통계
  static getResponseTimeStats() {
    return `
      SELECT 
        AVG(dst.response_time) as avg_response_time,
        MIN(dst.response_time) as min_response_time,
        MAX(dst.response_time) as max_response_time,
        COUNT(*) as device_count
      FROM device_status dst
      INNER JOIN devices d ON dst.device_id = d.id
      WHERE d.tenant_id = ? AND dst.response_time IS NOT NULL
    `;
  }

  // =============================================================================
  // 고급 검색 및 필터링 쿼리들
  // =============================================================================

  // 데이터 포인트 검색 (크로스 디바이스)
  static searchDataPoints() {
    return `
      SELECT 
        dp.id,
        dp.device_id,
        d.name as device_name,
        dp.name as point_name,
        dp.description,
        dp.data_type,
        dp.unit,
        dp.address,
        cv.current_value,
        cv.quality
      FROM data_points dp
      INNER JOIN devices d ON dp.device_id = d.id
      LEFT JOIN current_values cv ON dp.id = cv.point_id
      WHERE d.tenant_id = ? AND (
        dp.name LIKE ? OR 
        dp.description LIKE ? OR 
        d.name LIKE ?
      )
      ORDER BY d.name, dp.address
    `;
  }

  // 디바이스 그룹별 통계
  static getDeviceStatsByGroup() {
    return `
      SELECT 
        dg.id as group_id,
        dg.name as group_name,
        dg.group_type,
        COUNT(d.id) as device_count,
        SUM(CASE WHEN d.is_enabled = 1 THEN 1 ELSE 0 END) as enabled_count
      FROM device_groups dg
      LEFT JOIN devices d ON dg.id = d.device_group_id
      WHERE dg.tenant_id = ?
      GROUP BY dg.id, dg.name, dg.group_type
      ORDER BY dg.name
    `;
  }

  // =============================================================================
  // 🔥 추가: 연결 상태별 상세 조회
  // =============================================================================

  // 연결 상태별 디바이스 목록
  static getDevicesByConnectionStatus() {
    return `
      SELECT 
        d.id,
        d.name,
        d.protocol_type,
        d.endpoint,
        dst.connection_status,
        dst.last_communication,
        dst.error_count,
        dst.last_error,
        dst.response_time
      FROM devices d
      INNER JOIN device_status dst ON d.id = dst.device_id
      WHERE d.tenant_id = ? AND dst.connection_status = ?
      ORDER BY dst.last_communication DESC
    `;
  }

  // 연결 상태 통계 (전체)
  static getConnectionStatusSummary() {
    return `
      SELECT 
        dst.connection_status,
        COUNT(*) as count,
        AVG(dst.response_time) as avg_response_time,
        SUM(dst.error_count) as total_errors
      FROM device_status dst
      INNER JOIN devices d ON dst.device_id = d.id
      WHERE d.tenant_id = ?
      GROUP BY dst.connection_status
      ORDER BY dst.connection_status
    `;
  }

  // 오류 카운트가 높은 디바이스 목록
  static getTopErrorDevices() {
    return `
      SELECT 
        d.id,
        d.name,
        d.protocol_type,
        dst.connection_status,
        dst.error_count,
        dst.last_error,
        dst.last_communication
      FROM devices d
      INNER JOIN device_status dst ON d.id = dst.device_id
      WHERE d.tenant_id = ? AND dst.error_count > 0
      ORDER BY dst.error_count DESC, dst.last_communication DESC
      LIMIT ?
    `;
  }
}

module.exports = DeviceQueries;