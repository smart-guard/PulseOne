// ===========================================================================
// backend/lib/connection/postgres.js
// PostgreSQL 연결 관리 (ConfigManager 기반, 싱글톤 인스턴스)
// ===========================================================================

const { Pool } = require('pg');
const ConfigManager = require('../config/ConfigManager');

class PostgresConnection {
    constructor() {
        this.config = this.loadConfig();
        this.pool = null;
        this.isConnected = false;

        console.log(`📋 PostgreSQL 연결 설정:
   호스트: ${this.config.host}:${this.config.port}
   데이터베이스: ${this.config.database}
   사용자: ${this.config.user}`);
    }

    /**
     * 설정 로드
     */
    loadConfig() {
        const configManager = ConfigManager.getInstance();

        return {
            host: configManager.get('POSTGRES_MAIN_DB_HOST', configManager.get('POSTGRES_HOST', 'localhost')),
            port: configManager.getNumber('POSTGRES_MAIN_DB_PORT', configManager.getNumber('POSTGRES_PORT', 5432)),
            database: configManager.get('POSTGRES_MAIN_DB_NAME', configManager.get('POSTGRES_DB', 'pulseone')),
            user: configManager.get('POSTGRES_MAIN_DB_USER', configManager.get('POSTGRES_USER', 'postgres')),
            password: configManager.get('POSTGRES_MAIN_DB_PASSWORD', configManager.get('POSTGRES_PASSWORD', 'postgres123')),
            max: configManager.getNumber('POSTGRES_POOL_MAX', 10),
            idleTimeoutMillis: configManager.getNumber('POSTGRES_IDLE_TIMEOUT', 30000),
            connectionTimeoutMillis: configManager.getNumber('POSTGRES_CONNECT_TIMEOUT', 5000)
        };
    }

    /**
     * 데이터베이스 연결
     */
    async connect() {
        if (this.isConnected && this.pool) {
            return this.pool;
        }

        try {
            console.log('🔄 PostgreSQL 연결 시도...');

            this.pool = new Pool({
                host: this.config.host,
                port: this.config.port,
                database: this.config.database,
                user: this.config.user,
                password: this.config.password,
                max: this.config.max,
                idleTimeoutMillis: this.config.idleTimeoutMillis,
                connectionTimeoutMillis: this.config.connectionTimeoutMillis,
            });

            // 연결 테스트
            const testClient = await this.pool.connect();
            const result = await testClient.query('SELECT NOW() as current_time');
            testClient.release();

            this.isConnected = true;
            console.log('✅ PostgreSQL 연결 성공');
            console.log(`   현재 시간: ${result.rows[0].current_time}`);

            return this.pool;

        } catch (error) {
            this.isConnected = false;
            console.error('❌ PostgreSQL 연결 실패:', error.message);
            console.warn('⚠️  PostgreSQL 없이 계속 진행합니다.');
            throw error;
        }
    }

    /**
     * 쿼리 실행
     */
    async query(text, params = []) {
        if (!this.isConnected || !this.pool) {
            await this.connect();
        }

        try {
            const result = await this.pool.query(text, params);
            return result;
        } catch (error) {
            console.error('❌ PostgreSQL 쿼리 오류:', error.message);
            console.error('   쿼리:', text);
            console.error('   파라미터:', params);
            throw error;
        }
    }

    /**
     * 트랜잭션 실행
     */
    async transaction(callback) {
        if (!this.isConnected || !this.pool) {
            await this.connect();
        }

        const client = await this.pool.connect();

        try {
            await client.query('BEGIN');
            const result = await callback(client);
            await client.query('COMMIT');
            return result;
        } catch (error) {
            await client.query('ROLLBACK');
            throw error;
        } finally {
            client.release();
        }
    }

    /**
     * 배치 삽입
     */
    async batchInsert(tableName, columns, values) {
        if (!this.isConnected || !this.pool) {
            await this.connect();
        }

        const placeholders = values.map((_, i) =>
            `(${columns.map((_, j) => `$${i * columns.length + j + 1}`).join(', ')})`
        ).join(', ');

        const query = `INSERT INTO ${tableName} (${columns.join(', ')}) VALUES ${placeholders}`;
        const flatValues = values.flat();

        return await this.query(query, flatValues);
    }

    /**
     * 연결 종료
     */
    async close() {
        if (this.pool) {
            await this.pool.end();
            this.isConnected = false;
            console.log('📴 PostgreSQL 연결 종료');
        }
    }

    /**
     * 연결 상태 확인
     */
    isReady() {
        return this.isConnected && this.pool;
    }

    /**
     * 연결 정보 조회
     */
    getConnectionInfo() {
        return {
            host: this.config.host,
            port: this.config.port,
            database: this.config.database,
            user: this.config.user,
            isConnected: this.isConnected,
            poolSize: this.config.max
        };
    }

    /**
     * 헬스체크
     */
    async healthCheck() {
        try {
            const result = await this.query('SELECT 1 as health');
            return {
                status: 'healthy',
                responseTime: Date.now(),
                result: result.rows[0]
            };
        } catch (error) {
            return {
                status: 'unhealthy',
                error: error.message,
                responseTime: Date.now()
            };
        }
    }
}

// 싱글톤 인스턴스 생성
const postgresConnection = new PostgresConnection();

// 인스턴스를 export (query 메소드 사용 가능)
module.exports = postgresConnection;

// 클래스도 함께 export (필요한 경우)
module.exports.PostgresConnection = PostgresConnection;