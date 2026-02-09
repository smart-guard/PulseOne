// backend/lib/config/ConfigManager.js - 완성본 (Windows .env.production 완전 지원)
const path = require('path');
const fs = require('fs');
const os = require('os');

class ConfigManager {
    static instance = null;

    constructor() {
        this.env = new Map();
        this.loaded = false;
        this.loadedFiles = [];
        this.lastLoad = new Map();
        this.logger = console;
        this.lastInitialized = null;

        // 플랫폼 감지
        this.platform = this.detectPlatform();

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
     * 플랫폼 및 환경 자동 감지
     */
    detectPlatform() {
        const platform = os.platform();
        const cwd = process.cwd();

        // Docker 환경 감지
        const isDocker = fs.existsSync('/.dockerenv') ||
            process.env.DOCKER_CONTAINER === 'true' ||
            cwd.startsWith('/app') ||
            cwd.includes('/app/');

        // Windows 감지
        const isWindows = platform === 'win32';

        // Linux (non-Docker) 감지
        const isLinux = platform === 'linux' && !isDocker;

        const platformInfo = {
            type: platform,
            isWindows,
            isLinux,
            isDocker,
            isDevelopment: process.env.NODE_ENV === 'development',
            cwd: cwd
        };

        this.logger.log('🖥️ 플랫폼 감지:', {
            type: platformInfo.type,
            isDocker: platformInfo.isDocker,
            isWindows: platformInfo.isWindows,
            cwd: platformInfo.cwd
        });

        return platformInfo;
    }

    /**
     * 환경변수 초기화 - Windows .env.production 완전 지원
     */
    initialize() {
        if (this.loaded) return this;

        this.logger.log('🔧 ConfigManager 환경변수 로딩 시작...');

        try {
            // 현재 작업 디렉토리 확인
            const cwd = process.cwd();
            this.logger.log(`📁 현재 작업 디렉토리: ${cwd}`);

            // NODE_ENV 확인 및 설정
            let nodeEnv = process.env.NODE_ENV;

            // Windows에서 NODE_ENV가 없으면 production으로 가정
            if (!nodeEnv && this.platform.isWindows) {
                nodeEnv = 'production';
                process.env.NODE_ENV = 'production';
                this.logger.log('🪟 Windows: NODE_ENV가 없어서 production으로 설정');
            } else if (!nodeEnv) {
                nodeEnv = 'development';
                process.env.NODE_ENV = 'development';
            }

            this.logger.log(`🎯 NODE_ENV: ${nodeEnv}`);

            // Windows에서 강제로 .env.production 우선 검색
            let envFiles = [];
            if (this.platform.isWindows && nodeEnv === 'production') {
                envFiles = [
                    '.env.production',      // Windows production 우선
                    `.env.${nodeEnv}`,      // 일반 환경별
                    '.env.local',           // 로컬 오버라이드
                    '.env'                  // 기본 파일
                ];
                this.logger.log('🪟 Windows production 모드: .env.production 우선 탐색');
            } else {
                envFiles = [
                    `.env.${nodeEnv}`,      // .env.production, .env.development
                    '.env.local',           // 로컬 오버라이드
                    '.env'                  // 기본 파일
                ];
            }

            // 메인 .env 파일 로드 시도
            let envLoaded = false;
            let loadedFile = null;

            for (const envFile of envFiles) {
                const envPath = path.join(cwd, envFile);
                this.logger.log(`🔍 환경 파일 탐색: ${envPath}`);

                // Windows에서 파일 존재 확인 강화
                if (this.platform.isWindows) {
                    try {
                        const stats = fs.statSync(envPath);
                        if (stats.isFile()) {
                            this.logger.log(`✅ Windows: 파일 확인됨 ${envFile} (크기: ${stats.size} bytes)`);
                        }
                    } catch (err) {
                        this.logger.log(`❌ Windows: 파일 없음 ${envFile} - ${err.code}`);
                        continue;
                    }
                }

                if (this.loadEnvFile(envPath, false)) {
                    envLoaded = true;
                    loadedFile = envFile;
                    this.logger.log(`✅ 환경 파일 로드 성공: ${envFile}`);
                    break; // 첫 번째로 찾은 파일만 로드 (우선순위)
                }
            }

            // Windows 특화 fallback 경로들
            if (!envLoaded && this.platform.isWindows) {
                const windowsFallbackPaths = [
                    // 1순위: backend 폴더에서 실행 시 상위 config 폴더 확인
                    path.resolve(cwd, '..', 'config', '.env.production'),
                    path.resolve(cwd, '..', 'config', '.env'),
                    // 2순위: 상위 폴더 직접 확인
                    path.resolve(cwd, '..', '.env.production'),
                    path.resolve(cwd, '..', '.env'),
                    // 3순위: 현재 폴더의 config 하위 폴더
                    path.join(cwd, 'config', '.env.production'),
                    path.join(cwd, 'config', '.env'),
                    // 4순위: 형제 config 폴더 (같은 레벨)
                    path.resolve(cwd, '..', '..', 'config', '.env.production'),
                    path.resolve(cwd, '..', '..', 'config', '.env'),
                    // 5순위: 시스템 경로들
                    path.join(process.env.USERPROFILE || 'C:\\', 'PulseOne', 'config', '.env.production'),
                    path.join(process.env.USERPROFILE || 'C:\\', 'PulseOne', '.env.production'),
                    'C:\\PulseOne\\config\\.env.production',
                    'C:\\PulseOne\\.env.production'
                ];

                this.logger.log('🪟 Windows fallback 경로 검색 시작...');
                for (const envPath of windowsFallbackPaths) {
                    this.logger.log(`🔍 Windows fallback: ${envPath}`);
                    if (this.loadEnvFile(envPath, false)) {
                        envLoaded = true;
                        loadedFile = path.basename(envPath);
                        this.logger.log(`✅ Windows fallback 로드 성공: ${envPath}`);
                        break;
                    }
                }
            }

            // 일반 fallback 경로들 (모든 플랫폼)
            if (!envLoaded) {
                const fallbackPaths = [
                    path.join(cwd, 'config', '.env'),
                    path.join(__dirname, '../../../.env'),
                    path.join(__dirname, '../../../config/.env'),
                    path.join(__dirname, '../../.env'),
                    path.join(__dirname, '../config/.env')
                ];

                this.logger.log('🔍 일반 fallback 경로 검색 시작...');
                for (const envPath of fallbackPaths) {
                    this.logger.log(`🔍 fallback .env 파일 탐색: ${envPath}`);
                    if (this.loadEnvFile(envPath, false)) {
                        envLoaded = true;
                        loadedFile = path.basename(envPath);
                        this.logger.log(`✅ fallback .env 파일 로드 성공: ${envPath}`);
                        break;
                    }
                }
            }

            if (!envLoaded) {
                this.logger.warn('⚠️ .env 파일을 찾을 수 없습니다.');
                this.logger.warn(`⚠️ 탐색한 파일들: ${envFiles.join(', ')}`);
                this.logger.warn(`⚠️ 현재 디렉토리: ${cwd}`);
                this.logger.warn(`⚠️ 플랫폼: ${this.platform.type} (Windows: ${this.platform.isWindows})`);
            } else {
                this.logger.log(`🎉 메인 환경 파일 로드 완료: ${loadedFile}`);
            }

            // CONFIG_FILES 기반 추가 파일 로드
            const configFiles = this.get('CONFIG_FILES') || 'database.env,redis.env,timeseries.env,messaging.env';
            const configDirs = [
                path.join(cwd, 'config'),
                path.join(__dirname, '../../../config'),
                path.join(__dirname, '../../config')
            ];

            if (configFiles) {
                const files = configFiles.split(',').map(f => f.trim());

                files.forEach(file => {
                    let fileLoaded = false;
                    for (const configDir of configDirs) {
                        const fullPath = path.join(configDir, file);
                        if (this.loadEnvFile(fullPath, false)) {
                            fileLoaded = true;
                            break;
                        }
                    }
                    if (!fileLoaded) {
                        this.logger.warn(`⚠️ 설정 파일 없음: ${file}`);
                    }
                });
            }

            // process.env의 모든 변수 복사 (환경변수가 파일 설정보다 우선)
            Object.entries(process.env).forEach(([key, value]) => {
                if (value !== undefined) {
                    this.env.set(key, value);
                }
            });

            this.loaded = true;
            this.lastInitialized = new Date().toISOString();
            this.logger.log(`✅ 환경변수 로딩 완료 (${this.loadedFiles.length}개 파일)`);
            this.logger.log(`🎯 최종 환경: ${this.get('NODE_ENV')}`);

            // 디버깅 정보 출력
            this.printDebugInfo();

        } catch (error) {
            this.logger.error('❌ ConfigManager 초기화 실패:', error.message);
        }

        return this;
    }

    /**
     * .env 파일 로드 (Windows 특화 처리)
     */
    loadEnvFile(filePath, required = false) {
        try {
            const absolutePath = path.resolve(filePath);

            if (!fs.existsSync(absolutePath)) {
                if (required) {
                    throw new Error(`필수 환경변수 파일 없음: ${filePath}`);
                }
                return false;
            }

            // Windows에서 파일 읽기 권한 확인
            if (this.platform.isWindows) {
                try {
                    fs.accessSync(absolutePath, fs.constants.R_OK);
                } catch (accessError) {
                    this.logger.warn(`⚠️ Windows: 파일 읽기 권한 없음: ${path.basename(filePath)}`);
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

                // 환경변수 설정 (파일의 최신값으로 업데이트)
                this.env.set(key, cleanValue);
                process.env[key] = cleanValue;
                loadedCount++;
            });

            this.loadedFiles.push(path.basename(filePath));
            this.lastLoad.set(filePath, new Date());

            this.logger.log(`✅ 로드 성공: ${path.basename(filePath)} (${loadedCount}개 변수)`);
            return true;

        } catch (error) {
            if (required) {
                throw error;
            } else {
                this.logger.warn(`⚠️ 환경변수 파일 로드 실패: ${path.basename(filePath)} - ${error.message}`);
                return false;
            }
        }
    }

    /**
     * 플랫폼별 경로 자동 변환
     * Docker, Windows, Linux 모두 자동 처리
     */
    getSmartPath(configKey, defaultPath) {
        const rawPath = this.get(configKey, defaultPath);

        // Docker 환경
        if (this.platform.isDocker) {
            // 상대 경로를 Docker 절대 경로로 변환
            if (rawPath.startsWith('./')) {
                return '/app/' + rawPath.substring(2);
            }
            // 이미 /app 경로면 그대로 사용
            if (rawPath.startsWith('/app/')) {
                return rawPath;
            }
            // 다른 절대 경로도 그대로 사용
            if (path.isAbsolute(rawPath)) {
                return rawPath;
            }
            // 나머지는 /app 기준으로 처리
            return path.join('/app', rawPath);
        }

        // Windows/Linux (non-Docker)
        // 상대 경로 그대로 사용
        if (rawPath.startsWith('./') || rawPath.startsWith('../')) {
            return path.resolve(process.cwd(), rawPath);
        }

        // Unix 스타일 절대 경로를 상대 경로로 변환 (Windows용)
        if (this.platform.isWindows && rawPath.startsWith('/')) {
            const relativePath = rawPath.replace(/^\/app\//, './');
            return path.resolve(process.cwd(), relativePath);
        }

        // 절대 경로는 그대로 사용
        if (path.isAbsolute(rawPath)) {
            return rawPath;
        }

        // 나머지는 현재 디렉토리 기준
        return path.resolve(process.cwd(), rawPath);
    }

    /**
     * 환경변수 조회
     */
    get(key, defaultValue = undefined) {
        if (this.env.has(key)) {
            return this.env.get(key);
        }

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
     * 데이터베이스 설정 조회 - 모든 DB 타입 지원
     */
    getDatabaseConfig() {
        // SQLite 경로를 플랫폼별로 자동 처리
        // Default to absolute path based on process.cwd() (project root in non-docker, /app/backend in Docker)
        // In Docker: /app/backend -> ../data -> /app/data
        const defaultPath = path.resolve(process.cwd(), '../data/db/pulseone.db');

        const sqlitePath = this.getSmartPath('SQLITE_PATH',
            this.get('SQLITE_DB_PATH',
                this.get('DB_PATH', defaultPath)
            )
        );
        const backupPath = this.getSmartPath('SQLITE_BACKUP_PATH', './data/backup');
        const logsPath = this.getSmartPath('SQLITE_LOGS_PATH', './data/logs');
        const tempPath = this.getSmartPath('SQLITE_TEMP_PATH', './data/temp');

        return {
            type: this.get('DATABASE_TYPE', 'SQLITE'),

            // SQLite 설정
            sqlite: {
                enabled: this.getBoolean('SQLITE_ENABLED', true),
                path: sqlitePath,
                backupPath: backupPath,
                logsPath: logsPath,
                tempPath: tempPath,
                timeout: this.getNumber('SQLITE_BUSY_TIMEOUT', 5000),
                journalMode: this.get('SQLITE_JOURNAL_MODE', 'WAL'),
                foreignKeys: this.getBoolean('SQLITE_FOREIGN_KEYS', true),
                cacheSize: this.getNumber('SQLITE_CACHE_SIZE', 2000)
            },

            // PostgreSQL 설정
            postgresql: {
                enabled: this.getBoolean('POSTGRES_ENABLED', false),
                host: this.get('POSTGRES_HOST', 'localhost'),
                port: this.getNumber('POSTGRES_PORT', 5432),
                database: this.get('POSTGRES_DATABASE', 'pulseone'),
                user: this.get('POSTGRES_USER', 'postgres'),
                password: this.get('POSTGRES_PASSWORD', 'postgres123'),
                poolSize: this.getNumber('POSTGRES_POOL_SIZE', 10),
                ssl: this.getBoolean('POSTGRES_SSL', false)
            },

            // MariaDB/MySQL 설정
            mariadb: {
                enabled: this.getBoolean('MARIADB_ENABLED', false),
                host: this.get('MARIADB_HOST', 'localhost'),
                port: this.getNumber('MARIADB_PORT', 3306),
                database: this.get('MARIADB_DATABASE', 'pulseone'),
                user: this.get('MARIADB_USER', 'root'),
                password: this.get('MARIADB_PASSWORD', 'mariadb123'),
                poolSize: this.getNumber('MARIADB_POOL_SIZE', 10),
                connectionLimit: this.getNumber('MARIADB_CONNECTION_LIMIT', 100),
                charset: this.get('MARIADB_CHARSET', 'utf8mb4')
            },

            // Microsoft SQL Server 설정
            mssql: {
                enabled: this.getBoolean('MSSQL_ENABLED', false),
                host: this.get('MSSQL_HOST', 'localhost'),
                port: this.getNumber('MSSQL_PORT', 1433),
                database: this.get('MSSQL_DATABASE', 'pulseone'),
                user: this.get('MSSQL_USER', 'sa'),
                password: this.get('MSSQL_PASSWORD', 'MsSql123!'),
                instance: this.get('MSSQL_INSTANCE', ''),
                encrypt: this.getBoolean('MSSQL_ENCRYPT', false),
                trustServerCertificate: this.getBoolean('MSSQL_TRUST_SERVER_CERTIFICATE', true),
                poolSize: this.getNumber('MSSQL_POOL_SIZE', 10)
            },

            // 활성화된 데이터베이스 감지
            activeDatabase: this.getActiveDatabase()
        };
    }

    /**
     * 활성화된 데이터베이스 확인
     */
    getActiveDatabase() {
        const dbType = this.get('DATABASE_TYPE', 'SQLITE').toUpperCase();

        // 타입별 확인
        switch (dbType) {
            case 'POSTGRESQL':
            case 'POSTGRES':
            case 'PG':
                return 'postgresql';
            case 'MARIADB':
            case 'MYSQL':
                return 'mariadb';
            case 'MSSQL':
            case 'SQLSERVER':
                return 'mssql';
            case 'SQLITE':
            default:
                return 'sqlite';
        }
    }

    /**
     * Collector 설정 조회 - 플랫폼별 자동 처리
     */
    getCollectorConfig() {
        let collectorPath = this.get('COLLECTOR_EXECUTABLE_PATH', '');

        // 경로가 없으면 플랫폼별 기본값 사용
        if (!collectorPath) {
            if (this.platform.isWindows) {
                // Windows 기본 경로들
                const windowsPaths = [
                    path.join(process.cwd(), 'collector', 'bin', 'collector.exe'),
                    path.join(process.cwd(), 'collector', 'build', 'Release', 'collector.exe'),
                    path.resolve(process.cwd(), '..', 'collector.exe'),
                    'C:\\PulseOne\\collector.exe'
                ];

                for (const p of windowsPaths) {
                    if (fs.existsSync(p)) {
                        collectorPath = p;
                        break;
                    }
                }
            } else if (this.platform.isDocker) {
                // Docker 환경
                collectorPath = '/app/collector/bin/collector';
            } else {
                // Linux/macOS 기본 경로들
                const unixPaths = [
                    path.join(process.cwd(), 'collector', 'bin', 'collector'),
                    '/opt/pulseone/collector',
                    '/usr/local/bin/pulseone-collector'
                ];

                for (const p of unixPaths) {
                    if (fs.existsSync(p)) {
                        collectorPath = p;
                        break;
                    }
                }
            }
        }

        return {
            executable: collectorPath,
            workingDir: collectorPath ? path.dirname(collectorPath) : process.cwd(),
            host: this.get('COLLECTOR_HOST', 'localhost'),
            port: this.getNumber('COLLECTOR_PORT', 8001),
            healthCheckInterval: this.getNumber('COLLECTOR_HEALTH_CHECK_INTERVAL_MS', 30000),
            startTimeout: this.getNumber('COLLECTOR_START_TIMEOUT_MS', 10000),
            stopTimeout: this.getNumber('COLLECTOR_STOP_TIMEOUT_MS', 5000),
            autoRestart: this.getBoolean('COLLECTOR_AUTO_RESTART', true),
            maxRestartAttempts: this.getNumber('COLLECTOR_MAX_RESTART_ATTEMPTS', 3),
            restartDelay: this.getNumber('COLLECTOR_RESTART_DELAY_MS', 5000),
            platform: this.platform
        };
    }

    /**
     * Redis 설정 조회
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
     * 플랫폼 정보 반환
     */
    getPlatformInfo() {
        return this.platform;
    }

    /**
     * 디버깅용 정보 출력 (Windows 특화)
     */
    printDebugInfo() {
        this.logger.log('\n📋 ConfigManager 환경변수 디버깅 정보:');
        this.logger.log(`   플랫폼: ${this.platform.type}`);
        this.logger.log(`   Docker: ${this.platform.isDocker}`);
        this.logger.log(`   Windows: ${this.platform.isWindows}`);
        this.logger.log(`   로딩된 파일들: ${this.loadedFiles.join(', ')}`);
        this.logger.log(`   NODE_ENV: ${this.get('NODE_ENV')}`);
        this.logger.log(`   POSTGRES_HOST: ${this.get('POSTGRES_HOST')}`);
        this.logger.log(`   COLLECTOR_HOST: ${this.get('COLLECTOR_HOST')}`);
        this.logger.log(`   DATABASE_TYPE: ${this.get('DATABASE_TYPE')}`);
        this.logger.log(`   SQLITE_PATH (원본): ${this.get('SQLITE_PATH')}`);
        this.logger.log(`   SQLITE_PATH (변환): ${this.getSmartPath('SQLITE_PATH', './data/db/pulseone.db')}`);
        this.logger.log(`   COLLECTOR_PATH: ${this.getCollectorConfig().executable}`);

        // Windows 특화 디버깅
        if (this.platform.isWindows) {
            this.logger.log('\n🪟 Windows 특화 정보:');
            this.logger.log(`   작업 디렉토리: ${process.cwd()}`);
            this.logger.log(`   실행 파일 경로: ${process.execPath}`);
            this.logger.log(`   USERPROFILE: ${process.env.USERPROFILE}`);
            this.logger.log(`   환경 파일 탐색 결과: ${this.loadedFiles.length > 0 ? '성공' : '실패'}`);
        }
        this.logger.log('');
    }

    /**
     * Windows 환경에서 .env 파일 탐지 디버깅
     */
    debugWindowsEnvFiles() {
        if (!this.platform.isWindows) {
            this.logger.log('❌ Windows 환경이 아닙니다.');
            return;
        }

        this.logger.log('🪟 Windows .env 파일 디버깅 시작...');

        const cwd = process.cwd();
        const nodeEnv = process.env.NODE_ENV || 'development';

        // 체크할 파일들
        const envFiles = [
            '.env.production',
            '.env.development',
            '.env.local',
            '.env'
        ];

        // 체크할 경로들
        const searchPaths = [
            cwd,
            path.join(cwd, 'config'),
            path.join(cwd, 'backend'),
            path.join(cwd, 'backend', 'config'),
            path.resolve(cwd, '..'),
            path.resolve(cwd, '..', 'config'),
            'C:\\PulseOne',
            'C:\\PulseOne\\config'
        ];

        this.logger.log(`📍 NODE_ENV: ${nodeEnv}`);
        this.logger.log(`📁 현재 디렉토리: ${cwd}`);
        this.logger.log('');

        // 각 경로에서 각 파일 확인
        for (const searchPath of searchPaths) {
            this.logger.log(`📂 경로 확인: ${searchPath}`);

            try {
                if (!fs.existsSync(searchPath)) {
                    this.logger.log('   ❌ 경로 존재하지 않음');
                    continue;
                }

                for (const envFile of envFiles) {
                    const fullPath = path.join(searchPath, envFile);

                    if (fs.existsSync(fullPath)) {
                        const stats = fs.statSync(fullPath);
                        this.logger.log(`   ✅ ${envFile} (${stats.size} bytes)`);
                    } else {
                        this.logger.log(`   ❌ ${envFile}`);
                    }
                }

            } catch (error) {
                this.logger.log(`   ⚠️ 오류: ${error.message}`);
            }

            this.logger.log('');
        }
    }

    // 기존 모든 메서드들... (나머지는 동일하므로 생략)
    getAll() {
        const result = {};
        this.env.forEach((value, key) => {
            result[key] = value;
        });
        return result;
    }

    getLoadedFiles() {
        return this.loadedFiles || [];
    }

    getConfigStatus() {
        return {
            loaded: this.loaded,
            loadedFiles: this.loadedFiles,
            totalVariables: this.env.size,
            lastInitialized: this.lastInitialized,
            platform: this.platform
        };
    }

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

    set(key, value) {
        this.env.set(key, value);
        process.env[key] = value;
        return this;
    }

    has(key) {
        return this.env.has(key) || process.env.hasOwnProperty(key);
    }

    require(key) {
        const value = this.get(key);
        if (value === undefined || value === null || value === '') {
            throw new Error(`필수 환경변수가 설정되지 않음: ${key}`);
        }
        return value;
    }

    getDatabaseDebugInfo() {
        return this.getByPattern('^DATABASE_|^SQLITE_|^POSTGRES_');
    }

    getRedisDebugInfo() {
        return this.getByPattern('^REDIS_');
    }

    getCollectorDebugInfo() {
        return this.getByPattern('^COLLECTOR_');
    }

    reload() {
        this.logger.log('🔄 ConfigManager 설정 다시 로딩 중...');
        this.loaded = false;
        this.env.clear();
        this.loadedFiles = [];
        this.lastLoad.clear();
        this.platform = this.detectPlatform();
        this.initialize();
        return this;
    }

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

        result._platform = this.platform;
        return result;
    }

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
        if (!['SQLITE', 'POSTGRESQL', 'MARIADB', 'MSSQL'].includes(dbType)) {
            issues.push(`지원하지 않는 데이터베이스 타입: ${dbType}`);
        }

        // SQLite 경로 확인
        if (dbType === 'SQLITE') {
            const dbConfig = this.getDatabaseConfig();
            const dbDir = path.dirname(dbConfig.sqlite.path);
            if (!fs.existsSync(dbDir)) {
                issues.push(`SQLite 데이터베이스 디렉토리가 없음: ${dbDir}`);
            }
        }

        // Collector 경로 확인
        const collectorConfig = this.getCollectorConfig();
        if (collectorConfig.executable && !fs.existsSync(collectorConfig.executable)) {
            issues.push(`Collector 실행 파일이 없음: ${collectorConfig.executable}`);
        }

        return {
            isValid: issues.length === 0,
            issues: issues,
            platform: this.platform
        };
    }
}

// 싱글톤 인스턴스 생성 및 내보내기
const configManager = ConfigManager.getInstance();

// 기존 방식 호환성을 위한 직접 export
module.exports = {
    // ConfigManager 인스턴스
    getInstance: () => configManager,

    // 기존 방식 호환성
    REDIS_MAIN_HOST: configManager.get('REDIS_MAIN_HOST', configManager.get('REDIS_PRIMARY_HOST', 'localhost')),
    REDIS_MAIN_PORT: configManager.get('REDIS_MAIN_PORT', configManager.get('REDIS_PRIMARY_PORT', '6379')),
    REDIS_MAIN_PASSWORD: configManager.get('REDIS_MAIN_PASSWORD', configManager.get('REDIS_PRIMARY_PASSWORD', '')),

    // 자주 사용되는 환경변수들
    NODE_ENV: configManager.get('NODE_ENV', 'development'),
    LOG_LEVEL: configManager.get('LOG_LEVEL', 'info'),
    BACKEND_PORT: configManager.getNumber('BACKEND_PORT', 3000),
    DATABASE_TYPE: configManager.get('DATABASE_TYPE', 'SQLITE'),

    // 헬퍼 함수들
    get: (key, defaultValue) => configManager.get(key, defaultValue),
    getBoolean: (key, defaultValue) => configManager.getBoolean(key, defaultValue),
    getNumber: (key, defaultValue) => configManager.getNumber(key, defaultValue),
    getSmartPath: (key, defaultValue) => configManager.getSmartPath(key, defaultValue),
    require: (key) => configManager.require(key),

    // 설정 그룹들
    getDatabaseConfig: () => configManager.getDatabaseConfig(),
    getRedisConfig: () => configManager.getRedisConfig(),
    getServerConfig: () => configManager.getServerConfig(),
    getCollectorConfig: () => configManager.getCollectorConfig(),
    getPlatformInfo: () => configManager.getPlatformInfo(),

    // 추가 메서드들
    getLoadedFiles: () => configManager.getLoadedFiles(),
    getConfigStatus: () => configManager.getConfigStatus(),
    getByPattern: (pattern) => configManager.getByPattern(pattern),
    set: (key, value) => configManager.set(key, value),
    has: (key) => configManager.has(key),
    getDatabaseDebugInfo: () => configManager.getDatabaseDebugInfo(),
    getRedisDebugInfo: () => configManager.getRedisDebugInfo(),
    getCollectorDebugInfo: () => configManager.getCollectorDebugInfo(),
    reload: () => configManager.reload(),
    exportSafeConfig: () => configManager.exportSafeConfig(),
    validate: () => configManager.validate(),

    // Windows 디버깅 전용
    debugWindowsEnvFiles: () => configManager.debugWindowsEnvFiles()
};