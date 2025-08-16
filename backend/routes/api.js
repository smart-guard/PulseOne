// routes/api.js
const express = require('express');
const router = express.Router();

// Connection 모듈들 import
const redisClient = require('../lib/connection/redis');
const postgres = require('../lib/connection/postgres');
const { writePoint } = require('../lib/connection/influx');
const { sendToQueue } = require('../lib/connection/mq');

// API 정보
router.get('/info', (req, res) => {
  res.json({
    name: 'PulseOne Backend API',
    version: '1.0.0',
    description: 'IoT 데이터 수집 시스템 백엔드',
    environment: process.env.NODE_ENV || 'development',
    timestamp: new Date().toISOString(),
    endpoints: {
      health: '/health',
      info: '/api/info',
      redis_test: '/api/redis/test',
      redis_status: '/api/redis/status',
      redis_stats: '/api/redis/stats',
      redis_tree: '/api/redis/tree',
      db_test: '/api/db/test',
      mq_test: '/api/mq/test'
    }
  });
});

// =============================================================================
// 테스트 엔드포인트들 (개발용)
// =============================================================================

// Redis 테스트
router.get('/redis/test', async (req, res) => {
  try {
    const testKey = `test_${Date.now()}`;
    const testValue = 'redis_test_ok';
    
    // 쓰기 테스트
    await redisClient.set(testKey, testValue, { EX: 10 });
    
    // 읽기 테스트
    const result = await redisClient.get(testKey);
    
    if (result === testValue) {
      res.json({
        status: 'success',
        message: 'Redis 읽기/쓰기 테스트 성공',
        test_key: testKey,
        result: result,
        timestamp: new Date().toISOString()
      });
    } else {
      throw new Error('읽기 테스트 실패');
    }
  } catch (error) {
    console.error('Redis 테스트 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: 'Redis 테스트 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// PostgreSQL 테스트
router.get('/db/test', async (req, res) => {
  try {
    const result = await postgres.query(`
      SELECT 
        NOW() as current_time, 
        version() as db_version,
        current_database() as database_name
    `);
    
    res.json({
      status: 'success',
      message: 'PostgreSQL 테스트 성공',
      data: result.rows[0],
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('PostgreSQL 테스트 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: 'PostgreSQL 테스트 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// InfluxDB 테스트
router.get('/influx/test', async (req, res) => {
  try {
    // 테스트 포인트 작성
    writePoint('api_test', 
      { endpoint: '/api/influx/test', method: 'GET' }, 
      { value: 1, response_time: Math.random() * 100 }
    );
    
    res.json({
      status: 'success',
      message: 'InfluxDB 포인트 작성 성공',
      measurement: 'api_test',
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('InfluxDB 테스트 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: 'InfluxDB 테스트 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// RabbitMQ 테스트
router.get('/mq/test', async (req, res) => {
  try {
    const testMessage = {
      type: 'test_message',
      content: 'Hello from Backend API',
      timestamp: new Date().toISOString(),
      source: 'api_test'
    };

    const success = await sendToQueue('test_queue', testMessage);
    
    if (success) {
      res.json({
        status: 'success',
        message: 'RabbitMQ 메시지 전송 성공',
        queue: 'test_queue',
        sent_message: testMessage
      });
    } else {
      throw new Error('메시지 전송 실패');
    }
  } catch (error) {
    console.error('RabbitMQ 테스트 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: 'RabbitMQ 테스트 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// =============================================================================
// 🗄️ Redis 데이터 탐색 API (DataExplorer용) - 새로 추가
// =============================================================================

// Redis 연결 상태 확인
router.get('/redis/status', async (req, res) => {
  try {
    // Redis 클라이언트 상태 확인
    const info = await redisClient.ping();
    
    res.json({
      success: true,
      data: {
        status: 'connected',
        info: {
          ping: info,
          ready: redisClient.isReady || true,
          connected: redisClient.isOpen || true
        }
      },
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('Redis 상태 확인 실패:', error);
    res.json({
      success: false,
      data: {
        status: 'disconnected'
      },
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// Redis 통계 정보 조회
router.get('/redis/stats', async (req, res) => {
  try {
    // Redis INFO 명령으로 통계 조회 (시뮬레이션)
    const stats = {
      total_keys: Math.floor(Math.random() * 1000) + 100,
      memory_usage: Math.floor(Math.random() * 100) * 1024 * 1024, // bytes
      connected_clients: Math.floor(Math.random() * 10) + 1,
      commands_processed: Math.floor(Math.random() * 100000) + 10000,
      hits: Math.floor(Math.random() * 50000) + 5000,
      misses: Math.floor(Math.random() * 5000) + 500,
      expired_keys: Math.floor(Math.random() * 100) + 10
    };
    
    res.json({
      success: true,
      data: stats,
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('Redis 통계 조회 실패:', error);
    res.status(500).json({
      success: false,
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// Redis 키 트리 구조 조회
router.get('/redis/tree', async (req, res) => {
  try {
    const { parent_path } = req.query;
    
    // 시뮬레이션 트리 데이터 생성
    let treeData = [];
    
    if (!parent_path) {
      // 루트 레벨 - 테넌트들
      treeData = [
        {
          id: 'tenant_1',
          name: 'Samsung Electronics',
          path: 'pulseone:samsung',
          type: 'tenant',
          isExpanded: false,
          isLoaded: false,
          childCount: 3
        },
        {
          id: 'tenant_2', 
          name: 'LG Electronics',
          path: 'pulseone:lg',
          type: 'tenant',
          isExpanded: false,
          isLoaded: false,
          childCount: 2
        },
        {
          id: 'tenant_3',
          name: 'Hyundai Motor',
          path: 'pulseone:hyundai',
          type: 'tenant',
          isExpanded: false,
          isLoaded: false,
          childCount: 4
        }
      ];
    }
    
    res.json({
      success: true,
      data: treeData,
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('Redis 트리 조회 실패:', error);
    res.status(500).json({
      success: false,
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// 특정 노드의 자식 노드들 조회
router.get('/redis/tree/:nodeId/children', async (req, res) => {
  try {
    const { nodeId } = req.params;
    let children = [];
    
    // 시뮬레이션 자식 노드 생성
    if (nodeId.startsWith('tenant_')) {
      // 테넌트 하위 - 사이트들
      children = [
        {
          id: `${nodeId}_site_1`,
          name: 'Seoul Factory',
          path: `pulseone:${nodeId}:seoul_fab`,
          type: 'site',
          isExpanded: false,
          isLoaded: false,
          childCount: 5
        },
        {
          id: `${nodeId}_site_2`,
          name: 'Busan Factory', 
          path: `pulseone:${nodeId}:busan_fab`,
          type: 'site',
          isExpanded: false,
          isLoaded: false,
          childCount: 3
        }
      ];
    } else if (nodeId.includes('_site_')) {
      // 사이트 하위 - 디바이스들
      children = [
        {
          id: `${nodeId}_device_1`,
          name: 'Main PLC #001',
          path: `${nodeId}:plc001`,
          type: 'device',
          isExpanded: false,
          isLoaded: false,
          childCount: 15
        },
        {
          id: `${nodeId}_device_2`,
          name: 'Backup PLC #002',
          path: `${nodeId}:plc002`, 
          type: 'device',
          isExpanded: false,
          isLoaded: false,
          childCount: 12
        },
        {
          id: `${nodeId}_device_3`,
          name: 'Operator HMI #001',
          path: `${nodeId}:hmi001`,
          type: 'device',
          isExpanded: false,
          isLoaded: false,
          childCount: 8
        }
      ];
    } else if (nodeId.includes('_device_')) {
      // 디바이스 하위 - 데이터포인트들
      const dataPointNames = [
        'temperature_01', 'temperature_02', 'pressure_main', 'pressure_backup',
        'motor_speed', 'motor_current', 'vibration_x', 'vibration_y',
        'flow_rate', 'level_tank1', 'level_tank2', 'ph_value',
        'conductivity', 'turbidity', 'alarm_status'
      ];
      
      children = dataPointNames.slice(0, 10).map((name, index) => ({
        id: `${nodeId}_dp_${index}`,
        name: name.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase()),
        path: `${nodeId}:${name}`,
        type: 'datapoint',
        isExpanded: false,
        isLoaded: true,
        dataPoint: generateSimulationDataPoint(`${nodeId}:${name}`, name)
      }));
    }
    
    res.json({
      success: true,
      data: children,
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('노드 자식 조회 실패:', error);
    res.status(500).json({
      success: false,
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// Redis 키 검색
router.get('/redis/keys/search', async (req, res) => {
  try {
    const { pattern, limit = 100 } = req.query;
    
    // 시뮬레이션 검색 결과
    const simulationKeys = [
      'pulseone:samsung:seoul_fab:plc001:temperature_01',
      'pulseone:samsung:seoul_fab:plc001:temperature_02', 
      'pulseone:samsung:seoul_fab:plc001:pressure_main',
      'pulseone:lg:busan_fab:plc002:motor_speed',
      'pulseone:lg:busan_fab:plc002:motor_current',
      'pulseone:hyundai:ulsan_fab:hmi001:flow_rate',
      'pulseone:samsung:seoul_fab:plc001:vibration_x',
      'pulseone:samsung:seoul_fab:plc001:vibration_y',
      'pulseone:lg:busan_fab:hmi001:level_tank1',
      'pulseone:hyundai:ulsan_fab:plc003:ph_value'
    ];
    
    let keys = simulationKeys;
    if (pattern) {
      keys = simulationKeys.filter(key => 
        key.toLowerCase().includes(pattern.toLowerCase())
      );
    }
    
    res.json({
      success: true,
      data: {
        keys: keys.slice(0, parseInt(limit)),
        total: keys.length,
        cursor: keys.length > limit ? 'next_page' : null
      },
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('Redis 키 검색 실패:', error);
    res.status(500).json({
      success: false,
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// 특정 키의 데이터 조회
router.get('/redis/keys/:key/data', async (req, res) => {
  try {
    const { key } = req.params;
    const decodedKey = decodeURIComponent(key);
    
    // 시뮬레이션 데이터 포인트 생성
    const dataPoint = generateSimulationDataPoint(decodedKey, decodedKey.split(':').pop());
    
    res.json({
      success: true,
      data: dataPoint,
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('키 데이터 조회 실패:', error);
    res.status(500).json({
      success: false,
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// 여러 키의 데이터 일괄 조회
router.post('/redis/keys/bulk', async (req, res) => {
  try {
    const { keys } = req.body;
    
    if (!Array.isArray(keys)) {
      return res.status(400).json({
        success: false,
        error: 'keys must be an array',
        timestamp: new Date().toISOString()
      });
    }
    
    // 각 키에 대한 시뮬레이션 데이터 생성
    const dataPoints = keys.map(key => 
      generateSimulationDataPoint(key, key.split(':').pop())
    );
    
    res.json({
      success: true,
      data: dataPoints,
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('일괄 키 데이터 조회 실패:', error);
    res.status(500).json({
      success: false,
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// 데이터 내보내기
router.post('/redis/export', async (req, res) => {
  try {
    const { keys, format = 'json' } = req.body;
    
    if (!Array.isArray(keys)) {
      return res.status(400).json({
        success: false,
        error: 'keys must be an array'
      });
    }
    
    // 시뮬레이션 데이터 생성
    const data = keys.map(key => ({
      key,
      name: key.split(':').pop(),
      value: (Math.random() * 100).toFixed(2),
      timestamp: new Date().toISOString(),
      quality: Math.random() > 0.1 ? 'good' : 'uncertain'
    }));
    
    if (format === 'json') {
      res.setHeader('Content-Type', 'application/json');
      res.setHeader('Content-Disposition', 'attachment; filename="redis_export.json"');
      res.send(JSON.stringify(data, null, 2));
    } else {
      res.status(400).json({
        success: false,
        error: 'Unsupported format'
      });
    }
  } catch (error) {
    console.error('데이터 내보내기 실패:', error);
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

// =============================================================================
// 시뮬레이션 데이터 생성 함수
// =============================================================================

function generateSimulationDataPoint(key, name) {
  const dataTypes = ['number', 'boolean', 'string'];
  const qualities = ['good', 'bad', 'uncertain'];
  const units = ['°C', 'bar', 'rpm', 'A', 'mm', 'pH', '%', 'L/min'];
  
  const dataType = dataTypes[Math.floor(Math.random() * dataTypes.length)];
  let value;
  
  switch (dataType) {
    case 'number':
      value = (Math.random() * 100).toFixed(2);
      break;
    case 'boolean':
      value = Math.random() > 0.5;
      break;
    case 'string':
      value = `Status_${Math.floor(Math.random() * 10)}`;
      break;
  }
  
  return {
    id: key,
    key,
    name: name || key.split(':').pop(),
    value,
    dataType,
    unit: dataType === 'number' ? units[Math.floor(Math.random() * units.length)] : undefined,
    timestamp: new Date().toISOString(),
    quality: qualities[Math.floor(Math.random() * qualities.length)],
    size: Math.floor(Math.random() * 1024) + 64,
    ttl: Math.random() > 0.7 ? Math.floor(Math.random() * 3600) : undefined
  };
}

// =============================================================================
// 실제 API 엔드포인트들 (기존)
// =============================================================================

// 데이터 수집 현황
router.get('/data/status', async (req, res) => {
  try {
    // TODO: 실제 데이터 수집 상태 조회 로직
    res.json({
      status: 'success',
      data: {
        total_devices: 8,
        active_devices: 6,
        last_update: new Date().toISOString(),
        data_points_today: Math.floor(Math.random() * 10000) + 1000
      }
    });
  } catch (error) {
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

// 디바이스 목록
router.get('/devices', async (req, res) => {
  try {
    // TODO: 실제 디바이스 목록 조회 로직
    const simulationDevices = [
      { id: 1, name: 'PLC001', type: 'MODBUS_TCP', status: 'connected' },
      { id: 2, name: 'PLC002', type: 'MODBUS_RTU', status: 'connected' },
      { id: 3, name: 'HMI001', type: 'MQTT', status: 'disconnected' }
    ];
    
    res.json({
      status: 'success',
      data: simulationDevices,
      total: simulationDevices.length
    });
  } catch (error) {
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

module.exports = router;