// ============================================================================
// backend/lib/config/ConfigManager.js - 완성본 (권장사항 모두 적용)
// ============================================================================
const path = require('path');
const fs = require('fs');

class ConfigManager {
    static instance = null;
    
    constructor() {
        this.env = new Map();
        this.loaded = false;
        this.loadedFiles = [];
        this.lastLoad = new Map();
        this.logger = console;
        this.lastInitialized = null;
        
        // 즉시 초기화
        this.initialize();
    }

    static getInstance() {
        if (!ConfigManager.instance) {
            ConfigManager.instance = new ConfigManager();
        }
        return ConfigManager.instance;
    }

    /**
     * 환경변수 초기화 - 경로 문제 해결
     */
    initialize() {
        if (this.loaded) return this;

        this.logger.log('🔧 ConfigManager 환경변수 로딩 시작...');
        
        try {
            // 프로젝트 루트 찾기 (backend에서 실행되므로 상위로)
            const projectRoot = path.resolve(__dirname, '../../../');
            process.chdir(projectRoot); // 작업 디렉토리를 프로젝트 루트로 변경
            
            this.logger.log(`📁 프로젝트 루트: ${projectRoot}`);
            
            // 1. 메인 .env 파일 로드 (프로젝트 루트)
            this.loadEnvFile(path.join(projectRoot, '.env'), false);
            
            // 2. CONFIG_FILES 환경변수 기반 추가 파일 로드
            const configFiles = process.env.CONFIG_FILES || 'database.env,redis.env,timeseries.env,messaging.env';
            const configDir = path.join(projectRoot, 'config');
            
            this.logger.log(`📁 Config 디렉토리: ${configDir}`);
            
            if (configFiles) {
                const files = configFiles.split(',').map(f => f.trim());
                files.forEach(file => {
                    const fullPath = path.join(configDir, file);
                    this.loadEnvFile(fullPath, false);
                });
            }

            // 3. process.env의 모든 변수 복사
            Object.entries(process.env).forEach(([key, value]) => {
                if (!this.env.has(key)) {
                    this.env.set(key, value);
                }
            });

            this.loaded = true;
            this.lastInitialized = new Date().toISOString();
            this.logger.log(`✅ 환경변수 로딩 완료 (${this.loadedFiles.length}개 파일)`);
            
            // 디버깅 정보 출력
            this.printDebugInfo();
            
        } catch (error) {
            this.logger.error('❌ ConfigManager 초기화 실패:', error.message);
        }

        return this;
    }

    /**
     * .env 파일 로드
     */
    loadEnvFile(filePath, required = false) {
        try {
            const absolutePath = path.resolve(filePath);
            
            this.logger.log(`🔍 파일 확인 중: ${absolutePath}`);
            
            if (!fs.existsSync(absolutePath)) {
                if (required) {
                    throw new Error(`필수 환경변수 파일 없음: ${filePath}`);
                } else {
                    this.logger.warn(`⚠️ 기본 환경변수 파일 없음: ${filePath}`);
                    return false;
                }
            }

            const content = fs.readFileSync(absolutePath, 'utf8');
            const lines = content.split('\n');
            let loadedCount = 0;

            lines.forEach((line, index) => {
                line = line.trim();
                
                // 빈 줄이나 주석 무시
                if (!line || line.startsWith('#')) return;
                
                const equalIndex = line.indexOf('=');
                if (equalIndex === -1) return;
                
                const key = line.substring(0, equalIndex).trim();
                const value = line.substring(equalIndex + 1).trim();
                
                // 따옴표 제거
                const cleanValue = value.replace(/^["']|["']$/g, '');
                
                // 환경변수 설정 (기존값 우선)
                if (!this.env.has(key)) {
                    this.env.set(key, cleanValue);
                    process.env[key] = cleanValue; // process.env도 업데이트
                    loadedCount++;
                }
            });

            this.loadedFiles.push(path.basename(filePath));
            this.lastLoad.set(filePath, new Date());
            
            this.logger.log(`✅ 로드 성공: ${path.basename(filePath)} (${loadedCount}개 변수)`);
            return true;
            
        } catch (error) {
            if (required) {
                throw error;
            } else {
                this.logger.warn(`⚠️ 환경변수 파일 로드 실패: ${filePath}`, error.message);
                return false;
            }
        }
    }

    /**
     * 환경변수 조회 (기존 방식 완전 호환)
     */
    get(key, defaultValue = undefined) {
        // 1. 내부 Map에서 조회
        if (this.env.has(key)) {
            return this.env.get(key);
        }
        
        // 2. process.env에서 조회
        if (process.env[key] !== undefined) {
            return process.env[key];
        }
        
        return defaultValue;
    }

    /**
     * 숫자 타입 변환
     */
    getNumber(key, defaultValue = 0) {
        const value = this.get(key);
        if (value === undefined || value === null || value === '') {
            return defaultValue;
        }
        const parsed = parseInt(value, 10);
        return isNaN(parsed) ? defaultValue : parsed;
    }

    /**
     * 불리언 타입 변환
     */
    getBoolean(key, defaultValue = false) {
        const value = this.get(key);
        if (value === undefined || value === null || value === '') {
            return defaultValue;
        }
        return ['true', '1', 'yes', 'on'].includes(value.toLowerCase());
    }

    /**
     * 필수 환경변수 조회
     */
    require(key) {
        const value = this.get(key);
        if (value === undefined || value === null || value === '') {
            throw new Error(`필수 환경변수가 설정되지 않음: ${key}`);
        }
        return value;
    }

    /**
     * 데이터베이스 설정 조회 - 기존 환경변수 구조 완전 지원
     */
    getDatabaseConfig() {
        return {
            // 기본 타입
            type: this.get('DATABASE_TYPE', 'SQLITE'),
            
            // SQLite 설정 (기존 변수명 유지)
            sqlite: {
                enabled: this.getBoolean('SQLITE_ENABLED', true),
                path: this.get('SQLITE_PATH', './data/db/pulseone.db'),
                timeout: this.getNumber('SQLITE_BUSY_TIMEOUT', 5000),
                journalMode: this.get('SQLITE_JOURNAL_MODE', 'WAL'),
                foreignKeys: this.getBoolean('SQLITE_FOREIGN_KEYS', true),
                cacheSize: this.getNumber('SQLITE_CACHE_SIZE', 2000)
            },

            // PostgreSQL 설정 (기존 변수명 유지)
            postgresql: {
                enabled: this.getBoolean('POSTGRESQL_ENABLED', false),
                // Primary
                primaryHost: this.get('POSTGRES_PRIMARY_HOST', 'postgres'),
                primaryPort: this.getNumber('POSTGRES_PRIMARY_PORT', 5432),
                primaryDb: this.get('POSTGRES_PRIMARY_DB', 'pulseone'),
                primaryUser: this.get('POSTGRES_PRIMARY_USER', 'postgres'),
                primaryPassword: this.get('POSTGRES_PRIMARY_PASSWORD', 'postgres123'),
                // 기존 호환성 (MAIN)
                host: this.get('POSTGRES_MAIN_DB_HOST', 'postgres'),
                port: this.getNumber('POSTGRES_MAIN_DB_PORT', 5432),
                database: this.get('POSTGRES_MAIN_DB_NAME', 'pulseone'),
                user: this.get('POSTGRES_MAIN_DB_USER', 'postgres'),
                password: this.get('POSTGRES_MAIN_DB_PASSWORD', 'postgres123')
            }
        };
    }

    /**
     * Redis 설정 조회 - 기존 환경변수 구조 완전 지원
     */
    getRedisConfig() {
        return {
            enabled: this.getBoolean('REDIS_PRIMARY_ENABLED', false),
            host: this.get('REDIS_PRIMARY_HOST', 'localhost'),
            port: this.getNumber('REDIS_PRIMARY_PORT', 6379),
            password: this.get('REDIS_PRIMARY_PASSWORD', ''),
            db: this.getNumber('REDIS_PRIMARY_DB', 0),
            timeout: this.getNumber('REDIS_PRIMARY_TIMEOUT_MS', 5000),
            connectTimeout: this.getNumber('REDIS_PRIMARY_CONNECT_TIMEOUT_MS', 3000),
            poolSize: this.getNumber('REDIS_POOL_SIZE', 5),
            keyPrefix: this.get('REDIS_KEY_PREFIX', 'pulseone:'),
            testMode: this.getBoolean('REDIS_TEST_MODE', false),
            // 기존 호환성
            mainHost: this.get('REDIS_MAIN_HOST', this.get('REDIS_PRIMARY_HOST', 'localhost')),
            mainPort: this.getNumber('REDIS_MAIN_PORT', this.getNumber('REDIS_PRIMARY_PORT', 6379)),
            mainPassword: this.get('REDIS_MAIN_PASSWORD', this.get('REDIS_PRIMARY_PASSWORD', ''))
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
            stage: this.get('ENV_STAGE', 'dev'),
            logLevel: this.get('LOG_LEVEL', 'info'),
            buildType: this.get('BUILD_TYPE', 'dev')
        };
    }

    /**
     * 디버깅용 정보 출력
     */
    printDebugInfo() {
        this.logger.log('\n📋 ConfigManager 환경변수 디버깅 정보:');
        this.logger.log(`   로딩된 파일들: ${this.loadedFiles.join(', ')}`);
        this.logger.log(`   NODE_ENV: ${this.get('NODE_ENV')}`);
        this.logger.log(`   DATABASE_TYPE: ${this.get('DATABASE_TYPE')}`);
        this.logger.log(`   SQLITE_PATH: ${this.get('SQLITE_PATH')}`);
        this.logger.log(`   REDIS_PRIMARY_HOST: ${this.get('REDIS_PRIMARY_HOST')}`);
        this.logger.log(`   REDIS_PRIMARY_ENABLED: ${this.get('REDIS_PRIMARY_ENABLED')}`);
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
     * 로드된 파일 목록 반환 (권장사항 적용)
     */
    getLoadedFiles() {
        return this.loadedFiles || [];
    }

    /**
     * ConfigManager 상태 조회 (권장사항 적용)
     */
    getConfigStatus() {
        return {
            loaded: this.loaded,
            loadedFiles: this.loadedFiles,
            totalVariables: this.env.size,
            lastInitialized: this.lastInitialized
        };
    }

    /**
     * 특정 키 패턴으로 환경변수 검색 (권장사항 적용)
     */
    getByPattern(pattern) {
        const result = {};
        const regex = new RegExp(pattern, 'i');
        
        this.env.forEach((value, key) => {
            if (regex.test(key)) {
                result[key] = value;
            }
        });
        
        return result;
    }

    /**
     * 환경변수 동적 설정 (런타임에서 사용) (권장사항 적용)
     */
    set(key, value) {
        this.env.set(key, value);
        process.env[key] = value;
        return this;
    }

    /**
     * 환경변수 존재 여부 확인 (권장사항 적용)
     */
    has(key) {
        return this.env.has(key) || process.env.hasOwnProperty(key);
    }

    /**
     * 개발자용 헬퍼 - 모든 DATABASE_ 관련 설정 조회 (권장사항 적용)
     */
    getDatabaseDebugInfo() {
        return this.getByPattern('^DATABASE_|^SQLITE_|^POSTGRES_');
    }

    /**
     * 개발자용 헬퍼 - 모든 REDIS_ 관련 설정 조회 (권장사항 적용)
     */
    getRedisDebugInfo() {
        return this.getByPattern('^REDIS_');
    }

    /**
     * 개발자용 헬퍼 - 모든 DEV_ 관련 설정 조회 (권장사항 적용)
     */
    getDevDebugInfo() {
        return this.getByPattern('^DEV_|^DEFAULT_');
    }

    /**
     * 설정 파일 다시 로드 (권장사항 적용)
     */
    reload() {
        this.logger.log('🔄 ConfigManager 설정 다시 로딩 중...');
        this.loaded = false;
        this.env.clear();
        this.loadedFiles = [];
        this.lastLoad.clear();
        this.initialize();
        return this;
    }

    /**
     * 환경변수를 JSON 형태로 내보내기 (민감한 정보 제외) (권장사항 적용)
     */
    exportSafeConfig() {
        const sensitiveKeys = ['PASSWORD', 'SECRET', 'TOKEN', 'KEY', 'PRIVATE'];
        const result = {};
        
        this.env.forEach((value, key) => {
            const isSensitive = sensitiveKeys.some(sensitive => 
                key.toUpperCase().includes(sensitive)
            );
            
            if (!isSensitive) {
                result[key] = value;
            } else {
                result[key] = '***HIDDEN***';
            }
        });
        
        return result;
    }

    /**
     * 환경변수 유효성 검증 (권장사항 적용)
     */
    validate() {
        const issues = [];
        
        // 필수 환경변수 검증
        const required = ['NODE_ENV', 'DATABASE_TYPE'];
        required.forEach(key => {
            if (!this.has(key)) {
                issues.push(`필수 환경변수 누락: ${key}`);
            }
        });
        
        // 포트 번호 유효성 검증
        const port = this.getNumber('BACKEND_PORT');
        if (port < 1 || port > 65535) {
            issues.push(`잘못된 포트 번호: ${port}`);
        }
        
        // 데이터베이스 타입 검증
        const dbType = this.get('DATABASE_TYPE');
        if (!['SQLITE', 'POSTGRESQL'].includes(dbType)) {
            issues.push(`지원하지 않는 데이터베이스 타입: ${dbType}`);
        }
        
        return {
            isValid: issues.length === 0,
            issues: issues
        };
    }
}

// 싱글톤 인스턴스 생성 및 내보내기
const configManager = ConfigManager.getInstance();

// 기존 방식 호환성을 위한 직접 export
module.exports = {
    // ConfigManager 인스턴스
    getInstance: () => configManager,

    // 기존 방식 호환성 (redis.js에서 사용하는 이름들)
    REDIS_MAIN_HOST: configManager.get('REDIS_MAIN_HOST', configManager.get('REDIS_PRIMARY_HOST', 'localhost')),
    REDIS_MAIN_PORT: configManager.get('REDIS_MAIN_PORT', configManager.get('REDIS_PRIMARY_PORT', '6379')),
    REDIS_MAIN_PASSWORD: configManager.get('REDIS_MAIN_PASSWORD', configManager.get('REDIS_PRIMARY_PASSWORD', '')),

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
    
    // 권장사항 적용 - 새로운 메서드들
    getLoadedFiles: () => configManager.getLoadedFiles(),
    getConfigStatus: () => configManager.getConfigStatus(),
    getByPattern: (pattern) => configManager.getByPattern(pattern),
    set: (key, value) => configManager.set(key, value),
    has: (key) => configManager.has(key),
    getDatabaseDebugInfo: () => configManager.getDatabaseDebugInfo(),
    getRedisDebugInfo: () => configManager.getRedisDebugInfo(),
    getDevDebugInfo: () => configManager.getDevDebugInfo(),
    reload: () => configManager.reload(),
    exportSafeConfig: () => configManager.exportSafeConfig(),
    validate: () => configManager.validate()
};