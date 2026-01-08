const ConfigManager = require('../config/ConfigManager');
// backend/lib/database/DatabaseFactory.js
// 데이터베이스 팩토리 - 모든 DB 타입을 통합 관리

const path = require('path');
const QueryAdapter = require('./queryAdapter');

// 기존 연결 클래스들 import
const PostgresConnection = require('../connection/postgres');
const RedisConnection = require('../connection/redis');
const SqliteConnection = require('../connection/sqlite');
const MariaDBConnection = require('../connection/mariadb');

class DatabaseFactory {
    constructor(config = null) {
        this.config = config || this.loadConfig();
        this.connections = new Map();
        this.queryAdapter = new QueryAdapter();
        
        // 연결 타입별 클래스 매핑
        this.connectionClasses = {
            'postgresql': PostgresConnection,
            'postgres': PostgresConnection,
            'sqlite': SqliteConnection,
            'sqlite3': SqliteConnection,
            'mariadb': MariaDBConnection,
            'mysql': MariaDBConnection,
            'redis': RedisConnection
        };
    }

    /**
     * 설정 로드 (환경변수 기반)
     */
    loadConfig() {
        const configManager = ConfigManager.getInstance();
        const dbType = configManager.get('DATABASE_TYPE') || 'sqlite';
        
        const config = {
            database: {
                type: dbType,
                connectionTimeout: parseInt(process.env.DB_CONNECTION_TIMEOUT || '30000'),
                retryAttempts: parseInt(process.env.DB_RETRY_ATTEMPTS || '3'),
                retryDelay: parseInt(process.env.DB_RETRY_DELAY || '1000')
            },
            
            // SQLite 설정
            sqlite: {
                path: process.env.SQLITE_DB_PATH || './data/db/pulseone.db',
                logPath: process.env.SQLITE_LOG_DB_PATH || './data/db/pulseone_logs.db',
                walMode: process.env.SQLITE_WAL_MODE === 'true',
                busyTimeout: parseInt(process.env.SQLITE_BUSY_TIMEOUT || '30000')
            },

            // PostgreSQL 설정
            postgresql: {
                host: process.env.POSTGRES_MAIN_DB_HOST || 'localhost',
                port: parseInt(process.env.POSTGRES_MAIN_DB_PORT || '5432'),
                user: process.env.POSTGRES_MAIN_DB_USER || 'user',
                password: process.env.POSTGRES_MAIN_DB_PASSWORD || 'password',
                database: process.env.POSTGRES_MAIN_DB_NAME || 'pulseone',
                ssl: process.env.POSTGRES_SSL === 'true',
                poolMin: parseInt(process.env.POSTGRES_POOL_MIN || '2'),
                poolMax: parseInt(process.env.POSTGRES_POOL_MAX || '10')
            },

            // SQL Server 설정
            mssql: {
                server: process.env.MSSQL_HOST || 'localhost',
                port: parseInt(process.env.MSSQL_PORT || '1433'),
                user: process.env.MSSQL_USER || 'sa',
                password: process.env.MSSQL_PASSWORD || 'password',
                database: process.env.MSSQL_DATABASE || 'pulseone',
                encrypt: process.env.MSSQL_ENCRYPT === 'true',
                trustServerCertificate: process.env.MSSQL_TRUST_SERVER_CERTIFICATE === 'true'
            },

            // MariaDB 설정
            mariadb: {
                host: process.env.MARIADB_HOST || 'localhost',
                port: parseInt(process.env.MARIADB_PORT || '3306'),
                user: process.env.MARIADB_USER || 'user',
                password: process.env.MARIADB_PASSWORD || 'password',
                database: process.env.MARIADB_DATABASE || 'pulseone',
                connectionLimit: parseInt(process.env.MARIADB_CONNECTION_LIMIT || '10')
            }
        };

        return config;
    }

    /**
     * 메인 데이터베이스 연결 획득
     */
    async getMainConnection() {
        const cacheKey = `main_${this.config.database.type}`;
        
        if (this.connections.has(cacheKey)) {
            const conn = this.connections.get(cacheKey);
            if (await this.isConnectionValid(conn)) {
                return conn;
            } else {
                this.connections.delete(cacheKey);
            }
        }

        const connection = await this.createConnection(this.config.database.type, 'main');
        this.connections.set(cacheKey, connection);
        
        return connection;
    }

    /**
     * 특정 타입의 데이터베이스 연결 생성
     */
    async createConnection(dbType, purpose = 'main') {
        const ConnectionClass = this.connectionClasses[dbType.toLowerCase()];
        
        if (!ConnectionClass) {
            throw new Error(`지원하지 않는 데이터베이스 타입: ${dbType}`);
        }

        let connectionConfig;
        
        switch (dbType.toLowerCase()) {
        case 'postgresql':
        case 'postgres':
            connectionConfig = this.config.postgresql;
            break;
        case 'sqlite':
        case 'sqlite3':
            connectionConfig = this.config.sqlite;
            break;
        case 'mariadb':
        case 'mysql':
            connectionConfig = this.config.mariadb;
            break;
        case 'mssql':
            connectionConfig = this.config.mssql;
            break;
        default:
            throw new Error(`설정되지 않은 데이터베이스 타입: ${dbType}`);
        }

        try {
            let connection;
            if (dbType.toLowerCase() === 'sqlite' || dbType.toLowerCase() === 'sqlite3') {
                connection = ConnectionClass; // 이미 생성된 인스턴스 사용
                // SQLite 설정이 필요하다면 별도 메서드로 적용
                if (connection.updateConfig) {
                    connection.updateConfig(connectionConfig);
                }
            } else {
                connection = new ConnectionClass(connectionConfig); // 다른 DB는 생성자 사용
            }
            await connection.connect();
            
            console.log(`✅ ${dbType} 연결 성공 (${purpose})`);
            return connection;
            
        } catch (error) {
            console.error(`❌ ${dbType} 연결 실패 (${purpose}):`, error.message);
            throw error;
        }
    }

    /**
     * 쿼리 실행 (자동 연결 관리)
     */
    async executeQuery(query, params = []) {
        const connection = await this.getMainConnection();
        
        try {
            // 연결 클래스별 쿼리 실행 방법 통합
            if (connection.query) {
                return await connection.query(query, params);
            } else if (connection.execute) {
                return await connection.execute(query, params);
            } else if (connection.all) {
                // SQLite
                return await connection.all(query, params);
            } else {
                throw new Error('쿼리 실행 메소드를 찾을 수 없습니다');
            }
            
        } catch (error) {
            console.error('쿼리 실행 오류:', error);
            console.error('쿼리:', query);
            console.error('파라미터:', params);
            throw error;
        }
    }

    /**
     * 트랜잭션 실행
     */
    async executeTransaction(queries) {
        const connection = await this.getMainConnection();
        
        try {
            if (connection.beginTransaction) {
                await connection.beginTransaction();
            }

            const results = [];
            for (const { query, params } of queries) {
                const result = await this.executeQuery(query, params);
                results.push(result);
            }

            if (connection.commit) {
                await connection.commit();
            }

            return results;
            
        } catch (error) {
            if (connection.rollback) {
                await connection.rollback();
            }
            throw error;
        }
    }

    /**
     * 쿼리 어댑터 획득 (DB별 문법 변환)
     */
    getQueryAdapter(dbType = null) {
        const targetDbType = dbType || this.config.database.type;
        return this.queryAdapter.getAdapter(targetDbType);
    }

    /**
     * 연결 상태 확인
     */
    async isConnectionValid(connection) {
        try {
            if (!connection) return false;
            
            // 연결별 ping/health check
            if (connection.ping) {
                await connection.ping();
                return true;
            } else if (connection.query) {
                await connection.query('SELECT 1');
                return true;
            } else if (connection.get) {
                // SQLite
                await connection.get('SELECT 1');
                return true;
            }
            
            return false;
            
        } catch (error) {
            return false;
        }
    }

    /**
     * 모든 연결 정리
     */
    async closeAllConnections() {
        const closePromises = [];
        
        for (const [key, connection] of this.connections) {
            try {
                if (connection.close) {
                    closePromises.push(connection.close());
                } else if (connection.end) {
                    closePromises.push(connection.end());
                } else if (connection.destroy) {
                    closePromises.push(connection.destroy());
                }
            } catch (error) {
                console.warn(`연결 종료 중 오류 (${key}):`, error.message);
            }
        }

        await Promise.all(closePromises);
        this.connections.clear();
        
        console.log('✅ 모든 데이터베이스 연결이 정리되었습니다');
    }

    /**
     * 연결 상태 리포트
     */
    getConnectionStatus() {
        const status = {
            totalConnections: this.connections.size,
            databaseType: this.config.database.type,
            connections: []
        };

        for (const [key, connection] of this.connections) {
            status.connections.push({
                key,
                type: typeof connection.constructor.name,
                connected: connection.readyState === 'open' || connection.state === 'connected'
            });
        }

        return status;
    }

    /**
     * 설정 정보 (민감 정보 제외)
     */
    getPublicConfig() {
        const publicConfig = JSON.parse(JSON.stringify(this.config));
        
        // 민감 정보 제거
        const sensitiveKeys = ['password', 'token', 'secret', 'key'];
        
        function removeSensitiveData(obj) {
            for (const key in obj) {
                if (typeof obj[key] === 'object' && obj[key] !== null) {
                    removeSensitiveData(obj[key]);
                } else if (sensitiveKeys.some(sensitive => key.toLowerCase().includes(sensitive))) {
                    obj[key] = '***';
                }
            }
        }
        
        removeSensitiveData(publicConfig);
        return publicConfig;
    }
}

module.exports = DatabaseFactory;