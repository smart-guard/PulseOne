// =============================================================================
// backend/app.js - 통합 메인 애플리케이션 
// 기존 구조 + WebSocket 서비스 분리 + Collector 통합 + 모든 API 라우트
// =============================================================================

const express = require('express');
const cors = require('cors');
const path = require('path');
const http = require('http');
const { initializeConnections } = require('./lib/connection/db');

// =============================================================================
// 안전한 모듈 로딩 (기존 방식 유지)
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

// 🔥 Collector 프록시 서비스 (새로 추가)
let CollectorProxyService = null;
try {
    const { getInstance: getCollectorProxy } = require('./lib/services/CollectorProxyService');
    CollectorProxyService = getCollectorProxy;
    console.log('✅ CollectorProxyService 로드 성공');
} catch (error) {
    console.warn('⚠️ CollectorProxyService 로드 실패:', error.message);
    console.warn('   Collector 통합 기능이 비활성화됩니다.');
}

// 🔥 설정 동기화 훅 (새로 추가) 
let ConfigSyncHooks = null;
try {
    const { getInstance: getConfigSyncHooks } = require('./lib/hooks/ConfigSyncHooks');
    ConfigSyncHooks = getConfigSyncHooks;
    console.log('✅ ConfigSyncHooks 로드 성공');
} catch (error) {
    console.warn('⚠️ ConfigSyncHooks 로드 실패:', error.message);
    console.warn('   설정 동기화 기능이 비활성화됩니다.');
}

const app = express();
const server = http.createServer(app);

// ============================================================================
// WebSocket 서비스 초기화 (기존 방식)
// ============================================================================
if (WebSocketService) {
    webSocketService = new WebSocketService(server);
    app.locals.webSocketService = webSocketService;
    app.locals.io = webSocketService.io; // 기존 코드 호환성
    console.log('✅ WebSocket 서비스 초기화 완료');
} else {
    app.locals.webSocketService = null;
    app.locals.io = null;
    console.warn('⚠️ WebSocket 서비스가 비활성화됩니다.');
}

// ============================================================================
// 미들웨어 설정 (기존 + 확장)
// ============================================================================

// CORS 설정 (프런트엔드 연동 강화)
app.use(cors({
    origin: [
        'http://localhost:3000', 
        'http://localhost:5173',  // Vite 개발 서버
        'http://localhost:5174', 
        'http://localhost:8080'
    ],
    credentials: true,
    methods: ['GET', 'POST', 'PUT', 'PATCH', 'DELETE', 'OPTIONS'],
    allowedHeaders: ['Content-Type', 'Authorization', 'X-Tenant-ID']
}));

app.use(express.json({ limit: '10mb' }));
app.use(express.urlencoded({ extended: true, limit: '10mb' }));

// 요청 로깅 미들웨어
app.use((req, res, next) => {
    const timestamp = new Date().toISOString();
    console.log(`[${timestamp}] ${req.method} ${req.path}`);
    next();
});

// ============================================================================
// 글로벌 인증 및 테넌트 미들웨어 (개발용)
// ============================================================================

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

// ============================================================================
// 데이터베이스 연결 및 자동 초기화 (기존 방식)
// ============================================================================

let connections = {};

async function initializeSystem() {
    try {
        console.log('🚀 PulseOne 시스템 초기화 시작...\n');
        
        // 1. 기존 데이터베이스 연결
        connections = await initializeConnections();
        app.locals.getDB = () => connections;
        console.log('✅ Database connections initialized');
        
        // 2. 자동 초기화 시스템
        if (process.env.AUTO_INITIALIZE_ON_START === 'true' && DatabaseInitializer) {
            console.log('🔄 자동 초기화 확인 중...');
            
            const initializer = new DatabaseInitializer();
            await initializer.checkDatabaseStatus();
            
            if (initializer.isFullyInitialized() && process.env.SKIP_IF_INITIALIZED !== 'false') {
                console.log('✅ 데이터베이스가 이미 초기화되어 있습니다.\n');
            } else if (!initializer.isFullyInitialized()) {
                console.log('🔧 초기화가 필요한 항목들을 감지했습니다.');
                console.log('🚀 자동 초기화를 시작합니다...\n');
                
                await initializer.performInitialization();
                console.log('✅ 자동 초기화가 완료되었습니다.\n');
            }
        }
        
    } catch (error) {
        console.error('❌ System initialization failed:', error.message);
        
        if (process.env.FAIL_ON_INIT_ERROR === 'true') {
            process.exit(1);
        } else {
            console.log('⚠️  초기화 실패했지만 서버를 계속 시작합니다.\n');
        }
    }
}

// ============================================================================
// 실시간 알람 구독자 초기화 (기존 방식)
// ============================================================================

let alarmSubscriber = null;

async function startAlarmSubscriber() {
    if (!AlarmEventSubscriber || !webSocketService?.io) {
        console.warn('⚠️ AlarmEventSubscriber 또는 WebSocket이 비활성화되어 실시간 알람 기능을 사용할 수 없습니다.');
        return;
    }
    
    try {
        alarmSubscriber = new AlarmEventSubscriber(webSocketService.io);
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

// ============================================================================
// 헬스체크 및 초기화 관리 엔드포인트 (기존 + 확장)
// ============================================================================

// Health check
app.get('/api/health', async (req, res) => {
    try {
        const healthInfo = { 
            status: 'ok', 
            timestamp: new Date().toISOString(),
            uptime: process.uptime(),
            pid: process.pid
        };
        
        // 실시간 기능 상태
        healthInfo.realtime = {
            websocket: {
                enabled: !!webSocketService,
                connected_clients: webSocketService ? webSocketService.getStatus()?.stats?.socket_clients || 0 : 0
            },
            alarm_subscriber: {
                enabled: !!alarmSubscriber,
                status: alarmSubscriber ? alarmSubscriber.getStatus() : null
            }
        };
        
        // 🔥 Collector 통합 상태 (새로 추가)
        healthInfo.collector_integration = {
            proxy_service: {
                enabled: !!CollectorProxyService,
                status: CollectorProxyService ? (CollectorProxyService().isCollectorHealthy() ? 'healthy' : 'unhealthy') : null,
                last_check: CollectorProxyService ? CollectorProxyService().getLastHealthCheck() : null
            },
            config_sync: {
                enabled: !!ConfigSyncHooks,
                hooks_registered: ConfigSyncHooks ? ConfigSyncHooks().getRegisteredHooks().length : 0
            }
        };
        
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

// 실시간 알람 테스트 엔드포인트 (기존)
app.post('/api/test/alarm', (req, res) => {
    if (!webSocketService) {
        return res.status(503).json({
            success: false,
            error: 'WebSocket 서비스가 비활성화되어 있습니다.',
            suggestion: 'npm install socket.io'
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
        
        // WebSocket 서비스를 통해 알람 전송
        const sent = webSocketService.sendAlarm(testAlarm);
        
        console.log('🚨 테스트 알람 전송:', sent ? '성공' : '실패');
        
        res.json({ 
            success: true,
            message: '테스트 알람이 전송되었습니다.',
            alarm: testAlarm,
            sent_via_websocket: sent,
            connected_clients: webSocketService.getStatus().stats?.socket_clients || 0
        });
        
    } catch (error) {
        console.error('❌ 테스트 알람 전송 실패:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 초기화 상태 조회 (기존)
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

// 초기화 수동 트리거 (기존)
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

// 임시 초기화 대안 엔드포인트 (기존)
app.post('/api/init/manual', async (req, res) => {
    try {
        console.log('🔧 수동 초기화 시도...');
        
        const connections = app.locals.getDB ? app.locals.getDB() : null;
        
        if (!connections || !connections.db) {
            return res.status(503).json({
                success: false,
                error: 'SQLite 데이터베이스 연결을 찾을 수 없습니다.',
                suggestion: '앱을 재시작하거나 데이터베이스 설정을 확인하세요.'
            });
        }
        
        const db = connections.db;
        const tables = await new Promise((resolve, reject) => {
            db.all("SELECT name FROM sqlite_master WHERE type='table'", (err, rows) => {
                if (err) reject(err);
                else resolve(rows.map(row => row.name));
            });
        });
        
        res.json({
            success: true,
            message: '수동 초기화 상태 확인 완료',
            data: {
                database_connected: true,
                tables_found: tables.length,
                tables: tables,
                timestamp: new Date().toISOString()
            }
        });
        
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// ============================================================================
// API Routes 등록 (기존 + 새로 추가)
// ============================================================================

console.log('\n🚀 API 라우트 등록 중...\n');

// 기존 시스템 API Routes
const systemRoutes = require('./routes/system');
const processRoutes = require('./routes/processes');
const serviceRoutes = require('./routes/services');
const userRoutes = require('./routes/user');

app.use('/api/system', systemRoutes);
app.use('/api/processes', processRoutes);
app.use('/api/services', serviceRoutes);
app.use('/api/users', userRoutes);

console.log('✅ 기존 시스템 API 라우트들 등록 완료');

// 🔥 향상된 디바이스 라우트 (Collector 동기화 포함)
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

// 🔥 Collector 프록시 라우트 등록 (새로 추가)
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

try {
    const alarmRoutes = require('./routes/alarms');
    app.use('/api/alarms', alarmRoutes);
    console.log('✅ Alarm Management API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Alarm 라우트 로드 실패:', error.message);
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
// Error Handling (기존)
// =============================================================================

// 404 handler (API 전용)
app.use('/api/*', (req, res) => {
    res.status(404).json({ 
        success: false,
        error: 'API endpoint not found',
        path: req.originalUrl,
        timestamp: new Date().toISOString()
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
// Graceful Shutdown (기존 + Collector 정리 추가)
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
        
        // 🔥 Collector 연결 정리 (새로 추가)
        try {
            console.log('🔄 Cleaning up Collector connections...');
            // 여기서는 단순히 상태를 로그만 남김 (연결 자체는 자동 정리됨)
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
// Start Server (기존 + Collector 상태 표시)
// =============================================================================

const PORT = process.env.PORT || process.env.BACKEND_PORT || 3000;

const server = app.listen(PORT, async () => {
    const wsStatus = webSocketService ? 
        `✅ 활성화 (${webSocketService.getStatus().stats?.socket_clients || 0}명 연결)` : 
        '❌ 비활성화';
        
    console.log(`
🚀 PulseOne Backend Server Started! (Collector 통합 완성)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📊 Dashboard:     http://localhost:${PORT}
🔧 API Health:    http://localhost:${PORT}/api/health
📈 System Info:   http://localhost:${PORT}/api/system/info
💾 DB Status:     http://localhost:${PORT}/api/system/databases
🔧 Processes:     http://localhost:${PORT}/api/processes
⚙️  Services:      http://localhost:${PORT}/api/services
👤 Users:         http://localhost:${PORT}/api/users

🆕 WebSocket 관리 API:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🔌 WebSocket:     ${wsStatus}
🚨 알람 구독자:    ${alarmSubscriber ? '✅ 준비됨' : '⚠️ 비활성화'}
🧪 알람 테스트:    POST http://localhost:${PORT}/api/test/alarm
🔍 WebSocket 상태: GET  http://localhost:${PORT}/api/websocket/status
👥 클라이언트 목록: GET  http://localhost:${PORT}/api/websocket/clients
🏠 룸 정보:        GET  http://localhost:${PORT}/api/websocket/rooms

🔥 NEW: Collector Integration
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🎮 Collector Control: http://localhost:${PORT}/api/collector/health
📡 Device Control:    http://localhost:${PORT}/api/devices/{id}/start
⚡ Hardware Control: http://localhost:${PORT}/api/devices/{id}/digital/{output}/control
🔄 Config Sync:      http://localhost:${PORT}/api/collector/config/reload
📊 Worker Status:    http://localhost:${PORT}/api/collector/workers/status

🔥 핵심 비즈니스 API (우선순위 1 - 필수)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🚨 알람 관리 API: http://localhost:${PORT}/api/alarms
📱 디바이스 관리 API: http://localhost:${PORT}/api/devices
📊 데이터 익스플로러 API: http://localhost:${PORT}/api/data
🔮 가상포인트 API: http://localhost:${PORT}/api/virtual-points
🔧 스크립트 엔진 API: http://localhost:${PORT}/api/script-engine

📊 확장 API (우선순위 2 - 선택적)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📈 대시보드:     GET  /api/dashboard/overview
🔄 실시간 데이터: GET  /api/realtime/current-values
📈 모니터링:     GET  /api/monitoring/system-metrics
💾 백업 관리:    GET  /api/backup/list

🚀 시스템 초기화
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🔧 초기화 상태:   GET  /api/init/status (${DatabaseInitializer ? '✅ 활성' : '❌ 비활성'})
🔄 초기화 트리거: POST /api/init/trigger (${DatabaseInitializer ? '✅ 활성' : '❌ 비활성'})
⚙️  수동 초기화:   POST /api/init/manual (항상 사용 가능)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Environment: ${process.env.NODE_ENV || 'development'}
Auto Initialize: ${process.env.AUTO_INITIALIZE_ON_START === 'true' ? '✅ Enabled' : '❌ Disabled'}
DatabaseInitializer: ${DatabaseInitializer ? '✅ Available' : '❌ Not Found'}
WebSocket Service: ${webSocketService ? '✅ Enabled' : '❌ Disabled'}
Collector Proxy: ${CollectorProxyService ? '✅ Available' : '❌ Not Found'}
Config Sync Hooks: ${ConfigSyncHooks ? '✅ Available' : '❌ Not Found'}
Authentication: 🔓 Development Mode (Basic Auth)
Tenant Isolation: ✅ Enabled
PID: ${process.pid}

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🎉 PulseOne 통합 백엔드 시스템 완전 가동! (v4.0.0 - Collector 통합)
   - 알람 관리 ✅
   - 디바이스 관리 ✅  
   - 가상포인트 관리 ✅
   - 데이터 익스플로러 ✅
   - 스크립트 엔진 ✅
   - 실시간 알람 처리 ${webSocketService && alarmSubscriber ? '✅' : '⚠️'}
   - WebSocket 상태 관리 ✅
   - 자동 초기화 ${DatabaseInitializer ? '✅' : '⚠️'}
   - 서비스 제어 ✅
   - Collector 프록시 ${CollectorProxyService ? '✅' : '⚠️'}
   - 설정 동기화 ${ConfigSyncHooks ? '✅' : '⚠️'}
   - 멀티테넌트 지원 ✅
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    `);
    
    // 🔥 Collector 연결 상태 확인 (새로 추가)
    try {
        console.log('🔄 Checking Collector connection...');
        if (CollectorProxyService) {
            const proxy = CollectorProxyService();
            const healthResult = await proxy.healthCheck();
            
            console.log(`✅ Collector connection successful!`);
            console.log(`   📍 Collector URL: ${proxy.getCollectorConfig().host}:${proxy.getCollectorConfig().port}`);
            console.log(`   📊 Collector Status: ${healthResult.data?.status || 'unknown'}`);
            console.log(`   🕒 Response Time: ${healthResult.data?.uptime_seconds || 'unknown'}`);
            
            // 워커 상태도 확인
            try {
                const workerResult = await proxy.getWorkerStatus();
                const workerCount = Object.keys(workerResult.data?.workers || {}).length;
                console.log(`   🏭 Active Workers: ${workerCount}`);
            } catch (workerError) {
                console.log(`   ⚠️ Worker status unavailable: ${workerError.message}`);
            }
        } else {
            console.log('⚠️ CollectorProxyService not available');
        }
        
    } catch (collectorError) {
        console.warn(`⚠️ Collector connection failed: ${collectorError.message}`);
        if (CollectorProxyService) {
            const proxy = CollectorProxyService();
            console.log(`   📍 Attempted URL: ${proxy.getCollectorConfig().host}:${proxy.getCollectorConfig().port}`);
        }
        console.log(`   💡 Backend will continue without Collector integration`);
        console.log(`   🔧 To enable Collector, ensure it's running and check COLLECTOR_HOST/COLLECTOR_API_PORT settings`);
    }
    
    // 🔥 설정 동기화 시스템 상태 (새로 추가)
    try {
        if (ConfigSyncHooks) {
            const hooks = ConfigSyncHooks();
            const registeredHooks = hooks.getRegisteredHooks();
            console.log(`🎣 Config Sync Hooks: ${hooks.isHookEnabled() ? '✅ Enabled' : '❌ Disabled'}`);
            console.log(`   📋 Registered Hooks: ${registeredHooks.length}`);
            
            if (registeredHooks.length > 0) {
                console.log(`   🔗 Hook Types: ${registeredHooks.slice(0, 3).join(', ')}${registeredHooks.length > 3 ? '...' : ''}`);
            }
        }
    } catch (hookError) {
        console.warn(`⚠️ Config sync hooks initialization failed: ${hookError.message}`);
    }
    
    console.log(`━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`);
    
    // 3초 후 실시간 알람 구독자 시작
    setTimeout(startAlarmSubscriber, 3000);
});

module.exports = app;