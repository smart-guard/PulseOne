// =============================================================================
// backend/lib/database/DeviceRepository.js
// 디바이스 관리 통합 리포지토리 - AlarmOccurrenceRepository.js 패턴 준수
// =============================================================================

const DatabaseFactory = require('./BaseRepository');
const DeviceQueries = require('../queries/DeviceQueries');

class DeviceRepository {
  constructor() {
    this.dbFactory = new DatabaseFactory();
  }

  // =============================================================================
  // 디바이스 조회 메소드들
  // =============================================================================
  /**
   * ID로 디바이스 조회 (누락된 메서드)
   */
  async findById(id, tenantId = null) {
    try {
      console.log(`📱 DeviceRepository.findById 호출: id=${id}, tenantId=${tenantId}`);
      
      // DeviceQueries에서 쿼리 가져오기
      let query = DeviceQueries.getDevicesWithAllInfo();
      const params = [];

      // WHERE 조건 추가
      query += ` WHERE d.id = ?`;
      params.push(id);

      if (tenantId) {
        query += ` AND d.tenant_id = ?`;
        params.push(tenantId);
      }

      // 그룹화 (JOIN으로 인한 중복 방지)
      query += DeviceQueries.getGroupByAndOrder();

      console.log(`🔍 실행할 쿼리: ${query.substring(0, 100)}...`);
      console.log(`🔍 파라미터:`, params);

      const result = await this.dbFactory.executeQuery(query, params);
      const devices = Array.isArray(result) ? result : (result.rows || []);
      
      if (devices.length === 0) {
        console.log(`❌ 디바이스 ID ${id} 찾을 수 없음`);
        return null;
      }
      
      console.log(`✅ 디바이스 ID ${id} 조회 성공: ${devices[0].name}`);
      return this.parseDevice(devices[0]);
      
    } catch (error) {
      console.error('❌ DeviceRepository.findById 오류:', error);
      throw new Error(`디바이스 조회 실패: ${error.message}`);
    }
  }

  /**
   * 이름으로 디바이스 조회 (누락된 메서드)
   */
  async findByName(name, tenantId = null) {
    try {
      console.log(`📱 DeviceRepository.findByName 호출: name=${name}, tenantId=${tenantId}`);
      
      // DeviceQueries에서 쿼리 가져오기
      let query = DeviceQueries.getDevicesWithAllInfo();
      const params = [];

      // WHERE 조건 추가
      query += ` WHERE d.name = ?`;
      params.push(name);

      if (tenantId) {
        query += ` AND d.tenant_id = ?`;
        params.push(tenantId);
      }

      // 그룹화 (JOIN으로 인한 중복 방지)
      query += DeviceQueries.getGroupByAndOrder();

      console.log(`🔍 실행할 쿼리: ${query.substring(0, 100)}...`);
      console.log(`🔍 파라미터:`, params);

      const result = await this.dbFactory.executeQuery(query, params);
      const devices = Array.isArray(result) ? result : (result.rows || []);
      
      if (devices.length === 0) {
        console.log(`❌ 디바이스 이름 '${name}' 찾을 수 없음`);
        return null;
      }
      
      console.log(`✅ 디바이스 이름 '${name}' 조회 성공: ID ${devices[0].id}`);
      return this.parseDevice(devices[0]);
      
    } catch (error) {
      console.error('❌ DeviceRepository.findByName 오류:', error);
      throw new Error(`디바이스 이름 조회 실패: ${error.message}`);
    }
  }

  /**
   * 디바이스의 데이터포인트들 조회 (누락된 메서드) - DeviceQueries 사용
   */
  async getDeviceDataPoints(deviceId, tenantId = null) {
    try {
      console.log(`📊 DeviceRepository.getDeviceDataPoints 호출: deviceId=${deviceId}, tenantId=${tenantId}`);
      
      // 먼저 디바이스가 존재하는지 확인
      const device = await this.findById(deviceId, tenantId);
      if (!device) {
        throw new Error(`디바이스 ID ${deviceId}를 찾을 수 없습니다`);
      }

      // DeviceQueries에서 데이터포인트 쿼리 가져오기
      let query = DeviceQueries.getDataPointsByDevice();
      const params = [deviceId];

      if (tenantId) {
        // 테넌트 조건 추가
        query += DeviceQueries.addTenantFilterForDataPoints();
        params.push(tenantId);
      }

      // 정렬
      query += DeviceQueries.getDataPointsOrderBy();

      console.log(`🔍 데이터포인트 쿼리: ${query.substring(0, 100)}...`);
      console.log(`🔍 파라미터:`, params);

      const result = await this.dbFactory.executeQuery(query, params);
      const dataPoints = Array.isArray(result) ? result : (result.rows || []);
      
      console.log(`✅ 디바이스 ${deviceId}의 데이터포인트 ${dataPoints.length}개 조회 성공`);
      
      // 데이터포인트 파싱
      return dataPoints.map(dp => this.parseDataPoint(dp));
      
    } catch (error) {
      console.error('❌ DeviceRepository.getDeviceDataPoints 오류:', error);
      throw new Error(`디바이스 데이터포인트 조회 실패: ${error.message}`);
    }
  }
  // 디바이스 목록 조회 (모든 관련 정보 포함)
  async findAllDevices(filters = {}) {
    try {
        console.log('🔍 DeviceRepository.findAllDevices 호출:', filters);
        
        // 🔥 DeviceQueries의 기존 JOIN 쿼리 사용 (그대로)
        let query = DeviceQueries.getDevicesWithAllInfo();
        const params = [];

        // 기본 tenant 필터 (필수)
        query += DeviceQueries.addTenantFilter();
        params.push(filters.tenantId || filters.tenant_id || 1);

        // 선택적 필터들
        if (filters.siteId || filters.site_id) {
        query += DeviceQueries.addSiteFilter();
        params.push(filters.siteId || filters.site_id);
        }

        if (filters.protocolType || filters.protocol_type) {
        query += DeviceQueries.addProtocolTypeFilter();
        params.push(filters.protocolType || filters.protocol_type);
        }

        if (filters.deviceType || filters.device_type) {
        query += DeviceQueries.addDeviceTypeFilter();
        params.push(filters.deviceType || filters.device_type);
        }

        if (filters.search) {
        query += DeviceQueries.addSearchFilter();
        const searchTerm = `%${filters.search}%`;
        params.push(searchTerm, searchTerm, searchTerm, searchTerm);
        }

        // 그룹화 및 정렬
        query += DeviceQueries.getGroupByAndOrder();

        // 페이징
        const limit = filters.limit || 25;
        const page = filters.page || 1;
        const offset = (page - 1) * limit;
        
        query += DeviceQueries.addLimit();
        params.push(limit);

        console.log('🔍 실행할 쿼리:', query.substring(0, 200) + '...');
        console.log('🔍 파라미터:', params.length + '개');

        const result = await this.dbFactory.executeQuery(query, params);
        console.log('🔍 Query result type:', typeof result);
        console.log('🔍 Query result keys:', Object.keys(result || {}));
        
        // 결과 처리 (다양한 DB 드라이버 대응)
        let devices = [];
        if (Array.isArray(result)) {
            devices = result;
        } else if (result && result.rows) {
            devices = result.rows;
        } else if (result && result.recordset) {
            devices = result.recordset;
        } else {
            console.warn('🔍 예상치 못한 쿼리 결과 구조:', result);
            devices = [];
        }

        console.log(`✅ ${devices.length}개 디바이스 조회 완료`);

        // 데이터 파싱
        const parsedDevices = devices.map(device => this.parseDevice(device));

        // 페이징 정보 계산
        const totalCount = devices.length > 0 ? 
            (filters.page && filters.limit ? await this.getDeviceCount(filters) : devices.length) : 0;
        
        const pagination = {
            page: parseInt(page),
            limit: parseInt(limit),
            total_items: totalCount,
            has_next: page * limit < totalCount,
            has_prev: page > 1
        };

        return {
            items: parsedDevices,
            pagination: pagination
        };

    } catch (error) {
        console.error('❌ DeviceRepository.findAllDevices 오류:', error.message);
        console.error('❌ 스택:', error.stack);
        throw new Error(`디바이스 목록 조회 실패: ${error.message}`);
    }
  }

  // 디바이스 수 조회
  async getDeviceCount(filters = {}) {
    try {
        let query = DeviceQueries.getDeviceCount();
        const params = [];

        // 기본 tenant 필터
        query += DeviceQueries.addTenantFilter();
        params.push(filters.tenantId || filters.tenant_id || 1);

        // 추가 필터들
        if (filters.siteId || filters.site_id) {
        query += DeviceQueries.addSiteFilter();
        params.push(filters.siteId || filters.site_id);
        }

        if (filters.protocolType || filters.protocol_type) {
        query += DeviceQueries.addProtocolTypeFilter();
        params.push(filters.protocolType || filters.protocol_type);
        }

        if (filters.deviceType || filters.device_type) {
        query += DeviceQueries.addDeviceTypeFilter();
        params.push(filters.deviceType || filters.device_type);
        }

        if (filters.search) {
        query += DeviceQueries.addSearchFilter();
        const searchTerm = `%${filters.search}%`;
        params.push(searchTerm, searchTerm, searchTerm, searchTerm);
        }

        const result = await this.dbFactory.executeQuery(query, params);
        const countResult = Array.isArray(result) ? result[0] : (result.rows ? result.rows[0] : result);
        
        return countResult?.count || 0;
    } catch (error) {
        console.error('❌ 디바이스 수 조회 실패:', error.message);
        return 0;
    }
  }

  // 디바이스 상세 조회
  async findDeviceById(id) {
    try {
      let query = DeviceQueries.getDevicesWithAllInfo();
      query += DeviceQueries.addDeviceIdFilter();
      query += DeviceQueries.getGroupByAndOrder();

      const results = await this.dbFactory.executeQuery(query, [id]);
      
      if (results.length === 0) {
        return null;
      }

      const device = this.parseDevice(results[0]);

      // 데이터 포인트 정보도 함께 조회
      const dataPoints = await this.getDataPointsByDevice(id);
      device.data_points = dataPoints;

      return device;
    } catch (error) {
      console.error('Error finding device by ID:', error);
      throw error;
    }
  }

  // =============================================================================
  // 디바이스 CRUD 메소드들
  // =============================================================================

  // 디바이스 생성 (설정과 상태도 함께 생성)
  async createDevice(deviceData) {
    const connection = await this.dbFactory.getConnection();
    
    try {
      await connection.query('BEGIN');

      // 디바이스 생성
      const deviceParams = [
        deviceData.tenant_id,
        deviceData.site_id,
        deviceData.device_group_id || null,
        deviceData.edge_server_id || null,
        deviceData.name,
        deviceData.description || null,
        deviceData.device_type,
        deviceData.manufacturer || null,
        deviceData.model || null,
        deviceData.serial_number || null,
        deviceData.protocol_type,
        deviceData.endpoint,
        deviceData.config ? JSON.stringify(deviceData.config) : null,
        deviceData.polling_interval || 1000,
        deviceData.timeout || 3000,
        deviceData.retry_count || 3,
        deviceData.is_enabled !== false ? 1 : 0,
        deviceData.installation_date || null,
        deviceData.created_by || null
      ];

      const result = await connection.query(DeviceQueries.createDevice(), deviceParams);
      const deviceId = result.insertId || result.lastInsertRowid;

      // 기본 설정 생성
      await this.createDefaultDeviceSettings(connection, deviceId, deviceData.settings);

      // 초기 상태 생성
      await this.createInitialDeviceStatus(connection, deviceId);

      await connection.query('COMMIT');
      return await this.findDeviceById(deviceId);
    } catch (error) {
      await connection.query('ROLLBACK');
      console.error('Error creating device:', error);
      throw error;
    } finally {
      connection.release();
    }
  }

  // 디바이스 업데이트
  async updateDevice(id, deviceData) {
    const connection = await this.dbFactory.getConnection();
    
    try {
      await connection.query('BEGIN');

      // 디바이스 기본 정보 업데이트
      const deviceParams = [
        deviceData.name,
        deviceData.description || null,
        deviceData.device_type,
        deviceData.manufacturer || null,
        deviceData.model || null,
        deviceData.serial_number || null,
        deviceData.protocol_type,
        deviceData.endpoint,
        deviceData.config ? JSON.stringify(deviceData.config) : null,
        deviceData.polling_interval || 1000,
        deviceData.timeout || 3000,
        deviceData.retry_count || 3,
        deviceData.is_enabled !== false ? 1 : 0,
        deviceData.installation_date || null,
        deviceData.last_maintenance || null,
        id
      ];

      await connection.query(DeviceQueries.updateDevice(), deviceParams);

      // 설정 업데이트 (제공된 경우)
      if (deviceData.settings) {
        await this.updateDeviceSettings(connection, id, deviceData.settings);
      }

      await connection.query('COMMIT');
      return await this.findDeviceById(id);
    } catch (error) {
      await connection.query('ROLLBACK');
      console.error('Error updating device:', error);
      throw error;
    } finally {
      connection.release();
    }
  }

  // 디바이스 삭제
  async deleteDevice(id) {
    try {
      const result = await this.dbFactory.executeQuery(DeviceQueries.deleteDevice(), [id]);
      return result.affectedRows > 0 || result.changes > 0;
    } catch (error) {
      console.error('Error deleting device:', error);
      throw error;
    }
  }

  // =============================================================================
  // 디바이스 설정 메소드들
  // =============================================================================

  // 디바이스 설정 업데이트
  async updateDeviceSettings(connection, deviceId, settings) {
    try {
      const settingsParams = [
        deviceId,
        settings.polling_interval_ms || 1000,
        settings.connection_timeout_ms || 10000,
        settings.max_retry_count || 3,
        settings.retry_interval_ms || 5000,
        settings.backoff_time_ms || 60000,
        settings.keep_alive_enabled !== false ? 1 : 0,
        settings.keep_alive_interval_s || 30
      ];

      await (connection || this.dbFactory).executeQuery(DeviceQueries.upsertDeviceSettings(), settingsParams);
    } catch (error) {
      console.error('Error updating device settings:', error);
      throw error;
    }
  }

  // 디바이스 설정 조회
  async getDeviceSettings(deviceId) {
    try {
      const results = await this.dbFactory.executeQuery(DeviceQueries.getDeviceSettings(), [deviceId]);
      return results.length > 0 ? results[0] : null;
    } catch (error) {
      console.error('Error getting device settings:', error);
      throw error;
    }
  }

  // 기본 설정 생성
  async createDefaultDeviceSettings(connection, deviceId, customSettings = {}) {
    try {
      const defaultSettings = {
        polling_interval_ms: 1000,
        connection_timeout_ms: 10000,
        max_retry_count: 3,
        retry_interval_ms: 5000,
        backoff_time_ms: 60000,
        keep_alive_enabled: true,
        keep_alive_interval_s: 30,
        ...customSettings
      };

      await this.updateDeviceSettings(connection, deviceId, defaultSettings);
    } catch (error) {
      console.error('Error creating default device settings:', error);
      throw error;
    }
  }

  // =============================================================================
  // 디바이스 상태 메소드들
  // =============================================================================

  // 디바이스 상태 업데이트
  async updateDeviceStatus(deviceId, status) {
    try {
      const statusParams = [
        deviceId,
        status.status || 'unknown',
        status.last_seen || null,
        status.last_error || null,
        status.response_time || null,
        status.firmware_version || null,
        status.hardware_info ? JSON.stringify(status.hardware_info) : null,
        status.diagnostic_data ? JSON.stringify(status.diagnostic_data) : null
      ];

      await this.dbFactory.executeQuery(DeviceQueries.upsertDeviceStatus(), statusParams);
    } catch (error) {
      console.error('Error updating device status:', error);
      throw error;
    }
  }

  // 초기 상태 생성
  async createInitialDeviceStatus(connection, deviceId) {
    try {
      const initialStatus = {
        status: 'offline',
        last_seen: null,
        last_error: null,
        response_time: null,
        firmware_version: null,
        hardware_info: null,
        diagnostic_data: null
      };

      const statusParams = [
        deviceId,
        initialStatus.status,
        initialStatus.last_seen,
        initialStatus.last_error,
        initialStatus.response_time,
        initialStatus.firmware_version,
        initialStatus.hardware_info,
        initialStatus.diagnostic_data
      ];

      await connection.query(DeviceQueries.upsertDeviceStatus(), statusParams);
    } catch (error) {
      console.error('Error creating initial device status:', error);
      throw error;
    }
  }

  // =============================================================================
  // 데이터 포인트 메소드들
  // =============================================================================

  // 디바이스의 데이터 포인트 조회
  async getDataPointsByDevice(deviceId) {
    try {
      const results = await this.dbFactory.executeQuery(DeviceQueries.getDataPointsByDevice(), [deviceId]);
      return results.map(dp => this.parseDataPoint(dp));
    } catch (error) {
      console.error('Error getting data points by device:', error);
      throw error;
    }
  }

  // 데이터 포인트 생성
  async createDataPoint(dataPointData) {
    try {
      const dpParams = [
        dataPointData.device_id,
        dataPointData.name,
        dataPointData.description || null,
        dataPointData.address,
        dataPointData.address_string || null,
        dataPointData.data_type || 'UNKNOWN',
        dataPointData.access_mode || 'read',
        dataPointData.is_enabled !== false ? 1 : 0,
        dataPointData.is_writable ? 1 : 0,
        dataPointData.unit || null,
        dataPointData.scaling_factor || 1.0,
        dataPointData.scaling_offset || 0.0,
        dataPointData.min_value || 0.0,
        dataPointData.max_value || 0.0,
        dataPointData.log_enabled !== false ? 1 : 0,
        dataPointData.log_interval_ms || 0,
        dataPointData.log_deadband || 0.0,
        dataPointData.polling_interval_ms || 0,
        dataPointData.group_name || null,
        dataPointData.tags ? JSON.stringify(dataPointData.tags) : null,
        dataPointData.metadata ? JSON.stringify(dataPointData.metadata) : null,
        dataPointData.protocol_params ? JSON.stringify(dataPointData.protocol_params) : null
      ];

      const result = await this.dbFactory.executeQuery(DeviceQueries.createDataPoint(), dpParams);
      return result.insertId || result.lastInsertRowid;
    } catch (error) {
      console.error('Error creating data point:', error);
      throw error;
    }
  }

  // 데이터 포인트 업데이트
  async updateDataPoint(id, dataPointData) {
    try {
      const dpParams = [
        dataPointData.name,
        dataPointData.description || null,
        dataPointData.address,
        dataPointData.address_string || null,
        dataPointData.data_type || 'UNKNOWN',
        dataPointData.access_mode || 'read',
        dataPointData.is_enabled !== false ? 1 : 0,
        dataPointData.is_writable ? 1 : 0,
        dataPointData.unit || null,
        dataPointData.scaling_factor || 1.0,
        dataPointData.scaling_offset || 0.0,
        dataPointData.min_value || 0.0,
        dataPointData.max_value || 0.0,
        dataPointData.log_enabled !== false ? 1 : 0,
        dataPointData.log_interval_ms || 0,
        dataPointData.log_deadband || 0.0,
        dataPointData.polling_interval_ms || 0,
        dataPointData.group_name || null,
        dataPointData.tags ? JSON.stringify(dataPointData.tags) : null,
        dataPointData.metadata ? JSON.stringify(dataPointData.metadata) : null,
        dataPointData.protocol_params ? JSON.stringify(dataPointData.protocol_params) : null,
        id
      ];

      await this.dbFactory.executeQuery(DeviceQueries.updateDataPoint(), dpParams);
      return true;
    } catch (error) {
      console.error('Error updating data point:', error);
      throw error;
    }
  }

  // 데이터 포인트 삭제
  async deleteDataPoint(id) {
    try {
      const result = await this.dbFactory.executeQuery(DeviceQueries.deleteDataPoint(), [id]);
      return result.affectedRows > 0 || result.changes > 0;
    } catch (error) {
      console.error('Error deleting data point:', error);
      throw error;
    }
  }

  // =============================================================================
  // 현재값 메소드들
  // =============================================================================

  // 현재값 업데이트
  async updateCurrentValue(pointId, valueData) {
    try {
      const valueParams = [
        pointId,
        valueData.current_value !== undefined ? JSON.stringify(valueData.current_value) : null,
        valueData.raw_value !== undefined ? JSON.stringify(valueData.raw_value) : null,
        valueData.value_type || 'double',
        valueData.quality_code || 0,
        valueData.quality || 'not_connected',
        valueData.value_timestamp || null,
        valueData.quality_timestamp || null,
        valueData.last_log_time || null,
        valueData.last_read_time || null,
        valueData.last_write_time || null,
        valueData.read_count || 0,
        valueData.write_count || 0,
        valueData.error_count || 0
      ];

      await this.dbFactory.executeQuery(DeviceQueries.upsertCurrentValue(), valueParams);
    } catch (error) {
      console.error('Error updating current value:', error);
      throw error;
    }
  }

  // 디바이스의 모든 현재값 조회
  async getCurrentValuesByDevice(deviceId) {
    try {
      const results = await this.dbFactory.executeQuery(DeviceQueries.getCurrentValuesByDevice(), [deviceId]);
      return results.map(cv => this.parseCurrentValue(cv));
    } catch (error) {
      console.error('Error getting current values by device:', error);
      throw error;
    }
  }

  // =============================================================================
  // 통계 및 모니터링 메소드들
  // =============================================================================

  // 프로토콜별 디바이스 통계
  async getDeviceStatsByProtocol(tenantId) {
    try {
      const stats = await this.dbFactory.executeQuery(DeviceQueries.getDeviceCountByProtocol(), [tenantId]);
      return stats;
    } catch (error) {
      console.error('Error getting device stats by protocol:', error);
      throw error;
    }
  }

  // 사이트별 디바이스 통계
  async getDeviceStatsBySite(tenantId) {
    try {
      const stats = await this.dbFactory.executeQuery(DeviceQueries.getDeviceStatsBySite(), [tenantId]);
      return stats;
    } catch (error) {
      console.error('Error getting device stats by site:', error);
      throw error;
    }
  }

  // 전체 시스템 상태 요약
  async getSystemStatusSummary(tenantId) {
    try {
      const summary = await this.dbFactory.executeQuery(DeviceQueries.getSystemStatusSummary(), [tenantId]);
      return summary[0] || {};
    } catch (error) {
      console.error('Error getting system status summary:', error);
      throw error;
    }
  }

  // 최근 활동한 디바이스 목록
  async getRecentActiveDevices(tenantId, limit = 10) {
    try {
      const devices = await this.dbFactory.executeQuery(DeviceQueries.getRecentActiveDevices(), [tenantId, limit]);
      return devices;
    } catch (error) {
      console.error('Error getting recent active devices:', error);
      throw error;
    }
  }

  // 오류가 있는 디바이스 목록
  async getDevicesWithErrors(tenantId) {
    try {
      const devices = await this.dbFactory.executeQuery(DeviceQueries.getDevicesWithErrors(), [tenantId]);
      return devices;
    } catch (error) {
      console.error('Error getting devices with errors:', error);
      throw error;
    }
  }

  // 응답 시간 통계
  async getResponseTimeStats(tenantId) {
    try {
      const stats = await this.dbFactory.executeQuery(DeviceQueries.getResponseTimeStats(), [tenantId]);
      return stats[0] || {};
    } catch (error) {
      console.error('Error getting response time stats:', error);
      throw error;
    }
  }

  // 데이터 포인트 검색
  async searchDataPoints(tenantId, searchTerm) {
    try {
      const searchPattern = `%${searchTerm}%`;
      const dataPoints = await this.dbFactory.executeQuery(
        DeviceQueries.searchDataPoints(), 
        [tenantId, searchPattern, searchPattern, searchPattern]
      );
      return dataPoints.map(dp => this.parseDataPoint(dp));
    } catch (error) {
      console.error('Error searching data points:', error);
      throw error;
    }
  }

  // =============================================================================
  // 헬퍼 메소드들
  // =============================================================================

  // 디바이스 데이터 파싱
  parseDevice(device) {
    if (!device) return null;

    return {
      ...device,
      is_enabled: !!device.is_enabled,
      config: device.config ? JSON.parse(device.config) : {},
      settings: {
        polling_interval_ms: device.polling_interval_ms,
        connection_timeout_ms: device.connection_timeout_ms,
        max_retry_count: device.max_retry_count,
        retry_interval_ms: device.retry_interval_ms,
        backoff_time_ms: device.backoff_time_ms,
        keep_alive_enabled: !!device.keep_alive_enabled,
        keep_alive_interval_s: device.keep_alive_interval_s,
        updated_at: device.settings_updated_at
      },
      status: {
        status: device.connection_status || 'unknown',
        last_error: device.last_error,
        response_time: device.response_time,
        firmware_version: device.firmware_version,
        hardware_info: device.hardware_info ? JSON.parse(device.hardware_info) : null,
        diagnostic_data: device.diagnostic_data ? JSON.parse(device.diagnostic_data) : null,
        updated_at: device.status_updated_at
      }
    };
  }

  // 데이터 포인트 데이터 파싱
  parseDataPoint(dp) {
    return {
      ...dp,
      is_enabled: !!dp.is_enabled,
      is_writable: !!dp.is_writable,
      log_enabled: !!dp.log_enabled,
      tags: dp.tags ? JSON.parse(dp.tags) : [],
      metadata: dp.metadata ? JSON.parse(dp.metadata) : {},
      protocol_params: dp.protocol_params ? JSON.parse(dp.protocol_params) : {},
      current_value: dp.current_value ? JSON.parse(dp.current_value) : null,
      raw_value: dp.raw_value ? JSON.parse(dp.raw_value) : null
    };
  }

  // 현재값 데이터 파싱
  parseCurrentValue(cv) {
    return {
      ...cv,
      current_value: cv.current_value ? JSON.parse(cv.current_value) : null,
      raw_value: cv.raw_value ? JSON.parse(cv.raw_value) : null
    };
  }
}

module.exports = DeviceRepository;