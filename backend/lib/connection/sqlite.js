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
                timeout: config.getNumber('SQLITE_BUSY_TIMEOUT', 10000),
                journalMode: config.get('SQLITE_JOURNAL_MODE', 'WAL'),
                foreignKeys: config.getBoolean('SQLITE_FOREIGN_KEYS', true),
                cacheSize: config.getNumber('SQLITE_CACHE_SIZE', 2000)
            };
        }

        this.connection = null;
        this.isConnected = false;

        // 🔥 핵심 수정: ConfigManager를 통한 경로 처리 (Docker/Windows 대응)
        this.resolvedPath = this._resolvePath(this.config.path);

        console.log(`🔧 SQLite 연결 시도: ${this.resolvedPath}`);
    }

    /**
     * 경로 해석 - 상대 경로를 process.cwd() 기준으로 절대 경로로 변환
     */
    _resolvePath(configPath) {
        // :memory: 인 경우 그대로 반환 (SQLite 특수 경로)
        if (configPath === ':memory:') {
            return configPath;
        }

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

                            // 🔥 핵심: DB 오픈 직후 네이티브 레벨 busyTimeout 설정
                            // PRAGMA busy_timeout보다 먼저 적용되어 Statement 레벨
                            // SQLITE_BUSY 에러 자체를 방지 (내부적으로 대기 후 재시도)
                            this.connection.configure('busyTimeout', this.config.timeout || 10000);

                            // Database 인스턴스 에러 핸들러 (프로세스 크래시 방지)
                            this.connection.on('error', (dbErr) => {
                                console.error(`⚠️ [SQLite] Database error event (non-fatal): ${dbErr.message}`);
                            });

                            this.isConnected = true;
                            // PRAGMA를 serialize()로 순서 보장 후 resolve
                            this._applyPragmas(resolve);
                        }
                    }
                );
            });
        } catch (error) {
            console.error('❌ SQLite 연결 설정 실패:', error.message);
            throw error;
        }
    }

    _applyPragmas(onDone) {
        if (!this.connection) {
            if (onDone) onDone(this.connection);
            return;
        }

        // serialize()로 PRAGMA들을 직렬 실행하여 busy_timeout이
        // 첫 번째 쿼리보다 반드시 먼저 적용됨을 보장
        this.connection.serialize(() => {
            // busy_timeout을 가장 먼저 — Collector와의 락 충돌 방지
            this.connection.run(`PRAGMA busy_timeout = ${this.config.timeout}`);
            this.connection.run(`PRAGMA journal_mode = ${this.config.journalMode}`);

            if (this.config.foreignKeys) {
                this.connection.run('PRAGMA foreign_keys = ON');
            }

            // 성능 최적화
            this.connection.run(`PRAGMA cache_size = ${this.config.cacheSize}`);
            this.connection.run('PRAGMA temp_store = memory');
            this.connection.run('PRAGMA mmap_size = 268435456', (err) => {
                // 모든 PRAGMA 완료 후 resolve
                if (err) {
                    console.warn('⚠️ SQLite mmap_size PRAGMA 실패 (무시):', err.message);
                }
                console.log('✅ SQLite PRAGMA 설정 적용 완료');
                if (onDone) onDone(this.connection);
            });
        });
    }

    async query(sql, params = []) {
        if (!this.isConnected) {
            await this.connect();
        }

        return new Promise((resolve, reject) => {
            const sqlTrimmed = sql.trim().toUpperCase();
            // SELECT 또는 PRAGMA 쿼리 (결과가 있는 쿼리)
            if (sqlTrimmed.startsWith('SELECT') || sqlTrimmed.startsWith('PRAGMA')) {
                this.connection.all(sql, params, (err, rows) => {
                    if (err) {
                        console.error('❌ SQLite SELECT/PRAGMA 오류:', err.message);
                        console.error('   쿼리:', sql);
                        console.error('   파라미터:', params);
                        reject(err);
                    } else {
                        resolve({
                            rows: rows,
                            rowCount: rows ? rows.length : 0
                        });
                    }
                });
            }
            // INSERT, UPDATE, DELETE 쿼리 (결과가 없는 쿼리)
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