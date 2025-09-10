// =============================================================================
// DatabaseInitializer - 범용 데이터베이스 초기화 (확장 가능한 설계)
// =============================================================================

const fs = require('fs').promises;
const path = require('path');
const config = require('../config/ConfigManager');

class DatabaseInitializer {
    constructor(connections = null) {
        this.config = config.getInstance();
        this.connections = connections;
        
        // DatabaseAbstractionLayer 안전한 로딩
        try {
            const { DatabaseAbstractionLayer } = require('./DatabaseAbstractionLayer');
            this.dbLayer = new DatabaseAbstractionLayer(connections);
        } catch (error) {
            console.warn('⚠️ DatabaseAbstractionLayer 로드 실패, 직접 연결 모드 사용');
            this.dbLayer = null;
        }

        this.initStatus = {
            systemTables: false,
            tenantSchemas: false, 
            sampleData: false,
            indexesCreated: false
        };
        
        const dbConfig = this.config.getDatabaseConfig();
        this.databaseType = dbConfig.type.toLowerCase();
        
        // 스키마 경로들
        this.possibleSchemaPaths = [
            path.join(__dirname, 'schemas'),
            path.join(process.cwd(), 'backend', 'lib', 'database', 'schemas'),
            path.join(process.cwd(), 'schemas')
        ];
        
        this.schemasPath = null;
        
        console.log(`🔧 DatabaseInitializer: ${this.databaseType.toUpperCase()} 모드로 초기화`);
    }

    setConnections(connections) {
        this.connections = connections;
        if (this.dbLayer) {
            this.dbLayer.setConnections(connections);
        }
    }

    /**
     * 범용 SQL 실행 메서드 (데이터베이스 타입 자동 감지)
     */
    async executeSQL(statement, params = []) {
        if (this.dbLayer) {
            return await this.dbLayer.executeNonQuery(statement, params);
        }
        
        // DatabaseAbstractionLayer 없을 때 폴백
        return await this.executeDirectSQL(statement, params);
    }

    /**
     * 범용 쿼리 메서드 (데이터베이스 타입 자동 감지)  
     */
    async querySQL(query, params = []) {
        if (this.dbLayer) {
            return await this.dbLayer.executeQuery(query, params);
        }
        
        // DatabaseAbstractionLayer 없을 때 폴백
        return await this.queryDirectSQL(query, params);
    }

    /**
     * 직접 SQL 실행 (타입별 분기)
     */
    async executeDirectSQL(statement, params = []) {
        switch (this.databaseType) {
            case 'sqlite':
                return await this.executeSQLiteSQL(statement, params);
            case 'postgresql':
                return await this.executePostgresSQL(statement, params);
            case 'mysql':
                return await this.executeMySQLSQL(statement, params);
            default:
                throw new Error(`지원하지 않는 데이터베이스 타입: ${this.databaseType}`);
        }
    }

    /**
     * 직접 쿼리 실행 (타입별 분기)
     */
    async queryDirectSQL(query, params = []) {
        switch (this.databaseType) {
            case 'sqlite':
                return await this.querySQLiteSQL(query, params);
            case 'postgresql':
                return await this.queryPostgresSQL(query, params);
            case 'mysql':
                return await this.queryMySQLSQL(query, params);
            default:
                throw new Error(`지원하지 않는 데이터베이스 타입: ${this.databaseType}`);
        }
    }

    /**
     * SQLite 전용 실행
     */
    async executeSQLiteSQL(statement, params = []) {
        const sqlite3 = require('sqlite3').verbose();
        const dbConfig = this.config.getDatabaseConfig();
        const dbPath = dbConfig.sqlite.path;
        
        return new Promise((resolve, reject) => {
            const db = new sqlite3.Database(dbPath, (err) => {
                if (err) {
                    reject(err);
                    return;
                }
                
                if (params.length > 0) {
                    db.run(statement, params, (execErr) => {
                        db.close();
                        if (execErr) reject(execErr);
                        else resolve();
                    });
                } else {
                    db.exec(statement, (execErr) => {
                        db.close();
                        if (execErr) reject(execErr);
                        else resolve();
                    });
                }
            });
        });
    }

    /**
     * SQLite 전용 쿼리
     */
    async querySQLiteSQL(query, params = []) {
        const sqlite3 = require('sqlite3').verbose();
        const dbConfig = this.config.getDatabaseConfig();
        const dbPath = dbConfig.sqlite.path;
        
        return new Promise((resolve, reject) => {
            const db = new sqlite3.Database(dbPath, (err) => {
                if (err) {
                    reject(err);
                    return;
                }
                
                db.all(query, params, (execErr, rows) => {
                    db.close();
                    if (execErr) reject(execErr);
                    else resolve(rows);
                });
            });
        });
    }

    /**
     * PostgreSQL 전용 실행 (연결 객체 사용)
     */
    async executePostgresSQL(statement, params = []) {
        if (!this.connections?.postgres) {
            throw new Error('PostgreSQL 연결이 없습니다');
        }
        
        const result = await this.connections.postgres.query(statement, params);
        return result.rowCount;
    }

    /**
     * PostgreSQL 전용 쿼리 (연결 객체 사용)
     */
    async queryPostgresSQL(query, params = []) {
        if (!this.connections?.postgres) {
            throw new Error('PostgreSQL 연결이 없습니다');
        }
        
        const result = await this.connections.postgres.query(query, params);
        return result.rows;
    }

    /**
     * MySQL 전용 실행 (연결 객체 사용)
     */
    async executeMySQLSQL(statement, params = []) {
        if (!this.connections?.mysql) {
            throw new Error('MySQL 연결이 없습니다');
        }
        
        const [result] = await this.connections.mysql.execute(statement, params);
        return result.affectedRows;
    }

    /**
     * MySQL 전용 쿼리 (연결 객체 사용)
     */
    async queryMySQLSQL(query, params = []) {
        if (!this.connections?.mysql) {
            throw new Error('MySQL 연결이 없습니다');
        }
        
        const [rows] = await this.connections.mysql.execute(query, params);
        return rows;
    }

    /**
     * 스키마 경로 탐지
     */
    async findSchemasPath() {
        if (this.schemasPath) return this.schemasPath;

        for (const possiblePath of this.possibleSchemaPaths) {
            try {
                await fs.access(possiblePath);
                const files = await fs.readdir(possiblePath);
                const sqlFiles = files.filter(file => file.endsWith('.sql'));
                
                if (sqlFiles.length > 0) {
                    console.log(`✅ 스키마 경로 발견: ${possiblePath} (${sqlFiles.length}개 SQL 파일)`);
                    this.schemasPath = possiblePath;
                    return possiblePath;
                }
            } catch (error) {
                continue;
            }
        }

        console.log('⚠️ 스키마 폴더를 찾을 수 없어 메모리 스키마를 사용합니다.');
        return null;
    }

    /**
     * SQL 파일 실행
     */
    async executeSQLFile(filename) {
        try {
            const schemasPath = await this.findSchemasPath();
            let sqlContent;
            
            if (schemasPath) {
                const filePath = path.join(schemasPath, filename);
                try {
                    sqlContent = await fs.readFile(filePath, 'utf8');
                } catch (error) {
                    console.log(`  ⚠️ ${filename} 파일 없음, 기본 스키마 사용`);
                    sqlContent = this.getDefaultSchema(filename);
                }
            } else {
                sqlContent = this.getDefaultSchema(filename);
            }

            if (!sqlContent) {
                console.log(`  ⚠️ ${filename} 스키마를 찾을 수 없음, 스킵`);
                return true;
            }

            const statements = this.parseSQLStatements(sqlContent);
            console.log(`  📁 ${filename}: ${statements.length}개 SQL 명령 실행 중...`);
            
            let successCount = 0;
            for (const statement of statements) {
                try {
                    await this.executeSQL(statement);
                    successCount++;
                } catch (error) {
                    console.log(`    ⚠️ SQL 실행 실패: ${error.message}`);
                }
            }
            
            console.log(`  ✅ ${filename} 실행 완료 (${successCount}/${statements.length})`);
            return successCount > 0;
            
        } catch (error) {
            console.error(`❌ SQL 파일 실행 실패 (${filename}):`, error.message);
            return false;
        }
    }

    /**
     * 기본 스키마 반환 (메모리에서)
     */
    getDefaultSchema(filename) {
        const schemaMap = {
            '01-core-tables.sql': this.getCoreTables(),
            '02-users-sites.sql': this.getUsersSites(),
            '03-device-tables.sql': this.getDevicesDatapoints(),
            '04-virtual-points.sql': this.getVirtualPoints(),
            '05-alarm-tables.sql': this.getAlarmRules(),
            '06-log-tables.sql': this.getLogTables(),
            '07-indexes.sql': this.getIndexes(),
            '08-protocols-table.sql': this.getProtocolsTable(),
            'initial-data.sql': this.getInitialData()
        };
        
        return schemaMap[filename] || null;
    }

    /**
     * SQL 파싱
     */
    parseSQLStatements(sqlContent) {
        return sqlContent
            .split(';')
            .map(stmt => stmt.trim())
            .filter(stmt => {
                return stmt.length > 0 && 
                       !stmt.startsWith('--') && 
                       !stmt.startsWith('/*');
            })
            .map(stmt => {
                return stmt.split('\n')
                    .map(line => {
                        const commentIndex = line.indexOf('--');
                        return commentIndex !== -1 ? line.substring(0, commentIndex).trim() : line.trim();
                    })
                    .filter(line => line.length > 0)
                    .join(' ');
            })
            .filter(stmt => stmt.length > 0);
    }

    /**
     * 테이블 존재 확인 (데이터베이스 타입별)
     */
    async doesTableExist(tableName) {
        try {
            switch (this.databaseType) {
                case 'sqlite':
                    const sqliteResult = await this.querySQL(
                        "SELECT name FROM sqlite_master WHERE type='table' AND name = ?", 
                        [tableName]
                    );
                    return sqliteResult.length > 0;
                    
                case 'postgresql':
                    const pgResult = await this.querySQL(
                        "SELECT table_name FROM information_schema.tables WHERE table_schema = 'public' AND table_name = $1", 
                        [tableName]
                    );
                    return pgResult.length > 0;
                    
                case 'mysql':
                    const mysqlResult = await this.querySQL(
                        "SELECT table_name FROM information_schema.tables WHERE table_schema = DATABASE() AND table_name = ?", 
                        [tableName]
                    );
                    return mysqlResult.length > 0;
                    
                default:
                    return false;
            }
        } catch (error) {
            return false;
        }
    }

    /**
     * 완전 자동 초기화
     */
    async performAutoInitialization() {
        try {
            console.log('🚀 완전 자동 초기화 시작...\n');

            await this.findSchemasPath();
            await this.checkDatabaseStatus();

            const skipIfInitialized = this.config.getBoolean('SKIP_IF_INITIALIZED', true);
            if (this.isFullyInitialized() && skipIfInitialized) {
                console.log('✅ 데이터베이스가 이미 완전히 초기화되어 있습니다.');
                return true;
            }

            // 단계별 초기화
            if (!this.initStatus.systemTables) {
                console.log('📋 [1/5] 시스템 테이블 생성 중...');
                await this.createSystemTables();
                this.initStatus.systemTables = true;
            }
            
            if (!this.initStatus.tenantSchemas) {
                console.log('🏢 [2/5] 확장 테이블 생성 중...');
                await this.createExtendedTables();
                this.initStatus.tenantSchemas = true;
            }
            
            if (!this.initStatus.indexesCreated) {
                console.log('⚡ [3/5] 인덱스 생성 중...');
                await this.createIndexes();
                this.initStatus.indexesCreated = true;
            }
            
            if (!this.initStatus.sampleData) {
                console.log('📊 [4/5] 기본 데이터 생성 중...');
                await this.createSampleData();
                this.initStatus.sampleData = true;
            }

            console.log('🎯 [5/5] 초기 데이터 삽입 중...');
            await this.executeSQLFile('initial-data.sql');
            
            // 최종 상태 확인
            await this.checkDatabaseStatus();
            
            console.log('🎉 완전 자동 초기화가 성공적으로 완료되었습니다!');
            return true;
            
        } catch (error) {
            console.error('❌ 완전 자동 초기화 실패:', error.message);
            return false;
        }
    }

    async autoInitializeIfNeeded() {
        const autoInit = this.config.getBoolean('AUTO_INITIALIZE_ON_START', false);
        
        if (!autoInit) {
            console.log('🔧 데이터베이스 자동 초기화가 비활성화되어 있습니다.');
            return false;
        }

        return await this.performAutoInitialization();
    }

    async checkDatabaseStatus() {
        try {
            this.initStatus.systemTables = await this.checkSystemTables();
            this.initStatus.tenantSchemas = await this.checkExtendedTables();
            this.initStatus.sampleData = await this.checkSampleData();
            this.initStatus.indexesCreated = await this.checkIndexes();
            
            console.log('📊 초기화 상태:', this.initStatus);
        } catch (error) {
            console.error('❌ 데이터베이스 상태 확인 실패:', error.message);
        }
    }

    async createSystemTables() {
        const sqlFiles = ['01-core-tables.sql', '02-users-sites.sql', '03-device-tables.sql'];
        for (const sqlFile of sqlFiles) {
            await this.executeSQLFile(sqlFile);
        }
    }

    async createExtendedTables() {
        const sqlFiles = ['04-virtual-points.sql', '05-alarm-tables.sql', '06-log-tables.sql', '08-protocols-table.sql'];
        for (const sqlFile of sqlFiles) {
            await this.executeSQLFile(sqlFile);
        }
    }

    async createIndexes() {
        await this.executeSQLFile('07-indexes.sql');
    }

    async checkSystemTables() {
        const systemTables = ['tenants', 'users', 'sites', 'devices', 'datapoints'];
        let foundTables = 0;
        
        for (const tableName of systemTables) {
            if (await this.doesTableExist(tableName)) {
                foundTables++;
            }
        }
        
        console.log(`📋 시스템 테이블: ${foundTables}/${systemTables.length} 발견`);
        return foundTables >= systemTables.length;
    }

    async checkExtendedTables() {
        const extendedTables = ['virtual_points', 'alarm_rules', 'device_logs', 'protocols'];
        let foundTables = 0;
        
        for (const tableName of extendedTables) {
            if (await this.doesTableExist(tableName)) {
                foundTables++;
            }
        }
        
        console.log(`🏢 확장 테이블: ${foundTables}/${extendedTables.length} 발견`);
        return foundTables >= extendedTables.length;
    }

    async checkSampleData() {
        try {
            const result = await this.querySQL('SELECT COUNT(*) as count FROM tenants');
            const count = parseInt(result[0]?.count || '0');
            
            console.log(`📊 기본 데이터: ${count}개 테넌트 발견`);
            return count > 0;
        } catch (error) {
            console.log('📊 기본 데이터: 확인 실패, 생성 필요');
            return false;
        }
    }

    async checkIndexes() {
        try {
            // 데이터베이스별 인덱스 확인 쿼리
            let indexQuery;
            let indexName;
            
            switch (this.databaseType) {
                case 'sqlite':
                    indexQuery = "SELECT name FROM sqlite_master WHERE type='index' AND name = ?";
                    indexName = 'idx_users_tenant';
                    break;
                case 'postgresql':
                    indexQuery = "SELECT indexname FROM pg_indexes WHERE indexname = $1";
                    indexName = 'idx_users_tenant';
                    break;
                case 'mysql':
                    indexQuery = "SELECT index_name FROM information_schema.statistics WHERE index_name = ?";
                    indexName = 'idx_users_tenant';
                    break;
                default:
                    return false;
            }
            
            const result = await this.querySQL(indexQuery, [indexName]);
            const foundIndexes = result.length > 0;
            
            console.log(`⚡ 인덱스: ${foundIndexes ? '발견됨' : '생성 필요'}`);
            return foundIndexes;
        } catch (error) {
            console.log('⚡ 인덱스: 확인 실패, 생성 필요');
            return false;
        }
    }

    async createSampleData() {
        try {
            // 기본 테넌트 생성
            await this.executeSQL(
                `INSERT OR IGNORE INTO tenants (id, name, display_name, description, is_active) 
                 VALUES (1, 'default', 'Default Organization', 'Default tenant', 1)`
            );
            
            // 기본 사용자 생성  
            await this.executeSQL(
                `INSERT OR IGNORE INTO users (tenant_id, username, email, role, is_active) 
                 VALUES (1, 'admin', 'admin@pulseone.local', 'admin', 1)`
            );
            
            console.log('  ✅ 기본 테넌트 및 사용자 생성 완료');
        } catch (error) {
            console.error('기본 데이터 생성 실패:', error.message);
        }
    }

    async createBackup(force = false) {
        try {
            if (this.databaseType === 'sqlite') {
                const dbConfig = this.config.getDatabaseConfig();
                const backupDir = path.join(process.cwd(), 'data', 'backup');
                await fs.mkdir(backupDir, { recursive: true });
                
                const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
                const backupPath = path.join(backupDir, `pulseone_backup_${timestamp}.db`);
                
                await fs.copyFile(dbConfig.sqlite.path, backupPath);
                console.log(`✅ SQLite 백업 생성: ${backupPath}`);
                return backupPath;
            }
            
            return null;
        } catch (error) {
            console.error('❌ 백업 생성 실패:', error.message);
            if (force) throw error;
            return null;
        }
    }

    async performInitialization() {
        return this.performAutoInitialization();
    }

    isFullyInitialized() {
        return this.initStatus.systemTables && 
               this.initStatus.tenantSchemas && 
               this.initStatus.sampleData && 
               this.initStatus.indexesCreated;
    }

    // 스키마 템플릿들 (기존과 동일)
    getCoreTables() {
        return `-- 01-core-tables.sql
CREATE TABLE IF NOT EXISTS tenants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) UNIQUE NOT NULL,
    display_name VARCHAR(255),
    description TEXT,
    is_active BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);`;
    }

    getUsersSites() {
        return `-- 02-users-sites.sql
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    role VARCHAR(50) DEFAULT 'user',
    is_active BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id)
);

CREATE TABLE IF NOT EXISTS sites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    location VARCHAR(255),
    timezone VARCHAR(50) DEFAULT 'UTC',
    is_active BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id)
);`;
    }

    getDevicesDatapoints() {
        return `-- 03-devices-datapoints.sql
CREATE TABLE IF NOT EXISTS devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    device_type VARCHAR(50) NOT NULL,
    protocol_id INTEGER,
    endpoint VARCHAR(255),
    is_enabled BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id),
    FOREIGN KEY (site_id) REFERENCES sites(id)
);

CREATE TABLE IF NOT EXISTS datapoints (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    device_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    data_type VARCHAR(50) DEFAULT 'FLOAT',
    unit VARCHAR(20),
    address VARCHAR(100),
    scale_factor FLOAT DEFAULT 1.0,
    offset FLOAT DEFAULT 0.0,
    is_enabled BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id),
    FOREIGN KEY (device_id) REFERENCES devices(id)
);`;
    }

    getVirtualPoints() {
        return `-- 04-virtual-points.sql
CREATE TABLE IF NOT EXISTS virtual_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    expression TEXT NOT NULL,
    data_type VARCHAR(50) DEFAULT 'FLOAT',
    unit VARCHAR(20),
    update_interval INTEGER DEFAULT 60,
    is_enabled BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id)
);`;
    }

    getAlarmRules() {
        return `-- 05-alarm-rules.sql
CREATE TABLE IF NOT EXISTS alarm_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    condition_expression TEXT NOT NULL,
    severity VARCHAR(20) DEFAULT 'WARNING',
    is_enabled BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id)
);`;
    }

    getLogTables() {
        return `-- 06-log-tables.sql
CREATE TABLE IF NOT EXISTS device_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    device_id INTEGER,
    log_level VARCHAR(20) NOT NULL,
    category VARCHAR(50),
    message TEXT NOT NULL,
    details TEXT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    source VARCHAR(100),
    FOREIGN KEY (tenant_id) REFERENCES tenants(id),
    FOREIGN KEY (device_id) REFERENCES devices(id)
);

CREATE TABLE IF NOT EXISTS system_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    log_level VARCHAR(20) NOT NULL,
    category VARCHAR(50),
    component VARCHAR(100),
    message TEXT NOT NULL,
    details TEXT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    user_id INTEGER,
    session_id VARCHAR(100),
    FOREIGN KEY (tenant_id) REFERENCES tenants(id),
    FOREIGN KEY (user_id) REFERENCES users(id)
);`;
    }

    getProtocolsTable() {
        return `-- 08-protocols-table.sql
CREATE TABLE IF NOT EXISTS protocols (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    protocol_type VARCHAR(50) NOT NULL UNIQUE,
    display_name VARCHAR(100) NOT NULL,
    description TEXT,
    default_port INTEGER,
    category VARCHAR(50),
    is_enabled BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);`;
    }

    getIndexes() {
        return `-- 07-indexes.sql
CREATE INDEX IF NOT EXISTS idx_users_tenant ON users(tenant_id);
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
CREATE INDEX IF NOT EXISTS idx_devices_tenant ON devices(tenant_id);
CREATE INDEX IF NOT EXISTS idx_datapoints_device ON datapoints(device_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_tenant ON virtual_points(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_tenant ON alarm_rules(tenant_id);`;
    }

    getInitialData() {
        return `-- initial-data.sql
INSERT OR IGNORE INTO protocols (protocol_type, display_name, description, default_port, category) VALUES 
('MODBUS_TCP', 'Modbus TCP', 'Modbus TCP/IP 프로토콜', 502, 'industrial'),
('MQTT', 'MQTT', 'MQTT 메시징 프로토콜', 1883, 'iot'),
('HTTP_REST', 'HTTP REST', 'RESTful HTTP API', 80, 'web');`;
    }
}

module.exports = DatabaseInitializer;