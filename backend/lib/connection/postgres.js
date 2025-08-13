// lib/connection/postgres.js
const { Client, Pool } = require('pg');

class PostgresConnection {
    constructor(config) {
        this.config = config;
        this.client = null;
        this.pool = null;
        this.isConnected = false;
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
                max: 10,
                idleTimeoutMillis: 30000,
                connectionTimeoutMillis: 5000,
            });

            // 연결 테스트
            const testClient = await this.pool.connect();
            await testClient.query('SELECT NOW()');
            testClient.release();

            this.isConnected = true;
            console.log('✅ PostgreSQL 연결 성공');
            return this.pool;
        } catch (error) {
            console.error('❌ PostgreSQL 연결 실패:', error.message);
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
            console.error('PostgreSQL 쿼리 오류:', error.message);
            throw error;
        }
    }

    async close() {
        if (this.pool) {
            await this.pool.end();
            this.isConnected = false;
            console.log('📴 PostgreSQL 연결 종료');
        }
    }
}

module.exports = PostgresConnection;