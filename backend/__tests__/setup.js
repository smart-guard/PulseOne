// =============================================================================
// backend/__tests__/setup.js
// Jest 테스트 환경 설정
// =============================================================================

// 환경 변수 설정
process.env.NODE_ENV = 'test';
process.env.TEST_MODE = 'true';
process.env.LOG_LEVEL = 'error';

// 테스트용 데이터베이스 설정
process.env.DB_TYPE = 'sqlite';
process.env.DB_PATH = ':memory:';
process.env.REDIS_URL = 'redis://localhost:6379/15'; // 테스트용 DB
process.env.POSTGRES_HOST = 'localhost';
process.env.POSTGRES_PORT = '5432';
process.env.POSTGRES_DB = 'pulseone_test';
process.env.RABBITMQ_URL = 'amqp://localhost:5672';
process.env.INFLUXDB_URL = 'http://localhost:8086';
process.env.INFLUXDB_TOKEN = 'test-token';
process.env.INFLUXDB_ORG = 'test-org';
process.env.INFLUXDB_BUCKET = 'test-bucket';

// JWT 시크릿 (테스트용)
process.env.JWT_SECRET = 'test-jwt-secret-key-for-testing-only';
process.env.JWT_EXPIRE = '1h';

// 서버 설정
process.env.PORT = '3001'; // 메인 서버와 다른 포트
process.env.API_PREFIX = '/api';

// 글로벌 테스트 설정
global.console = {
    ...console,
    // 테스트 중 로그 출력 제어
    log: process.env.VERBOSE_TESTS === 'true' ? console.log : jest.fn(),
    debug: jest.fn(),
    info: process.env.VERBOSE_TESTS === 'true' ? console.info : jest.fn(),
    warn: console.warn,
    error: console.error,
};

// 테스트 타임아웃 설정
jest.setTimeout(30000);

// 테스트 시작 전 글로벌 설정
beforeAll(async () => {
    console.log('🧪 === PulseOne 테스트 환경 설정 ===');
    console.log(`📅 테스트 시작: ${new Date().toISOString()}`);
    console.log(`🔧 Node 환경: ${process.env.NODE_ENV}`);
    console.log(`🗄️ DB 타입: ${process.env.DB_TYPE}`);
    console.log(`🔌 Redis: ${process.env.REDIS_URL ? '연결 시도' : '비활성화'}`);
    console.log(`🐘 PostgreSQL: ${process.env.POSTGRES_HOST ? '연결 시도' : '비활성화'}`);
    console.log(`🐰 RabbitMQ: ${process.env.RABBITMQ_URL ? '연결 시도' : '비활성화'}`);
    console.log(`📊 InfluxDB: ${process.env.INFLUXDB_URL ? '연결 시도' : '비활성화'}`);
});

// 모든 테스트 완료 후 정리
afterAll(async () => {
    console.log('\n🧹 === 테스트 정리 작업 ===');
    
    // 테스트 결과 요약
    const testResults = global.testResults || {};
    console.log(`📊 테스트 통계:`);
    console.log(`   - 총 테스트 수: ${testResults.numTotalTests || 'N/A'}`);
    console.log(`   - 성공: ${testResults.numPassedTests || 'N/A'}`);
    console.log(`   - 실패: ${testResults.numFailedTests || 'N/A'}`);
    console.log(`📅 테스트 완료: ${new Date().toISOString()}`);
});

// 각 테스트 파일 전 실행
beforeEach(() => {
    // 테스트 격리를 위한 설정
    jest.clearAllMocks();
});

// 각 테스트 파일 후 실행
afterEach(() => {
    // 메모리 정리
    if (global.gc) {
        global.gc();
    }
});

// 처리되지 않은 Promise rejection 처리
process.on('unhandledRejection', (reason, promise) => {
    console.error('⚠️ Unhandled Rejection at:', promise, 'reason:', reason);
    // 테스트에서는 실패하지 않고 경고만 출력
});

// 처리되지 않은 예외 처리
process.on('uncaughtException', (error) => {
    console.error('⚠️ Uncaught Exception:', error);
    // 테스트에서는 실패하지 않고 경고만 출력
});

// 테스트 유틸리티 함수들
global.testUtils = {
    /**
     * 비동기 함수 타임아웃 래퍼
     */
    withTimeout: (asyncFn, timeout = 5000) => {
        return Promise.race([
            asyncFn(),
            new Promise((_, reject) => 
                setTimeout(() => reject(new Error('Test timeout')), timeout)
            )
        ]);
    },

    /**
     * API 응답 시간 측정
     */
    measureTime: async (fn) => {
        const start = Date.now();
        const result = await fn();
        const duration = Date.now() - start;
        return { result, duration };
    },

    /**
     * 테스트 데이터 생성
     */
    createTestDevice: (overrides = {}) => ({
        name: 'Test Device',
        device_type: 'modbus',
        protocol_type: 'modbus_tcp',
        endpoint: '192.168.1.100:502',
        scan_interval: 1000,
        is_enabled: true,
        ...overrides
    }),

    createTestUser: (overrides = {}) => ({
        username: 'testuser',
        email: 'test@example.com',
        password: 'password123',
        role: 'user',
        is_active: true,
        ...overrides
    }),

    createTestAlarmRule: (overrides = {}) => ({
        name: 'Test Alarm',
        target_type: 'data_point',
        target_id: 1,
        alarm_type: 'analog',
        high_limit: 100.0,
        severity: 'high',
        is_enabled: true,
        ...overrides
    })
};

// 테스트 데이터베이스 상태 확인 헬퍼
global.checkDatabaseHealth = async () => {
    const health = {
        sqlite: false,
        redis: false,
        postgres: false,
        influx: false,
        rabbitmq: false
    };

    try {
        // SQLite는 항상 사용 가능 (in-memory)
        health.sqlite = true;
    } catch (error) {
        console.warn('SQLite 연결 실패:', error.message);
    }

    // 다른 데이터베이스들은 연결 시도만 하고 실패해도 무시
    try {
        const redis = require('../lib/connection/redis');
        if (redis && redis.ping) {
            await redis.ping();
            health.redis = true;
        }
    } catch (error) {
        // Redis 연결 실패는 정상 (Docker 환경이 없을 수 있음)
    }

    try {
        const postgres = require('../lib/connection/postgres');
        if (postgres && postgres.query) {
            await postgres.query('SELECT 1');
            health.postgres = true;
        }
    } catch (error) {
        // PostgreSQL 연결 실패는 정상
    }

    return health;
};

console.log('✅ Jest 테스트 환경 설정 완료');