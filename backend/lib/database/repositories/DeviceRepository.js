// =============================================================================
// backend/lib/database/repositories/DeviceRepository.js - 완전 수정본
// protocol_id 직접 처리, 에러 처리 개선
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
  
  async findById(id, tenantId = null) {
    try {
      console.log(`DeviceRepository.findById 호출: id=${id}, tenantId=${tenantId}`);
      
      let query = DeviceQueries.getDevicesWithAllInfo();
      const params = [];

      query += ` AND d.id = ?`;
      params.push(id);

      if (tenantId) {
        query += ` AND d.tenant_id = ?`;
        params.push(tenantId);
      }

      query += DeviceQueries.getGroupByAndOrder();

      const result = await this.dbFactory.executeQuery(query, params);
      const devices = Array.isArray(result) ? result : (result.rows || []);
      
      if (devices.length === 0) {
        console.log(`디바이스 ID ${id} 찾을 수 없음`);
        return null;
      }
      
      console.log(`디바이스 ID ${id} 조회 성공: ${devices[0].name}`);
      return this.parseDevice(devices[0]);
      
    } catch (error) {
      console.error('DeviceRepository.findById 오류:', error);
      throw new Error(`디바이스 조회 실패: ${error.message}`);
    }
  }

  async findByName(name, tenantId = null) {
    try {
      console.log(`DeviceRepository.findByName 호출: name=${name}, tenantId=${tenantId}`);
      
      let query = DeviceQueries.getDevicesWithAllInfo();
      const params = [];

      query += ` AND d.name = ?`;
      params.push(name);

      if (tenantId) {
        query += ` AND d.tenant_id = ?`;
        params.push(tenantId);
      }

      query += DeviceQueries.getGroupByAndOrder();

      const result = await this.dbFactory.executeQuery(query, params);
      const devices = Array.isArray(result) ? result : (result.rows || []);
      
      if (devices.length === 0) {
        console.log(`디바이스 이름 '${name}' 찾을 수 없음`);
        return null;
      }
      
      console.log(`디바이스 이름 '${name}' 조회 성공: ID ${devices[0].id}`);
      return this.parseDevice(devices[0]);
      
    } catch (error) {
      console.error('DeviceRepository.findByName 오류:', error);
      throw new Error(`디바이스 이름 조회 실패: ${error.message}`);
    }
  }

  async findAllDevices(filters = {}) {
    try {
        console.log('DeviceRepository.findAllDevices 호출:', filters);
        
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

        if (filters.protocolId || filters.protocol_id) {
          query += DeviceQueries.addProtocolIdFilter();
          params.push(filters.protocolId || filters.protocol_id);
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

        console.log('실행할 쿼리:', query.substring(0, 200) + '...');
        console.log('파라미터:', params.length + '개');

        const result = await this.dbFactory.executeQuery(query, params);
        
        // 결과 처리 (다양한 DB 드라이버 대응)
        let devices = [];
        if (Array.isArray(result)) {
            devices = result;
        } else if (result && result.rows) {
            devices = result.rows;
        } else if (result && result.recordset) {
            devices = result.recordset;
        } else {
            console.warn('예상치 못한 쿼리 결과 구조:', result);
            devices = [];
        }

        console.log(`${devices.length}개 디바이스 조회 완료`);

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
        console.error('DeviceRepository.findAllDevices 오류:', error.message);
        console.error('스택:', error.stack);
        throw new Error(`디바이스 목록 조회 실패: ${error.message}`);
    }
  }

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

        if (filters.protocolId || filters.protocol_id) {
          query += ` AND d.protocol_id = ?`;
          params.push(filters.protocolId || filters.protocol_id);
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
        console.error('디바이스 수 조회 실패:', error.message);
        return 0;
    }
  }

  // =============================================================================
  // 프로토콜 관련 메소드들
  // =============================================================================

  async getAvailableProtocols(tenantId) {
    try {
      console.log(`DeviceRepository.getAvailableProtocols: tenantId=${tenantId}`);
      
      const query = DeviceQueries.getAvailableProtocols();
      const result = await this.dbFactory.executeQuery(query, [tenantId]);
      const protocols = Array.isArray(result) ? result : (result.rows || []);
      
      console.log(`지원 프로토콜 ${protocols.length}개 조회 완료`);

      return protocols.map(protocol => ({
        id: protocol.id,
        name: protocol.display_name,
        value: protocol.protocol_type,
        description: protocol.description,
        protocol_type: protocol.protocol_type,
        display_name: protocol.display_name,
        default_port: protocol.default_port,
        uses_serial: !!protocol.uses_serial,
        requires_broker: !!protocol.requires_broker,
        supported_operations: this.parseJsonField(protocol.supported_operations),
        supported_data_types: this.parseJsonField(protocol.supported_data_types),
        connection_params: this.parseJsonField(protocol.connection_params),
        default_polling_interval: protocol.default_polling_interval,
        default_timeout: protocol.default_timeout,
        category: protocol.category,
        device_count: parseInt(protocol.device_count) || 0,
        enabled_count: parseInt(protocol.enabled_count) || 0,
        connected_count: parseInt(protocol.connected_count) || 0
      }));
      
    } catch (error) {
      console.error('DeviceRepository.getAvailableProtocols 실패:', error.message);
      console.error('Error stack:', error.stack);
      throw error;
    }
  }

  // =============================================================================
  // CRUD 메소드들
  // =============================================================================

  async createDevice(deviceData, tenantId = null) {
    try {
      console.log(`DeviceRepository.createDevice: ${deviceData.name} (protocol_id: ${deviceData.protocol_id})`);
      
      await this.dbFactory.executeQuery('BEGIN TRANSACTION');
      
      try {
        // 디바이스 생성
        const deviceQuery = DeviceQueries.createDevice();
        const deviceParams = [
          tenantId || deviceData.tenant_id,
          deviceData.site_id || 1,
          deviceData.device_group_id || null,
          deviceData.edge_server_id || null,
          deviceData.name,
          deviceData.description || null,
          deviceData.device_type || 'PLC',
          deviceData.manufacturer || null,
          deviceData.model || null,
          deviceData.serial_number || null,
          deviceData.protocol_id,
          deviceData.endpoint,
          deviceData.config ? JSON.stringify(deviceData.config) : '{}',
          deviceData.polling_interval || 1000,
          deviceData.timeout || 3000,
          deviceData.retry_count || 3,
          deviceData.is_enabled !== false ? 1 : 0,
          deviceData.installation_date || null,
          deviceData.created_by || 1
        ];
        
        const deviceResult = await this.dbFactory.executeQuery(deviceQuery, deviceParams);
        
        // ID 추출
        let deviceId = null;
        if (deviceResult) {
          if (deviceResult.insertId) {
            deviceId = deviceResult.insertId;
          } else if (deviceResult.lastInsertRowid) {
            deviceId = deviceResult.lastInsertRowid;
          } else if (deviceResult.changes && deviceResult.changes > 0) {
            const idResult = await this.dbFactory.executeQuery('SELECT last_insert_rowid() as id');
            if (idResult && idResult.length > 0 && idResult[0].id) {
              deviceId = idResult[0].id;
            }
          }
        }
        
        if (!deviceId) {
          const createdDevice = await this.findByName(deviceData.name, tenantId);
          if (createdDevice && createdDevice.id) {
            deviceId = createdDevice.id;
          } else {
            throw new Error('디바이스 생성 실패: ID를 얻을 수 없음');
          }
        }
        
        console.log(`디바이스 생성 완료: ID ${deviceId}`);
        
        // 디바이스 상태 생성
        try {
          const statusQuery = `
            INSERT INTO device_status (device_id, connection_status, error_count, updated_at) 
            VALUES (?, 'disconnected', 0, CURRENT_TIMESTAMP)
          `;
          await this.dbFactory.executeQuery(statusQuery, [deviceId]);
        } catch (statusError) {
          console.warn('디바이스 상태 생성 실패 (계속 진행):', statusError.message);
        }
        
        await this.dbFactory.executeQuery('COMMIT');
        
        const createdDevice = await this.findById(deviceId, tenantId);
        console.log(`완전한 디바이스 생성 성공: ${deviceData.name} (ID: ${deviceId})`);
        
        return createdDevice;
        
      } catch (error) {
        await this.dbFactory.executeQuery('ROLLBACK');
        throw error;
      }
      
    } catch (error) {
      console.error('DeviceRepository.createDevice 실패:', error.message);
      throw new Error(`완전한 디바이스 생성 실패: ${error.message}`);
    }
  }

  async updateDeviceInfo(id, updateData, tenantId = null) {
    try {
      console.log(`DeviceRepository.updateDeviceInfo: ID ${id}`);
      
      const allowedFields = [
        'name', 'description', 'device_type', 'manufacturer', 'model', 'serial_number',
        'protocol_id', 'endpoint', 'config', 'polling_interval', 'timeout',
        'retry_count', 'is_enabled', 'installation_date', 'last_maintenance'
      ];
      
      const updateFields = [];
      const params = [];
      
      allowedFields.forEach(field => {
        if (updateData[field] !== undefined) {
          updateFields.push(field);
          if (field === 'config' && typeof updateData[field] === 'object') {
            params.push(JSON.stringify(updateData[field]));
          } else if (field === 'is_enabled') {
            params.push(updateData[field] ? 1 : 0);
          } else {
            params.push(updateData[field]);
          }
        }
      });
      
      if (updateFields.length === 0) {
        console.warn('업데이트할 필드가 없습니다');
        return await this.findById(id, tenantId);
      }
      
      const query = DeviceQueries.updateDeviceFields(updateFields);
      params.push(id, tenantId || 1);
      
      console.log(`업데이트 필드: ${updateFields.join(', ')}`);
      
      await this.dbFactory.executeQuery(query, params);
      
      return await this.findById(id, tenantId);
      
    } catch (error) {
      console.error('DeviceRepository.updateDeviceInfo 실패:', error.message);
      throw new Error(`디바이스 정보 업데이트 실패: ${error.message}`);
    }
  }

  async deleteById(id, tenantId = null) {
    try {
      console.log(`DeviceRepository.deleteById: ID ${id}`);
      
      const query = DeviceQueries.deleteDevice();
      const params = [id, tenantId || 1];
      
      const result = await this.dbFactory.executeQuery(query, params);
      
      const affected = result.changes || result.affectedRows || 0;
      
      console.log(`디바이스 완전 삭제 완료: ${affected}개 행 영향받음`);
      return affected > 0;
      
    } catch (error) {
      console.error('DeviceRepository.deleteById 실패:', error.message);
      throw new Error(`디바이스 삭제 실패: ${error.message}`);
    }
  }

  // =============================================================================
  // 상태 관리 메소드들
  // =============================================================================

  async updateDeviceStatus(id, isEnabled, tenantId = null) {
    try {
      console.log(`DeviceRepository.updateDeviceStatus: ID ${id}, enabled=${isEnabled}`);
      
      const query = DeviceQueries.updateDeviceEnabled();
      const params = [isEnabled ? 1 : 0, id, tenantId || 1];
      
      await this.dbFactory.executeQuery(query, params);
      
      if (!isEnabled) {
        await this.updateDeviceConnection(id, 'disconnected', null, tenantId);
      }
      
      return await this.findById(id, tenantId);
      
    } catch (error) {
      console.error('DeviceRepository.updateDeviceStatus 실패:', error.message);
      throw new Error(`디바이스 상태 업데이트 실패: ${error.message}`);
    }
  }

  async updateDeviceConnection(id, connectionStatus, lastCommunication = null, tenantId = null) {
    try {
      console.log(`DeviceRepository.updateDeviceConnection: ID ${id}, status=${connectionStatus}`);
      
      const query = DeviceQueries.updateDeviceConnectionStatus();
      const params = [
        id, 
        connectionStatus, 
        lastCommunication || (connectionStatus === 'connected' ? new Date().toISOString() : null)
      ];
      
      await this.dbFactory.executeQuery(query, params);
      
      return await this.findById(id, tenantId);
      
    } catch (error) {
      console.error('DeviceRepository.updateDeviceConnection 실패:', error.message);
      throw new Error(`디바이스 연결 상태 업데이트 실패: ${error.message}`);
    }
  }

  // =============================================================================
  // 통계 메소드들
  // =============================================================================

  async getStatsByTenant(tenantId) {
    try {
      console.log(`DeviceRepository.getStatsByTenant 호출: tenantId=${tenantId}`);
      
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

      console.log(`디바이스 통계 조회 완료: ${stats.total_devices}개 디바이스`);
      return stats;
      
    } catch (error) {
      console.error('DeviceRepository.getStatsByTenant 실패:', error.message);
      
      // 에러 발생 시 기본 통계 반환
      console.log('기본 통계 데이터로 대체...');
      return this.getDefaultStats(tenantId);
    }
  }

  // =============================================================================
  // 데이터포인트 메소드들
  // =============================================================================

  async getDataPointsByDevice(deviceId, tenantId = null, options = {}) {
    try {
      console.log(`DeviceRepository.getDataPointsByDevice 호출: deviceId=${deviceId}, tenantId=${tenantId}`, options);
      
      let query = DeviceQueries.getDataPointsByDevice();
      const params = [deviceId];

      if (options.data_type) {
        query += ` AND data_type = ?`;
        params.push(options.data_type);
      }

      if (options.enabled_only === true) {
        query += ` AND is_enabled = 1`;
      }

      const sortBy = options.sort_by || 'name';
      const sortOrder = options.sort_order || 'ASC';
      query += ` ORDER BY ${sortBy} ${sortOrder}`;

      if (options.page && options.limit) {
        const page = parseInt(options.page);
        const limit = parseInt(options.limit);
        const offset = (page - 1) * limit;
        query += ` LIMIT ${limit} OFFSET ${offset}`;
      }

      const result = await this.dbFactory.executeQuery(query, params);
      const dataPoints = Array.isArray(result) ? result : (result.rows || []);
      
      console.log(`디바이스 ${deviceId}의 데이터포인트 ${dataPoints.length}개 조회 성공`);
      
      let pagination = null;
      if (options.page && options.limit) {
        const totalCount = await this.getDataPointCount(deviceId, tenantId, options);
        pagination = {
          page: parseInt(options.page),
          limit: parseInt(options.limit),
          total_items: totalCount,
          has_next: (parseInt(options.page) * parseInt(options.limit)) < totalCount,
          has_prev: parseInt(options.page) > 1
        };
      }
      
      const parsedPoints = dataPoints.map(dp => this.parseDataPoint(dp));
      
      return pagination ? { items: parsedPoints, pagination } : parsedPoints;
      
    } catch (error) {
      console.error('DeviceRepository.getDataPointsByDevice 오류:', error);
      throw new Error(`디바이스 데이터포인트 조회 실패: ${error.message}`);
    }
  }

  async getDataPointCount(deviceId, tenantId = null, options = {}) {
    try {
      let countQuery = `SELECT COUNT(*) as count FROM data_points WHERE device_id = ?`;
      const params = [deviceId];

      if (options.data_type) {
        countQuery += ` AND data_type = ?`;
        params.push(options.data_type);
      }

      if (options.enabled_only === true) {
        countQuery += ` AND is_enabled = 1`;
      }

      const result = await this.dbFactory.executeQuery(countQuery, params);
      const countResult = Array.isArray(result) ? result[0] : (result.rows ? result.rows[0] : result);
      
      return countResult?.count || 0;
    } catch (error) {
      console.error('데이터포인트 개수 조회 실패:', error);
      return 0;
    }
  }

  // =============================================================================
  // 헬퍼 메소드들
  // =============================================================================

  parseDevice(device) {
    if (!device) return null;

    return {
      ...device,
      is_enabled: !!device.is_enabled,
      config: device.config ? JSON.parse(device.config) : {},
      
      protocol: {
        id: device.protocol_id,
        type: device.protocol_type,
        name: device.protocol_name || device.protocol_type
      },
      
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

  parseJsonField(jsonStr) {
    if (!jsonStr) return null;
    try {
      return typeof jsonStr === 'string' ? JSON.parse(jsonStr) : jsonStr;
    } catch (error) {
      console.warn('JSON 파싱 실패:', error.message);
      return null;
    }
  }

  // 통계 계산 헬퍼 메서드들
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
}

module.exports = DeviceRepository;