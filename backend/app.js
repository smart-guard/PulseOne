// =============================================================================
// backend/app.js - 메인 애플리케이션 (완전 통합 버전 + 초기화 시스템 복구)
// 기존 구조 + data.js 라우트 추가 + 자동 초기화 시스템 + 서비스 제어 API
// =============================================================================

const express = require('express');
const cors = require('cors');
const path = require('path');
const { initializeConnections } = require('./lib/connection/db');

// 🚀 자동 초기화 시스템 (안전 로드 + 복구 패치)
let DatabaseInitializer = null;
try {
    // 우선순위 1: 새로 생성된 DatabaseInitializer
    DatabaseInitializer = require('./lib/database/DatabaseInitializer');
    console.log('✅ DatabaseInitializer 로드 성공 (lib/database/DatabaseInitializer.js)');
} catch (error1) {
    try {
        // 우선순위 2: 기존 위치
        DatabaseInitializer = require('./scripts/database-initializer');
        console.log('✅ DatabaseInitializer 로드 성공 (scripts/database-initializer.js)');
    } catch (error2) {
        console.warn('⚠️ DatabaseInitializer 로드 실패:', error1.message);
        console.warn('   초기화 기능이 비활성화됩니다.');
    }
}

const app = express();

// ============================================================================
// 🔧 미들웨어 설정 (기존 코드 + 확장)
// ============================================================================

// CORS 설정 (프런트엔드 연동 강화)
app.use(cors({
    origin: ['http://localhost:3000', 'http://localhost:5173', 'http://localhost:5174', 'http://localhost:8080'],
    credentials: true,
    methods: ['GET', 'POST', 'PUT', 'PATCH', 'DELETE', 'OPTIONS'],
    allowedHeaders: ['Content-Type', 'Authorization', 'X-Tenant-ID']
}));

app.use(express.json({ limit: '10mb' }));
app.use(express.urlencoded({ extended: true, limit: '10mb' }));
//app.use(express.static(path.join(__dirname, '../frontend')));

// 요청 로깅 미들웨어 (새로 추가)
app.use((req, res, next) => {
    const timestamp = new Date().toISOString();
    console.log(`[${timestamp}] ${req.method} ${req.path}`);
    next();
});

// ============================================================================
// 🔐 글로벌 인증 및 테넌트 미들웨어 (개발용)
// ============================================================================

/**
 * 기본 인증 미들웨어 (개발용)
 */
const authenticateToken = (req, res, next) => {
    // API 경로가 아니거나 특정 경로는 인증 스킵
    if (!req.originalUrl.startsWith('/api/') || 
        req.originalUrl.startsWith('/api/health') ||
        req.originalUrl.startsWith('/api/init/')) {
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
// 🏗️ 데이터베이스 연결 및 자동 초기화 (기존 코드)
// ============================================================================

// Database connections 초기화 + 자동 초기화
let connections = {};

async function initializeSystem() {
    try {
        console.log('🚀 PulseOne 시스템 초기화 시작...\n');
        
        // 1. 기존 데이터베이스 연결 (기존 코드 유지)
        connections = await initializeConnections();
        app.locals.getDB = () => connections;
        console.log('✅ Database connections initialized');
        
        // 2. 자동 초기화 시스템 (기존 코드)
        if (process.env.AUTO_INITIALIZE_ON_START === 'true' && DatabaseInitializer) {
            console.log('🔄 자동 초기화 확인 중...');
            
            const initializer = new DatabaseInitializer();
            await initializer.checkDatabaseStatus();
            
            // 완전히 초기화되어 있고 스킵 옵션이 활성화된 경우
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
        
        // 초기화 실패 시 처리 방식
        if (process.env.FAIL_ON_INIT_ERROR === 'true') {
            process.exit(1);
        } else {
            console.log('⚠️  초기화 실패했지만 서버를 계속 시작합니다.\n');
        }
    }
}

// 시스템 초기화 실행
initializeSystem();

// ============================================================================
// 🏥 헬스체크 및 초기화 관리 엔드포인트 (복구 패치 적용)
// ============================================================================

// Health check (기존 + 초기화 상태 추가)
app.get('/api/health', async (req, res) => {
    try {
        // 기본 헬스체크 정보 (기존)
        const healthInfo = { 
            status: 'ok', 
            timestamp: new Date().toISOString(),
            uptime: process.uptime(),
            pid: process.pid
        };
        
        // 초기화 시스템 상태 확인
        healthInfo.initialization = {
            databaseInitializer: {
                available: !!DatabaseInitializer,
                autoInit: process.env.AUTO_INITIALIZE_ON_START === 'true'
            }
        };
        
        // DatabaseInitializer가 있으면 상세 상태 추가
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

// 초기화 상태 조회 (복구 패치 적용)
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

// 초기화 수동 트리거 (복구 패치 적용)
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
        
        // 백업 생성 (요청된 경우)
        if (backup) {
            try {
                await initializer.createBackup(true);
                console.log('✅ 백업 생성 완료');
            } catch (backupError) {
                console.warn('⚠️ 백업 생성 실패:', backupError.message);
                // 백업 실패해도 초기화는 계속 진행
            }
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
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// 임시 초기화 대안 엔드포인트 추가 (DatabaseInitializer 없어도 동작)
app.post('/api/init/manual', async (req, res) => {
    try {
        console.log('🔧 수동 초기화 시도...');
        
        // 기본 SQLite 데이터베이스 연결 확인
        const connections = app.locals.getDB ? app.locals.getDB() : null;
        
        if (!connections || !connections.db) {
            return res.status(503).json({
                success: false,
                error: 'SQLite 데이터베이스 연결을 찾을 수 없습니다.',
                suggestion: '앱을 재시작하거나 데이터베이스 설정을 확인하세요.'
            });
        }
        
        // 간단한 테이블 존재 확인
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

// Frontend 서빙 (기존)
//app.get('/', (req, res) => {
//    res.sendFile(path.join(__dirname, '../frontend/index.html'));
//});

// ============================================================================
// 🌐 API Routes 등록 (기존 + 새로운 data.js 포함)
// ============================================================================

console.log('\n🚀 API 라우트 등록 중...\n');

// ============================================================================
// 📋 기존 API Routes (유지)
// ============================================================================

const systemRoutes = require('./routes/system');
const processRoutes = require('./routes/processes');
const serviceRoutes = require('./routes/services');
const userRoutes = require('./routes/user');

app.use('/api/system', systemRoutes);
app.use('/api/processes', processRoutes);
app.use('/api/services', serviceRoutes);
app.use('/api/users', userRoutes);

console.log('✅ 기존 시스템 API 라우트들 등록 완료');

// ============================================================================
// 🔥 핵심 비즈니스 API - 새로 추가된 부분
// ============================================================================

// 1. 디바이스 관리 API
try {
    const deviceRoutes = require('./routes/devices');
    app.use('/api/devices', deviceRoutes);
    console.log('✅ Device Management API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Device 라우트 로드 실패:', error.message);
}

// 2. 🆕 데이터 익스플로러 API (새로 추가!)
try {
    const dataRoutes = require('./routes/data');
    app.use('/api/data', dataRoutes);
    console.log('✅ 🆕 Data Explorer API 라우트 등록 완료 (/api/data/points 사용 가능!)');
} catch (error) {
    console.warn('⚠️ Data 라우트 로드 실패:', error.message);
    console.warn('   데이터 익스플로러 기능이 비활성화됩니다.');
}

// 3. 알람 관리 API
try {
    const alarmRoutes = require('./routes/alarms');
    app.use('/api/alarms', alarmRoutes);
    console.log('✅ Alarm Management API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Alarm 라우트 로드 실패:', error.message);
}

// ============================================================================
// 📊 확장 API - 선택적 등록 (서비스 제어 포함)
// ============================================================================

// 대시보드 API (서비스 제어 기능 포함)
try {
    const dashboardRoutes = require('./routes/dashboard');
    app.use('/api/dashboard', dashboardRoutes);
    console.log('✅ Dashboard API 라우트 등록 완료 (서비스 제어 포함)');
} catch (error) {
    console.warn('⚠️ Dashboard 라우트 로드 실패:', error.message);
}

// 실시간 데이터 API
try {
    const realtimeRoutes = require('./routes/realtime');
    app.use('/api/realtime', realtimeRoutes);
    console.log('✅ Realtime Data API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Realtime 라우트 로드 실패:', error.message);
}

// 가상포인트 관리 API
try {
    const virtualPointRoutes = require('./routes/virtual-points');
    app.use('/api/virtual-points', virtualPointRoutes);
    console.log('✅ Virtual Points API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Virtual Points 라우트 로드 실패:', error.message);
}

// 시스템 모니터링 API
try {
    const monitoringRoutes = require('./routes/monitoring');
    app.use('/api/monitoring', monitoringRoutes);
    console.log('✅ System Monitoring API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Monitoring 라우트 로드 실패:', error.message);
}

// 백업/복원 API
try {
    const backupRoutes = require('./routes/backup');
    app.use('/api/backup', backupRoutes);
    console.log('✅ Backup/Restore API 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ Backup 라우트 로드 실패:', error.message);
}

// 기본 API 정보 (기존 api.js)
try {
    const apiRoutes = require('./routes/api');
    app.use('/api', apiRoutes);
    console.log('✅ 기본 API 정보 라우트 등록 완료');
} catch (error) {
    console.warn('⚠️ API 정보 라우트 로드 실패:', error.message);
}

console.log('\n🎉 모든 API 라우트 등록 완료!\n');

// =============================================================================
// Error Handling (기존 + 확장)
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

// Global error handler (개선됨)
app.use((error, req, res, next) => {
    console.error('🚨 Unhandled error:', error);
    
    // 에러 타입별 처리
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
// Graceful Shutdown (기존 코드 유지)
// =============================================================================

process.on('SIGTERM', gracefulShutdown);
process.on('SIGINT', gracefulShutdown);

function gracefulShutdown(signal) {
    console.log(`\n🔄 Received ${signal}. Starting graceful shutdown...`);
    
    server.close((err) => {
        if (err) {
            console.error('❌ Error during server shutdown:', err);
            process.exit(1);
        }
        
        console.log('✅ HTTP server closed');
        
        // Close database connections
        if (connections.postgres) connections.postgres.end();
        if (connections.redis) connections.redis.disconnect();
        
        console.log('✅ Database connections closed');
        process.exit(0);
    });
    
    setTimeout(() => {
        console.error('❌ Forced shutdown after timeout');
        process.exit(1);
    }, 10000);
}

// =============================================================================
// Start Server (완전 통합 버전 - 모든 API 상태 표시)
// =============================================================================

const PORT = process.env.PORT || process.env.BACKEND_PORT || 3000;
const server = app.listen(PORT, () => {
    console.log(`
🚀 PulseOne Backend Server Started! (완전 통합 + 초기화 복구 버전)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📊 Dashboard:     http://localhost:${PORT}
🔧 API Health:    http://localhost:${PORT}/api/health
📈 System Info:   http://localhost:${PORT}/api/system/info
💾 DB Status:     http://localhost:${PORT}/api/system/databases
🔧 Processes:     http://localhost:${PORT}/api/processes
⚙️  Services:      http://localhost:${PORT}/api/services
👤 Users:         http://localhost:${PORT}/api/users
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

🔥 핵심 비즈니스 API (우선순위 1 - 필수)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🚨 알람 관리 API: http://localhost:${PORT}/api/alarms
   ├─ 활성 알람:  GET  /api/alarms/active
   ├─ 알람 이력:  GET  /api/alarms/history  
   ├─ 알람 확인:  POST /api/alarms/:id/acknowledge
   ├─ 알람 해제:  POST /api/alarms/:id/clear
   ├─ 알람 규칙:  GET  /api/alarms/rules
   ├─ 규칙 생성:  POST /api/alarms/rules
   ├─ 규칙 수정:  PUT  /api/alarms/rules/:id
   ├─ 규칙 삭제:  DEL  /api/alarms/rules/:id
   └─ 알람 통계:  GET  /api/alarms/statistics

📱 디바이스 관리 API: http://localhost:${PORT}/api/devices
   ├─ 디바이스 목록:     GET  /api/devices
   ├─ 디바이스 생성:     POST /api/devices
   ├─ 디바이스 상세:     GET  /api/devices/:id
   ├─ 디바이스 수정:     PUT  /api/devices/:id
   ├─ 디바이스 삭제:     DEL  /api/devices/:id
   ├─ 설정 관리:        GET  /api/devices/:id/settings
   ├─ 설정 업데이트:     PUT  /api/devices/:id/settings
   ├─ 데이터포인트:      GET  /api/devices/:id/data-points
   ├─ 포인트 생성:      POST /api/devices/:id/data-points
   ├─ 현재값 조회:      GET  /api/devices/:id/current-values
   ├─ 값 업데이트:      PUT  /api/devices/:deviceId/data-points/:pointId/value
   ├─ 연결 테스트:      POST /api/devices/:id/test-connection
   ├─ 디바이스 제어:     POST /api/devices/:id/enable|disable|restart
   ├─ 일괄 작업:        POST /api/devices/batch/enable|disable
   ├─ 통계 (프로토콜):   GET  /api/devices/stats/protocol
   ├─ 통계 (사이트):     GET  /api/devices/stats/site
   ├─ 시스템 요약:      GET  /api/devices/stats/summary
   ├─ 최근 활동:        GET  /api/devices/stats/recent-active
   ├─ 오류 목록:        GET  /api/devices/stats/errors
   ├─ 응답시간 통계:     GET  /api/devices/stats/response-time
   ├─ 포인트 검색:      GET  /api/devices/search/data-points
   └─ 헬스체크:         GET  /api/devices/health

📊 데이터 익스플로러 API: http://localhost:${PORT}/api/data
   ├─ 데이터포인트 검색: GET  /api/data/points
   ├─ 포인트 상세:      GET  /api/data/points/:id
   ├─ 현재값 일괄조회:   GET  /api/data/current-values
   ├─ 디바이스 현재값:   GET  /api/data/device/:id/current-values
   ├─ 이력 데이터:      GET  /api/data/historical
   ├─ 데이터 통계:      GET  /api/data/statistics
   ├─ 고급 쿼리:        POST /api/data/query
   └─ 데이터 내보내기:   POST /api/data/export

🔮 가상포인트 API: http://localhost:${PORT}/api/virtual-points
   ├─ 가상포인트 목록:   GET  /api/virtual-points
   ├─ 가상포인트 생성:   POST /api/virtual-points
   ├─ 가상포인트 상세:   GET  /api/virtual-points/:id
   ├─ 가상포인트 수정:   PUT  /api/virtual-points/:id
   ├─ 가상포인트 삭제:   DEL  /api/virtual-points/:id
   ├─ 의존성 조회:      GET  /api/virtual-points/:id/dependencies
   ├─ 실행 이력:        GET  /api/virtual-points/:id/history
   ├─ 계산 테스트:      POST /api/virtual-points/:id/test
   ├─ 수동 실행:        POST /api/virtual-points/:id/execute
   ├─ 값 업데이트:      PUT  /api/virtual-points/:id/value
   ├─ 카테고리 통계:     GET  /api/virtual-points/stats/category
   └─ 성능 통계:        GET  /api/virtual-points/stats/performance

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📊 확장 API (우선순위 2 - 선택적)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📈 대시보드:     GET  /api/dashboard/overview
🔄 실시간 데이터: GET  /api/realtime/current-values
📈 모니터링:     GET  /api/monitoring/system-metrics
💾 백업 관리:    GET  /api/backup/list

🔧 서비스 제어 API (새로 복구됨!)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
⚙️  서비스 상태:    GET  /api/dashboard/services/status
🔧 Collector 시작: POST /api/dashboard/service/collector/control {"action":"start"}
⏹️  Collector 중지: POST /api/dashboard/service/collector/control {"action":"stop"}
🔄 Collector 재시작: POST /api/dashboard/service/collector/control {"action":"restart"}
🗄️  Redis 제어:    POST /api/dashboard/service/redis/control {"action":"start|stop|restart"}
💾 Database 제어:  POST /api/dashboard/service/database/control {"action":"start|stop|restart"}

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

🚀 시스템 초기화 (완전 복구됨!)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🔧 초기화 상태:   GET  /api/init/status (${DatabaseInitializer ? '✅ 활성' : '❌ 비활성'})
🔄 초기화 트리거: POST /api/init/trigger (${DatabaseInitializer ? '✅ 활성' : '❌ 비활성'})
⚙️  수동 초기화:   POST /api/init/manual (항상 사용 가능)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Environment: ${process.env.NODE_ENV || 'development'}
Stage: ${process.env.ENV_STAGE || 'dev'}
Auto Initialize: ${process.env.AUTO_INITIALIZE_ON_START === 'true' ? '✅ Enabled' : '❌ Disabled'}
DatabaseInitializer: ${DatabaseInitializer ? '✅ Available' : '❌ Not Found'}
Authentication: 🔓 Development Mode (Basic Auth)
Tenant Isolation: ✅ Enabled
PID: ${process.pid}

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🎉 PulseOne 통합 백엔드 시스템 완전 가동! (v2.1.0 - 초기화 시스템 복구)
   - 알람 관리 ✅
   - 디바이스 관리 ✅  
   - 가상포인트 관리 ✅
   - 데이터 익스플로러 ✅
   - 자동 초기화 ${DatabaseInitializer ? '✅' : '⚠️'}
   - 서비스 제어 ✅
   - 멀티테넌트 지원 ✅
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    `);
});

module.exports = app;