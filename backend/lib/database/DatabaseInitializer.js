const { DatabaseAbstractionLayer } = require('./DatabaseAbstractionLayer');
const fs = require('fs').promises;
const path = require('path');

class DatabaseInitializer {
    constructor(connections = null) {
        this.dbLayer = new DatabaseAbstractionLayer(connections);
        this.initStatus = {
            systemTables: false,
            tenantSchemas: false, 
            sampleData: false,
            indexesCreated: false
        };
        this.databaseType = process.env.DATABASE_TYPE || 'SQLITE';
        this.schemasPath = path.join(__dirname, 'schemas');
        
        console.log(`🔧 DatabaseInitializer: ${this.databaseType} 모드로 초기화`);
    }

    setConnections(connections) {
        this.dbLayer.setConnections(connections);
        console.log('✅ DatabaseInitializer connections 설정됨');
    }

    /**
     * SQL 파일 로드 및 실행
     */
    async executeSQLFile(filename) {
        try {
            const filePath = path.join(this.schemasPath, filename);
            const sqlContent = await fs.readFile(filePath, 'utf8');
            
            // SQL 파일을 개별 명령으로 분할
            const statements = sqlContent
                .split(';')
                .map(stmt => stmt.trim())
                .filter(stmt => stmt.length > 0 && !stmt.startsWith('--'));
            
            console.log(`  📁 ${filename}: ${statements.length}개 SQL 명령 실행 중...`);
            
            for (const statement of statements) {
                try {
                    // DatabaseAbstractionLayer를 통해 DB별 호환 쿼리로 변환 후 실행
                    await this.dbLayer.executeCreateTable(statement);
                } catch (error) {
                    console.log(`    ⚠️ SQL 실행 실패: ${error.message}`);
                }
            }
            
            console.log(`  ✅ ${filename} 실행 완료`);
            return true;
            
        } catch (error) {
            console.error(`❌ SQL 파일 실행 실패 (${filename}):`, error.message);
            return false;
        }
    }

    async createSystemTables() {
        try {
            // SQL 파일들을 순서대로 실행
            const sqlFiles = [
                '01-core-tables.sql',
                '02-users-sites.sql',
                '03-devices-datapoints.sql'
            ];
            
            for (const sqlFile of sqlFiles) {
                const filePath = path.join(this.schemasPath, sqlFile);
                
                // 파일 존재 확인
                try {
                    await fs.access(filePath);
                    await this.executeSQLFile(sqlFile);
                } catch (error) {
                    console.log(`  ⚠️ ${sqlFile} 파일 없음, 스킵`);
                }
            }
            
        } catch (error) {
            console.error('시스템 테이블 생성 실패:', error.message);
            throw error;
        }
    }

    async createExtendedTables() {
        try {
            const extendedSqlFiles = [
                '04-virtual-points.sql',
                '05-alarm-rules.sql', 
                '06-script-library.sql'
            ];
            
            for (const sqlFile of extendedSqlFiles) {
                const filePath = path.join(this.schemasPath, sqlFile);
                
                try {
                    await fs.access(filePath);
                    await this.executeSQLFile(sqlFile);
                } catch (error) {
                    console.log(`  ⚠️ ${sqlFile} 파일 없음, 스킵`);
                }
            }
            
        } catch (error) {
            console.error('확장 테이블 생성 실패:', error.message);
            throw error;
        }
    }

    // 나머지 메서드들은 기존과 동일
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
            throw error;
        }
    }

    async performInitialization() {
        try {
            console.log('🚀 데이터베이스 초기화 시작 (SQL 파일 기반)...\n');
            
            if (!this.initStatus.systemTables) {
                console.log('📋 [1/4] 시스템 테이블 생성 중...');
                await this.createSystemTables();
                this.initStatus.systemTables = true;
                console.log('✅ 시스템 테이블 생성 완료\n');
            }
            
            if (!this.initStatus.tenantSchemas) {
                console.log('🏢 [2/4] 확장 테이블 생성 중...');
                await this.createExtendedTables();
                this.initStatus.tenantSchemas = true;
                console.log('✅ 확장 테이블 생성 완료\n');
            }
            
            // 인덱스 및 데이터 생성은 기존과 동일
            if (!this.initStatus.indexesCreated) {
                console.log('⚡ [3/4] 인덱스 생성 중...');
                await this.createIndexes();
                this.initStatus.indexesCreated = true;
                console.log('✅ 인덱스 생성 완료\n');
            }
            
            if (!this.initStatus.sampleData) {
                console.log('📊 [4/4] 기본 데이터 생성 중...');
                await this.createSampleData();
                this.initStatus.sampleData = true;
                console.log('✅ 기본 데이터 생성 완료\n');
            }
            
            console.log('🎉 데이터베이스 초기화가 완전히 완료되었습니다!');
            
        } catch (error) {
            console.error('❌ 데이터베이스 초기화 실패:', error.message);
            throw error;
        }
    }

    isFullyInitialized() {
        return this.initStatus.systemTables && 
               this.initStatus.tenantSchemas && 
               this.initStatus.sampleData && 
               this.initStatus.indexesCreated;
    }

    // 기존 check 메서드들과 동일
    async checkSystemTables() {
        try {
            const systemTables = ['tenants', 'users', 'sites', 'devices', 'datapoints'];
            let foundTables = 0;
            
            for (const tableName of systemTables) {
                const exists = await this.dbLayer.doesTableExist(tableName);
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
            const extendedTables = ['virtual_points', 'alarm_rules', 'script_library'];
            let foundTables = 0;
            
            for (const tableName of extendedTables) {
                const exists = await this.dbLayer.doesTableExist(tableName);
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
            const result = await this.dbLayer.executeQuery('SELECT COUNT(*) as count FROM tenants');
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

    async createIndexes() {
        try {
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
                    await this.dbLayer.executeNonQuery(indexSQL);
                    console.log(`  ✅ 인덱스 생성 완료`);
                } catch (error) {
                    console.log(`  ⚠️ 인덱스 생성 실패: ${error.message}`);
                }
            }
            
        } catch (error) {
            console.error('인덱스 생성 실패:', error.message);
        }
    }

    async createSampleData() {
        try {
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

    async createBackup(force = false) {
        try {
            if (this.databaseType.toUpperCase() === 'SQLITE') {
                const backupDir = path.join(process.cwd(), 'data', 'backup');
                await fs.mkdir(backupDir, { recursive: true });
                
                const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
                const backupPath = path.join(backupDir, `pulseone_backup_${timestamp}.db`);
                
                const originalPath = process.env.SQLITE_PATH || process.env.SQLITE_DB_PATH || './data/db/pulseone.db';
                
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
}

module.exports = DatabaseInitializer;