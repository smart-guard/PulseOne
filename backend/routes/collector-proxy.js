// =============================================================================
// backend/routes/collector-proxy.js
// Collector API 프록시 라우트 (개선: 불필요한 연결 체크 감소)
// =============================================================================

const express = require('express');
const router = express.Router();
const { getInstance: getCollectorProxy } = require('../lib/services/CollectorProxyService');

// 헬스 체크 캐시 (불필요한 반복 체크 방지)
let healthCheckCache = {
  timestamp: 0,
  isHealthy: false,
  cacheDuration: 30000,  // 30초 캐시
  error: null
};

// 미들웨어: Collector 연결 확인 (캐싱 + 개발 환경 스킵)
const checkCollectorConnection = async (req, res, next) => {
  // 1. 개발 환경에서 스킵 옵션
  if (process.env.NODE_ENV === 'development' && 
      process.env.SKIP_COLLECTOR_CHECK === 'true') {
    return next();
  }
  
  try {
    const proxy = getCollectorProxy();
    const now = Date.now();
    
    // 2. 캐시된 결과 사용 (30초 이내)
    if (now - healthCheckCache.timestamp < healthCheckCache.cacheDuration) {
      if (!healthCheckCache.isHealthy) {
        // 개발 환경에서는 DEBUG 레벨 로그만
        if (process.env.NODE_ENV === 'development') {
          console.debug(`🔌 Collector unavailable (cached) - ${req.method} ${req.path}`);
        } else {
          console.warn(`🔌 Collector unavailable (cached) - ${req.method} ${req.path}`);
        }
        
        return res.status(503).json({
          success: false,
          error: 'Collector service is unavailable',
          details: healthCheckCache.error || 'Collector 서비스에 연결할 수 없습니다.',
          cached_since: new Date(healthCheckCache.timestamp).toISOString(),
          lastHealthCheck: proxy.getLastHealthCheck(),
          collectorConfig: proxy.getCollectorConfig()
        });
      }
      return next();
    }
    
    // 3. 실제 헬스 체크 수행
    if (!proxy.isCollectorHealthy()) {
      try {
        await proxy.healthCheck();
        
        // 성공: 캐시 업데이트
        healthCheckCache = {
          timestamp: now,
          isHealthy: true,
          cacheDuration: 30000,
          error: null
        };
        
        console.log('✅ Collector 연결 복구됨');
        
      } catch (error) {
        // 실패: 캐시 업데이트
        healthCheckCache = {
          timestamp: now,
          isHealthy: false,
          cacheDuration: 30000,
          error: error.message
        };
        
        // 개발 환경에서는 조용히
        if (process.env.NODE_ENV === 'development') {
          console.debug(`🔌 Collector check failed - ${req.method} ${req.path}`);
        } else {
          console.warn(`🔌 Collector check failed - ${req.method} ${req.path}: ${error.message}`);
        }
        
        return res.status(503).json({
          success: false,
          error: 'Collector service is unavailable',
          details: error.message,
          lastHealthCheck: proxy.getLastHealthCheck(),
          collectorConfig: proxy.getCollectorConfig()
        });
      }
    } else {
      // 이미 healthy 상태
      healthCheckCache = {
        timestamp: now,
        isHealthy: true,
        cacheDuration: 30000,
        error: null
      };
    }
    
    next();
    
  } catch (error) {
    console.error('❌ checkCollectorConnection error:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to check collector connection',
      details: error.message
    });
  }
};

// 공통 에러 핸들러
const handleProxyError = (error, req, res) => {
  // 개발 환경에서는 DEBUG 레벨
  const logFn = process.env.NODE_ENV === 'development' ? console.debug : console.error;
  logFn(`❌ Collector Proxy Error [${req.method} ${req.originalUrl}]:`, error.message);
  
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
// 🔥 1. 디바이스 제어 API
// =============================================================================

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

router.post('/alarms/:alarmId/acknowledge', checkCollectorConnection, async (req, res) => {
  try {
    const { alarmId } = req.params;
    const { user_id, comment } = req.body;
    
    const userId = user_id || req.user?.id || 1;
    
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

// Health 엔드포인트는 체크 미들웨어 없이 직접 호출
router.get('/health', async (req, res) => {
  try {
    const proxy = getCollectorProxy();
    const result = await proxy.healthCheck();
    
    // 성공 시 캐시 업데이트
    healthCheckCache = {
      timestamp: Date.now(),
      isHealthy: true,
      cacheDuration: 30000,
      error: null
    };
    
    res.json({
      success: true,
      status: 'healthy',
      data: result.data,
      last_check: result.timestamp,
      collector_config: proxy.getCollectorConfig()
    });
    
  } catch (error) {
    // 실패 시 캐시 업데이트
    healthCheckCache = {
      timestamp: Date.now(),
      isHealthy: false,
      cacheDuration: 30000,
      error: error.message
    };
    
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