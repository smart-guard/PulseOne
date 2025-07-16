const mariadb = require('mariadb');
const env = require('../../../config/env');

class MariaDB {
  constructor() {
    this.pool = mariadb.createPool({
      host: env.DB_HOST,
      user: env.DB_USER,
      password: env.DB_PASSWORD,
      database: env.DB_NAME,
      port: env.DB_PORT || 3306,
    });
  }

  async query(sql, params) {
    const conn = await this.pool.getConnection();
    try {
      return await conn.query(sql, params);
    } finally {
      conn.release();
    }
  }

  async close() {
    await this.pool.end();
  }
}

module.exports = MariaDB;
