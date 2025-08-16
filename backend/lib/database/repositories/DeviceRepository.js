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
      query += ` AND d.id = ?`;
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
  async getCurrentValuesByDevice(deviceId, tenantId = null) {
    try {
      console.log(`📊 DeviceRepository.getCurrentValuesByDevice 호출: deviceId=${deviceId}, tenantId=${tenantId}`);
      
      // DeviceQueries 사용 (기존 코드 그대로)
      const result = await this.dbFactory.executeQuery(DeviceQueries.getCurrentValuesByDevice(), [deviceId]);
      const rawValues = Array.isArray(result) ? result : (result.rows || []);
      
      console.log(`✅ 디바이스 ID ${deviceId} 현재값 ${rawValues.length}개 조회 완료`);

      // parseCurrentValue 메서드 사용 (없으면 직접 파싱)
      return rawValues.map(cv => {
        try {
          return this.parseCurrentValue ? this.parseCurrentValue(cv) : this.parseCurrentValueInline(cv);
        } catch (parseError) {
          console.warn(`현재값 파싱 실패 for point ${cv.point_id}:`, parseError.message);
          return this.parseCurrentValueInline(cv);
        }
      });
      
    } catch (error) {
      console.error(`❌ DeviceRepository.getCurrentValuesByDevice 실패:`, error.message);
      throw new Error(`디바이스 현재값 조회 실패: ${error.message}`);
    }
  }
  /**
   * 현재값 인라인 파싱 메서드 (parseCurrentValue가 없을 때 사용)
   */
  parseCurrentValueInline(cv) {
    let parsedCurrentValue = null;
    let parsedRawValue = null;

    // JSON 파싱 처리
    if (cv.current_value) {
      try {
        parsedCurrentValue = typeof cv.current_value === 'string' 
          ? JSON.parse(cv.current_value) 
          : cv.current_value;
      } catch (parseError) {
        parsedCurrentValue = cv.current_value;
      }
    }

    if (cv.raw_value) {
      try {
        parsedRawValue = typeof cv.raw_value === 'string' 
          ? JSON.parse(cv.raw_value) 
          : cv.raw_value;
      } catch (parseError) {
        parsedRawValue = cv.raw_value;
      }
    }

    return {
      point_id: cv.point_id,
      point_name: cv.point_name || `Point_${cv.point_id}`,
      unit: cv.unit || '',
      current_value: parsedCurrentValue,
      raw_value: parsedRawValue,
      value_type: cv.value_type || 'double',
      quality_code: cv.quality_code || 0,
      quality: cv.quality || 'not_connected',
      value_timestamp: cv.value_timestamp,
      quality_timestamp: cv.quality_timestamp,
      last_log_time: cv.last_log_time,
      last_read_time: cv.last_read_time,
      last_write_time: cv.last_write_time,
      read_count: cv.read_count || 0,
      write_count: cv.write_count || 0,
      error_count: cv.error_count || 0,
      updated_at: cv.updated_at
    };
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
    try {
      let parsedCurrentValue = null;
      let parsedRawValue = null;

      // JSON 파싱 처리
      if (cv.current_value) {
        try {
          parsedCurrentValue = typeof cv.current_value === 'string' 
            ? JSON.parse(cv.current_value) 
            : cv.current_value;
        } catch (parseError) {
          console.warn(`current_value JSON 파싱 실패 for point ${cv.point_id}:`, parseError.message);
          parsedCurrentValue = cv.current_value;
        }
      }

      if (cv.raw_value) {
        try {
          parsedRawValue = typeof cv.raw_value === 'string' 
            ? JSON.parse(cv.raw_value) 
            : cv.raw_value;
        } catch (parseError) {
          console.warn(`raw_value JSON 파싱 실패 for point ${cv.point_id}:`, parseError.message);
          parsedRawValue = cv.raw_value;
        }
      }

      return {
        point_id: cv.point_id,
        point_name: cv.point_name || `Point_${cv.point_id}`,
        unit: cv.unit || '',
        current_value: parsedCurrentValue,
        raw_value: parsedRawValue,
        value_type: cv.value_type || 'double',
        quality_code: cv.quality_code || 0,
        quality: cv.quality || 'not_connected',
        value_timestamp: cv.value_timestamp,
        quality_timestamp: cv.quality_timestamp,
        last_log_time: cv.last_log_time,
        last_read_time: cv.last_read_time,
        last_write_time: cv.last_write_time,
        read_count: cv.read_count || 0,
        write_count: cv.write_count || 0,
        error_count: cv.error_count || 0,
        updated_at: cv.updated_at
      };
    } catch (error) {
      console.error(`parseCurrentValue 실패 for point ${cv?.point_id}:`, error.message);
      return {
        point_id: cv?.point_id || 0,
        point_name: cv?.point_name || 'Unknown',
        unit: cv?.unit || '',
        current_value: null,
        raw_value: null,
        value_type: 'double',
        quality_code: 0,
        quality: 'not_connected',
        value_timestamp: null,
        quality_timestamp: null,
        last_log_time: null,
        last_read_time: null,
        last_write_time: null,
        read_count: 0,
        write_count: 0,
        error_count: 0,
        updated_at: cv?.updated_at || null
      };
    }
  }

  /**
   * 지원 프로토콜 목록 조회
   * @param {number} tenantId 
   * @returns {Promise<Array>}
   */
  async getAvailableProtocols(tenantId) {
    try {
      console.log(`📋 DeviceRepository.getAvailableProtocols 호출: tenantId=${tenantId}`);
      
      // DeviceQueries 사용 (기존 패턴 준수)
      const query = DeviceQueries.getAvailableProtocols();
      const params = [tenantId];

      console.log(`🔍 프로토콜 조회 쿼리: ${query.substring(0, 100)}...`);
      console.log(`🔍 파라미터:`, params);

      const result = await this.dbFactory.executeQuery(query, params);
      const protocols = Array.isArray(result) ? result : (result.rows || []);
      
      console.log(`✅ 지원 프로토콜 ${protocols.length}개 조회 완료`);

      // 프로토콜 데이터 가공
      return protocols.map(protocol => ({
        protocol_type: protocol.protocol_type,
        device_count: protocol.device_count || 0,
        connected_count: protocol.connected_count || 0,
        enabled_count: protocol.enabled_count || 0,
        description: this.getProtocolDescription(protocol.protocol_type)
      }));
      
    } catch (error) {
      console.error(`❌ DeviceRepository.getAvailableProtocols 실패:`, error.message);
      
      // 에러 발생 시 기본 프로토콜 목록 반환
      console.log('🔄 기본 프로토콜 목록으로 대체...');
      return this.getDefaultProtocols();
    }
  }

  /**
   * 테넌트별 디바이스 통계 조회
   * @param {number} tenantId 
   * @returns {Promise<Object>}
   */
  async getStatsByTenant(tenantId) {
    try {
      console.log(`📊 DeviceRepository.getStatsByTenant 호출: tenantId=${tenantId}`);
      
      // 여러 통계 쿼리 실행
      const stats = {};

      // 1. 전체 디바이스 통계
      const deviceCountQuery = DeviceQueries.getDeviceCountByStatus();
      const deviceCounts = await this.dbFactory.executeQuery(deviceCountQuery, [tenantId]);
      
      // 2. 프로토콜별 통계
      const protocolStatsQuery = DeviceQueries.getDeviceCountByProtocol();
      const protocolStats = await this.dbFactory.executeQuery(protocolStatsQuery, [tenantId]);
      
      // 3. 사이트별 통계
      const siteStatsQuery = DeviceQueries.getDeviceCountBySite();
      const siteStats = await this.dbFactory.executeQuery(siteStatsQuery, [tenantId]);

      // 통계 데이터 조합
      stats.total_devices = this.calculateTotalDevices(deviceCounts);
      stats.connected_devices = this.calculateConnectedDevices(deviceCounts);
      stats.disconnected_devices = stats.total_devices - stats.connected_devices;
      stats.enabled_devices = this.calculateEnabledDevices(deviceCounts);
      stats.protocol_distribution = this.formatProtocolStats(protocolStats);
      stats.site_distribution = this.formatSiteStats(siteStats);
      stats.connection_rate = stats.total_devices > 0 
        ? ((stats.connected_devices / stats.total_devices) * 100).toFixed(1) + '%'
        : '0%';
      stats.last_updated = new Date().toISOString();

      console.log(`✅ 디바이스 통계 조회 완료: ${stats.total_devices}개 디바이스`);
      return stats;
      
    } catch (error) {
      console.error(`❌ DeviceRepository.getStatsByTenant 실패:`, error.message);
      
      // 에러 발생 시 기본 통계 반환
      console.log('🔄 기본 통계 데이터로 대체...');
      return this.getDefaultStats(tenantId);
    }
  }

  /**
   * 프로토콜 설명 반환
   * @param {string} protocolType 
   * @returns {string}
   */
  getProtocolDescription(protocolType) {
    const descriptions = {
      'MODBUS_TCP': 'Modbus TCP/IP - 산업용 이더넷 통신',
      'MODBUS_RTU': 'Modbus RTU - 시리얼 통신 프로토콜',
      'BACNET': 'BACnet - 빌딩 자동화 네트워크',
      'MQTT': 'MQTT - IoT 메시징 프로토콜',
      'OPC_UA': 'OPC UA - 산업용 표준 통신',
      'ETHERNET_IP': 'EtherNet/IP - 산업용 이더넷',
      'PROFINET': 'PROFINET - 지멘스 산업용 네트워크'
    };
    return descriptions[protocolType] || `${protocolType} 프로토콜`;
  }

  /**
   * 기본 프로토콜 목록 (에러 시 사용)
   * @returns {Array}
   */
  getDefaultProtocols() {
    return [
      {
        protocol_type: 'MODBUS_TCP',
        device_count: 0,
        connected_count: 0,
        enabled_count: 0,
        description: 'Modbus TCP/IP - 산업용 이더넷 통신'
      },
      {
        protocol_type: 'MODBUS_RTU', 
        device_count: 0,
        connected_count: 0,
        enabled_count: 0,
        description: 'Modbus RTU - 시리얼 통신 프로토콜'
      },
      {
        protocol_type: 'BACNET',
        device_count: 0,
        connected_count: 0,
        enabled_count: 0,
        description: 'BACnet - 빌딩 자동화 네트워크'
      },
      {
        protocol_type: 'MQTT',
        device_count: 0,
        connected_count: 0,
        enabled_count: 0,
        description: 'MQTT - IoT 메시징 프로토콜'
      }
    ];
  }

  /**
   * 기본 통계 데이터 (에러 시 사용)
   * @param {number} tenantId 
   * @returns {Object}
   */
  getDefaultStats(tenantId) {
    return {
      total_devices: 0,
      connected_devices: 0,
      disconnected_devices: 0,
      enabled_devices: 0,
      protocol_distribution: [],
      site_distribution: [],
      connection_rate: '0%',
      last_updated: new Date().toISOString(),
      note: 'Default statistics due to query error'
    };
  }

  /**
   * 디바이스 개수 계산 헬퍼 메서드들
   */
  calculateTotalDevices(deviceCounts) {
    if (!Array.isArray(deviceCounts) || deviceCounts.length === 0) return 0;
    return deviceCounts.reduce((total, row) => total + (parseInt(row.device_count) || 0), 0);
  }

  calculateConnectedDevices(deviceCounts) {
    if (!Array.isArray(deviceCounts) || deviceCounts.length === 0) return 0;
    return deviceCounts
      .filter(row => row.connection_status === 'connected')
      .reduce((total, row) => total + (parseInt(row.device_count) || 0), 0);
  }

  calculateEnabledDevices(deviceCounts) {
    if (!Array.isArray(deviceCounts) || deviceCounts.length === 0) return 0;
    return deviceCounts
      .filter(row => row.is_enabled === 1)
      .reduce((total, row) => total + (parseInt(row.device_count) || 0), 0);
  }

  formatProtocolStats(protocolStats) {
    if (!Array.isArray(protocolStats)) return [];
    return protocolStats.map(stat => ({
      protocol_type: stat.protocol_type,
      total_count: parseInt(stat.total_count) || 0,
      enabled_count: parseInt(stat.enabled_count) || 0,
      connected_count: parseInt(stat.connected_count) || 0,
      percentage: stat.total_count > 0 
        ? ((parseInt(stat.total_count) / parseInt(stat.total_count)) * 100).toFixed(1)
        : '0'
    }));
  }

  formatSiteStats(siteStats) {
    if (!Array.isArray(siteStats)) return [];
    return siteStats.map(stat => ({
      site_id: parseInt(stat.site_id) || 0,
      site_name: stat.site_name || 'Unknown Site',
      device_count: parseInt(stat.device_count) || 0,
      connected_count: parseInt(stat.connected_count) || 0
    }));
  }

}

module.exports = DeviceRepository;