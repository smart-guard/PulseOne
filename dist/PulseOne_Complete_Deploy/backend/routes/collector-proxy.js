// =============================================================================
// backend/routes/collector-proxy.js
// Collector API 프록시 라우트 (모든 Frontend → Collector 요청 처리)
// =============================================================================

const express = require('express');
const router = express.Router();
const { getInstance: getCollectorProxy } = require('../lib/services/CollectorProxyService');

// 미들웨어: Collector 연결 확인
const checkCollectorConnection = async (req, res, next) => {
  try {
    const proxy = getCollectorProxy();
    
    if (!proxy.isCollectorHealthy()) {
      // 연결 상태 체크 시도
      try {
        await proxy.healthCheck();
      } catch (error) {
        return res.status(503).json({
          success: false,
          error: 'Collector service is unavailable',
          details: 'Collector 서비스에 연결할 수 없습니다. 서비스가 실행 중인지 확인하세요.',
          lastHealthCheck: proxy.getLastHealthCheck(),
          collectorConfig: proxy.getCollectorConfig()
        });
      }
    }
    
    next();
  } catch (error) {
    res.status(500).json({
      success: false,
      error: 'Failed to check collector connection',
      details: error.message
    });
  }
};

// 공통 에러 핸들러
const handleProxyError = (error, req, res) => {
  console.error(`❌ Collector Proxy Error [${req.method} ${req.originalUrl}]:`, error);
  
  if (error.name === 'CollectorProxyError') {
    const statusCode = error.status || 500;
    
    res.status(statusCode).json({
      success: false,
      error: `Collector API Error: ${error.operation}`,
      details: error.collectorError || error.message,
      context: error.context,
      collectorResponse: error.collectorResponse
    });
  } else {
    res.status(500).json({
      success: false,
      error: 'Proxy communication failed',
      details: error.message
    });
  }
};

// =============================================================================
// 🔥 1. 디바이스 제어 API (최우선 구현)
// =============================================================================

/**
 * POST /api/collector/devices/:deviceId/start
 * 디바이스 워커 시작
 */
router.post('/devices/:deviceId/start', checkCollectorConnection, async (req, res) => {
  try {
    const { deviceId } = req.params;
    const { force_restart, ...options } = req.body;
    
    console.log(`🚀 Starting device worker: ${deviceId}`);
    
    const proxy = getCollectorProxy();
    const result = await proxy.startDevice(deviceId, { forceRestart: force_restart, ...options });
    
    res.json({
      success: true,
      message: `Device ${deviceId} started successfully`,
      data: result.data,
      device_id: deviceId,
      action: 'start',
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * POST /api/collector/devices/:deviceId/stop
 * 디바이스 워커 중지
 */
router.post('/devices/:deviceId/stop', checkCollectorConnection, async (req, res) => {
  try {
    const { deviceId } = req.params;
    const { graceful, ...options } = req.body;
    
    console.log(`🛑 Stopping device worker: ${deviceId}`);
    
    const proxy = getCollectorProxy();
    const result = await proxy.stopDevice(deviceId, { graceful, ...options });
    
    res.json({
      success: true,
      message: `Device ${deviceId} stopped successfully`,
      data: result.data,
      device_id: deviceId,
      action: 'stop',
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * POST /api/collector/devices/:deviceId/pause
 * 디바이스 워커 일시정지
 */
router.post('/devices/:deviceId/pause', checkCollectorConnection, async (req, res) => {
  try {
    const { deviceId } = req.params;
    
    console.log(`⏸️ Pausing device worker: ${deviceId}`);
    
    const proxy = getCollectorProxy();
    const result = await proxy.pauseDevice(deviceId);
    
    res.json({
      success: true,
      message: `Device ${deviceId} paused successfully`,
      data: result.data,
      device_id: deviceId,
      action: 'pause',
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * POST /api/collector/devices/:deviceId/resume
 * 디바이스 워커 재개
 */
router.post('/devices/:deviceId/resume', checkCollectorConnection, async (req, res) => {
  try {
    const { deviceId } = req.params;
    
    console.log(`▶️ Resuming device worker: ${deviceId}`);
    
    const proxy = getCollectorProxy();
    const result = await proxy.resumeDevice(deviceId);
    
    res.json({
      success: true,
      message: `Device ${deviceId} resumed successfully`,
      data: result.data,
      device_id: deviceId,
      action: 'resume',
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * POST /api/collector/devices/:deviceId/restart
 * 디바이스 워커 재시작
 */
router.post('/devices/:deviceId/restart', checkCollectorConnection, async (req, res) => {
  try {
    const { deviceId } = req.params;
    const { wait, ...options } = req.body;
    
    console.log(`🔄 Restarting device worker: ${deviceId}`);
    
    const proxy = getCollectorProxy();
    const result = await proxy.restartDevice(deviceId, { wait, ...options });
    
    res.json({
      success: true,
      message: `Device ${deviceId} restarted successfully`,
      data: result.data,
      device_id: deviceId,
      action: 'restart',
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * GET /api/collector/devices/:deviceId/status
 * 디바이스 실시간 상태 조회
 */
router.get('/devices/:deviceId/status', checkCollectorConnection, async (req, res) => {
  try {
    const { deviceId } = req.params;
    
    const proxy = getCollectorProxy();
    const result = await proxy.getDeviceStatus(deviceId);
    
    res.json({
      success: true,
      data: result.data,
      device_id: deviceId,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

// =============================================================================
// 🔥 2. 하드웨어 제어 API
// =============================================================================

/**
 * POST /api/collector/devices/:deviceId/digital/:outputId/control
 * 디지털 출력 제어 (릴레이, 솔레노이드 등)
 */
router.post('/devices/:deviceId/digital/:outputId/control', checkCollectorConnection, async (req, res) => {
  try {
    const { deviceId, outputId } = req.params;
    const { state, duration, force } = req.body;
    
    if (state === undefined || state === null) {
      return res.status(400).json({
        success: false,
        error: 'Missing required parameter: state',
        details: 'state must be true or false'
      });
    }
    
    console.log(`🔌 Digital control: Device ${deviceId}, Output ${outputId}, State: ${state}`);
    
    const proxy = getCollectorProxy();
    const result = await proxy.controlDigitalOutput(deviceId, outputId, state, { duration, force });
    
    res.json({
      success: true,
      message: `Digital output ${outputId} set to ${state}`,
      data: result.data,
      device_id: deviceId,
      output_id: outputId,
      state: Boolean(state),
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * POST /api/collector/devices/:deviceId/analog/:outputId/control
 * 아날로그 출력 제어 (4-20mA, 0-10V 등)
 */
router.post('/devices/:deviceId/analog/:outputId/control', checkCollectorConnection, async (req, res) => {
  try {
    const { deviceId, outputId } = req.params;
    const { value, unit, ramp_time } = req.body;
    
    if (value === undefined || value === null) {
      return res.status(400).json({
        success: false,
        error: 'Missing required parameter: value',
        details: 'value must be a number'
      });
    }
    
    console.log(`📊 Analog control: Device ${deviceId}, Output ${outputId}, Value: ${value}`);
    
    const proxy = getCollectorProxy();
    const result = await proxy.controlAnalogOutput(deviceId, outputId, value, { unit, rampTime: ramp_time });
    
    res.json({
      success: true,
      message: `Analog output ${outputId} set to ${value}${unit ? ' ' + unit : ''}`,
      data: result.data,
      device_id: deviceId,
      output_id: outputId,
      value: Number(value),
      unit: unit || null,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * POST /api/collector/devices/:deviceId/parameters/:parameterId/set
 * 디바이스 파라미터 설정
 */
router.post('/devices/:deviceId/parameters/:parameterId/set', checkCollectorConnection, async (req, res) => {
  try {
    const { deviceId, parameterId } = req.params;
    const { value, data_type, validate } = req.body;
    
    if (value === undefined || value === null) {
      return res.status(400).json({
        success: false,
        error: 'Missing required parameter: value'
      });
    }
    
    console.log(`⚙️ Parameter set: Device ${deviceId}, Parameter ${parameterId}, Value: ${value}`);
    
    const proxy = getCollectorProxy();
    const result = await proxy.setParameter(deviceId, parameterId, value, { dataType: data_type, validate });
    
    res.json({
      success: true,
      message: `Parameter ${parameterId} set to ${value}`,
      data: result.data,
      device_id: deviceId,
      parameter_id: parameterId,
      value,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

// =============================================================================
// 🔥 3. 실시간 데이터 API
// =============================================================================

/**
 * GET /api/collector/devices/:deviceId/data/current
 * 디바이스 현재 데이터 조회
 */
router.get('/devices/:deviceId/data/current', checkCollectorConnection, async (req, res) => {
  try {
    const { deviceId } = req.params;
    const { point_ids } = req.query;
    
    const pointIds = point_ids ? point_ids.split(',').map(id => id.trim()) : [];
    
    const proxy = getCollectorProxy();
    const result = await proxy.getCurrentData(deviceId, pointIds);
    
    res.json({
      success: true,
      data: result.data,
      device_id: deviceId,
      point_count: result.data?.points?.length || 0,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * GET /api/collector/alarms/active
 * 활성 알람 목록 조회
 */
router.get('/alarms/active', checkCollectorConnection, async (req, res) => {
  try {
    const { severity, device_id, limit, acknowledged } = req.query;
    
    const options = {
      severity,
      deviceId: device_id,
      limit: limit ? parseInt(limit) : 50,
      acknowledged: acknowledged === 'true'
    };
    
    const proxy = getCollectorProxy();
    const result = await proxy.getActiveAlarms(options);
    
    res.json({
      success: true,
      data: result.data,
      query_options: options,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * POST /api/collector/alarms/:alarmId/acknowledge
 * 알람 확인
 */
router.post('/alarms/:alarmId/acknowledge', checkCollectorConnection, async (req, res) => {
  try {
    const { alarmId } = req.params;
    const { user_id, comment } = req.body;
    
    // 사용자 ID는 세션에서 가져오거나 body에서 받음
    const userId = user_id || req.user?.id || 1; // 기본값 설정
    
    console.log(`✅ Acknowledging alarm ${alarmId} by user ${userId}`);
    
    const proxy = getCollectorProxy();
    const result = await proxy.acknowledgeAlarm(alarmId, userId, comment || '');
    
    res.json({
      success: true,
      message: 'Alarm acknowledged successfully',
      data: result.data,
      alarm_id: alarmId,
      user_id: userId,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * POST /api/collector/alarms/:alarmId/clear
 * 알람 클리어
 */
router.post('/alarms/:alarmId/clear', checkCollectorConnection, async (req, res) => {
  try {
    const { alarmId } = req.params;
    const { user_id, comment } = req.body;
    
    const userId = user_id || req.user?.id || 1;
    
    console.log(`🔄 Clearing alarm ${alarmId} by user ${userId}`);
    
    const proxy = getCollectorProxy();
    const result = await proxy.clearAlarm(alarmId, userId, comment || '');
    
    res.json({
      success: true,
      message: 'Alarm cleared successfully',
      data: result.data,
      alarm_id: alarmId,
      user_id: userId,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

// =============================================================================
// 🔥 4. 설정 동기화 API
// =============================================================================

/**
 * POST /api/collector/devices/:deviceId/config/reload
 * 디바이스 설정 재로드
 */
router.post('/devices/:deviceId/config/reload', checkCollectorConnection, async (req, res) => {
  try {
    const { deviceId } = req.params;
    
    console.log(`🔄 Reloading config for device ${deviceId}`);
    
    const proxy = getCollectorProxy();
    const result = await proxy.reloadDeviceConfig(deviceId);
    
    res.json({
      success: true,
      message: `Configuration reloaded for device ${deviceId}`,
      data: result.data,
      device_id: deviceId,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * POST /api/collector/config/reload
 * 전체 설정 재로드
 */
router.post('/config/reload', checkCollectorConnection, async (req, res) => {
  try {
    console.log('🔄 Reloading all configurations');
    
    const proxy = getCollectorProxy();
    const result = await proxy.reloadAllConfigs();
    
    res.json({
      success: true,
      message: 'All configurations reloaded successfully',
      data: result.data,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * POST /api/collector/devices/:deviceId/sync
 * 디바이스 설정 동기화
 */
router.post('/devices/:deviceId/sync', checkCollectorConnection, async (req, res) => {
  try {
    const { deviceId } = req.params;
    const settings = req.body;
    
    console.log(`🔄 Syncing settings for device ${deviceId}`);
    
    const proxy = getCollectorProxy();
    const result = await proxy.syncDeviceSettings(deviceId, settings);
    
    res.json({
      success: true,
      message: `Settings synchronized for device ${deviceId}`,
      data: result.data,
      device_id: deviceId,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * POST /api/collector/config/notify-change
 * 설정 변경 알림
 */
router.post('/config/notify-change', checkCollectorConnection, async (req, res) => {
  try {
    const { type, entity_id, changes } = req.body;
    
    if (!type || !entity_id) {
      return res.status(400).json({
        success: false,
        error: 'Missing required parameters: type, entity_id'
      });
    }
    
    console.log(`🔔 Notifying config change: ${type} ${entity_id}`);
    
    const proxy = getCollectorProxy();
    const result = await proxy.notifyConfigChange(type, entity_id, changes || {});
    
    res.json({
      success: true,
      message: 'Configuration change notification sent',
      data: result.data,
      change_type: type,
      entity_id,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

// =============================================================================
// 🔥 5. 시스템 관리 API
// =============================================================================

/**
 * GET /api/collector/statistics
 * Collector 통계 정보 조회
 */
router.get('/statistics', checkCollectorConnection, async (req, res) => {
  try {
    const proxy = getCollectorProxy();
    const result = await proxy.getSystemStatistics();
    
    res.json({
      success: true,
      data: result.data,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * GET /api/collector/workers/status
 * 워커 상태 조회
 */
router.get('/workers/status', checkCollectorConnection, async (req, res) => {
  try {
    const proxy = getCollectorProxy();
    const result = await proxy.getWorkerStatus();
    
    res.json({
      success: true,
      data: result.data,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    handleProxyError(error, req, res);
  }
});

/**
 * GET /api/collector/health
 * Collector 헬스 체크
 */
router.get('/health', async (req, res) => {
  try {
    const proxy = getCollectorProxy();
    const result = await proxy.healthCheck();
    
    res.json({
      success: true,
      status: 'healthy',
      data: result.data,
      last_check: result.timestamp,
      collector_config: proxy.getCollectorConfig()
    });
    
  } catch (error) {
    res.status(503).json({
      success: false,
      status: 'unhealthy',
      error: 'Collector health check failed',
      details: error.message,
      last_healthy_check: getCollectorProxy().getLastHealthCheck(),
      collector_config: getCollectorProxy().getCollectorConfig()
    });
  }
});

module.exports = router;