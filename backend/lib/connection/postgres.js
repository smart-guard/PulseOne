// ===========================================================================
// backend/lib/connection/postgres.js - ConfigManager 사용하도록 수정
// ===========================================================================
const { Client, Pool } = require('pg');
const ConfigManager = require('../config/ConfigManager');

const config = ConfigManager.getInstance();

class PostgresConnection {
    constructor(customConfig = null) {
        // customConfig가 있으면 사용, 없으면 ConfigManager에서 로드
        if (customConfig) {
            this.config = customConfig;
        } else {
            this.config = {
                host: config.get('POSTGRES_MAIN_DB_HOST', 'localhost'),
                port: config.getNumber('POSTGRES_MAIN_DB_PORT', 5432),
                database: config.get('POSTGRES_MAIN_DB_NAME', 'pulseone'),
                user: config.get('POSTGRES_MAIN_DB_USER', 'postgres'),
                password: config.get('POSTGRES_MAIN_DB_PASSWORD', '')
            };
        }
        
        this.client = null;
        this.pool = null;
        this.isConnected = false;
        
        console.log(`📋 PostgreSQL 연결 설정:
   호스트: ${this.config.host}:${this.config.port}
   데이터베이스: ${this.config.database}
   사용자: ${this.config.user}`);
    }

    async connect() {
        try {
            // Pool 연결 사용 (권장)
            this.pool = new Pool({
                host: this.config.host,
                port: this.config.port,
                database: this.config.database,
                user: this.config.user,
                password: this.config.password,
                max: 10,                    // 최대 연결 수
                idleTimeoutMillis: 30000,   // 유휴 타임아웃
                connectionTimeoutMillis: 5000, // 연결 타임아웃
            });

            // 연결 테스트
            const testClient = await this.pool.connect();
            const result = await testClient.query('SELECT NOW()');
            testClient.release();

            this.isConnected = true;
            console.log('✅ PostgreSQL 연결 성공');
            console.log(`   현재 시간: ${result.rows[0].now}`);
            
            return this.pool;
        } catch (error) {
            console.error('❌ PostgreSQL 연결 실패:', error.message);
            console.log('⚠️  PostgreSQL 없이 계속 진행합니다.');
            throw error;
        }
    }

    async query(text, params = []) {
        if (!this.isConnected) {
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

    // 트랜잭션 지원
    async transaction(callback) {
        if (!this.isConnected) {
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

    // 배치 처리
    async batchInsert(tableName, columns, values) {
        if (!this.isConnected) {
            await this.connect();
        }

        const placeholders = values.map((_, i) => 
            `(${columns.map((_, j) => `$${i * columns.length + j + 1}`).join(', ')})`
        ).join(', ');

        const query = `INSERT INTO ${tableName} (${columns.join(', ')}) VALUES ${placeholders}`;
        const flatValues = values.flat();

        return await this.query(query, flatValues);
    }

    async close() {
        if (this.pool) {
            await this.pool.end();
            this.isConnected = false;
            console.log('📴 PostgreSQL 연결 종료');
        }
    }

    // 연결 상태 확인
    isReady() {
        return this.isConnected && this.pool;
    }

    // 연결 정보 조회
    getConnectionInfo() {
        return {
            host: this.config.host,
            port: this.config.port,
            database: this.config.database,
            user: this.config.user,
            isConnected: this.isConnected
        };
    }
}

// 싱글톤 인스턴스 생성
const postgresConnection = new PostgresConnection();

// 기존 인터페이스 호환성 유지
module.exports = postgresConnection;

// 추가 export (팩토리 패턴 지원)
module.exports.PostgresConnection = PostgresConnection;