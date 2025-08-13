// ===========================================================================
// backend/lib/connection/db.js - ConfigManager 사용 + initializeConnections 추가
// ===========================================================================
const ConfigManager = require('../config/ConfigManager');

const config = ConfigManager.getInstance();

console.log('🔗 데이터베이스 연결 모듈 로드 중...');

// 데이터베이스 타입에 따른 연결 함수들
const connectionMap = {
    redis: () => {
        try {
            return require('./redis');
        } catch (error) {
            console.warn('⚠️ Redis 연결 모듈 로드 실패:', error.message);
            return null;
        }
    },
    
    postgres: () => {
        try {
            return require('./postgres');
        } catch (error) {
            console.warn('⚠️ PostgreSQL 연결 모듈 로드 실패:', error.message);
            return null;
        }
    },
    
    influx: () => {
        try {
            return require('./influx');
        } catch (error) {
            console.warn('⚠️ InfluxDB 연결 모듈 로드 실패:', error.message);
            return null;
        }
    },
    
    mq: () => {
        try {
            return require('./mq');
        } catch (error) {
            console.warn('⚠️ RabbitMQ 연결 모듈 로드 실패:', error.message);
            return null;
        }
    },
    
    rpc: () => {
        try {
            return require('./rpc');
        } catch (error) {
            console.warn('⚠️ RPC 연결 모듈 로드 실패:', error.message);
            return null;
        }
    },
    
    timeseries: () => {
        try {
            return require('./timeseries');
        } catch (error) {
            console.warn('⚠️ TimeSeries 연결 모듈 로드 실패:', error.message);
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

    // 각 연결 타입별로 초기화
    for (const [type, connectionFn] of Object.entries(connectionMap)) {
        try {
            console.log(`🔄 ${type} 연결 초기화 중...`);
            
            const connection = connectionFn();
            
            if (connection) {
                // 연결이 성공적으로 로드된 경우
                connections[type] = connection;
                
                // 연결 테스트 (가능한 경우)
                if (connection.connect && typeof connection.connect === 'function') {
                    await connection.connect();
                } else if (connection.ping && typeof connection.ping === 'function') {
                    await connection.ping();
                } else if (connection.initialize && typeof connection.initialize === 'function') {
                    await connection.initialize();
                }
                
                results.push({ type, status: 'success', connection });
                console.log(`✅ ${type} 연결 성공`);
            } else {
                results.push({ type, status: 'failed', error: 'Connection returned null' });
                console.log(`❌ ${type} 연결 실패`);
            }
            
        } catch (error) {
            results.push({ type, status: 'error', error: error.message });
            console.error(`❌ ${type} 연결 오류:`, error.message);
        }
    }

    // 연결 결과 요약
    const successful = results.filter(r => r.status === 'success').length;
    const total = results.length;
    
    console.log(`\n📊 연결 초기화 완료: ${successful}/${total} 성공`);
    
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
        .filter(r => r.status !== 'success')
        .map(r => r.type)
        .join(', ');
        
    if (failedServices) {
        console.log(`⚠️  연결 실패: ${failedServices}`);
    }

    console.log(''); // 빈 줄 추가

    return connections;
}

/**
 * 개별 연결 생성 함수들
 */
async function createRedisConnection() {
    return connectionMap.redis();
}

async function createPostgresConnection() {
    return connectionMap.postgres();
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
            } else {
                status[type] = true; // 연결 객체가 존재하면 연결된 것으로 간주
            }
        } else {
            status[type] = false;
        }
    }
    
    return status;
}

module.exports = {
    // 메인 함수 (app.js에서 사용)
    initializeConnections,
    
    // 개별 연결 생성
    createRedisConnection,
    createPostgresConnection,
    createInfluxConnection,
    createMQConnection,
    createRPCConnection,
    createTimeSeriesConnection,
    
    // 유틸리티
    closeAllConnections,
    checkConnectionStatus,
    
    // ConfigManager 인스턴스
    config
};