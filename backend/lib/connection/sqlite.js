const sqlite3 = require('sqlite3').verbose();
const env = require('../../../config/env');

class SQLiteDB {
  constructor() {
    this.db = new sqlite3.Database(env.SQLITE_PATH || './db.sqlite');
  }

  query(sql, params = []) {
    return new Promise((resolve, reject) => {
      this.db.all(sql, params, (err, rows) => {
        if (err) return reject(err);
        resolve(rows);
      });
    });
  }

  close() {
    return new Promise((resolve, reject) => {
      this.db.close(err => (err ? reject(err) : resolve()));
    });
  }
}

module.exports = SQLiteDB;
