// ===========================================================================
// lib/connection/mariadb.js
// ===========================================================================
const mariadb = require('mariadb');
const env = require('../../../config/env');

class MariaDB {
  constructor() {
    this.pool = mariadb.createPool({
      host: env.DB_HOST || 'localhost',
      user: env.DB_USER || 'root',
      password: env.DB_PASSWORD || '',
      database: env.DB_NAME || 'pulseone',
      port: env.DB_PORT || 3306,
      connectionLimit: 10,
      acquireTimeout: 30000,
      timeout: 30000,
      reconnect: true
    });

    console.log(`✅ MariaDB 연결 풀 생성: ${env.DB_HOST || 'localhost'}:${env.DB_PORT || 3306}`);
  }

  async query(sql, params = []) {
    let conn;
    try {
      conn = await this.pool.getConnection();
      const start = Date.now();
      const result = await conn.query(sql, params);
      const duration = Date.now() - start;
      
      console.log('📊 MariaDB Query 실행:', { 
        sql: sql.substring(0, 100) + '...', 
        duration, 
        rows: Array.isArray(result) ? result.length : 1 
      });
      
      return result;
    } catch (error) {
      console.error('❌ MariaDB Query 실패:', error.message);
      throw error;
    } finally {
      if (conn) conn.release();
    }
  }

  async transaction(callback) {
    let conn;
    try {
      conn = await this.pool.getConnection();
      await conn.beginTransaction();
      
      const result = await callback(conn);
      
      await conn.commit();
      return result;
    } catch (error) {
      if (conn) await conn.rollback();
      console.error('❌ MariaDB 트랜잭션 실패:', error.message);
      throw error;
    } finally {
      if (conn) conn.release();
    }
  }

  async close() {
    try {
      await this.pool.end();
      console.log('✅ MariaDB 연결 풀 종료됨');
    } catch (error) {
      console.error('❌ MariaDB 종료 실패:', error.message);
    }
  }
}

module.exports = new MariaDB();