// backend/routes/services.js
// 웹에서 서비스 제어를 위한 API 엔드포인트

const express = require('express');
const router = express.Router();
const ServiceManager = require('../lib/services/serviceManager');

// ========================================
// 서비스 상태 조회
// ========================================

// 모든 서비스 상태 조회
router.get('/status', async (req, res) => {
  try {
    const status = ServiceManager.getAllServicesStatus();
    res.json({
      success: true,
      data: status
    });
  } catch (error) {
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

// 특정 서비스 상태 조회
router.get('/status/:serviceName', (req, res) => {
  try {
    const { serviceName } = req.params;
    const status = ServiceManager.getServiceStatus(serviceName);
    
    res.json({
      success: true,
      service: serviceName,
      data: status
    });
  } catch (error) {
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

// ========================================
// 로컬 서비스 제어
// ========================================

// Collector 서비스 시작
router.post('/collector/start', async (req, res) => {
  try {
    const config = req.body.config || {};
    const result = await ServiceManager.startCollector(config);
    
    if (result.success) {
      res.json({
        success: true,
        message: 'Collector started successfully',
        pid: result.pid
      });
    } else {
      res.status(400).json({
        success: false,
        error: result.error
      });
    }
  } catch (error) {
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

// Collector 서비스 중지
router.post('/collector/stop', async (req, res) => {
  try {
    const result = await ServiceManager.stopCollector();
    
    if (result.success) {
      res.json({
        success: true,
        message: 'Collector stopped successfully'
      });
    } else {
      res.status(400).json({
        success: false,
        error: result.error
      });
    }
  } catch (error) {
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

// Collector 서비스 재시작
router.post('/collector/restart', async (req, res) => {
  try {
    const config = req.body.config || {};
    const result = await ServiceManager.restartCollector(config);
    
    if (result.success) {
      res.json({
        success: true,
        message: 'Collector restarted successfully',
        pid: result.pid
      });
    } else {
      res.status(400).json({
        success: false,
        error: result.error
      });
    }
  } catch (error) {
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

// ========================================
// 원격 노드 관리
// ========================================

// 원격 노드 등록
router.post('/nodes/register', async (req, res) => {
  try {
    const nodeInfo = req.body;
    const result = await ServiceManager.registerRemoteNode(nodeInfo);
    
    res.json({
      success: true,
      message: 'Remote node registered successfully',
      nodeId: nodeInfo.nodeId
    });
  } catch (error) {
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

// 원격 서비스 제어
router.post('/nodes/:nodeId/services/:serviceName/:action', async (req, res) => {
  try {
    const { nodeId, serviceName, action } = req.params;
    const config = req.body.config || {};
    
    const result = await ServiceManager.controlRemoteService(nodeId, serviceName, action, config);
    
    if (result.success) {
      res.json({
        success: true,
        message: `${action} ${serviceName} on ${nodeId} successful`,
        data: result.data
      });
    } else {
      res.status(400).json({
        success: false,
        error: result.error
      });
    }
  } catch (error) {
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

// ========================================
// 설정 관리
// ========================================

// 서비스 설정 저장
router.post('/config/:serviceName', async (req, res) => {
  try {
    const { serviceName } = req.params;
    const config = req.body;
    
    await ServiceManager.saveServiceConfig(serviceName, config);
    
    res.json({
      success: true,
      message: `Configuration saved for ${serviceName}`
    });
  } catch (error) {
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

// 서비스 설정 조회
router.get('/config/:serviceName', (req, res) => {
  try {
    const { serviceName } = req.params;
    const config = ServiceManager.serviceConfigs.get(serviceName) || {};
    
    res.json({
      success: true,
      service: serviceName,
      config: config
    });
  } catch (error) {
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

// ========================================
// 헬스 체크
// ========================================

// 전체 시스템 헬스 체크
router.get('/health', async (req, res) => {
  try {
    const health = await ServiceManager.performHealthCheck();
    
    const statusCode = health.overall_status === 'healthy' ? 200 : 
                      health.overall_status === 'degraded' ? 206 : 503;
    
    res.status(statusCode).json({
      success: true,
      health: health
    });
  } catch (error) {
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

// ========================================
// 실시간 서비스 로그
// ========================================

// WebSocket을 통한 실시간 로그 스트리밍
router.get('/logs/:serviceName/stream', (req, res) => {
  // WebSocket 연결로 업그레이드
  if (req.headers.upgrade === 'websocket') {
    // WebSocket 핸들러는 별도 파일에서 처리
    res.status(400).json({
      error: 'WebSocket connection required'
    });
  } else {
    res.status(400).json({
      error: 'WebSocket upgrade required'
    });
  }
});

// 서비스 로그 히스토리 조회
router.get('/logs/:serviceName', (req, res) => {
  try {
    const { serviceName } = req.params;
    const { lines = 100 } = req.query;
    
    // 로그 파일에서 최근 라인 읽기
    const logs = ServiceManager.getServiceLogs(serviceName, parseInt(lines));
    
    res.json({
      success: true,
      service: serviceName,
      logs: logs
    });
  } catch (error) {
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

module.exports = router;