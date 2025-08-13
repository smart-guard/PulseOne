// ===========================================================================
// backend/lib/connection/redis.js - 기존 환경변수 구조 완전 호환
// ===========================================================================
const redis = require('redis');
const ConfigManager = require('../config/ConfigManager');

const config = ConfigManager.getInstance();

class RedisManager {
    constructor() {
        this.client = null;
        this.isConnecting = false;
        this.isConnected = false;
        
        // 기존 환경변수 구조 사용
        this.redisConfig = {
            enabled: config.getBoolean('REDIS_PRIMARY_ENABLED', false),
            host: config.get('REDIS_PRIMARY_HOST', 'localhost'),
            port: config.getNumber('REDIS_PRIMARY_PORT', 6379),
            password: config.get('REDIS_PRIMARY_PASSWORD', ''),
            db: config.getNumber('REDIS_PRIMARY_DB', 0),
            timeout: config.getNumber('REDIS_PRIMARY_TIMEOUT_MS', 5000),
            connectTimeout: config.getNumber('REDIS_PRIMARY_CONNECT_TIMEOUT_MS', 3000),
            keyPrefix: config.get('REDIS_KEY_PREFIX', 'pulseone:'),
            testMode: config.getBoolean('REDIS_TEST_MODE', false)
        };
        
        // 기존 REDIS_MAIN_* 환경변수 호환성
        if (!this.redisConfig.enabled && config.get('REDIS_MAIN_HOST')) {
            this.redisConfig.host = config.get('REDIS_MAIN_HOST', 'localhost');
            this.redisConfig.port = config.getNumber('REDIS_MAIN_PORT', 6379);
            this.redisConfig.password = config.get('REDIS_MAIN_PASSWORD', '');
            this.redisConfig.enabled = true;
        }
    }

    async getClient() {
        // Redis가 비활성화되어 있으면 Mock 클라이언트 반환
        if (!this.redisConfig.enabled) {
            console.log('⚠️ Redis가 비활성화됨 (REDIS_PRIMARY_ENABLED=false)');
            return this.createMockClient();
        }

        // 이미 연결된 클라이언트가 있으면 반환
        if (this.client && this.isConnected) {
            return this.client;
        }

        // 연결 중이면 기다림
        if (this.isConnecting) {
            await this.waitForConnection();
            return this.client;
        }

        // 새로 연결 시작
        return await this.connect();
    }

    async connect() {
        if (this.isConnecting) {
            return await this.waitForConnection();
        }

        this.isConnecting = true;

        try {
            console.log('🔗 Redis 연결 설정 로드 중...');

            // Redis 설정
            const redisUrl = `redis://${this.redisConfig.host}:${this.redisConfig.port}`;
            const clientConfig = {
                url: redisUrl,
                retry_unfulfilled_commands: true,
                retry_delay_on_failure: 2000,
                max_attempts: 3,
                connect_timeout: this.redisConfig.connectTimeout,
                command_timeout: this.redisConfig.timeout
            };

            // 비밀번호가 설정된 경우 추가
            if (this.redisConfig.password) {
                clientConfig.password = this.redisConfig.password;
            }

            // 데이터베이스 선택
            if (this.redisConfig.db > 0) {
                clientConfig.database = this.redisConfig.db;
            }

            console.log(`📋 Redis 연결 설정:
   호스트: ${this.redisConfig.host}:${this.redisConfig.port}
   데이터베이스: ${this.redisConfig.db}
   비밀번호: ${this.redisConfig.password ? '설정됨' : '없음'}
   키 접두사: ${this.redisConfig.keyPrefix}
   테스트 모드: ${this.redisConfig.testMode}`);

            this.client = redis.createClient(clientConfig);

            // 이벤트 핸들러 설정
            this.setupEventHandlers();

            // 연결 시도
            await this.client.connect();

            this.isConnected = true;
            this.isConnecting = false;

            console.log('✅ Redis 연결 성공');
            
            // 테스트 모드면 추가 설정
            if (this.redisConfig.testMode) {
                console.log('🧪 Redis 테스트 모드 활성화');
            }

            return this.client;

        } catch (error) {
            this.isConnecting = false;
            this.isConnected = false;
            
            console.error('❌ Redis 초기 연결 실패:', error.message);
            console.log('⚠️  Redis 없이 계속 진행합니다.');
            
            // Redis 없어도 Mock 클라이언트 반환
            this.client = this.createMockClient();
            return this.client;
        }
    }

    setupEventHandlers() {
        if (!this.client) return;

        this.client.on('error', (err) => {
            console.error('❌ Redis 연결 오류:', err.message);
            this.isConnected = false;
        });

        this.client.on('connect', () => {
            console.log('🔄 Redis 연결됨');
        });

        this.client.on('ready', () => {
            console.log('✅ Redis 준비 완료');
            this.isConnected = true;
        });

        this.client.on('disconnect', () => {
            console.warn('⚠️ Redis 연결 해제됨');
            this.isConnected = false;
        });

        this.client.on('reconnecting', () => {
            console.log('🔄 Redis 재연결 시도 중...');
        });
    }

    async waitForConnection() {
        return new Promise((resolve) => {
            const checkConnection = () => {
                if (!this.isConnecting) {
                    resolve(this.client);
                } else {
                    setTimeout(checkConnection, 100);
                }
            };
            checkConnection();
        });
    }

    createMockClient() {
        // Redis가 없을 때 사용할 Mock 클라이언트
        console.log('🎭 Redis Mock 클라이언트 생성');
        return {
            get: async () => null,
            set: async () => 'OK',
            setEx: async () => 'OK',
            del: async () => 1,
            exists: async () => 0,
            expire: async () => 1,
            ttl: async () => -1,
            keys: async () => [],
            flushall: async () => 'OK',
            ping: async () => 'PONG',
            quit: async () => 'OK',
            disconnect: async () => {},
            isOpen: false,
            isReady: false,
            isMock: true
        };
    }

    async close() {
        if (this.client && this.isConnected && !this.client.isMock) {
            try {
                await this.client.quit();
                console.log('📴 Redis 연결 종료');
            } catch (error) {
                console.error('❌ Redis 종료 오류:', error.message);
            }
        }
        this.client = null;
        this.isConnected = false;
        this.isConnecting = false;
    }

    getStatus() {
        return {
            enabled: this.redisConfig.enabled,
            connected: this.isConnected,
            connecting: this.isConnecting,
            client: !!this.client,
            host: this.redisConfig.host,
            port: this.redisConfig.port,
            testMode: this.redisConfig.testMode
        };
    }
}

// 싱글톤 인스턴스
let redisManager;

function getRedisManager() {
    if (!redisManager) {
        redisManager = new RedisManager();
    }
    return redisManager;
}

// 기존 인터페이스 호환성을 위한 프록시 클라이언트
const redisProxy = new Proxy({}, {
    get(target, prop) {
        if (prop === 'getManager') {
            return getRedisManager;
        }
        
        return async function(...args) {
            const manager = getRedisManager();
            const client = await manager.getClient();
            
            if (client && typeof client[prop] === 'function') {
                return await client[prop](...args);
            } else {
                // Mock 클라이언트의 경우 기본값 반환
                if (client && client.isMock) {
                    return client[prop] ? await client[prop](...args) : null;
                }
                console.warn(`⚠️ Redis 메서드 ${prop} 호출 실패`);
                return null;
            }
        };
    }
});

module.exports = redisProxy;