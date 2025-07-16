// routes/health.js
const express = require('express');
const router = express.Router();

// 헬스체크 엔드포인트
router.get('/health', async (req, res) => {
  const health = {
    status: 'ok',
    timestamp: new Date().toISOString(),
    uptime: process.uptime(),
    memory: {
      used: Math.round(process.memoryUsage().heapUsed / 1024 / 1024) + 'MB',
      total: Math.round(process.memoryUsage().heapTotal / 1024 / 1024) + 'MB'
    },
    environment: process.env.NODE_ENV || 'development',
    services: {}
  };

  // app.locals에서 DB 클라이언트들 가져오기
  const db = req.app.locals.getDB();

  try {
    // Redis 상태 확인
    if (db.redis && db.redis.isReady) {
      await db.redis.ping();
      health.services.redis = 'connected';
    } else {
      health.services.redis = 'disconnected';
      health.status = 'degraded';
    }
  } catch (error) {
    health.services.redis = 'error';
    health.status = 'degraded';
  }

  try {
    // PostgreSQL 상태 확인
    if (db.postgres) {
      await db.postgres.query('SELECT 1');
      health.services.postgres = 'connected';
    } else {
      health.services.postgres = 'not_initialized';
      health.status = 'degraded';
    }
  } catch (error) {
    health.services.postgres = 'disconnected';
    health.status = 'degraded';
  }

  // 필요시 다른 DB들도 체크
  // if (process.env.USE_INFLUXDB === 'true') {
  //   health.services.influxdb = 'initialized';
  // }

  const statusCode = health.status === 'ok' ? 200 : 503;
  res.status(statusCode).json(health);
});

// 간단한 핑 엔드포인트
router.get('/ping', (req, res) => {
  res.json({ 
    status: 'pong', 
    timestamp: new Date().toISOString() 
  });
});

module.exports = router;