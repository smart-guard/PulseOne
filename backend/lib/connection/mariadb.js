// lib/connection/mariadb.js
const mysql = require('mysql2/promise');

class MariaDBConnection {
    constructor(config) {
        this.config = config;
        this.connection = null;
        this.pool = null;
        this.isConnected = false;
    }

    async connect() {
        try {
            // Connection Pool 사용 (권장)
            this.pool = mysql.createPool({
                host: this.config.host,
                port: this.config.port,
                database: this.config.database,
                user: this.config.user,
                password: this.config.password,
                charset: this.config.charset || 'utf8mb4',
                timezone: this.config.timezone || 'local',
                connectionLimit: 10,
                acquireTimeout: 60000,
                timeout: 60000,
                reconnect: true
            });

            // 연결 테스트
            const testConnection = await this.pool.getConnection();
            await testConnection.query('SELECT 1');
            testConnection.release();

            this.isConnected = true;
            console.log('✅ MariaDB/MySQL 연결 성공');
            return this.pool;
        } catch (error) {
            console.error('❌ MariaDB/MySQL 연결 실패:', error.message);
            throw error;
        }
    }

    async query(sql, params = []) {
        if (!this.isConnected) {
            await this.connect();
        }

        try {
            const [rows, fields] = await this.pool.execute(sql, params);
            return {
                rows: rows,
                rowCount: Array.isArray(rows) ? rows.length : rows.affectedRows,
                fields: fields
            };
        } catch (error) {
            console.error('MariaDB/MySQL 쿼리 오류:', error.message);
            throw error;
        }
    }

    async close() {
        if (this.pool) {
            await this.pool.end();
            this.isConnected = false;
            console.log('📴 MariaDB/MySQL 연결 종료');
        }
    }
}

module.exports = MariaDBConnection;