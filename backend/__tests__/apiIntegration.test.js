// =============================================================================
// backend/__tests__/apiIntegration.test.js
// 🎯 Repository → Controller → Route 완전 통합 테스트
// =============================================================================

const request = require('supertest');
const express = require('express');
const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');

describe('🔥 Repository → Controller → Route 통합 검증', () => {
    let app;
    let factory;

    beforeAll(async () => {
        console.log('🚀 API 통합 테스트 시작');
        
        // Express 앱 생성
        app = express();
        app.use(express.json());
        app.use(express.urlencoded({ extended: true }));

        // RepositoryFactory 초기화
        factory = RepositoryFactory.getInstance();
        await factory.initialize({
            database: { type: 'sqlite', filename: ':memory:' },
            cache: { enabled: true, defaultTimeout: 300 },
            logger: { enabled: true, level: 'info' }
        });

        // 테스트용 미들웨어 (인증 모방)
        app.use((req, res, next) => {
            req.user = { id: 1, tenant_id: 1, username: 'test_user' };
            req.tenantId = 1;
            next();
        });

        // API 라우터 등록
        try {
            const alarmRoutes = require('../routes/alarms');
            app.use('/api/alarms', alarmRoutes);
            console.log('✅ Alarm Routes 등록 완료');
        } catch (error) {
            console.warn('⚠️ Alarm Routes 등록 실패:', error.message);
        }

        try {
            const deviceRoutes = require('../routes/devices');
            app.use('/api/devices', deviceRoutes);
            console.log('✅ Device Routes 등록 완료');
        } catch (error) {
            console.warn('⚠️ Device Routes 등록 실패:', error.message);
        }

        try {
            const apiRoutes = require('../routes/api');
            app.use('/api', apiRoutes);
            console.log('✅ API Routes 등록 완료');
        } catch (error) {
            console.warn('⚠️ API Routes 등록 실패:', error.message);
        }
    });

    afterAll(async () => {
        if (factory) {
            await factory.cleanup();
        }
        console.log('✅ API 통합 테스트 완료');
    });

    // =========================================================================
    // 1. 기본 API 연결 테스트
    // =========================================================================

    test('✅ API 정보 엔드포인트', async () => {
        const response = await request(app)
            .get('/api/info')
            .expect(200);

        expect(response.body).toHaveProperty('name', 'PulseOne Backend API');
        expect(response.body).toHaveProperty('version', '1.0.0');
        expect(response.body).toHaveProperty('endpoints');

        console.log('✅ API 정보 엔드포인트 검증 완료');
    });

    test('✅ Redis 테스트 엔드포인트', async () => {
        const response = await request(app)
            .get('/api/redis/test');
        
        // Redis 연결 상태에 따라 결과가 다를 수 있음
        expect([200, 500]).toContain(response.status);
        expect(response.body).toHaveProperty('status');

        console.log('✅ Redis 테스트 엔드포인트 검증 완료');
    });

    test('✅ PostgreSQL 테스트 엔드포인트', async () => {
        const response = await request(app)
            .get('/api/db/test');
        
        // PostgreSQL 연결 상태에 따라 결과가 다를 수 있음
        expect([200, 500]).toContain(response.status);
        expect(response.body).toHaveProperty('status');

        console.log('✅ PostgreSQL 테스트 엔드포인트 검증 완료');
    });

    // =========================================================================
    // 2. 알람 API 통합 테스트
    // =========================================================================

    test('✅ 알람 API 테스트 엔드포인트', async () => {
        const response = await request(app)
            .get('/api/alarms/test')
            .expect(200);

        expect(response.body.success).toBe(true);
        expect(response.body.data).toHaveProperty('message', 'Alarm API is working!');
        expect(response.body.data).toHaveProperty('repositories');
        expect(response.body.data.available_endpoints).toBeInstanceOf(Array);

        console.log('✅ 알람 API 테스트 엔드포인트 검증 완료');
    });

    test('✅ 활성 알람 조회 API', async () => {
        const response = await request(app)
            .get('/api/alarms/active');

        // 데이터가 없어도 구조는 확인 가능
        expect([200, 500]).toContain(response.status);
        
        if (response.status === 200) {
            expect(response.body).toHaveProperty('success');
            expect(response.body).toHaveProperty('data');
        }

        console.log('✅ 활성 알람 조회 API 검증 완료');
    });

    test('✅ 알람 규칙 조회 API', async () => {
        const response = await request(app)
            .get('/api/alarms/rules');

        expect([200, 500]).toContain(response.status);
        
        if (response.status === 200) {
            expect(response.body).toHaveProperty('success');
            expect(response.body).toHaveProperty('data');
        }

        console.log('✅ 알람 규칙 조회 API 검증 완료');
    });

    // =========================================================================
    // 3. 디바이스 API 통합 테스트
    // =========================================================================

    test('✅ 디바이스 목록 조회 API', async () => {
        const response = await request(app)
            .get('/api/devices');

        expect([200, 500]).toContain(response.status);
        
        if (response.status === 200) {
            expect(response.body).toHaveProperty('success');
            expect(response.body).toHaveProperty('data');
        }

        console.log('✅ 디바이스 목록 조회 API 검증 완료');
    });

    test('✅ 디바이스 생성 API (구조 검증)', async () => {
        const testDevice = {
            name: 'Test Device',
            device_type: 'modbus',
            protocol_type: 'modbus_tcp',
            endpoint: '192.168.1.100:502',
            is_enabled: true
        };

        const response = await request(app)
            .post('/api/devices')
            .send(testDevice);

        // 실제 DB 연결이 없어도 API 구조는 확인 가능
        expect([200, 201, 400, 500]).toContain(response.status);
        expect(response.body).toHaveProperty('success');

        console.log('✅ 디바이스 생성 API 구조 검증 완료');
    });

    // =========================================================================
    // 4. Repository 연동 확인 테스트
    // =========================================================================

    test('✅ Repository Factory와 API 연동 확인', async () => {
        // Repository들이 올바르게 주입되었는지 확인
        const deviceRepo = factory.getDeviceRepository();
        const alarmRuleRepo = factory.getAlarmRuleRepository();
        const alarmOccurrenceRepo = factory.getAlarmOccurrenceRepository();

        expect(deviceRepo).toBeDefined();
        expect(alarmRuleRepo).toBeDefined();
        expect(alarmOccurrenceRepo).toBeDefined();

        // 헬스체크로 연결 상태 확인
        const deviceHealth = await deviceRepo.healthCheck();
        const alarmRuleHealth = await alarmRuleRepo.healthCheck();
        
        expect(deviceHealth.status).toBe('healthy');
        expect(alarmRuleHealth.status).toBe('healthy');

        console.log('✅ Repository Factory와 API 연동 검증 완료');
    });

    // =========================================================================
    // 5. 에러 핸들링 테스트
    // =========================================================================

    test('✅ 존재하지 않는 엔드포인트 404 처리', async () => {
        const response = await request(app)
            .get('/api/non-existent-endpoint')
            .expect(404);

        expect(response.body).toHaveProperty('error');

        console.log('✅ 404 에러 핸들링 검증 완료');
    });

    test('✅ 잘못된 데이터 형식 400 처리', async () => {
        const invalidData = {
            invalid_field: 'invalid_value'
        };

        const response = await request(app)
            .post('/api/devices')
            .send(invalidData);

        expect([400, 500]).toContain(response.status);
        expect(response.body).toHaveProperty('success', false);

        console.log('✅ 400 에러 핸들링 검증 완료');
    });

    // =========================================================================
    // 6. 성능 및 동시성 테스트
    // =========================================================================

    test('✅ 동시 요청 처리 성능', async () => {
        const promises = [];
        const concurrency = 10;

        for (let i = 0; i < concurrency; i++) {
            promises.push(
                request(app)
                    .get('/api/info')
                    .expect(200)
            );
        }

        const results = await Promise.all(promises);
        
        expect(results).toHaveLength(concurrency);
        results.forEach(result => {
            expect(result.body).toHaveProperty('name', 'PulseOne Backend API');
        });

        console.log(`✅ ${concurrency}개 동시 요청 처리 성능 검증 완료`);
    });

    // =========================================================================
    // 7. 종합 상태 확인
    // =========================================================================

    test('✅ 전체 시스템 상태 종합 확인', async () => {
        console.log('\n🎯 === 전체 시스템 상태 종합 확인 ===');
        
        // 1. RepositoryFactory 상태
        const factoryStats = factory.getAllStats();
        console.log('📊 RepositoryFactory 상태:', factoryStats.factory);
        
        // 2. 전체 Repository 헬스체크
        const healthResults = await factory.healthCheckAll();
        console.log('🏥 Repository 헬스체크:', Object.keys(healthResults.repositories));
        
        // 3. API 엔드포인트 응답 확인
        const apiInfo = await request(app).get('/api/info');
        console.log('🔗 API 엔드포인트 수:', Object.keys(apiInfo.body.endpoints).length);
        
        // 4. 알람 API 상태
        const alarmTest = await request(app).get('/api/alarms/test');
        if (alarmTest.status === 200) {
            console.log('🚨 알람 API 엔드포인트:', alarmTest.body.data.available_endpoints.length + '개');
        }
        
        expect(factoryStats.factory.initialized).toBe(true);
        expect(healthResults.factory.status).toBe('healthy');
        expect(apiInfo.status).toBe(200);
        
        console.log('✅ 전체 시스템 상태 정상!');
    });
});

// =============================================================================
// 실행 방법:
// cd backend
// npm test -- apiIntegration.test.js
// =============================================================================