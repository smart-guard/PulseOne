// routes/api.js
const express = require('express');
const router = express.Router();

// Connection 모듈들 import (기존 패턴 준수)
const ConfigManager = require('../lib/config/ConfigManager');
const redisClient = require('../lib/connection/redis');
const postgres = require('../lib/connection/postgres');
const { writePoint } = require('../lib/connection/influx');
const { sendToQueue } = require('../lib/connection/mq');

// ConfigManager 인스턴스 가져오기 (기존 패턴 준수)
const config = ConfigManager.getInstance();

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
// 테스트 엔드포인트들 (개발용) - 기존 패턴 유지
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

// 🔥 Redis 연결 상태 즉시 확인용 엔드포인트 (router 패턴 수정)
router.get('/redis-connection-check', async (req, res) => {
  try {
    console.log('🔍 Redis 연결 상태 확인 시작...');
    
    // ConfigManager 가져오기
    let redisConfig;
    try {
      redisConfig = config.getRedisConfig();
      console.log('✅ ConfigManager에서 Redis 설정 로드 성공');
    } catch (configError) {
      console.error('❌ ConfigManager에서 Redis 설정 로드 실패:', configError.message);
      redisConfig = {
        enabled: process.env.REDIS_PRIMARY_ENABLED === 'true',
        host: process.env.REDIS_PRIMARY_HOST || 'localhost',
        port: parseInt(process.env.REDIS_PRIMARY_PORT) || 6379,
        db: parseInt(process.env.REDIS_PRIMARY_DB) || 0,
        error: configError.message
      };
    }
    
    console.log('📋 Redis 설정:', {
      enabled: redisConfig.enabled,
      host: redisConfig.host,
      port: redisConfig.port,
      db: redisConfig.db
    });
    
    // Redis 연결 시도
    let connectionStatus = 'not_tested';
    let connectionError = null;
    let pingResult = null;
    
    try {
      console.log('🔗 Redis 클라이언트 로드 중...');
      
      if (redisClient) {
        console.log('✅ Redis 클라이언트 로드 성공');
        
        if (typeof redisClient.ping === 'function') {
          console.log('🏓 Redis ping 테스트 중...');
          pingResult = await redisClient.ping();
          connectionStatus = 'connected';
          console.log('✅ Redis ping 성공:', pingResult);
        } else if (typeof redisClient.getClient === 'function') {
          console.log('🔗 getClient() 메서드 사용');
          const client = await redisClient.getClient();
          if (client && typeof client.ping === 'function') {
            pingResult = await client.ping();
            connectionStatus = 'connected';
            console.log('✅ Redis ping 성공 (via getClient):', pingResult);
          } else {
            connectionStatus = 'client_ping_unavailable';
          }
        } else {
          connectionStatus = 'ping_method_not_found';
          console.log('⚠️ ping 메서드를 찾을 수 없음');
        }
      } else {
        connectionStatus = 'client_not_loaded';
        console.log('❌ Redis 클라이언트를 로드할 수 없음');
      }
    } catch (error) {
      connectionStatus = 'connection_failed';
      connectionError = error.message;
      console.error('❌ Redis 연결 실패:', error.message);
    }
    
    // 환경변수 확인
    const envVars = {
      NODE_ENV: process.env.NODE_ENV,
      REDIS_PRIMARY_ENABLED: process.env.REDIS_PRIMARY_ENABLED,
      REDIS_PRIMARY_HOST: process.env.REDIS_PRIMARY_HOST,
      REDIS_PRIMARY_PORT: process.env.REDIS_PRIMARY_PORT,
      REDIS_PRIMARY_DB: process.env.REDIS_PRIMARY_DB,
      REDIS_MAIN_HOST: process.env.REDIS_MAIN_HOST,
      REDIS_MAIN_PORT: process.env.REDIS_MAIN_PORT,
      CONFIG_FILES: process.env.CONFIG_FILES
    };
    
    const response = {
      status: 'success',
      message: 'Redis 연결 상태 확인 완료',
      data: {
        connection: {
          status: connectionStatus,
          ping_result: pingResult,
          error: connectionError
        },
        config: redisConfig,
        environment: envVars,
        route_test: {
          endpoint: '/api/redis-connection-check',
          working: true,
          note: 'Router pattern is working correctly'
        }
      },
      timestamp: new Date().toISOString()
    };
    
    console.log('📤 응답 전송 중...');
    res.json(response);
    
  } catch (error) {
    console.error('🚨 Redis 연결 확인 실패:', error);
    res.status(500).json({
      status: 'error',
      message: 'Redis 연결 확인 실패',
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
// 🗄️ Redis 데이터 탐색 API (DataExplorer용) - ConfigManager 패턴 100% 준수
// =============================================================================

// Redis 연결 상태 확인 (ConfigManager 사용)
router.get('/redis/status', async (req, res) => {
  try {
    // ConfigManager를 통해 Redis 설정 가져오기 (기존 패턴 준수)
    const redisConfig = config.getRedisConfig();
    
    console.log('🔧 Redis 설정 확인:', {
      enabled: redisConfig.enabled,
      host: redisConfig.host,
      port: redisConfig.port,
      db: redisConfig.db
    });
    
    // Redis 클라이언트 상태 확인 시도
    let pingResult = 'PONG';
    let actuallyConnected = false;
    
    try {
      if (redisClient && typeof redisClient.ping === 'function') {
        pingResult = await redisClient.ping();
        actuallyConnected = true;
      } else if (redisClient && typeof redisClient.getClient === 'function') {
        const client = await redisClient.getClient();
        if (client && typeof client.ping === 'function') {
          pingResult = await client.ping();
          actuallyConnected = true;
        }
      }
    } catch (err) {
      console.warn('🔥 Redis ping 실패, 시뮬레이션 모드 사용:', err.message);
      actuallyConnected = false;
    }
    
    res.json({
      status: 'success',
      message: 'Redis 연결 상태 확인 완료',
      data: {
        status: actuallyConnected ? 'connected' : 'simulated',
        ping: pingResult,
        ready: actuallyConnected,
        connected: actuallyConnected,
        config: {
          enabled: redisConfig.enabled,
          host: redisConfig.host,
          port: redisConfig.port,
          database: redisConfig.db,
          keyPrefix: redisConfig.keyPrefix,
          testMode: redisConfig.testMode
        },
        stats: {
          total_keys: 537,
          memory_usage: '45.2MB',
          uptime_seconds: Math.floor(Math.random() * 100000) + 86400
        }
      },
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('Redis 상태 확인 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: 'Redis 상태 확인 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// Redis 통계 정보 조회 (ConfigManager 패턴 준수)
router.get('/redis/stats', async (req, res) => {
  try {
    // ConfigManager에서 Redis 설정 읽기
    const redisConfig = config.getRedisConfig();
    
    // 시뮬레이션 통계 데이터 (기존 패턴 준수)
    const stats = {
      connection: {
        host: redisConfig.host,
        port: redisConfig.port,
        database: redisConfig.db,
        status: redisConfig.enabled ? 'enabled' : 'disabled'
      },
      performance: {
        total_keys: Math.floor(Math.random() * 1000) + 537,
        memory: {
          used: '45.2MB',
          peak: '52.1MB',
          total: '512MB',
          used_bytes: Math.floor(Math.random() * 100 * 1024 * 1024) + 45 * 1024 * 1024
        },
        connections: {
          clients: Math.floor(Math.random() * 10) + 12,
          blocked: Math.floor(Math.random() * 3),
          max_clients: 1000
        },
        operations: {
          total_commands: Math.floor(Math.random() * 100000) + 125847,
          commands_per_sec: Math.floor(Math.random() * 200) + 143,
          keyspace_hits: Math.floor(Math.random() * 50000) + 98234,
          keyspace_misses: Math.floor(Math.random() * 5000) + 1247,
          hit_rate: '98.7%'
        }
      },
      uptime: Math.floor(Math.random() * 100000) + 86400,
      last_save: new Date(Date.now() - Math.random() * 3600000).toISOString()
    };
    
    res.json({
      status: 'success',
      message: 'Redis 통계 조회 완료',
      data: stats,
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('Redis 통계 조회 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: 'Redis 통계 조회 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// Redis 키 트리 구조 조회 (루트) - ConfigManager 패턴 준수
router.get('/redis/tree', async (req, res) => {
  try {
    console.log('🌳 Redis 트리 조회 요청:', req.query);
    
    const { parent_path } = req.query;
    
    // ConfigManager에서 Redis 설정 가져오기 (기존 패턴 준수)
    const redisConfig = config.getRedisConfig();
    const keyPrefix = redisConfig.keyPrefix || 'pulseone:';
    
    let treeData = [];
    
    if (!parent_path) {
      // 루트 레벨 - 테스트 데이터 생성
      treeData = [
        {
          id: 'tenant_demo',
          name: 'Demo Corporation',
          path: `${keyPrefix}demo`,
          type: 'tenant',
          isExpanded: false,
          isLoaded: false,
          childCount: 3,
          metadata: {
            tenant_id: 1,
            last_update: new Date().toISOString()
          }
        },
        {
          id: 'tenant_test',
          name: 'Test Factory',
          path: `${keyPrefix}test`,
          type: 'tenant',
          isExpanded: false,
          isLoaded: false,
          childCount: 2,
          metadata: {
            tenant_id: 2,
            last_update: new Date().toISOString()
          }
        },
        {
          id: 'tenant_samsung',
          name: 'Samsung Electronics',
          path: `${keyPrefix}samsung`,
          type: 'tenant',
          isExpanded: false,
          isLoaded: false,
          childCount: 4,
          metadata: {
            tenant_id: 3,
            last_update: new Date().toISOString()
          }
        }
      ];
    } else {
      // 하위 레벨 - 부모 경로에 따라 다른 데이터
      if (parent_path.includes('demo')) {
        treeData = [
          {
            id: 'device_demo_1',
            name: 'Demo Device 1',
            path: `${parent_path}:device1`,
            type: 'device',
            isExpanded: false,
            isLoaded: false,
            childCount: 5,
            metadata: {
              device_id: 1,
              last_update: new Date().toISOString()
            }
          },
          {
            id: 'device_demo_2',
            name: 'Demo Device 2',
            path: `${parent_path}:device2`,
            type: 'device',
            isExpanded: false,
            isLoaded: false,
            childCount: 3,
            metadata: {
              device_id: 2,
              last_update: new Date().toISOString()
            }
          }
        ];
      } else {
        // 빈 결과
        treeData = [];
      }
    }
    
    console.log('✅ Redis 트리 데이터 생성:', { count: treeData.length, data: treeData });
    
    // 🔥 Frontend가 기대하는 응답 구조로 수정 (data에 직접 배열)
    res.json({
      status: 'success',
      message: 'Redis 트리 구조 조회 완료',
      data: treeData,  // ✅ 직접 배열로 전달
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    console.error('❌ Redis 트리 조회 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: 'Redis 트리 조회 실패',
      error: error.message,
      data: [],  // 에러 시에도 빈 배열
      timestamp: new Date().toISOString()
    });
  }
});

// 특정 노드의 자식 노드들 조회 (ConfigManager 패턴 준수)
router.get('/redis/tree/:nodeId/children', async (req, res) => {
  try {
    const { nodeId } = req.params;
    
    // ConfigManager에서 설정 가져오기
    const redisConfig = config.getRedisConfig();
    const keyPrefix = redisConfig.keyPrefix || 'pulseone:';
    
    let children = [];
    
    // 시뮬레이션 자식 노드 생성 (기존 패턴 준수)
    if (nodeId.startsWith('tenant_')) {
      // 테넌트 하위 - 사이트들
      const tenantNames = {
        'tenant_1': 'samsung',
        'tenant_2': 'lg', 
        'tenant_3': 'hyundai'
      };
      const tenantKey = tenantNames[nodeId];
      
      children = [
        {
          id: `${nodeId}_site_1`,
          name: 'Seoul Factory',
          path: `${keyPrefix}${tenantKey}:seoul_fab`,
          type: 'site',
          icon: 'fas fa-industry',
          isExpanded: false,
          isLoaded: false,
          childCount: 5,
          description: '서울 제조 공장'
        },
        {
          id: `${nodeId}_site_2`,
          name: 'Busan Factory', 
          path: `${keyPrefix}${tenantKey}:busan_fab`,
          type: 'site',
          icon: 'fas fa-industry',
          isExpanded: false,
          isLoaded: false,
          childCount: 3,
          description: '부산 제조 공장'
        }
      ];
      
      if (nodeId === 'tenant_3') {
        children.push({
          id: `${nodeId}_site_3`,
          name: 'Ulsan Factory',
          path: `${keyPrefix}hyundai:ulsan_fab`,
          type: 'site',
          icon: 'fas fa-industry',
          isExpanded: false,
          isLoaded: false,
          childCount: 7,
          description: '울산 제조 공장'
        });
      }
    } else if (nodeId.includes('_site_')) {
      // 사이트 하위 - 디바이스들
      children = [
        {
          id: `${nodeId}_device_1`,
          name: 'Main PLC #001',
          path: `${nodeId}:plc001`,
          type: 'device',
          icon: 'fas fa-microchip',
          isExpanded: false,
          isLoaded: false,
          childCount: 15,
          description: '메인 PLC 컨트롤러',
          status: 'connected'
        },
        {
          id: `${nodeId}_device_2`,
          name: 'Backup PLC #002',
          path: `${nodeId}:plc002`, 
          type: 'device',
          icon: 'fas fa-microchip',
          isExpanded: false,
          isLoaded: false,
          childCount: 12,
          description: '백업 PLC 컨트롤러',
          status: Math.random() > 0.2 ? 'connected' : 'disconnected'
        },
        {
          id: `${nodeId}_device_3`,
          name: 'Operator HMI #001',
          path: `${nodeId}:hmi001`,
          type: 'device',
          icon: 'fas fa-desktop',
          isExpanded: false,
          isLoaded: false,
          childCount: 8,
          description: '오퍼레이터 HMI 단말',
          status: 'connected'
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
      
      children = dataPointNames.slice(0, 12).map((name, index) => ({
        id: `${nodeId}_dp_${index}`,
        name: name.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase()),
        path: `${keyPrefix}${nodeId}:${name}`,
        type: 'datapoint',
        icon: 'fas fa-chart-line',
        isExpanded: false,
        isLoaded: true,
        dataPoint: generateSimulationDataPoint(`${keyPrefix}${nodeId}:${name}`, name)
      }));
    }
    
    res.json({
      status: 'success',
      message: `노드 ${nodeId}의 자식 노드 조회 완료`,
      data: {
        children,
        parent_node_id: nodeId,
        key_prefix_used: keyPrefix
      },
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('노드 자식 조회 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: '노드 자식 조회 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// Redis 키 검색 (ConfigManager 패턴 준수)
router.get('/redis/keys/search', async (req, res) => {
  try {
    const { pattern, limit = 100, cursor = 0 } = req.query;
    
    // ConfigManager에서 키 접두사 가져오기
    const redisConfig = config.getRedisConfig();
    const keyPrefix = redisConfig.keyPrefix || 'pulseone:';
    
    // 시뮬레이션 검색 결과 (키 접두사 사용)
    const simulationKeys = [
      `${keyPrefix}samsung:seoul_fab:plc001:temperature_01`,
      `${keyPrefix}samsung:seoul_fab:plc001:temperature_02`, 
      `${keyPrefix}samsung:seoul_fab:plc001:pressure_main`,
      `${keyPrefix}samsung:seoul_fab:plc001:pressure_backup`,
      `${keyPrefix}samsung:seoul_fab:plc001:motor_speed`,
      `${keyPrefix}samsung:seoul_fab:plc001:motor_current`,
      `${keyPrefix}samsung:seoul_fab:plc001:vibration_x`,
      `${keyPrefix}samsung:seoul_fab:plc001:vibration_y`,
      `${keyPrefix}samsung:seoul_fab:plc002:flow_rate`,
      `${keyPrefix}samsung:seoul_fab:plc002:level_tank1`,
      `${keyPrefix}samsung:seoul_fab:hmi001:ph_value`,
      `${keyPrefix}lg:busan_fab:plc002:motor_speed`,
      `${keyPrefix}lg:busan_fab:plc002:motor_current`,
      `${keyPrefix}lg:busan_fab:hmi001:level_tank1`,
      `${keyPrefix}lg:busan_fab:hmi001:conductivity`,
      `${keyPrefix}hyundai:ulsan_fab:plc003:temperature_01`,
      `${keyPrefix}hyundai:ulsan_fab:plc003:pressure_main`,
      `${keyPrefix}hyundai:ulsan_fab:hmi001:flow_rate`,
      `${keyPrefix}hyundai:ulsan_fab:hmi001:alarm_status`,
      `${keyPrefix}hyundai:ulsan_fab:plc004:turbidity`
    ];
    
    let keys = simulationKeys;
    if (pattern && pattern.trim()) {
      keys = simulationKeys.filter(key => 
        key.toLowerCase().includes(pattern.toLowerCase())
      );
    }
    
    const startIndex = parseInt(cursor);
    const endIndex = startIndex + parseInt(limit);
    const resultKeys = keys.slice(startIndex, endIndex);
    const hasMore = endIndex < keys.length;
    
    res.json({
      status: 'success',
      message: `키 검색 완료 (패턴: ${pattern || '*'})`,
      data: {
        keys: resultKeys,
        total: keys.length,
        cursor: hasMore ? endIndex.toString() : '0',
        next_page: hasMore ? endIndex : null,
        search_config: {
          key_prefix: keyPrefix,
          pattern_used: pattern || '*'
        }
      },
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('Redis 키 검색 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: 'Redis 키 검색 실패',
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
    
    // 시뮬레이션 데이터 포인트 생성 (기존 패턴 준수)
    const dataPoint = generateSimulationDataPoint(decodedKey, decodedKey.split(':').pop());
    
    res.json({
      status: 'success',
      message: `키 ${decodedKey} 데이터 조회 완료`,
      data: dataPoint,
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('키 데이터 조회 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: '키 데이터 조회 실패',
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
        status: 'error',
        message: 'keys must be an array',
        timestamp: new Date().toISOString()
      });
    }
    
    // 각 키에 대한 시뮬레이션 데이터 생성 (기존 패턴 준수)
    const dataPoints = keys.map(key => 
      generateSimulationDataPoint(key, key.split(':').pop())
    );
    
    res.json({
      status: 'success',
      message: `${keys.length}개 키 일괄 조회 완료`,
      data: dataPoints,
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('일괄 키 데이터 조회 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: '일괄 키 데이터 조회 실패',
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
        status: 'error',
        message: 'keys must be an array',
        timestamp: new Date().toISOString()
      });
    }
    
    // 시뮬레이션 데이터 생성 (기존 패턴 준수)
    const data = keys.map(key => {
      const dataPoint = generateSimulationDataPoint(key, key.split(':').pop());
      return {
        key,
        name: dataPoint.name,
        value: dataPoint.value,
        dataType: dataPoint.dataType,
        unit: dataPoint.unit,
        timestamp: dataPoint.timestamp,
        quality: dataPoint.quality
      };
    });
    
    if (format === 'json') {
      res.setHeader('Content-Type', 'application/json');
      res.setHeader('Content-Disposition', 'attachment; filename="redis_export.json"');
      res.json({
        status: 'success',
        message: `${keys.length}개 키 내보내기 완료`,
        export_data: data,
        exported_at: new Date().toISOString(),
        total_keys: keys.length
      });
    } else {
      res.status(400).json({
        status: 'error',
        message: 'Unsupported format. Only JSON is supported.',
        timestamp: new Date().toISOString()
      });
    }
  } catch (error) {
    console.error('데이터 내보내기 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: '데이터 내보내기 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// =============================================================================
// 시뮬레이션 데이터 생성 함수 (기존 패턴 준수)
// =============================================================================

function generateSimulationDataPoint(key, name) {
  const dataTypes = ['number', 'boolean', 'string'];
  const qualities = ['good', 'bad', 'uncertain'];
  const units = ['°C', 'bar', 'rpm', 'A', 'mm', 'pH', '%', 'L/min', 'V', 'Hz'];
  
  // 키 이름에 따라 적절한 데이터 타입 결정
  let dataType = 'number';
  if (name.includes('alarm') || name.includes('status')) {
    dataType = Math.random() > 0.5 ? 'boolean' : 'string';
  } else if (name.includes('mode') || name.includes('state')) {
    dataType = 'string';
  }
  
  let value;
  let unit;
  
  switch (dataType) {
    case 'number':
      if (name.includes('temperature')) {
        value = (Math.random() * 50 + 20).toFixed(2); // 20-70°C
        unit = '°C';
      } else if (name.includes('pressure')) {
        value = (Math.random() * 10 + 1).toFixed(2); // 1-11 bar
        unit = 'bar';
      } else if (name.includes('speed')) {
        value = (Math.random() * 3000 + 500).toFixed(0); // 500-3500 rpm
        unit = 'rpm';
      } else if (name.includes('current')) {
        value = (Math.random() * 50 + 5).toFixed(2); // 5-55 A
        unit = 'A';
      } else if (name.includes('level')) {
        value = (Math.random() * 100).toFixed(1); // 0-100%
        unit = '%';
      } else if (name.includes('flow')) {
        value = (Math.random() * 200 + 10).toFixed(2); // 10-210 L/min
        unit = 'L/min';
      } else if (name.includes('ph')) {
        value = (Math.random() * 8 + 4).toFixed(2); // 4-12 pH
        unit = 'pH';
      } else {
        value = (Math.random() * 100).toFixed(2);
        unit = units[Math.floor(Math.random() * units.length)];
      }
      break;
    case 'boolean':
      value = Math.random() > 0.3; // 70% true
      break;
    case 'string':
      const statusValues = ['Normal', 'Warning', 'Alarm', 'Maintenance', 'Stop', 'Auto', 'Manual'];
      value = statusValues[Math.floor(Math.random() * statusValues.length)];
      break;
  }
  
  // 품질 결정 (95% good quality)
  const quality = Math.random() > 0.05 ? 'good' : qualities[Math.floor(Math.random() * qualities.length)];
  
  return {
    id: key,
    key,
    name: name ? name.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase()) : key.split(':').pop(),
    value,
    dataType,
    unit: dataType === 'number' ? unit : undefined,
    timestamp: new Date().toISOString(),
    quality,
    size: Math.floor(Math.random() * 1024) + 64,
    ttl: Math.random() > 0.7 ? Math.floor(Math.random() * 3600) + 300 : undefined,
    metadata: {
      source: 'simulation',
      last_update: new Date(Date.now() - Math.random() * 30000).toISOString(),
      update_count: Math.floor(Math.random() * 1000) + 100
    }
  };
}

// =============================================================================
// 기존 API 엔드포인트들 (유지)
// =============================================================================

// 데이터 수집 현황
router.get('/data/status', async (req, res) => {
  try {
    // 시뮬레이션 데이터 수집 상태 (기존 패턴 준수)
    res.json({
      status: 'success',
      message: '데이터 수집 현황 조회 완료',
      data: {
        total_devices: 8,
        active_devices: 6,
        last_update: new Date().toISOString(),
        data_points_today: Math.floor(Math.random() * 10000) + 15000,
        collection_rate: '98.5%',
        errors_today: Math.floor(Math.random() * 50) + 2
      },
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('데이터 현황 조회 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: '데이터 현황 조회 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// 디바이스 목록
router.get('/devices', async (req, res) => {
  try {
    // 시뮬레이션 디바이스 목록 (기존 패턴 준수)
    const simulationDevices = [
      { 
        id: 1, 
        name: 'PLC001', 
        type: 'MODBUS_TCP', 
        status: 'connected',
        ip: '192.168.1.100',
        last_seen: new Date(Date.now() - Math.random() * 60000).toISOString()
      },
      { 
        id: 2, 
        name: 'PLC002', 
        type: 'MODBUS_RTU', 
        status: 'connected',
        ip: '192.168.1.101',
        last_seen: new Date(Date.now() - Math.random() * 120000).toISOString()
      },
      { 
        id: 3, 
        name: 'HMI001', 
        type: 'MQTT', 
        status: Math.random() > 0.2 ? 'connected' : 'disconnected',
        ip: '192.168.1.150',
        last_seen: new Date(Date.now() - Math.random() * 300000).toISOString()
      }
    ];
    
    res.json({
      status: 'success',
      message: '디바이스 목록 조회 완료',
      data: simulationDevices,
      total: simulationDevices.length,
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    console.error('디바이스 목록 조회 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: '디바이스 목록 조회 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// 🔥 누락된 Redis 키 트리 조회 (Backend 응답 구조에 맞게 수정)
router.get('/redis/tree', async (req, res) => {
  try {
    console.log('🌳 Redis 트리 조회 요청:', req.query);
    
    const { parent_path } = req.query;
    
    // ConfigManager에서 Redis 설정 가져오기 (기존 패턴 준수)
    const redisConfig = config.getRedisConfig();
    const keyPrefix = redisConfig.keyPrefix || 'pulseone:';
    
    let treeData = [];
    
    if (!parent_path) {
      // 루트 레벨 - 테스트 데이터 생성
      treeData = [
        {
          id: 'tenant_demo',
          name: 'Demo Corporation',
          path: `${keyPrefix}demo`,
          type: 'tenant',
          isExpanded: false,
          isLoaded: false,
          childCount: 3,
          metadata: {
            tenant_id: 1,
            last_update: new Date().toISOString()
          }
        },
        {
          id: 'tenant_test',
          name: 'Test Factory',
          path: `${keyPrefix}test`,
          type: 'tenant',
          isExpanded: false,
          isLoaded: false,
          childCount: 2,
          metadata: {
            tenant_id: 2,
            last_update: new Date().toISOString()
          }
        }
      ];
    } else {
      // 하위 레벨 - 부모 경로에 따라 다른 데이터
      if (parent_path.includes('demo')) {
        treeData = [
          {
            id: 'device_demo_1',
            name: 'Demo Device 1',
            path: `${parent_path}:device1`,
            type: 'device',
            isExpanded: false,
            isLoaded: false,
            childCount: 5,
            metadata: {
              device_id: 1,
              last_update: new Date().toISOString()
            }
          },
          {
            id: 'device_demo_2',
            name: 'Demo Device 2',
            path: `${parent_path}:device2`,
            type: 'device',
            isExpanded: false,
            isLoaded: false,
            childCount: 3,
            metadata: {
              device_id: 2,
              last_update: new Date().toISOString()
            }
          }
        ];
      } else {
        // 빈 결과
        treeData = [];
      }
    }
    
    console.log('✅ Redis 트리 데이터 생성:', { count: treeData.length });
    
    // 🔥 Frontend가 기대하는 응답 구조로 수정
    res.json({
      status: 'success',
      message: 'Redis 트리 구조 조회 완료',
      data: treeData,  // Frontend가 response.data에서 treeData를 기대함
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    console.error('❌ Redis 트리 조회 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: 'Redis 트리 조회 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// 🔥 누락된 Redis 통계 조회 (Frontend가 기대하는 필드명으로 수정)
router.get('/redis/stats', async (req, res) => {
  try {
    console.log('📊 Redis 통계 조회 요청');
    
    // Frontend RedisStats 인터페이스에 맞는 데이터 생성
    const stats = {
      total_keys: Math.floor(Math.random() * 1000) + 142,
      memory_usage: Math.floor(Math.random() * 100 * 1024 * 1024) + 45 * 1024 * 1024, // bytes
      connected_clients: Math.floor(Math.random() * 10) + 5,
      commands_processed: Math.floor(Math.random() * 100000) + 125847,
      hits: Math.floor(Math.random() * 50000) + 98234,
      misses: Math.floor(Math.random() * 5000) + 1247,
      expired_keys: Math.floor(Math.random() * 100) + 23
    };
    
    console.log('✅ Redis 통계 생성:', stats);
    
    // 🔥 Frontend가 기대하는 응답 구조로 수정
    res.json({
      status: 'success',
      message: 'Redis 통계 조회 완료',
      data: stats,  // ✅ 직접 stats 객체 전달
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    console.error('❌ Redis 통계 조회 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: 'Redis 통계 조회 실패',
      error: error.message,
      data: {
        total_keys: 0,
        memory_usage: 0,
        connected_clients: 0,
        commands_processed: 0,
        hits: 0,
        misses: 0,
        expired_keys: 0
      },
      timestamp: new Date().toISOString()
    });
  }
});

// 🔥 누락된 Redis 키 검색
router.get('/redis/keys/search', async (req, res) => {
  try {
    console.log('🔍 Redis 키 검색 요청:', req.query);
    
    const { pattern, limit = 100 } = req.query;
    
    // Mock 키 데이터 생성
    const mockKeys = [
      'pulseone:demo:device1:temperature',
      'pulseone:demo:device1:pressure',
      'pulseone:demo:device1:flow_rate',
      'pulseone:demo:device2:voltage',
      'pulseone:demo:device2:current',
      'pulseone:test:device1:rpm',
      'pulseone:test:device1:vibration'
    ];
    
    let filteredKeys = mockKeys;
    
    if (pattern && pattern !== '*') {
      const regex = new RegExp(pattern.replace(/\*/g, '.*'), 'i');
      filteredKeys = mockKeys.filter(key => regex.test(key));
    }
    
    const result = {
      keys: filteredKeys.slice(0, parseInt(limit)),
      total: filteredKeys.length,
      cursor: null
    };
    
    console.log('✅ Redis 키 검색 완료:', { pattern, found: result.keys.length });
    
    res.json({
      status: 'success',
      message: 'Redis 키 검색 완료',
      data: result,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    console.error('❌ Redis 키 검색 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: 'Redis 키 검색 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// =============================================================================
// 🔥 누락된 디바이스 관리 API 추가 (간단한 Mock 버전)
// =============================================================================

// 디바이스 목록 조회 (기존 것과 중복되지 않도록 주의)
if (!router.stack.some(layer => layer.route && layer.route.path === '/devices')) {
  router.get('/devices', async (req, res) => {
    try {
      console.log('📱 디바이스 목록 조회 요청');
      
      // Mock 디바이스 데이터 생성
      const mockDevices = [
        {
          id: 1,
          name: 'Demo Modbus Device',
          protocol_type: 'MODBUS_TCP',
          device_type: 'PLC',
          endpoint: '192.168.1.100:502',
          is_enabled: true,
          connection_status: 'connected',
          status: 'running',
          last_seen: new Date().toISOString(),
          manufacturer: 'Schneider Electric',
          model: 'M340',
          data_points_count: 15,
          created_at: '2025-01-01T00:00:00Z'
        },
        {
          id: 2,
          name: 'Demo MQTT Sensor',
          protocol_type: 'MQTT',
          device_type: 'Sensor',
          endpoint: 'mqtt://broker.example.com:1883',
          is_enabled: true,
          connection_status: 'connected',
          status: 'running',
          last_seen: new Date(Date.now() - 30000).toISOString(),
          manufacturer: 'Generic',
          model: 'IoT-Sensor-v2',
          data_points_count: 8,
          created_at: '2025-01-02T00:00:00Z'
        },
        {
          id: 3,
          name: 'Demo BACnet Controller',
          protocol_type: 'BACNET',
          device_type: 'Controller',
          endpoint: '192.168.1.200',
          is_enabled: false,
          connection_status: 'disconnected',
          status: 'stopped',
          last_seen: new Date(Date.now() - 3600000).toISOString(),
          manufacturer: 'Honeywell',
          model: 'BACnet-Pro',
          data_points_count: 22,
          created_at: '2025-01-03T00:00:00Z'
        }
      ];
      
      console.log('✅ 디바이스 목록 생성:', { count: mockDevices.length });
      
      // 🔥 Backend 응답 구조에 맞게 수정
      res.json({
        status: 'success',
        message: '디바이스 목록 조회 완료',
        data: mockDevices,
        pagination: {
          total: mockDevices.length,
          page: 1,
          pageSize: 20,
          totalPages: 1
        },
        timestamp: new Date().toISOString()
      });
      
    } catch (error) {
      console.error('❌ 디바이스 목록 조회 실패:', error.message);
      res.status(500).json({
        status: 'error',
        message: '디바이스 목록 조회 실패',
        error: error.message,
        timestamp: new Date().toISOString()
      });
    }
  });
}

// 디바이스 상태 변경 엔드포인트들
router.post('/devices/:id/enable', async (req, res) => {
  try {
    const deviceId = parseInt(req.params.id);
    console.log(`🟢 디바이스 ${deviceId} 활성화 요청`);
    
    res.json({
      status: 'success',
      message: `디바이스 ${deviceId} 활성화 완료`,
      data: { device_id: deviceId, enabled: true },
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    console.error('❌ 디바이스 활성화 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: '디바이스 활성화 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

router.post('/devices/:id/disable', async (req, res) => {
  try {
    const deviceId = parseInt(req.params.id);
    console.log(`🔴 디바이스 ${deviceId} 비활성화 요청`);
    
    res.json({
      status: 'success',
      message: `디바이스 ${deviceId} 비활성화 완료`,
      data: { device_id: deviceId, enabled: false },
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    console.error('❌ 디바이스 비활성화 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: '디바이스 비활성화 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

router.post('/devices/:id/restart', async (req, res) => {
  try {
    const deviceId = parseInt(req.params.id);
    console.log(`🔄 디바이스 ${deviceId} 재시작 요청`);
    
    res.json({
      status: 'success',
      message: `디바이스 ${deviceId} 재시작 완료`,
      data: { device_id: deviceId, restarted: true },
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    console.error('❌ 디바이스 재시작 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: '디바이스 재시작 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

router.post('/devices/:id/test-connection', async (req, res) => {
  try {
    const deviceId = parseInt(req.params.id);
    console.log(`🔗 디바이스 ${deviceId} 연결 테스트 요청`);
    
    // Mock 연결 테스트 결과
    const testResult = {
      device_id: deviceId,
      connection_successful: Math.random() > 0.2, // 80% 성공률
      response_time: Math.floor(Math.random() * 200) + 50, // 50-250ms
      tested_at: new Date().toISOString()
    };
    
    res.json({
      status: 'success',
      message: `디바이스 ${deviceId} 연결 테스트 완료`,
      data: testResult,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    console.error('❌ 디바이스 연결 테스트 실패:', error.message);
    res.status(500).json({
      status: 'error',
      message: '디바이스 연결 테스트 실패',
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

module.exports = router;