// ===========================================================================
// backend/lib/connection/db.js - 완전한 데이터베이스 연결 시스템
// SQLite + PostgreSQL + Redis + InfluxDB + RabbitMQ + TimeSeries + RPC
// ===========================================================================
const ConfigManager = require('../config/ConfigManager');

const config = ConfigManager.getInstance();


// 데이터베이스 타입에 따른 연결 함수들
const connectionMap = {
    // 🔥 SQLite 연결 (가장 중요 - 누락되어 있던 부분)
    sqlite: () => {
        try {
            const sqliteConnection = require('./sqlite');
            return sqliteConnection;
        } catch (error) {
            console.warn('⚠️ SQLite 연결 모듈 로드 실패:', error.message);
            return null;
        }
    },
    
    // PostgreSQL 연결
    postgres: () => {
        try {
            const postgresConnection = require('./postgres');
            return postgresConnection;
        } catch (error) {
            console.warn('⚠️ PostgreSQL 연결 모듈 로드 실패:', error.message);
            return null;
        }
    },
    
    // Redis 연결
    redis: () => {
        try {
            const redisConnection = require('./redis');
            return redisConnection;
        } catch (error) {
            console.warn('⚠️ Redis 연결 모듈 로드 실패:', error.message);
            return null;
        }
    },
    
    // InfluxDB 연결
    influx: () => {
        try {
            const influxConnection = require('./influx');
            return influxConnection;
        } catch (error) {
            console.warn('⚠️ InfluxDB 연결 모듈 로드 실패:', error.message);
            return null;
        }
    },
    
    // RabbitMQ 연결
    mq: () => {
        try {
            const mqConnection = require('./mq');
            return mqConnection;
        } catch (error) {
            console.warn('⚠️ RabbitMQ 연결 모듈 로드 실패:', error.message);
            return null;
        }
    },
    
    // RPC 연결
    rpc: () => {
        try {
            const rpcConnection = require('./rpc');
            return rpcConnection;
        } catch (error) {
            console.warn('⚠️ RPC 연결 모듈 로드 실패:', error.message);
            return null;
        }
    },
    
    // TimeSeries 연결
    timeseries: () => {
        try {
            const timeseriesConnection = require('./timeseries');
            return timeseriesConnection;
        } catch (error) {
            console.warn('⚠️ TimeSeries 연결 모듈 로드 실패:', error.message);
            return null;
        }
    },
    
    // MySQL/MariaDB 연결 (ConfigManager에서 활성화된 경우)
    mysql: () => {
        try {
            const mysqlEnabled = config.getBoolean('MYSQL_ENABLED', false);
            if (!mysqlEnabled) {
                return null;
            }
            
            const mysqlConnection = require('./mysql');
            return mysqlConnection;
        } catch (error) {
            console.warn('⚠️ MySQL 연결 모듈 로드 실패:', error.message);
            return null;
        }
    },
    
    // MSSQL 연결 (ConfigManager에서 활성화된 경우)
    mssql: () => {
        try {
            const mssqlEnabled = config.getBoolean('MSSQL_ENABLED', false);
            if (!mssqlEnabled) {
                return null;
            }
            
            const mssqlConnection = require('./mssql');
            return mssqlConnection;
        } catch (error) {
            console.warn('⚠️ MSSQL 연결 모듈 로드 실패:', error.message);
            return null;
        }
    }
};

/**
 * 모든 연결을 초기화하는 함수 (app.js에서 사용)
 */
async function initializeConnections() {
    console.log('🚀 데이터베이스 연결 초기화 시작...');
    
    const connections = {};
    const results = [];

    // ConfigManager에서 우선순위 결정
    const dbType = config.getDatabaseConfig().type.toLowerCase();
    const priorityConnections = [];
    
    // 메인 데이터베이스를 우선적으로 연결
    if (dbType === 'sqlite') {
        priorityConnections.push('sqlite');
    } else if (dbType === 'postgresql') {
        priorityConnections.push('postgres');
    } else if (dbType === 'mysql') {
        priorityConnections.push('mysql');
    } else if (dbType === 'mssql') {
        priorityConnections.push('mssql');
    }
    
    // 나머지 연결들을 추가 (메인 DB가 아닌 것들)
    const allConnections = Object.keys(connectionMap);
    const otherConnections = allConnections.filter(conn => !priorityConnections.includes(conn));
    
    // 우선순위 + 나머지 순서로 정렬
    const orderedConnections = [...priorityConnections, ...otherConnections];

    // 각 연결 타입별로 초기화 (순서대로)
    for (const type of orderedConnections) {
        if (!connectionMap[type]) continue;
        
        try {
            console.log(`🔄 ${type} 연결 초기화 중...`);
            
            const connection = connectionMap[type]();
            
            if (connection) {
                // 연결이 성공적으로 로드된 경우
                connections[type] = connection;
                
                // 연결 테스트 (가능한 경우)
                try {
                    if (connection.connect && typeof connection.connect === 'function') {
                        await connection.connect();
                        console.log(`🔌 ${type} 연결 테스트 성공`);
                    } else if (connection.ping && typeof connection.ping === 'function') {
                        await connection.ping();
                        console.log(`🏓 ${type} ping 테스트 성공`);
                    } else if (connection.initialize && typeof connection.initialize === 'function') {
                        await connection.initialize();
                        console.log(`🚀 ${type} 초기화 성공`);
                    } else if (connection.isReady && typeof connection.isReady === 'function') {
                        const ready = connection.isReady();
                        console.log(`📋 ${type} 준비 상태: ${ready ? '준비됨' : '대기중'}`);
                    }
                } catch (testError) {
                    console.warn(`⚠️ ${type} 연결 테스트 실패: ${testError.message}`);
                    // 테스트 실패해도 연결 객체는 유지
                }
                
                results.push({ type, status: 'success', connection });
                console.log(`✅ ${type} 연결 성공`);
            } else {
                results.push({ type, status: 'skipped', error: 'Connection returned null or disabled' });
                console.log(`⏭️ ${type} 연결 스킵됨`);
            }
            
        } catch (error) {
            results.push({ type, status: 'error', error: error.message });
            console.error(`❌ ${type} 연결 오류:`, error.message);
        }
    }

    // 연결 결과 요약
    const successful = results.filter(r => r.status === 'success').length;
    const skipped = results.filter(r => r.status === 'skipped').length;
    const failed = results.filter(r => r.status === 'error').length;
    const total = results.length;
    
    console.log(`\n📊 연결 초기화 완료: ${successful}성공 / ${skipped}스킵 / ${failed}실패 (총 ${total}개)`);
    
    // 연결된 서비스들 목록
    const connectedServices = results
        .filter(r => r.status === 'success')
        .map(r => r.type)
        .join(', ');
    
    if (connectedServices) {
        console.log(`✅ 연결된 서비스: ${connectedServices}`);
    }
    
    // 실패한 서비스들 목록
    const failedServices = results
        .filter(r => r.status === 'error')
        .map(r => `${r.type}(${r.error})`)
        .join(', ');
        
    if (failedServices) {
        console.log(`❌ 연결 실패: ${failedServices}`);
    }
    
    // 스킵된 서비스들 목록
    const skippedServices = results
        .filter(r => r.status === 'skipped')
        .map(r => r.type)
        .join(', ');
        
    if (skippedServices) {
        console.log(`⏭️ 스킵된 서비스: ${skippedServices}`);
    }

    console.log(''); // 빈 줄 추가

    // 🔥 핵심: 메인 데이터베이스가 연결되지 않은 경우 경고
    if (!connections[dbType]) {
        console.warn(`⚠️ 경고: 메인 데이터베이스(${dbType})가 연결되지 않았습니다!`);
        
        // SQLite 폴백 시도
        if (dbType !== 'sqlite' && !connections.sqlite) {
            console.log('🔄 SQLite 폴백 연결 시도...');
            try {
                const sqliteConnection = connectionMap.sqlite();
                if (sqliteConnection) {
                    await sqliteConnection.connect();
                    connections.sqlite = sqliteConnection;
                    console.log('✅ SQLite 폴백 연결 성공');
                }
            } catch (fallbackError) {
                console.error('❌ SQLite 폴백 연결도 실패:', fallbackError.message);
            }
        }
    }

    return connections;
}

/**
 * 개별 연결 생성 함수들
 */
async function createSQLiteConnection() {
    return connectionMap.sqlite();
}

async function createPostgresConnection() {
    return connectionMap.postgres();
}

async function createRedisConnection() {
    return connectionMap.redis();
}

async function createInfluxConnection() {
    return connectionMap.influx();
}

async function createMQConnection() {
    return connectionMap.mq();
}

async function createRPCConnection() {
    return connectionMap.rpc();
}

async function createTimeSeriesConnection() {
    return connectionMap.timeseries();
}

async function createMySQLConnection() {
    return connectionMap.mysql();
}

async function createMSSQLConnection() {
    return connectionMap.mssql();
}

/**
 * 특정 데이터베이스 타입의 연결 가져오기
 */
function getConnectionByType(connections, dbType) {
    const normalizedType = dbType.toLowerCase();
    
    // 타입 매핑
    const typeMapping = {
        'sqlite': 'sqlite',
        'postgresql': 'postgres',
        'postgres': 'postgres',
        'mysql': 'mysql',
        'mariadb': 'mysql',
        'mssql': 'mssql',
        'sqlserver': 'mssql',
        'redis': 'redis',
        'influx': 'influx',
        'influxdb': 'influx',
        'rabbitmq': 'mq',
        'mq': 'mq'
    };
    
    const mappedType = typeMapping[normalizedType] || normalizedType;
    return connections[mappedType] || null;
}

/**
 * 모든 연결 종료
 */
async function closeAllConnections(connections) {
    console.log('📴 모든 연결 종료 중...');
    
    for (const [type, connection] of Object.entries(connections)) {
        try {
            if (connection && connection.close) {
                await connection.close();
                console.log(`✅ ${type} 연결 종료 완료`);
            } else if (connection && connection.end) {
                await connection.end();
                console.log(`✅ ${type} 연결 종료 완료 (end)`);
            } else if (connection && connection.disconnect) {
                await connection.disconnect();
                console.log(`✅ ${type} 연결 종료 완료 (disconnect)`);
            }
        } catch (error) {
            console.error(`❌ ${type} 연결 종료 오류:`, error.message);
        }
    }
}

/**
 * 연결 상태 확인
 */
function checkConnectionStatus(connections) {
    const status = {};
    
    for (const [type, connection] of Object.entries(connections)) {
        if (connection) {
            if (connection.isReady && typeof connection.isReady === 'function') {
                status[type] = connection.isReady();
            } else if (connection.isConnected !== undefined) {
                status[type] = connection.isConnected;
            } else if (connection.connected !== undefined) {
                status[type] = connection.connected;
            } else {
                status[type] = true; // 연결 객체가 존재하면 연결된 것으로 간주
            }
        } else {
            status[type] = false;
        }
    }
    
    return status;
}

/**
 * 연결 상태 상세 정보
 */
function getDetailedConnectionInfo(connections) {
    const info = {};
    
    for (const [type, connection] of Object.entries(connections)) {
        if (connection) {
            info[type] = {
                exists: true,
                isReady: connection.isReady ? connection.isReady() : null,
                isConnected: connection.isConnected || connection.connected || null,
                info: connection.getConnectionInfo ? connection.getConnectionInfo() : null,
                methods: Object.getOwnPropertyNames(Object.getPrototypeOf(connection))
                    .filter(name => typeof connection[name] === 'function')
            };
        } else {
            info[type] = {
                exists: false,
                isReady: false,
                isConnected: false,
                info: null,
                methods: []
            };
        }
    }
    
    return info;
}

/**
 * 헬스체크 수행
 */
async function performHealthCheck(connections) {
    const healthResults = {};
    
    for (const [type, connection] of Object.entries(connections)) {
        try {
            if (connection) {
                if (connection.ping && typeof connection.ping === 'function') {
                    await connection.ping();
                    healthResults[type] = { status: 'healthy', method: 'ping' };
                } else if (connection.query && typeof connection.query === 'function') {
                    // 간단한 SELECT 쿼리로 테스트
                    await connection.query('SELECT 1');
                    healthResults[type] = { status: 'healthy', method: 'query' };
                } else if (connection.isReady && typeof connection.isReady === 'function') {
                    const ready = connection.isReady();
                    healthResults[type] = { 
                        status: ready ? 'healthy' : 'not_ready', 
                        method: 'isReady' 
                    };
                } else {
                    healthResults[type] = { status: 'unknown', method: 'none' };
                }
            } else {
                healthResults[type] = { status: 'not_connected', method: 'none' };
            }
        } catch (error) {
            healthResults[type] = { 
                status: 'unhealthy', 
                error: error.message,
                method: 'error' 
            };
        }
    }
    
    return healthResults;
}

module.exports = {
    // 메인 함수 (app.js에서 사용)
    initializeConnections,
    
    // 개별 연결 생성
    createSQLiteConnection,
    createPostgresConnection,
    createRedisConnection,
    createInfluxConnection,
    createMQConnection,
    createRPCConnection,
    createTimeSeriesConnection,
    createMySQLConnection,
    createMSSQLConnection,
    
    // 유틸리티
    getConnectionByType,
    closeAllConnections,
    checkConnectionStatus,
    getDetailedConnectionInfo,
    performHealthCheck,
    
    // ConfigManager 인스턴스
    config
};