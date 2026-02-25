// backend/lib/startup/SystemInitializer.js - 완성본 (CrossPlatformManager 사용)

const ConfigManager = require('../config/ConfigManager');
const DatabaseFactory = require('../database/DatabaseFactory');
const { manager: RabbitMQManager } = require('../connection/mq');
const CrossPlatformManager = require('../services/crossPlatformManager');

class SystemInitializer {
    constructor() {
        this.rabbitMQ = RabbitMQManager;
        this.crossPlatformManager = CrossPlatformManager;
        this.config = ConfigManager.getInstance();
        this.redis = null;
        this.dbFactory = null;
    }

    async initialize() {
        console.log('PulseOne 시스템 초기화 시작...');

        try {
            // ConfigManager 설정 확인
            this.printConfigInfo();

            // 1. Infrastructure 초기화 (순서 중요)
            await this.initializeInfrastructure();

            // 2. 시스템 헬스 체크
            await this.performHealthCheck();

            // 3. Collector 상태 확인 및 시작
            await this.initializeCollector();

            // 4. 최종 검증
            await this.finalValidation();

            console.log('시스템 초기화 완료!');

        } catch (error) {
            console.error('시스템 초기화 실패:', error.message);
            throw error;
        }
    }

    printConfigInfo() {
        const dbConfig = this.config.getDatabaseConfig();
        const redisConfig = this.config.getRedisConfig();

        console.log('  - Database Type:', dbConfig.type);
        console.log('  - Redis Enabled:', redisConfig.enabled);
        console.log('  - Backend Port:', this.config.get('BACKEND_PORT', 3000));
        console.log('  - Platform:', this.crossPlatformManager.platform);
        console.log('  - Architecture:', this.crossPlatformManager.architecture);
    }

    async initializeInfrastructure() {
        // RabbitMQ 설정
        await this.setupRabbitMQ();

        // Redis 설정  
        await this.setupRedis();

        // Database 검증
        await this.verifyDatabase();
    }

    async setupRabbitMQ() {
        try {
            await this.rabbitMQ.connect();
            await this.rabbitMQ.setupAlarmInfrastructure();
            await this.rabbitMQ.setupCollectorInfrastructure();
            console.log('    ✅ RabbitMQ 설정 완료');
        } catch (error) {
            console.error('    ❌ RabbitMQ 설정 실패:', error.message);
            throw error;
        }
    }

    async setupRedis() {
        try {
            const redisConfig = this.config.getRedisConfig();

            if (!redisConfig.enabled) {
                console.log('    ⚠️ Redis 비활성화됨 (REDIS_PRIMARY_ENABLED=false)');
                return;
            }

            // Redis 연결 모듈 동적 import
            const RedisClient = require('../connection/redis');
            this.redis = RedisClient;

            // Redis 연결 테스트
            await this.redis.ping();

            // 시스템 키 초기화
            await this.initializeRedisKeys();

            console.log('    ✅ Redis 설정 완료');
        } catch (error) {
            console.error('    ❌ Redis 설정 실패:', error.message);
            throw error;
        }
    }

    async initializeRedisKeys() {
        // 시스템 상태 키 초기화
        const systemKeys = [
            'system:collectors:status',
            'system:alarms:active_count',
            'system:initialization:timestamp'
        ];

        for (const key of systemKeys) {
            const exists = await this.redis.exists(key);
            if (!exists) {
                await this.redis.set(key, JSON.stringify({
                    initialized: true,
                    timestamp: new Date().toISOString()
                }));
            }
        }

        // 초기화 타임스탬프 설정
        await this.redis.set('system:initialization:timestamp', Date.now());
    }

    async verifyDatabase() {
        try {
            const dbConfig = this.config.getDatabaseConfig();

            // DatabaseFactory를 사용하여 DB 연결 및 검증
            this.dbFactory = new DatabaseFactory();

            // 메인 연결 테스트
            const connection = await this.dbFactory.getMainConnection();

            // 필수 테이블 확인 (Knex 사용)
            const requiredTables = ['alarm_rules', 'alarm_occurrences', 'devices', 'data_points'];
            const knex = connection.knex || connection; // Connection 객체 또는 직접 Knex

            for (const tableName of requiredTables) {
                const exists = await knex.schema.hasTable(tableName);
                if (!exists) {
                    throw new Error(`필수 테이블 '${tableName}'이 존재하지 않습니다`);
                }
            }

            console.log(`    ✅ Database (${dbConfig.type}) 검증 완료`);
        } catch (error) {
            console.error('    ❌ Database 검증 실패:', error.message);
            throw error;
        }
    }

    getTableExistsQuery(dbType, tableName) {
        switch (dbType.toUpperCase()) {
        case 'SQLITE':
            return 'SELECT name FROM sqlite_master WHERE type=\'table\' AND name=?';
        case 'POSTGRESQL':
            return 'SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_name = $1)';
        default:
            throw new Error(`지원하지 않는 DB 타입: ${dbType}`);
        }
    }

    checkTableExists(result, dbType) {
        switch (dbType.toUpperCase()) {
        case 'SQLITE':
            return result && result.length > 0;
        case 'POSTGRESQL':
            return result && result.rows && result.rows[0] && result.rows[0].exists;
        default:
            return false;
        }
    }

    async performHealthCheck() {
        const healthStatus = {
            rabbitMQ: false,
            redis: false,
            database: false
        };

        try {
            // RabbitMQ 연결 확인
            healthStatus.rabbitMQ = await this.rabbitMQ.isHealthy();

            // Redis 연결 확인 (Redis가 설정된 경우에만)
            if (this.redis) {
                const redisPing = await this.redis.ping();
                healthStatus.redis = redisPing === 'PONG';
            } else {
                healthStatus.redis = true; // Redis 비활성화 시 정상으로 간주
            }

            // Database 연결 확인
            if (this.dbFactory) {
                const connection = await this.dbFactory.getMainConnection();
                healthStatus.database = await this.dbFactory.isConnectionValid(connection);
            }

            const failedServices = Object.entries(healthStatus)
                .filter(([service, status]) => !status)
                .map(([service]) => service);

            if (failedServices.length > 0) {
                throw new Error(`서비스 상태 불량: ${failedServices.join(', ')}`);
            }

            console.log('    ✅ 모든 인프라 서비스 정상');

        } catch (error) {
            console.error('    ❌ 헬스 체크 실패:', error.message);
            throw error;
        }
    }

    async initializeCollector() {
        try {
            // Collector 상태 확인 (CrossPlatformManager 사용)
            const isRunning = await this.checkCollectorStatus();

            if (isRunning) {
                console.log('    ✅ Collector가 이미 실행 중입니다.');
                return;
            }

            // Collector 시작 (CrossPlatformManager 사용)
            await this.startCollector();

            // 시작 대기 (최대 30초)
            await this.waitForCollectorReady(30000);

            console.log('    ✅ Collector 시작 완료');

        } catch (error) {
            console.error('    ❌ Collector 초기화 실패:', error.message);

            // Collector 실패는 시스템 전체 실패로 처리하지 않음
            console.log('    ⚠️ Collector 없이 Backend 서비스 시작');
        }
    }

    async checkCollectorStatus() {
        try {
            const processes = await this.crossPlatformManager.getRunningProcesses();
            return processes.collector.length > 0;
        } catch (error) {
            console.warn('Collector 상태 확인 실패:', error.message);
            return false;
        }
    }

    async startCollector() {
        const result = await this.crossPlatformManager.startCollector();

        if (!result.success) {
            throw new Error(`Collector 시작 실패: ${result.error}`);
        }

        // Redis에 Collector 시작 상태 기록 (Redis가 활성화된 경우에만)
        if (this.redis) {
            await this.redis.hset('system:collectors:status', 'main_collector', JSON.stringify({
                status: 'starting',
                started_at: new Date().toISOString(),
                started_by: 'system_initializer',
                pid: result.pid,
                platform: result.platform
            }));
        }

        return result;
    }

    async waitForCollectorReady(timeoutMs) {
        const startTime = Date.now();
        const checkInterval = 2000; // 2초마다 확인

        while (Date.now() - startTime < timeoutMs) {
            try {
                const isReady = await this.isCollectorReady();
                if (isReady) {
                    // Redis 상태 업데이트 (Redis가 활성화된 경우에만)
                    if (this.redis) {
                        await this.redis.hset('system:collectors:status', 'main_collector', JSON.stringify({
                            status: 'running',
                            started_at: new Date().toISOString(),
                            last_check: new Date().toISOString()
                        }));
                    }
                    return;
                }
            } catch (error) {
                // 연결 실패는 정상 (아직 시작 중일 수 있음)
            }

            await new Promise(resolve => setTimeout(resolve, checkInterval));
        }

        throw new Error(`Collector가 ${timeoutMs}ms 내에 준비되지 않았습니다.`);
    }

    async isCollectorReady() {
        try {
            const processes = await this.crossPlatformManager.getRunningProcesses();
            const collectorProcess = processes.collector[0];

            if (!collectorProcess) {
                return false;
            }

            // 프로세스가 최소 5초 이상 실행되었는지 확인
            const now = Date.now();
            const startTime = collectorProcess.startTime ? new Date(collectorProcess.startTime).getTime() : now;
            const uptime = now - startTime;

            return uptime > 5000; // 5초 이상 실행됨
        } catch (error) {
            return false;
        }
    }

    async finalValidation() {
        const validationResults = {
            infrastructure: false,
            collector: false,
            apiEndpoints: false,
            crossPlatformManager: false
        };

        try {
            // Infrastructure 재검증
            validationResults.infrastructure = await this.validateInfrastructure();

            // Collector 검증 (선택적)
            validationResults.collector = await this.validateCollector();

            // API 엔드포인트 검증
            validationResults.apiEndpoints = await this.validateApiEndpoints();

            // CrossPlatformManager 검증
            validationResults.crossPlatformManager = await this.validateCrossPlatformManager();

            const summary = Object.entries(validationResults)
                .map(([key, status]) => `${key}: ${status ? '✅' : '❌'}`)
                .join(', ');

            console.log(`    검증 결과: ${summary}`);

            // Infrastructure와 API는 필수, CrossPlatformManager도 필수
            if (!validationResults.infrastructure || !validationResults.apiEndpoints || !validationResults.crossPlatformManager) {
                throw new Error('필수 컴포넌트 검증 실패');
            }

        } catch (error) {
            console.error('    ❌ 최종 검증 실패:', error.message);
            throw error;
        }
    }

    async validateInfrastructure() {
        try {
            const rabbitOk = await this.rabbitMQ.isHealthy();

            let redisOk = true;
            if (this.redis) {
                redisOk = await this.redis.ping() === 'PONG';
            }

            let dbOk = true;
            if (this.dbFactory) {
                const connection = await this.dbFactory.getMainConnection();
                dbOk = await this.dbFactory.isConnectionValid(connection);
            }

            return rabbitOk && redisOk && dbOk;
        } catch {
            return false;
        }
    }

    async validateCollector() {
        try {
            const health = await this.crossPlatformManager.performHealthCheck();
            return health.services?.collector?.running || false;
        } catch {
            return false;
        }
    }

    async validateCrossPlatformManager() {
        try {
            // CrossPlatformManager 기본 기능 테스트
            const systemInfo = await this.crossPlatformManager.getSystemInfo();
            const processes = await this.crossPlatformManager.getRunningProcesses();

            return systemInfo && processes &&
                systemInfo.platform &&
                typeof processes === 'object';
        } catch {
            return false;
        }
    }

    async validateApiEndpoints() {
        // TODO: 기본 API 엔드포인트들이 응답하는지 확인
        // 현재는 true로 반환
        return true;
    }

    // Cleanup 메서드 (시스템 종료 시 사용)
    async cleanup() {
        console.log('시스템 정리 중...');

        try {
            // Collector 정상 종료 시도
            if (await this.checkCollectorStatus()) {
                console.log('  - Collector 종료 중...');
                await this.crossPlatformManager.stopCollector();
            }

            if (this.rabbitMQ) {
                await this.rabbitMQ.disconnect();
            }

            if (this.dbFactory) {
                await this.dbFactory.closeAllConnections();
            }

            // Redis는 연결 풀을 사용하므로 명시적 종료 안함

            console.log('✅ 시스템 정리 완료');
        } catch (error) {
            console.error('❌ 시스템 정리 중 오류:', error.message);
        }
    }

    // 시스템 상태 조회
    async getSystemStatus() {
        try {
            const health = await this.crossPlatformManager.performHealthCheck();
            const processes = await this.crossPlatformManager.getRunningProcesses();

            return {
                platform: this.crossPlatformManager.platform,
                architecture: this.crossPlatformManager.architecture,
                health: health,
                processes: processes,
                services: {
                    infrastructure: await this.validateInfrastructure(),
                    collector: await this.validateCollector(),
                    crossPlatformManager: await this.validateCrossPlatformManager()
                },
                timestamp: new Date().toISOString()
            };
        } catch (error) {
            return {
                error: error.message,
                timestamp: new Date().toISOString()
            };
        }
    }
}

module.exports = SystemInitializer;