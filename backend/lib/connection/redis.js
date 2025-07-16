// ===========================================================================
// lib/connection/redis.js
// ===========================================================================
const redis = require('redis');
const env = require('../../../config/env');

// Redis 설정
const redisConfig = {
  url: `redis://${env.REDIS_MAIN_HOST || 'localhost'}:${env.REDIS_MAIN_PORT || '6379'}`,
  retry_unfulfilled_commands: true,
  retry_delay_on_failure: 2000,
  max_attempts: 3,
  connect_timeout: 5000
};

// 비밀번호가 설정된 경우 추가
if (env.REDIS_MAIN_PASSWORD) {
  redisConfig.password = env.REDIS_MAIN_PASSWORD;
}

const redisClient = redis.createClient(redisConfig);

// 이벤트 핸들러
redisClient.on('error', (err) => {
  console.error('❌ Redis 연결 오류:', err.message);
});

redisClient.on('connect', () => {
  console.log('✅ Redis 연결 성공');
});

redisClient.on('disconnect', () => {
  console.warn('⚠️ Redis 연결 해제됨');
});

redisClient.on('reconnecting', () => {
  console.log('🔄 Redis 재연결 시도 중...');
});

// 연결 초기화
redisClient.connect().catch((error) => {
  console.error('Redis 초기 연결 실패:', error.message);
});

module.exports = redisClient;