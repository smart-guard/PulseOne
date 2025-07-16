// ===========================================================================
// lib/connection/sqlite.js
// ===========================================================================
const sqlite3 = require('sqlite3').verbose();
const env = require('../../../config/env');
const path = require('path');
const fs = require('fs');

class SQLiteDB {
  constructor() {
    this.dbPath = env.SQLITE_PATH || path.join(process.cwd(), 'db.sqlite');
    
    // 디렉토리 확인 및 생성
    const dbDir = path.dirname(this.dbPath);
    if (!fs.existsSync(dbDir)) {
      fs.mkdirSync(dbDir, { recursive: true });
    }

    this.db = new sqlite3.Database(this.dbPath, (err) => {
      if (err) {
        console.error('❌ SQLite 연결 실패:', err.message);
      } else {
        console.log(`✅ SQLite 연결 성공: ${this.dbPath}`);
      }
    });

    // WAL 모드 활성화 (성능 향상)
    this.db.exec('PRAGMA journal_mode = WAL;');
  }

  query(sql, params = []) {
    return new Promise((resolve, reject) => {
      const start = Date.now();
      
      this.db.all(sql, params, (err, rows) => {
        const duration = Date.now() - start;
        
        if (err) {
          console.error('❌ SQLite Query 실패:', err.message);
          return reject(err);
        }
        
        console.log('📊 SQLite Query 실행:', { 
          sql: sql.substring(0, 100) + '...', 
          duration, 
          rows: rows ? rows.length : 0 
        });
        
        resolve(rows);
      });
    });
  }

  run(sql, params = []) {
    return new Promise((resolve, reject) => {
      this.db.run(sql, params, function(err) {
        if (err) {
          console.error('❌ SQLite Run 실패:', err.message);
          return reject(err);
        }
        resolve({ lastID: this.lastID, changes: this.changes });
      });
    });
  }

  transaction(callback) {
    return new Promise(async (resolve, reject) => {
      this.db.serialize(async () => {
        try {
          this.db.run('BEGIN TRANSACTION');
          const result = await callback(this);
          this.db.run('COMMIT', (err) => {
            if (err) reject(err);
            else resolve(result);
          });
        } catch (error) {
          this.db.run('ROLLBACK');
          reject(error);
        }
      });
    });
  }

  close() {
    return new Promise((resolve, reject) => {
      this.db.close((err) => {
        if (err) {
          console.error('❌ SQLite 종료 실패:', err.message);
          reject(err);
        } else {
          console.log('✅ SQLite 연결 종료됨');
          resolve();
        }
      });
    });
  }
}

module.exports = new SQLiteDB();