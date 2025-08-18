// ===========================================================================
// backend/lib/connection/redis.js - redis.env 설정 기반
// ===========================================================================
const redis = require('redis');
const ConfigManager = require('../config/ConfigManager');

const config = ConfigManager.getInstance();

class RedisManager {
    constructor() {
        this.client = null;
        this.isConnecting = false;
        this.isConnected = false;
        
        // redis.env 파일의 환경변수 기반 설정
        this.redisConfig = {
            enabled: config.getBoolean('REDIS_PRIMARY_ENABLED', false),
            host: config.get('REDIS_PRIMARY_HOST', 'localhost'),
            port: config.getNumber('REDIS_PRIMARY_PORT', 6379),
            password: config.get('REDIS_PRIMARY_PASSWORD', ''),
            db: config.getNumber('REDIS_PRIMARY_DB', 0),
            timeout: config.getNumber('REDIS_PRIMARY_TIMEOUT_MS', 5000),
            connectTimeout: config.getNumber('REDIS_PRIMARY_CONNECT_TIMEOUT_MS', 3000),
            
            // 연결 풀 설정
            poolSize: config.getNumber('REDIS_POOL_SIZE', 5),
            poolMaxIdle: config.getNumber('REDIS_POOL_MAX_IDLE', 3),
            poolMaxActive: config.getNumber('REDIS_POOL_MAX_ACTIVE', 10),
            poolMaxWait: config.getNumber('REDIS_POOL_MAX_WAIT_MS', 5000),
            connectionTimeout: config.getNumber('REDIS_CONNECTION_TIMEOUT_MS', 30000),
            commandTimeout: config.getNumber('REDIS_COMMAND_TIMEOUT_MS', 10000),
            tcpKeepAlive: config.getBoolean('REDIS_TCP_KEEPALIVE', true),
            
            // 데이터 저장 설정 (접두사 없음)
            keyPrefix: '', // 접두사 사용 안함
            defaultExpiry: config.getNumber('REDIS_DEFAULT_EXPIRY_S', 3600),
            maxMemoryPolicy: config.get('REDIS_MAX_MEMORY_POLICY', 'allkeys-lru'),
            
            // 연결 관리
            autoPing: config.getBoolean('REDIS_AUTO_PING', true),
            pingInterval: config.getNumber('REDIS_PING_INTERVAL_S', 30) * 1000
        };
        
        console.log('🔧 Redis 설정 로드:', {
            enabled: this.redisConfig.enabled,
            host: this.redisConfig.host,
            port: this.redisConfig.port,
            db: this.redisConfig.db,
            hasPassword: !!this.redisConfig.password,
            poolSize: this.redisConfig.poolSize,
            autoPing: this.redisConfig.autoPing
        });
    }

    async getClient() {
        if (!this.redisConfig.enabled) {
            console.log('⚠️ Redis 비활성화됨 (REDIS_PRIMARY_ENABLED=false)');
            throw new Error('Redis가 비활성화되어 있습니다');
        }

        if (this.client && this.isConnected) {
            return this.client;
        }

        if (this.isConnecting) {
            return await this.waitForConnection();
        }

        return await this.connect();
    }

    async connect() {
        if (this.isConnecting) {
            return await this.waitForConnection();
        }

        this.isConnecting = true;

        try {
            console.log(`🔗 Redis 연결 시도: ${this.redisConfig.host}:${this.redisConfig.port}`);

            // Redis v4 클라이언트 설정
            const clientConfig = {
                socket: {
                    host: this.redisConfig.host,
                    port: this.redisConfig.port,
                    connectTimeout: this.redisConfig.connectTimeout,
                    commandTimeout: this.redisConfig.commandTimeout,
                    keepAlive: this.redisConfig.tcpKeepAlive,
                    reconnectStrategy: (retries) => {
                        if (retries > 3) {
                            console.error('❌ Redis 재연결 최대 시도 횟수 초과');
                            return false;
                        }
                        const delay = Math.min(retries * 1000, 5000);
                        console.log(`🔄 ${delay}ms 후 Redis 재연결 시도 (${retries}/3)`);
                        return delay;
                    }
                },
                // 비밀번호 설정
                password: this.redisConfig.password || undefined,
                // 데이터베이스 선택
                database: this.redisConfig.db
            };

            this.client = redis.createClient(clientConfig);
            this.setupEventHandlers();

            // 연결 시도
            await this.client.connect();

            // 연결 테스트
            const pong = await this.client.ping();
            if (pong !== 'PONG') {
                throw new Error('Redis PING 테스트 실패');
            }

            this.isConnected = true;
            this.isConnecting = false;

            console.log('✅ Redis 연결 성공');
            
            // Auto ping 설정
            if (this.redisConfig.autoPing) {
                this.startAutoPing();
            }

            return this.client;

        } catch (error) {
            this.isConnecting = false;
            this.isConnected = false;
            
            console.error('❌ Redis 연결 실패:', error.message);
            
            if (this.client) {
                try {
                    await this.client.disconnect();
                } catch (disconnectError) {
                    // 무시
                }
                this.client = null;
            }
            
            throw new Error(`Redis 연결 실패: ${error.message}`);
        }
    }

    setupEventHandlers() {
        if (!this.client) return;

        this.client.on('error', (err) => {
            console.error('❌ Redis 오류:', err.message);
            this.isConnected = false;
        });

        this.client.on('connect', () => {
            console.log('🔄 Redis 소켓 연결됨');
        });

        this.client.on('ready', () => {
            console.log('✅ Redis 클라이언트 준비 완료');
            this.isConnected = true;
        });

        this.client.on('disconnect', () => {
            console.warn('⚠️ Redis 연결 해제됨');
            this.isConnected = false;
            this.stopAutoPing();
        });

        this.client.on('reconnecting', () => {
            console.log('🔄 Redis 재연결 시도 중...');
        });

        this.client.on('end', () => {
            console.log('📴 Redis 연결 종료됨');
            this.isConnected = false;
            this.stopAutoPing();
        });
    }

    startAutoPing() {
        if (this.pingIntervalId) {
            clearInterval(this.pingIntervalId);
        }

        this.pingIntervalId = setInterval(async () => {
            try {
                if (this.client && this.isConnected) {
                    await this.client.ping();
                    console.log('🏓 Redis Auto-ping 성공');
                }
            } catch (error) {
                console.warn('⚠️ Redis Auto-ping 실패:', error.message);
            }
        }, this.redisConfig.pingInterval);

        console.log(`🏓 Redis Auto-ping 시작 (${this.redisConfig.pingInterval / 1000}초 간격)`);
    }

    stopAutoPing() {
        if (this.pingIntervalId) {
            clearInterval(this.pingIntervalId);
            this.pingIntervalId = null;
            console.log('🏓 Redis Auto-ping 중지');
        }
    }

    async waitForConnection() {
        return new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                reject(new Error('Redis 연결 대기 시간 초과'));
            }, this.redisConfig.connectTimeout);

            const checkConnection = () => {
                if (!this.isConnecting) {
                    clearTimeout(timeout);
                    if (this.client && this.isConnected) {
                        resolve(this.client);
                    } else {
                        reject(new Error('Redis 연결 실패'));
                    }
                } else {
                    setTimeout(checkConnection, 100);
                }
            };
            checkConnection();
        });
    }

    async close() {
        this.stopAutoPing();
        
        if (this.client && this.isConnected) {
            try {
                await this.client.quit();
                console.log('📴 Redis 연결 정상 종료');
            } catch (error) {
                console.error('❌ Redis 종료 오류:', error.message);
                try {
                    await this.client.disconnect();
                } catch (disconnectError) {
                    // 무시
                }
            }
        }
        this.client = null;
        this.isConnected = false;
        this.isConnecting = false;
    }

    async healthCheck() {
        try {
            if (!this.client || !this.isConnected) {
                return { status: 'disconnected', error: 'No connection' };
            }

            const start = Date.now();
            const pong = await this.client.ping();
            const latency = Date.now() - start;

            const info = await this.client.info('memory');
            const usedMemory = info.match(/used_memory_human:([^\r\n]+)/)?.[1];

            return {
                status: 'healthy',
                latency: `${latency}ms`,
                response: pong,
                memory: usedMemory,
                uptime: this.isConnected ? 'connected' : 'disconnected',
                config: {
                    host: this.redisConfig.host,
                    port: this.redisConfig.port,
                    db: this.redisConfig.db
                }
            };
        } catch (error) {
            return {
                status: 'unhealthy',
                error: error.message
            };
        }
    }

    getStatus() {
        return {
            enabled: this.redisConfig.enabled,
            connected: this.isConnected,
            connecting: this.isConnecting,
            client: !!this.client,
            config: {
                host: this.redisConfig.host,
                port: this.redisConfig.port,
                db: this.redisConfig.db,
                poolSize: this.redisConfig.poolSize,
                autoPing: this.redisConfig.autoPing,
                keyPrefix: this.redisConfig.keyPrefix || '(없음)'
            }
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

// realtime.js에서 사용할 getRedisClient 함수
async function getRedisClient() {
    try {
        const manager = getRedisManager();
        const client = await manager.getClient();
        
        if (!client) {
            throw new Error('Redis 클라이언트가 null입니다');
        }
        
        if (!client.isReady) {
            throw new Error('Redis 클라이언트가 준비되지 않았습니다');
        }
        
        return client;
    } catch (error) {
        console.error('❌ getRedisClient 실패:', error.message);
        return null;
    }
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
                console.warn(`⚠️ Redis 메서드 ${prop} 호출 실패`);
                return null;
            }
        };
    }
});

// exports
module.exports = {
    ...redisProxy,
    getRedisClient,
    getRedisManager
};


