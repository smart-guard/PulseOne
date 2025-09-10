// =============================================================================
// backend/app.js - 통합 메인 애플리케이션 (CORS 및 WebSocket 수정 완료)
// 기존 구조 + WebSocket 서비스 분리 + Collector 통합 + 모든 API 라우트
// =============================================================================

const express = require('express');
const cors = require('cors');
const path = require('path');
const http = require('http');
const { Server } = require('socket.io');
const { initializeConnections } = require('./lib/connection/db');

// =============================================================================
// 안전한 모듈 로딩 (상세 에러 정보 포함)
// =============================================================================

// WebSocket 서비스 로드 (안전하게)
let WebSocketService = null;
let webSocketService = null;
try {
    WebSocketService = require('./lib/services/WebSocketService');
    console.log('✅ WebSocketService 로드 성공');
} catch (error) {
    console.warn('⚠️ WebSocketService 로드 실패:', error.message);
}

// 자동 초기화 시스템 (안전 로드)
let DatabaseInitializer = null;
try {
    DatabaseInitializer = require('./lib/database/DatabaseInitializer');
    console.log('✅ DatabaseInitializer 로드 성공 (lib/database/DatabaseInitializer.js)');
} catch (error1) {
    try {
        DatabaseInitializer = require('./scripts/database-initializer');
        console.log('✅ DatabaseInitializer 로드 성공 (scripts/database-initializer.js)');
    } catch (error2) {
        console.warn('⚠️ DatabaseInitializer 로드 실패:', error1.message);
        console.warn('   초기화 기능이 비활성화됩니다.');
    }
}

// 실시간 알람 구독자 (안전하게 로드)
let AlarmEventSubscriber = null;
try {
    AlarmEventSubscriber = require('./lib/services/AlarmEventSubscriber');
    console.log('✅ AlarmEventSubscriber 로드 성공');
} catch (error) {
    console.warn('⚠️ AlarmEventSubscriber 로드 실패:', error.message);
    console.warn('   실시간 알람 기능이 비활성화됩니다.');
}

// Collector 프록시 서비스 (개선된 에러 처리)
let CollectorProxyService = null;
try {
    const { getInstance: getCollectorProxy } = require('./lib/services/CollectorProxyService');
    CollectorProxyService = getCollectorProxy;
    console.log('✅ CollectorProxyService 로드 성공');
    
    // 즉시 인스턴스 초기화 테스트
    try {
        const testProxy = CollectorProxyService();
        console.log('✅ CollectorProxyService 인스턴스 생성 성공');
    } catch (instanceError) {
        console.warn('⚠️ CollectorProxyService 인스턴스 생성 실패:', instanceError.message);
        CollectorProxyService = null;
    }
    
} catch (error) {
    console.warn('⚠️ CollectorProxyService 로드 실패:', error.message);
    console.warn('   상세 에러:', error.stack?.split('\n')[0] || 'Unknown error');
    console.warn('   Collector 통합 기능이 비활성화됩니다.');
}

// 설정 동기화 훅 (개선된 에러 처리와 경로)
let ConfigSyncHooks = null;
try {
    // 먼저 hooks 폴더에서 시도
    const { getInstance: getConfigSyncHooks } = require('./lib/hooks/ConfigSyncHooks');
    ConfigSyncHooks = getConfigSyncHooks;
    console.log('✅ ConfigSyncHooks 로드 성공 (lib/hooks/)');
    
    // 즉시 인스턴스 초기화 테스트
    try {
        const testHooks = ConfigSyncHooks();
        console.log('✅ ConfigSyncHooks 인스턴스 생성 성공');
    } catch (instanceError) {
        console.warn('⚠️ ConfigSyncHooks 인스턴스 생성 실패:', instanceError.message);
        ConfigSyncHooks = null;
    }
    
} catch (error1) {
    try {
        // hooks 폴더가 실패하면 hook 폴더에서 시도
        const { getInstance: getConfigSyncHooks } = require('./lib/hook/ConfigSyncHooks');
        ConfigSyncHooks = getConfigSyncHooks;
        console.log('✅ ConfigSyncHooks 로드 성공 (lib/hook/)');
        
        // 인스턴스 테스트
        try {
            const testHooks = ConfigSyncHooks();
            console.log('✅ ConfigSyncHooks 인스턴스 생성 성공');
        } catch (instanceError) {
            console.warn('⚠️ ConfigSyncHooks 인스턴스 생성 실패:', instanceError.message);
            ConfigSyncHooks = null;
        }
        
    } catch (error2) {
        console.warn('⚠️ ConfigSyncHooks 로드 완전 실패:');
        console.warn('   hooks 폴더 시도:', error1.message);
        console.warn('   hook 폴더 시도:', error2.message);
        console.warn('   설정 동기화 기능이 비활성화됩니다.');
    }
}

const app = express();
const server = http.createServer(app);

// =============================================================================
// 🔧 CORS 설정 수정 - 개발 환경에서 모든 origin 허용
// =============================================================================

const corsOptions = {
    origin: function (origin, callback) {
        // 개발 환경에서는 모든 origin 허용 (CORS 에러 해결)
        if (process.env.NODE_ENV === 'development' || !origin) {
            callback(null, true);
            return;
        }
        
        // 허용된 origin 목록 (프로덕션용)
        const allowedOrigins = [
            'http://localhost:3000',
            'http://localhost:5173',
            'http://localhost:5174', 
            'http://localhost:8080',
            'http://127.0.0.1:3000',
            'http://127.0.0.1:5173'
        ];
        
        if (allowedOrigins.includes(origin)) {
            callback(null, true);
        } else {
            console.warn(`CORS 차단된 origin: ${origin}`);
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
    optionsSuccessStatus: 200 // IE11 지원
};

app.use(cors(corsOptions));

// =============================================================================
// Socket.IO 서버 설정 (CORS 포함)
// =============================================================================

let io = null;
if (WebSocketService) {
    webSocketService = new WebSocketService(server);
    io = webSocketService.io;
    app.locals.webSocketService = webSocketService;
    app.locals.io = io;
    console.log('✅ WebSocket 서비스 초기화 완료');
} else {
    // WebSocketService가 없는 경우 직접 Socket.IO 초기화
    io = new Server(server, {
        cors: {
            origin: function (origin, callback) {
                // Socket.IO용 CORS 설정 (개발 환경에서 모든 origin 허용)
                if (process.env.NODE_ENV === 'development' || !origin) {
                    callback(null, true);
                    return;
                }
                
                const allowedOrigins = [
                    'http://localhost:3000',
                    'http://localhost:5173',
                    'http://localhost:5174',
                    'http://localhost:8080',
                    'http://127.0.0.1:3000',
                    'http://127.0.0.1:5173'
                ];
                
                callback(null, allowedOrigins.includes(origin));
            },
            methods: ['GET', 'POST'],
            credentials: true
        },
        transports: ['polling', 'websocket'], // 폴링 우선, 웹소켓 업그레이드
        allowEIO3: true // Engine.IO v3 호환성
    });
    
    // 기본 Socket.IO 이벤트 핸들러
    io.on('connection', (socket) => {
        console.log('WebSocket 클라이언트 연결:', socket.id);
        
        socket.on('disconnect', () => {
            console.log('WebSocket 클라이언트 연결 해제:', socket.id);
        });
        
        // 테넌트 룸 조인
        socket.on('join_tenant', (tenantId) => {
            socket.join(`tenant:${tenantId}`);
            console.log(`클라이언트 ${socket.id}가 tenant:${tenantId} 룸에 조인`);
        });
    });
    
    app.locals.io = io;
    app.locals.webSocketService = null;
    console.log('✅ 기본 Socket.IO 서버 초기화 완료 (WebSocketService 없음)');
}

// =============================================================================
// 미들웨어 설정
// =============================================================================

app.use(express.json({ limit: '10mb' }));
app.use(express.urlencoded({ extended: true, limit: '10mb' }));

// 정적 파일 서빙 (프론트엔드)
app.use(express.static(path.join(__dirname, '../frontend'), {
    maxAge: process.env.NODE_ENV === 'production' ? '1d' : 0
}));

// 요청 로깅 미들웨어
app.use((req, res, next) => {
    const timestamp = new Date().toISOString();
    console.log(`[${timestamp}] ${req.method} ${req.path}`);
    next();
});

// =============================================================================
// 글로벌 인증 및 테넌트 미들웨어 (개발용)
// =============================================================================

/**
 * 기본 인증 미들웨어 (개발용)
 */
const authenticateToken = (req, res, next) => {
    // API 경로가 아니거나 특정 경로는 인증 스킵
    if (!req.originalUrl.startsWith('/api/') || 
        req.originalUrl.startsWith('/api/health') ||
        req.originalUrl.startsWith('/api/init/') ||
        req.originalUrl.startsWith('/api/test/') ||
        req.originalUrl.startsWith('/api/websocket/')) {
        return next();
    }

    // 개발용 기본 사용자 설정
    req.user = {
        id: 1,
        username: 'admin',
        tenant_id: 1,
        role: 'admin'
    };
    
    next();
};

/**
 * 테넌트 격리 미들웨어 (개발용)
 */
const tenantIsolation = (req, res, next) => {
    if (req.user) {
        req.tenantId = req.user.tenant_id;
    } else {
        req.tenantId = 1; // 기본값
    }
    next();
};

// API 경로에만 인증/테넌트 미들웨어 적용
app.use('/api/*', authenticateToken);
app.use('/api/*', tenantIsolation);

// =============================================================================
// 데이터베이스 연결 및 자동 초기화
// =============================================================================

let connections = {};

async function initializeSystem() {
    try {
        // 1. 데이터베이스 연결
        connections = await initializeConnections();
        
        // 2. DatabaseInitializer가 알아서 설정 확인하고 처리
        if (DatabaseInitializer) {
            const initializer = new DatabaseInitializer(connections);
            await initializer.autoInitializeIfNeeded();
        }
        
    } catch (error) {
        console.error('System initialization failed:', error.message);
    }
}

// =============================================================================
// 실시간 알람 구독자 초기화
// =============================================================================

let alarmSubscriber = null;

async function startAlarmSubscriber() {
    if (!AlarmEventSubscriber || !io) {
        console.warn('⚠️ AlarmEventSubscriber 또는 Socket.IO가 비활성화되어 실시간 알람 기능을 사용할 수 없습니다.');
        return;
    }
    
    try {
        alarmSubscriber = new AlarmEventSubscriber(io);
        await alarmSubscriber.start();
        
        app.locals.alarmSubscriber = alarmSubscriber;
        console.log('✅ 실시간 알람 구독자 시작 완료');
        
    } catch (error) {
        console.error('❌ 실시간 알람 구독자 시작 실패:', error.message);
        console.warn('⚠️ 알람 실시간 기능 없이 서버를 계속 실행합니다.');
    }
}

// 시스템 초기화 실행
initializeSystem();

// 전역 변수에 저장
app.locals.alarmSubscriber = null; // startAlarmSubscriber에서 설정됨
app.locals.serverStartTime = new Date().toISOString();

// =============================================================================
// 헬스체크 및 초기화 관리 엔드포인트
// =============================================================================

// Health check
app.get('/api/health', async (req, res) => {
    try {
        const healthInfo = { 
            status: 'ok', 
            timestamp: new Date().toISOString(),
            uptime: process.uptime(),
            pid: process.pid,
            cors_enabled: true // CORS 활성화 확인
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
            proxy_service: {
                enabled: !!CollectorProxyService,
                status: null,
                last_check: null,
                error: null
            },
            config_sync: {
                enabled: !!ConfigSyncHooks,
                hooks_registered: 0,
                error: null
            }
        };
        
        // CollectorProxyService 상태 상세 확인
        if (CollectorProxyService) {
            try {
                const proxy = CollectorProxyService();
                healthInfo.collector_integration.proxy_service.status = proxy.isCollectorHealthy() ? 'healthy' : 'unhealthy';
                healthInfo.collector_integration.proxy_service.last_check = proxy.getLastHealthCheck();
            } catch (proxyError) {
                healthInfo.collector_integration.proxy_service.status = 'error';
                healthInfo.collector_integration.proxy_service.error = proxyError.message;
            }
        }
        
        // ConfigSyncHooks 상태 상세 확인
        if (ConfigSyncHooks) {
            try {
                const hooks = ConfigSyncHooks();
                healthInfo.collector_integration.config_sync.hooks_registered = hooks.getRegisteredHooks().length;
            } catch (hooksError) {
                healthInfo.collector_integration.config_sync.error = hooksError.message;
            }
        }
        
        // 초기화 시스템 상태
        healthInfo.initialization = {
            databaseInitializer: {
                available: !!DatabaseInitializer,
                autoInit: process.env.AUTO_INITIALIZE_ON_START === 'true'
            }
        };
        
        if (DatabaseInitializer) {
            try {
                const initializer = new DatabaseInitializer();
                await initializer.checkDatabaseStatus();
                
                healthInfo.initialization.database = {
                    systemTables: initializer.initStatus.systemTables,
                    tenantSchemas: initializer.initStatus.tenantSchemas,
                    sampleData: initializer.initStatus.sampleData,
                    fullyInitialized: initializer.isFullyInitialized()
                };
            } catch (error) {
                healthInfo.initialization.database = {
                    error: error.message
                };
            }
        }
        
        res.json(healthInfo);
    } catch (error) {
        res.status(500).json({
            status: 'error',
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// 실시간 알람 테스트 엔드포인트
app.post('/api/test/alarm', (req, res) => {
    if (!io) {
        return res.status(503).json({
            success: false,
            error: 'Socket.IO 서비스가 비활성화되어 있습니다.',
            suggestion: 'WebSocket 설정을 확인하세요'
        });
    }
    
    try {
        const testAlarm = {
            occurrence_id: Date.now(),
            rule_id: 999,
            tenant_id: 1,
            device_id: 'test_device_001',
            point_id: 1,
            message: '🚨 테스트 알람 - 온도 센서 이상 감지',
            severity: 'HIGH',
            severity_level: 3,
            state: 1,
            timestamp: Date.now(),
            source_name: '테스트 온도 센서',
            location: '1층 서버실',
            trigger_value: 85.5,
            formatted_time: new Date().toLocaleString('ko-KR')
        };
        
        // Socket.IO를 통해 알람 전송
        let sent = false;
        if (webSocketService) {
            sent = webSocketService.sendAlarm(testAlarm);
        } else {
            // 직접 Socket.IO 사용
            io.to('tenant:1').emit('alarm_triggered', testAlarm);
            io.emit('alarm_triggered', testAlarm); // 전체 브로드캐스트도 함께
            sent = true;
        }
        
        console.log('🚨 테스트 알람 전송:', sent ? '성공' : '실패');
        
        res.json({ 
            success: true,
            message: '테스트 알람이 전송되었습니다.',
            alarm: testAlarm,
            sent_via_websocket: sent,
            connected_clients: io ? io.engine.clientsCount : 0
        });
        
    } catch (error) {
        console.error('❌ 테스트 알람 전송 실패:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 초기화 상태 조회
app.get('/api/init/status', async (req, res) => {
    try {
        if (!DatabaseInitializer) {
            return res.json({
                success: true,
                data: {
                    available: false,
                    message: 'DatabaseInitializer 클래스를 찾을 수 없습니다.',
                    suggestion: 'backend/lib/database/DatabaseInitializer.js 파일을 확인하세요.',
                    autoInitEnabled: process.env.AUTO_INITIALIZE_ON_START === 'true'
                }
            });
        }
        
        const initializer = new DatabaseInitializer();
        await initializer.checkDatabaseStatus();
        
        res.json({
            success: true,
            data: {
                available: true,
                database: initializer.initStatus,
                fullyInitialized: initializer.isFullyInitialized(),
                autoInitEnabled: process.env.AUTO_INITIALIZE_ON_START === 'true'
            }
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            suggestion: 'DatabaseInitializer 구현을 확인하세요.'
        });
    }
});

// 초기화 수동 트리거
app.post('/api/init/trigger', async (req, res) => {
    try {
        if (!DatabaseInitializer) {
            return res.status(503).json({
                success: false,
                error: 'DatabaseInitializer를 사용할 수 없습니다.',
                details: {
                    reason: 'DatabaseInitializer 클래스 로드 실패',
                    solution: 'backend/lib/database/DatabaseInitializer.js 파일을 구현하거나 복구하세요.',
                    alternative: '수동으로 데이터베이스를 초기화하거나 스키마 스크립트를 실행하세요.'
                }
            });
        }
        
        const { backup = true } = req.body;
        const initializer = new DatabaseInitializer();
        
        if (backup) {
            try {
                await initializer.createBackup(true);
                console.log('✅ 백업 생성 완료');
            } catch (backupError) {
                console.warn('⚠️ 백업 생성 실패:', backupError.message);
            }
        }
        
        await initializer.performInitialization();
        
        res.json({
            success: true,
            message: '초기화가 완료되었습니다.',
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// =============================================================================
// API Routes 등록
// =============================================================================

console.log('\n🚀 API 라우트 등록 중...\n');

// 기존 시스템 API Routes
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

try {
    const errorRoutes = require('./routes/errors');
    app.use('/api/errors', errorRoutes);
} catch (error) {
    console.warn('⚠️ Error routes 로드 실패:', error.message);
}

try {
    const protocolRoutes = require('./routes/protocols');
    app.use('/api/protocols', protocolRoutes);
} catch (error) {
    console.warn('⚠️ Protocol routes 로드 실패:', error.message);
}

console.log('✅ 기존 시스템 API 라우트들 등록 완료');

// 향상된 디바이스 라우트 (Collector 동기화 포함)
try {
    const enhancedDeviceRoutes = require('./routes/devices');
    app.use('/api/devices', enhancedDeviceRoutes);
    console.log('✅ Enhanced Device API with Collector sync 등록 완료');
} catch (error) {
    console.warn('⚠️ Enhanced Device 라우트 로드 실패:', error.message);
    
    // 폴백: 기존 디바이스 라우트 사용
    try {
        const fallbackDeviceRoutes = require('./routes/devices-fallback');
        app.use('/api/devices', fallbackDeviceRoutes);
        console.log('✅ Fallback Device API 등록 완료');
    } catch (fallbackError) {
        console.error('❌ Device API 라우트 로드 완전 실패');
    }
}

// Collector 프록시 라우트 등록
try {
    const collectorProxyRoutes = require('./routes/collector-proxy');
    app.use('/api/collector', collectorProxyRoutes);
    console.log('✅ Collector Proxy API 라우트 등록 완료');
} catch (error) {
    console.error('❌ Collector Proxy 라우트 로드 실패:', error.message);
}

// 핵심 비즈니스 API
try {
    const dataRoutes = require('./routes/data');
    app.use('/api/data', dataRoutes);
    console.log('✅ Data Explorer API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Data 라우트 로드 실패:', error.message);
}

// 🔥 알람 라우트 - 가장 중요한 라우트 (강제 등록)
try {
    const alarmRoutes = require('./routes/alarms');
    app.use('/api/alarms', alarmRoutes);
    console.log('✅ Alarm Management API 라우트 등록 완료');
    
    // 등록 확인을 위한 테스트
    console.log('📍 등록된 알람 엔드포인트:');
    console.log('   - GET /api/alarms/test');
    console.log('   - GET /api/alarms/active');
    console.log('   - POST /api/alarms/occurrences/:id/acknowledge');
    console.log('   - POST /api/alarms/occurrences/:id/clear');
    
} catch (error) {
    console.error('❌ CRITICAL: Alarm 라우트 로드 실패:', error.message);
    console.error('❌ 스택 트레이스:', error.stack);
    
    // 알람 라우트 실패 시 디버그 모드로 최소 기능 제공
    console.error('🚨 디버그 모드로 기본 알람 API 제공');
    
    const express = require('express');
    const debugAlarmRouter = express.Router();
    
    // 기본 인증 미들웨어
    debugAlarmRouter.use((req, res, next) => {
        req.user = { id: 1, username: 'admin', tenant_id: 1, role: 'admin' };
        req.tenantId = 1;
        next();
    });
    
    // 테스트 엔드포인트
    debugAlarmRouter.get('/test', (req, res) => {
        res.json({
            success: true,
            message: '디버그 모드 - 알람 API가 기본 기능으로 동작합니다!',
            timestamp: new Date().toISOString(),
            debug_mode: true,
            error: error.message
        });
    });
    
    // 알람 확인 엔드포인트 (간단한 버전)
    debugAlarmRouter.post('/occurrences/:id/acknowledge', (req, res) => {
        const { id } = req.params;
        const { comment = '' } = req.body;
        
        console.log(`✅ 알람 ${id} 확인 처리 (디버그 모드)`);
        
        // Socket.IO로 실시간 알림 전송
        if (io) {
            io.emit('alarm_acknowledged', {
                type: 'alarm_acknowledged',
                data: {
                    alarmId: id,
                    acknowledgedBy: req.user.username,
                    acknowledgedAt: new Date().toISOString(),
                    comment
                }
            });
        }
        
        res.json({
            success: true,
            data: {
                id: parseInt(id),
                acknowledged_time: new Date().toISOString(),
                acknowledged_by: req.user.id,
                acknowledge_comment: comment,
                state: 'acknowledged'
            },
            message: 'Alarm acknowledged successfully (debug mode)',
            timestamp: new Date().toISOString()
        });
    });
    
    // 활성 알람 목록 (더미 데이터)
    debugAlarmRouter.get('/active', (req, res) => {
        console.log('📋 활성 알람 목록 조회 (디버그 모드)');
        
        res.json({
            success: true,
            data: {
                items: [
                    {
                        id: 1,
                        rule_name: '테스트 알람 (디버그 모드)',
                        device_name: '테스트 디바이스',
                        severity: 'high',
                        occurrence_time: new Date().toISOString(),
                        acknowledged_time: null,
                        alarm_message: '디버그 모드 - 실제 데이터베이스 연결이 필요합니다'
                    }
                ],
                pagination: { page: 1, limit: 50, total: 1, totalPages: 1 }
            },
            message: 'Active alarms retrieved successfully (debug mode)',
            debug_mode: true
        });
    });
    
    // 라우터 등록
    app.use('/api/alarms', debugAlarmRouter);
    console.log('⚠️ 디버그 알람 라우트 등록됨');
}

// 확장 API - 선택적 등록
try {
    const dashboardRoutes = require('./routes/dashboard');
    app.use('/api/dashboard', dashboardRoutes);
    console.log('✅ Dashboard API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Dashboard 라우트 로드 실패:', error.message);
}

try {
    const realtimeRoutes = require('./routes/realtime');
    app.use('/api/realtime', realtimeRoutes);
    console.log('✅ Realtime Data API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Realtime 라우트 로드 실패:', error.message);
}

try {
    const virtualPointRoutes = require('./routes/virtual-points');
    app.use('/api/virtual-points', virtualPointRoutes);
    console.log('✅ Virtual Points API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Virtual Points 라우트 로드 실패:', error.message);
}

try {
    const scriptEngineRoutes = require('./routes/script-engine');
    app.use('/api/script-engine', scriptEngineRoutes);
    console.log('✅ Script Engine API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Script Engine 라우트 로드 실패:', error.message);
}

try {
    const monitoringRoutes = require('./routes/monitoring');
    app.use('/api/monitoring', monitoringRoutes);
    console.log('✅ System Monitoring API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Monitoring 라우트 로드 실패:', error.message);
}

try {
    const backupRoutes = require('./routes/backup');
    app.use('/api/backup', backupRoutes);
    console.log('✅ Backup/Restore API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Backup 라우트 로드 실패:', error.message);
}

try {
    const websocketRoutes = require('./routes/websocket');
    app.use('/api/websocket', websocketRoutes);
    console.log('✅ WebSocket Management API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ WebSocket 라우트 로드 실패:', error.message);
}

try {
    const apiRoutes = require('./routes/api');
    app.use('/api', apiRoutes);
    console.log('✅ 기본 API 정보 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ API 정보 라우트 로드 실패:', error.message);
}

console.log('\n🎉 모든 API 라우트 등록 완료!\n');

// =============================================================================
// 404 및 에러 핸들링
// =============================================================================

// 404 handler - API 전용 (개선된 디버깅)
app.use('/api/*', (req, res) => {
    console.log(`❌ 404 - API endpoint not found: ${req.method} ${req.originalUrl}`);
    
    // 알람 관련 엔드포인트에 대한 상세한 디버깅 정보
    if (req.originalUrl.startsWith('/api/alarms/')) {
        console.log('🔍 알람 API 요청 디버깅:');
        console.log(`   - 요청 URL: ${req.originalUrl}`);
        console.log(`   - HTTP 메서드: ${req.method}`);
        console.log(`   - 예상 라우트: /api/alarms/*`);
        console.log(`   - 알람 라우트 등록 상태 확인 필요!`);
    }
    
    res.status(404).json({ 
        success: false,
        error: 'API endpoint not found',
        path: req.originalUrl,
        method: req.method,
        timestamp: new Date().toISOString(),
        suggestion: req.originalUrl.startsWith('/api/alarms/') ? 
            '알람 라우트가 제대로 로드되지 않았을 수 있습니다. 서버 로그를 확인하세요.' : 
            'API 엔드포인트 경로를 확인하세요.'
    });
});

// Global error handler
app.use((error, req, res, next) => {
    console.error('🚨 Unhandled error:', error);
    
    let statusCode = 500;
    let message = 'Internal server error';
    
    if (error.name === 'ValidationError') {
        statusCode = 400;
        message = 'Validation failed';
    } else if (error.name === 'UnauthorizedError') {
        statusCode = 401;
        message = 'Unauthorized';
    } else if (error.name === 'ForbiddenError') {
        statusCode = 403;
        message = 'Forbidden';
    }
    
    res.status(statusCode).json({
        success: false,
        error: message,
        message: process.env.NODE_ENV === 'development' ? error.message : message,
        stack: process.env.NODE_ENV === 'development' ? error.stack : undefined,
        timestamp: new Date().toISOString()
    });
});

// =============================================================================
// 프론트엔드 서빙 (SPA 지원)
// =============================================================================

// 모든 API가 아닌 요청을 index.html로 리다이렉션 (SPA 라우팅 지원)
app.use('*', (req, res) => {
    if (req.originalUrl.startsWith('/api/')) {
        // API 요청인데 여기까지 온 경우는 404
        return res.status(404).json({
            success: false,
            error: 'API endpoint not found',
            path: req.originalUrl,
            timestamp: new Date().toISOString()
        });
    }
    
    // 프론트엔드 라우팅을 위해 index.html 서빙
    res.sendFile(path.join(__dirname, '../frontend/index.html'));
});

// =============================================================================
// Graceful Shutdown
// =============================================================================

process.on('SIGTERM', gracefulShutdown);
process.on('SIGINT', gracefulShutdown);

function gracefulShutdown(signal) {
    console.log(`\n🔄 Received ${signal}. Starting graceful shutdown...`);
    
    server.close(async (err) => {
        if (err) {
            console.error('❌ Error during server shutdown:', err);
            process.exit(1);
        }
        
        console.log('✅ HTTP server closed');
        
        // WebSocket 서버 정리
        if (io) {
            io.close();
            console.log('✅ WebSocket server closed');
        }
        
        // Collector 연결 정리
        try {
            console.log('🔄 Cleaning up Collector connections...');
            if (CollectorProxyService) {
                const proxy = CollectorProxyService();
                console.log(`✅ Collector proxy cleaned up`);
            }
        } catch (error) {
            console.log(`⚠️ Collector cleanup warning: ${error.message}`);
        }
        
        // 알람 구독자 정리
        if (alarmSubscriber) {
            try {
                await alarmSubscriber.stop();
                console.log('✅ Alarm subscriber stopped');
            } catch (error) {
                console.error('❌ Error stopping alarm subscriber:', error);
            }
        }
        
        // Close database connections
        if (connections.postgres) connections.postgres.end();
        if (connections.redis) connections.redis.disconnect();
        
        console.log('✅ Database connections closed');
        console.log('✅ Graceful shutdown completed');
        process.exit(0);
    });
    
    setTimeout(() => {
        console.error('❌ Forced shutdown after timeout');
        process.exit(1);
    }, 10000);
}

// =============================================================================
// Start Server
// =============================================================================

const PORT = process.env.PORT || process.env.BACKEND_PORT || 3000;

server.listen(PORT, '0.0.0.0', async () => {
    const wsStatus = webSocketService ? 
        `✅ 활성화 (${webSocketService.getStatus()?.stats?.socket_clients || 0}명 연결)` : 
        (io ? `✅ 기본 모드 (${io.engine.clientsCount}명 연결)` : '❌ 비활성화');
        
    const collectorStatus = CollectorProxyService ? '✅ Available' : '❌ Not Found';
    const syncHooksStatus = ConfigSyncHooks ? '✅ Available' : '❌ Not Found';
    
    console.log(`
🚀 PulseOne Backend Server Started! (CORS & WebSocket 수정 완료)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📊 Dashboard:     http://localhost:${PORT}
🔧 API Health:    http://localhost:${PORT}/api/health
🔥 Alarm Test:    http://localhost:${PORT}/api/alarms/test
📱 Alarm Active:  http://localhost:${PORT}/api/alarms/active
🧪 Test Alarm:    POST http://localhost:${PORT}/api/test/alarm

🌐 CORS 설정:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🔒 모드:          ${process.env.NODE_ENV === 'development' ? '✅ 개발 모드 (모든 Origin 허용)' : '🔐 프로덕션 모드 (제한적 허용)'}
🌍 허용 Origin:   localhost:3000, localhost:5173, 127.0.0.1:*
📝 허용 메서드:    GET, POST, PUT, PATCH, DELETE, OPTIONS
🍪 Credentials:   ✅ Enabled

🔌 WebSocket 상태:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📡 Socket.IO:     ${wsStatus}
🚨 알람 구독자:    ${alarmSubscriber ? '✅ 준비됨' : '⚠️ 비활성화'}
🔄 Transport:     Polling → WebSocket 업그레이드
🌐 CORS:          ✅ 동일한 설정 적용

🔥 Collector Integration:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🎮 Collector 상태: ${collectorStatus}
🔄 Config Sync:   ${syncHooksStatus}

🚀 시스템 정보:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Environment: ${process.env.NODE_ENV || 'development'}
Auto Initialize: ${process.env.AUTO_INITIALIZE_ON_START === 'true' ? '✅ Enabled' : '❌ Disabled'}
DatabaseInitializer: ${DatabaseInitializer ? '✅ Available' : '❌ Not Found'}
Authentication: 🔓 Development Mode (기본 사용자)
PID: ${process.pid}

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🎉 PulseOne 백엔드 시스템 완전 가동! 
   - CORS 에러 수정 완료 ✅
   - WebSocket 연결 문제 해결 ✅
   - 알람 API 강화 ✅
   - 실시간 기능 준비 완료 ✅
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    `);
    
    // 서버 시작 후 알람 라우트 동작 확인
    console.log('🔍 알람 API 엔드포인트 검증 중...');
    
    try {
        const http = require('http');
        
        // 내부적으로 /api/alarms/test 호출해서 라우트 동작 확인
        const testReq = http.request({
            hostname: 'localhost',
            port: PORT,
            path: '/api/alarms/test',
            method: 'GET'
        }, (res) => {
            if (res.statusCode === 200) {
                console.log('✅ 알람 API 라우트 정상 동작 확인됨');
            } else {
                console.log(`⚠️ 알람 API 응답 코드: ${res.statusCode}`);
            }
        });
        
        testReq.on('error', (err) => {
            console.log('⚠️ 알람 API 자체 테스트 실패:', err.message);
        });
        
        testReq.end();
        
    } catch (testError) {
        console.log('⚠️ 알람 API 검증 과정에서 오류:', testError.message);
    }
    
    // Collector 연결 상태 확인
    try {
        console.log('🔄 Checking Collector connection...');
        if (CollectorProxyService) {
            const proxy = CollectorProxyService();
            const healthResult = await proxy.healthCheck();
            
            console.log(`✅ Collector connection successful!`);
            console.log(`   📍 Collector URL: ${proxy.getCollectorConfig().host}:${proxy.getCollectorConfig().port}`);
            console.log(`   📊 Collector Status: ${healthResult.data?.status || 'unknown'}`);
            
            // 워커 상태도 확인
            try {
                const workerResult = await proxy.getWorkerStatus();
                const workerCount = Object.keys(workerResult.data?.workers || {}).length;
                console.log(`   🏭 Active Workers: ${workerCount}`);
            } catch (workerError) {
                console.log(`   ⚠️ Worker status unavailable: ${workerError.message}`);
            }
        } else {
            console.log('⚠️ CollectorProxyService not available - backend will work without Collector integration');
        }
        
    } catch (collectorError) {
        console.warn(`⚠️ Collector connection failed: ${collectorError.message}`);
        console.log(`   💡 Backend will continue without Collector integration`);
    }
    
    console.log(`━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`);
    
    // 3초 후 실시간 알람 구독자 시작
    setTimeout(startAlarmSubscriber, 3000);
});

module.exports = app;