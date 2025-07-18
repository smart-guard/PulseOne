#!/usr/bin/env node

/**
 * 🎯 PulseOne Smart Database Initializer
 * 엔지니어가 DB 종류만 선택하면 모든 것이 자동으로 설정되는 시스템
 */

const fs = require('fs').promises;
const path = require('path');
const readline = require('readline');

class SmartDatabaseInitializer {
    constructor() {
        this.rl = readline.createInterface({
            input: process.stdin,
            output: process.stdout
        });
        
        // 지원하는 DB 타입과 자동 설정
        this.dbTypes = {
            'postgresql': {
                name: 'PostgreSQL',
                description: '🏢 대규모/엔터프라이즈용 (권장)',
                icon: '🐘',
                useCases: ['많은 사용자', '복잡한 쿼리', '높은 안정성 필요'],
                autoConfig: {
                    libraries: ['pg'],
                    defaultPort: 5432,
                    containerName: 'pulseone-postgres',
                    requiresDocker: true
                }
            },
            'mssql': {
                name: 'Microsoft SQL Server',
                description: '🏭 기업/윈도우 환경용',
                icon: '🔵',
                useCases: ['윈도우 서버', 'Microsoft 생태계', '.NET 연동'],
                autoConfig: {
                    libraries: ['mssql'],
                    defaultPort: 1433,
                    containerName: 'pulseone-mssql',
                    requiresDocker: true
                }
            },
            'sqlite': {
                name: 'SQLite',
                description: '📱 소규모/개발용 (가장 간단)',
                icon: '💾',
                useCases: ['개발/테스트', '소규모 현장', '오프라인 환경'],
                autoConfig: {
                    libraries: ['sqlite3'],
                    defaultPath: './data/pulseone.db',
                    requiresDocker: false
                }
            },
            'mariadb': {
                name: 'MariaDB',
                description: '🔄 중간 규모용 (MySQL 호환)',
                icon: '🔶',
                useCases: ['중간 규모', 'MySQL 경험자', '비용 절약'],
                autoConfig: {
                    libraries: ['mariadb'],
                    defaultPort: 3306,
                    containerName: 'pulseone-mariadb',
                    requiresDocker: true
                }
            }
        };
    }

    /**
     * 🚀 메인 실행 함수
     */
    async run() {
        try {
            console.log('\n🎯 PulseOne 스마트 데이터베이스 설정');
            console.log('================================\n');
            
            // 현재 설정 확인
            const currentDb = await this.detectCurrentDatabase();
            
            if (currentDb) {
                console.log(`📊 현재 설정: ${this.dbTypes[currentDb].icon} ${this.dbTypes[currentDb].name}\n`);
                
                const changeDb = await this.askQuestion('다른 데이터베이스로 변경하시겠습니까? (y/n): ');
                if (changeDb.toLowerCase() !== 'y') {
                    console.log('✅ 기존 설정을 유지합니다.');
                    await this.initializeWithCurrentDb(currentDb);
                    return;
                }
            }
            
            // DB 선택 메뉴 표시
            const selectedDb = await this.showDatabaseSelectionMenu();
            
            // 자동 설정 및 초기화
            await this.autoConfigureAndInitialize(selectedDb);
            
        } catch (error) {
            console.error('❌ 설정 중 오류 발생:', error.message);
            process.exit(1);
        } finally {
            this.rl.close();
        }
    }

    /**
     * 🔍 현재 DB 설정 감지
     */
    async detectCurrentDatabase() {
        try {
            const envPath = path.join(__dirname, '../../config/.env.dev');
            const envContent = await fs.readFile(envPath, 'utf8');
            
            if (envContent.includes('DB_TYPE=postgresql')) return 'postgresql';
            if (envContent.includes('DB_TYPE=mssql')) return 'mssql';
            if (envContent.includes('DB_TYPE=sqlite')) return 'sqlite';
            if (envContent.includes('DB_TYPE=mariadb')) return 'mariadb';
            
            return null;
        } catch (error) {
            return null;
        }
    }

    /**
     * 🎛️ DB 선택 메뉴 표시
     */
    async showDatabaseSelectionMenu() {
        console.log('🗄️  사용할 데이터베이스를 선택하세요:\n');
        
        const dbKeys = Object.keys(this.dbTypes);
        
        dbKeys.forEach((key, index) => {
            const db = this.dbTypes[key];
            console.log(`${index + 1}. ${db.icon} ${db.name}`);
            console.log(`   ${db.description}`);
            console.log(`   사용 케이스: ${db.useCases.join(', ')}`);
            console.log('');
        });
        
        const choice = await this.askQuestion(`선택하세요 (1-${dbKeys.length}): `);
        const choiceIndex = parseInt(choice) - 1;
        
        if (choiceIndex < 0 || choiceIndex >= dbKeys.length) {
            console.log('❌ 잘못된 선택입니다.');
            return this.showDatabaseSelectionMenu();
        }
        
        const selectedDb = dbKeys[choiceIndex];
        console.log(`\n✅ ${this.dbTypes[selectedDb].icon} ${this.dbTypes[selectedDb].name}을(를) 선택했습니다.\n`);
        
        return selectedDb;
    }

    /**
     * ⚙️ 자동 설정 및 초기화
     */
    async autoConfigureAndInitialize(dbType) {
        const db = this.dbTypes[dbType];
        
        console.log('🔧 자동 설정을 시작합니다...\n');
        
        // 1. 환경 설정 파일 생성
        console.log('📝 1/5: 환경 설정 파일 생성 중...');
        await this.createEnvironmentConfig(dbType);
        console.log('   ✅ 완료\n');
        
        // 2. 필요한 디렉토리 생성
        console.log('📁 2/5: 디렉토리 구조 생성 중...');
        await this.createDirectories(dbType);
        console.log('   ✅ 완료\n');
        
        // 3. Docker 설정 (필요한 경우)
        if (db.autoConfig.requiresDocker) {
            console.log('🐳 3/5: Docker 환경 설정 중...');
            await this.setupDockerEnvironment(dbType);
            console.log('   ✅ 완료\n');
        } else {
            console.log('⏭️  3/5: Docker 설정 건너뜀 (SQLite는 불필요)\n');
        }
        
        // 4. 데이터베이스 초기화
        console.log('🗄️  4/5: 데이터베이스 초기화 중...');
        await this.initializeDatabase(dbType);
        console.log('   ✅ 완료\n');
        
        // 5. 검증 및 완료
        console.log('🔍 5/5: 설정 검증 중...');
        await this.validateSetup(dbType);
        console.log('   ✅ 완료\n');
        
        this.showCompletionMessage(dbType);
    }

    /**
     * 📝 환경 설정 파일 자동 생성
     */
    async createEnvironmentConfig(dbType) {
        const config = this.generateEnvironmentConfig(dbType);
        const envPath = path.join(__dirname, '../../config/.env.dev');
        
        // 기존 설정 백업
        try {
            await fs.copyFile(envPath, `${envPath}.backup.${Date.now()}`);
        } catch (error) {
            // 파일이 없으면 무시
        }
        
        await fs.writeFile(envPath, config);
    }

    /**
     * 🏗️ DB별 환경 설정 생성
     */
    generateEnvironmentConfig(dbType) {
        const baseConfig = `# ===============================================================================
# PulseOne Auto-Generated Configuration
# Generated: ${new Date().toISOString()}
# Database: ${this.dbTypes[dbType].name}
# ===============================================================================

# Database Type
DB_TYPE=${dbType}

# Application
NODE_ENV=development
BACKEND_PORT=3000
LOG_LEVEL=info

# Auto-Initialization
AUTO_INITIALIZE_ON_START=true
SKIP_IF_INITIALIZED=true
CREATE_SAMPLE_DATA=true
CREATE_DEMO_TENANT=true

`;

        switch (dbType) {
            case 'postgresql':
                return baseConfig + `
# PostgreSQL Configuration (Docker)
POSTGRES_MAIN_DB_HOST=pulseone-postgres
POSTGRES_MAIN_DB_PORT=5432
POSTGRES_MAIN_DB_USER=user
POSTGRES_MAIN_DB_PASSWORD=password
POSTGRES_MAIN_DB_NAME=pulseone

POSTGRES_LOG_DB_HOST=pulseone-postgres
POSTGRES_LOG_DB_PORT=5432
POSTGRES_LOG_DB_USER=user
POSTGRES_LOG_DB_PASSWORD=password
POSTGRES_LOG_DB_NAME=pulseone_logs

# Redis (Optional - for better performance)
REDIS_MAIN_HOST=pulseone-redis
REDIS_MAIN_PORT=6379
REDIS_MAIN_PASSWORD=""

# InfluxDB (Optional - for time-series data)
INFLUXDB_HOST=pulseone-influx
INFLUXDB_PORT=8086
INFLUXDB_TOKEN=mytoken
INFLUXDB_ORG=pulseone
INFLUXDB_BUCKET=metrics
`;

            case 'mssql':
                return baseConfig + `
# Microsoft SQL Server Configuration (Docker)
MSSQL_HOST=pulseone-mssql
MSSQL_PORT=1433
MSSQL_USER=sa
MSSQL_PASSWORD=YourStrong@Password123
MSSQL_DATABASE=pulseone
MSSQL_ENCRYPT=false
MSSQL_TRUST_SERVER_CERTIFICATE=true

# MSSQL Log Database (same server, different database)
MSSQL_LOG_DATABASE=pulseone_logs

# Redis (Optional - for better performance)
REDIS_MAIN_HOST=pulseone-redis
REDIS_MAIN_PORT=6379
REDIS_MAIN_PASSWORD=""

# InfluxDB (Optional - for time-series data)
INFLUXDB_HOST=pulseone-influx
INFLUXDB_PORT=8086
INFLUXDB_TOKEN=mytoken
INFLUXDB_ORG=pulseone
INFLUXDB_BUCKET=metrics
`;

            case 'sqlite':
                return baseConfig + `
# SQLite Configuration (No Docker needed!)
SQLITE_DB_PATH=./data/pulseone.db
SQLITE_LOG_DB_PATH=./data/pulseone_logs.db

# SQLite Options
SQLITE_WAL_MODE=true
SQLITE_BUSY_TIMEOUT=30000

# Optional Redis (for better performance)
USE_REDIS=false
# REDIS_MAIN_HOST=localhost
# REDIS_MAIN_PORT=6379

# Optional InfluxDB (for time-series data)
USE_INFLUXDB=false
`;

            case 'mariadb':
                return baseConfig + `
# MariaDB Configuration (Docker)
MARIADB_HOST=pulseone-mariadb
MARIADB_PORT=3306
MARIADB_USER=user
MARIADB_PASSWORD=password
MARIADB_DATABASE=pulseone

# Redis (Optional - for better performance)
REDIS_MAIN_HOST=pulseone-redis
REDIS_MAIN_PORT=6379
REDIS_MAIN_PASSWORD=""
`;

            default:
                return baseConfig;
        }
    }

    /**
     * 📁 필요한 디렉토리 생성
     */
    async createDirectories(dbType) {
        const directories = [
            './data',
            './logs',
            './backups'
        ];
        
        for (const dir of directories) {
            try {
                await fs.mkdir(path.join(__dirname, '../..', dir), { recursive: true });
            } catch (error) {
                // 이미 존재하면 무시
            }
        }
    }

    /**
     * 🐳 Docker 환경 설정
     */
    async setupDockerEnvironment(dbType) {
        const dockerCompose = this.generateDockerCompose(dbType);
        const dockerPath = path.join(__dirname, '../../docker-compose.dev.yml');
        
        await fs.writeFile(dockerPath, dockerCompose);
        
        console.log('   🐳 Docker Compose 파일이 생성되었습니다.');
        console.log('   💡 실행하려면: docker-compose -f docker-compose.dev.yml up -d');
    }

    /**
     * 🐳 Docker Compose 파일 생성
     */
    generateDockerCompose(dbType) {
        const baseServices = {
            redis: `
  pulseone-redis:
    image: redis:7-alpine
    container_name: pulseone-redis
    ports:
      - "6379:6379"
    command: redis-server --appendonly yes
    volumes:
      - redis_data:/data
    restart: unless-stopped
`,
            influxdb: `
  pulseone-influx:
    image: influxdb:2.0
    container_name: pulseone-influx
    ports:
      - "8086:8086"
    environment:
      - INFLUXDB_DB=pulseone
      - INFLUXDB_ADMIN_USER=admin
      - INFLUXDB_ADMIN_PASSWORD=password
    volumes:
      - influx_data:/var/lib/influxdb2
    restart: unless-stopped
`
        };

        let dbService = '';
        
        switch (dbType) {
            case 'postgresql':
                dbService = `
  pulseone-postgres:
    image: postgres:15-alpine
    container_name: pulseone-postgres
    ports:
      - "5432:5432"
    environment:
      - POSTGRES_USER=user
      - POSTGRES_PASSWORD=password
      - POSTGRES_DB=pulseone
    volumes:
      - postgres_data:/var/lib/postgresql/data
    restart: unless-stopped
`;
                break;
                
            case 'mssql':
                dbService = `
  pulseone-mssql:
    image: mcr.microsoft.com/mssql/server:2022-latest
    container_name: pulseone-mssql
    ports:
      - "1433:1433"
    environment:
      - ACCEPT_EULA=Y
      - SA_PASSWORD=YourStrong@Password123
      - MSSQL_PID=Express
    volumes:
      - mssql_data:/var/opt/mssql
    restart: unless-stopped
`;
                break;
                
            case 'mariadb':
                dbService = `
  pulseone-mariadb:
    image: mariadb:10.11
    container_name: pulseone-mariadb
    ports:
      - "3306:3306"
    environment:
      - MARIADB_ROOT_PASSWORD=rootpassword
      - MARIADB_DATABASE=pulseone
      - MARIADB_USER=user
      - MARIADB_PASSWORD=password
    volumes:
      - mariadb_data:/var/lib/mysql
    restart: unless-stopped
`;
                break;
        }

        const volumes = `
volumes:
  ${dbType === 'postgresql' ? 'postgres_data:' : ''}
  ${dbType === 'mssql' ? 'mssql_data:' : ''}
  ${dbType === 'mariadb' ? 'mariadb_data:' : ''}
  redis_data:
  influx_data:
`;

        return `version: '3.8'

services:${dbService}${baseServices.redis}${baseServices.influxdb}
${volumes}`;
    }

    /**
     * 🗄️ 데이터베이스 초기화
     */
    async initializeDatabase(dbType) {
        // 환경 변수 다시 로드
        require('dotenv').config({ path: path.join(__dirname, '../../config/.env.dev') });
        
        const InitializerClass = this.getInitializerClass(dbType);
        const initializer = new InitializerClass();
        
        await initializer.performInitialization();
    }

    /**
     * 🔧 DB별 초기화 클래스 반환
     */
    getInitializerClass(dbType) {
        // 모든 DB 타입에 대해 통합된 database-initializer 사용
        return require('./database-initializer');
    }

    /**
     * ✅ 설정 검증
     */
    async validateSetup(dbType) {
        // 간단한 검증 (파일 존재 확인 등)
        const envPath = path.join(__dirname, '../../config/.env.dev');
        
        try {
            await fs.access(envPath);
            console.log('   📝 환경 설정 파일 확인됨');
        } catch (error) {
            throw new Error('환경 설정 파일을 찾을 수 없습니다.');
        }
        
        if (dbType === 'sqlite') {
            const dataDir = path.join(__dirname, '../../data');
            try {
                await fs.access(dataDir);
                console.log('   📁 데이터 디렉토리 확인됨');
            } catch (error) {
                throw new Error('데이터 디렉토리를 찾을 수 없습니다.');
            }
        }
    }

    /**
     * 🎉 완료 메시지 표시
     */
    showCompletionMessage(dbType) {
        const db = this.dbTypes[dbType];
        
        console.log('🎉 설정이 완료되었습니다!\n');
        console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
        console.log(`📊 선택된 데이터베이스: ${db.icon} ${db.name}`);
        console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
        
        if (db.autoConfig.requiresDocker) {
            console.log('🐳 다음 단계:');
            console.log('   1. Docker 컨테이너 시작:');
            console.log('      docker-compose -f docker-compose.dev.yml up -d');
            console.log('');
            console.log('   2. 서버 시작:');
            console.log('      npm run start:auto');
        } else {
            console.log('🚀 다음 단계:');
            console.log('   서버 시작: npm run start:auto');
        }
        
        console.log('');
        console.log('💡 팁:');
        console.log('   - 상태 확인: npm run db:status');
        console.log('   - 설정 변경: npm run db:init');
        console.log('   - 로그 확인: tail -f logs/pulseone.log');
        console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n');
    }

    /**
     * 🔄 기존 DB로 초기화
     */
    async initializeWithCurrentDb(dbType) {
        console.log('🔄 기존 설정으로 초기화를 진행합니다...\n');
        
        await this.initializeDatabase(dbType);
        
        console.log('✅ 기존 설정으로 초기화가 완료되었습니다!');
        console.log('🚀 서버 시작: npm run start:auto\n');
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

// SQLite 초기화 클래스 (간단 버전)
class SQLiteInitializer {
    async performInitialization() {
        console.log('   📱 SQLite 데이터베이스 초기화...');
        
        // SQLite 파일 생성 및 기본 테이블 생성
        const sqlite3 = require('sqlite3').verbose();
        const dbPath = process.env.SQLITE_DB_PATH || './data/pulseone.db';
        
        return new Promise((resolve, reject) => {
            const db = new sqlite3.Database(dbPath, (err) => {
                if (err) {
                    reject(err);
                    return;
                }
                
                // 기본 테이블 생성
                db.serialize(() => {
                    db.run(`CREATE TABLE IF NOT EXISTS tenants (
                        id TEXT PRIMARY KEY,
                        company_name TEXT NOT NULL,
                        company_code TEXT UNIQUE NOT NULL,
                        contact_email TEXT,
                        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                    )`);
                    
                    db.run(`CREATE TABLE IF NOT EXISTS users (
                        id TEXT PRIMARY KEY,
                        username TEXT UNIQUE NOT NULL,
                        email TEXT UNIQUE NOT NULL,
                        full_name TEXT,
                        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                    )`);
                    
                    // 샘플 데이터 삽입
                    db.run(`INSERT OR IGNORE INTO tenants (id, company_name, company_code, contact_email) 
                            VALUES ('demo-id', 'Demo Company', 'demo', 'demo@example.com')`);
                });
                
                db.close((err) => {
                    if (err) reject(err);
                    else resolve();
                });
            });
        });
    }
}

// 🚀 메인 실행
if (require.main === module) {
    const initializer = new SmartDatabaseInitializer();
    initializer.run().catch(console.error);
}

module.exports = SmartDatabaseInitializer;