const { Pool } = require('pg');
const { env } = require('../config');

class PostgresDB {
  constructor() {
    this.pool = new Pool({
      host: env.DB_HOST,
      user: env.DB_USER,
      password: env.DB_PASSWORD,
      database: env.DB_NAME,
      port: env.DB_PORT || 5432,
    });
  }

  async query(sql, params) {
    return this.pool.query(sql, params);
  }

  async close() {
    await this.pool.end();
  }
}

module.exports = PostgresDB;
