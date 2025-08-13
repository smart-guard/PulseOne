// lib/connection/sqlite.js
const sqlite3 = require('sqlite3').verbose();
const fs = require('fs');
const path = require('path');

class SqliteConnection {
    constructor(config) {
        this.config = config;
        this.db = null;
        this.isConnected = false;
    }

    async connect() {
        return new Promise((resolve, reject) => {
            try {
                // 디렉토리 생성 확인
                const dbDir = path.dirname(this.config.path);
                if (!fs.existsSync(dbDir)) {
                    fs.mkdirSync(dbDir, { recursive: true });
                }

                this.db = new sqlite3.Database(this.config.path, sqlite3.OPEN_READWRITE | sqlite3.OPEN_CREATE, (err) => {
                    if (err) {
                        console.error('❌ SQLite 연결 실패:', err.message);
                        reject(err);
                    } else {
                        this.isConnected = true;
                        this._applySQLitePragmas();
                        console.log('✅ SQLite 연결 성공');
                        resolve(this.db);
                    }
                });
            } catch (error) {
                console.error('❌ SQLite 모듈 로드 실패:', error.message);
                reject(error);
            }
        });
    }

    _applySQLitePragmas() {
        // 성능 최적화 PRAGMA 설정
        if (this.config.journalMode) {
            this.db.run(`PRAGMA journal_mode = ${this.config.journalMode}`);
        }
        if (this.config.busyTimeout) {
            this.db.run(`PRAGMA busy_timeout = ${this.config.busyTimeout}`);
        }
        if (this.config.foreignKeys) {
            this.db.run(`PRAGMA foreign_keys = ON`);
        }
        console.log('✅ SQLite PRAGMA 설정 적용');
    }

    async query(sql, params = []) {
        if (!this.isConnected) {
            await this.connect();
        }

        return new Promise((resolve, reject) => {
            if (sql.trim().toUpperCase().startsWith('SELECT')) {
                this.db.all(sql, params, (err, rows) => {
                    if (err) {
                        console.error('SQLite 쿼리 오류:', err.message);
                        reject(err);
                    } else {
                        resolve({ rows, rowCount: rows.length });
                    }
                });
            } else {
                this.db.run(sql, params, function(err) {
                    if (err) {
                        console.error('SQLite 쿼리 오류:', err.message);
                        reject(err);
                    } else {
                        resolve({ 
                            rows: [], 
                            rowCount: this.changes,
                            lastID: this.lastID 
                        });
                    }
                });
            }
        });
    }

    async close() {
        if (this.db) {
            return new Promise((resolve, reject) => {
                this.db.close((err) => {
                    if (err) {
                        reject(err);
                    } else {
                        this.isConnected = false;
                        console.log('📴 SQLite 연결 종료');
                        resolve();
                    }
                });
            });
        }
    }
}

module.exports = SqliteConnection;