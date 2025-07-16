// lib/connection/db.js
const path = require('path');
const env = require('../../../config/env');


// DB 타입에 따라 적절한 클래스 반환
function getDB() {
  const dbType = env.DB_TYPE || 'postgres';
  
  switch (dbType) {
    case 'postgres':
      const PostgresDB = require('./postgres');
      return new PostgresDB();
    case 'mariadb':
      const MariaDB = require('./mariadb');
      return new MariaDB();
    case 'sqlite':
      const SQLiteDB = require('./sqlite');
      return new SQLiteDB();
    default:
      throw new Error(`Unsupported DB type: ${dbType}`);
  }
}

module.exports = getDB();