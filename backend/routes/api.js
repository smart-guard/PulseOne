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
// 실제 API 엔드포인트들 (나중에 추가할 부분)
// =============================================================================

// 데이터 수집 현황
router.get('/data/status', async (req, res) => {
  try {
    // TODO: 실제 데이터 수집 상태 조회 로직
    res.json({
      status: 'success',
      data: {
        total_devices: 0,
        active_devices: 0,
        last_update: new Date().toISOString(),
        data_points_today: 0
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
    res.json({
      status: 'success',
      data: [],
      total: 0
    });
  } catch (error) {
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

module.exports = router;