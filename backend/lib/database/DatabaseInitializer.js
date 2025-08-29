// =============================================================================
// backend/lib/database/DatabaseInitializer.js
// PulseOne 데이터베이스 자동 초기화 시스템 - device_id INTEGER + cleared_by 완성본
// =============================================================================

const path = require('path');
const fs = require('fs').promises;
const { existsSync } = require('fs');
const sqlite3 = require('sqlite3').verbose();

class DatabaseInitializer {
    constructor() {
        // 초기화 상태 추적
        this.initStatus = {
            systemTables: false,
            tenantSchemas: false,
            sampleData: false,
            indexes: false
        };

        // 경로 설정
        this.projectRoot = path.join(__dirname, '../../../');
        this.dataDir = path.join(this.projectRoot, 'data');
        this.dbPath = path.join(this.dataDir, 'pulseone.db');
        this.schemasDir = path.join(__dirname, 'schemas');
        this.dataFilesDir = path.join(__dirname, 'data');
        
        console.log('DatabaseInitializer 초기화');
        console.log('프로젝트 루트:', this.projectRoot);
        console.log('데이터베이스 경로:', this.dbPath);
        console.log('스키마 파일 경로:', this.schemasDir);
    }

    /**
     * 데이터베이스 상태 확인
     */
    async checkDatabaseStatus() {
        try {
            console.log('데이터베이스 상태 확인 중...');

            // 데이터베이스 파일 존재 확인
            if (!existsSync(this.dbPath)) {
                console.log('데이터베이스 파일이 없습니다. 새로 생성이 필요합니다.');
                return this.initStatus;
            }

            const db = await this.openDatabase();

            // 1. 시스템 테이블 확인
            this.initStatus.systemTables = await this.checkSystemTables(db);
            
            // 2. 테넌트 스키마 확인
            this.initStatus.tenantSchemas = await this.checkTenantSchemas(db);
            
            // 3. 샘플 데이터 확인
            this.initStatus.sampleData = await this.checkSampleData(db);
            
            // 4. 인덱스 확인
            this.initStatus.indexes = await this.checkIndexes(db);

            await this.closeDatabase(db);

            console.log('데이터베이스 상태 확인 완료:', this.initStatus);
            return this.initStatus;

        } catch (error) {
            console.error('데이터베이스 상태 확인 실패:', error.message);
            throw error;
        }
    }

    /**
     * 완전한 초기화 수행
     */
    async performInitialization() {
        try {
            console.log('데이터베이스 초기화 시작...');

            // 데이터 디렉토리 생성
            await this.ensureDataDirectory();

            // 스키마 파일 존재 확인
            await this.checkSchemaFiles();

            // 데이터베이스 연결
            const db = await this.openDatabase();

            // 1. 시스템 테이블 생성 (스키마 파일에서)
            if (!this.initStatus.systemTables) {
                await this.createSystemTablesFromFiles(db);
                this.initStatus.systemTables = true;
                console.log('시스템 테이블 생성 완료');
            }

            // 2. 인덱스 생성 (스키마 파일에서)
            if (!this.initStatus.indexes) {
                await this.createIndexesFromFile(db);
                this.initStatus.indexes = true;
                console.log('인덱스 생성 완료');
            }

            // 3. 초기 데이터 삽입 (데이터 파일에서)
            if (!this.initStatus.sampleData) {
                await this.insertSampleDataFromFile(db);
                this.initStatus.sampleData = true;
                console.log('샘플 데이터 삽입 완료');
            }

            // 4. 테넌트 스키마 완료 표시
            this.initStatus.tenantSchemas = true;

            await this.closeDatabase(db);

            console.log('데이터베이스 초기화 완료!');
            return this.initStatus;

        } catch (error) {
            console.error('데이터베이스 초기화 실패:', error.message);
            throw error;
        }
    }

    /**
     * 백업 생성
     */
    async createBackup(force = false) {
        try {
            if (!existsSync(this.dbPath)) {
                console.log('백업할 데이터베이스가 없습니다.');
                return null;
            }

            const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
            const backupPath = path.join(this.dataDir, `pulseone_backup_${timestamp}.db`);

            await fs.copyFile(this.dbPath, backupPath);
            console.log(`백업 생성 완료: ${backupPath}`);
            
            return backupPath;

        } catch (error) {
            console.error('백업 생성 실패:', error.message);
            if (!force) throw error;
            return null;
        }
    }

    /**
     * 완전 초기화 여부 확인
     */
    isFullyInitialized() {
        return Object.values(this.initStatus).every(status => status === true);
    }

    // =============================================================================
    // 스키마 파일 관련 메소드들
    // =============================================================================

    /**
     * 스키마 파일들 존재 확인
     */
    async checkSchemaFiles() {
        const requiredFiles = [
            '01-core-tables.sql',
            '02-device-tables.sql', 
            '03-alarm-tables.sql',
            '04-indexes.sql'
        ];

        console.log('스키마 파일 확인 중...');

        for (const file of requiredFiles) {
            const filePath = path.join(this.schemasDir, file);
            if (!existsSync(filePath)) {
                console.warn(`스키마 파일 없음: ${file}`);
                // 파일이 없으면 생성
                await this.createMissingSchemaFile(file);
            } else {
                console.log(`${file} 존재 확인`);
            }
        }

        // 데이터 파일 확인
        const dataFile = path.join(this.dataFilesDir, 'initial-data.sql');
        if (!existsSync(dataFile)) {
            console.warn('데이터 파일 없음: initial-data.sql');
            await this.createInitialDataFile();
        } else {
            console.log('initial-data.sql 존재 확인');
        }
    }

    /**
     * 스키마 파일에서 시스템 테이블 생성
     */
    async createSystemTablesFromFiles(db) {
        console.log('스키마 파일에서 시스템 테이블 생성 중...');

        const schemaFiles = [
            '01-core-tables.sql',
            '02-device-tables.sql',
            '03-alarm-tables.sql'
        ];

        for (const filename of schemaFiles) {
            await this.executeSchemaFile(db, filename);
        }

        console.log('시스템 테이블 생성 완료');
    }

    /**
     * 스키마 파일에서 인덱스 생성
     */
    async createIndexesFromFile(db) {
        console.log('스키마 파일에서 인덱스 생성 중...');
        await this.executeSchemaFile(db, '04-indexes.sql');
        console.log('인덱스 생성 완료');
    }

    /**
     * 데이터 파일에서 샘플 데이터 삽입
     */
    async insertSampleDataFromFile(db) {
        console.log('데이터 파일에서 샘플 데이터 삽입 중...');
        await this.executeDataFile(db, 'initial-data.sql');
        console.log('샘플 데이터 삽입 완료');
    }

    /**
     * 스키마 파일 실행
     */
    async executeSchemaFile(db, filename) {
        const filePath = path.join(this.schemasDir, filename);
        
        try {
            console.log(`스키마 파일 실행: ${filename}`);
            const sqlContent = await fs.readFile(filePath, 'utf8');
            await this.executeSQLScript(db, sqlContent);
            console.log(`${filename} 실행 완료`);
        } catch (error) {
            console.error(`${filename} 실행 실패:`, error.message);
            throw error;
        }
    }

    /**
     * 데이터 파일 실행
     */
    async executeDataFile(db, filename) {
        const filePath = path.join(this.dataFilesDir, filename);
        
        try {
            console.log(`데이터 파일 실행: ${filename}`);
            const sqlContent = await fs.readFile(filePath, 'utf8');
            await this.executeSQLScript(db, sqlContent);
            console.log(`${filename} 실행 완료`);
        } catch (error) {
            console.error(`${filename} 실행 실패:`, error.message);
            throw error;
        }
    }

    /**
     * SQL 스크립트 실행 (여러 문장 처리)
     */
    async executeSQLScript(db, sqlContent) {
        // 주석 제거 및 정리
        const cleanedSQL = sqlContent
            .replace(/--.*$/gm, '') // 한줄 주석 제거
            .replace(/\/\*[\s\S]*?\*\//g, '') // 블록 주석 제거
            .replace(/\s+/g, ' ') // 여러 공백을 하나로
            .trim();

        // 세미콜론으로 문장 분리
        const statements = cleanedSQL
            .split(';')
            .map(stmt => stmt.trim())
            .filter(stmt => stmt.length > 0);

        for (const statement of statements) {
            await this.executeSQL(db, statement);
        }
    }

    // =============================================================================
    // 스키마 파일 자동 생성 메소드들 - device_id INTEGER + cleared_by 추가
    // =============================================================================

    /**
     * 누락된 스키마 파일 생성
     */
    async createMissingSchemaFile(filename) {
        const filePath = path.join(this.schemasDir, filename);
        
        // schemas 디렉토리 생성
        if (!existsSync(this.schemasDir)) {
            await fs.mkdir(this.schemasDir, { recursive: true });
        }

        let content = '';

        switch (filename) {
            case '01-core-tables.sql':
                content = this.getCoreTablesSQL();
                break;
            case '02-device-tables.sql':
                content = this.getDeviceTablesSQL();
                break;
            case '03-alarm-tables.sql':
                content = this.getAlarmTablesSQL();
                break;
            case '04-indexes.sql':
                content = this.getIndexesSQL();
                break;
        }

        await fs.writeFile(filePath, content, 'utf8');
        console.log(`스키마 파일 생성: ${filename}`);
    }

    /**
     * 초기 데이터 파일 생성
     */
    async createInitialDataFile() {
        const filePath = path.join(this.dataFilesDir, 'initial-data.sql');
        
        // data 디렉토리 생성
        if (!existsSync(this.dataFilesDir)) {
            await fs.mkdir(this.dataFilesDir, { recursive: true });
        }

        const content = this.getInitialDataSQL();
        await fs.writeFile(filePath, content, 'utf8');
        console.log('데이터 파일 생성: initial-data.sql');
    }

    // =============================================================================
    // SQL 내용 생성 메소드들 - device_id INTEGER + cleared_by 완전 반영
    // =============================================================================

    getCoreTablesSQL() {
        return `-- =============================================================================
-- 01-core-tables.sql - 핵심 시스템 테이블
-- =============================================================================

-- 스키마 버전 관리
CREATE TABLE IF NOT EXISTS schema_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    version VARCHAR(20) NOT NULL,
    applied_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    description TEXT
);

-- 테넌트 테이블
CREATE TABLE IF NOT EXISTS tenants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    company_name VARCHAR(100) NOT NULL UNIQUE,
    company_code VARCHAR(20) NOT NULL UNIQUE,
    domain VARCHAR(100) UNIQUE,
    contact_name VARCHAR(100),
    contact_email VARCHAR(100),
    contact_phone VARCHAR(20),
    subscription_plan VARCHAR(20) DEFAULT 'starter',
    subscription_status VARCHAR(20) DEFAULT 'active',
    max_edge_servers INTEGER DEFAULT 3,
    max_data_points INTEGER DEFAULT 1000,
    max_users INTEGER DEFAULT 5,
    is_active INTEGER DEFAULT 1,
    timezone VARCHAR(50) DEFAULT 'UTC',
    currency VARCHAR(3) DEFAULT 'USD',
    language VARCHAR(5) DEFAULT 'en',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Edge 서버 테이블
CREATE TABLE IF NOT EXISTS edge_servers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    server_name VARCHAR(100) NOT NULL,
    server_code VARCHAR(20) NOT NULL,
    hostname VARCHAR(255),
    ip_address VARCHAR(45),
    port INTEGER DEFAULT 3000,
    status VARCHAR(20) DEFAULT 'offline',
    last_heartbeat DATETIME,
    version VARCHAR(20),
    capabilities TEXT,
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    UNIQUE(tenant_id, server_code)
);

-- 사용자 테이블
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    username VARCHAR(50) NOT NULL,
    email VARCHAR(100) NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(100),
    role VARCHAR(20) DEFAULT 'operator',
    permissions TEXT,
    is_active INTEGER DEFAULT 1,
    last_login DATETIME,
    login_count INTEGER DEFAULT 0,
    password_changed_at DATETIME,
    must_change_password INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    UNIQUE(tenant_id, username),
    UNIQUE(tenant_id, email)
);

-- 사용자 세션 테이블
CREATE TABLE IF NOT EXISTS user_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    session_token VARCHAR(255) NOT NULL UNIQUE,
    refresh_token VARCHAR(255),
    ip_address VARCHAR(45),
    user_agent TEXT,
    expires_at DATETIME NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_accessed DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_active INTEGER DEFAULT 1,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- 사이트 테이블
CREATE TABLE IF NOT EXISTS sites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    edge_server_id INTEGER,
    site_name VARCHAR(100) NOT NULL,
    site_code VARCHAR(20) NOT NULL,
    description TEXT,
    location_address TEXT,
    location_city VARCHAR(100),
    location_state VARCHAR(100),
    location_country VARCHAR(100),
    location_postal_code VARCHAR(20),
    location_coordinates VARCHAR(50),
    timezone VARCHAR(50) DEFAULT 'UTC',
    contact_name VARCHAR(100),
    contact_phone VARCHAR(20),
    contact_email VARCHAR(100),
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, site_code)
);

-- 디바이스 그룹 테이블
CREATE TABLE IF NOT EXISTS device_groups (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER,
    group_name VARCHAR(100) NOT NULL,
    group_type VARCHAR(50) DEFAULT 'functional',
    description TEXT,
    parent_group_id INTEGER,
    sort_order INTEGER DEFAULT 0,
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (parent_group_id) REFERENCES device_groups(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, site_id, group_name)
);`;
    }

    getDeviceTablesSQL() {
        return `-- =============================================================================
-- 02-device-tables.sql - 디바이스 관련 테이블
-- =============================================================================

-- 디바이스 테이블
CREATE TABLE IF NOT EXISTS devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER,
    device_group_id INTEGER,
    edge_server_id INTEGER,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    device_type VARCHAR(50) NOT NULL,
    manufacturer VARCHAR(100),
    model VARCHAR(100),
    serial_number VARCHAR(100),
    protocol_type VARCHAR(50) NOT NULL,
    endpoint VARCHAR(255) NOT NULL,
    config TEXT,
    polling_interval INTEGER DEFAULT 1000,
    timeout INTEGER DEFAULT 3000,
    retry_count INTEGER DEFAULT 3,
    is_enabled INTEGER DEFAULT 1,
    installation_date DATE,
    last_maintenance DATE,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE SET NULL,
    FOREIGN KEY (device_group_id) REFERENCES device_groups(id) ON DELETE SET NULL,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, name)
);

-- 디바이스 상태 테이블
CREATE TABLE IF NOT EXISTS device_status (
    device_id INTEGER PRIMARY KEY,
    connection_status VARCHAR(20) DEFAULT 'disconnected',
    last_communication DATETIME,
    error_count INTEGER DEFAULT 0,
    last_error TEXT,
    response_time INTEGER,
    firmware_version VARCHAR(50),
    hardware_info TEXT,
    diagnostic_data TEXT,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- 디바이스 설정 테이블
CREATE TABLE IF NOT EXISTS device_settings (
    device_id INTEGER PRIMARY KEY,
    polling_interval_ms INTEGER DEFAULT 1000,
    connection_timeout_ms INTEGER DEFAULT 5000,
    max_retry_count INTEGER DEFAULT 3,
    retry_interval_ms INTEGER DEFAULT 5000,
    backoff_time_ms INTEGER DEFAULT 60000,
    keep_alive_enabled INTEGER DEFAULT 1,
    keep_alive_interval_s INTEGER DEFAULT 30,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- 데이터포인트 테이블
CREATE TABLE IF NOT EXISTS data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    device_id INTEGER NOT NULL,
    point_name VARCHAR(100) NOT NULL,
    point_address VARCHAR(100) NOT NULL,
    data_type VARCHAR(20) NOT NULL DEFAULT 'number',
    unit VARCHAR(20),
    scaling_factor REAL DEFAULT 1.0,
    scaling_offset REAL DEFAULT 0.0,
    min_value REAL,
    max_value REAL,
    decimal_places INTEGER DEFAULT 2,
    is_writable INTEGER DEFAULT 0,
    is_enabled INTEGER DEFAULT 1,
    polling_rate INTEGER DEFAULT 1000,
    archive_enabled INTEGER DEFAULT 1,
    archive_interval INTEGER DEFAULT 60000,
    description TEXT,
    tags TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    UNIQUE(device_id, point_address)
);

-- 가상포인트 테이블
CREATE TABLE IF NOT EXISTS virtual_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    scope_type VARCHAR(20) DEFAULT 'tenant',
    site_id INTEGER,
    device_id INTEGER,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    formula TEXT NOT NULL,
    data_type VARCHAR(20) DEFAULT 'float',
    unit VARCHAR(20),
    calculation_interval INTEGER DEFAULT 1000,
    calculation_trigger VARCHAR(20) DEFAULT 'timer',
    is_enabled INTEGER DEFAULT 1,
    category VARCHAR(50) DEFAULT 'calculation',
    tags TEXT,
    execution_type VARCHAR(20) DEFAULT 'javascript',
    cache_duration_ms INTEGER DEFAULT 0,
    error_handling VARCHAR(50) DEFAULT 'return_null',
    last_error TEXT,
    execution_count INTEGER DEFAULT 0,
    avg_execution_time_ms REAL DEFAULT 0,
    last_execution_time DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE SET NULL,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, name)
);

-- 가상포인트 입력값 테이블
CREATE TABLE IF NOT EXISTS virtual_point_inputs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    point_id INTEGER,
    virtual_point_input_id INTEGER,
    alias VARCHAR(100) NOT NULL,
    data_type VARCHAR(20) DEFAULT 'number',
    default_value TEXT,
    is_required INTEGER DEFAULT 1,
    sort_order INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE SET NULL,
    FOREIGN KEY (virtual_point_input_id) REFERENCES virtual_points(id) ON DELETE SET NULL,
    UNIQUE(virtual_point_id, alias)
);`;
    }

    getAlarmTablesSQL() {
        return `-- =============================================================================
-- 03-alarm-tables.sql - 알람 관련 테이블 (device_id INTEGER + cleared_by 완전 반영)
-- =============================================================================

-- 알람 규칙 테이블 - 실제 스키마와 완전 일치
CREATE TABLE IF NOT EXISTS alarm_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    target_type VARCHAR(20) NOT NULL,
    target_id INTEGER,
    target_group VARCHAR(100),
    alarm_type VARCHAR(20) NOT NULL,
    high_high_limit REAL,
    high_limit REAL,
    low_limit REAL,
    low_low_limit REAL,
    deadband REAL DEFAULT 0,
    rate_of_change REAL DEFAULT 0,
    trigger_condition VARCHAR(20),
    condition_script TEXT,
    message_script TEXT,
    message_config TEXT,
    message_template TEXT,
    severity VARCHAR(20) DEFAULT 'medium',
    priority INTEGER DEFAULT 100,
    auto_acknowledge INTEGER DEFAULT 0,
    acknowledge_timeout_min INTEGER DEFAULT 0,
    auto_clear INTEGER DEFAULT 1,
    suppression_rules TEXT,
    notification_enabled INTEGER DEFAULT 1,
    notification_delay_sec INTEGER DEFAULT 0,
    notification_repeat_interval_min INTEGER DEFAULT 0,
    notification_channels TEXT,
    notification_recipients TEXT,
    is_enabled INTEGER DEFAULT 1,
    is_latched INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER, 
    template_id INTEGER, 
    rule_group VARCHAR(36), 
    created_by_template INTEGER DEFAULT 0, 
    last_template_update DATETIME, 
    escalation_enabled INTEGER DEFAULT 0,
    escalation_max_level INTEGER DEFAULT 3,
    escalation_rules TEXT DEFAULT NULL,
    category VARCHAR(50) DEFAULT NULL,
    tags TEXT DEFAULT NULL,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id)
);

-- 알람 발생 테이블 - device_id INTEGER + cleared_by 추가 완성
CREATE TABLE IF NOT EXISTS alarm_occurrences (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_id INTEGER NOT NULL,
    tenant_id INTEGER NOT NULL,
    occurrence_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    trigger_value TEXT,
    trigger_condition TEXT,
    alarm_message TEXT,
    severity VARCHAR(20),
    state VARCHAR(20) DEFAULT 'active',
    acknowledged_time DATETIME,
    acknowledged_by INTEGER,
    acknowledge_comment TEXT,
    cleared_time DATETIME,
    cleared_value TEXT,
    clear_comment TEXT,
    cleared_by INTEGER,                        -- 추가: 알람 해제한 사용자 ID
    notification_sent INTEGER DEFAULT 0,
    notification_time DATETIME,
    notification_count INTEGER DEFAULT 0,
    notification_result TEXT,
    context_data TEXT,
    source_name VARCHAR(100),
    location VARCHAR(200),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    device_id INTEGER,                         -- INTEGER로 수정!
    point_id INTEGER,
    category VARCHAR(50) DEFAULT NULL,
    tags TEXT DEFAULT NULL,
    FOREIGN KEY (rule_id) REFERENCES alarm_rules(id) ON DELETE CASCADE,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (acknowledged_by) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY (cleared_by) REFERENCES users(id) ON DELETE SET NULL
);

-- 알람 규칙 템플릿 테이블
CREATE TABLE IF NOT EXISTS alarm_rule_templates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    category VARCHAR(50),
    condition_type VARCHAR(50) NOT NULL,
    condition_template TEXT NOT NULL,
    default_config TEXT NOT NULL,
    severity VARCHAR(20) DEFAULT 'warning',
    message_template TEXT,
    applicable_data_types TEXT,
    applicable_device_types TEXT,
    applicable_units TEXT,
    industry VARCHAR(50),
    equipment_type VARCHAR(50),
    usage_count INTEGER DEFAULT 0,
    is_active INTEGER DEFAULT 1,
    is_system_template INTEGER DEFAULT 0,
    tags TEXT DEFAULT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, name)
);

-- 스크립트 라이브러리 테이블
CREATE TABLE IF NOT EXISTS script_library (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL DEFAULT 0,
    category VARCHAR(50) NOT NULL,
    name VARCHAR(100) NOT NULL,
    display_name VARCHAR(100),
    description TEXT,
    script_code TEXT NOT NULL,
    parameters TEXT,
    return_type VARCHAR(20) DEFAULT 'number',
    tags TEXT,
    example_usage TEXT,
    is_system INTEGER DEFAULT 0,
    usage_count INTEGER DEFAULT 0,
    rating REAL DEFAULT 0.0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(tenant_id, name)
);`;
    }

    getIndexesSQL() {
        return `-- =============================================================================
-- 04-indexes.sql - 인덱스 생성 (device_id INTEGER + cleared_by 반영)
-- =============================================================================

-- 디바이스 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_devices_tenant_id ON devices(tenant_id);
CREATE INDEX IF NOT EXISTS idx_devices_site_id ON devices(site_id);
CREATE INDEX IF NOT EXISTS idx_devices_protocol_type ON devices(protocol_type);
CREATE INDEX IF NOT EXISTS idx_devices_device_type ON devices(device_type);
CREATE INDEX IF NOT EXISTS idx_devices_is_enabled ON devices(is_enabled);

-- 데이터포인트 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_data_points_tenant_id ON data_points(tenant_id);
CREATE INDEX IF NOT EXISTS idx_data_points_device_id ON data_points(device_id);
CREATE INDEX IF NOT EXISTS idx_data_points_is_enabled ON data_points(is_enabled);

-- 가상포인트 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_virtual_points_tenant_id ON virtual_points(tenant_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_is_enabled ON virtual_points(is_enabled);

-- 알람 규칙 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_rules_tenant ON alarm_rules(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_target ON alarm_rules(target_type, target_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_enabled ON alarm_rules(is_enabled);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_template_id ON alarm_rules(template_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_rule_group ON alarm_rules(rule_group);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_created_by_template ON alarm_rules(created_by_template);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_category ON alarm_rules(category);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_tags ON alarm_rules(tags);

-- 알람 발생 관련 인덱스 - device_id INTEGER + cleared_by 반영
CREATE INDEX IF NOT EXISTS idx_alarm_occ_rule_id ON alarm_occurrences(rule_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occ_tenant_id ON alarm_occurrences(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occ_state ON alarm_occurrences(state);
CREATE INDEX IF NOT EXISTS idx_alarm_occ_severity ON alarm_occurrences(severity);
CREATE INDEX IF NOT EXISTS idx_alarm_occ_occurrence_time ON alarm_occurrences(occurrence_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_occ_device_id ON alarm_occurrences(device_id);  -- INTEGER 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_occ_point_id ON alarm_occurrences(point_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occ_category ON alarm_occurrences(category);
CREATE INDEX IF NOT EXISTS idx_alarm_occ_acknowledged_by ON alarm_occurrences(acknowledged_by);
CREATE INDEX IF NOT EXISTS idx_alarm_occ_cleared_by ON alarm_occurrences(cleared_by);  -- cleared_by 인덱스 추가

-- 알람 템플릿 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_templates_tenant ON alarm_rule_templates(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_category ON alarm_rule_templates(category);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_active ON alarm_rule_templates(is_active);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_system ON alarm_rule_templates(is_system_template);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_usage ON alarm_rule_templates(usage_count DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_name ON alarm_rule_templates(tenant_id, name);

-- 스크립트 라이브러리 인덱스
CREATE UNIQUE INDEX IF NOT EXISTS idx_script_library_tenant_name ON script_library(tenant_id, name);
CREATE INDEX IF NOT EXISTS idx_script_library_category ON script_library(category);
CREATE INDEX IF NOT EXISTS idx_script_library_tenant ON script_library(tenant_id);
CREATE INDEX IF NOT EXISTS idx_script_library_is_system ON script_library(is_system);
CREATE INDEX IF NOT EXISTS idx_script_library_usage_count ON script_library(usage_count DESC);

-- 사용자 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_users_tenant_id ON users(tenant_id);
CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
CREATE INDEX IF NOT EXISTS idx_users_is_active ON users(is_active);

-- 세션 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_user_sessions_user_id ON user_sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_user_sessions_session_token ON user_sessions(session_token);
CREATE INDEX IF NOT EXISTS idx_user_sessions_expires_at ON user_sessions(expires_at);

-- 디바이스 상태 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_device_status_connection_status ON device_status(connection_status);
CREATE INDEX IF NOT EXISTS idx_device_status_last_communication ON device_status(last_communication);

-- 사이트 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_sites_tenant_id ON sites(tenant_id);
CREATE INDEX IF NOT EXISTS idx_sites_edge_server_id ON sites(edge_server_id);
CREATE INDEX IF NOT EXISTS idx_sites_is_active ON sites(is_active);`;
    }

    getInitialDataSQL() {
        return `-- =============================================================================
-- initial-data.sql - 초기 샘플 데이터 (device_id INTEGER 반영)
-- =============================================================================

-- 기본 테넌트 생성
INSERT OR REPLACE INTO tenants (
    id, company_name, company_code, domain, contact_name, contact_email, 
    subscription_plan, subscription_status, max_edge_servers, max_data_points, max_users
) VALUES (
    1, 'Smart Guard Corporation', 'SGC', 'smartguard.local', 'System Admin', 'admin@smartguard.local',
    'professional', 'active', 10, 10000, 50
);

-- Edge 서버 생성
INSERT OR REPLACE INTO edge_servers (
    id, tenant_id, server_name, server_code, hostname, ip_address, port, status, version
) VALUES (
    1, 1, 'Main Edge Server', 'EDGE01', 'localhost', '127.0.0.1', 3000, 'online', '2.1.0'
);

-- 기본 사용자 생성 (admin/admin)
INSERT OR REPLACE INTO users (
    id, tenant_id, username, email, password_hash, full_name, role, is_active
) VALUES (
    1, 1, 'admin', 'admin@smartguard.local', 
    '$2b$10$8K1p/a0dQt3C4qtzNzQgZe7d7o.S4Q8YjSFkjEJQq4zB1YfH5YnS6',
    'System Administrator', 'admin', 1
);

-- 기본 사이트 생성
INSERT OR REPLACE INTO sites (
    id, tenant_id, edge_server_id, site_name, site_code, description, 
    location_city, location_country, timezone
) VALUES (
    1, 1, 1, 'Seoul Main Factory', 'SITE1', 'Main manufacturing facility in Seoul',
    'Seoul', 'South Korea', 'Asia/Seoul'
);

-- 디바이스 그룹 생성
INSERT OR REPLACE INTO device_groups (
    id, tenant_id, site_id, group_name, group_type, description
) VALUES 
(1, 1, 1, 'Production Line A', 'functional', 'Main production line equipment'),
(2, 1, 1, 'HVAC System', 'functional', 'Heating, ventilation, and air conditioning');

-- 샘플 디바이스 생성
INSERT OR REPLACE INTO devices (
    id, tenant_id, site_id, device_group_id, edge_server_id, name, description,
    device_type, manufacturer, model, protocol_type, endpoint, config, 
    polling_interval, is_enabled
) VALUES 
(1, 1, 1, 1, 1, 'PLC-001', 'Main Production PLC', 'PLC', 'Siemens', 'S7-1200', 
 'MODBUS_TCP', '192.168.1.10:502', '{"slave_id":1,"timeout":3000}', 1000, 0),
(2, 1, 1, 1, 1, 'HMI-001', 'Operator Interface Panel', 'HMI', 'Schneider', 'Magelis',
 'MODBUS_TCP', '192.168.1.11:502', '{"slave_id":2,"timeout":3000}', 2000, 1),
(3, 1, 1, 2, 1, 'HVAC-CTRL-001', 'Main HVAC Controller', 'CONTROLLER', 'Honeywell', 'T7350',
 'BACNET', '192.168.1.20', '{"device_id":20,"network":1}', 5000, 1);

-- 디바이스 상태 초기화
INSERT OR REPLACE INTO device_status (device_id, connection_status) VALUES
(1, 'disconnected'), (2, 'connected'), (3, 'connected');

-- 디바이스 설정 초기화  
INSERT OR REPLACE INTO device_settings (device_id) VALUES (1), (2), (3);

-- 샘플 데이터포인트 생성
INSERT OR REPLACE INTO data_points (
    id, tenant_id, device_id, point_name, point_address, data_type, unit, is_enabled
) VALUES 
-- PLC-001 데이터포인트
(1, 1, 1, 'Temperature', '40001', 'number', '°C', 1),
(2, 1, 1, 'Pressure', '40002', 'number', 'bar', 1),
(3, 1, 1, 'Flow Rate', '40003', 'number', 'L/min', 1),
(4, 1, 1, 'Level', '40004', 'number', 'mm', 1),
(5, 1, 1, 'Vibration', '40005', 'number', 'Hz', 1),
-- HMI-001 데이터포인트
(6, 1, 2, 'Temperature', '40001', 'number', '°C', 1),
(7, 1, 2, 'Pressure', '40002', 'number', 'bar', 1),
-- HVAC-CTRL-001 데이터포인트
(8, 1, 3, 'Temperature', 'analogValue:1', 'number', '°C', 1),
(9, 1, 3, 'Pressure', 'analogValue:2', 'number', 'bar', 1);

-- 샘플 가상포인트 생성
INSERT OR REPLACE INTO virtual_points (
    id, tenant_id, name, description, formula, data_type, unit,
    calculation_interval, calculation_trigger, is_enabled, category
) VALUES (
    1, 1, 'Test VP Fixed', 'Fixed value test virtual point', '1 + 1', 'float', null,
    1000, 'timer', 1, 'calculation'
);

-- 샘플 알람 규칙 생성
INSERT OR REPLACE INTO alarm_rules (
    id, tenant_id, name, description, target_type, target_id,
    alarm_type, high_limit, severity, message_template, is_enabled
) VALUES (
    1, 1, 'High Temperature Alert', 'Temperature exceeds safe threshold', 'data_point', 1,
    'threshold', 80.0, 'major', 
    'High temperature detected: {{value}}°C', 1
);

-- 샘플 알람 발생 테스트 데이터 (device_id INTEGER + cleared_by 확인용)
INSERT OR REPLACE INTO alarm_occurrences (
    id, rule_id, tenant_id, device_id, point_id, 
    trigger_value, alarm_message, severity, state, cleared_by
) VALUES (
    1, 1, 1, 1, 1,  -- device_id는 INTEGER 1
    '85.5', 'High temperature detected: 85.5°C', 'major', 'cleared', 1
);`;
    }

    // =============================================================================
    // 기존 내부 메소드들
    // =============================================================================

    async ensureDataDirectory() {
        try {
            if (!existsSync(this.dataDir)) {
                await fs.mkdir(this.dataDir, { recursive: true });
                console.log(`데이터 디렉토리 생성: ${this.dataDir}`);
            }
        } catch (error) {
            console.error('데이터 디렉토리 생성 실패:', error.message);
            throw error;
        }
    }

    async openDatabase() {
        return new Promise((resolve, reject) => {
            const db = new sqlite3.Database(this.dbPath, (err) => {
                if (err) {
                    reject(new Error(`데이터베이스 연결 실패: ${err.message}`));
                } else {
                    db.serialize(() => {
                        db.run("PRAGMA foreign_keys = ON");
                        db.run("PRAGMA journal_mode = WAL");
                        db.run("PRAGMA synchronous = NORMAL");
                        db.run("PRAGMA cache_size = 10000");
                    });
                    resolve(db);
                }
            });
        });
    }

    async closeDatabase(db) {
        return new Promise((resolve) => {
            db.close((err) => {
                if (err) {
                    console.warn('데이터베이스 닫기 오류:', err.message);
                }
                resolve();
            });
        });
    }

    async checkSystemTables(db) {
        const requiredTables = [
            'tenants', 'edge_servers', 'users', 'user_sessions',
            'sites', 'device_groups', 'devices', 'data_points',
            'virtual_points', 'alarm_rules', 'alarm_occurrences'
        ];

        for (const table of requiredTables) {
            const exists = await this.tableExists(db, table);
            if (!exists) {
                console.log(`필수 테이블 없음: ${table}`);
                return false;
            }
        }

        console.log('모든 시스템 테이블 존재 확인');
        return true;
    }

    async checkTenantSchemas(db) {
        try {
            const tenantCount = await this.getRowCount(db, 'tenants');
            return tenantCount > 0;
        } catch (error) {
            return false;
        }
    }

    async checkSampleData(db) {
        try {
            const deviceCount = await this.getRowCount(db, 'devices');
            const userCount = await this.getRowCount(db, 'users');
            return deviceCount > 0 && userCount > 0;
        } catch (error) {
            return false;
        }
    }

    async checkIndexes(db) {
        return new Promise((resolve) => {
            db.get(
                "SELECT COUNT(*) as count FROM sqlite_master WHERE type='index' AND name NOT LIKE 'sqlite_%'",
                (err, row) => {
                    if (err) {
                        resolve(false);
                    } else {
                        resolve(row.count > 5);
                    }
                }
            );
        });
    }

    async tableExists(db, tableName) {
        return new Promise((resolve) => {
            db.get(
                "SELECT name FROM sqlite_master WHERE type='table' AND name=?",
                [tableName],
                (err, row) => {
                    resolve(!!row);
                }
            );
        });
    }

    async getRowCount(db, tableName) {
        return new Promise((resolve, reject) => {
            db.get(`SELECT COUNT(*) as count FROM ${tableName}`, (err, row) => {
                if (err) {
                    reject(err);
                } else {
                    resolve(row.count);
                }
            });
        });
    }

    async executeSQL(db, sql) {
        return new Promise((resolve, reject) => {
            db.run(sql, (err) => {
                if (err) {
                    console.error(`SQL 실행 실패: ${err.message}`);
                    console.error(`SQL: ${sql.substring(0, 100)}...`);
                    reject(err);
                } else {
                    resolve();
                }
            });
        });
    }
}

module.exports = DatabaseInitializer;