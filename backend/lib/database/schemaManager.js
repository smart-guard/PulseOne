// backend/lib/database/schemaManager.js
// 개선된 스키마 관리자 - 파일 분리 및 테이블 존재 확인

const fs = require('fs').promises;
const path = require('path');

class SchemaManager {
    constructor(dbFactory) {
        this.dbFactory = dbFactory;
        this.schemaVersion = '1.0.0';
        this.schemasPath = path.join(__dirname, 'schemas');
        this.dataPath = path.join(__dirname, 'data');
    }

    async initializeDatabase() {
        console.log('🗄️  Initializing database schema...');
    
        try {
            // 1. 스키마 버전 테이블 생성 (항상 먼저)
            await this.createVersionTable();
      
            // 2. 현재 스키마 버전 확인
            const currentVersion = await this.getCurrentVersion();
      
            // 3. 스키마 파일 순차 실행
            await this.executeSchemaFiles();
      
            // 4. 인덱스 생성
            await this.createIndexes();
      
            // 5. 초기 데이터 삽입 (테이블이 비어있는 경우만)
            await this.insertInitialDataIfEmpty();
      
            // 6. 스키마 버전 업데이트
            if (!currentVersion || currentVersion !== this.schemaVersion) {
                await this.updateSchemaVersion();
            }
      
            console.log('✅ Database schema initialized successfully');
            return true;
        } catch (error) {
            console.error('❌ Database initialization failed:', error);
            throw error;
        }
    }

    async createVersionTable() {
        const adapter = this.dbFactory.getQueryAdapter(this.dbFactory.config.database.type || 'sqlite');
    
        const query = `
      CREATE TABLE IF NOT EXISTS schema_versions (
        id ${adapter.autoIncrement},
        version VARCHAR(20) NOT NULL,
        applied_at ${adapter.timestamp},
        description TEXT
      )
    `;
    
        await this.dbFactory.executeQuery(query);
    }

    async executeSchemaFiles() {
        console.log('📊 Executing schema files...');
    
        // 스키마 파일 실행 순서 (의존성 고려)
        const schemaFiles = [
            'core-tables.sql',      // 기본 시스템 테이블 (tenants, system_admins 등)
            'device-tables.sql',    // 디바이스 관련 테이블
            'alarm-tables.sql',     // 알람 관련 테이블
            'log-tables.sql'        // 로그 관련 테이블
        ];

        for (const filename of schemaFiles) {
            await this.executeSchemaFile(filename);
        }
    }

    async executeSchemaFile(filename) {
        const filePath = path.join(this.schemasPath, filename);
    
        try {
            // 파일 존재 확인
            await fs.access(filePath);
      
            console.log(`📄 Processing schema file: ${filename}`);
      
            // SQL 파일 읽기
            let sqlContent = await fs.readFile(filePath, 'utf8');
      
            // 템플릿 변수 치환
            sqlContent = this.replaceTemplateVariables(sqlContent);
      
            // SQL 문 분리 및 실행
            const statements = this.parseSQLStatements(sqlContent);
      
            for (let i = 0; i < statements.length; i++) {
                const statement = statements[i].trim();
                if (statement && !statement.startsWith('--')) {
                    try {
                        await this.dbFactory.executeQuery(statement);
            
                        // 테이블 생성 확인
                        if (statement.toUpperCase().includes('CREATE TABLE')) {
                            const tableName = this.extractTableName(statement);
                            const exists = await this.tableExists(tableName);
                            console.log(`${exists ? '✅' : '❌'} Table '${tableName}': ${exists ? 'created/verified' : 'failed'}`);
                        }
                    } catch (error) {
                        console.error(`❌ Error executing statement ${i + 1} in ${filename}:`, error.message);
                        throw error;
                    }
                }
            }
      
            console.log(`✅ Schema file '${filename}' executed successfully`);
      
        } catch (error) {
            if (error.code === 'ENOENT') {
                console.warn(`⚠️  Schema file not found: ${filename}`);
            } else {
                throw error;
            }
        }
    }

    replaceTemplateVariables(sqlContent) {
        const adapter = this.dbFactory.getQueryAdapter(this.dbFactory.config.database.type || 'sqlite');
    
        // 템플릿 변수 치환
        const replacements = {
            '{{AUTO_INCREMENT}}': adapter.autoIncrement,
            '{{UUID}}': adapter.uuid,
            '{{TIMESTAMP}}': adapter.timestamp,
            '{{BOOLEAN}}': adapter.boolean,
            '{{TEXT}}': adapter.text,
            '{{TRUE}}': adapter.boolean === 'BOOLEAN' ? 'TRUE' : '1',
            '{{FALSE}}': adapter.boolean === 'BOOLEAN' ? 'FALSE' : '0'
        };

        let result = sqlContent;
        for (const [template, replacement] of Object.entries(replacements)) {
            result = result.replace(new RegExp(template, 'g'), replacement);
        }

        return result;
    }

    parseSQLStatements(sqlContent) {
    // 세미콜론으로 구분하되, 문자열 내부의 세미콜론은 무시
        const statements = [];
        let current = '';
        let inString = false;
        let stringChar = '';

        for (let i = 0; i < sqlContent.length; i++) {
            const char = sqlContent[i];
      
            if (!inString && (char === '"' || char === '\'')) {
                inString = true;
                stringChar = char;
            } else if (inString && char === stringChar) {
                inString = false;
                stringChar = '';
            } else if (!inString && char === ';') {
                if (current.trim()) {
                    statements.push(current.trim());
                }
                current = '';
                continue;
            }
      
            current += char;
        }

        if (current.trim()) {
            statements.push(current.trim());
        }

        return statements;
    }

    extractTableName(createStatement) {
        const match = createStatement.match(/CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?([`\[\"]?)(\w+)\1/i);
        return match ? match[2] : null;
    }

    async tableExists(tableName) {
        try {
            const dbType = this.dbFactory.config.database.type || 'sqlite';
      
            if (dbType === 'sqlite') {
                const result = await this.dbFactory.executeQuery(
                    'SELECT name FROM sqlite_master WHERE type=\'table\' AND name=?',
                    [tableName]
                );
                return result.length > 0;
            } else if (dbType === 'postgresql') {
                const result = await this.dbFactory.executeQuery(
                    'SELECT table_name FROM information_schema.tables WHERE table_name=$1',
                    [tableName]
                );
                return result.length > 0;
            } else if (dbType === 'mysql') {
                const result = await this.dbFactory.executeQuery(
                    'SELECT table_name FROM information_schema.tables WHERE table_name=?',
                    [tableName]
                );
                return result.length > 0;
            }
      
            return false;
        } catch (error) {
            console.warn(`Warning: Could not check table existence for '${tableName}':`, error.message);
            return false;
        }
    }

    async createIndexes() {
        console.log('🔍 Creating indexes...');
    
        try {
            const indexFile = path.join(this.schemasPath, 'indexes.sql');
            await this.executeSchemaFile('indexes.sql');
        } catch (error) {
            console.warn('⚠️  Index creation completed with warnings:', error.message);
        }
    }

    async insertInitialDataIfEmpty() {
        console.log('📝 Checking for initial data...');
    
        // 기본 데이터가 필요한 테이블들 확인
        const requiredData = [
            { table: 'system_admins', file: 'initial-data.sql' },
            // 다른 필수 초기 데이터 테이블들...
        ];

        for (const { table, file } of requiredData) {
            const isEmpty = await this.isTableEmpty(table);
            if (isEmpty) {
                console.log(`📝 Inserting initial data for '${table}'...`);
                await this.executeDataFile(file);
            } else {
                console.log(`✅ Table '${table}' already has data, skipping initial data`);
            }
        }
    }

    async isTableEmpty(tableName) {
        try {
            const result = await this.dbFactory.executeQuery(`SELECT COUNT(*) as count FROM ${tableName}`);
            const count = result[0].count || result[0].COUNT || result[0]['COUNT(*)'] || 0;
            return count === 0;
        } catch (error) {
            console.warn(`Warning: Could not check if table '${tableName}' is empty:`, error.message);
            return true; // 에러 시 비어있다고 가정
        }
    }

    async executeDataFile(filename) {
        const filePath = path.join(this.dataPath, filename);
    
        try {
            console.log(`📄 Processing data file: ${filename}`);
      
            let sqlContent = await fs.readFile(filePath, 'utf8');
            sqlContent = this.replaceTemplateVariables(sqlContent);
      
            const statements = this.parseSQLStatements(sqlContent);
      
            for (const statement of statements) {
                if (statement.trim() && !statement.startsWith('--')) {
                    try {
                        await this.dbFactory.executeQuery(statement);
                    } catch (error) {
                        console.warn(`Warning in data file ${filename}:`, error.message);
                    }
                }
            }
      
            console.log(`✅ Data file '${filename}' executed successfully`);
      
        } catch (error) {
            if (error.code === 'ENOENT') {
                console.warn(`⚠️  Data file not found: ${filename}`);
            } else {
                console.error(`❌ Error executing data file ${filename}:`, error);
            }
        }
    }

    async getCurrentVersion() {
        try {
            const result = await this.dbFactory.executeQuery(
                'SELECT version FROM schema_versions ORDER BY applied_at DESC LIMIT 1'
            );
            return result.length > 0 ? result[0].version : null;
        } catch (error) {
            return null;
        }
    }

    async updateSchemaVersion() {
        const query = `
      INSERT INTO schema_versions (version, description)
      VALUES (?, 'PulseOne schema version ${this.schemaVersion}')
    `;
    
        await this.dbFactory.executeQuery(query, [this.schemaVersion]);
        console.log(`📋 Schema version updated to ${this.schemaVersion}`);
    }
}

module.exports = SchemaManager;