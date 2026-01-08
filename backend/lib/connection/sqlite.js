// ===========================================================================
// backend/lib/connection/sqlite.js - 경로 문제 완전 해결
// 🔥 상대 경로를 process.cwd() 기준으로 올바르게 처리
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
                path: config.getDatabaseConfig().sqlite.path, // ConfigManager의 통합 경로 사용
                timeout: config.getNumber('SQLITE_BUSY_TIMEOUT', 5000),
                journalMode: config.get('SQLITE_JOURNAL_MODE', 'WAL'),
                foreignKeys: config.getBoolean('SQLITE_FOREIGN_KEYS', true),
                cacheSize: config.getNumber('SQLITE_CACHE_SIZE', 2000)
            };
        }

        this.connection = null;
        this.isConnected = false;

        // 🔥 핵심 수정: ConfigManager를 통한 경로 처리 (Docker/Windows 대응)
        this.resolvedPath = this.config.path;

        console.log(`📋 SQLite 연결 설정:
   설정 경로: ${this.config.path}
   작업 디렉토리: ${process.cwd()}
   해석된 경로: ${this.resolvedPath}
   저널 모드: ${this.config.journalMode}
   외래키: ${this.config.foreignKeys ? '활성화' : '비활성화'}`);
    }

    /**
     * 경로 해석 - 상대 경로를 process.cwd() 기준으로 절대 경로로 변환
     */
    _resolvePath(configPath) {
        // 이미 절대 경로인 경우 그대로 사용
        if (path.isAbsolute(configPath)) {
            return configPath;
        }

        // 상대 경로인 경우 process.cwd() 기준으로 해석
        const resolved = path.resolve(process.cwd(), configPath);

        console.log(`🔍 경로 해석:
   원본: ${configPath}
   기준: ${process.cwd()}
   결과: ${resolved}`);

        return resolved;
    }

    /**
     * 디렉토리 생성 및 권한 확인
     */
    async _ensureDirectory() {
        try {
            const dbDir = path.dirname(this.resolvedPath);

            // 디렉토리가 존재하지 않으면 생성
            if (!fs.existsSync(dbDir)) {
                fs.mkdirSync(dbDir, { recursive: true });
                console.log(`✅ SQLite 디렉토리 생성: ${dbDir}`);
            }

            // 쓰기 권한 확인
            try {
                fs.accessSync(dbDir, fs.constants.W_OK);
                console.log(`✅ 디렉토리 쓰기 권한 확인: ${dbDir}`);
            } catch (permError) {
                console.warn(`⚠️ 디렉토리 쓰기 권한 없음: ${dbDir}`);
                throw new Error(`SQLite 디렉토리 쓰기 권한이 없습니다: ${dbDir}`);
            }

            return true;
        } catch (error) {
            console.error(`❌ 디렉토리 생성/권한 확인 실패: ${error.message}`);
            throw error;
        }
    }

    async connect() {
        try {
            // 디렉토리 생성 및 권한 확인
            await this._ensureDirectory();

            return new Promise((resolve, reject) => {
                console.log(`🔧 SQLite 연결 시도: ${this.resolvedPath}`);

                this.connection = new sqlite3.Database(
                    this.resolvedPath,
                    sqlite3.OPEN_READWRITE | sqlite3.OPEN_CREATE,
                    (err) => {
                        if (err) {
                            console.error('❌ SQLite 연결 실패:', err.message);
                            console.error('   에러 코드:', err.code);
                            console.error('   시도한 경로:', this.resolvedPath);
                            reject(err);
                        } else {
                            console.log('✅ SQLite 연결 성공');
                            console.log(`   파일 위치: ${this.resolvedPath}`);

                            this.isConnected = true;
                            this._applyPragmas();
                            resolve(this.connection);
                        }
                    }
                );
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
                this.connection.run('PRAGMA foreign_keys = ON');
            }

            // 성능 최적화
            this.connection.run(`PRAGMA cache_size = ${this.config.cacheSize}`);
            this.connection.run('PRAGMA temp_store = memory');
            this.connection.run('PRAGMA mmap_size = 268435456'); // 256MB

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
                this.connection.run(sql, params, function (err) {
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

    // DatabaseAbstractionLayer 호환성을 위한 메서드들
    async all(sql, params = []) {
        if (!this.isConnected) {
            await this.connect();
        }

        return new Promise((resolve, reject) => {
            this.connection.all(sql, params, (err, rows) => {
                if (err) {
                    console.error('❌ SQLite all() 오류:', err.message);
                    console.error('   쿼리:', sql);
                    console.error('   파라미터:', params);
                    reject(err);
                } else {
                    resolve(rows || []);
                }
            });
        });
    }

    async run(sql, params = []) {
        if (!this.isConnected) {
            await this.connect();
        }

        return new Promise((resolve, reject) => {
            this.connection.run(sql, params, function (err) {
                if (err) {
                    console.error('❌ SQLite run() 오류:', err.message);
                    console.error('   쿼리:', sql);
                    console.error('   파라미터:', params);
                    reject(err);
                } else {
                    resolve({
                        lastID: this.lastID,
                        changes: this.changes
                    });
                }
            });
        });
    }

    async get(sql, params = []) {
        if (!this.isConnected) {
            await this.connect();
        }

        return new Promise((resolve, reject) => {
            this.connection.get(sql, params, (err, row) => {
                if (err) {
                    console.error('❌ SQLite get() 오류:', err.message);
                    reject(err);
                } else {
                    resolve(row || null);
                }
            });
        });
    }

    exec(sql, callback) {
        if (!this.isConnected) {
            this.connect().then(() => {
                this.connection.exec(sql, callback);
            }).catch(callback);
        } else {
            this.connection.exec(sql, callback);
        }
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
                        this.connection = null;
                        resolve();
                    }
                });
            });
        }
    }

    isReady() {
        return this.isConnected && this.connection !== null;
    }

    getConnectionInfo() {
        return {
            configPath: this.config.path,
            resolvedPath: this.resolvedPath,
            workingDirectory: process.cwd(),
            journalMode: this.config.journalMode,
            foreignKeys: this.config.foreignKeys,
            isConnected: this.isConnected
        };
    }

    async ping() {
        try {
            if (!this.isConnected) {
                await this.connect();
            }

            const result = await this.get('SELECT 1 as ping');
            return result && result.ping === 1;
        } catch (error) {
            console.error('❌ SQLite ping 실패:', error.message);
            throw error;
        }
    }
}

// 인스턴스를 기본으로 export하고, 클래스를 추가로 export
const sqliteConnection = new SQLiteConnection();
module.exports = sqliteConnection;