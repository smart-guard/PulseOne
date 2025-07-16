// ===========================================================================
// lib/connection/postgres.js
// ===========================================================================
const { Pool } = require('pg');
const env = require('../../../config/env');

class PostgresDB {
  constructor() {
    this.pool = new Pool({
      host: env.POSTGRES_MAIN_DB_HOST || 'localhost',
      port: env.POSTGRES_MAIN_DB_PORT || 5432,
      user: env.POSTGRES_MAIN_DB_USER || 'user',
      password: env.POSTGRES_MAIN_DB_PASSWORD || 'password',
      database: env.POSTGRES_MAIN_DB_NAME || 'pulseone',
      max: 20, // 최대 연결 수
      idleTimeoutMillis: 30000,
      connectionTimeoutMillis: 2000,
    });

    // 연결 이벤트 핸들러
    this.pool.on('connect', () => {
      console.log('✅ PostgreSQL 클라이언트 연결됨');
    });

    this.pool.on('error', (err) => {
      console.error('❌ PostgreSQL Pool 오류:', err.message);
    });
  }

  async query(text, params) {
    const client = await this.pool.connect();
    try {
      const start = Date.now();
      const res = await client.query(text, params);
      const duration = Date.now() - start;
      console.log('📊 Query 실행:', { text, duration, rows: res.rowCount });
      return res;
    } catch (error) {
      console.error('❌ Query 실패:', error.message);
      throw error;
    } finally {
      client.release();
    }
  }

  async transaction(callback) {
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

  async close() {
    await this.pool.end();
    console.log('✅ PostgreSQL 연결 풀 종료됨');
  }
}

module.exports = new PostgresDB();