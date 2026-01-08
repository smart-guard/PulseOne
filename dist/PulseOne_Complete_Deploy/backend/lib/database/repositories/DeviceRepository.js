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
      
      // 🔥 수정: await 추가
      return await this.parseDevice(devices[0]);
      
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

        // 🔥 누락 필터 1: 연결상태
        if (filters.connectionStatus || filters.connection_status) {
          query += DeviceQueries.addConnectionStatusFilter();
          params.push(filters.connectionStatus || filters.connection_status);
        }

        // 🔥 누락 필터 2: 상태  
        if (filters.status) {
          if (filters.status === 'running') {
            query += DeviceQueries.addStatusRunningFilter();
          } else if (filters.status === 'stopped') {
            query += DeviceQueries.addStatusStoppedFilter();
          } else if (filters.status === 'error') {
            query += DeviceQueries.addStatusErrorFilter();
          }
        }

        if (filters.search) {
          query += DeviceQueries.addSearchFilter();
          const searchTerm = `%${filters.search}%`;
          params.push(searchTerm, searchTerm, searchTerm, searchTerm);
        }

        // 그룹화 및 정렬 (원본으로 복원)
        query += DeviceQueries.getGroupByAndOrder();

        // 페이징 처리 (기존 그대로)
        const limit = filters.limit || 25;
        const page = filters.page || 1;
        const offset = (page - 1) * limit;
        
        // LIMIT 추가
        query += DeviceQueries.addLimit();
        params.push(limit);
        
        // OFFSET 추가  
        query += DeviceQueries.addOffset();
        params.push(offset);

        console.log(`페이징 설정: page=${page}, limit=${limit}, offset=${offset}`);
        console.log('실행할 쿼리:', query.substring(0, 200) + '...');
        console.log('파라미터:', params.length + '개', params);

        const result = await this.dbFactory.executeQuery(query, params);
        
        // 결과 처리 (다양한 DB 드라이버 대응) - 기존 그대로
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

        console.log(`${devices.length}개 디바이스 조회 완료 (page=${page})`);

        // 데이터 파싱 (기존 그대로)
        const parsedDevices = await Promise.all(devices.map(device => this.parseDevice(device)));

        // 페이징 정보 계산 (기존 그대로)
        const totalCount = filters.page && filters.limit ?
          await this.getDeviceCount(filters) : devices.length;
        
        const pagination = {
            page: parseInt(page),
            limit: parseInt(limit),
            total_items: totalCount,
            has_next: page * limit < totalCount,
            has_prev: page > 1
        };

        console.log('페이지네이션 정보:', pagination);

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


  // =============================================================================
  // 🔥 새로 추가: status 필드 정규화 메서드
  // =============================================================================

  parseDeviceWithStatus(device) {
    try {
      // 기존 파싱
      const parsed = this.parseDevice(device);

      // 🔥 status 필드 정규화 (is_enabled + connection_status 조합)
      let status = 'unknown';
      
      if (parsed.is_enabled === false || parsed.is_enabled === 0) {
        status = 'stopped';  // 비활성화 = stopped
      } else if (parsed.connection_status === 'error') {
        status = 'error';    // 연결오류 = error
      } else if (parsed.connection_status === 'connected') {
        status = 'running';  // 연결됨 + 활성화 = running
      } else if (parsed.connection_status === 'disconnected') {
        status = 'stopped';  // 연결안됨 = stopped
      } else {
        status = parsed.is_enabled ? 'running' : 'stopped';
      }

      // 🔥 클라이언트 호환성을 위한 추가 필드들
      return {
        ...parsed,
        status: status,  // 정규화된 상태
        // 백업 필드들 (기존 코드 호환성)
        worker_status: status,
        device_status: status,
        // 통계 필드들 추가
        data_point_count: parsed.data_point_count || 0,
        enabled_point_count: parsed.enabled_point_count || 0,
        last_seen: parsed.last_communication || parsed.updated_at
      };

    } catch (error) {
      console.warn('Device status parsing 실패:', error.message);
      return this.parseDevice(device); // 기본 파싱만 반환
    }
  }

  // =============================================================================
  // 🔥 수정: getDeviceCount에도 필터링 추가
  // =============================================================================

  async getDeviceCount(filters = {}) {
    try {
        let query = DeviceQueries.getDeviceCount();
        const params = [];

        // 기본 tenant 필터
        query += DeviceQueries.addTenantFilter();
        params.push(filters.tenantId || filters.tenant_id || 1);

        // 🔥 상태 필터링 추가 (기존 패턴 유지)
        if (filters.connectionStatus || filters.connection_status) {
          query += DeviceQueries.addConnectionStatusFilter();
          params.push(filters.connectionStatus || filters.connection_status);
        }

        if (filters.status) {
          if (filters.status === 'running') {
            query += DeviceQueries.addStatusRunningFilter();
          } else if (filters.status === 'stopped') {
            query += DeviceQueries.addStatusStoppedFilter();
          } else if (filters.status === 'error') {
            query += DeviceQueries.addStatusErrorFilter();
          }
        }

        // 기존 필터들
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

  // DeviceRepository.js - parseDevice 메서드 완성본
  // 🔥 settings 필드를 실제 DB에서 조회하도록 수정

  parseDevice(device) {
    if (!device) return null;

    try {
      let parsedConfig = {};
      
      if (device.config) {
        try {
          if (typeof device.config === 'string') {
            if (device.config.startsWith('{') || device.config.startsWith('[')) {
              parsedConfig = JSON.parse(device.config);
            } else {
              console.warn(`Device ${device.id}: Invalid JSON config:`, device.config);
            }
          } else if (typeof device.config === 'object') {
            parsedConfig = device.config;
          }
        } catch (configError) {
          console.warn(`Device ${device.id}: Config parse error:`, configError.message);
        }
      }

      // 🔥 핵심 최적화: JOIN으로 가져온 device_settings 데이터 사용
      const settings = {};
      
      // JOIN 결과에서 settings 필드들이 있으면 사용
      if (device.polling_interval_ms !== undefined && device.polling_interval_ms !== null) {
        settings.polling_interval_ms = device.polling_interval_ms;
        settings.connection_timeout_ms = device.connection_timeout_ms || 5000;
        settings.max_retry_count = device.max_retry_count || 3;
        settings.retry_interval_ms = device.retry_interval_ms || 1000;
        settings.backoff_time_ms = device.backoff_time_ms || 2000;
        settings.keep_alive_enabled = !!device.keep_alive_enabled;
        settings.keep_alive_interval_s = device.keep_alive_interval_s || 30;
      } else {
        // JOIN에서 settings 데이터가 없으면 기본값 사용
        settings.polling_interval_ms = 1000;
        settings.connection_timeout_ms = 5000;
        settings.max_retry_count = 3;
        settings.retry_interval_ms = 1000;
        settings.backoff_time_ms = 2000;
        settings.keep_alive_enabled = false;
        settings.keep_alive_interval_s = 30;
      }

      return {
        id: device.id,
        tenant_id: device.tenant_id,
        site_id: device.site_id,
        device_group_id: device.device_group_id,
        edge_server_id: device.edge_server_id,
        name: device.name,
        description: device.description,
        device_type: device.device_type,
        manufacturer: device.manufacturer,
        model: device.model,
        serial_number: device.serial_number,
        protocol_id: device.protocol_id,
        protocol_type: device.protocol_type,
        endpoint: device.endpoint,
        config: parsedConfig,
        polling_interval: device.polling_interval,
        timeout: device.timeout,
        retry_count: device.retry_count,
        is_enabled: !!device.is_enabled,
        installation_date: device.installation_date,
        last_maintenance: device.last_maintenance,
        created_at: device.created_at,
        updated_at: device.updated_at,
        
        // 프로토콜 정보
        protocol_name: device.protocol_name,
        
        // 상태 정보 (device_status 테이블에서 JOIN으로 가져온 데이터)
        connection_status: device.connection_status || 'disconnected',
        last_communication: device.last_communication,
        error_count: device.error_count || 0,
        last_error: device.last_error,
        response_time: device.response_time,
        firmware_version: device.firmware_version,
        hardware_info: device.hardware_info,
        diagnostic_data: device.diagnostic_data,
        
        // 사이트 정보
        site_name: device.site_name,
        site_code: device.site_code,
        
        // 그룹 정보
        group_name: device.group_name,
        group_type: device.group_type,
        
        // 통계
        data_point_count: parseInt(device.data_point_count) || 0,
        enabled_point_count: parseInt(device.enabled_point_count) || 0,
        
        // 🔥 핵심 수정: JOIN으로 가져온 settings 데이터 사용 (추가 DB 쿼리 없음)
        settings: settings
      };
    } catch (error) {
      console.error('Device parsing 오류:', error);
      return null;
    }
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

  /**
 * 디바이스 설정 업데이트 또는 생성 (UPSERT)
 * @param {number} deviceId - 디바이스 ID
 * @param {Object} settings - 설정 객체
 * @param {number} tenantId - 테넌트 ID
 * @returns {Promise<boolean>} 성공 여부
 */
async updateDeviceSettings(deviceId, settings, tenantId = null) {
    try {
        console.log(`DeviceRepository.updateDeviceSettings: Device ${deviceId}`, settings);
        
        const DeviceQueries = require('../queries/DeviceQueries');
        
        // 기존 DeviceQueries.upsertDeviceSettings() 쿼리 사용
        const query = DeviceQueries.upsertDeviceSettings();
        
        const params = [
            deviceId,
            settings.polling_interval_ms || 1000,
            settings.connection_timeout_ms || 5000,
            settings.max_retry_count || 3,
            settings.retry_interval_ms || 1000,
            settings.backoff_time_ms || 2000,
            settings.keep_alive_enabled ? 1 : 0,
            settings.keep_alive_interval_s || 30
        ];

        const result = await this.dbFactory.executeQuery(query, params);
        
        console.log(`✅ 디바이스 ${deviceId} settings 업데이트 완료`);
        return true;

    } catch (error) {
        console.error('DeviceRepository.updateDeviceSettings 실패:', error.message);
        throw new Error(`디바이스 설정 업데이트 실패: ${error.message}`);
    }
}

/**
 * 디바이스 설정 조회
 * @param {number} deviceId - 디바이스 ID
 * @returns {Promise<Object|null>} 설정 객체 또는 null
 */
async getDeviceSettings(deviceId) {
    try {
        console.log(`DeviceRepository.getDeviceSettings: Device ${deviceId}`);
        
        const DeviceQueries = require('../queries/DeviceQueries');
        const query = DeviceQueries.getDeviceSettings();

        const result = await this.dbFactory.executeQuery(query, [deviceId]);
        const settings = Array.isArray(result) ? result[0] : result;

        if (!settings) {
            console.log(`디바이스 ${deviceId}의 설정이 없음`);
            return null;
        }

        // DB 값을 프론트엔드 형태로 변환
        const formattedSettings = {
            polling_interval_ms: settings.polling_interval_ms || 1000,
            connection_timeout_ms: settings.connection_timeout_ms || 5000,
            max_retry_count: settings.max_retry_count || 3,
            retry_interval_ms: settings.retry_interval_ms || 1000,
            backoff_time_ms: settings.backoff_time_ms || 2000,
            keep_alive_enabled: !!settings.keep_alive_enabled,
            keep_alive_interval_s: settings.keep_alive_interval_s || 30
        };

        console.log(`디바이스 ${deviceId} 설정 조회 완료`);
        return formattedSettings;

    } catch (error) {
        console.error('DeviceRepository.getDeviceSettings 실패:', error.message);
        throw new Error(`디바이스 설정 조회 실패: ${error.message}`);
    }
  }
}

module.exports = DeviceRepository;