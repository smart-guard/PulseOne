// ===========================================================================
// backend/lib/connection/sqlite.js - SQLite 연결 클래스
// ===========================================================================
const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const fs = require('fs');
const ConfigManager = require('../config/ConfigManager');

const config = ConfigManager.getInstance();

class SQLiteConnection {
    constructor(customConfig = null) {
        // customConfig가 있으면 사용, 없으면 ConfigManager에서 로드
        if (customConfig) {
            this.config = customConfig;
        } else {
            this.config = {
                path: config.get('SQLITE_PATH', './data/db/pulseone.db'),
                timeout: config.getNumber('SQLITE_BUSY_TIMEOUT', 5000),
                journalMode: config.get('SQLITE_JOURNAL_MODE', 'WAL'),
                foreignKeys: config.getBoolean('SQLITE_FOREIGN_KEYS', true),
                cacheSize: config.getNumber('SQLITE_CACHE_SIZE', 2000)
            };
        }
        
        this.connection = null;
        this.isConnected = false;
        
        console.log(`📋 SQLite 연결 설정:
   파일: ${this.config.path}
   저널 모드: ${this.config.journalMode}
   외래키: ${this.config.foreignKeys ? '활성화' : '비활성화'}`);
    }

    async connect() {
        try {
            // 디렉토리 생성
            const dbDir = path.dirname(this.config.path);
            if (!fs.existsSync(dbDir)) {
                fs.mkdirSync(dbDir, { recursive: true });
                console.log(`✅ SQLite 디렉토리 생성: ${dbDir}`);
            }

            // 절대 경로로 변환
            const absolutePath = path.resolve(this.config.path);
            
            return new Promise((resolve, reject) => {
                this.connection = new sqlite3.Database(absolutePath, sqlite3.OPEN_READWRITE | sqlite3.OPEN_CREATE, (err) => {
                    if (err) {
                        console.error('❌ SQLite 연결 실패:', err.message);
                        reject(err);
                    } else {
                        console.log('✅ SQLite 연결 성공');
                        console.log(`   파일 위치: ${absolutePath}`);
                        
                        this.isConnected = true;
                        this._applyPragmas();
                        resolve(this.connection);
                    }
                });
            });
        } catch (error) {
            console.error('❌ SQLite 연결 설정 실패:', error.message);
            throw error;
        }
    }

    _applyPragmas() {
        if (!this.connection) return;

        try {
            // PRAGMA 설정 적용
            this.connection.run(`PRAGMA journal_mode = ${this.config.journalMode}`);
            this.connection.run(`PRAGMA busy_timeout = ${this.config.timeout}`);
            
            if (this.config.foreignKeys) {
                this.connection.run(`PRAGMA foreign_keys = ON`);
            }
            
            // 성능 최적화
            this.connection.run(`PRAGMA cache_size = 10000`);
            this.connection.run(`PRAGMA temp_store = memory`);
            this.connection.run(`PRAGMA mmap_size = 268435456`); // 256MB
            
            console.log('✅ SQLite PRAGMA 설정 적용 완료');
        } catch (error) {
            console.warn('⚠️ SQLite PRAGMA 설정 실패:', error.message);
        }
    }

    async query(sql, params = []) {
        if (!this.isConnected) {
            await this.connect();
        }
        
        return new Promise((resolve, reject) => {
            // SELECT 쿼리
            if (sql.trim().toUpperCase().startsWith('SELECT')) {
                this.connection.all(sql, params, (err, rows) => {
                    if (err) {
                        console.error('❌ SQLite SELECT 오류:', err.message);
                        console.error('   쿼리:', sql);
                        console.error('   파라미터:', params);
                        reject(err);
                    } else {
                        resolve({
                            rows: rows,
                            rowCount: rows.length
                        });
                    }
                });
            } 
            // INSERT, UPDATE, DELETE 쿼리
            else {
                this.connection.run(sql, params, function(err) {
                    if (err) {
                        console.error('❌ SQLite 실행 오류:', err.message);
                        console.error('   쿼리:', sql);
                        console.error('   파라미터:', params);
                        reject(err);
                    } else {
                        resolve({
                            lastID: this.lastID,
                            changes: this.changes,
                            rowCount: this.changes
                        });
                    }
                });
            }
        });
    }

    // 트랜잭션 지원
    async transaction(callback) {
        if (!this.isConnected) {
            await this.connect();
        }

        return new Promise(async (resolve, reject) => {
            this.connection.serialize(async () => {
                this.connection.run('BEGIN TRANSACTION');
                
                try {
                    const result = await callback(this);
                    this.connection.run('COMMIT', (err) => {
                        if (err) reject(err);
                        else resolve(result);
                    });
                } catch (error) {
                    this.connection.run('ROLLBACK', (rollbackErr) => {
                        if (rollbackErr) {
                            console.error('❌ SQLite 롤백 실패:', rollbackErr.message);
                        }
                        reject(error);
                    });
                }
            });
        });
    }

    // 배치 실행
    async batchExecute(queries) {
        if (!this.isConnected) {
            await this.connect();
        }

        return new Promise((resolve, reject) => {
            this.connection.serialize(() => {
                this.connection.run('BEGIN TRANSACTION');
                
                const results = [];
                let completed = 0;
                let hasError = false;

                queries.forEach((queryObj, index) => {
                    const { sql, params = [] } = queryObj;
                    
                    this.connection.run(sql, params, function(err) {
                        if (err && !hasError) {
                            hasError = true;
                            this.connection.run('ROLLBACK');
                            reject(err);
                            return;
                        }
                        
                        results[index] = {
                            lastID: this.lastID,
                            changes: this.changes
                        };
                        
                        completed++;
                        
                        if (completed === queries.length && !hasError) {
                            this.connection.run('COMMIT', (commitErr) => {
                                if (commitErr) reject(commitErr);
                                else resolve(results);
                            });
                        }
                    });
                });
            });
        });
    }

    async close() {
        if (this.connection) {
            return new Promise((resolve, reject) => {
                this.connection.close((err) => {
                    if (err) {
                        console.error('❌ SQLite 종료 오류:', err.message);
                        reject(err);
                    } else {
                        console.log('📴 SQLite 연결 종료');
                        this.isConnected = false;
                        resolve();
                    }
                });
            });
        }
    }

    isReady() {
        return this.isConnected && this.connection;
    }

    getConnectionInfo() {
        return {
            path: this.config.path,
            journalMode: this.config.journalMode,
            foreignKeys: this.config.foreignKeys,
            isConnected: this.isConnected
        };
    }
}

// 싱글톤 인스턴스 생성
const sqliteConnection = new SQLiteConnection();

module.exports = sqliteConnection;
module.exports.SQLiteConnection = SQLiteConnection;
// SQLiteConnection 클래스 export
module.exports = SQLiteConnection;

