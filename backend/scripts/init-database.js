#!/usr/bin/env node

// ============================================================================
// backend/scripts/init-database.js
// 멀티 데이터베이스 초기화 스크립트
// ============================================================================

const path = require('path');
const fs = require('fs').promises;
const DatabaseFactory = require('../lib/database/dbFactory');

class DatabaseInitializer {
    constructor() {
        this.dbFactory = new DatabaseFactory();
        this.dbType = this.dbFactory.config.database.type;
        this.schemasPath = path.join(__dirname, '../lib/database/schemas');
        
        console.log(`🚀 Database Initializer starting for: ${this.dbType.toUpperCase()}`);
    }

    /**
     * 메인 초기화 실행
     */
    async initialize() {
        try {
            console.log('🔧 Starting database initialization...');
            
            // 1. 연결 테스트
            await this.testConnection();
            
            // 2. 스키마 생성
            await this.createSchemas();
            
            // 3. 초기 데이터 삽입
            await this.insertInitialData();
            
            // 4. 인덱스 생성
            await this.createIndexes();
            
            console.log('✅ Database initialization completed successfully!');
            
        } catch (error) {
            console.error('❌ Database initialization failed:', error);
            throw error;
        } finally {
            await this.dbFactory.closeAllConnections();
        }
    }

    /**
     * 데이터베이스 연결 테스트
     */
    async testConnection() {
        console.log('🔌 Testing database connection...');
        
        try {
            const connection = await this.dbFactory.getMainConnection();
            
            // 간단한 쿼리로 연결 확인
            await this.dbFactory.executeQuery('SELECT 1 as test');
            
            console.log(`✅ Connected to ${this.dbType} successfully`);
            
        } catch (error) {
            console.error(`❌ Failed to connect to ${this.dbType}:`, error.message);
            throw error;
        }
    }

    /**
     * 스키마 파일들 실행
     */
    async createSchemas() {
        console.log('📋 Creating database schemas...');
        
        const schemaFiles = [
            '01-core-tables.sql',
            '02-users-sites.sql', 
            '03-device-tables.sql',
            '04-virtual-points.sql',
            '05-alarm-tables.sql',
            '06-log-tables.sql'
        ];

        for (const filename of schemaFiles) {
            await this.executeSchemaFile(filename);
        }
    }

    /**
     * 개별 스키마 파일 실행
     */
    async executeSchemaFile(filename) {
        const filePath = path.join(this.schemasPath, filename);
        
        try {
            console.log(`📄 Processing: ${filename}`);
            
            // 파일 읽기
            let sqlContent = await fs.readFile(filePath, 'utf8');
            
            // DB별 쿼리 변환
            const queryAdapter = this.dbFactory.getQueryAdapter();
            sqlContent = queryAdapter.adaptQuery(sqlContent);
            
            // SQL 문 분리 및 실행
            const statements = this.parseSQLStatements(sqlContent);
            
            for (const statement of statements) {
                if (statement.trim() && !statement.startsWith('--')) {
                    try {
                        await this.dbFactory.executeQuery(statement);
                    } catch (error) {
                        // 테이블이 이미 존재하는 경우는 경고만 출력
                        if (error.message.includes('already exists') || 
                            error.message.includes('duplicate') ||
                            error.message.includes('already defined')) {
                            console.warn(`⚠️  ${filename}: ${error.message}`);
                        } else {
                            throw error;
                        }
                    }
                }
            }
            
            console.log(`✅ ${filename} executed successfully`);
            
        } catch (error) {
            if (error.code === 'ENOENT') {
                console.warn(`⚠️  Schema file not found: ${filename}`);
            } else {
                console.error(`❌ Error executing ${filename}:`, error);
                throw error;
            }
        }
    }

    /**
     * SQL 문 파싱 (세미콜론으로 분리)
     */
    parseSQLStatements(sqlContent) {
        // 주석 제거
        const cleanedSql = sqlContent
            .replace(/--.*$/gm, '') // 라인 주석
            .replace(/\/\*[\s\S]*?\*\//g, ''); // 블록 주석
        
        // 세미콜론으로 분리하되 문자열 내부의 세미콜론은 제외
        const statements = [];
        let currentStatement = '';
        let inString = false;
        let stringChar = '';
        
        for (let i = 0; i < cleanedSql.length; i++) {
            const char = cleanedSql[i];
            
            if (!inString && (char === '"' || char === "'")) {
                inString = true;
                stringChar = char;
            } else if (inString && char === stringChar) {
                inString = false;
                stringChar = '';
            } else if (!inString && char === ';') {
                if (currentStatement.trim()) {
                    statements.push(currentStatement.trim());
                    currentStatement = '';
                }
                continue;
            }
            
            currentStatement += char;
        }
        
        // 마지막 문 추가
        if (currentStatement.trim()) {
            statements.push(currentStatement.trim());
        }
        
        return statements;
    }

    /**
     * 초기 데이터 삽입
     */
    async insertInitialData() {
        console.log('📝 Inserting initial data...');
        
        try {
            // 시스템 관리자 생성
            await this.createSystemAdmin();
            
            // 기본 테넌트 생성 (데모용)
            await this.createDemoTenant();
            
            // 기본 사이트 생성
            await this.createDemoSite();
            
        } catch (error) {
            console.warn('⚠️  Initial data insertion warnings:', error.message);
        }
    }

    /**
     * 시스템 관리자 생성
     */
    async createSystemAdmin() {
        const checkQuery = 'SELECT COUNT(*) as count FROM users WHERE role = ? AND tenant_id IS NULL';
        const result = await this.dbFactory.executeQuery(checkQuery, ['system_admin']);
        
        const count = this.getCountFromResult(result);
        
        if (count === 0) {
            const insertQuery = `
                INSERT INTO users (
                    tenant_id, username, email, password_hash, full_name, role, 
                    permissions, is_active, created_at, updated_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ${this.getCurrentTimestamp()}, ${this.getCurrentTimestamp()})
            `;
            
            const params = [
                null, // system admin은 tenant_id가 null
                'admin',
                'admin@pulseone.local',
                '$2b$10$YourHashedPasswordHere', // TODO: 실제 환경에서는 bcrypt로 해시된 비밀번호
                'System Administrator',
                'system_admin',
                JSON.stringify(['*']), // 모든 권한
                1
            ];
            
            await this.dbFactory.executeQuery(insertQuery, params);
            console.log('✅ System administrator created');
        } else {
            console.log('✅ System administrator already exists');
        }
    }

    /**
     * 데모 테넌트 생성
     */
    async createDemoTenant() {
        const checkQuery = 'SELECT COUNT(*) as count FROM tenants WHERE company_code = ?';
        const result = await this.dbFactory.executeQuery(checkQuery, ['DEMO']);
        
        const count = this.getCountFromResult(result);
        
        if (count === 0) {
            const insertQuery = `
                INSERT INTO tenants (
                    company_name, company_code, contact_name, contact_email,
                    subscription_plan, subscription_status, is_active,
                    created_at, updated_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ${this.getCurrentTimestamp()}, ${this.getCurrentTimestamp()})
            `;
            
            const params = [
                'Demo Company',
                'DEMO',
                'Demo Manager',
                'demo@company.com',
                'professional',
                'active',
                1
            ];
            
            await this.dbFactory.executeQuery(insertQuery, params);
            console.log('✅ Demo tenant created');
        } else {
            console.log('✅ Demo tenant already exists');
        }
    }

    /**
     * 데모 사이트 생성
     */
    async createDemoSite() {
        // 데모 테넌트 ID 조회
        const tenantQuery = 'SELECT id FROM tenants WHERE company_code = ?';
        const tenantResult = await this.dbFactory.executeQuery(tenantQuery, ['DEMO']);
        
        if (tenantResult.length === 0) return;
        
        const tenantId = this.getIdFromResult(tenantResult[0]);
        
        const checkQuery = 'SELECT COUNT(*) as count FROM sites WHERE tenant_id = ? AND code = ?';
        const result = await this.dbFactory.executeQuery(checkQuery, [tenantId, 'DEMO_SITE']);
        
        const count = this.getCountFromResult(result);
        
        if (count === 0) {
            const insertQuery = `
                INSERT INTO sites (
                    tenant_id, name, code, site_type, description, 
                    hierarchy_level, hierarchy_path, is_active,
                    created_at, updated_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ${this.getCurrentTimestamp()}, ${this.getCurrentTimestamp()})
            `;
            
            const params = [
                tenantId,
                'Demo Factory',
                'DEMO_SITE',
                'factory',
                'Demonstration factory for PulseOne',
                0,
                '/DEMO_SITE',
                1
            ];
            
            await this.dbFactory.executeQuery(insertQuery, params);
            console.log('✅ Demo site created');
        } else {
            console.log('✅ Demo site already exists');
        }
    }

    /**
     * 인덱스 생성
     */
    async createIndexes() {
        console.log('🔍 Creating indexes...');
        
        try {
            await this.executeSchemaFile('07-indexes.sql');
        } catch (error) {
            console.warn('⚠️  Index creation completed with warnings:', error.message);
        }
    }

    /**
     * DB별 현재 시간 함수 반환
     */
    getCurrentTimestamp() {
        switch (this.dbType) {
            case 'postgresql':
            case 'postgres':
                return 'NOW()';
            case 'sqlite':
            case 'sqlite3':
                return "datetime('now')";
            case 'mariadb':
            case 'mysql':
                return 'NOW()';
            case 'mssql':
            case 'sqlserver':
                return 'GETDATE()';
            default:
                return 'NOW()';
        }
    }

    /**
     * DB별 COUNT 결과 파싱
     */
    getCountFromResult(result) {
        if (Array.isArray(result)) {
            const row = result[0];
            return row.count || row.COUNT || row['COUNT(*)'] || 0;
        }
        return result.count || result.COUNT || result['COUNT(*)'] || 0;
    }

    /**
     * DB별 ID 결과 파싱
     */
    getIdFromResult(result) {
        return result.id || result.ID || result[0];
    }
}

// 스크립트 직접 실행 시
if (require.main === module) {
    const initializer = new DatabaseInitializer();
    
    initializer.initialize()
        .then(() => {
            console.log('🎉 Database initialization completed!');
            process.exit(0);
        })
        .catch((error) => {
            console.error('💥 Database initialization failed:', error);
            process.exit(1);
        });
}

module.exports = DatabaseInitializer;