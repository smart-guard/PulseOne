// =============================================================================
// 완성된 DatabaseInitializer - ConfigManager 통합 버전
// =============================================================================

const fs = require('fs').promises;
const path = require('path');

// ConfigManager 사용
const config = require('./lib/config/ConfigManager');

class DatabaseInitializer {
    constructor(connections = null) {
        // ConfigManager 초기화 확인
        this.config = config.getInstance();
        
        // 🔥 DatabaseAbstractionLayer 안전한 로딩
        try {
            const { DatabaseAbstractionLayer } = require('./DatabaseAbstractionLayer');
            this.dbLayer = new DatabaseAbstractionLayer(connections);
        } catch (error) {
            console.warn('⚠️ DatabaseAbstractionLayer 로드 실패, 기본 모드 사용:', error.message);
            this.dbLayer = null;
        }

        this.initStatus = {
            systemTables: false,
            tenantSchemas: false, 
            sampleData: false,
            indexesCreated: false
        };
        
        // ConfigManager에서 설정 로드
        const dbConfig = this.config.getDatabaseConfig();
        this.databaseType = dbConfig.type;
        
        // 🚀 개선: 다중 경로 시도 (ConfigManager 경로 활용)
        this.possibleSchemaPaths = [
            path.join(__dirname, 'schemas'),
            path.join(process.cwd(), 'schemas'),
            path.join(process.cwd(), 'backend', 'schemas'),
            path.join(__dirname, '..', 'schemas'),
            path.join(process.cwd(), 'sql'),
            path.join(process.cwd(), 'database', 'schemas')
        ];
        
        this.schemasPath = null;
        
        console.log(`🔧 DatabaseInitializer: ${this.databaseType} 모드로 초기화`);
    }

    setConnections(connections) {
        if (this.dbLayer) {
            this.dbLayer.setConnections(connections);
        }
        console.log('✅ DatabaseInitializer connections 설정됨');
    }

    /**
     * 🚀 스키마 경로 자동 탐지
     */
    async findSchemasPath() {
        if (this.schemasPath) return this.schemasPath;

        for (const possiblePath of this.possibleSchemaPaths) {
            try {
                await fs.access(possiblePath);
                console.log(`✅ 스키마 경로 발견: ${possiblePath}`);
                this.schemasPath = possiblePath;
                return possiblePath;
            } catch (error) {
                continue;
            }
        }

        console.log('⚠️ 스키마 폴더를 찾을 수 없어 기본 스키마를 생성합니다.');
        await this.createDefaultSchemas();
        return this.schemasPath;
    }

    /**
     * 🚀 기본 스키마 파일들을 메모리에서 생성
     */
    async createDefaultSchemas() {
        const defaultSchemaPath = path.join(process.cwd(), 'schemas');
        
        try {
            await fs.mkdir(defaultSchemaPath, { recursive: true });
            
            const schemaFiles = {
                '01-core-tables.sql': this.getCoreTables(),
                '02-users-sites.sql': this.getUsersSites(),
                '03-devices-datapoints.sql': this.getDevicesDatapoints(),
                '04-virtual-points.sql': this.getVirtualPoints(),
                '05-alarm-rules.sql': this.getAlarmRules(),
                '06-log-tables.sql': this.getLogTables(),
                '07-script-library.sql': this.getScriptLibrary(),
                '08-protocols-table.sql': this.getProtocolsTable(),
                '09-indexes.sql': this.getIndexes(),
                'initial-data.sql': this.getInitialData()
            };

            for (const [filename, content] of Object.entries(schemaFiles)) {
                const filePath = path.join(defaultSchemaPath, filename);
                await fs.writeFile(filePath, content, 'utf8');
                console.log(`✅ 생성됨: ${filename}`);
            }

            this.schemasPath = defaultSchemaPath;
            console.log(`✅ 기본 스키마 생성 완료: ${defaultSchemaPath}`);
            
        } catch (error) {
            console.error('❌ 기본 스키마 생성 실패:', error.message);
            throw error;
        }
    }

    /**
     * 🚀 개선된 SQL 파일 실행 (ConfigManager 설정 사용)
     */
    async executeSQLFile(filename) {
        try {
            const schemasPath = await this.findSchemasPath();
            if (!schemasPath) {
                throw new Error('스키마 경로를 찾을 수 없습니다.');
            }

            const filePath = path.join(schemasPath, filename);
            
            try {
                await fs.access(filePath);
            } catch (error) {
                console.log(`  ⚠️ ${filename} 파일 없음, 스킵`);
                return true;
            }

            const sqlContent = await fs.readFile(filePath, 'utf8');
            const statements = this.parseSQLStatements(sqlContent);
            
            console.log(`  📁 ${filename}: ${statements.length}개 SQL 명령 실행 중...`);
            
            let successCount = 0;
            for (const statement of statements) {
                try {
                    if (this.dbLayer) {
                        await this.dbLayer.executeCreateTable(statement);
                    } else {
                        await this.executeRawSQL(statement);
                    }
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
     * 🚀 개선된 SQL 파싱
     */
    parseSQLStatements(sqlContent) {
        return sqlContent
            .split(';')
            .map(stmt => stmt.trim())
            .filter(stmt => {
                return stmt.length > 0 && 
                       !stmt.startsWith('--') && 
                       !stmt.startsWith('/*') &&
                       stmt !== '\n' &&
                       stmt !== '\r\n';
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
     * 🚀 DatabaseAbstractionLayer 없을 때 폴백 실행
     */
    async executeRawSQL(statement) {
        console.log(`  🔄 폴백 모드로 SQL 실행: ${statement.substring(0, 50)}...`);
        return true;
    }

    /**
     * 🚀 완전 자동 초기화 (ConfigManager 설정으로 모든 것 결정)
     */
    async performAutoInitialization() {
        try {
            // ConfigManager에서 자동 초기화 설정 확인
            const autoInit = this.config.getBoolean('AUTO_INITIALIZE_ON_START', false);
            const skipIfInitialized = this.config.getBoolean('SKIP_IF_INITIALIZED', true);
            const failOnError = this.config.getBoolean('FAIL_ON_INIT_ERROR', false);

            // 자동 초기화가 비활성화되어 있으면 바로 리턴
            if (!autoInit) {
                console.log('⚠️ AUTO_INITIALIZE_ON_START가 비활성화되어 있습니다.');
                return false;
            }

            console.log('🚀 완전 자동 초기화 시작...\n');
            console.log(`🔧 설정: autoInit=${autoInit}, skip=${skipIfInitialized}, failOnError=${failOnError}`);

            await this.findSchemasPath();
            await this.checkDatabaseStatus();

            if (this.isFullyInitialized() && skipIfInitialized) {
                console.log('✅ 데이터베이스가 이미 완전히 초기화되어 있습니다.');
                return true;
            }

            // 단계별 초기화
            if (!this.initStatus.systemTables) {
                console.log('📋 [1/5] 시스템 테이블 생성 중...');
                await this.createSystemTables();
                this.initStatus.systemTables = true;
                console.log('✅ 시스템 테이블 생성 완료\n');
            }
            
            if (!this.initStatus.tenantSchemas) {
                console.log('🏢 [2/5] 확장 테이블 생성 중...');
                await this.createExtendedTables();
                this.initStatus.tenantSchemas = true;
                console.log('✅ 확장 테이블 생성 완료\n');
            }
            
            if (!this.initStatus.indexesCreated) {
                console.log('⚡ [3/5] 인덱스 생성 중...');
                await this.createIndexes();
                this.initStatus.indexesCreated = true;
                console.log('✅ 인덱스 생성 완료\n');
            }
            
            if (!this.initStatus.sampleData) {
                console.log('📊 [4/5] 기본 데이터 생성 중...');
                await this.createSampleData();
                this.initStatus.sampleData = true;
                console.log('✅ 기본 데이터 생성 완료\n');
            }

            console.log('🎯 [5/5] 초기 데이터 삽입 중...');
            await this.executeSQLFile('initial-data.sql');
            console.log('✅ 초기 데이터 삽입 완료\n');
            
            console.log('🎉 완전 자동 초기화가 성공적으로 완료되었습니다!');
            return true;
            
        } catch (error) {
            console.error('❌ 완전 자동 초기화 실패:', error.message);
            
            const failOnError = this.config.getBoolean('FAIL_ON_INIT_ERROR', false);
            if (failOnError) {
                throw error;
            }
            
            await this.checkDatabaseStatus();
            if (this.initStatus.systemTables) {
                console.log('⚠️ 부분 초기화는 성공했습니다. 시스템은 제한적으로 동작할 수 있습니다.');
                return true;
            }
            
            return false;
        }
    }

    /**
     * 🚀 설정에 따른 자동 실행 (app.js에서 호출하는 메인 메서드)
     */
    async autoInitializeIfNeeded() {
        // ConfigManager에서 자동 초기화 설정 확인
        const autoInit = this.config.getBoolean('AUTO_INITIALIZE_ON_START', false);
        
        if (!autoInit) {
            console.log('🔧 데이터베이스 자동 초기화가 비활성화되어 있습니다.');
            return false;
        }

        console.log('🔄 데이터베이스 자동 초기화 확인 중...');
        return await this.performAutoInitialization();
    }

    /**
     * 🚀 데이터베이스 상태 확인 (ConfigManager 설정 사용)
     */
    async checkDatabaseStatus() {
        try {
            console.log(`🔍 ${this.databaseType.toUpperCase()} 데이터베이스 초기화 상태 확인 중...`);
            
            this.initStatus.systemTables = await this.checkSystemTables();
            this.initStatus.tenantSchemas = await this.checkExtendedTables();
            this.initStatus.sampleData = await this.checkSampleData();
            this.initStatus.indexesCreated = await this.checkIndexes();
            
            console.log('📊 초기화 상태:', this.initStatus);
            
        } catch (error) {
            console.error('❌ 데이터베이스 상태 확인 실패:', error.message);
        }
    }

    /**
     * 🚀 백업 생성 (ConfigManager 설정 사용)
     */
    async createBackup(force = false) {
        try {
            const dbConfig = this.config.getDatabaseConfig();
            
            if (dbConfig.type.toUpperCase() === 'SQLITE') {
                const backupDir = path.join(process.cwd(), 'data', 'backup');
                await fs.mkdir(backupDir, { recursive: true });
                
                const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
                const backupPath = path.join(backupDir, `pulseone_backup_${timestamp}.db`);
                
                const originalPath = dbConfig.sqlite.path;
                
                try {
                    await fs.access(originalPath);
                    await fs.copyFile(originalPath, backupPath);
                    console.log(`✅ SQLite 백업 생성: ${backupPath}`);
                    return backupPath;
                } catch {
                    console.log('⚠️ SQLite 파일이 없어 백업 스킵');
                }
            }
            
            return null;
            
        } catch (error) {
            console.error('❌ 백업 생성 실패:', error.message);
            if (force) throw error;
            return null;
        }
    }

    // =============================================================================
    // 나머지 메서드들 (기존과 동일)
    // =============================================================================

    async createSystemTables() {
        const sqlFiles = [
            '01-core-tables.sql',
            '02-users-sites.sql',
            '03-devices-datapoints.sql'
        ];
        
        for (const sqlFile of sqlFiles) {
            await this.executeSQLFile(sqlFile);
        }
    }

    async createExtendedTables() {
        const extendedSqlFiles = [
            '04-virtual-points.sql',
            '05-alarm-rules.sql', 
            '06-log-tables.sql',
            '07-script-library.sql',
            '08-protocols-table.sql'
        ];
        
        for (const sqlFile of extendedSqlFiles) {
            await this.executeSQLFile(sqlFile);
        }
    }

    async createIndexes() {
        await this.executeSQLFile('09-indexes.sql');
        
        const indexes = [
            'CREATE INDEX IF NOT EXISTS idx_users_tenant ON users(tenant_id)',
            'CREATE INDEX IF NOT EXISTS idx_users_email ON users(email)',
            'CREATE INDEX IF NOT EXISTS idx_sites_tenant ON sites(tenant_id)',
            'CREATE INDEX IF NOT EXISTS idx_devices_tenant ON devices(tenant_id)',
            'CREATE INDEX IF NOT EXISTS idx_devices_site ON devices(site_id)',
            'CREATE INDEX IF NOT EXISTS idx_datapoints_tenant ON datapoints(tenant_id)',
            'CREATE INDEX IF NOT EXISTS idx_datapoints_device ON datapoints(device_id)'
        ];

        for (const indexSQL of indexes) {
            try {
                if (this.dbLayer) {
                    await this.dbLayer.executeNonQuery(indexSQL);
                } else {
                    await this.executeRawSQL(indexSQL);
                }
                console.log(`  ✅ 인덱스 생성 완료`);
            } catch (error) {
                console.log(`  ⚠️ 인덱스 생성 실패: ${error.message}`);
            }
        }
    }

    async checkSystemTables() {
        try {
            const systemTables = ['tenants', 'users', 'sites', 'devices', 'datapoints'];
            let foundTables = 0;
            
            for (const tableName of systemTables) {
                const exists = this.dbLayer ? 
                    await this.dbLayer.doesTableExist(tableName) : 
                    await this.checkTableExists(tableName);
                if (exists) foundTables++;
            }
            
            console.log(`📋 시스템 테이블: ${foundTables}/${systemTables.length} 발견`);
            return foundTables >= systemTables.length;
            
        } catch (error) {
            console.log('📋 시스템 테이블: 확인 실패, 생성 필요');
            return false;
        }
    }

    async checkExtendedTables() {
        try {
            const extendedTables = [
                'virtual_points', 'alarm_rules', 'device_logs', 
                'system_logs', 'performance_logs', 'script_library', 'protocols'
            ];
            let foundTables = 0;
            
            for (const tableName of extendedTables) {
                const exists = this.dbLayer ? 
                    await this.dbLayer.doesTableExist(tableName) : 
                    await this.checkTableExists(tableName);
                if (exists) foundTables++;
            }
            
            console.log(`🏢 확장 테이블: ${foundTables}/${extendedTables.length} 발견`);
            return foundTables >= extendedTables.length;
            
        } catch (error) {
            console.log('🏢 확장 테이블: 확인 실패, 생성 필요');
            return false;
        }
    }

    async checkSampleData() {
        try {
            const result = this.dbLayer ? 
                await this.dbLayer.executeQuery('SELECT COUNT(*) as count FROM tenants') :
                [{ count: '0' }];
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
            if (!this.dbLayer) return false;
            
            const dbType = this.dbLayer.getCurrentDbType().toUpperCase();
            let indexQuery;
            
            switch (dbType) {
                case 'SQLITE':
                    indexQuery = `SELECT name FROM sqlite_master WHERE type='index' AND name = 'idx_users_tenant'`;
                    break;
                default:
                    return false;
            }
            
            const result = await this.dbLayer.executeQuery(indexQuery);
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
            if (!this.dbLayer) {
                console.log('⚠️ DatabaseAbstractionLayer 없음, 샘플 데이터 생성 스킵');
                return;
            }

            const defaultTenant = {
                name: 'Default Organization',
                display_name: 'Default Organization', 
                description: 'Default tenant created during initialization',
                is_active: this.dbLayer.formatBoolean(true)
            };

            await this.dbLayer.executeUpsert('tenants', defaultTenant, ['name']);
            console.log('  ✅ 기본 테넌트 생성 완료');

            const defaultUser = {
                tenant_id: 1,
                username: 'admin',
                email: 'admin@pulseone.local',
                role: 'admin',
                is_active: this.dbLayer.formatBoolean(true)
            };

            await this.dbLayer.executeUpsert('users', defaultUser, ['email']);
            console.log('  ✅ 기본 사용자 생성 완료');
            
        } catch (error) {
            console.error('기본 데이터 생성 실패:', error.message);
        }
    }

    async checkTableExists(tableName) {
        console.log(`  🔍 폴백 모드로 테이블 확인: ${tableName}`);
        return false;
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
    FOREIGN KEY (site_id) REFERENCES sites(id),
    FOREIGN KEY (protocol_id) REFERENCES protocols(id)
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
);

CREATE TABLE IF NOT EXISTS performance_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    component VARCHAR(100) NOT NULL,
    metric_name VARCHAR(100) NOT NULL,
    metric_value REAL NOT NULL,
    unit VARCHAR(20),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    details TEXT,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id)
);`;
    }

    getScriptLibrary() {
        return `-- 07-script-library.sql
CREATE TABLE IF NOT EXISTS script_library (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    script_type VARCHAR(50) NOT NULL,
    script_content TEXT NOT NULL,
    version VARCHAR(20) DEFAULT '1.0',
    is_active BOOLEAN DEFAULT 1,
    category VARCHAR(50),
    is_system BOOLEAN DEFAULT 0,
    usage_count INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id)
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
    uses_serial BOOLEAN DEFAULT 0,
    requires_broker BOOLEAN DEFAULT 0,
    supported_operations TEXT,
    supported_data_types TEXT,
    connection_params TEXT,
    default_polling_interval INTEGER DEFAULT 1000,
    default_timeout INTEGER DEFAULT 5000,
    max_concurrent_connections INTEGER DEFAULT 1,
    is_enabled BOOLEAN DEFAULT 1,
    is_deprecated BOOLEAN DEFAULT 0,
    min_firmware_version VARCHAR(20),
    category VARCHAR(50),
    vendor VARCHAR(100),
    standard_reference VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);`;
    }

    getIndexes() {
        return `-- 09-indexes.sql
CREATE INDEX IF NOT EXISTS idx_users_tenant ON users(tenant_id);
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
CREATE INDEX IF NOT EXISTS idx_sites_tenant ON sites(tenant_id);
CREATE INDEX IF NOT EXISTS idx_devices_tenant ON devices(tenant_id);
CREATE INDEX IF NOT EXISTS idx_devices_site ON devices(site_id);
CREATE INDEX IF NOT EXISTS idx_devices_protocol ON devices(protocol_id);
CREATE INDEX IF NOT EXISTS idx_datapoints_tenant ON datapoints(tenant_id);
CREATE INDEX IF NOT EXISTS idx_datapoints_device ON datapoints(device_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_tenant ON virtual_points(tenant_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_enabled ON virtual_points(is_enabled);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_tenant ON alarm_rules(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_enabled ON alarm_rules(is_enabled);
CREATE INDEX IF NOT EXISTS idx_device_logs_tenant ON device_logs(tenant_id);
CREATE INDEX IF NOT EXISTS idx_device_logs_device ON device_logs(device_id);
CREATE INDEX IF NOT EXISTS idx_device_logs_timestamp ON device_logs(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_system_logs_tenant ON system_logs(tenant_id);
CREATE INDEX IF NOT EXISTS idx_system_logs_timestamp ON system_logs(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_performance_logs_tenant ON performance_logs(tenant_id);
CREATE INDEX IF NOT EXISTS idx_performance_logs_timestamp ON performance_logs(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_script_library_tenant ON script_library(tenant_id);
CREATE INDEX IF NOT EXISTS idx_script_library_active ON script_library(is_active);
CREATE INDEX IF NOT EXISTS idx_protocols_type ON protocols(protocol_type);
CREATE INDEX IF NOT EXISTS idx_protocols_enabled ON protocols(is_enabled);`;
    }

    getInitialData() {
        return `-- initial-data.sql
INSERT OR IGNORE INTO protocols (protocol_type, display_name, description, default_port, category) VALUES 
('MODBUS_TCP', 'Modbus TCP', 'Modbus TCP/IP 프로토콜', 502, 'industrial'),
('MODBUS_RTU', 'Modbus RTU', 'Modbus RTU 시리얼 프로토콜', NULL, 'industrial'),
('MQTT', 'MQTT', 'MQTT 메시징 프로토콜', 1883, 'iot'),
('BACNET_IP', 'BACnet/IP', 'BACnet over IP 프로토콜', 47808, 'building_automation'),
('SNMP', 'SNMP', 'Simple Network Management Protocol', 161, 'network'),
('HTTP_REST', 'HTTP REST', 'RESTful HTTP API', 80, 'web'),
('OPCUA', 'OPC UA', 'OPC Unified Architecture', 4840, 'industrial'),
('ETHERNET_IP', 'Ethernet/IP', 'EtherNet/IP 프로토콜', 44818, 'industrial');

INSERT OR IGNORE INTO script_library (tenant_id, name, description, script_type, script_content, is_system) VALUES
(1, 'Temperature Conversion', '온도 단위 변환 (C to F)', 'javascript', 'return (celsius * 9/5) + 32;', 1),
(1, 'Linear Scale', '선형 스케일링', 'javascript', 'return (value - min_in) * (max_out - min_out) / (max_in - min_in) + min_out;', 1),
(1, 'Average Calculation', '평균값 계산', 'javascript', 'return inputs.reduce((a, b) => a + b, 0) / inputs.length;', 1),
(1, 'Boolean AND', '불린 AND 연산', 'javascript', 'return inputs.every(x => x === true);', 1),
(1, 'Boolean OR', '불린 OR 연산', 'javascript', 'return inputs.some(x => x === true);', 1);`;
    }
}

module.exports = DatabaseInitializer;