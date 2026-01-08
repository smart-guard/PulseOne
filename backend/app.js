// =============================================================================
// backend/app.js - PulseOne Backend Server 완성본
// CLI 정리 + 캐시 제어 + 로그 파일 저장 통합
// ✅ 개발 환경 Frontend 서빙 문제 수정
// =============================================================================

// =============================================================================
// CLI 처리 (단일화된 버전)
// =============================================================================
if (process.argv.includes('--version') || process.argv.includes('-v')) {
    console.log('PulseOne Industrial IoT Platform');
    console.log('Backend Server v2.1.0');
    console.log('Build: Production');
    process.exit(0);
}

if (process.argv.includes('--help') || process.argv.includes('-h')) {
    console.log('PulseOne Industrial IoT Platform - Backend Server');
    console.log('Usage: pulseone-backend.exe [options]');
    console.log('');
    console.log('Options:');
    console.log('  --version, -v         Show version information');
    console.log('  --help, -h            Show this help message');
    console.log('  --port=PORT          Set server port (default: 3000)');
    console.log('  --env=ENV            Set environment (development/production)');
    console.log('  --config=PATH        Set config directory path');
    console.log('  --data=PATH          Set data directory path');
    console.log('  --logs=PATH          Set logs directory path');
    console.log('  --auto-init          Enable automatic database initialization');
    console.log('  --no-cache           Disable all caching (for development)');
    console.log('');
    console.log('Examples:');
    console.log('  pulseone-backend.exe --port=8080');
    console.log('  pulseone-backend.exe --env=development --auto-init');
    console.log('  pulseone-backend.exe --config=./config --data=./data');
    console.log('');
    console.log('Environment Variables:');
    console.log('  NODE_ENV             Set environment mode');
    console.log('  BACKEND_PORT         Set server port');
    console.log('  DATABASE_TYPE        Set database type (SQLITE/POSTGRESQL)');
    console.log('  AUTO_INITIALIZE_ON_START  Enable auto initialization');
    console.log('');
    process.exit(0);
}

// CLI 인수 처리
const portArg = process.argv.find(arg => arg.startsWith('--port='));
if (portArg) {
    process.env.BACKEND_PORT = portArg.split('=')[1];
}

const envArg = process.argv.find(arg => arg.startsWith('--env='));
if (envArg) {
    process.env.NODE_ENV = envArg.split('=')[1];
}

const configArg = process.argv.find(arg => arg.startsWith('--config='));
if (configArg) {
    process.env.CONFIG_DIR = configArg.split('=')[1];
}

const dataArg = process.argv.find(arg => arg.startsWith('--data='));
if (dataArg) {
    process.env.DATA_DIR = dataArg.split('=')[1];
}

const logsArg = process.argv.find(arg => arg.startsWith('--logs='));
if (logsArg) {
    process.env.LOGS_DIR = logsArg.split('=')[1];
}

if (process.argv.includes('--auto-init')) {
    process.env.AUTO_INITIALIZE_ON_START = 'true';
}

const noCacheMode = process.argv.includes('--no-cache');
if (noCacheMode) {
    process.env.DISABLE_ALL_CACHE = 'true';
}

// =============================================================================
// 기본 모듈 imports
// =============================================================================
const express = require('express');
const cors = require('cors');
const path = require('path');
const http = require('http');
const { Server } = require('socket.io');
const { initializeConnections } = require('./lib/connection/db');

// =============================================================================
// ConfigManager 및 LogManager 로드 (최우선)
// =============================================================================
const config = require('./lib/config/ConfigManager');
const logger = require('./lib/utils/LogManager');

// 서버 시작 로그
logger.system('INFO', 'PulseOne Backend Server 시작 중...', {
    version: '2.1.0',
    nodeVersion: process.version,
    platform: process.platform,
    pid: process.pid,
    workingDir: process.cwd()
});

// ConfigManager 설정 로드
const serverConfig = config.getServerConfig();
const dbConfig = config.getDatabaseConfig();
const redisConfig = config.getRedisConfig();

logger.system('INFO', 'ConfigManager 설정 로드 완료', {
    environment: serverConfig.env,
    stage: serverConfig.stage,
    port: serverConfig.port,
    databaseType: dbConfig.type,
    autoInit: config.getBoolean('AUTO_INITIALIZE_ON_START', false)
});

// =============================================================================
// 캐시 제어 미들웨어 로드
// =============================================================================
let CacheControlMiddleware = null;
let createChromeCacheBuster = null;
let createSPACacheMiddleware = null;

try {
    const cacheControlModule = require('./lib/middleware/cacheControl');
    CacheControlMiddleware = cacheControlModule.CacheControlMiddleware;
    createChromeCacheBuster = cacheControlModule.createChromeCacheBuster;
    createSPACacheMiddleware = cacheControlModule.createSPACacheMiddleware;
    
    logger.system('INFO', '캐시 제어 미들웨어 로드 성공');
} catch (error) {
    logger.system('WARN', '캐시 제어 미들웨어 로드 실패', { error: error.message });
}

// =============================================================================
// 안전한 모듈 로딩 (로그 기록 포함)
// =============================================================================

// WebSocket 서비스
let WebSocketService = null;
let webSocketService = null;
try {
    WebSocketService = require('./lib/services/WebSocketService');
    logger.system('INFO', 'WebSocketService 로드 성공');
} catch (error) {
    logger.system('WARN', 'WebSocketService 로드 실패', { error: error.message });
}

// 데이터베이스 초기화 시스템
let DatabaseInitializer = null;
try {
    DatabaseInitializer = require('./lib/database/DatabaseInitializer');
    logger.system('INFO', 'DatabaseInitializer 로드 성공');
} catch (error) {
    logger.system('WARN', 'DatabaseInitializer 로드 실패', { error: error.message });
}

// 알람 이벤트 구독자
let AlarmEventSubscriber = null;
try {
    AlarmEventSubscriber = require('./lib/services/AlarmEventSubscriber');
    logger.system('INFO', 'AlarmEventSubscriber 로드 성공');
} catch (error) {
    logger.system('WARN', 'AlarmEventSubscriber 로드 실패', { error: error.message });
}

// Collector 프록시 서비스
let CollectorProxyService = null;
try {
    const { getInstance: getCollectorProxy } = require('./lib/services/CollectorProxyService');
    CollectorProxyService = getCollectorProxy;
    
    // 인스턴스 테스트
    const testProxy = CollectorProxyService();
    logger.system('INFO', 'CollectorProxyService 로드 및 인스턴스 생성 성공');
} catch (error) {
    logger.system('WARN', 'CollectorProxyService 로드 실패', { 
        error: error.message,
        stack: error.stack?.split('\n')[0] 
    });
    CollectorProxyService = null;
}

// 설정 동기화 훅
let ConfigSyncHooks = null;
try {
    const { getInstance: getConfigSyncHooks } = require('./lib/hooks/ConfigSyncHooks');
    ConfigSyncHooks = getConfigSyncHooks;
    
    // 인스턴스 테스트
    const testHooks = ConfigSyncHooks();
    logger.system('INFO', 'ConfigSyncHooks 로드 및 인스턴스 생성 성공');
} catch (error1) {
    try {
        const { getInstance: getConfigSyncHooks } = require('./lib/hook/ConfigSyncHooks');
        ConfigSyncHooks = getConfigSyncHooks;
        const testHooks = ConfigSyncHooks();
        logger.system('INFO', 'ConfigSyncHooks 로드 성공 (대체 경로)');
    } catch (error2) {
        logger.system('WARN', 'ConfigSyncHooks 로드 완전 실패', {
            hooksError: error1.message,
            hookError: error2.message
        });
    }
}

// =============================================================================
// Express 앱 및 서버 설정
// =============================================================================
const app = express();
const server = http.createServer(app);

// =============================================================================
// 캐시 제어 미들웨어 적용 (최우선)
// =============================================================================
if (CacheControlMiddleware && createChromeCacheBuster && createSPACacheMiddleware) {
    // 1. Chrome + Windows 특화 캐시 버스터
    app.use(createChromeCacheBuster());
    logger.system('DEBUG', 'Chrome 캐시 버스터 미들웨어 적용');

    // 2. 캐시 제어 미들웨어
    const cacheControl = new CacheControlMiddleware({
        disableCacheInDev: true,
        staticFileMaxAge: serverConfig.env === 'production' ? 300 : 0,
        apiCacheMaxAge: 0,
        htmlNoCache: true,
        addVersionParam: true,
        disableETag: config.getBoolean('DISABLE_ALL_CACHE', noCacheMode)
    });
    
    app.use(cacheControl.createMiddleware());
    logger.system('DEBUG', '캐시 제어 미들웨어 적용');

    // 3. SPA 캐시 방지
    app.use(createSPACacheMiddleware());
    logger.system('DEBUG', 'SPA 캐시 방지 미들웨어 적용');
    
    logger.system('INFO', '브라우저 캐시 제어 시스템 완전 활성화', {
        chromeCacheBuster: true,
        cacheControl: true,
        spaCacheMiddleware: true,
        noCacheMode: noCacheMode
    });
} else {
    // 폴백: 기본 캐시 헤더
    app.use((req, res, next) => {
        res.set({
            'Cache-Control': 'no-cache, no-store, must-revalidate, private, max-age=0',
            'Pragma': 'no-cache',
            'Expires': '0',
            'Last-Modified': new Date().toUTCString()
        });
        next();
    });
    
    logger.system('WARN', '기본 캐시 방지 헤더만 적용됨 (고급 캐시 제어 미들웨어 없음)');
}

// =============================================================================
// 요청 로깅 미들웨어 (LogManager 사용)
// =============================================================================
const requestLogger = logger.requestLogger();
app.use(requestLogger);

logger.system('INFO', '요청 로깅 미들웨어 활성화', {
    logToFile: true,
    logToConsole: serverConfig.logLevel === 'debug'
});

// =============================================================================
// CORS 설정 (ConfigManager 기반)
// =============================================================================
const corsOptions = {
    origin: function (origin, callback) {
        const isDevelopment = serverConfig.env === 'development';
        
        if (isDevelopment || !origin) {
            callback(null, true);
            return;
        }
        
        const allowedOrigins = [
            'http://localhost:3000',
            'http://localhost:5173',
            'http://localhost:5174', 
            'http://localhost:8080',
            'http://127.0.0.1:3000',
            'http://127.0.0.1:5173',
            config.get('FRONTEND_URL', 'http://localhost:5173'),
            ...config.get('CORS_ALLOWED_ORIGINS', '').split(',').filter(Boolean)
        ];
        
        if (allowedOrigins.includes(origin)) {
            callback(null, true);
        } else {
            logger.api('WARN', 'CORS 차단된 요청', { origin });
            callback(new Error('Not allowed by CORS'));
        }
    },
    credentials: true,
    methods: ['GET', 'POST', 'PUT', 'PATCH', 'DELETE', 'OPTIONS'],
    allowedHeaders: [
        'Content-Type', 
        'Authorization', 
        'X-Tenant-ID',
        'X-Requested-With',
        'Accept',
        'Origin'
    ],
    optionsSuccessStatus: 200
};

app.use(cors(corsOptions));

logger.system('INFO', 'CORS 설정 완료', {
    developmentMode: serverConfig.env === 'development',
    allowedOrigins: serverConfig.env === 'development' ? '모든 Origin 허용' : '제한적 허용'
});

// =============================================================================
// Socket.IO 서버 설정 (로그 포함)
// =============================================================================
let io = null;

if (WebSocketService) {
    webSocketService = new WebSocketService(server);
    io = webSocketService.io;
    app.locals.webSocketService = webSocketService;
    app.locals.io = io;
    
    logger.system('INFO', 'WebSocket 서비스 초기화 완료', {
        service: 'WebSocketService',
        transport: 'polling + websocket'
    });
} else {
    // 직접 Socket.IO 초기화
    io = new Server(server, {
        cors: {
            origin: function (origin, callback) {
                const isDevelopment = serverConfig.env === 'development';
                if (isDevelopment || !origin) {
                    callback(null, true);
                    return;
                }
                
                const allowedOrigins = [
                    'http://localhost:3000',
                    'http://localhost:5173',
                    'http://localhost:5174',
                    config.get('FRONTEND_URL', 'http://localhost:5173')
                ];
                
                callback(null, allowedOrigins.includes(origin));
            },
            methods: ['GET', 'POST'],
            credentials: true
        },
        transports: ['polling', 'websocket'],
        allowEIO3: true
    });
    
    io.on('connection', (socket) => {
        logger.system('DEBUG', 'WebSocket 클라이언트 연결', { socketId: socket.id });
        
        socket.on('disconnect', () => {
            logger.system('DEBUG', 'WebSocket 클라이언트 연결 해제', { socketId: socket.id });
        });
        
        socket.on('join_tenant', (tenantId) => {
            socket.join(`tenant:${tenantId}`);
            logger.system('DEBUG', '테넌트 룸 조인', { 
                socketId: socket.id, 
                tenantId: tenantId 
            });
        });
    });
    
    app.locals.io = io;
    app.locals.webSocketService = null;
    
    logger.system('INFO', '기본 Socket.IO 서버 초기화 완료', {
        service: 'Direct Socket.IO',
        corsEnabled: true
    });
}

// =============================================================================
// 기본 미들웨어 설정
// =============================================================================
const maxRequestSize = config.get('MAX_REQUEST_SIZE', '10mb');
app.use(express.json({ limit: maxRequestSize }));
app.use(express.urlencoded({ extended: true, limit: maxRequestSize }));

// =============================================================================
// ✅ 정적 파일 서빙 (프로덕션 환경에만 활성화)
// =============================================================================
const staticMaxAge = serverConfig.env === 'production' ? '1d' : 0;

// 개발 환경에서는 Frontend를 별도 컨테이너(Vite 개발 서버, 포트 5173)에서 제공
// 프로덕션 환경에서만 Backend가 빌드된 정적 파일을 서빙
if (serverConfig.env === 'production') {
    app.use(express.static(path.join(__dirname, '../frontend'), {
        maxAge: staticMaxAge,
        setHeaders: (res, filepath) => {
            // 정적 파일에도 캐시 방지 헤더 추가
            if (config.getBoolean('DISABLE_ALL_CACHE', noCacheMode)) {
                res.set({
                    'Cache-Control': 'no-cache, no-store, must-revalidate',
                    'Pragma': 'no-cache',
                    'Expires': '0'
                });
            }
        }
    }));
    
    logger.system('INFO', '정적 파일 서빙 활성화 (프로덕션)', {
        staticPath: path.join(__dirname, '../frontend'),
        maxAge: staticMaxAge
    });
} else {
    logger.system('INFO', '정적 파일 서빙 비활성화 (개발 환경)', {
        frontendUrl: 'http://localhost:5173',
        note: 'Frontend는 별도 Vite 개발 서버에서 실행됨'
    });
}

logger.system('INFO', '기본 미들웨어 설정 완료', {
    maxRequestSize: maxRequestSize,
    staticFileServing: serverConfig.env === 'production' ? 'enabled' : 'disabled'
});

// =============================================================================
// 인증 및 테넌트 미들웨어 (로그 포함)
// =============================================================================
const authenticateToken = (req, res, next) => {
    if (!req.originalUrl.startsWith('/api/') || 
        req.originalUrl.startsWith('/api/health') ||
        req.originalUrl.startsWith('/api/init/') ||
        req.originalUrl.startsWith('/api/test/') ||
        req.originalUrl.startsWith('/api/websocket/')) {
        return next();
    }

    const devUser = {
        id: config.getNumber('DEV_USER_ID', 1),
        username: config.get('DEV_USERNAME', 'admin'),
        tenant_id: config.getNumber('DEV_TENANT_ID', 1),
        role: config.get('DEV_USER_ROLE', 'admin')
    };
    
    req.user = devUser;
    
    // 인증 로그 (DEBUG 레벨에서만)
    if (serverConfig.logLevel === 'debug') {
        logger.api('DEBUG', 'API 인증 처리', {
            url: req.originalUrl,
            method: req.method,
            userId: devUser.id,
            tenantId: devUser.tenant_id
        });
    }
    
    next();
};

const tenantIsolation = (req, res, next) => {
    if (req.user) {
        req.tenantId = req.user.tenant_id;
    } else {
        req.tenantId = config.getNumber('DEFAULT_TENANT_ID', 1);
    }
    next();
};

app.use('/api/*', authenticateToken);
app.use('/api/*', tenantIsolation);

logger.system('INFO', '인증 및 테넌트 미들웨어 설정 완료', {
    devMode: true,
    defaultTenantId: config.getNumber('DEFAULT_TENANT_ID', 1)
});

// =============================================================================
// 데이터베이스 연결 및 자동 초기화 (로그 강화)
// =============================================================================
let connections = {};

async function initializeSystem() {
    try {
        logger.system('INFO', 'PulseOne 시스템 초기화 시작');
        
        // 1. 데이터베이스 연결
        logger.database('INFO', '데이터베이스 연결 초기화 중...');
        connections = await initializeConnections();
        app.locals.getDB = () => connections;
        logger.database('INFO', '데이터베이스 연결 초기화 완료');
        
        // 2. 자동 초기화 확인
        const autoInitialize = config.getBoolean('AUTO_INITIALIZE_ON_START', false);
        const skipIfInitialized = config.getBoolean('SKIP_IF_INITIALIZED', false);
        const failOnInitError = config.getBoolean('FAIL_ON_INIT_ERROR', false);
        
        logger.system('INFO', '자동 초기화 설정 확인', {
            autoInitialize,
            skipIfInitialized,
            failOnInitError,
            databaseInitializerAvailable: !!DatabaseInitializer
        });
        
        if (autoInitialize && DatabaseInitializer) {
            logger.system('INFO', '자동 초기화 프로세스 시작');
            
            const initializer = new DatabaseInitializer(connections);
            await initializer.checkDatabaseStatus();
            
            if (initializer.isFullyInitialized() && skipIfInitialized) {
                logger.system('INFO', '데이터베이스 이미 초기화됨 - 스킵');
            } else if (!initializer.isFullyInitialized()) {
                logger.system('INFO', '데이터베이스 초기화 필요 - 프로세스 시작');
                
                try {
                    const createBackup = config.getBoolean('CREATE_BACKUP_ON_INIT', true);
                    if (createBackup) {
                        logger.database('INFO', '초기화 전 백업 생성 중...');
                        await initializer.createBackup(true);
                        logger.database('INFO', '백업 생성 완료');
                    }
                    
                    await initializer.performInitialization();
                    logger.system('INFO', '데이터베이스 자동 초기화 완료');
                } catch (initError) {
                    logger.system('ERROR', '자동 초기화 실패', { error: initError.message });
                    
                    if (failOnInitError) {
                        logger.system('FATAL', '초기화 실패로 인한 서버 종료');
                        process.exit(1);
                    }
                }
            }
        } else if (autoInitialize) {
            logger.system('WARN', '자동 초기화 활성화되었으나 DatabaseInitializer 없음');
        }
        
        logger.system('INFO', '시스템 초기화 완료');
        
    } catch (error) {
        logger.system('ERROR', '시스템 초기화 실패', { error: error.message });
        
        const failOnInitError = config.getBoolean('FAIL_ON_INIT_ERROR', false);
        if (failOnInitError) {
            logger.system('FATAL', '시스템 초기화 실패로 인한 서버 종료');
            process.exit(1);
        }
    }
}

// =============================================================================
// 실시간 알람 구독자 초기화 (로그 포함)
// =============================================================================
let alarmSubscriber = null;

async function startAlarmSubscriber() {
    if (!AlarmEventSubscriber || !io) {
        logger.services('WARN', '실시간 알람 서비스 비활성화', {
            alarmSubscriberAvailable: !!AlarmEventSubscriber,
            socketIoAvailable: !!io
        });
        return;
    }
    
    try {
        logger.services('INFO', '실시간 알람 구독자 시작 중...');
        alarmSubscriber = new AlarmEventSubscriber(io);
        await alarmSubscriber.start();
        
        app.locals.alarmSubscriber = alarmSubscriber;
        logger.services('INFO', '실시간 알람 구독자 시작 완료');
        
    } catch (error) {
        logger.services('ERROR', '실시간 알람 구독자 시작 실패', { error: error.message });
    }
}

// 시스템 초기화 실행
initializeSystem();

// =============================================================================
// 헬스체크 및 관리 엔드포인트
// =============================================================================

// Health check
app.get('/api/health', async (req, res) => {
    try {
        const healthInfo = { 
            status: 'ok', 
            timestamp: new Date().toISOString(),
            uptime: process.uptime(),
            pid: process.pid,
            config_manager: {
                enabled: true,
                environment: serverConfig.env,
                stage: serverConfig.stage,
                database_type: dbConfig.type,
                auto_init: config.getBoolean('AUTO_INITIALIZE_ON_START'),
                log_level: serverConfig.logLevel
            },
            cache_control: {
                enabled: !!CacheControlMiddleware,
                no_cache_mode: config.getBoolean('DISABLE_ALL_CACHE', noCacheMode),
                static_max_age: staticMaxAge
            },
            logging: {
                enabled: true,
                log_to_file: true,
                log_to_console: true,
                log_level: serverConfig.logLevel
            }
        };
        
        // 실시간 기능 상태
        healthInfo.realtime = {
            websocket: {
                enabled: !!(webSocketService || io),
                connected_clients: webSocketService ? 
                    webSocketService.getStatus()?.stats?.socket_clients || 0 : 
                    (io ? io.engine.clientsCount : 0)
            },
            alarm_subscriber: {
                enabled: !!alarmSubscriber,
                status: alarmSubscriber ? alarmSubscriber.getStatus() : null
            }
        };
        
        // Collector 통합 상태
        healthInfo.collector_integration = {
            proxy_service: { enabled: !!CollectorProxyService },
            config_sync: { enabled: !!ConfigSyncHooks }
        };
        
        // 초기화 상태
        healthInfo.initialization = {
            databaseInitializer: {
                available: !!DatabaseInitializer,
                autoInit: config.getBoolean('AUTO_INITIALIZE_ON_START')
            }
        };
        
        logger.api('DEBUG', 'Health check 요청', { status: 'ok' });
        res.json(healthInfo);
        
    } catch (error) {
        logger.api('ERROR', 'Health check 실패', { error: error.message });
        res.status(500).json({
            status: 'error',
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// 로그 상태 조회 API
app.get('/api/system/logs', (req, res) => {
    try {
        const logStats = logger.getStats();
        
        logger.api('INFO', '로그 상태 조회');
        res.json({
            success: true,
            data: logStats,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        logger.api('ERROR', '로그 상태 조회 실패', { error: error.message });
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 로그 설정 변경 API
app.post('/api/system/logs/settings', (req, res) => {
    try {
        const { logLevel, logToConsole, logToFile } = req.body;
        
        logger.updateSettings({
            logLevel,
            logToConsole,
            logToFile
        });
        
        logger.system('INFO', '로그 설정 변경됨', { 
            newSettings: { logLevel, logToConsole, logToFile },
            changedBy: req.user?.username || 'system'
        });
        
        res.json({
            success: true,
            message: '로그 설정이 변경되었습니다.',
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        logger.api('ERROR', '로그 설정 변경 실패', { error: error.message });
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 테스트 알람 엔드포인트
app.post('/api/test/alarm', (req, res) => {
    if (!io) {
        return res.status(503).json({
            success: false,
            error: 'Socket.IO 서비스가 비활성화되어 있습니다.'
        });
    }
    
    try {
        const testAlarm = {
            occurrence_id: Date.now(),
            rule_id: 999,
            tenant_id: config.getNumber('DEFAULT_TENANT_ID', 1),
            device_id: 'test_device_001',
            point_id: 1,
            message: '테스트 알람 - 온도 센서 이상 감지',
            severity: 'HIGH',
            severity_level: 3,
            state: 1,
            timestamp: Date.now(),
            source_name: '테스트 온도 센서',
            location: '1층 서버실',
            trigger_value: 85.5,
            formatted_time: new Date().toLocaleString('ko-KR')
        };
        
        let sent = false;
        if (webSocketService) {
            sent = webSocketService.sendAlarm(testAlarm);
        } else {
            io.to(`tenant:${testAlarm.tenant_id}`).emit('alarm_triggered', testAlarm);
            io.emit('alarm_triggered', testAlarm);
            sent = true;
        }
        
        logger.services('INFO', '테스트 알람 전송', { 
            alarmId: testAlarm.occurrence_id,
            sent: sent,
            connectedClients: io ? io.engine.clientsCount : 0
        });
        
        res.json({ 
            success: true,
            message: '테스트 알람이 전송되었습니다.',
            alarm: testAlarm,
            sent_via_websocket: sent,
            connected_clients: io ? io.engine.clientsCount : 0
        });
        
    } catch (error) {
        logger.services('ERROR', '테스트 알람 전송 실패', { error: error.message });
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// =============================================================================
// API Routes 등록 (로그 포함)
// =============================================================================
logger.system('INFO', 'API 라우트 등록 시작');

// 기본 시스템 라우트들
const systemRoutes = require('./routes/system');
const processRoutes = require('./routes/processes');
const serviceRoutes = require('./routes/services');
const userRoutes = require('./routes/user');
const dataRoutes = require('./routes/data');

app.use('/api/system', systemRoutes);
app.use('/api/processes', processRoutes);
app.use('/api/services', serviceRoutes);
app.use('/api/users', userRoutes);
app.use('/api/data', dataRoutes);

logger.system('INFO', '기본 시스템 라우트 등록 완료');

// 장치 관리 라우트
try {
    const deviceRoutes = require('./routes/devices');
    app.use('/api/devices', deviceRoutes);
    logger.system('INFO', 'Device API 라우트 등록 완료');
} catch (error) {
    logger.system('WARN', 'Device 라우트 로드 실패', { error: error.message });
}

// 프로토콜 관리 라우트
try {
    const protocolRoutes = require('./routes/protocols');
    app.use('/api/protocols', protocolRoutes);
    logger.system('INFO', 'Protocol API 라우트 등록 완료');
} catch (error) {
    logger.system('WARN', 'Protocol 라우트 로드 실패', { error: error.message });
}

// 알람 관리 라우트 (중요)
try {
    const alarmRoutes = require('./routes/alarms');
    app.use('/api/alarms', alarmRoutes);
    logger.system('INFO', 'Alarm API 라우트 등록 완료');
} catch (error) {
    logger.system('ERROR', 'Alarm 라우트 로드 실패', { error: error.message });
    
    // 디버그 모드 알람 라우트
    const express = require('express');
    const debugAlarmRouter = express.Router();
    
    debugAlarmRouter.use((req, res, next) => {
        req.user = { 
            id: config.getNumber('DEV_USER_ID', 1), 
            username: config.get('DEV_USERNAME', 'admin'), 
            tenant_id: config.getNumber('DEV_TENANT_ID', 1), 
            role: config.get('DEV_USER_ROLE', 'admin') 
        };
        req.tenantId = config.getNumber('DEV_TENANT_ID', 1);
        next();
    });
    
    debugAlarmRouter.get('/test', (req, res) => {
        logger.services('INFO', '디버그 알람 API 테스트 호출');
        res.json({
            success: true,
            message: '디버그 모드 - 알람 API 기본 기능',
            timestamp: new Date().toISOString(),
            debug_mode: true
        });
    });
    
    app.use('/api/alarms', debugAlarmRouter);
    logger.system('WARN', '디버그 알람 라우트 등록됨');
}

// 선택적 라우트들
const optionalRoutes = [
    { path: './routes/collector-proxy', mount: '/api/collector', name: 'Collector Proxy' },
    { path: './routes/dashboard', mount: '/api/dashboard', name: 'Dashboard' },
    { path: './routes/realtime', mount: '/api/realtime', name: 'Realtime Data' },
    { path: './routes/virtual-points', mount: '/api/virtual-points', name: 'Virtual Points' },
    { path: './routes/sites', mount: '/api/sites', name: 'Site Management' },
    { path: './routes/data-points', mount: '/api/data-points', name: 'Data Points' },
    { path: './routes/monitoring', mount: '/api/monitoring', name: 'System Monitoring' },
    { path: './routes/backup', mount: '/api/backup', name: 'Backup/Restore' },
    { path: './routes/websocket', mount: '/api/websocket', name: 'WebSocket Management' }
];

optionalRoutes.forEach(route => {
    try {
        const routeModule = require(route.path);
        app.use(route.mount, routeModule);
        logger.system('INFO', `${route.name} API 라우트 등록 완료`);
    } catch (error) {
        logger.system('DEBUG', `${route.name} 라우트 로드 실패`, { error: error.message });
    }
});

logger.system('INFO', 'API 라우트 등록 완료');

// =============================================================================
// 에러 핸들링 (로그 포함)
// =============================================================================

// 404 handler
app.use('/api/*', (req, res) => {
    logger.api('WARN', 'API 엔드포인트 없음', {
        method: req.method,
        url: req.originalUrl,
        userAgent: req.get('User-Agent')
    });
    
    res.status(404).json({ 
        success: false,
        error: 'API endpoint not found',
        path: req.originalUrl,
        method: req.method,
        timestamp: new Date().toISOString()
    });
});

// Global error handler
const errorLogger = logger.errorLogger();
app.use(errorLogger);

app.use((error, req, res, next) => {
    logger.app('ERROR', '처리되지 않은 에러', {
        error: error.message,
        stack: error.stack,
        url: req.originalUrl,
        method: req.method,
        userId: req.user?.id
    });
    
    let statusCode = 500;
    let message = 'Internal server error';
    
    if (error.name === 'ValidationError') {
        statusCode = 400;
        message = 'Validation failed';
    } else if (error.name === 'UnauthorizedError') {
        statusCode = 401;
        message = 'Unauthorized';
    }
    
    res.status(statusCode).json({
        success: false,
        error: message,
        message: serverConfig.env === 'development' ? error.message : message,
        timestamp: new Date().toISOString()
    });
});

// =============================================================================
// ✅ 프론트엔드 서빙 (SPA 지원) - 프로덕션 환경만
// =============================================================================
if (serverConfig.env === 'production') {
    // 프로덕션: SPA를 위한 catch-all 라우트
    app.get('*', (req, res) => {
        res.sendFile(path.join(__dirname, '../frontend/index.html'));
    });
    
    logger.system('INFO', 'SPA Catch-all 라우트 등록 (프로덕션)');
} else {
    // 개발 환경: Frontend는 별도 서버에서 제공됨을 안내
    app.get('*', (req, res) => {
        res.status(404).json({
            success: false,
            error: 'Not Found',
            message: 'Development mode: Frontend is served at http://localhost:5173',
            backend_api: `http://localhost:${serverConfig.port}/api`,
            timestamp: new Date().toISOString()
        });
    });
    
    logger.system('INFO', 'Development 모드: Frontend 서빙 비활성화');
}

// =============================================================================
// Graceful Shutdown (로그 포함)
// =============================================================================
process.on('SIGTERM', gracefulShutdown);
process.on('SIGINT', gracefulShutdown);

function gracefulShutdown(signal) {
    logger.system('INFO', 'Graceful shutdown 시작', { signal });
    
    const shutdownTimeout = config.getNumber('SHUTDOWN_TIMEOUT_MS', 10000);
    
    server.close(async (err) => {
        if (err) {
            logger.system('ERROR', 'HTTP 서버 종료 중 오류', { error: err.message });
            process.exit(1);
        }
        
        logger.system('INFO', 'HTTP 서버 종료 완료');
        
        // WebSocket 서버 정리
        if (io) {
            io.close();
            logger.system('INFO', 'WebSocket 서버 종료 완료');
        }
        
        // 알람 구독자 정리
        if (alarmSubscriber) {
            try {
                await alarmSubscriber.stop();
                logger.system('INFO', '알람 구독자 종료 완료');
            } catch (error) {
                logger.system('ERROR', '알람 구독자 종료 중 오류', { error: error.message });
            }
        }
        
        // 데이터베이스 연결 정리
        if (connections.postgres) connections.postgres.end();
        if (connections.redis) connections.redis.disconnect();
        logger.system('INFO', '데이터베이스 연결 종료 완료');
        
        // 로그 매니저 종료
        logger.shutdown();
        
        console.log('✅ Graceful shutdown 완료');
        process.exit(0);
    });
    
    setTimeout(() => {
        logger.system('ERROR', 'Graceful shutdown 타임아웃 - 강제 종료');
        console.error('❌ 강제 종료');
        process.exit(1);
    }, shutdownTimeout);
}

// =============================================================================
// 서버 시작 (로그 포함)
// =============================================================================
const PORT = serverConfig.port;

server.listen(PORT, '0.0.0.0', async () => {
    const wsStatus = webSocketService ? 
        `활성화 (${webSocketService.getStatus()?.stats?.socket_clients || 0}명 연결)` : 
        (io ? `기본 모드 (${io.engine.clientsCount}명 연결)` : '비활성화');
        
    // 서버 시작 로그
    logger.system('INFO', 'PulseOne Backend Server 시작 완료', {
        port: PORT,
        environment: serverConfig.env,
        stage: serverConfig.stage,
        pid: process.pid,
        websocket: wsStatus,
        collectorIntegration: !!CollectorProxyService,
        cacheControl: !!CacheControlMiddleware,
        logManager: true,
        frontendServing: serverConfig.env === 'production' ? 'enabled' : 'disabled'
    });
    
    console.log(`
🚀 PulseOne Backend Server Started!
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📊 Backend API:   http://localhost:${PORT}/api
🔧 API Health:    http://localhost:${PORT}/api/health
📋 System Info:   http://localhost:${PORT}/api/system/info
📝 Log Status:    http://localhost:${PORT}/api/system/logs
🔥 Alarm Test:    http://localhost:${PORT}/api/alarms/test
🧪 Test Alarm:    POST http://localhost:${PORT}/api/test/alarm

${serverConfig.env === 'development' ? 
        `🎨 Frontend:      http://localhost:5173 (Vite Dev Server)
⚠️  Note:          개발 환경 - Frontend는 별도 컨테이너에서 실행됨` :
        `📊 Dashboard:     http://localhost:${PORT}
✅ Note:          프로덕션 환경 - Backend가 정적 파일 서빙`}

🌐 Environment: ${serverConfig.env}
🏷️  Stage: ${serverConfig.stage}
📝 Log Level: ${serverConfig.logLevel}
🚫 Cache Control: ${CacheControlMiddleware ? '✅ 고급 제어' : '⚠️ 기본 제어'}
💾 Database: ${dbConfig.type}
🔄 Auto Init: ${config.getBoolean('AUTO_INITIALIZE_ON_START') ? '✅' : '❌'}
📡 WebSocket: ${wsStatus}
🚨 Alarm Service: ${alarmSubscriber ? '✅ Ready' : '⚠️ Disabled'}
🔧 Collector: ${CollectorProxyService ? '✅ Available' : '❌ Not Found'}
📦 Static Files: ${serverConfig.env === 'production' ? '✅ Enabled' : '❌ Disabled (Dev)'}

PID: ${process.pid}
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    `);
    
    // Collector 연결 확인
    if (CollectorProxyService) {
        try {
            logger.services('INFO', 'Collector 연결 상태 확인 중...');
            const proxy = CollectorProxyService();
            const healthResult = await proxy.healthCheck();
            
            logger.services('INFO', 'Collector 연결 성공', {
                url: `${proxy.getCollectorConfig().host}:${proxy.getCollectorConfig().port}`,
                status: healthResult.data?.status
            });
            
        } catch (collectorError) {
            logger.services('WARN', 'Collector 연결 실패', { error: collectorError.message });
        }
    }
    
    // 알람 구독자 시작 (지연)
    const alarmStartDelay = config.getNumber('ALARM_SUBSCRIBER_START_DELAY_MS', 3000);
    setTimeout(startAlarmSubscriber, alarmStartDelay);
});

module.exports = app;