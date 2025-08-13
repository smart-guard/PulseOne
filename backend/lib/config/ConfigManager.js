// ============================================================================
// config/ConfigManager.js
// 환경변수 전용 관리 시스템 (C++ ConfigManager 패턴 적용)
// ============================================================================

const path = require('path');
const fs = require('fs');
const dotenv = require('dotenv');

/**
 * 환경변수 관리자 클래스 (싱글톤)
 * 모든 .env 파일들을 통합 관리하고 전역 환경변수 제공
 */
class ConfigManager {
    constructor() {
        this.configDir = __dirname;
        this.loaded = false;
        this.loadedFiles = [];
        this.env = new Map();
        this.lastLoad = new Map();
        this.logger = console;
        
        // 환경변수 파일 우선순위 정의
        this.envFilePriority = [
            '.env',              // 기본 환경변수
            'database.env',      // 데이터베이스 설정
            'redis.env',         // Redis 설정
            'timeseries.env',    // InfluxDB 설정
            'messaging.env',     // RabbitMQ 설정
            'security.env'       // 보안 설정
        ];
        
        // 자동 초기화
        this.initialize();
    }

    // ========================================================================
    // 싱글톤 패턴
    // ========================================================================

    static getInstance() {
        if (!ConfigManager.instance) {
            ConfigManager.instance = new ConfigManager();
        }
        return ConfigManager.instance;
    }

    // ========================================================================
    // 초기화 및 로딩
    // ========================================================================

    /**
     * 환경변수 초기화 및 로딩
     */
    initialize() {
        if (this.loaded) {
            return this.env;
        }

        this.logger.log('🔧 ConfigManager 환경변수 로딩 시작...');

        try {
            // 1. 우선순위에 따라 환경변수 파일 로드
            this.loadEnvironmentFiles();

            // 2. 환경변수를 내부 Map으로 복사
            this.copyProcessEnv();

            this.loaded = true;
            this.logger.log(`✅ 환경변수 로딩 완료 (${this.loadedFiles.length}개 파일)`);
            
            // 3. 개발 환경에서 디버깅 정보 출력
            if (this.get('NODE_ENV') === 'development') {
                this.printDebugInfo();
            }

            return this.env;

        } catch (error) {
            this.logger.error('❌ ConfigManager 환경변수 로딩 실패:', error.message);
            throw error;
        }
    }

    /**
     * 환경변수 파일들 로드 (우선순위 기반)
     */
    loadEnvironmentFiles() {
        // 우선순위 파일들 먼저 로드
        this.envFilePriority.forEach(filename => {
            this.loadEnvFile(filename);
        });

        // 추가 .env 파일들 자동 탐지 및 로드
        const additionalFiles = this.findAdditionalEnvFiles();
        additionalFiles.forEach(filename => {
            if (!this.envFilePriority.includes(filename)) {
                this.loadEnvFile(filename);
            }
        });
    }

    /**
     * 추가 환경변수 파일 탐지
     */
    findAdditionalEnvFiles() {
        const envFiles = [];
        
        try {
            const files = fs.readdirSync(this.configDir);
            
            // .env로 끝나는 파일들 필터링
            const additionalEnvFiles = files
                .filter(file => file.endsWith('.env'))
                .sort();
            
            envFiles.push(...additionalEnvFiles);

        } catch (error) {
            this.logger.warn('⚠️ config 디렉토리 읽기 실패:', error.message);
        }

        return envFiles;
    }

    /**
     * 개별 환경변수 파일 로드
     */
    loadEnvFile(filename) {
        const filePath = path.join(this.configDir, filename);
        
        if (!fs.existsSync(filePath)) {
            if (filename === '.env') {
                this.logger.warn(`⚠️ 기본 환경변수 파일 없음: ${filename}`);
            }
            return false;
        }

        try {
            const result = dotenv.config({ path: filePath });
            
            if (result.error) {
                this.logger.warn(`⚠️ ${filename} 로딩 오류:`, result.error.message);
                return false;
            }

            this.loadedFiles.push(filename);
            this.lastLoad.set(filename, Date.now());
            this.logger.log(`✅ 로드됨: ${filename}`);
            return true;

        } catch (error) {
            this.logger.error(`❌ ${filename} 로딩 실패:`, error.message);
            return false;
        }
    }

    /**
     * process.env를 내부 Map으로 복사
     */
    copyProcessEnv() {
        // 모든 환경변수를 내부 Map으로 복사
        Object.keys(process.env).forEach(key => {
            this.env.set(key, process.env[key]);
        });
    }

    // ========================================================================
    // 환경변수 조회 메서드들
    // ========================================================================

    /**
     * 환경변수 값 조회 (기본값 지원)
     */
    get(key, defaultValue = undefined) {
        const value = this.env.get(key) || process.env[key];
        return value !== undefined ? value : defaultValue;
    }

    /**
     * 환경변수 값 설정 (메모리상에서만)
     */
    set(key, value) {
        this.env.set(key, value);
        process.env[key] = value;
    }

    /**
     * Boolean 값 조회
     */
    getBoolean(key, defaultValue = false) {
        const value = this.get(key);
        if (value === undefined) return defaultValue;
        
        return value.toLowerCase() === 'true' || value === '1';
    }

    /**
     * Number 값 조회
     */
    getNumber(key, defaultValue = 0) {
        const value = this.get(key);
        if (value === undefined) return defaultValue;
        
        const num = parseInt(value, 10);
        return isNaN(num) ? defaultValue : num;
    }

    /**
     * 배열 값 조회 (쉼표로 구분)
     */
    getArray(key, defaultValue = []) {
        const value = this.get(key);
        if (!value) return defaultValue;
        
        return value.split(',').map(item => item.trim()).filter(item => item);
    }

    /**
     * 필수 환경변수 확인
     */
    require(key) {
        const value = this.get(key);
        if (value === undefined || value === '') {
            throw new Error(`필수 환경변수가 설정되지 않음: ${key}`);
        }
        return value;
    }

    /**
     * 여러 환경변수 일괄 확인
     */
    requireAll(keys) {
        const missing = [];
        const values = {};

        keys.forEach(key => {
            try {
                values[key] = this.require(key);
            } catch (error) {
                missing.push(key);
            }
        });

        if (missing.length > 0) {
            throw new Error(`누락된 필수 환경변수들: ${missing.join(', ')}`);
        }

        return values;
    }

    // ========================================================================
    // 설정 그룹별 조회 메서드들
    // ========================================================================

    /**
     * 환경별 설정 조회
     */
    getEnvironmentConfig() {
        const env = this.get('NODE_ENV', 'development');
        
        return {
            isDevelopment: env === 'development',
            isProduction: env === 'production',
            isTest: env === 'test',
            isStaging: env === 'staging',
            environment: env
        };
    }

    /**
     * 데이터베이스 설정 조회
     */
    getDatabaseConfig() {
        return {
            type: this.get('DATABASE_TYPE', 'SQLITE'),
            enabled: this.getBoolean('DATABASE_ENABLED', true),
            
            // SQLite
            sqlite: {
                enabled: this.getBoolean('SQLITE_ENABLED', true),
                path: this.get('SQLITE_DB_PATH', './data/db/pulseone.db'),
                timeout: this.getNumber('SQLITE_TIMEOUT_MS', 30000),
                journalMode: this.get('SQLITE_JOURNAL_MODE', 'WAL'),
                foreignKeys: this.getBoolean('SQLITE_FOREIGN_KEYS', true)
            },

            // PostgreSQL
            postgresql: {
                enabled: this.getBoolean('POSTGRESQL_ENABLED', false),
                host: this.get('POSTGRESQL_HOST', 'localhost'),
                port: this.getNumber('POSTGRESQL_PORT', 5432),
                database: this.get('POSTGRESQL_DATABASE', 'pulseone'),
                username: this.get('POSTGRESQL_USERNAME', 'postgres'),
                password: this.get('POSTGRESQL_PASSWORD', '')
            }
        };
    }

    /**
     * Redis 설정 조회
     */
    getRedisConfig() {
        return {
            enabled: this.getBoolean('REDIS_PRIMARY_ENABLED', true),
            host: this.get('REDIS_PRIMARY_HOST', 'localhost'),
            port: this.getNumber('REDIS_PRIMARY_PORT', 6379),
            password: this.get('REDIS_PRIMARY_PASSWORD', ''),
            db: this.getNumber('REDIS_PRIMARY_DB', 0),
            poolSize: this.getNumber('REDIS_POOL_SIZE', 20),
            keyPrefix: this.get('REDIS_KEY_PREFIX', 'pulseone:'),
            testMode: this.getBoolean('REDIS_TEST_MODE', false)
        };
    }

    /**
     * 서버 설정 조회
     */
    getServerConfig() {
        return {
            port: this.getNumber('BACKEND_PORT', 3000),
            host: this.get('BACKEND_HOST', '0.0.0.0'),
            env: this.get('NODE_ENV', 'development'),
            logLevel: this.get('LOG_LEVEL', 'info')
        };
    }

    /**
     * InfluxDB 설정 조회
     */
    getInfluxConfig() {
        return {
            enabled: this.getBoolean('INFLUX_ENABLED', false),
            host: this.get('INFLUXDB_HOST', 'localhost'),
            port: this.getNumber('INFLUXDB_PORT', 8086),
            token: this.get('INFLUXDB_TOKEN', ''),
            org: this.get('INFLUXDB_ORG', 'pulseone'),
            bucket: this.get('INFLUXDB_BUCKET', 'timeseries')
        };
    }

    /**
     * RabbitMQ 설정 조회
     */
    getRabbitMQConfig() {
        return {
            enabled: this.getBoolean('RABBITMQ_ENABLED', false),
            host: this.get('RABBITMQ_HOST', 'localhost'),
            port: this.getNumber('RABBITMQ_PORT', 5672),
            user: this.get('RABBITMQ_USER', 'guest'),
            password: this.get('RABBITMQ_PASSWORD', 'guest'),
            vhost: this.get('RABBITMQ_VHOST', '/'),
            managementPort: this.getNumber('RABBITMQ_MANAGEMENT_PORT', 15672)
        };
    }

    // ========================================================================
    // 유틸리티 메서드들
    // ========================================================================

    /**
     * 환경변수 유효성 검증
     */
    validate(validationRules) {
        const errors = [];

        Object.entries(validationRules).forEach(([key, rules]) => {
            const value = this.get(key);

            // Required 체크
            if (rules.required && (value === undefined || value === '')) {
                errors.push(`${key}: 필수 값이 누락됨`);
                return;
            }

            // Type 체크
            if (value && rules.type) {
                switch (rules.type) {
                    case 'number':
                        if (isNaN(Number(value))) {
                            errors.push(`${key}: 숫자 형식이 아님 (현재값: ${value})`);
                        }
                        break;
                    case 'boolean':
                        if (!['true', 'false', '1', '0'].includes(value.toLowerCase())) {
                            errors.push(`${key}: boolean 형식이 아님 (현재값: ${value})`);
                        }
                        break;
                    case 'email':
                        if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(value)) {
                            errors.push(`${key}: 이메일 형식이 아님 (현재값: ${value})`);
                        }
                        break;
                }
            }

            // Enum 체크
            if (value && rules.enum && !rules.enum.includes(value)) {
                errors.push(`${key}: 허용되지 않는 값 (현재값: ${value}, 허용값: ${rules.enum.join(', ')})`);
            }
        });

        if (errors.length > 0) {
            throw new Error(`환경변수 유효성 검증 실패:\n${errors.join('\n')}`);
        }

        return true;
    }

    /**
     * 디버깅용 정보 출력
     */
    printDebugInfo() {
        this.logger.log('\n📋 ConfigManager 환경변수 디버깅 정보:');
        this.logger.log(`   로딩된 파일들: ${this.loadedFiles.join(', ')}`);
        this.logger.log(`   NODE_ENV: ${this.get('NODE_ENV')}`);
        this.logger.log(`   DATABASE_TYPE: ${this.get('DATABASE_TYPE')}`);
        this.logger.log(`   REDIS_PRIMARY_HOST: ${this.get('REDIS_PRIMARY_HOST')}`);
        this.logger.log(`   BACKEND_PORT: ${this.get('BACKEND_PORT')}`);
        this.logger.log('');
    }

    /**
     * 모든 환경변수 조회 (디버깅용)
     */
    getAll() {
        const result = {};
        this.env.forEach((value, key) => {
            result[key] = value;
        });
        return result;
    }

    /**
     * 로드된 파일 목록 조회
     */
    getLoadedFiles() {
        return [...this.loadedFiles];
    }

    /**
     * 재로드 (개발 중 설정 변경 시)
     */
    reload() {
        this.loaded = false;
        this.loadedFiles = [];
        this.env.clear();
        this.lastLoad.clear();
        return this.initialize();
    }
}

// 싱글톤 인스턴스 생성 및 내보내기
const configManager = ConfigManager.getInstance();

// 기존 방식 호환성을 위한 직접 export (redis.js 등에서 사용)
module.exports = {
    // ConfigManager 인스턴스
    getInstance: () => configManager,

    // 기존 방식 호환성 (redis.js에서 사용하는 이름들)
    REDIS_MAIN_HOST: configManager.get('REDIS_PRIMARY_HOST', 'localhost'),
    REDIS_MAIN_PORT: configManager.get('REDIS_PRIMARY_PORT', '6379'),
    REDIS_MAIN_PASSWORD: configManager.get('REDIS_PRIMARY_PASSWORD', ''),

    // 자주 사용되는 환경변수들
    NODE_ENV: configManager.get('NODE_ENV', 'development'),
    LOG_LEVEL: configManager.get('LOG_LEVEL', 'info'),
    BACKEND_PORT: configManager.getNumber('BACKEND_PORT', 3000),
    DATABASE_TYPE: configManager.get('DATABASE_TYPE', 'SQLITE'),

    // 헬퍼 함수들 (전역에서 사용 가능)
    get: (key, defaultValue) => configManager.get(key, defaultValue),
    getBoolean: (key, defaultValue) => configManager.getBoolean(key, defaultValue),
    getNumber: (key, defaultValue) => configManager.getNumber(key, defaultValue),
    require: (key) => configManager.require(key),
    
    // 설정 그룹들
    getDatabaseConfig: () => configManager.getDatabaseConfig(),
    getRedisConfig: () => configManager.getRedisConfig(),
    getServerConfig: () => configManager.getServerConfig(),
    getInfluxConfig: () => configManager.getInfluxConfig(),
    getRabbitMQConfig: () => configManager.getRabbitMQConfig()
};