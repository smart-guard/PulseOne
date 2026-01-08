// =============================================================================
// DatabaseInitializer - 스키마 불일치 문제 해결
// 🔥 핵심 수정: 메모리 스키마 제거, 파일 기반 초기화만 사용
// 🔥 SKIP_IF_INITIALIZED 로직 개선
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

        // 스키마 경로들 - 루트 data/sql 우선
        this.possibleSchemaPaths = [
            path.join(process.cwd(), 'data', 'sql'),
            path.join(process.cwd(), '..', 'data', 'sql'), // backend 폴더에서 실행 시 대비
            path.join(__dirname, 'schemas'),
            path.join(process.cwd(), 'backend', 'lib', 'database', 'schemas')
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
        if (!this.connections?.sqlite) {
            throw new Error('SQLite 연결이 없습니다');
        }

        return await this.connections.sqlite.run(statement, params);
    }

    /**
     * SQLite 전용 쿼리
     */
    async querySQLiteSQL(query, params = []) {
        if (!this.connections?.sqlite) {
            throw new Error('SQLite 연결이 없습니다');
        }

        return await this.connections.sqlite.all(query, params);
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

        console.log('❌ 스키마 폴더를 찾을 수 없습니다. 초기화 실패.');
        return null;
    }

    /**
     * SQL 파일 실행 (메모리 스키마 폴백 제거)
     */
    async executeSQLFile(filename) {
        try {
            const schemasPath = await this.findSchemasPath();

            if (!schemasPath) {
                console.error(`❌ 스키마 경로를 찾을 수 없어 ${filename} 실행 불가`);
                return false;
            }

            const filePath = path.join(schemasPath, filename);

            try {
                const sqlContent = await fs.readFile(filePath, 'utf8');
                console.log(`📁 실제 파일에서 읽음: ${filePath}`);

                if (!sqlContent || sqlContent.trim().length === 0) {
                    console.log(`⚠️ ${filename} 파일이 비어있음, 스킵`);
                    return true;
                }

                // SQL 파싱 및 실행
                const statements = this.parseAdvancedSQLStatements(sqlContent);
                console.log(`  📁 ${filename}: ${statements.length}개 SQL 명령 실행 중...`);

                let successCount = 0;
                for (const statement of statements) {
                    try {
                        await this.executeSQL(statement);
                        successCount++;
                    } catch (error) {
                        console.log(`    ⚠️ SQL 실행 실패: ${error.message}`);
                        console.log(`    📝 실패한 SQL (일부): ${statement.substring(0, 100)}...`);
                    }
                }

                console.log(`  ✅ ${filename} 실행 완료 (${successCount}/${statements.length})`);
                return successCount > 0;

            } catch (fileError) {
                console.error(`❌ ${filename} 파일 읽기 실패: ${fileError.message}`);
                return false;
            }

        } catch (error) {
            console.error(`❌ SQL 파일 실행 실패 (${filename}):`, error.message);
            return false;
        }
    }

    /**
     * 개선된 SQL 파싱 - JavaScript 코드가 포함된 SQL 문 올바르게 처리
     */
    parseAdvancedSQLStatements(sqlContent) {
        // 주석 제거 (단, 문자열 내부의 주석은 보존)
        let cleanedContent = this.removeCommentsFromSQL(sqlContent);

        // 문자열 리터럴 임시 치환 (JavaScript 코드 보호)
        const { content: protectedContent, placeholders } = this.protectStringLiterals(cleanedContent);

        // 세미콜론으로 분할 (보호된 문자열 내부의 세미콜론은 분할하지 않음)
        const rawStatements = protectedContent.split(';');

        // 문자열 리터럴 복원 및 정리
        const statements = rawStatements
            .map(stmt => this.restoreStringLiterals(stmt.trim(), placeholders))
            .filter(stmt => stmt.length > 0 && !stmt.match(/^\s*--/)) // 빈 문장과 주석 제거
            .map(stmt => stmt.trim());

        return statements;
    }

    /**
     * SQL에서 주석 제거 (문자열 내부 제외)
     */
    removeCommentsFromSQL(sql) {
        const lines = sql.split('\n');
        const cleanedLines = [];

        for (let line of lines) {
            // 문자열 내부가 아닌 경우에만 주석 제거
            let inSingleQuote = false;
            let inDoubleQuote = false;
            let cleaned = '';

            for (let i = 0; i < line.length; i++) {
                const char = line[i];
                const nextChar = line[i + 1];

                if (char === '\'' && !inDoubleQuote) {
                    inSingleQuote = !inSingleQuote;
                } else if (char === '"' && !inSingleQuote) {
                    inDoubleQuote = !inDoubleQuote;
                } else if (char === '-' && nextChar === '-' && !inSingleQuote && !inDoubleQuote) {
                    // 문자열 외부의 주석 시작점에서 나머지 줄 무시
                    break;
                }

                cleaned += char;
            }

            if (cleaned.trim()) {
                cleanedLines.push(cleaned);
            }
        }

        return cleanedLines.join('\n');
    }

    /**
     * 문자열 리터럴을 임시 플레이스홀더로 치환
     */
    protectStringLiterals(sql) {
        const placeholders = {};
        let placeholderIndex = 0;
        let result = '';
        let i = 0;

        while (i < sql.length) {
            const char = sql[i];

            if (char === '\'' || char === '"') {
                // 문자열 시작
                const quote = char;
                let stringLiteral = quote;
                i++; // 시작 따옴표 다음으로

                // 문자열 끝까지 찾기
                while (i < sql.length) {
                    const currentChar = sql[i];
                    stringLiteral += currentChar;

                    if (currentChar === quote) {
                        // 이스케이프된 따옴표 확인 (연속된 따옴표)
                        if (i + 1 < sql.length && sql[i + 1] === quote) {
                            stringLiteral += sql[i + 1];
                            i += 2;
                            continue;
                        } else {
                            // 문자열 끝
                            break;
                        }
                    }
                    i++;
                }

                // 플레이스홀더로 치환
                const placeholder = `__STRING_LITERAL_${placeholderIndex}__`;
                placeholders[placeholder] = stringLiteral;
                result += placeholder;
                placeholderIndex++;
            } else {
                result += char;
            }
            i++;
        }

        return { content: result, placeholders };
    }

    /**
     * 플레이스홀더를 원래 문자열로 복원
     */
    restoreStringLiterals(statement, placeholders) {
        let restored = statement;
        for (const [placeholder, original] of Object.entries(placeholders)) {
            restored = restored.replace(new RegExp(placeholder, 'g'), original);
        }
        return restored;
    }

    /**
     * 테이블 존재 확인 (데이터베이스 타입별)
     */
    async doesTableExist(tableName) {
        try {
            switch (this.databaseType) {
            case 'sqlite':
                const sqliteResult = await this.querySQL(
                    'SELECT name FROM sqlite_master WHERE type=\'table\' AND name = ?',
                    [tableName]
                );
                return sqliteResult.length > 0;

            case 'postgresql':
                const pgResult = await this.querySQL(
                    'SELECT table_name FROM information_schema.tables WHERE table_schema = \'public\' AND table_name = $1',
                    [tableName]
                );
                return pgResult.length > 0;

            case 'mysql':
                const mysqlResult = await this.querySQL(
                    'SELECT table_name FROM information_schema.tables WHERE table_schema = DATABASE() AND table_name = ?',
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
     * 🔥 핵심: 개선된 초기화 상태 확인 (안전한 체크)
     */
    async checkIfAlreadyInitialized() {
        try {
            console.log('🔍 데이터베이스 초기화 상태 확인 중...');

            // 1단계: 기본 연결 확인
            if (!this.connections) {
                console.log('📋 데이터베이스 연결이 없음 - 초기화 필요');
                return false;
            }

            // 2단계: 핵심 테이블 존재 확인 (안전한 방식)
            const requiredTables = ['tenants', 'users', 'sites', 'protocols', 'devices'];
            let missingTables = [];

            for (const tableName of requiredTables) {
                try {
                    const exists = await this.doesTableExist(tableName);
                    if (!exists) {
                        missingTables.push(tableName);
                    }
                } catch (error) {
                    // 테이블 확인 자체가 실패하면 해당 테이블이 없는 것으로 간주
                    console.log(`📋 테이블 '${tableName}' 확인 실패 (${error.message}) - 없는 것으로 간주`);
                    missingTables.push(tableName);
                }
            }

            if (missingTables.length > 0) {
                console.log(`📋 누락된 필수 테이블: ${missingTables.join(', ')} - 초기화 필요`);
                return false;
            }

            console.log('✅ 모든 필수 테이블 존재함');

            // 3단계: 기본 데이터 존재 확인 (안전한 방식)
            try {
                const tenantResult = await this.querySQL('SELECT COUNT(*) as count FROM tenants');
                const tenantCount = parseInt(tenantResult[0]?.count || '0');

                if (tenantCount === 0) {
                    console.log('📊 테넌트 데이터 없음 - 초기화 필요');
                    return false;
                }

                console.log(`📊 테넌트 데이터 확인: ${tenantCount}개`);
            } catch (error) {
                console.log(`📊 테넌트 데이터 확인 실패 (${error.message}) - 초기화 필요`);
                return false;
            }

            console.log('✅ 데이터베이스가 완전히 초기화되어 있음');
            return true;

        } catch (error) {
            // 최상위 예외 처리: 어떤 예상치 못한 에러든 초기화 필요로 간주
            console.log(`❌ 초기화 상태 확인 중 예외 발생: ${error.message}`);
            console.log('🔧 안전을 위해 초기화 진행');
            return false;
        }
    }

    /**
     * 🔥 핵심: 완전 자동 초기화 (SKIP_IF_INITIALIZED 로직 개선)
     */
    async performAutoInitialization() {
        try {
            console.log('🚀 완전 자동 초기화 시작...\n');

            // 스키마 경로 확인
            await this.findSchemasPath();
            if (!this.schemasPath) {
                console.error('❌ 스키마 파일을 찾을 수 없어 초기화 불가');
                return false;
            }

            // SKIP_IF_INITIALIZED 체크 (개선된 로직)
            const skipIfInitialized = this.config.getBoolean('SKIP_IF_INITIALIZED', true);

            if (skipIfInitialized) {
                console.log('🔍 기존 데이터베이스 상태 확인 중...');

                const isAlreadyInitialized = await this.checkIfAlreadyInitialized();

                if (isAlreadyInitialized) {
                    console.log('✅ 데이터베이스가 이미 완전히 초기화되어 있습니다. 초기화를 건너뜁니다.');
                    console.log('💡 강제 초기화를 원하면 SKIP_IF_INITIALIZED=false로 설정하세요.');
                    return true;
                }

                console.log('📋 기존 데이터가 불완전하거나 없어서 초기화를 진행합니다.');
            } else {
                console.log('🔧 SKIP_IF_INITIALIZED=false 설정으로 강제 초기화 진행');
            }

            // 실제 초기화 단계
            console.log('📋 [1/2] 마스터 스키마 생성 중 (schema.sql)...');
            await this.executeSQLFile('schema.sql');

            console.log('📊 [2/2] 마스터 시드 데이터 삽입 중 (seed.sql)...');
            await this.executeSQLFile('seed.sql');

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
        const sqlFiles = ['01-core-tables.sql', '02-users-sites.sql', '03-protocols-table.sql'];
        for (const sqlFile of sqlFiles) {
            await this.executeSQLFile(sqlFile);
        }
    }

    async createExtendedTables() {
        const sqlFiles = ['04-device-tables.sql', '05-alarm-tables.sql', '06-virtual-points.sql', '07-log-tables.sql'];
        for (const sqlFile of sqlFiles) {
            await this.executeSQLFile(sqlFile);
        }
    }

    async createIndexes() {
        await this.executeSQLFile('08-indexes.sql');
    }

    async checkSystemTables() {
        const systemTables = ['tenants', 'users', 'sites', 'protocols', 'devices'];
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
        const extendedTables = ['virtual_points', 'alarm_rules', 'system_logs'];
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
                indexQuery = 'SELECT name FROM sqlite_master WHERE type=\'index\' AND name = ?';
                indexName = 'idx_users_tenant';
                break;
            case 'postgresql':
                indexQuery = 'SELECT indexname FROM pg_indexes WHERE indexname = $1';
                indexName = 'idx_users_tenant';
                break;
            case 'mysql':
                indexQuery = 'SELECT index_name FROM information_schema.statistics WHERE index_name = ?';
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

    /**
     * 🔥 수정: 실제 테이블 구조에 맞는 기본 데이터 생성
     */
    async createSampleData() {
        try {
            console.log('  🔍 tenants 테이블 구조 확인 중...');

            // 실제 테이블 구조에 맞는 INSERT
            try {
                await this.executeSQL(
                    `INSERT OR IGNORE INTO tenants (company_name, company_code, domain, contact_email, subscription_plan, subscription_status, is_active) 
                    VALUES ('Default Company', 'DEFAULT', 'default.local', 'admin@default.local', 'starter', 'active', 1)`
                );
                console.log('  ✅ 기본 테넌트 생성 성공');
            } catch (tenantError) {
                console.log(`  ⚠️ 테넌트 생성 실패: ${tenantError.message}`);
            }

            // 사용자는 성공했으므로 그대로 유지
            try {
                await this.executeSQL(
                    `INSERT OR IGNORE INTO users (tenant_id, username, email, role, is_active) 
                    VALUES (1, 'admin', 'admin@pulseone.local', 'admin', 1)`
                );
                console.log('  ✅ 기본 사용자 생성 성공');
            } catch (userError) {
                console.log(`  ⚠️ 사용자 생성 실패: ${userError.message}`);
            }

        } catch (error) {
            console.error('기본 데이터 생성 실패:', error.message);
        }
    }

    async createBackup(force = false) {
        try {
            if (this.databaseType === 'sqlite') {
                const dbConfig = this.config.getDatabaseConfig();

                // 🔥 수정: 데이터베이스 파일과 같은 디렉토리에 backup 폴더 생성
                const dbPath = dbConfig.sqlite.path; // 예: /app/data/db/pulseone.db
                const dbDir = path.dirname(dbPath);  // 예: /app/data/db
                const backupDir = path.join(dbDir, '..', 'backup'); // /app/data/backup

                await fs.mkdir(backupDir, { recursive: true });

                const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
                const backupPath = path.join(backupDir, `pulseone_backup_${timestamp}.db`);

                await fs.copyFile(dbPath, backupPath);
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
}

module.exports = DatabaseInitializer;