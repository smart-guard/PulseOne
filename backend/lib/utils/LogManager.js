// backend/lib/utils/LogManager.js - .env 설정 완전 활용 + 날짜별 구조화
const fs = require('fs');
const path = require('path');
const util = require('util');

// ConfigManager 연동
const config = require('../config/ConfigManager');

class LogManager {
    static instance = null;

    constructor() {
        this.initialized = false;
        this.logStreams = new Map();
        this.currentDate = null;
        this.logLevels = {
            DEBUG: 0,
            INFO: 1,
            WARN: 2,
            ERROR: 3,
            FATAL: 4
        };

        // .env 설정 완전 활용
        this.currentLevel = this.parseLogLevel(config.get('LOG_LEVEL', 'info'));
        this.logToConsole = config.getBoolean('LOG_TO_CONSOLE', true);
        this.logToFile = config.getBoolean('LOG_TO_FILE', true);
        this.logBaseDir = config.getSmartPath('LOG_FILE_PATH', './data/logs/');
        this.maxFileSizeMB = config.getNumber('LOG_MAX_SIZE_MB', 100);
        this.maxFiles = config.getNumber('LOG_MAX_FILES', 30);

        // 추가 로그 구조화 설정 (.env에 추가 가능)
        this.useDateFolders = config.getBoolean('LOG_USE_DATE_FOLDERS', true);
        this.useHourlyFiles = config.getBoolean('LOG_USE_HOURLY_FILES', false);
        this.retentionDays = config.getNumber('LOG_RETENTION_DAYS', 30);

        // 로그 카테고리별 파일 설정
        this.logCategories = {
            application: 'application',
            error: 'error',
            crossplatform: 'crossplatform',
            database: 'database',
            services: 'services',
            api: 'api',
            system: 'system'
        };

        this.cleanupTimer = null;
        this.cleanupInterval = null;

        this.initialize();
    }

    static getInstance() {
        if (!LogManager.instance) {
            LogManager.instance = new LogManager();
        }
        return LogManager.instance;
    }

    /**
     * 로그 매니저 초기화
     */
    initialize() {
        if (this.initialized) return;

        try {
            // 현재 날짜/시간 설정
            this.updateCurrentDate();

            // 기본 디렉토리 생성
            this.ensureLogDirectory();

            // 로그 스트림 초기화
            if (this.logToFile) {
                this.initializeLogStreams();
            }

            // 정리 작업 스케줄링
            this.scheduleCleanupTasks();

            this.initialized = true;
            this.log('system', 'INFO', 'LogManager 초기화 완료', {
                logLevel: this.getCurrentLevelName(),
                logToConsole: this.logToConsole,
                logToFile: this.logToFile,
                baseDirectory: this.logBaseDir,
                maxFileSizeMB: this.maxFileSizeMB,
                maxFiles: this.maxFiles,
                useDateFolders: this.useDateFolders,
                retentionDays: this.retentionDays
            });

        } catch (error) {
            console.error('LogManager 초기화 실패:', error);
            this.logToFile = false;
        }
    }

    /**
     * 로그 디렉토리 구조 생성
     * 구조: logs/YYYY-MM-DD/category_YYYY-MM-DD_HH.log
     */
    ensureLogDirectory() {
        // 기본 로그 디렉토리 생성
        if (!fs.existsSync(this.logBaseDir)) {
            fs.mkdirSync(this.logBaseDir, { recursive: true });
        }

        // 날짜별 폴더 생성
        if (this.useDateFolders) {
            const dateFolder = this.getDateFolder();
            if (!fs.existsSync(dateFolder)) {
                fs.mkdirSync(dateFolder, { recursive: true });
            }
        }
    }

    /**
     * 현재 날짜 업데이트
     */
    updateCurrentDate() {
        const now = new Date();
        const newDate = this.formatDateString(now);

        if (this.currentDate !== newDate) {
            const oldDate = this.currentDate;
            this.currentDate = newDate;

            // 날짜가 변경되면 새 폴더 생성 및 스트림 재설정
            if (oldDate && this.logToFile) {
                this.log('system', 'INFO', `날짜 변경: ${oldDate} -> ${newDate}, 로그 파일 재구성`);
                this.closeLogStreams();
                this.ensureLogDirectory();
                this.initializeLogStreams();
            }
        }
    }

    /**
     * 날짜 문자열 포맷
     */
    formatDateString(date) {
        const year = date.getFullYear();
        const month = String(date.getMonth() + 1).padStart(2, '0');
        const day = String(date.getDate()).padStart(2, '0');
        return `${year}-${month}-${day}`;
    }

    /**
     * KST 타임스탬프 생성 (UTC+9)
     * 포맷: YYYY-MM-DD HH:mm:ss.SSS
     */
    getKSTTimestamp() {
        const now = new Date();
        const kstOffset = 9 * 60 * 60 * 1000; // 9 hours
        const kstDate = new Date(now.getTime() + kstOffset);

        // toISOString() returns YYYY-MM-DDTHH:mm:ss.sssZ
        // We want YYYY-MM-DD HH:mm:ss.SSS (no T, no Z)
        return kstDate.toISOString().replace('T', ' ').replace('Z', '');
    }


    /**
     * 시간 문자열 포맷 (시간별 파일용)
     */
    formatTimeString(date) {
        const hour = String(date.getHours()).padStart(2, '0');
        return hour;
    }

    /**
     * 날짜별 폴더 경로 반환
     */
    getDateFolder() {
        if (this.useDateFolders) {
            return path.join(this.logBaseDir, this.currentDate);
        }
        return this.logBaseDir;
    }

    /**
     * 로그 파일 경로 생성
     */
    getLogFilePath(category) {
        const dateFolder = this.getDateFolder();
        const now = new Date();

        let filename;
        if (this.useHourlyFiles) {
            // 시간별 파일: api_2024-01-15_14.log
            const hour = this.formatTimeString(now);
            filename = `${category}_${this.currentDate}_${hour}.log`;
        } else {
            // 일별 파일: api_2024-01-15.log
            filename = `${category}_${this.currentDate}.log`;
        }

        return path.join(dateFolder, filename);
    }

    /**
     * 로그 스트림 초기화
     */
    initializeLogStreams() {
        Object.entries(this.logCategories).forEach(([category, filename]) => {
            try {
                const filePath = this.getLogFilePath(filename);

                // 디렉토리가 없으면 생성
                const dir = path.dirname(filePath);
                if (!fs.existsSync(dir)) {
                    fs.mkdirSync(dir, { recursive: true });
                }

                const stream = fs.createWriteStream(filePath, {
                    flags: 'a',
                    encoding: 'utf8'
                });

                stream.on('error', (error) => {
                    console.error(`로그 스트림 오류 (${category}):`, error);
                });

                this.logStreams.set(category, {
                    stream: stream,
                    filePath: filePath,
                    category: category
                });

            } catch (error) {
                console.error(`로그 스트림 생성 실패 (${category}):`, error);
            }
        });
    }

    /**
     * 로그 스트림 종료
     */
    closeLogStreams() {
        this.logStreams.forEach((streamInfo, category) => {
            try {
                streamInfo.stream.end();
            } catch (error) {
                console.error(`로그 스트림 종료 실패 (${category}):`, error);
            }
        });
        this.logStreams.clear();
    }

    /**
     * 로그 경로 해결
     */
    resolveLogPath(logPath) {
        if (logPath === null || logPath === undefined) {
            console.error('❌ resolveLogPath: logPath is null or undefined!');
            // binary 실행 환경인지 확인 (pkg 등)
            const isBinary = process.pkg !== undefined || path.basename(process.execPath).includes('node') === false;
            const baseDir = isBinary ? path.dirname(process.execPath) : process.cwd();
            return path.resolve(baseDir, './logs');
        }

        if (path.isAbsolute(logPath)) {
            return logPath;
        }

        // binary 실행 환경인지 확인 (pkg 등)
        const isBinary = process.pkg !== undefined || path.basename(process.execPath).includes('node') === false;
        const baseDir = isBinary ? path.dirname(process.execPath) : process.cwd();

        if (logPath === null || logPath === undefined) {
            console.error('❌ resolveLogPath: logPath is null or undefined!');
            return path.resolve(isBinary ? path.dirname(process.execPath) : process.cwd(), './logs');
        }

        try {
            return path.resolve(baseDir, logPath);
        } catch (err) {
            console.error(`❌ path.resolve error: baseDir=${baseDir}, logPath=${logPath}`, err);
            return path.resolve(baseDir, './logs');
        }
    }

    /**
     * 로그 레벨 파싱
     */
    parseLogLevel(level) {
        const upperLevel = level.toString().toUpperCase();
        return this.logLevels[upperLevel] !== undefined ?
            this.logLevels[upperLevel] :
            this.logLevels.INFO;
    }

    /**
     * 현재 로그 레벨 이름 반환
     */
    getCurrentLevelName() {
        const levelNames = Object.keys(this.logLevels);
        return levelNames[this.currentLevel] || 'INFO';
    }

    /**
     * 메인 로그 함수
     */
    log(category, level, message, metadata = null) {
        // 날짜 변경 확인 (매 로그마다 체크 - 성능보다 정확성 우선)
        this.updateCurrentDate();

        const levelNum = this.parseLogLevel(level);

        // 현재 설정된 레벨보다 낮으면 무시
        if (levelNum < this.currentLevel) {
            return;
        }

        const logEntry = this.formatLogEntry(category, level, message, metadata);

        // 콘솔 출력
        if (this.logToConsole) {
            this.outputToConsole(level, logEntry);
        }

        // 파일 출력
        if (this.logToFile && this.initialized) {
            this.outputToFile(category, level, logEntry);
        }
    }

    /**
     * 로그 엔트리 포맷팅
     */
    formatLogEntry(category, level, message, metadata) {
        // const timestamp = new Date().toISOString();
        const timestamp = this.getKSTTimestamp();

        const pid = process.pid;

        let logLine = `${timestamp} [${pid}] [${level.toUpperCase()}] [${category.toUpperCase()}] ${message}`;

        if (metadata) {
            if (typeof metadata === 'object') {
                logLine += ` | ${JSON.stringify(metadata)}`;
            } else {
                logLine += ` | ${metadata}`;
            }
        }

        return logLine;
    }

    /**
     * 콘솔 출력 (색상 포함)
     */
    outputToConsole(level, logEntry) {
        const colors = {
            DEBUG: '\x1b[36m',   // cyan
            INFO: '\x1b[32m',    // green  
            WARN: '\x1b[33m',    // yellow
            ERROR: '\x1b[31m',   // red
            FATAL: '\x1b[35m'    // magenta
        };

        const resetColor = '\x1b[0m';
        const color = colors[level.toUpperCase()] || colors.INFO;

        console.log(`${color}${logEntry}${resetColor}`);
    }

    /**
     * 파일 출력
     */
    outputToFile(category, level, logEntry) {
        // 해당 카테고리 스트림에 출력
        const streamInfo = this.logStreams.get(category);
        if (streamInfo && streamInfo.stream) {
            streamInfo.stream.write(logEntry + '\n');
        }

        // ERROR/FATAL은 error 카테고리에도 출력
        if ((level.toUpperCase() === 'ERROR' || level.toUpperCase() === 'FATAL') && category !== 'error') {
            const errorStreamInfo = this.logStreams.get('error');
            if (errorStreamInfo && errorStreamInfo.stream) {
                errorStreamInfo.stream.write(logEntry + '\n');
            }
        }

        // 파일 크기 체크 및 로테이션
        this.checkFileRotation(category);
    }

    /**
     * 파일 크기 체크 및 로테이션
     */
    checkFileRotation(category) {
        const streamInfo = this.logStreams.get(category);
        if (!streamInfo) return;

        try {
            const stats = fs.statSync(streamInfo.filePath);
            const fileSizeMB = stats.size / (1024 * 1024);

            if (fileSizeMB > this.maxFileSizeMB) {
                this.rotateLogFile(category, streamInfo);
            }
        } catch (error) {
            // 파일 통계 확인 실패는 무시
        }
    }

    /**
     * 로그 파일 로테이션
     */
    rotateLogFile(category, streamInfo) {
        try {
            // 현재 스트림 종료
            streamInfo.stream.end();

            // 백업 파일명 생성 (timestamp 추가)
            const timestamp = this.getKSTTimestamp().replace(/[: ]/g, '-').replace('.', '-');

            const dir = path.dirname(streamInfo.filePath);
            const ext = path.extname(streamInfo.filePath);
            const basename = path.basename(streamInfo.filePath, ext);
            const backupPath = path.join(dir, `${basename}_${timestamp}${ext}`);

            // 현재 파일을 백업으로 이동
            fs.renameSync(streamInfo.filePath, backupPath);

            // 새 스트림 생성
            const newStream = fs.createWriteStream(streamInfo.filePath, {
                flags: 'a',
                encoding: 'utf8'
            });

            newStream.on('error', (error) => {
                console.error(`로그 스트림 오류 (${category}):`, error);
            });

            this.logStreams.set(category, {
                stream: newStream,
                filePath: streamInfo.filePath,
                category: category
            });

            this.log('system', 'INFO', `로그 파일 로테이션 완료: ${category}`);

        } catch (error) {
            console.error(`로그 파일 로테이션 실패 (${category}):`, error);
        }
    }

    /**
     * 정리 작업 스케줄링
     */
    scheduleCleanupTasks() {
        // 매일 자정에 오래된 로그 정리
        const now = new Date();
        const tomorrow = new Date(now);
        tomorrow.setDate(tomorrow.getDate() + 1);
        tomorrow.setHours(0, 0, 0, 0);

        const timeUntilMidnight = tomorrow.getTime() - now.getTime();

        this.cleanupTimer = setTimeout(() => {
            this.cleanupOldLogs();

            // 이후 24시간마다 실행
            this.cleanupInterval = setInterval(() => {
                this.cleanupOldLogs();
            }, 24 * 60 * 60 * 1000);

        }, timeUntilMidnight);
    }

    /**
     * 오래된 로그 정리 (.env의 LOG_MAX_FILES 및 retentionDays 사용)
     */
    async cleanupOldLogs() {
        if (!this.useDateFolders) return;

        try {
            const cutoffDate = new Date();
            cutoffDate.setDate(cutoffDate.getDate() - this.retentionDays);

            const logDirs = fs.readdirSync(this.logBaseDir, { withFileTypes: true });

            for (const dir of logDirs) {
                if (dir.isDirectory()) {
                    const dirDate = new Date(dir.name);

                    if (dirDate < cutoffDate) {
                        const fullPath = path.join(this.logBaseDir, dir.name);

                        // 디렉토리 삭제
                        fs.rmSync(fullPath, { recursive: true, force: true });
                        this.log('system', 'INFO', `오래된 로그 디렉토리 삭제: ${dir.name}`);
                    }
                }
            }

        } catch (error) {
            this.log('system', 'ERROR', '로그 정리 중 오류', { error: error.message });
        }
    }

    /**
     * 편의 함수들
     */
    debug(category, message, metadata) {
        this.log(category, 'DEBUG', message, metadata);
    }

    info(category, message, metadata) {
        this.log(category, 'INFO', message, metadata);
    }

    warn(category, message, metadata) {
        this.log(category, 'WARN', message, metadata);
    }

    error(category, message, metadata) {
        this.log(category, 'ERROR', message, metadata);
    }

    fatal(category, message, metadata) {
        this.log(category, 'FATAL', message, metadata);
    }

    /**
     * 특화된 로거들
     */
    crossplatform(level, message, metadata) {
        this.log('crossplatform', level, message, metadata);
    }

    database(level, message, metadata) {
        this.log('database', level, message, metadata);
    }

    api(level, message, metadata) {
        this.log('api', level, message, metadata);
    }

    services(level, message, metadata) {
        this.log('services', level, message, metadata);
    }

    system(level, message, metadata) {
        this.log('system', level, message, metadata);
    }

    app(level, message, metadata) {
        this.log('application', level, message, metadata);
    }

    /**
     * 요청 로깅 미들웨어용
     */
    logRequest(req, res, responseTime) {
        const logData = {
            method: req.method,
            url: req.originalUrl,
            ip: req.ip || req.connection.remoteAddress,
            userAgent: req.get('User-Agent'),
            statusCode: res.statusCode,
            responseTime: `${responseTime}ms`,
            contentLength: res.get('Content-Length') || 0
        };

        const level = res.statusCode >= 400 ? 'WARN' : 'INFO';
        this.api(level, `${req.method} ${req.originalUrl} - ${res.statusCode}`, logData);
    }

    /**
     * 에러 로깅
     */
    logError(error, req = null) {
        const errorData = {
            name: error.name,
            message: error.message,
            stack: error.stack,
            url: req?.originalUrl,
            method: req?.method,
            ip: req?.ip,
            timestamp: this.getKSTTimestamp()
        };

        this.error('application', `Unhandled Error: ${error.message}`, errorData);
    }

    /**
     * 로그 통계 조회
     */
    getLogStats() {
        const stats = {
            directory: this.logBaseDir,
            currentLevel: this.getCurrentLevelName(),
            logToConsole: this.logToConsole,
            logToFile: this.logToFile,
            useDateFolders: this.useDateFolders,
            useHourlyFiles: this.useHourlyFiles,
            maxFileSizeMB: this.maxFileSizeMB,
            maxFiles: this.maxFiles,
            retentionDays: this.retentionDays,
            files: {},
            totalStreams: this.logStreams.size,
            currentDate: this.currentDate
        };

        // 현재 활성 로그 파일들
        this.logStreams.forEach((streamInfo, category) => {
            try {
                if (fs.existsSync(streamInfo.filePath)) {
                    const fileStats = fs.statSync(streamInfo.filePath);
                    stats.files[category] = {
                        filename: path.basename(streamInfo.filePath),
                        fullPath: streamInfo.filePath,
                        size: fileStats.size,
                        sizeFormatted: this.formatBytes(fileStats.size),
                        modified: fileStats.mtime,
                        exists: true
                    };
                }
            } catch (error) {
                stats.files[category] = {
                    filename: path.basename(streamInfo.filePath),
                    exists: false,
                    error: error.message
                };
            }
        });

        return stats;
    }

    /**
     * 바이트 포맷팅
     */
    formatBytes(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    /**
     * 로그 설정 변경
     */
    updateSettings(settings) {
        const oldSettings = {
            logToConsole: this.logToConsole,
            logToFile: this.logToFile,
            logLevel: this.getCurrentLevelName()
        };

        if (settings.logToConsole !== undefined) {
            this.logToConsole = settings.logToConsole;
        }

        if (settings.logToFile !== undefined) {
            this.logToFile = settings.logToFile;
        }

        if (settings.logLevel !== undefined) {
            this.currentLevel = this.parseLogLevel(settings.logLevel);
        }

        this.system('INFO', '로그 설정 변경됨', {
            before: oldSettings,
            after: {
                logToConsole: this.logToConsole,
                logToFile: this.logToFile,
                logLevel: this.getCurrentLevelName()
            }
        });
    }

    /**
     * 정리 및 종료
     */
    shutdown() {
        this.system('INFO', 'LogManager 종료 중...');

        if (this.cleanupTimer) {
            clearTimeout(this.cleanupTimer);
            this.cleanupTimer = null;
        }

        if (this.cleanupInterval) {
            clearInterval(this.cleanupInterval);
            this.cleanupInterval = null;
        }

        this.logStreams.forEach((streamInfo, category) => {
            try {
                streamInfo.stream.end();
                this.system('INFO', `로그 스트림 종료: ${category}`);
            } catch (error) {
                console.error(`로그 스트림 종료 실패 (${category}):`, error);
            }
        });

        this.logStreams.clear();
        this.initialized = false;
    }
}

// 싱글톤 인스턴스
const logManager = LogManager.getInstance();

// Express 미들웨어 팩토리
function createRequestLogger() {
    return (req, res, next) => {
        const start = Date.now();

        res.on('finish', () => {
            const responseTime = Date.now() - start;
            logManager.logRequest(req, res, responseTime);
        });

        next();
    };
}

// Express 에러 핸들러 팩토리
function createErrorLogger() {
    return (error, req, res, next) => {
        logManager.logError(error, req);
        next(error);
    };
}

// Export
module.exports = {
    getInstance: () => logManager,

    // 편의 함수들
    log: (category, level, message, metadata) => logManager.log(category, level, message, metadata),
    debug: (category, message, metadata) => logManager.debug(category, message, metadata),
    info: (category, message, metadata) => logManager.info(category, message, metadata),
    warn: (category, message, metadata) => logManager.warn(category, message, metadata),
    error: (category, message, metadata) => logManager.error(category, message, metadata),
    fatal: (category, message, metadata) => logManager.fatal(category, message, metadata),

    // 특화 로거들
    app: (level, message, metadata) => logManager.app(level, message, metadata),
    crossplatform: (level, message, metadata) => logManager.crossplatform(level, message, metadata),
    database: (level, message, metadata) => logManager.database(level, message, metadata),
    api: (level, message, metadata) => logManager.api(level, message, metadata),
    services: (level, message, metadata) => logManager.services(level, message, metadata),
    system: (level, message, metadata) => logManager.system(level, message, metadata),

    // Express 미들웨어들
    requestLogger: createRequestLogger,
    errorLogger: createErrorLogger,

    // 유틸리티들
    getStats: () => logManager.getLogStats(),
    setLevel: (level) => logManager.setLogLevel(level),
    updateSettings: (settings) => logManager.updateSettings(settings),
    shutdown: () => logManager.shutdown(),
    getKSTTimestamp: () => logManager.getKSTTimestamp()
};