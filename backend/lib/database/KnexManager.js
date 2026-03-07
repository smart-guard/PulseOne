/**
 * backend/lib/database/KnexManager.js
 * Knex.js 인스턴스 중앙 관리자 - 멀티 DB 지원
 */

const knex = require('knex');
const ConfigManager = require('../config/ConfigManager');

class KnexManager {
    constructor() {
        this.configManager = ConfigManager.getInstance();
        this.instances = new Map();
    }

    /**
     * Singleton 인스턴스 반환
     */
    static getInstance() {
        if (!this.instance) {
            this.instance = new KnexManager();
        }
        return this.instance;
    }

    /**
     * 특정 DB 타입에 대한 Knex 인스턴스 반환
     * @param {string} dbType 'sqlite', 'postgres', 'mariadb' 등
     */
    getKnex(dbType = null) {
        const type = (dbType || this.configManager.get('DATABASE_TYPE', 'sqlite')).toLowerCase();

        if (this.instances.has(type)) {
            return this.instances.get(type);
        }

        const knexConfig = this._createKnexConfig(type);
        const instance = knex(knexConfig);

        this.instances.set(type, instance);
        return instance;
    }

    /**
     * Knex 설정 생성
     */
    _createKnexConfig(type) {
        switch (type) {
            case 'sqlite':
            case 'sqlite3':
                return {
                    client: 'sqlite3',
                    connection: {
                        filename: this.configManager.getDatabaseConfig().sqlite.path
                    },
                    useNullAsDefault: true,
                    pool: {
                        afterCreate: (conn, cb) => {
                            conn.run('PRAGMA journal_mode = WAL', (err) => {
                                if (err) return cb(err);
                                conn.run('PRAGMA busy_timeout = 5000', cb);
                            });
                        }
                    }
                };

            case 'postgresql':
            case 'postgres':
                const pg = this.configManager.getDatabaseConfig().postgresql;
                return {
                    client: 'pg',
                    connection: {
                        host: pg.host,
                        port: pg.port,
                        user: pg.user,
                        password: pg.password,
                        database: pg.database,
                        ssl: pg.ssl ? { rejectUnauthorized: false } : false
                    },
                    pool: { min: pg.poolMin, max: pg.poolMax }
                };

            case 'mariadb':
            case 'mysql':
                const maria = this.configManager.getDatabaseConfig().mariadb;
                return {
                    client: 'mysql2',
                    connection: {
                        host: maria.host,
                        port: maria.port,
                        user: maria.user,
                        password: maria.password,
                        database: maria.database
                    },
                    pool: { min: 2, max: maria.connectionLimit || 10 }
                };

            default:
                throw new Error(`Knex는 아직 ${type} 타입을 지원하지 않습니다.`);
        }
    }

    /**
     * DB 타입별 현재 시간 raw 표현 반환
     * knex.raw("datetime('now','localtime')") 대신 사용 — 크로스 DB 호환
     *
     * @param {object} knexInstance - Knex 인스턴스
     * @returns {object} knex.raw() 또는 knex.fn.now() 결과
     *
     * 사용 예:
     *   updated_at: KnexManager.getInstance().nowRaw(this.knex)
     */
    nowRaw(knexInstance) {
        const type = (this.configManager.get('DATABASE_TYPE', 'sqlite')).toLowerCase();
        switch (type) {
            case 'sqlite':
            case 'sqlite3':
                return knexInstance.raw("datetime('now', 'localtime')");
            case 'postgresql':
            case 'postgres':
                return knexInstance.raw('NOW()');
            case 'mariadb':
            case 'mysql':
                return knexInstance.raw('NOW()');
            case 'mssql':
            case 'sqlserver':
                return knexInstance.raw('GETDATE()');
            default:
                return knexInstance.raw('NOW()');
        }
    }

    /**
     * 모든 연결 종료
     */
    async destroyAll() {
        for (const [type, instance] of this.instances) {
            await instance.destroy();
            console.log(`✅ Knex instance (${type}) destroyed`);
        }
        this.instances.clear();
    }
}

module.exports = KnexManager;
