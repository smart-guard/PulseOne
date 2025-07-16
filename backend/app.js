// app.js - PulseOne Backend Application
require('dotenv').config();
const express = require('express');

const app = express();

// =============================================================================
// 미들웨어 설정
// =============================================================================

// JSON 파싱
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// 간단한 로깅
app.use((req, res, next) => {
  console.log(`[${new Date().toISOString()}] ${req.method} ${req.path}`);
  next();
});

// =============================================================================
// DB 연결 상태 관리
// =============================================================================

// 실제 사용하는 DB만 import
let redisClient = null;
let postgres = null;

// 초기화 함수
async function initializeConnections() {
  try {
    console.log('🚀 데이터베이스 연결 초기화 시작...');
    
    // Redis 연결 (필수)
    try {
      redisClient = require('./lib/connection/redis');
      console.log('✅ Redis 연결 초기화 완료');
    } catch (error) {
      console.error('❌ Redis 연결 실패:', error.message);
    }

    // PostgreSQL 연결 (필수)
    try {
      postgres = require('./lib/connection/postgres');
      console.log('✅ PostgreSQL 연결 초기화 완료');
    } catch (error) {
      console.error('❌ PostgreSQL 연결 실패:', error.message);
    }

    // 필요에 따라 다른 DB들도 추가
    // if (process.env.USE_INFLUXDB === 'true') {
    //   influx = require('./lib/connection/influx');
    // }

    console.log('✅ 데이터베이스 연결 초기화 완료');
  } catch (error) {
    console.error('❌ 데이터베이스 초기화 실패:', error.message);
  }
}

// Express에서 DB 클라이언트들에 접근할 수 있도록 설정
app.locals.getDB = () => ({
  redis: redisClient,
  postgres: postgres
});

// =============================================================================
// 미들웨어 설정
// =============================================================================

// JSON 파싱
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// 간단한 로깅
app.use((req, res, next) => {
  console.log(`[${new Date().toISOString()}] ${req.method} ${req.path}`);
  next();
});

// =============================================================================
// 라우터 연결
// =============================================================================

// 헬스체크 라우터
app.use('/', require('./routes/health'));

// API 라우터
app.use('/api', require('./routes/api'));

// 사용자 라우터 (기존)
app.use('/api/users', require('./routes/user'));

// =============================================================================
// 에러 처리
// =============================================================================

// 404 처리
app.use('*', (req, res) => {
  res.status(404).json({
    status: 'error',
    message: 'API 엔드포인트를 찾을 수 없습니다',
    path: req.originalUrl,
    timestamp: new Date().toISOString()
  });
});

// 전역 에러 처리
app.use((err, req, res, next) => {
  console.error('❌ Server Error:', err.stack);
  
  res.status(err.statusCode || 500).json({
    status: 'error',
    message: process.env.NODE_ENV === 'production' 
      ? '서버 오류가 발생했습니다' 
      : err.message,
    timestamp: new Date().toISOString()
  });
});

// =============================================================================
// 모듈 시작 시 초기화
// =============================================================================

// 테스트 환경이 아닐 때만 자동 초기화
if (require.main !== module) {
  initializeConnections();
}

module.exports = app;
module.exports.initializeConnections = initializeConnections;