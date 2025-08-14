// =============================================================================
// backend/routes/devices.js
// 디바이스 관리 통합 API 라우트 - routes/alarms.js 패턴 준수
// =============================================================================

const express = require('express');
const router = express.Router();
const DeviceRepository = require('../lib/database/repositories/DeviceRepository');

// Repository 인스턴스 생성
const deviceRepo = new DeviceRepository();

// =============================================================================
// 디바이스 CRUD API
// =============================================================================

/**
 * GET /api/devices
 * 디바이스 목록 조회 (모든 관련 정보 포함)
 */
router.get('/', async (req, res) => {
  try {
    const filters = {
      tenant_id: req.query.tenant_id,
      site_id: req.query.site_id,
      device_group_id: req.query.device_group_id,
      protocol_type: req.query.protocol_type,
      device_type: req.query.device_type,
      is_enabled: req.query.is_enabled !== undefined ? req.query.is_enabled === 'true' : undefined,
      status: req.query.status,
      search: req.query.search,
      limit: req.query.limit
    };

    // undefined 값 제거
    Object.keys(filters).forEach(key => filters[key] === undefined && delete filters[key]);

    const devices = await deviceRepo.findAllDevices(filters);

    res.json({
      success: true,
      data: devices,
      count: devices.length
    });
  } catch (error) {
    console.error('Error fetching devices:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch devices',
      details: error.message
    });
  }
});

/**
 * GET /api/devices/:id
 * 디바이스 상세 조회 (데이터 포인트 포함)
 */
router.get('/:id', async (req, res) => {
  try {
    const { id } = req.params;
    const device = await deviceRepo.findDeviceById(id);

    if (!device) {
      return res.status(404).json({
        success: false,
        error: 'Device not found'
      });
    }

    res.json({
      success: true,
      data: device
    });
  } catch (error) {
    console.error('Error fetching device:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch device',
      details: error.message
    });
  }
});

/**
 * POST /api/devices
 * 디바이스 생성
 */
router.post('/', async (req, res) => {
  try {
    const deviceData = req.body;

    // 필수 필드 검증
    if (!deviceData.name || !deviceData.protocol_type || !deviceData.endpoint || !deviceData.tenant_id) {
      return res.status(400).json({
        success: false,
        error: 'Missing required fields: name, protocol_type, endpoint, tenant_id'
      });
    }

    const createdDevice = await deviceRepo.createDevice(deviceData);

    res.status(201).json({
      success: true,
      data: createdDevice,
      message: 'Device created successfully'
    });
  } catch (error) {
    console.error('Error creating device:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to create device',
      details: error.message
    });
  }
});

/**
 * PUT /api/devices/:id
 * 디바이스 업데이트
 */
router.put('/:id', async (req, res) => {
  try {
    const { id } = req.params;
    const deviceData = req.body;

    // 존재 확인
    const existing = await deviceRepo.findDeviceById(id);
    if (!existing) {
      return res.status(404).json({
        success: false,
        error: 'Device not found'
      });
    }

    const updatedDevice = await deviceRepo.updateDevice(id, deviceData);

    res.json({
      success: true,
      data: updatedDevice,
      message: 'Device updated successfully'
    });
  } catch (error) {
    console.error('Error updating device:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to update device',
      details: error.message
    });
  }
});

/**
 * DELETE /api/devices/:id
 * 디바이스 삭제
 */
router.delete('/:id', async (req, res) => {
  try {
    const { id } = req.params;

    // 존재 확인
    const existing = await deviceRepo.findDeviceById(id);
    if (!existing) {
      return res.status(404).json({
        success: false,
        error: 'Device not found'
      });
    }

    const deleted = await deviceRepo.deleteDevice(id);

    if (deleted) {
      res.json({
        success: true,
        message: 'Device deleted successfully'
      });
    } else {
      res.status(500).json({
        success: false,
        error: 'Failed to delete device'
      });
    }
  } catch (error) {
    console.error('Error deleting device:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to delete device',
      details: error.message
    });
  }
});

// =============================================================================
// 디바이스 설정 API
// =============================================================================

/**
 * GET /api/devices/:id/settings
 * 디바이스 설정 조회
 */
router.get('/:id/settings', async (req, res) => {
  try {
    const { id } = req.params;
    const settings = await deviceRepo.getDeviceSettings(id);

    if (!settings) {
      return res.status(404).json({
        success: false,
        error: 'Device settings not found'
      });
    }

    res.json({
      success: true,
      data: settings
    });
  } catch (error) {
    console.error('Error fetching device settings:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch device settings',
      details: error.message
    });
  }
});

/**
 * PUT /api/devices/:id/settings
 * 디바이스 설정 업데이트
 */
router.put('/:id/settings', async (req, res) => {
  try {
    const { id } = req.params;
    const settings = req.body;

    await deviceRepo.updateDeviceSettings(null, id, settings);

    const updatedSettings = await deviceRepo.getDeviceSettings(id);

    res.json({
      success: true,
      data: updatedSettings,
      message: 'Device settings updated successfully'
    });
  } catch (error) {
    console.error('Error updating device settings:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to update device settings',
      details: error.message
    });
  }
});

// =============================================================================
// 디바이스 상태 API
// =============================================================================

/**
 * PUT /api/devices/:id/status
 * 디바이스 상태 업데이트
 */
router.put('/:id/status', async (req, res) => {
  try {
    const { id } = req.params;
    const status = req.body;

    await deviceRepo.updateDeviceStatus(id, status);

    res.json({
      success: true,
      message: 'Device status updated successfully'
    });
  } catch (error) {
    console.error('Error updating device status:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to update device status',
      details: error.message
    });
  }
});

// =============================================================================
// 데이터 포인트 API
// =============================================================================

/**
 * GET /api/devices/:id/data-points
 * 디바이스의 데이터 포인트 목록 조회
 */
router.get('/:id/data-points', async (req, res) => {
  try {
    const { id } = req.params;
    const dataPoints = await deviceRepo.getDataPointsByDevice(id);

    res.json({
      success: true,
      data: dataPoints,
      count: dataPoints.length
    });
  } catch (error) {
    console.error('Error fetching data points:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch data points',
      details: error.message
    });
  }
});

/**
 * POST /api/devices/:id/data-points
 * 데이터 포인트 생성
 */
router.post('/:id/data-points', async (req, res) => {
  try {
    const { id } = req.params;
    const dataPointData = { ...req.body, device_id: parseInt(id) };

    // 필수 필드 검증
    if (!dataPointData.name || dataPointData.address === undefined) {
      return res.status(400).json({
        success: false,
        error: 'Missing required fields: name, address'
      });
    }

    const dataPointId = await deviceRepo.createDataPoint(dataPointData);

    res.status(201).json({
      success: true,
      data: { id: dataPointId },
      message: 'Data point created successfully'
    });
  } catch (error) {
    console.error('Error creating data point:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to create data point',
      details: error.message
    });
  }
});

/**
 * PUT /api/devices/:deviceId/data-points/:pointId
 * 데이터 포인트 업데이트
 */
router.put('/:deviceId/data-points/:pointId', async (req, res) => {
  try {
    const { pointId } = req.params;
    const dataPointData = req.body;

    const updated = await deviceRepo.updateDataPoint(pointId, dataPointData);

    if (updated) {
      res.json({
        success: true,
        message: 'Data point updated successfully'
      });
    } else {
      res.status(404).json({
        success: false,
        error: 'Data point not found'
      });
    }
  } catch (error) {
    console.error('Error updating data point:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to update data point',
      details: error.message
    });
  }
});

/**
 * DELETE /api/devices/:deviceId/data-points/:pointId
 * 데이터 포인트 삭제
 */
router.delete('/:deviceId/data-points/:pointId', async (req, res) => {
  try {
    const { pointId } = req.params;

    const deleted = await deviceRepo.deleteDataPoint(pointId);

    if (deleted) {
      res.json({
        success: true,
        message: 'Data point deleted successfully'
      });
    } else {
      res.status(404).json({
        success: false,
        error: 'Data point not found'
      });
    }
  } catch (error) {
    console.error('Error deleting data point:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to delete data point',
      details: error.message
    });
  }
});

// =============================================================================
// 현재값 API
// =============================================================================

/**
 * GET /api/devices/:id/current-values
 * 디바이스의 모든 현재값 조회
 */
router.get('/:id/current-values', async (req, res) => {
  try {
    const { id } = req.params;
    const currentValues = await deviceRepo.getCurrentValuesByDevice(id);

    res.json({
      success: true,
      data: currentValues,
      count: currentValues.length
    });
  } catch (error) {
    console.error('Error fetching current values:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch current values',
      details: error.message
    });
  }
});

/**
 * PUT /api/devices/:deviceId/data-points/:pointId/value
 * 특정 데이터 포인트의 현재값 업데이트
 */
router.put('/:deviceId/data-points/:pointId/value', async (req, res) => {
  try {
    const { pointId } = req.params;
    const valueData = req.body;

    await deviceRepo.updateCurrentValue(pointId, valueData);

    res.json({
      success: true,
      message: 'Current value updated successfully'
    });
  } catch (error) {
    console.error('Error updating current value:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to update current value',
      details: error.message
    });
  }
});

// =============================================================================
// 통계 및 모니터링 API
// =============================================================================

/**
 * GET /api/devices/stats/protocol
 * 프로토콜별 디바이스 통계
 */
router.get('/stats/protocol', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id;
    
    if (!tenantId) {
      return res.status(400).json({
        success: false,
        error: 'tenant_id is required'
      });
    }

    const stats = await deviceRepo.getDeviceStatsByProtocol(tenantId);

    res.json({
      success: true,
      data: stats
    });
  } catch (error) {
    console.error('Error fetching protocol stats:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch protocol stats',
      details: error.message
    });
  }
});

/**
 * GET /api/devices/stats/site
 * 사이트별 디바이스 통계
 */
router.get('/stats/site', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id;
    
    if (!tenantId) {
      return res.status(400).json({
        success: false,
        error: 'tenant_id is required'
      });
    }

    const stats = await deviceRepo.getDeviceStatsBySite(tenantId);

    res.json({
      success: true,
      data: stats
    });
  } catch (error) {
    console.error('Error fetching site stats:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch site stats',
      details: error.message
    });
  }
});

/**
 * GET /api/devices/stats/summary
 * 전체 시스템 상태 요약
 */
router.get('/stats/summary', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id;
    
    if (!tenantId) {
      return res.status(400).json({
        success: false,
        error: 'tenant_id is required'
      });
    }

    const summary = await deviceRepo.getSystemStatusSummary(tenantId);

    res.json({
      success: true,
      data: summary
    });
  } catch (error) {
    console.error('Error fetching system summary:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch system summary',
      details: error.message
    });
  }
});

/**
 * GET /api/devices/stats/recent-active
 * 최근 활동한 디바이스 목록
 */
router.get('/stats/recent-active', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id;
    const limit = parseInt(req.query.limit) || 10;
    
    if (!tenantId) {
      return res.status(400).json({
        success: false,
        error: 'tenant_id is required'
      });
    }

    const devices = await deviceRepo.getRecentActiveDevices(tenantId, limit);

    res.json({
      success: true,
      data: devices,
      count: devices.length
    });
  } catch (error) {
    console.error('Error fetching recent active devices:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch recent active devices',
      details: error.message
    });
  }
});

/**
 * GET /api/devices/stats/errors
 * 오류가 있는 디바이스 목록
 */
router.get('/stats/errors', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id;
    
    if (!tenantId) {
      return res.status(400).json({
        success: false,
        error: 'tenant_id is required'
      });
    }

    const devices = await deviceRepo.getDevicesWithErrors(tenantId);

    res.json({
      success: true,
      data: devices,
      count: devices.length
    });
  } catch (error) {
    console.error('Error fetching devices with errors:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch devices with errors',
      details: error.message
    });
  }
});

/**
 * GET /api/devices/stats/response-time
 * 응답 시간 통계
 */
router.get('/stats/response-time', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id;
    
    if (!tenantId) {
      return res.status(400).json({
        success: false,
        error: 'tenant_id is required'
      });
    }

    const stats = await deviceRepo.getResponseTimeStats(tenantId);

    res.json({
      success: true,
      data: stats
    });
  } catch (error) {
    console.error('Error fetching response time stats:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch response time stats',
      details: error.message
    });
  }
});

// =============================================================================
// 고급 검색 API
// =============================================================================

/**
 * GET /api/devices/search/data-points
 * 데이터 포인트 검색 (크로스 디바이스)
 */
router.get('/search/data-points', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id;
    const searchTerm = req.query.q;
    
    if (!tenantId) {
      return res.status(400).json({
        success: false,
        error: 'tenant_id is required'
      });
    }

    if (!searchTerm) {
      return res.status(400).json({
        success: false,
        error: 'Search term (q) is required'
      });
    }

    const dataPoints = await deviceRepo.searchDataPoints(tenantId, searchTerm);

    res.json({
      success: true,
      data: dataPoints,
      count: dataPoints.length,
      search_term: searchTerm
    });
  } catch (error) {
    console.error('Error searching data points:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to search data points',
      details: error.message
    });
  }
});

// =============================================================================
// 디바이스 제어 API
// =============================================================================

/**
 * POST /api/devices/:id/enable
 * 디바이스 활성화
 */
router.post('/:id/enable', async (req, res) => {
  try {
    const { id } = req.params;

    const device = await deviceRepo.findDeviceById(id);
    if (!device) {
      return res.status(404).json({
        success: false,
        error: 'Device not found'
      });
    }

    await deviceRepo.updateDevice(id, { ...device, is_enabled: true });

    res.json({
      success: true,
      message: 'Device enabled successfully'
    });
  } catch (error) {
    console.error('Error enabling device:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to enable device',
      details: error.message
    });
  }
});

/**
 * POST /api/devices/:id/disable
 * 디바이스 비활성화
 */
router.post('/:id/disable', async (req, res) => {
  try {
    const { id } = req.params;

    const device = await deviceRepo.findDeviceById(id);
    if (!device) {
      return res.status(404).json({
        success: false,
        error: 'Device not found'
      });
    }

    await deviceRepo.updateDevice(id, { ...device, is_enabled: false });

    res.json({
      success: true,
      message: 'Device disabled successfully'
    });
  } catch (error) {
    console.error('Error disabling device:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to disable device',
      details: error.message
    });
  }
});

/**
 * POST /api/devices/:id/restart
 * 디바이스 재시작 요청 (시뮬레이션)
 */
router.post('/:id/restart', async (req, res) => {
  try {
    const { id } = req.params;

    const device = await deviceRepo.findDeviceById(id);
    if (!device) {
      return res.status(404).json({
        success: false,
        error: 'Device not found'
      });
    }

    // 상태를 재시작 중으로 업데이트
    await deviceRepo.updateDeviceStatus(id, {
      status: 'restarting',
      last_seen: new Date(),
      last_error: null
    });

    // 실제 구현에서는 여기서 디바이스 재시작 명령을 전송

    res.json({
      success: true,
      message: 'Device restart requested successfully'
    });
  } catch (error) {
    console.error('Error restarting device:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to restart device',
      details: error.message
    });
  }
});

/**
 * POST /api/devices/:id/test-connection
 * 디바이스 연결 테스트
 */
router.post('/:id/test-connection', async (req, res) => {
  try {
    const { id } = req.params;

    const device = await deviceRepo.findDeviceById(id);
    if (!device) {
      return res.status(404).json({
        success: false,
        error: 'Device not found'
      });
    }

    // 연결 테스트 시뮬레이션
    const startTime = Date.now();
    const success = Math.random() > 0.1; // 90% 성공률
    const responseTime = startTime + Math.floor(Math.random() * 1000) + 50;

    if (success) {
      await deviceRepo.updateDeviceStatus(id, {
        status: 'online',
        last_seen: new Date(),
        response_time: responseTime - startTime,
        last_error: null
      });

      res.json({
        success: true,
        data: {
          connection_status: 'online',
          response_time: responseTime - startTime,
          tested_at: new Date()
        },
        message: 'Connection test successful'
      });
    } else {
      const error = 'Connection timeout';
      await deviceRepo.updateDeviceStatus(id, {
        status: 'offline',
        last_error: error,
        response_time: null
      });

      res.json({
        success: false,
        data: {
          connection_status: 'offline',
          error: error,
          tested_at: new Date()
        },
        message: 'Connection test failed'
      });
    }
  } catch (error) {
    console.error('Error testing device connection:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to test device connection',
      details: error.message
    });
  }
});

// =============================================================================
// 배치 작업 API
// =============================================================================

/**
 * POST /api/devices/batch/enable
 * 디바이스 일괄 활성화
 */
router.post('/batch/enable', async (req, res) => {
  try {
    const { device_ids } = req.body;

    if (!device_ids || !Array.isArray(device_ids) || device_ids.length === 0) {
      return res.status(400).json({
        success: false,
        error: 'device_ids array is required'
      });
    }

    const results = [];
    for (const deviceId of device_ids) {
      try {
        const device = await deviceRepo.findDeviceById(deviceId);
        if (device) {
          await deviceRepo.updateDevice(deviceId, { ...device, is_enabled: true });
          results.push({ device_id: deviceId, success: true });
        } else {
          results.push({ device_id: deviceId, success: false, error: 'Device not found' });
        }
      } catch (error) {
        results.push({ device_id: deviceId, success: false, error: error.message });
      }
    }

    const successCount = results.filter(r => r.success).length;

    res.json({
      success: true,
      data: {
        total: device_ids.length,
        successful: successCount,
        failed: device_ids.length - successCount,
        results: results
      },
      message: `${successCount}/${device_ids.length} devices enabled successfully`
    });
  } catch (error) {
    console.error('Error in batch enable:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to enable devices',
      details: error.message
    });
  }
});

/**
 * POST /api/devices/batch/disable
 * 디바이스 일괄 비활성화
 */
router.post('/batch/disable', async (req, res) => {
  try {
    const { device_ids } = req.body;

    if (!device_ids || !Array.isArray(device_ids) || device_ids.length === 0) {
      return res.status(400).json({
        success: false,
        error: 'device_ids array is required'
      });
    }

    const results = [];
    for (const deviceId of device_ids) {
      try {
        const device = await deviceRepo.findDeviceById(deviceId);
        if (device) {
          await deviceRepo.updateDevice(deviceId, { ...device, is_enabled: false });
          results.push({ device_id: deviceId, success: true });
        } else {
          results.push({ device_id: deviceId, success: false, error: 'Device not found' });
        }
      } catch (error) {
        results.push({ device_id: deviceId, success: false, error: error.message });
      }
    }

    const successCount = results.filter(r => r.success).length;

    res.json({
      success: true,
      data: {
        total: device_ids.length,
        successful: successCount,
        failed: device_ids.length - successCount,
        results: results
      },
      message: `${successCount}/${device_ids.length} devices disabled successfully`
    });
  } catch (error) {
    console.error('Error in batch disable:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to disable devices',
      details: error.message
    });
  }
});

/**
 * POST /api/devices/batch/update-settings
 * 디바이스 설정 일괄 업데이트
 */
router.post('/batch/update-settings', async (req, res) => {
  try {
    const { device_ids, settings } = req.body;

    if (!device_ids || !Array.isArray(device_ids) || device_ids.length === 0) {
      return res.status(400).json({
        success: false,
        error: 'device_ids array is required'
      });
    }

    if (!settings) {
      return res.status(400).json({
        success: false,
        error: 'settings object is required'
      });
    }

    const results = [];
    for (const deviceId of device_ids) {
      try {
        await deviceRepo.updateDeviceSettings(null, deviceId, settings);
        results.push({ device_id: deviceId, success: true });
      } catch (error) {
        results.push({ device_id: deviceId, success: false, error: error.message });
      }
    }

    const successCount = results.filter(r => r.success).length;

    res.json({
      success: true,
      data: {
        total: device_ids.length,
        successful: successCount,
        failed: device_ids.length - successCount,
        results: results
      },
      message: `${successCount}/${device_ids.length} device settings updated successfully`
    });
  } catch (error) {
    console.error('Error in batch settings update:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to update device settings',
      details: error.message
    });
  }
});

// =============================================================================
// 헬스체크 및 진단 API
// =============================================================================

/**
 * GET /api/devices/health
 * 전체 디바이스 헬스체크
 */
router.get('/health', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id;
    
    if (!tenantId) {
      return res.status(400).json({
        success: false,
        error: 'tenant_id is required'
      });
    }

    const [summary, protocolStats, siteStats, recentActive, errors] = await Promise.all([
      deviceRepo.getSystemStatusSummary(tenantId),
      deviceRepo.getDeviceStatsByProtocol(tenantId),
      deviceRepo.getDeviceStatsBySite(tenantId),
      deviceRepo.getRecentActiveDevices(tenantId, 5),
      deviceRepo.getDevicesWithErrors(tenantId)
    ]);

    const healthScore = summary.total_devices > 0 
      ? Math.round((summary.online_devices / summary.total_devices) * 100)
      : 0;

    res.json({
      success: true,
      data: {
        health_score: healthScore,
        summary,
        protocol_stats: protocolStats,
        site_stats: siteStats,
        recent_active: recentActive,
        errors: errors.slice(0, 10), // 최근 10개 오류만
        last_updated: new Date()
      }
    });
  } catch (error) {
    console.error('Error fetching device health:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch device health',
      details: error.message
    });
  }
});

module.exports = router;