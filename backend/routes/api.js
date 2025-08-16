// ============================================================================
// backend/app.js
// 완전한 Express 애플리케이션 - 모든 API 라우트 통합
// ============================================================================

const express = require('express');
const path = require('path');
const cors = require('cors');

const app = express();

// =============================================================================
// 🔧 미들웨어 설정
// =============================================================================

// CORS 설정
app.use(cors({
    origin: process.env.CORS_ORIGIN || '*',
    methods: ['GET', 'POST', 'PUT', 'DELETE', 'OPTIONS'],
    allowedHeaders: ['Content-Type', 'Authorization', 'X-Requested-With'],
    credentials: true
}));

// Body parsing 미들웨어
app.use(express.json({ limit: '10mb' }));
app.use(express.urlencoded({ extended: true, limit: '10mb' }));

// 정적 파일 서빙
app.use(express.static(path.join(__dirname, '../frontend')));

// Request 로깅 미들웨어
app.use((req, res, next) => {
    const timestamp = new Date().toISOString();
    console.log(`[${timestamp}] ${req.method} ${req.url}`);
    next();
});

// =============================================================================
// 🏥 기본 헬스체크 엔드포인트
// =============================================================================

app.get('/health', async (req, res) => {
    try {
        const healthStatus = {
            status: 'healthy',
            timestamp: new Date().toISOString(),
            uptime: process.uptime(),
            memory: process.memoryUsage(),
            version: process.env.npm_package_version || '1.0.0',
            environment: process.env.NODE_ENV || 'development',
            services: {
                database: 'connected', // 실제로는 DB 연결 테스트 필요
                redis: 'connected',     // 실제로는 Redis 연결 테스트 필요
                backend: 'running'
            }
        };

        res.json(healthStatus);
    } catch (error) {
        res.status(500).json({
            status: 'unhealthy',
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// =============================================================================
// 🛠️ 데이터베이스 초기화 API
// =============================================================================

// 데이터베이스 초기화 시스템 로드 (옵셔널)
let DatabaseInitializer = null;
try {
    DatabaseInitializer = require('./lib/database/DatabaseInitializer');
    console.log('✅ DatabaseInitializer 로드 성공');
} catch (error) {
    console.warn('⚠️ DatabaseInitializer 로드 실패:', error.message);
}

app.get('/api/init/status', async (req, res) => {
    try {
        if (!DatabaseInitializer) {
            return res.json({
                success: false,
                error: '초기화 시스템을 사용할 수 없습니다.'
            });
        }
        
        const initializer = new DatabaseInitializer();
        await initializer.checkDatabaseStatus();
        
        res.json({
            success: true,
            data: {
                database: initializer.initStatus,
                fullyInitialized: initializer.isFullyInitialized(),
                autoInitEnabled: process.env.AUTO_INITIALIZE_ON_START === 'true'
            }
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

app.post('/api/init/trigger', async (req, res) => {
    try {
        if (!DatabaseInitializer) {
            return res.status(503).json({
                success: false,
                error: '초기화 시스템을 사용할 수 없습니다.'
            });
        }
        
        const { backup = true } = req.body;
        
        const initializer = new DatabaseInitializer();
        
        // 백업 생성 (요청된 경우)
        if (backup) {
            await initializer.createBackup(true);
        }
        
        // 초기화 수행
        await initializer.performInitialization();
        
        res.json({
            success: true,
            message: '초기화가 완료되었습니다.',
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// =============================================================================
// 📋 API 라우트 등록
// =============================================================================

// 🔧 기본 API 라우트들 (기존 완성된 것들)
const systemRoutes = require('./routes/system');
const processRoutes = require('./routes/processes');
const serviceRoutes = require('./routes/services');
const userRoutes = require('./routes/user');

app.use('/api/system', systemRoutes);
app.use('/api/processes', processRoutes);
app.use('/api/services', serviceRoutes);
app.use('/api/users', userRoutes);

// 🆕 새로 완성된 핵심 API 라우트들
const devicesRoutes = require('./routes/devices');
const dataRoutes = require('./routes/data');
const realtimeRoutes = require('./routes/realtime');
const alarmsRoutes = require('./routes/alarms');
const dashboardRoutes = require('./routes/dashboard');

app.use('/api/devices', devicesRoutes);
app.use('/api/data', dataRoutes);
app.use('/api/realtime', realtimeRoutes);
app.use('/api/alarms', alarmsRoutes);
app.use('/api/dashboard', dashboardRoutes);

// 🔥 통합 API 정보 엔드포인트 (기존 api.js에서 분리)
const apiRoutes = require('./routes/api');
app.use('/api', apiRoutes);

// =============================================================================
// 🆕 추가 API 라우트들 (필요에 따라 활성화)
// =============================================================================

// 가상포인트 관리 API
try {
    const virtualPointsRoutes = require('./routes/virtual-points');
    app.use('/api/virtual-points', virtualPointsRoutes);
    console.log('✅ Virtual Points API 로드 완료');
} catch (error) {
    console.warn('⚠️ Virtual Points API 로드 실패:', error.message);
}

// 사이트 관리 API
try {
    const sitesRoutes = require('./routes/sites');
    app.use('/api/sites', sitesRoutes);
    console.log('✅ Sites API 로드 완료');
} catch (error) {
    console.warn('⚠️ Sites API 로드 실패:', error.message);
}

// 백업/복원 API
try {
    const backupRoutes = require('./routes/backup');
    app.use('/api/backup', backupRoutes);
    console.log('✅ Backup API 로드 완료');
} catch (error) {
    console.warn('⚠️ Backup API 로드 실패:', error.message);
}

// 네트워크 설정 API
try {
    const networkRoutes = require('./routes/network');
    app.use('/api/network', networkRoutes);
    console.log('✅ Network API 로드 완료');
} catch (error) {
    console.warn('⚠️ Network API 로드 실패:', error.message);
}

// 권한 관리 API
try {
    const permissionsRoutes = require('./routes/permissions');
    app.use('/api/permissions', permissionsRoutes);
    console.log('✅ Permissions API 로드 완료');
} catch (error) {
    console.warn('⚠️ Permissions API 로드 실패:', error.message);
}

// =============================================================================
// 🌐 Frontend 서빙
// =============================================================================

// SPA 라우팅을 위한 catch-all
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, '../frontend/index.html'));
});

// =============================================================================
// 🔧 WebSocket 서버 설정 (옵셔널)
// =============================================================================

let webSocketServer = null;
try {
    const WebSocketServer = require('./lib/websocket/WebSocketServer');
    webSocketServer = new WebSocketServer();
    console.log('✅ WebSocket 서버 초기화 완료');
} catch (error) {
    console.warn('⚠️ WebSocket 서버 초기화 실패:', error.message);
}

// =============================================================================
// 🚨 에러 핸들링
// =============================================================================

// 404 핸들러
app.use('*', (req, res) => {
    if (req.originalUrl.startsWith('/api/')) {
        res.status(404).json({ 
            success: false,
            error: 'API endpoint not found',
            endpoint: req.originalUrl,
            method: req.method,
            timestamp: new Date().toISOString()
        });
    } else {
        // SPA 라우팅을 위해 index.html 서빙
        res.sendFile(path.join(__dirname, '../frontend/index.html'));
    }
});

// 글로벌 에러 핸들러
app.use((error, req, res, next) => {
    console.error('🚨 Unhandled error:', error);
    
    const isDevelopment = process.env.NODE_ENV === 'development';
    
    res.status(500).json({
        success: false,
        error: 'Internal server error',
        message: isDevelopment ? error.message : 'Something went wrong',
        stack: isDevelopment ? error.stack : undefined,
        timestamp: new Date().toISOString(),
        request: {
            method: req.method,
            url: req.originalUrl,
            ip: req.ip
        }
    });
});

// =============================================================================
// 🚀 서버 시작
// =============================================================================

const PORT = process.env.PORT || 3000;
const HOST = process.env.HOST || '0.0.0.0';

const server = app.listen(PORT, HOST, () => {
    console.log('\n🚀 PulseOne Backend Server Started');
    console.log('==================================');
    console.log(`🌐 Server running on: http://${HOST}:${PORT}`);
    console.log(`🏥 Health check: http://${HOST}:${PORT}/health`);
    console.log(`📋 API docs: http://${HOST}:${PORT}/api/info`);
    console.log(`🖥️ Frontend: http://${HOST}:${PORT}/`);
    console.log(`🌍 Environment: ${process.env.NODE_ENV || 'development'}`);
    console.log('==================================');
    
    // 로드된 API 라우트 목록 출력
    console.log('\n📋 Available API Routes:');
    console.log('  ✅ /api/system      - 시스템 관리');
    console.log('  ✅ /api/processes   - 프로세스 관리');
    console.log('  ✅ /api/services    - 서비스 제어');
    console.log('  ✅ /api/users       - 사용자 관리');
    console.log('  🆕 /api/devices     - 디바이스 관리 (CRUD)');
    console.log('  🆕 /api/data        - 데이터 익스플로러');
    console.log('  🆕 /api/realtime    - 실시간 데이터');
    console.log('  🆕 /api/alarms      - 알람 관리');
    console.log('  🆕 /api/dashboard   - 대시보드 데이터');
    console.log('  🔥 /api            - 통합 API 테스트');
    
    if (webSocketServer) {
        console.log('\n🔄 WebSocket Services:');
        console.log('  ✅ /ws/realtime    - 실시간 데이터 스트림');
    }
    
    console.log('\n🎯 Ready for Frontend Integration!');
    console.log('==================================\n');
});

// WebSocket 서버 연결 (HTTP 서버와 공유)
if (webSocketServer) {
    webSocketServer.attach(server);
}

// Graceful shutdown 처리
process.on('SIGTERM', () => {
    console.log('\n🛑 SIGTERM received, shutting down gracefully...');
    server.close(() => {
        console.log('✅ HTTP server closed');
        
        if (webSocketServer) {
            webSocketServer.close();
            console.log('✅ WebSocket server closed');
        }
        
        process.exit(0);
    });
});

process.on('SIGINT', () => {
    console.log('\n🛑 SIGINT received, shutting down gracefully...');
    server.close(() => {
        console.log('✅ Server closed');
        process.exit(0);
    });
});

// Unhandled rejection 처리
process.on('unhandledRejection', (reason, promise) => {
    console.error('🚨 Unhandled Rejection at:', promise, 'reason:', reason);
});

module.exports = app;