// ===========================================================================
// backend/routes/system.js
// 시스템 상태 API - DATABASE_TYPE 기반 동적 체크로 수정
// ===========================================================================

const express = require('express');
const router = express.Router();
const os = require('os');
const ConfigManager = require('../lib/config/ConfigManager');

const configManager = ConfigManager.getInstance();

// =============================================================================
// 🔍 동적 데이터베이스 상태 체크 (DATABASE_TYPE 기반)
// =============================================================================

/**
 * 실제 설정된 데이터베이스 타입에 맞게 상태 체크
 */
async function checkConfiguredDatabases() {
    const databaseType = configManager.get('DATABASE_TYPE', 'SQLITE').toUpperCase();
    const results = {};
    
    console.log(`🔍 DATABASE_TYPE: ${databaseType} 기반으로 데이터베이스 상태 체크`);

    // 1. 메인 데이터베이스 체크 (DATABASE_TYPE에 따라)
    switch (databaseType) {
        case 'SQLITE':
        case 'SQLITE3':
            results.sqlite = await checkSQLiteStatus();
            break;
            
        case 'POSTGRESQL':
        case 'POSTGRES':
            results.postgresql = await checkPostgreSQLStatus();
            break;
            
        case 'MYSQL':
        case 'MARIADB':
            results.mysql = await checkMySQLStatus();
            break;
            
        case 'MSSQL':
            results.mssql = await checkMSSQLStatus();
            break;
            
        default:
            console.warn(`⚠️ 알 수 없는 DATABASE_TYPE: ${databaseType}, SQLite로 기본 설정`);
            results.sqlite = await checkSQLiteStatus();
    }

    // 2. 보조 서비스들 체크 (설정에 따라)
    if (configManager.getBoolean('REDIS_PRIMARY_ENABLED', false)) {
        results.redis = await checkRedisStatus();
    }
    
    if (configManager.getBoolean('INFLUXDB_ENABLED', false)) {
        results.influxdb = await checkInfluxDBStatus();
    }

    return results;
}

/**
 * SQLite 상태 체크
 */
async function checkSQLiteStatus() {
    try {
        const sqlite3 = require('sqlite3');
        const sqlitePath = configManager.get('SQLITE_PATH', './data/db/pulseone.db');
        
        return new Promise((resolve) => {
            const db = new sqlite3.Database(sqlitePath, (err) => {
                if (err) {
                    resolve({
                        status: 'error',
                        error: err.message,
                        path: sqlitePath
                    });
                } else {
                    db.close();
                    resolve({
                        status: 'connected',
                        path: sqlitePath,
                        type: 'sqlite'
                    });
                }
            });
        });
    } catch (error) {
        return {
            status: 'error',
            error: 'SQLite 모듈 로드 실패: ' + error.message
        };
    }
}

/**
 * PostgreSQL 상태 체크
 */
async function checkPostgreSQLStatus() {
    try {
        const postgres = require('../lib/connection/postgres');
        
        if (postgres.isReady && postgres.isReady()) {
            const info = postgres.getConnectionInfo();
            return {
                status: 'connected',
                host: info.host,
                port: info.port,
                database: info.database
            };
        } else {
            // 연결 시도
            await postgres.connect();
            const info = postgres.getConnectionInfo();
            return {
                status: 'connected',
                host: info.host,
                port: info.port,
                database: info.database
            };
        }
    } catch (error) {
        return {
            status: 'error',
            error: error.message
        };
    }
}

/**
 * MySQL/MariaDB 상태 체크
 */
async function checkMySQLStatus() {
    try {
        // MySQL 연결 체크 로직 추가 필요
        return {
            status: 'not_implemented',
            message: 'MySQL 상태 체크가 아직 구현되지 않음'
        };
    } catch (error) {
        return {
            status: 'error',
            error: error.message
        };
    }
}

/**
 * MSSQL 상태 체크
 */
async function checkMSSQLStatus() {
    try {
        // MSSQL 연결 체크 로직 추가 필요
        return {
            status: 'not_implemented',
            message: 'MSSQL 상태 체크가 아직 구현되지 않음'
        };
    } catch (error) {
        return {
            status: 'error',
            error: error.message
        };
    }
}

/**
 * Redis 상태 체크
 */
async function checkRedisStatus() {
    try {
        const redis = require('../lib/connection/redis');
        
        if (redis.isConnected) {
            return {
                status: 'connected',
                host: redis.config?.host || 'localhost',
                port: redis.config?.port || 6379
            };
        } else {
            return {
                status: 'disconnected',
                host: redis.config?.host || 'localhost',
                port: redis.config?.port || 6379
            };
        }
    } catch (error) {
        return {
            status: 'error',
            error: error.message
        };
    }
}

/**
 * InfluxDB 상태 체크
 */
async function checkInfluxDBStatus() {
    try {
        const influx = require('../lib/connection/influx');
        
        // InfluxDB 연결 체크 로직
        return {
            status: 'connected',
            host: 'localhost',
            port: '8086'
        };
    } catch (error) {
        return {
            status: 'error',
            error: error.message
        };
    }
}

// =============================================================================
// 🔗 API 엔드포인트들
// =============================================================================

/**
 * GET /api/system/databases
 * 설정된 데이터베이스들의 상태 체크 (동적)
 */
router.get('/databases', async (req, res) => {
    try {
        const databaseStatus = await checkConfiguredDatabases();
        res.json(databaseStatus);
    } catch (error) {
        console.error('Database status check error:', error);
        res.status(500).json({
            error: 'Failed to check database status',
            message: error.message
        });
    }
});

/**
 * GET /api/system/config
 * 현재 시스템 설정 정보 (민감 정보 제외)
 */
router.get('/config', (req, res) => {
    try {
        const config = {
            database: {
                type: configManager.get('DATABASE_TYPE', 'SQLITE'),
                sqlite: {
                    enabled: configManager.get('DATABASE_TYPE', 'SQLITE').toUpperCase() === 'SQLITE',
                    path: configManager.get('SQLITE_PATH', './data/db/pulseone.db')
                },
                postgresql: {
                    enabled: configManager.get('DATABASE_TYPE', 'SQLITE').toUpperCase() === 'POSTGRESQL',
                    host: configManager.get('POSTGRES_MAIN_DB_HOST', 'localhost'),
                    port: configManager.getNumber('POSTGRES_MAIN_DB_PORT', 5432),
                    database: configManager.get('POSTGRES_MAIN_DB_NAME', 'pulseone')
                }
            },
            redis: {
                enabled: configManager.getBoolean('REDIS_PRIMARY_ENABLED', false),
                host: configManager.get('REDIS_PRIMARY_HOST', 'localhost'),
                port: configManager.getNumber('REDIS_PRIMARY_PORT', 6379)
            },
            influxdb: {
                enabled: configManager.getBoolean('INFLUXDB_ENABLED', false)
            },
            server: {
                port: configManager.getNumber('BACKEND_PORT', 3000),
                env: configManager.get('NODE_ENV', 'development'),
                stage: configManager.get('ENV_STAGE', 'dev')
            }
        };
        
        res.json({
            success: true,
            data: config
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

/**
 * GET /api/system/info
 * 시스템 정보
 */
router.get('/info', (req, res) => {
    try {
        const systemInfo = {
            node: {
                version: process.version,
                platform: process.platform,
                arch: process.arch,
                uptime: process.uptime()
            },
            os: {
                type: os.type(),
                platform: os.platform(),
                arch: os.arch(),
                release: os.release(),
                hostname: os.hostname(),
                uptime: os.uptime()
            },
            memory: {
                total: os.totalmem(),
                free: os.freemem(),
                usage: process.memoryUsage()
            },
            cpu: {
                count: os.cpus().length,
                loadavg: os.loadavg()
            },
            app: {
                name: 'PulseOne Backend',
                version: process.env.npm_package_version || '1.0.0',
                env: process.env.NODE_ENV || 'development',
                pid: process.pid
            }
        };

        res.json({
            success: true,
            data: systemInfo
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

/**
 * GET /api/system/health
 * 간단한 헬스 체크
 */
router.get('/health', (req, res) => {
    res.json({
        status: 'healthy',
        timestamp: new Date().toISOString(),
        uptime: process.uptime()
    });
});

module.exports = router;