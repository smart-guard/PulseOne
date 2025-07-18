#!/usr/bin/env node

/**
 * 🎯 PulseOne Database Initializer
 * 모든 DB 타입을 하나의 클래스로 처리하는 통합 초기화 시스템
 */

const fs = require('fs').promises;
const path = require('path');
const readline = require('readline');
require('dotenv').config();

class DatabaseInitializer {
    constructor() {
        this.dbType = process.env.DB_TYPE || 'sqlite';
        this.rl = readline.createInterface({
            input: process.stdin,
            output: process.stdout
        });
        
        // 초기화 상태 추적
        this.initStatus = {
            systemTables: false,
            tenantSchemas: false,
            sampleData: false,
            redis: false,
            configuration: false
        };
        
        // DB 연결 객체들
        this.connections = {
            main: null,
            redis: null
        };
    }

    /**
     * 🚀 메인 실행 함수
     */
    async run() {
        try {
            console.log('\n🚀 PulseOne 통합 데이터베이스 초기화 v2.0');
            console.log('===============================================\n');
            
            // 현재 DB 타입 표시
            console.log(`📊 현재 DB 타입: ${this.getDBIcon()} ${this.getDBName()}\n`);
            
            // 자동 초기화 모드 체크
            if (process.env.AUTO_INITIALIZE_ON_START === 'true') {
                await this.autoInitialize();
            } else {
                await this.showInteractiveMenu();
            }
            
        } catch (error) {
            console.error('❌ 초기화 중 오류 발생:', error.message);
            if (process.env.LOG_LEVEL === 'debug') {
                console.error(error.stack);
            }
        } finally {
            await this.cleanup();
            this.rl.close();
        }
    }

    /**
     * 🔍 DB 타입별 아이콘 반환
     */
    getDBIcon() {
        const icons = {
            'postgresql': '🐘',
            'mssql': '🔵',
            'sqlite': '💾',
            'mariadb': '🔶'
        };
        return icons[this.dbType] || '🗄️';
    }

    /**
     * 📊 DB 타입별 이름 반환
     */
    getDBName() {
        const names = {
            'postgresql': 'PostgreSQL',
            'mssql': 'Microsoft SQL Server',
            'sqlite': 'SQLite',
            'mariadb': 'MariaDB'
        };
        return names[this.dbType] || 'Unknown Database';
    }

    /**
     * 🔄 자동 초기화 실행
     */
    async autoInitialize() {
        console.log('🔍 데이터베이스 상태 확인 중...');
        
        await this.checkDatabaseStatus();
        
        if (this.isFullyInitialized() && process.env.SKIP_IF_INITIALIZED !== 'false') {
            console.log('✅ 데이터베이스가 이미 초기화되어 있습니다. 건너뜁니다.\n');
            return;
        }
        
        if (!this.isFullyInitialized()) {
            console.log('🔧 초기화가 필요한 항목들을 감지했습니다...\n');
            await this.performInitialization();
        }
    }

    /**
     * 🎛️ 인터랙티브 메뉴 표시
     */
    async showInteractiveMenu() {
        await this.checkDatabaseStatus();
        
        console.log('🎛️ 초기화 옵션을 선택하세요:\n');
        console.log('1. 🔍 현재 상태 확인');
        console.log('2. 🚀 전체 자동 초기화');
        console.log('3. 🔧 부분 초기화 (선택적)');
        console.log('4. 💾 백업 생성');
        console.log('5. 🗑️  데이터베이스 재설정 (주의!)');
        console.log('6. ❌ 종료\n');
        
        const choice = await this.askQuestion('선택하세요 (1-6): ');
        
        switch (choice.trim()) {
            case '1': await this.showDetailedStatus(); break;
            case '2': await this.performFullInitialization(); break;
            case '3': await this.performSelectiveInitialization(); break;
            case '4': await this.createBackup(); break;
            case '5': await this.resetDatabase(); break;
            case '6': console.log('👋 초기화를 종료합니다.'); return;
            default: 
                console.log('❌ 잘못된 선택입니다.');
                await this.showInteractiveMenu();
        }
    }

    /**
     * 🔍 데이터베이스 상태 확인 (DB 타입별 자동 처리)
     */
    async checkDatabaseStatus() {
        console.log('🔍 데이터베이스 연결 및 상태 확인 중...\n');
        
        try {
            // DB 타입별 연결 및 상태 확인
            switch (this.dbType) {
                case 'postgresql':
                    await this.checkPostgreSQLStatus();
                    break;
                case 'mssql':
                    await this.checkMSSQLStatus();
                    break;
                case 'sqlite':
                    await this.checkSQLiteStatus();
                    break;
                case 'mariadb':
                    await this.checkMariaDBStatus();
                    break;
                default:
                    throw new Error(`지원하지 않는 DB 타입: ${this.dbType}`);
            }
            
            // Redis 상태 확인 (선택적)
            if (process.env.USE_REDIS !== 'false') {
                await this.checkRedisStatus();
            }
            
        } catch (error) {
            console.error('❌ 상태 확인 중 오류:', error.message);
        }
    }

    /**
     * 🐘 PostgreSQL 상태 확인
     */
    async checkPostgreSQLStatus() {
        const { Pool } = require('pg');
        const pool = new Pool({
            host: process.env.POSTGRES_MAIN_DB_HOST,
            port: process.env.POSTGRES_MAIN_DB_PORT,
            user: process.env.POSTGRES_MAIN_DB_USER,
            password: process.env.POSTGRES_MAIN_DB_PASSWORD,
            database: process.env.POSTGRES_MAIN_DB_NAME
        });
        
        try {
            await pool.query('SELECT NOW()');
            console.log('✅ PostgreSQL 연결 성공');
            
            await this.checkCommonTables(pool, 'postgresql');
            this.connections.main = pool;
            
        } catch (error) {
            console.log('❌ PostgreSQL 연결 실패:', error.message);
            await pool.end();
        }
    }

    /**
     * 🔵 MSSQL 상태 확인
     */
    async checkMSSQLStatus() {
        const sql = require('mssql');
        const config = {
            server: process.env.MSSQL_HOST,
            port: parseInt(process.env.MSSQL_PORT),
            user: process.env.MSSQL_USER,
            password: process.env.MSSQL_PASSWORD,
            database: process.env.MSSQL_DATABASE,
            options: {
                encrypt: process.env.MSSQL_ENCRYPT === 'true',
                trustServerCertificate: process.env.MSSQL_TRUST_SERVER_CERTIFICATE !== 'false'
            }
        };
        
        try {
            const pool = await sql.connect(config);
            console.log('✅ MSSQL 연결 성공');
            
            await this.checkCommonTables(pool, 'mssql');
            this.connections.main = pool;
            
        } catch (error) {
            console.log('❌ MSSQL 연결 실패:', error.message);
        }
    }

    /**
     * 💾 SQLite 상태 확인
     */
    async checkSQLiteStatus() {
        const sqlite3 = require('sqlite3').verbose();
        const dbPath = process.env.SQLITE_DB_PATH || './data/pulseone.db';
        
        return new Promise((resolve) => {
            const db = new sqlite3.Database(dbPath, (err) => {
                if (err) {
                    console.log('❌ SQLite 연결 실패:', err.message);
                    resolve();
                    return;
                }
                
                console.log('✅ SQLite 연결 성공');
                
                // SQLite 용 테이블 확인
                db.get(`SELECT name FROM sqlite_master WHERE type='table' AND name='tenants'`, (err, row) => {
                    this.initStatus.systemTables = !!row;
                    console.log(`   - 시스템 테이블: ${this.initStatus.systemTables ? '✅' : '❌'}`);
                    
                    // 간단한 샘플 데이터 확인
                    if (this.initStatus.systemTables) {
                        db.get(`SELECT COUNT(*) as count FROM tenants WHERE company_code = 'demo'`, (err, row) => {
                            this.initStatus.sampleData = row && row.count > 0;
                            console.log(`   - 샘플 데이터: ${this.initStatus.sampleData ? '✅' : '❌'}`);
                            
                            this.connections.main = db;
                            resolve();
                        });
                    } else {
                        this.connections.main = db;
                        resolve();
                    }
                });
            });
        });
    }

    /**
     * 🔶 MariaDB 상태 확인
     */
    async checkMariaDBStatus() {
        const mariadb = require('mariadb');
        
        try {
            const pool = mariadb.createPool({
                host: process.env.MARIADB_HOST,
                port: process.env.MARIADB_PORT,
                user: process.env.MARIADB_USER,
                password: process.env.MARIADB_PASSWORD,
                database: process.env.MARIADB_DATABASE
            });
            
            const conn = await pool.getConnection();
            console.log('✅ MariaDB 연결 성공');
            
            await this.checkCommonTables(conn, 'mariadb');
            this.connections.main = pool;
            
        } catch (error) {
            console.log('❌ MariaDB 연결 실패:', error.message);
        }
    }

    /**
     * 📊 공통 테이블 확인 (PostgreSQL, MSSQL, MariaDB용)
     */
    async checkCommonTables(connection, dbType) {
        try {
            let query;
            
            if (dbType === 'mssql') {
                query = `SELECT COUNT(*) as count FROM INFORMATION_SCHEMA.TABLES 
                        WHERE TABLE_SCHEMA = 'dbo' AND TABLE_NAME IN ('tenants', 'system_admins')`;
            } else {
                query = `SELECT COUNT(*) as count FROM information_schema.tables 
                        WHERE table_schema = 'public' AND table_name IN ('tenants', 'system_admins')`;
            }
            
            const result = await connection.query(query);
            const count = dbType === 'mssql' ? result.recordset[0].count : result.rows[0].count;
            
            this.initStatus.systemTables = count >= 2;
            console.log(`   - 시스템 테이블: ${this.initStatus.systemTables ? '✅' : '❌'}`);
            
            // 샘플 데이터 확인
            if (this.initStatus.systemTables) {
                const sampleQuery = `SELECT COUNT(*) as count FROM tenants WHERE company_code = 'demo'`;
                const sampleResult = await connection.query(sampleQuery);
                const sampleCount = dbType === 'mssql' ? sampleResult.recordset[0].count : sampleResult.rows[0].count;
                
                this.initStatus.sampleData = sampleCount > 0;
                console.log(`   - 샘플 데이터: ${this.initStatus.sampleData ? '✅' : '❌'}`);
            }
            
        } catch (error) {
            console.log(`   ⚠️  테이블 확인 실패: ${error.message}`);
        }
    }

    /**
     * 🔴 Redis 상태 확인
     */
    async checkRedisStatus() {
        if (this.dbType === 'sqlite' && process.env.USE_REDIS === 'false') {
            console.log('⏭️  Redis 확인 건너뜀 (SQLite 단독 모드)');
            return;
        }
        
        try {
            const redis = require('redis');
            const client = redis.createClient({
                url: `redis://${process.env.REDIS_MAIN_HOST || 'localhost'}:${process.env.REDIS_MAIN_PORT || 6379}`,
                password: process.env.REDIS_MAIN_PASSWORD || undefined
            });
            
            await client.connect();
            await client.ping();
            console.log('✅ Redis 연결 성공');
            
            const keys = await client.keys('pulseone:*');
            this.initStatus.redis = keys.length > 0;
            console.log(`   - Redis 데이터: ${this.initStatus.redis ? '✅' : '❌'}`);
            
            this.connections.redis = client;
            
        } catch (error) {
            console.log('❌ Redis 연결 실패:', error.message);
        }
    }

    /**
     * ✅ 완전 초기화 여부 확인
     */
    isFullyInitialized() {
        const coreInitialized = this.initStatus.systemTables;
        const sampleDataOk = this.initStatus.sampleData || process.env.CREATE_SAMPLE_DATA === 'false';
        
        return coreInitialized && sampleDataOk;
    }

    /**
     * 🔧 실제 초기화 작업 수행
     */
    async performInitialization() {
        const steps = [
            { 
                name: '시스템 테이블 생성', 
                fn: () => this.initializeSystemTables(), 
                condition: !this.initStatus.systemTables 
            },
            { 
                name: '샘플 데이터 생성', 
                fn: () => this.initializeSampleData(), 
                condition: !this.initStatus.sampleData && process.env.CREATE_SAMPLE_DATA !== 'false' 
            },
            { 
                name: 'Redis 초기화', 
                fn: () => this.initializeRedis(), 
                condition: !this.initStatus.redis && process.env.USE_REDIS !== 'false' 
            }
        ];
        
        const activeSteps = steps.filter(step => step.condition);
        
        if (activeSteps.length === 0) {
            console.log('✅ 모든 항목이 이미 초기화되어 있습니다.\n');
            return;
        }
        
        for (let i = 0; i < activeSteps.length; i++) {
            const step = activeSteps[i];
            console.log(`📋 ${i + 1}/${activeSteps.length}: ${step.name}...`);
            
            try {
                await step.fn();
                console.log(`   ✅ 완료\n`);
            } catch (error) {
                console.error(`   ❌ 실패: ${error.message}\n`);
                throw error;
            }
        }
        
        console.log('🎉 모든 초기화가 완료되었습니다!\n');
    }

    /**
     * 🏗️ 시스템 테이블 생성 (DB 타입별 자동 처리)
     */
    async initializeSystemTables() {
        switch (this.dbType) {
            case 'postgresql':
                await this.createPostgreSQLTables();
                break;
            case 'mssql':
                await this.createMSSQLTables();
                break;
            case 'sqlite':
                await this.createSQLiteTables();
                break;
            case 'mariadb':
                await this.createMariaDBTables();
                break;
        }
        
        this.initStatus.systemTables = true;
    }

    /**
     * 🐘 PostgreSQL 테이블 생성
     */
    async createPostgreSQLTables() {
        const pool = this.connections.main;
        
        // 확장 생성
        await pool.query('CREATE EXTENSION IF NOT EXISTS "uuid-ossp"');
        
        // 기본 테이블들 생성
        await pool.query(`
            CREATE TABLE IF NOT EXISTS tenants (
                id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
                company_name VARCHAR(100) NOT NULL UNIQUE,
                company_code VARCHAR(20) NOT NULL UNIQUE,
                contact_email VARCHAR(100),
                subscription_plan VARCHAR(20) DEFAULT 'starter',
                is_active BOOLEAN DEFAULT true,
                created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
            )
        `);
        
        await pool.query(`
            CREATE TABLE IF NOT EXISTS system_admins (
                id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
                username VARCHAR(50) UNIQUE NOT NULL,
                email VARCHAR(100) UNIQUE NOT NULL,
                password_hash VARCHAR(255) NOT NULL,
                full_name VARCHAR(100),
                is_active BOOLEAN DEFAULT true,
                created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
            )
        `);
    }

    /**
     * 🔵 MSSQL 테이블 생성
     */
    async createMSSQLTables() {
        const pool = this.connections.main;
        
        await pool.request().query(`
            IF NOT EXISTS (SELECT * FROM sysobjects WHERE name='tenants' AND xtype='U')
            CREATE TABLE tenants (
                id UNIQUEIDENTIFIER PRIMARY KEY DEFAULT NEWID(),
                company_name NVARCHAR(100) NOT NULL UNIQUE,
                company_code NVARCHAR(20) NOT NULL UNIQUE,
                contact_email NVARCHAR(100),
                subscription_plan NVARCHAR(20) DEFAULT 'starter',
                is_active BIT DEFAULT 1,
                created_at DATETIME2 DEFAULT GETDATE()
            )
        `);
        
        await pool.request().query(`
            IF NOT EXISTS (SELECT * FROM sysobjects WHERE name='system_admins' AND xtype='U')
            CREATE TABLE system_admins (
                id UNIQUEIDENTIFIER PRIMARY KEY DEFAULT NEWID(),
                username NVARCHAR(50) UNIQUE NOT NULL,
                email NVARCHAR(100) UNIQUE NOT NULL,
                password_hash NVARCHAR(255) NOT NULL,
                full_name NVARCHAR(100),
                is_active BIT DEFAULT 1,
                created_at DATETIME2 DEFAULT GETDATE()
            )
        `);
    }

    /**
     * 💾 SQLite 테이블 생성
     */
    async createSQLiteTables() {
        const db = this.connections.main;
        
        return new Promise((resolve, reject) => {
            db.serialize(() => {
                db.run(`CREATE TABLE IF NOT EXISTS tenants (
                    id TEXT PRIMARY KEY DEFAULT (lower(hex(randomblob(16)))),
                    company_name TEXT NOT NULL UNIQUE,
                    company_code TEXT UNIQUE NOT NULL,
                    contact_email TEXT,
                    subscription_plan TEXT DEFAULT 'starter',
                    is_active INTEGER DEFAULT 1,
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                )`);
                
                db.run(`CREATE TABLE IF NOT EXISTS system_admins (
                    id TEXT PRIMARY KEY DEFAULT (lower(hex(randomblob(16)))),
                    username TEXT UNIQUE NOT NULL,
                    email TEXT UNIQUE NOT NULL,
                    password_hash TEXT NOT NULL,
                    full_name TEXT,
                    is_active INTEGER DEFAULT 1,
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                )`, (err) => {
                    if (err) reject(err);
                    else resolve();
                });
            });
        });
    }

    /**
     * 🔶 MariaDB 테이블 생성
     */
    async createMariaDBTables() {
        const pool = this.connections.main;
        const conn = await pool.getConnection();
        
        try {
            await conn.query(`
                CREATE TABLE IF NOT EXISTS tenants (
                    id VARCHAR(36) PRIMARY KEY DEFAULT (UUID()),
                    company_name VARCHAR(100) NOT NULL UNIQUE,
                    company_code VARCHAR(20) NOT NULL UNIQUE,
                    contact_email VARCHAR(100),
                    subscription_plan VARCHAR(20) DEFAULT 'starter',
                    is_active BOOLEAN DEFAULT true,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            `);
            
            await conn.query(`
                CREATE TABLE IF NOT EXISTS system_admins (
                    id VARCHAR(36) PRIMARY KEY DEFAULT (UUID()),
                    username VARCHAR(50) UNIQUE NOT NULL,
                    email VARCHAR(100) UNIQUE NOT NULL,
                    password_hash VARCHAR(255) NOT NULL,
                    full_name VARCHAR(100),
                    is_active BOOLEAN DEFAULT true,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            `);
        } finally {
            conn.release();
        }
    }

    /**
     * 📊 샘플 데이터 생성
     */
    async initializeSampleData() {
        const sampleTenant = {
            company_name: 'Demo Company',
            company_code: 'demo',
            contact_email: 'demo@example.com',
            subscription_plan: 'professional'
        };
        
        switch (this.dbType) {
            case 'postgresql':
            case 'mariadb':
                await this.connections.main.query(`
                    INSERT INTO tenants (company_name, company_code, contact_email, subscription_plan)
                    VALUES (?, ?, ?, ?)
                    ON DUPLICATE KEY UPDATE company_name = company_name
                `, [sampleTenant.company_name, sampleTenant.company_code, 
                    sampleTenant.contact_email, sampleTenant.subscription_plan]);
                break;
                
            case 'mssql':
                await this.connections.main.request().query(`
                    IF NOT EXISTS (SELECT 1 FROM tenants WHERE company_code = 'demo')
                    INSERT INTO tenants (company_name, company_code, contact_email, subscription_plan)
                    VALUES ('${sampleTenant.company_name}', '${sampleTenant.company_code}', 
                           '${sampleTenant.contact_email}', '${sampleTenant.subscription_plan}')
                `);
                break;
                
            case 'sqlite':
                return new Promise((resolve, reject) => {
                    this.connections.main.run(`
                        INSERT OR IGNORE INTO tenants (company_name, company_code, contact_email, subscription_plan)
                        VALUES (?, ?, ?, ?)
                    `, [sampleTenant.company_name, sampleTenant.company_code, 
                        sampleTenant.contact_email, sampleTenant.subscription_plan], (err) => {
                        if (err) reject(err);
                        else resolve();
                    });
                });
        }
        
        this.initStatus.sampleData = true;
    }

    /**
     * 🔴 Redis 초기화
     */
    async initializeRedis() {
        if (!this.connections.redis) {
            console.log('   ⏭️  Redis 연결이 없습니다. 건너뜁니다.');
            return;
        }
        
        await this.connections.redis.set('pulseone:init:timestamp', new Date().toISOString());
        await this.connections.redis.set('pulseone:init:version', '2.0');
        
        this.initStatus.redis = true;
    }

    /**
     * 💾 백업 생성 (간단 버전)
     */
    async createBackup() {
        const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
        const backupDir = path.join(process.env.BACKUP_PATH || './backups/', `backup_${timestamp}`);
        
        try {
            await fs.mkdir(backupDir, { recursive: true });
            
            const metadata = {
                timestamp: new Date().toISOString(),
                dbType: this.dbType,
                status: this.initStatus
            };
            
            await fs.writeFile(
                path.join(backupDir, 'backup_metadata.json'), 
                JSON.stringify(metadata, null, 2)
            );
            
            console.log(`💾 백업이 생성되었습니다: ${backupDir}`);
        } catch (error) {
            console.error('❌ 백업 중 오류:', error.message);
        }
    }

    /**
     * 📋 상세 상태 표시
     */
    async showDetailedStatus() {
        console.log('\n📋 데이터베이스 상태 상세 정보:\n');
        
        console.log(`${this.getDBIcon()} ${this.getDBName()}:`);
        console.log(`   - 시스템 테이블: ${this.initStatus.systemTables ? '✅ 존재' : '❌ 없음'}`);
        console.log(`   - 샘플 데이터: ${this.initStatus.sampleData ? '✅ 존재' : '❌ 없음'}`);
        
        if (process.env.USE_REDIS !== 'false') {
            console.log('\n🔴 Redis:');
            console.log(`   - 초기화 상태: ${this.initStatus.redis ? '✅ 완료' : '❌ 필요'}`);
        }
        
        console.log('\n🎯 전체 상태:');
        console.log(`   - 완전 초기화: ${this.isFullyInitialized() ? '✅ 완료' : '❌ 필요'}`);
        
        console.log('\n');
        await this.showInteractiveMenu();
    }

    /**
     * 🔧 선택적 초기화
     */
    async performSelectiveInitialization() {
        console.log('\n🔧 선택적 초기화:\n');
        console.log('1. 시스템 테이블만');
        console.log('2. 샘플 데이터만');
        console.log('3. Redis만');
        console.log('4. 뒤로 가기\n');
        
        const choice = await this.askQuestion('선택하세요 (1-4): ');
        
        try {
            switch (choice.trim()) {
                case '1':
                    await this.initializeSystemTables();
                    break;
                case '2':
                    await this.initializeSampleData();
                    break;
                case '3':
                    await this.initializeRedis();
                    break;
                case '4':
                    await this.showInteractiveMenu();
                    return;
                default:
                    console.log('❌ 잘못된 선택입니다.');
                    return;
            }
            
            console.log('✅ 선택된 초기화가 완료되었습니다!\n');
            
        } catch (error) {
            console.error('❌ 초기화 중 오류:', error.message);
        }
        
        await this.showInteractiveMenu();
    }

    /**
     * 🚀 전체 초기화 수행
     */
    async performFullInitialization() {
        console.log('\n🚀 전체 초기화를 시작합니다...\n');
        
        // 백업 생성 (기존 데이터가 있는 경우)
        if (this.isFullyInitialized()) {
            console.log('💾 기존 데이터 백업 중...');
            await this.createBackup();
        }
        
        await this.performInitialization();
    }

    /**
     * 🗑️ 데이터베이스 재설정 (간단 버전)
     */
    async resetDatabase() {
        console.log('\n⚠️  이 기능은 안전을 위해 인터랙티브 메뉴에서만 제공됩니다.');
        console.log('수동으로 테이블을 삭제한 후 다시 초기화를 실행하세요.\n');
        
        await this.showInteractiveMenu();
    }

    /**
     * 🔄 리소스 정리
     */
    async cleanup() {
        try {
            if (this.connections.main) {
                switch (this.dbType) {
                    case 'postgresql':
                        await this.connections.main.end();
                        break;
                    case 'mssql':
                        await this.connections.main.close();
                        break;
                    case 'sqlite':
                        this.connections.main.close();
                        break;
                    case 'mariadb':
                        await this.connections.main.end();
                        break;
                }
            }
            
            if (this.connections.redis) {
                await this.connections.redis.disconnect();
            }
        } catch (error) {
            // 정리 중 오류는 무시
        }
    }

    /**
     * ❓ 사용자 입력 받기
     */
    askQuestion(question) {
        return new Promise((resolve) => {
            this.rl.question(question, resolve);
        });
    }
}

// 🚀 메인 실행
if (require.main === module) {
    const initializer = new DatabaseInitializer();
    initializer.run().catch(console.error);
}

module.exports = DatabaseInitializer;