PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE schema_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    version VARCHAR(20) NOT NULL,
    applied_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    description TEXT
);
INSERT INTO schema_versions VALUES(1,'2.1.0','2026-01-16 01:13:27','Complete PulseOne v2.1.0 schema - C++ SQLQueries.h compatible');
CREATE TABLE tenants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 🔥 기본 회사 정보
    company_name VARCHAR(100) NOT NULL UNIQUE,
    company_code VARCHAR(20) NOT NULL UNIQUE,
    domain VARCHAR(100) UNIQUE,
    
    -- 🔥 연락처 정보
    contact_name VARCHAR(100),
    contact_email VARCHAR(100),
    contact_phone VARCHAR(20),
    
    -- 🔥 SaaS 구독 정보
    subscription_plan VARCHAR(20) DEFAULT 'starter',      -- starter, professional, enterprise
    subscription_status VARCHAR(20) DEFAULT 'active',     -- active, trial, suspended, cancelled
    max_edge_servers INTEGER DEFAULT 3,
    max_data_points INTEGER DEFAULT 1000,
    max_users INTEGER DEFAULT 5,
    
    -- 🔥 billing 정보
    billing_cycle VARCHAR(20) DEFAULT 'monthly',          -- monthly, yearly
    subscription_start_date DATETIME,
    trial_end_date DATETIME,
    next_billing_date DATETIME,
    
    -- 🔥 상태 및 메타데이터
    is_active INTEGER DEFAULT 1,
    timezone VARCHAR(50) DEFAULT 'UTC',
    currency VARCHAR(3) DEFAULT 'USD',
    language VARCHAR(5) DEFAULT 'en',
    
    -- 🔥 감사 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    is_deleted BOOLEAN DEFAULT 0,
    
    -- 🔥 제약조건
    CONSTRAINT chk_subscription_plan CHECK (subscription_plan IN ('starter', 'professional', 'enterprise')),
    CONSTRAINT chk_subscription_status CHECK (subscription_status IN ('active', 'trial', 'suspended', 'cancelled'))
);
INSERT INTO tenants VALUES(1,'Smart Factory Korea','SFK001','smartfactory.pulseone.io','Factory Manager','manager@smartfactory.co.kr','+82-2-1234-5678','professional','active',10,10000,50,'monthly',NULL,NULL,NULL,1,'UTC','USD','en','2026-01-16 01:13:27','2026-01-16 01:13:27',0);
INSERT INTO tenants VALUES(2,'Global Manufacturing Inc','GMI002','global-mfg.pulseone.io','Operations Director','ops@globalmfg.com','+1-555-0123','enterprise','active',50,100000,200,'monthly',NULL,NULL,NULL,1,'UTC','USD','en','2026-01-16 01:13:27','2026-01-16 01:13:27',0);
INSERT INTO tenants VALUES(3,'Demo Corporation','DEMO','demo.pulseone.io','Demo Manager','demo@pulseone.com','+82-10-0000-0000','starter','trial',3,1000,10,'monthly',NULL,NULL,NULL,1,'UTC','USD','en','2026-01-16 01:13:27','2026-01-16 01:13:27',0);
INSERT INTO tenants VALUES(4,'Test Factory Ltd','TEST','test.pulseone.io','Test Engineer','test@testfactory.com','+82-31-9999-8888','professional','active',5,5000,25,'monthly',NULL,NULL,NULL,1,'UTC','USD','en','2026-01-16 01:13:27','2026-01-16 01:13:27',0);
CREATE TABLE edge_servers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 🔥 서버 식별 정보
    server_name VARCHAR(100) NOT NULL,
    factory_name VARCHAR(100),
    location VARCHAR(200),
    
    -- 🔥 네트워크 정보
    ip_address VARCHAR(45),
    mac_address VARCHAR(17),
    hostname VARCHAR(100),
    port INTEGER DEFAULT 8080,
    
    -- 🔥 등록 및 보안
    registration_token VARCHAR(255) UNIQUE,
    instance_key VARCHAR(255) UNIQUE,                      -- 컬렉터 인스턴스 고유 키 (hostname:hash)
    activation_code VARCHAR(50),
    api_key VARCHAR(255),
    
    -- 🔥 상태 정보
    status VARCHAR(20) DEFAULT 'pending',                  -- pending, active, inactive, maintenance, error
    last_seen DATETIME,
    last_heartbeat DATETIME,
    version VARCHAR(20),
    
    -- 🔥 성능 정보
    cpu_usage REAL DEFAULT 0.0,
    memory_usage REAL DEFAULT 0.0,
    disk_usage REAL DEFAULT 0.0,
    uptime_seconds INTEGER DEFAULT 0,
    
    -- 🔥 설정 정보
    config TEXT,                                           -- JSON 형태 설정
    capabilities TEXT,                                     -- JSON 배열: 지원 프로토콜
    sync_enabled INTEGER DEFAULT 1,
    auto_update_enabled INTEGER DEFAULT 1,
    
    -- 🔥 메타데이터
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_deleted INTEGER DEFAULT 0,
    site_id INTEGER, server_type TEXT DEFAULT 'collector',
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE SET NULL,
    
    -- 🔥 제약조건
    CONSTRAINT chk_edge_status CHECK (status IN ('pending', 'active', 'inactive', 'maintenance', 'error'))
);
INSERT INTO edge_servers VALUES(1,1,'Main Collector','Seoul Main Factory',NULL,'127.0.0.1',NULL,NULL,8080,NULL,'docker-collector-1:70a33d',NULL,NULL,'active','2026-01-18 12:33:15','2026-01-18 12:33:15','2.1.0',0.0,0.0,0.0,0,NULL,NULL,1,1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,1,'collector');
INSERT INTO edge_servers VALUES(2,2,'NY Collector','New York Plant',NULL,'10.0.1.5',NULL,NULL,8080,NULL,NULL,NULL,NULL,'active',NULL,NULL,'2.1.0',0.0,0.0,0.0,0,NULL,NULL,1,1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,3,'collector');
INSERT INTO edge_servers VALUES(3,3,'Demo Collector','Demo Factory',NULL,'192.168.100.20',NULL,NULL,8080,NULL,NULL,NULL,NULL,'active',NULL,NULL,'2.1.0',0.0,0.0,0.0,0,NULL,NULL,1,1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,5,'collector');
INSERT INTO edge_servers VALUES(4,4,'Test Collector','Test Facility',NULL,'192.168.200.20',NULL,NULL,8080,NULL,NULL,NULL,NULL,'active',NULL,NULL,'2.1.0',0.0,0.0,0.0,0,NULL,NULL,1,1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,6,'collector');
INSERT INTO edge_servers VALUES(6,1,'Primary Export Gateway',NULL,NULL,'127.0.0.1',NULL,NULL,8080,NULL,NULL,NULL,NULL,'pending',NULL,NULL,NULL,0.0,0.0,0.0,0,NULL,NULL,1,1,'2026-01-18 12:30:30','2026-01-18 12:30:30',0,NULL,'gateway');
CREATE TABLE system_settings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 🔥 설정 키-값
    key_name VARCHAR(100) UNIQUE NOT NULL,
    value TEXT NOT NULL,
    description TEXT,
    
    -- 🔥 분류 및 접근성
    category VARCHAR(50) DEFAULT 'general',               -- general, security, performance, protocol
    data_type VARCHAR(20) DEFAULT 'string',               -- string, integer, boolean, json
    is_public INTEGER DEFAULT 0,                          -- 0=관리자만, 1=모든 사용자
    is_readonly INTEGER DEFAULT 0,                        -- 0=수정가능, 1=읽기전용
    
    -- 🔥 검증
    validation_regex VARCHAR(255),                        -- 값 검증용 정규식
    allowed_values TEXT,                                  -- JSON 배열: 허용값 목록
    min_value REAL,                                       -- 숫자형 최솟값
    max_value REAL,                                       -- 숫자형 최댓값
    
    -- 🔥 메타데이터
    updated_by INTEGER,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 🔥 제약조건
    CONSTRAINT chk_data_type CHECK (data_type IN ('string', 'integer', 'boolean', 'json', 'float'))
);
CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER,                                    -- NULL = 시스템 관리자, 값 있음 = 테넌트 사용자
    
    -- 🔥 기본 인증 정보
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    
    -- 🔥 개인 정보
    full_name VARCHAR(100),
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    phone VARCHAR(20),
    department VARCHAR(100),
    position VARCHAR(100),
    employee_id VARCHAR(50),
    
    -- 🔥 통합 권한 시스템
    role VARCHAR(20) NOT NULL,                            -- system_admin, company_admin, site_admin, engineer, operator, viewer
    permissions TEXT,                                     -- JSON 배열: ["manage_users", "view_data", "control_devices"]
    
    -- 🔥 접근 범위 제어 (테넌트 사용자만)
    site_access TEXT,                                     -- JSON 배열: [1, 2, 3] (접근 가능한 사이트 ID들)
    device_access TEXT,                                   -- JSON 배열: [10, 11, 12] (접근 가능한 디바이스 ID들)
    data_point_access TEXT,                               -- JSON 배열: [100, 101, 102] (접근 가능한 데이터포인트 ID들)
    
    -- 🔥 보안 설정
    two_factor_enabled INTEGER DEFAULT 0,
    two_factor_secret VARCHAR(32),
    password_reset_token VARCHAR(255),
    password_reset_expires DATETIME,
    email_verification_token VARCHAR(255),
    email_verified INTEGER DEFAULT 0,
    
    -- 🔥 계정 상태
    is_active INTEGER DEFAULT 1,
    is_locked INTEGER DEFAULT 0,
    failed_login_attempts INTEGER DEFAULT 0,
    last_login DATETIME,
    last_password_change DATETIME,
    must_change_password INTEGER DEFAULT 0,
    
    -- 🔥 사용자 설정
    timezone VARCHAR(50) DEFAULT 'UTC',
    language VARCHAR(5) DEFAULT 'en',
    theme VARCHAR(20) DEFAULT 'light',                    -- light, dark, auto
    notification_preferences TEXT,                        -- JSON 객체
    
    -- 🔥 감사 정보
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_activity DATETIME,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_role CHECK (role IN ('system_admin', 'company_admin', 'site_admin', 'engineer', 'operator', 'viewer')),
    CONSTRAINT chk_theme CHECK (theme IN ('light', 'dark', 'auto'))
);
INSERT INTO users VALUES(1,1,'admin','admin@example.com','dummy_hash','System Admin',NULL,NULL,NULL,NULL,NULL,NULL,'system_admin',NULL,NULL,NULL,NULL,0,NULL,NULL,NULL,NULL,0,1,0,0,NULL,NULL,0,'UTC','en','light',NULL,NULL,'2026-01-16 06:16:15','2026-01-16 06:16:15',NULL);
CREATE TABLE user_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    
    -- 🔥 세션 정보
    token_hash VARCHAR(255) NOT NULL,
    refresh_token_hash VARCHAR(255),
    session_id VARCHAR(100) UNIQUE NOT NULL,
    
    -- 🔥 접속 정보
    ip_address VARCHAR(45),
    user_agent TEXT,
    device_fingerprint VARCHAR(255),
    
    -- 🔥 위치 정보 (선택)
    country VARCHAR(2),
    city VARCHAR(100),
    
    -- 🔥 세션 상태
    is_active INTEGER DEFAULT 1,
    expires_at DATETIME NOT NULL,
    last_used DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 🔥 메타데이터
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);
CREATE TABLE sites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    parent_site_id INTEGER,                               -- 계층 구조 지원
    
    -- 🔥 사이트 기본 정보
    name VARCHAR(100) NOT NULL,
    code VARCHAR(20) NOT NULL,                            -- 테넌트 내 고유 코드
    site_type VARCHAR(50) NOT NULL,                       -- company, factory, office, building, floor, line, area, zone
    description TEXT,
    
    -- 🔥 위치 정보
    location VARCHAR(200),
    address TEXT,
    coordinates VARCHAR(100),                             -- GPS 좌표 "lat,lng"
    postal_code VARCHAR(20),
    country VARCHAR(2),                                   -- ISO 3166-1 alpha-2
    city VARCHAR(100),
    state_province VARCHAR(100),
    
    -- 🔥 시간대 및 지역 설정
    timezone VARCHAR(50) DEFAULT 'UTC',
    currency VARCHAR(3) DEFAULT 'USD',
    language VARCHAR(5) DEFAULT 'en',
    
    -- 🔥 연락처 정보
    manager_name VARCHAR(100),
    manager_email VARCHAR(100),
    manager_phone VARCHAR(20),
    emergency_contact VARCHAR(200),
    
    -- 🔥 운영 정보
    operating_hours VARCHAR(100),                         -- "08:00-18:00" 또는 "24/7"
    shift_pattern VARCHAR(50),                            -- "3-shift", "2-shift", "day-only"
    working_days VARCHAR(20) DEFAULT 'MON-FRI',           -- "MON-FRI", "MON-SAT", "24/7"
    
    -- 🔥 시설 정보
    floor_area REAL,                                      -- 면적 (평방미터)
    ceiling_height REAL,                                  -- 천장 높이 (미터)
    max_occupancy INTEGER,                                -- 최대 수용 인원
    
    -- 🔥 Edge 서버 연결
    edge_server_id INTEGER,
    
    -- 🔥 계층 및 정렬
    hierarchy_level INTEGER DEFAULT 0,                   -- 0=Company, 1=Factory, 2=Building, 3=Line, etc.
    hierarchy_path VARCHAR(500),                         -- "/1/2/3" 형태 경로
    sort_order INTEGER DEFAULT 0,
    
    -- 🔥 상태 및 설정
    is_active INTEGER DEFAULT 1,
    is_deleted INTEGER DEFAULT 0,                        -- ⬅️ Added for soft delete
    is_visible INTEGER DEFAULT 1,                        -- 사용자에게 표시 여부
    monitoring_enabled INTEGER DEFAULT 1,                -- 모니터링 활성화
    
    -- 🔥 메타데이터
    tags TEXT,                                           -- JSON 배열
    metadata TEXT,                                       -- JSON 객체 (추가 속성)
    
    -- 🔥 감사 정보
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (parent_site_id) REFERENCES sites(id) ON DELETE SET NULL,
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    UNIQUE(tenant_id, code),
    CONSTRAINT chk_site_type CHECK (site_type IN ('company', 'factory', 'office', 'building', 'floor', 'line', 'area', 'zone', 'room'))
);
INSERT INTO sites VALUES(1,1,NULL,'Seoul Main Factory','SMF001','factory','Main manufacturing facility','Seoul Industrial Complex',NULL,NULL,NULL,NULL,NULL,NULL,'UTC','USD','en',NULL,NULL,NULL,NULL,NULL,NULL,'MON-FRI',NULL,NULL,NULL,NULL,0,NULL,0,1,0,1,1,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO sites VALUES(2,1,NULL,'Busan Secondary Plant','BSP002','factory','Secondary production facility','Busan Industrial Park',NULL,NULL,NULL,NULL,NULL,NULL,'UTC','USD','en',NULL,NULL,NULL,NULL,NULL,NULL,'MON-FRI',NULL,NULL,NULL,NULL,0,NULL,0,1,0,1,1,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO sites VALUES(3,2,NULL,'New York Plant','NYP003','factory','East Coast Manufacturing Plant','New York Industrial Zone',NULL,NULL,NULL,NULL,NULL,NULL,'UTC','USD','en',NULL,NULL,NULL,NULL,NULL,NULL,'MON-FRI',NULL,NULL,NULL,NULL,0,NULL,0,1,0,1,1,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO sites VALUES(4,2,NULL,'Detroit Automotive Plant','DAP004','factory','Automotive Manufacturing Plant','Detroit, MI',NULL,NULL,NULL,NULL,NULL,NULL,'UTC','USD','en',NULL,NULL,NULL,NULL,NULL,NULL,'MON-FRI',NULL,NULL,NULL,NULL,0,NULL,0,1,0,1,1,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO sites VALUES(5,3,NULL,'Demo Factory','DEMO005','factory','Demonstration facility','Demo Location',NULL,NULL,NULL,NULL,NULL,NULL,'UTC','USD','en',NULL,NULL,NULL,NULL,NULL,NULL,'MON-FRI',NULL,NULL,NULL,NULL,0,NULL,0,1,0,1,1,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO sites VALUES(6,4,NULL,'Test Facility','TEST006','factory','Testing and R&D facility','Test Location',NULL,NULL,NULL,NULL,NULL,NULL,'UTC','USD','en',NULL,NULL,NULL,NULL,NULL,NULL,'MON-FRI',NULL,NULL,NULL,NULL,0,NULL,0,1,0,1,1,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
CREATE TABLE user_favorites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    
    -- 🔥 즐겨찾기 대상
    target_type VARCHAR(20) NOT NULL,                    -- 'site', 'device', 'data_point', 'alarm', 'dashboard'
    target_id INTEGER NOT NULL,
    
    -- 🔥 메타데이터
    display_name VARCHAR(100),
    notes TEXT,
    sort_order INTEGER DEFAULT 0,
    
    -- 🔥 감사 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    UNIQUE(user_id, target_type, target_id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_target_type CHECK (target_type IN ('site', 'device', 'data_point', 'alarm', 'dashboard', 'virtual_point'))
);
CREATE TABLE user_notification_settings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    
    -- 🔥 알림 채널 설정
    email_enabled INTEGER DEFAULT 1,
    sms_enabled INTEGER DEFAULT 0,
    push_enabled INTEGER DEFAULT 1,
    slack_enabled INTEGER DEFAULT 0,
    teams_enabled INTEGER DEFAULT 0,
    
    -- 🔥 알림 타입별 설정
    alarm_notifications INTEGER DEFAULT 1,
    system_notifications INTEGER DEFAULT 1,
    maintenance_notifications INTEGER DEFAULT 1,
    report_notifications INTEGER DEFAULT 0,
    
    -- 🔥 알림 시간 설정
    quiet_hours_start TIME,                              -- 알림 차단 시작 시간
    quiet_hours_end TIME,                                -- 알림 차단 종료 시간
    weekend_notifications INTEGER DEFAULT 0,             -- 주말 알림 허용
    
    -- 🔥 연락처 정보
    sms_phone VARCHAR(20),
    slack_webhook_url VARCHAR(255),
    teams_webhook_url VARCHAR(255),
    
    -- 🔥 메타데이터
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    UNIQUE(user_id)
);
CREATE TABLE protocols (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 기본 정보
    protocol_type VARCHAR(50) NOT NULL UNIQUE,      -- MODBUS_TCP, MODBUS_RTU, MQTT, etc.
    display_name VARCHAR(100) NOT NULL,             -- "Modbus TCP", "MQTT", etc.
    description TEXT,                               -- 상세 설명
    
    -- 네트워크 정보
    default_port INTEGER,                           -- 기본 포트 (502, 1883, etc.)
    uses_serial INTEGER DEFAULT 0,                 -- 시리얼 통신 사용 여부
    requires_broker INTEGER DEFAULT 0,             -- 브로커 필요 여부 (MQTT 등)
    
    -- 기능 지원 정보 (JSON)
    supported_operations TEXT,                      -- ["read_coils", etc.]
    supported_data_types TEXT,                      -- ["boolean", "int16", "float32", etc.]
    connection_params TEXT,                         -- 연결에 필요한 파라미터 스키마
    capabilities TEXT DEFAULT '{}',                 -- 프로토콜별 특수 역량 (JSON)
    
    -- 설정 정보
    default_polling_interval INTEGER DEFAULT 1000, -- 기본 수집 주기 (ms)
    default_timeout INTEGER DEFAULT 5000,          -- 기본 타임아웃 (ms)
    max_concurrent_connections INTEGER DEFAULT 1,   -- 최대 동시 연결 수
    
    -- 상태 정보
    is_enabled INTEGER DEFAULT 1,                  -- 프로토콜 활성화 여부
    is_deprecated INTEGER DEFAULT 0,               -- 사용 중단 예정
    min_firmware_version VARCHAR(20),              -- 최소 펌웨어 버전
    
    -- 분류 정보
    category VARCHAR(50),                           -- industrial, iot, building_automation, etc.
    vendor VARCHAR(100),                            -- 제조사/개발사
    standard_reference VARCHAR(100),               -- 표준 문서 참조
    
    -- 메타데이터
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 제약조건
    CONSTRAINT chk_category CHECK (category IN ('industrial', 'iot', 'building_automation', 'network', 'web'))
);
INSERT INTO protocols VALUES(1,'MODBUS_TCP','Modbus TCP/IP','Industrial protocol over Ethernet',502,0,0,'["read", "write"]','["BOOL", "INT16", "UINT16", "INT32", "UINT32", "FLOAT32"]','{"slave_id": {"type": "integer", "default": 1}}','{}',1000,3000,10,1,0,'1.0','industrial','Modbus Organization','Modbus Application Protocol V1.1b3','2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO protocols VALUES(2,'MODBUS_RTU','Modbus RTU','Modbus over serial communication',NULL,1,0,'["read", "write"]','["BOOL", "INT16", "UINT16", "INT32", "UINT32", "FLOAT32"]','{"slave_id": {"type": "integer", "default": 1}}','{}',1000,3000,1,1,0,'1.0','industrial','Modbus Organization','Modbus over Serial Line V1.02','2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO protocols VALUES(4,'MQTT','MQTT v3.1.1','Lightweight messaging protocol for IoT',1883,0,1,'["publish", "subscribe"]','["BOOL", "INT32", "FLOAT32", "STRING", "JSON"]','{"broker_url": {"type": "string"}, "client_id": {"type": "string"}}','{}',0,5000,100,1,0,'3.1.1','iot','MQTT.org','MQTT v3.1.1 OASIS Standard','2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO protocols VALUES(6,'BACNET','BACnet/IP','Building automation and control networks',47808,0,0,'["read", "write"]','["BOOL", "INT32", "UINT32", "FLOAT32", "STRING"]','{"device_id": {"type": "integer", "default": 1001}}','{}',5000,10000,5,1,0,'1.0','building_automation','ASHRAE','ANSI/ASHRAE 135-2020','2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO protocols VALUES(13,'BLE_BEACON','BLE Beacon','Bluetooth Low Energy Beaconing',NULL,0,0,'["read"]','["BOOL", "INT32", "FLOAT32", "STRING"]','{"uuid": {"type": "string"}}','{}',1000,3000,100,1,0,'4.0','iot','Bluetooth SIG','Bluetooth Core Specification 4.0','2026-01-16 01:13:27','2026-01-16 01:13:27');
CREATE TABLE manufacturers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL UNIQUE,
    description TEXT,
    country VARCHAR(50),
    website VARCHAR(255),
    logo_url VARCHAR(255),
    is_active INTEGER DEFAULT 1,
    is_deleted INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
INSERT INTO manufacturers VALUES(1,'LS Electric','Global total solution provider in electric power and automation','South Korea','https://www.lselectric.co.kr',NULL,1,0,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO manufacturers VALUES(2,'Siemens','German multinational technology conglomerate','Germany','https://www.siemens.com',NULL,1,0,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO manufacturers VALUES(3,'Schneider Electric','French multinational company specializing in digital automation and energy management','France','https://www.se.com',NULL,1,0,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO manufacturers VALUES(4,'ABB','Swedish-Swiss multinational corporation operating mainly in robotics, power, heavy electrical equipment, and automation technology','Switzerland','https://new.abb.com',NULL,1,0,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO manufacturers VALUES(5,'Delta Electronics','Taiwanese electronics manufacturing company','Taiwan','https://www.deltaww.com',NULL,1,0,'2026-01-16 01:13:27','2026-01-16 01:13:27');
CREATE TABLE device_models (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    manufacturer_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    model_number VARCHAR(100),
    device_type VARCHAR(50),                             -- PLC, HMI, SENSOR, etc.
    description TEXT,
    image_url VARCHAR(255),
    manual_url VARCHAR(255),
    metadata TEXT,                                       -- JSON 형태
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (manufacturer_id) REFERENCES manufacturers(id) ON DELETE CASCADE,
    UNIQUE(manufacturer_id, name)
);
INSERT INTO device_models VALUES(1,1,'XGT Series','XGK-CPUS','PLC','LS Electric XGT Series Programmable Logic Controller',NULL,NULL,NULL,1,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO device_models VALUES(2,1,'iS7 Inverter','SV-iS7','INVERTER','LS Electric iS7 Series High Performance Inverter',NULL,NULL,NULL,1,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO device_models VALUES(3,2,'S7-1200','CPU 1214C','PLC','Siemens SIMATIC S7-1200 Compact Controller',NULL,NULL,NULL,1,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO device_models VALUES(4,3,'PowerLogic PM8000','METSEPM8000','METER','Schneider Electric PowerLogic PM8000 series power meter',NULL,NULL,NULL,1,'2026-01-16 01:13:27','2026-01-16 01:13:27');
CREATE TABLE device_groups (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER NOT NULL,
    parent_group_id INTEGER,                              -- 계층 그룹 지원
    
    -- 🔥 그룹 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    group_type VARCHAR(50) DEFAULT 'functional',          -- functional, physical, protocol, location
    
    -- 🔥 메타데이터
    tags TEXT,                                           -- JSON 배열
    metadata TEXT,                                       -- JSON 객체
    
    -- 🔥 상태 정보
    is_active INTEGER DEFAULT 1,
    sort_order INTEGER DEFAULT 0,
    
    -- 🔥 감사 정보
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (parent_group_id) REFERENCES device_groups(id) ON DELETE SET NULL,
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_group_type CHECK (group_type IN ('functional', 'physical', 'protocol', 'location'))
);
CREATE TABLE device_group_assignments (
    device_id INTEGER NOT NULL,
    group_id INTEGER NOT NULL,
    is_primary INTEGER DEFAULT 0,                         -- 대표 그룹 여부
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (device_id, group_id),
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    FOREIGN KEY (group_id) REFERENCES device_groups(id) ON DELETE CASCADE
);
CREATE TABLE driver_plugins (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 🔥 플러그인 기본 정보
    name VARCHAR(100) NOT NULL,
    protocol_type VARCHAR(50) NOT NULL,
    version VARCHAR(20) NOT NULL,
    file_path VARCHAR(255) NOT NULL,
    description TEXT,
    
    -- 🔥 플러그인 메타데이터
    author VARCHAR(100),
    website VARCHAR(255),
    documentation_url VARCHAR(255),
    api_version INTEGER DEFAULT 1,
    min_system_version VARCHAR(20),
    
    -- 🔥 설정 스키마
    config_schema TEXT,                                   -- JSON 스키마 정의
    default_config TEXT,                                 -- JSON 기본 설정값
    
    -- 🔥 상태 정보
    is_enabled INTEGER DEFAULT 1,
    is_system INTEGER DEFAULT 0,                         -- 시스템 제공 드라이버
    
    -- 🔥 라이선스 정보
    license_type VARCHAR(20) DEFAULT 'free',             -- free, commercial, trial
    license_expires DATETIME,
    license_key VARCHAR(255),
    
    -- 🔥 성능 정보
    max_concurrent_connections INTEGER DEFAULT 10,
    supported_features TEXT,                             -- JSON 배열
    
    -- 🔥 감사 정보
    installed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 🔥 제약조건
    UNIQUE(protocol_type, version),
    CONSTRAINT chk_license_type CHECK (license_type IN ('free', 'commercial', 'trial'))
);
CREATE TABLE devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER NOT NULL,
    device_group_id INTEGER,
    edge_server_id INTEGER,
    
    -- 디바이스 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    device_type VARCHAR(50) NOT NULL,                    -- PLC, HMI, SENSOR, GATEWAY, METER, CONTROLLER, ROBOT, INVERTER
    manufacturer VARCHAR(100),
    model VARCHAR(100),
    serial_number VARCHAR(100),
    firmware_version VARCHAR(50),
    
    -- 프로토콜 설정 (외래키 사용)
    protocol_id INTEGER NOT NULL,
    endpoint VARCHAR(255) NOT NULL,                      -- IP:Port 또는 연결 문자열
    config TEXT NOT NULL,                               -- JSON 형태 프로토콜별 설정
    
    -- 기본 수집 설정 (세부 설정은 device_settings 테이블 참조)
    polling_interval INTEGER DEFAULT 1000,               -- 밀리초
    timeout INTEGER DEFAULT 3000,                       -- 밀리초
    retry_count INTEGER DEFAULT 3,
    
    -- 물리적 정보
    location_description VARCHAR(200),
    installation_date DATE,
    last_maintenance DATE,
    next_maintenance DATE,
    warranty_expires DATE,
    
    -- 상태 정보
    is_enabled INTEGER DEFAULT 1,
    is_deleted INTEGER DEFAULT 0,                       -- ⬅️ Added for soft delete
    is_simulation_mode INTEGER DEFAULT 0,               -- 시뮬레이션 모드
    priority INTEGER DEFAULT 100,                       -- 수집 우선순위 (낮을수록 높은 우선순위)
    
    -- 메타데이터
    tags TEXT,                                          -- JSON 배열
    metadata TEXT,                                      -- JSON 객체
    custom_fields TEXT,                                 -- JSON 객체 (사용자 정의 필드)
    
    -- 템플릿 및 제조사 연동
    template_device_id INTEGER,
    manufacturer_id INTEGER,
    model_id INTEGER,
    
    -- 감사 정보
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (device_group_id) REFERENCES device_groups(id) ON DELETE SET NULL,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
    FOREIGN KEY (protocol_id) REFERENCES protocols(id) ON DELETE RESTRICT,
    FOREIGN KEY (created_by) REFERENCES users(id),
    FOREIGN KEY (manufacturer_id) REFERENCES manufacturers(id) ON DELETE SET NULL,
    FOREIGN KEY (model_id) REFERENCES device_models(id) ON DELETE SET NULL,
    FOREIGN KEY (template_device_id) REFERENCES template_devices(id) ON DELETE SET NULL,
    
    -- 제약조건
    CONSTRAINT chk_device_type CHECK (device_type IN ('PLC', 'HMI', 'SENSOR', 'GATEWAY', 'METER', 'CONTROLLER', 'ROBOT', 'INVERTER', 'DRIVE', 'SWITCH'))
);
INSERT INTO devices VALUES(1,1,1,NULL,1,'PLC-001','Main production line PLC','PLC','Siemens','S7-1515F',NULL,NULL,1,'172.18.0.7:50502','{"slave_id": 1, "timeout_ms": 3000, "byte_order": "big_endian", "unit_id": 1}',1000,3000,3,NULL,NULL,NULL,NULL,NULL,1,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO devices VALUES(2,1,1,NULL,1,'HMI-001','Operator HMI panel','HMI','Schneider Electric','Harmony GT2512',NULL,NULL,1,'172.18.0.7:50502','{"slave_id": 1, "timeout_ms": 3000, "screen_size": "12_inch", "unit_id": 1}',2000,3000,3,NULL,NULL,NULL,NULL,NULL,1,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO devices VALUES(3,1,1,NULL,1,'ROBOT-001','Automated welding robot','ROBOT','KUKA','KR 16-3 F',NULL,NULL,1,'172.18.0.7:50502','{"slave_id": 1, "timeout_ms": 2000, "coordinate_system": "world", "unit_id": 1}',500,2000,5,NULL,NULL,NULL,NULL,NULL,1,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO devices VALUES(4,1,1,NULL,1,'VFD-001','Conveyor motor VFD','INVERTER','ABB','ACS580-01',NULL,NULL,1,'172.18.0.7:50502','{"slave_id": 1, "timeout_ms": 3000, "motor_rated_power": "15kW", "unit_id": 1}',1000,3000,3,NULL,NULL,NULL,NULL,NULL,1,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO devices VALUES(5,1,1,NULL,1,'HVAC-001','Main air handling unit','CONTROLLER','Honeywell','Spyder',NULL,NULL,2,'192.168.1.30:47808','{"device_id": 1001, "network": 1, "max_apdu": 1476, "segmentation": "segmented_both"}',5000,10000,3,NULL,NULL,NULL,NULL,NULL,1,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO devices VALUES(6,1,1,NULL,1,'METER-001','Main facility power meter','METER','Schneider Electric','PowerLogic PM8000',NULL,NULL,1,'172.18.0.7:50502','{"slave_id": 1, "timeout_ms": 3000, "measurement_class": "0.2S", "unit_id": 1}',5000,3000,3,NULL,NULL,NULL,NULL,NULL,1,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO devices VALUES(7,2,3,NULL,2,'PLC-NY-001','New York plant main PLC','PLC','Rockwell Automation','CompactLogix 5380',NULL,NULL,1,'172.18.0.7:50502','{"slave_id": 1, "timeout_ms": 3000, "communication_format": "RTU_over_TCP", "unit_id": 1}',1000,3000,3,NULL,NULL,NULL,NULL,NULL,1,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO devices VALUES(8,2,3,NULL,2,'ROBOT-NY-001','Assembly robot station 1','ROBOT','Fanuc','R-2000iD/210F',NULL,NULL,4,'10.0.1.15:44818','{"connection_type": "explicit", "assembly_instance": 100, "originator_to_target": 500, "target_to_originator": 500}',200,1000,5,NULL,NULL,NULL,NULL,NULL,1,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO devices VALUES(9,3,5,NULL,3,'DEMO-PLC-001','Demo PLC for training','PLC','Demo Manufacturer','Demo-PLC-v2',NULL,NULL,1,'172.18.0.7:50502','{"slave_id": 1, "timeout_ms": 3000, "demo_mode": true, "unit_id": 1}',2000,3000,3,NULL,NULL,NULL,NULL,NULL,1,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO devices VALUES(10,3,5,NULL,3,'DEMO-IOT-001','IoT gateway for wireless sensors','GATEWAY','Generic IoT','MultiProtocol-GW',NULL,NULL,4,'mqtt://192.168.100.20:1883','{"client_id": "demo_gateway", "username": "demo_user", "password": "demo_pass", "keep_alive": 60}',0,5000,3,NULL,NULL,NULL,NULL,NULL,1,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO devices VALUES(11,4,6,NULL,4,'TEST-PLC-001','Advanced test PLC for R&D','PLC','Test Systems','TestPLC-Pro',NULL,NULL,1,'172.18.0.7:50502','{"slave_id": 1, "timeout_ms": 3000, "test_mode": true, "advanced_diagnostics": true, "unit_id": 1}',1000,3000,3,NULL,NULL,NULL,NULL,NULL,1,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
CREATE TABLE device_settings (
    device_id INTEGER PRIMARY KEY,
    
    -- 🔥 폴링 및 타이밍 설정
    polling_interval_ms INTEGER DEFAULT 1000,
    scan_rate_override INTEGER,                          -- 개별 디바이스 스캔 주기 오버라이드
    scan_group INTEGER DEFAULT 1,                       -- 스캔 그룹 (동시 스캔 제어)
    
    -- 🔥 연결 및 통신 설정
    connection_timeout_ms INTEGER DEFAULT 10000,
    read_timeout_ms INTEGER DEFAULT 5000,
    write_timeout_ms INTEGER DEFAULT 5000,
    inter_frame_delay_ms INTEGER DEFAULT 10,            -- 프레임 간 지연
    
    -- 🔥 재시도 정책
    max_retry_count INTEGER DEFAULT 3,
    retry_interval_ms INTEGER DEFAULT 5000,
    backoff_multiplier DECIMAL(3,2) DEFAULT 1.5,        -- 지수 백오프
    backoff_time_ms INTEGER DEFAULT 60000,
    max_backoff_time_ms INTEGER DEFAULT 300000,         -- 최대 5분
    
    -- 🔥 Keep-alive 설정
    keep_alive_enabled INTEGER DEFAULT 1,
    keep_alive_interval_s INTEGER DEFAULT 30,
    keep_alive_timeout_s INTEGER DEFAULT 10,
    
    -- 🔥 데이터 품질 관리
    data_validation_enabled INTEGER DEFAULT 1,
    outlier_detection_enabled INTEGER DEFAULT 0,
    deadband_enabled INTEGER DEFAULT 1,
    
    -- 🔥 로깅 및 진단
    detailed_logging_enabled INTEGER DEFAULT 0,
    performance_monitoring_enabled INTEGER DEFAULT 1,
    diagnostic_mode_enabled INTEGER DEFAULT 0,
    communication_logging_enabled INTEGER DEFAULT 0,    -- 통신 로그 기록
    
    -- 🔥 버퍼링 설정
    read_buffer_size INTEGER DEFAULT 1024,
    write_buffer_size INTEGER DEFAULT 1024,
    queue_size INTEGER DEFAULT 100,                     -- 명령 큐 크기
    
    -- 🔥 메타데이터
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_by INTEGER,                                 -- 설정을 변경한 사용자
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    FOREIGN KEY (updated_by) REFERENCES users(id)
);
INSERT INTO device_settings VALUES(1,1000,NULL,1,10000,5000,5000,10,3,5000,1.5,60000,300000,1,30,10,1,0,1,0,1,0,0,1024,1024,100,'2026-01-16 01:13:27','2026-01-16 01:13:27',NULL);
INSERT INTO device_settings VALUES(2,2000,NULL,1,10000,5000,5000,10,3,5000,1.5,60000,300000,1,30,10,1,0,1,0,1,0,0,1024,1024,100,'2026-01-16 01:13:27','2026-01-16 01:13:27',NULL);
INSERT INTO device_settings VALUES(3,500,NULL,1,10000,2000,2000,10,5,3000,1.5,60000,300000,1,15,5,1,1,1,1,1,1,0,2048,2048,200,'2026-01-16 01:13:27','2026-01-16 01:13:27',NULL);
INSERT INTO device_settings VALUES(4,1000,NULL,1,10000,5000,5000,10,3,5000,1.5,60000,300000,1,30,10,1,0,1,0,1,0,0,1024,1024,100,'2026-01-16 01:13:27','2026-01-16 01:13:27',NULL);
INSERT INTO device_settings VALUES(5,5000,NULL,1,15000,10000,10000,10,3,10000,2,120000,600000,1,60,20,1,0,1,0,1,0,0,1024,1024,50,'2026-01-16 01:13:27','2026-01-16 01:13:27',NULL);
INSERT INTO device_settings VALUES(6,5000,NULL,1,10000,5000,5000,10,3,5000,1.5,60000,300000,1,30,10,1,0,1,0,1,0,0,1024,1024,100,'2026-01-16 01:13:27','2026-01-16 01:13:27',NULL);
CREATE TABLE device_status (
    device_id INTEGER PRIMARY KEY,
    
    -- 🔥 연결 상태
    connection_status VARCHAR(20) NOT NULL DEFAULT 'disconnected',  -- connected, disconnected, connecting, error
    last_communication DATETIME,
    connection_established_at DATETIME,
    
    -- 🔥 에러 정보
    error_count INTEGER DEFAULT 0,
    last_error TEXT,
    last_error_time DATETIME,
    consecutive_error_count INTEGER DEFAULT 0,
    
    -- 🔥 성능 정보
    response_time INTEGER,                              -- 평균 응답 시간 (ms)
    min_response_time INTEGER,                          -- 최소 응답 시간 (ms)
    max_response_time INTEGER,                          -- 최대 응답 시간 (ms)
    throughput_ops_per_sec REAL DEFAULT 0.0,           -- 초당 처리 작업 수
    
    -- 🔥 통계 정보
    total_requests INTEGER DEFAULT 0,
    successful_requests INTEGER DEFAULT 0,
    failed_requests INTEGER DEFAULT 0,
    uptime_percentage REAL DEFAULT 0.0,
    
    -- 🔥 디바이스 진단 정보
    firmware_version VARCHAR(50),
    hardware_info TEXT,                                 -- JSON 형태
    diagnostic_data TEXT,                               -- JSON 형태
    cpu_usage REAL,                                     -- 디바이스 CPU 사용률
    memory_usage REAL,                                  -- 디바이스 메모리 사용률
    
    -- 🔥 메타데이터
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    CONSTRAINT chk_connection_status CHECK (connection_status IN ('connected', 'disconnected', 'connecting', 'error', 'maintenance'))
);
INSERT INTO device_status VALUES(1,'connected','2026-01-18 12:33:35','2026-01-16 00:13:27',0,NULL,NULL,0,3,8,45,2.5,48367,48367,0,99.9000000000000056,'V16.0.3','{"cpu": "S7-1515F", "memory": "512MB"}','{"last_scan": "normal"}',25.30000000000000071,68.5,'2026-01-18 12:33:35');
INSERT INTO device_status VALUES(2,'connected','2026-01-18 12:33:35','2026-01-15 23:13:27',0,NULL,NULL,0,5,12,78,1.800000000000000044,43858,43858,0,99.5,'V2.1.4','{"screen": "12inch", "memory": "256MB"}','{"display": "active"}',18.69999999999999929,45.20000000000000285,'2026-01-18 12:33:35');
INSERT INTO device_status VALUES(3,'connected','2026-01-18 12:33:35','2026-01-16 00:43:27',0,NULL,NULL,0,7,5,25,5.200000000000000177,46545,46545,0,100.0,'V8.3.2','{"axes": 6, "payload": "16kg"}','{"position": "active"}',42.10000000000000142,72.79999999999999716,'2026-01-18 12:33:35');
INSERT INTO device_status VALUES(4,'connected','2026-01-16 01:12:27','2026-01-16 00:28:27',0,NULL,NULL,0,20,10,55,1.199999999999999956,600,600,0,99.7999999999999972,'V1.5.8','{"power": "15kW", "voltage": "480V"}','{"drive": "running"}',15.19999999999999929,38.89999999999999858,'2026-01-16 01:13:27');
INSERT INTO device_status VALUES(5,'connected','2026-01-16 01:10:27','2026-01-15 19:13:27',0,NULL,NULL,0,45,25,120,0.8000000000000000444,200,200,0,98.5,'V3.2.1','{"zones": 4, "capacity": "50kW"}','{"system": "auto"}',8.5,28.30000000000000071,'2026-01-16 01:13:27');
INSERT INTO device_status VALUES(6,'connected','2026-01-18 12:33:35','2026-01-15 17:13:27',0,NULL,NULL,0,2,20,85,1.0,43478,43478,0,99.2000000000000028,'V2.8.5','{"class": "0.2S", "ct_ratio": "1000:1"}','{"measurement": "active"}',12.80000000000000071,35.70000000000000285,'2026-01-18 12:33:35');
CREATE TABLE data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    
    -- 🔥 기본 식별 정보 (Struct DataPoint와 완전 일치)
    name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 🔥 주소 정보 (Struct DataPoint와 완전 일치)
    address INTEGER NOT NULL,                           -- uint32_t address
    address_string VARCHAR(255),                        -- std::string address_string (별칭)
    
    -- 🔥 데이터 타입 및 접근성 (Struct DataPoint와 완전 일치)
    data_type VARCHAR(20) NOT NULL DEFAULT 'UNKNOWN',   -- std::string data_type
    access_mode VARCHAR(10) DEFAULT 'read',             -- std::string access_mode (read, write, read_write)
    is_enabled INTEGER DEFAULT 1,                      -- bool is_enabled
    is_writable INTEGER DEFAULT 0,                     -- bool is_writable (계산됨)
    
    -- 🔥 엔지니어링 단위 및 스케일링 (Struct DataPoint와 완전 일치)
    unit VARCHAR(50),                                   -- std::string unit
    scaling_factor REAL DEFAULT 1.0,                   -- double scaling_factor
    scaling_offset REAL DEFAULT 0.0,                   -- double scaling_offset
    min_value REAL DEFAULT 0.0,                        -- double min_value
    max_value REAL DEFAULT 0.0,                        -- double max_value
    
    -- 🔥🔥🔥 로깅 및 수집 설정 (SQLQueries.h가 찾던 핵심 컬럼들!)
    log_enabled INTEGER DEFAULT 1,                     -- bool log_enabled ✅
    log_interval_ms INTEGER DEFAULT 0,                 -- uint32_t log_interval_ms ✅
    log_deadband REAL DEFAULT 0.0,                     -- double log_deadband ✅
    polling_interval_ms INTEGER DEFAULT 0,             -- uint32_t polling_interval_ms
    
    -- 🔥 데이터 품질 설정
    quality_check_enabled INTEGER DEFAULT 1,
    range_check_enabled INTEGER DEFAULT 1,
    rate_of_change_limit REAL DEFAULT 0.0,             -- 변화율 제한
    
    -- 🔥🔥🔥 메타데이터 (SQLQueries.h가 찾던 핵심 컬럼들!)
    group_name VARCHAR(50),                             -- std::string group
    tags TEXT,                                          -- std::string tags (JSON 배열) ✅
    metadata TEXT,                                      -- std::string metadata (JSON 객체) ✅
    protocol_params TEXT,                               -- JSON: 프로토콜별 매개변수
    
    -- 🔥 알람 관련 설정
    alarm_enabled INTEGER DEFAULT 0,
    high_alarm_limit REAL,
    low_alarm_limit REAL,
    alarm_deadband REAL DEFAULT 0.0,
    
    -- 🔥 보관 정책
    retention_policy VARCHAR(20) DEFAULT 'standard',    -- standard, extended, minimal, custom
    compression_enabled INTEGER DEFAULT 1,
    
    -- 🔥 시간 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    is_deleted BOOLEAN DEFAULT 0, mapping_key VARCHAR(255),
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    UNIQUE(device_id, address),
    CONSTRAINT chk_data_type CHECK (data_type IN ('BOOL', 'INT8', 'UINT8', 'INT16', 'UINT16', 'INT32', 'UINT32', 'INT64', 'UINT64', 'FLOAT32', 'FLOAT64', 'STRING', 'UNKNOWN')),
    CONSTRAINT chk_access_mode CHECK (access_mode IN ('read', 'write', 'read_write')),
    CONSTRAINT chk_retention_policy CHECK (retention_policy IN ('standard', 'extended', 'minimal', 'custom'))
);
INSERT INTO data_points VALUES(1,1,'Production_Count','Production counter',1001,NULL,'UINT32','read',1,0,'pcs',1.0,0.0,0.0,999999.0,1,5000,0.0,0,1,1,0.0,'production','["production", "counter"]','{"importance": "high"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(2,1,'Line_Speed','Line speed sensor',1002,NULL,'FLOAT32','read',1,0,'m/min',0.1000000000000000055,0.0,0.0,100.0,1,1000,0.0,0,1,1,0.0,'production','["speed", "line"]','{"sensor_type": "encoder"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(3,1,'Motor_Current','Motor current sensor',1003,NULL,'INT16','read',1,0,'A',0.1000000000000000055,0.0,0.0,50.0,1,1000,0.0,0,1,1,0.0,'electrical','["current", "motor"]','{"sensor_model": "CT-100A"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(4,1,'Temperature','Process temperature',1004,NULL,'INT16','read',1,0,'°C',0.1000000000000000055,0.0,-40.0,150.0,1,2000,0.0,0,1,1,0.0,'environmental','["temperature", "process"]','{"sensor_location": "heat_exchanger"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(5,1,'Emergency_Stop','Emergency stop button',1005,NULL,'BOOL','read',1,0,'',1.0,0.0,0.0,1.0,1,0,0.0,0,1,1,0.0,'safety','["emergency", "stop", "safety"]','{"critical": true}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(6,2,'Screen_Status','HMI screen status',2001,NULL,'UINT16','read',1,0,'',1.0,0.0,0.0,255.0,1,5000,0.0,0,1,1,0.0,'interface','["screen", "hmi"]','{"screen_type": "main"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(7,2,'Active_Alarms','Number of active alarms',2002,NULL,'UINT16','read',1,0,'count',1.0,0.0,0.0,100.0,1,2000,0.0,0,1,1,0.0,'alarms','["alarms", "active"]','{"priority": "high"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(8,2,'User_Level','Current user access level',2003,NULL,'UINT16','read',1,0,'',1.0,0.0,0.0,5.0,1,10000,0.0,0,1,1,0.0,'security','["user", "access"]','{"levels": "0=guest,1=operator,2=engineer,3=admin"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(9,3,'Robot_X_Position','Robot X position',3001,NULL,'FLOAT32','read',1,0,'mm',0.0100000000000000002,0.0,-1611.0,1611.0,1,500,0.0,0,1,1,0.0,'position','["robot", "position", "x-axis"]','{"coordinate_system": "world"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(10,3,'Robot_Y_Position','Robot Y position',3003,NULL,'FLOAT32','read',1,0,'mm',0.0100000000000000002,0.0,-1611.0,1611.0,1,500,0.0,0,1,1,0.0,'position','["robot", "position", "y-axis"]','{"coordinate_system": "world"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(11,3,'Robot_Z_Position','Robot Z position',3005,NULL,'FLOAT32','read',1,0,'mm',0.0100000000000000002,0.0,-1000.0,2000.0,1,500,0.0,0,1,1,0.0,'position','["robot", "position", "z-axis"]','{"coordinate_system": "world"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(12,3,'Welding_Current','Welding current',3007,NULL,'FLOAT32','read',1,0,'A',0.1000000000000000055,0.0,0.0,350.0,1,1000,0.0,0,1,1,0.0,'welding','["welding", "current"]','{"welding_process": "MIG"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(13,5,'Zone1_Temperature','Production Zone 1 Temperature',1,NULL,'FLOAT32','read',1,0,'°C',1.0,0.0,-10.0,50.0,1,3000,0.0,0,1,1,0.0,'hvac','["temperature", "zone1"]','{"zone": "production_area_1"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(14,5,'Zone1_Humidity','Production Zone 1 Humidity',2,NULL,'FLOAT32','read',1,0,'%RH',1.0,0.0,0.0,100.0,1,3000,0.0,0,1,1,0.0,'hvac','["humidity", "zone1"]','{"zone": "production_area_1"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(15,6,'Active_Power','Active power consumption',1001,NULL,'FLOAT32','read',1,0,'kW',0.0100000000000000002,0.0,0.0,1000.0,1,2000,0.0,0,1,1,0.0,'energy','["power", "active"]','{"meter_type": "PM8000"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(16,6,'Reactive_Power','Reactive power',1003,NULL,'FLOAT32','read',1,0,'kVAR',0.0100000000000000002,0.0,-500.0,500.0,1,2000,0.0,0,1,1,0.0,'energy','["power", "reactive"]','{"meter_type": "PM8000"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
INSERT INTO data_points VALUES(17,6,'Power_Factor','Power factor',1005,NULL,'FLOAT32','read',1,0,'',0.00100000000000000002,0.0,0.0,1.0,1,5000,0.0,0,1,1,0.0,'energy','["power", "factor"]','{"meter_type": "PM8000"}',NULL,0,NULL,NULL,0.0,'standard',1,'2026-01-16 01:13:27','2026-01-16 01:13:27',0,NULL);
CREATE TABLE current_values (
    point_id INTEGER PRIMARY KEY,
    
    -- 🔥 실제 값 (DataVariant 직렬화)
    current_value TEXT,                                 -- JSON으로 DataVariant 저장 
    raw_value TEXT,                                     -- JSON으로 원시값 저장
    previous_value TEXT,                                -- JSON으로 이전값 저장
    
    -- 🔥 데이터 타입 정보
    value_type VARCHAR(10) DEFAULT 'double',            -- bool, int16, uint16, int32, uint32, float, double, string
    
    -- 🔥 데이터 품질 및 상태
    quality_code INTEGER DEFAULT 0,                    -- DataQuality enum 값
    quality VARCHAR(20) DEFAULT 'not_connected',       -- 텍스트 표현 (good, bad, uncertain, not_connected)
    
    -- 🔥 타임스탬프들
    value_timestamp DATETIME,                          -- 값 변경 시간
    quality_timestamp DATETIME,                        -- 품질 변경 시간  
    last_log_time DATETIME,                            -- 마지막 로깅 시간
    last_read_time DATETIME,                           -- 마지막 읽기 시간
    last_write_time DATETIME,                          -- 마지막 쓰기 시간
    
    -- 🔥 통계 카운터들
    read_count INTEGER DEFAULT 0,                      -- 읽기 횟수
    write_count INTEGER DEFAULT 0,                     -- 쓰기 횟수
    error_count INTEGER DEFAULT 0,                     -- 에러 횟수
    change_count INTEGER DEFAULT 0,                    -- 값 변경 횟수
    
    -- 🔥 알람 상태
    alarm_state VARCHAR(20) DEFAULT 'normal',          -- normal, high, low, critical
    alarm_active INTEGER DEFAULT 0,
    alarm_acknowledged INTEGER DEFAULT 0,
    
    -- 🔥 메타데이터
    source_info TEXT,                                  -- JSON: 값 소스 정보
    
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건 (대소문자 구분 없이 처리하도록 LOWER() 사용 및 FLOAT32/64 추가)
    CONSTRAINT chk_value_type CHECK (LOWER(value_type) IN ('bool', 'int8', 'uint8', 'int16', 'uint16', 'int32', 'uint32', 'int64', 'uint64', 'float', 'double', 'string', 'float32', 'float64', 'json')),
    CONSTRAINT chk_quality CHECK (LOWER(quality) IN ('good', 'bad', 'uncertain', 'not_connected', 'device_failure', 'sensor_failure', 'comm_failure', 'out_of_service', 'unknown', 'manual', 'simulated', 'stale')),
    CONSTRAINT chk_alarm_state CHECK (LOWER(alarm_state) IN ('normal', 'high', 'low', 'critical', 'warning', 'active', 'cleared', 'acknowledged'))

);
INSERT INTO current_values VALUES(1,'{"value":65602538.0}','{"value":65602538.0}',NULL,'double',1,'good','2026-01-18T12:33:35.316Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.316Z');
INSERT INTO current_values VALUES(2,'{"value":1.3753567303222189e-36}','{"value":1.3753567303222189e-36}',NULL,'double',1,'good','2026-01-18T12:33:35.318Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.318Z');
INSERT INTO current_values VALUES(3,'{"value":31.6}','{"value":31.6}',NULL,'double',1,'good','2026-01-18T12:33:35.321Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.322Z');
INSERT INTO current_values VALUES(4,'{"value":39.5}','{"value":39.5}',NULL,'double',1,'good','2026-01-18T12:33:35.327Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.327Z');
INSERT INTO current_values VALUES(5,'{"value":1005.0}','{"value":1005.0}',NULL,'double',1,'good','2026-01-18T12:33:35.329Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.329Z');
INSERT INTO current_values VALUES(6,'{"value":2001.0}','{"value":2001.0}',NULL,'double',1,'good','2026-01-18T12:33:35.292Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.292Z');
INSERT INTO current_values VALUES(7,'{"value":2002.0}','{"value":2002.0}',NULL,'double',1,'good','2026-01-18T12:33:35.295Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.295Z');
INSERT INTO current_values VALUES(8,'{"value":2003.0}','{"value":2003.0}',NULL,'double',1,'good','2026-01-18T12:33:35.297Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.297Z');
INSERT INTO current_values VALUES(9,'{"value":7.127705211253357e-32}','{"value":7.127705211253357e-32}',NULL,'double',1,'good','2026-01-18T12:33:35.299Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.299Z');
INSERT INTO current_values VALUES(10,'{"value":7.204743584523197e-32}','{"value":7.204743584523197e-32}',NULL,'double',1,'good','2026-01-18T12:33:35.301Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.301Z');
INSERT INTO current_values VALUES(11,'{"value":7.281781957793037e-32}','{"value":7.281781957793037e-32}',NULL,'double',1,'good','2026-01-18T12:33:35.304Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.304Z');
INSERT INTO current_values VALUES(12,'{"value":7.358820331062878e-32}','{"value":7.358820331062878e-32}',NULL,'double',1,'good','2026-01-18T12:33:35.306Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.306Z');
INSERT INTO current_values VALUES(13,'{"value": 22.3}','{"value": 22.3}',NULL,'float',0,'good','2026-01-16 01:10:27','2026-01-16 01:13:27','2026-01-16 01:13:27','2026-01-16 01:13:27','2026-01-16 01:13:27',42,0,0,0,'normal',0,0,NULL,'2026-01-16 01:13:27');
INSERT INTO current_values VALUES(14,'{"value": 58.2}','{"value": 58.2}',NULL,'float',0,'good','2026-01-16 01:11:27','2026-01-16 01:13:27','2026-01-16 01:13:27','2026-01-16 01:13:27','2026-01-16 01:13:27',38,0,0,0,'normal',0,0,NULL,'2026-01-16 01:13:27');
INSERT INTO current_values VALUES(15,'{"value":1.3695407811758852e-36}','{"value":1.3695407811758852e-36}',NULL,'double',1,'good','2026-01-18T12:33:35.308Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.308Z');
INSERT INTO current_values VALUES(16,'{"value":3.453125358119151e-38}','{"value":3.453125358119151e-38}',NULL,'double',1,'good','2026-01-18T12:33:35.311Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.311Z');
INSERT INTO current_values VALUES(17,'{"value":1.3930510269247378e-36}','{"value":1.3930510269247378e-36}',NULL,'double',1,'good','2026-01-18T12:33:35.313Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.313Z');
INSERT INTO current_values VALUES(20,'{"value":41.6}','{"value":41.6}',NULL,'double',1,'good','2026-01-18T12:33:35.336Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-01-18T12:33:35.337Z');
CREATE TABLE template_devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    model_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,                           -- 템플릿 명칭
    description TEXT,
    protocol_id INTEGER NOT NULL,
    config TEXT NOT NULL,                                -- 기본 프로토콜 설정 (JSON)
    polling_interval INTEGER DEFAULT 1000,
    timeout INTEGER DEFAULT 3000,
    is_public INTEGER DEFAULT 1,                         -- 시스템 공유 여부
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (model_id) REFERENCES device_models(id) ON DELETE CASCADE,
    FOREIGN KEY (protocol_id) REFERENCES protocols(id) ON DELETE RESTRICT
);
INSERT INTO template_devices VALUES(1,1,'XGT Standard Modbus TCP','Standard template for LS XGT PLC using Modbus TCP',1,'{"slave_id": 1, "byte_order": "big_endian"}',1000,3000,1,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO template_devices VALUES(2,2,'iS7 Inverter Basic','Basic monitoring and control for iS7 Inverter',1,'{"slave_id": 1, "byte_order": "big_endian"}',1000,3000,1,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO template_devices VALUES(3,3,'S7-1200 Standard','Standard monitoring for Siemens S7-1200',1,'{"slave_id": 1, "byte_order": "big_endian"}',1000,3000,1,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO template_devices VALUES(4,4,'PM8000 Basic','Basic energy monitoring for Schneider PM8000',1,'{"slave_id": 1, "byte_order": "big_endian"}',1000,3000,1,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27');
CREATE TABLE template_device_settings (
    template_device_id INTEGER PRIMARY KEY,
    polling_interval_ms INTEGER DEFAULT 1000,
    connection_timeout_ms INTEGER DEFAULT 10000,
    read_timeout_ms INTEGER DEFAULT 5000,
    write_timeout_ms INTEGER DEFAULT 5000,
    max_retry_count INTEGER DEFAULT 3,
    FOREIGN KEY (template_device_id) REFERENCES template_devices(id) ON DELETE CASCADE
);
INSERT INTO template_device_settings VALUES(1,1000,10000,5000,5000,3);
INSERT INTO template_device_settings VALUES(2,1000,10000,5000,5000,3);
INSERT INTO template_device_settings VALUES(3,1000,10000,5000,5000,3);
INSERT INTO template_device_settings VALUES(4,5000,10000,5000,5000,3);
CREATE TABLE template_data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    template_device_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    address INTEGER NOT NULL,
    address_string VARCHAR(255),
    data_type VARCHAR(20) NOT NULL,
    access_mode VARCHAR(10) DEFAULT 'read',
    unit VARCHAR(50),
    scaling_factor REAL DEFAULT 1.0,
    is_writable INTEGER DEFAULT 0,
    is_active INTEGER DEFAULT 1,
    sort_order INTEGER DEFAULT 0,
    metadata TEXT,                                       -- JSON 형태
    FOREIGN KEY (template_device_id) REFERENCES template_devices(id) ON DELETE CASCADE
);
INSERT INTO template_data_points VALUES(1,1,'CPU_Status',NULL,0,NULL,'UINT16','read','',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(2,1,'Run_Stop',NULL,1,NULL,'BOOL','read','',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(3,1,'Error_Code',NULL,2,NULL,'UINT16','read','',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(4,1,'Input_W0',NULL,100,NULL,'UINT16','read','',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(5,1,'Output_W0',NULL,200,NULL,'UINT16','read_write','',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(6,2,'Frequency',NULL,1,NULL,'FLOAT32','read','Hz',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(7,2,'Output_Current',NULL,2,NULL,'FLOAT32','read','A',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(8,2,'Output_Voltage',NULL,3,NULL,'FLOAT32','read','V',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(9,2,'DC_Link_Voltage',NULL,4,NULL,'FLOAT32','read','V',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(10,2,'Status_Word',NULL,10,NULL,'UINT16','read','',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(11,2,'Control_Word',NULL,11,NULL,'UINT16','read_write','',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(12,3,'Status',NULL,0,NULL,'UINT16','read','',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(13,3,'Run_Mode',NULL,1,NULL,'BOOL','read','',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(14,3,'Process_Value',NULL,10,NULL,'FLOAT32','read','',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(15,3,'Setpoint',NULL,12,NULL,'FLOAT32','read_write','',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(16,4,'Voltage_A-N',NULL,100,NULL,'FLOAT32','read','V',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(17,4,'Voltage_B-N',NULL,102,NULL,'FLOAT32','read','V',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(18,4,'Voltage_C-N',NULL,104,NULL,'FLOAT32','read','V',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(19,4,'Current_A',NULL,110,NULL,'FLOAT32','read','A',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(20,4,'Current_B',NULL,112,NULL,'FLOAT32','read','A',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(21,4,'Current_C',NULL,114,NULL,'FLOAT32','read','A',1.0,0,1,0,NULL);
INSERT INTO template_data_points VALUES(22,4,'Total_Active_Power',NULL,120,NULL,'FLOAT32','read','kW',1.0,0,1,0,NULL);
CREATE TABLE alarm_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 기본 알람 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 대상 정보 (현재 DB와 일치)
    target_type VARCHAR(20) NOT NULL,               -- 'data_point', 'virtual_point', 'group'
    target_id INTEGER,
    target_group VARCHAR(100),
    
    -- 알람 타입
    alarm_type VARCHAR(20) NOT NULL,                -- 'analog', 'digital', 'script'
    
    -- 아날로그 알람 설정
    high_high_limit REAL,
    high_limit REAL,
    low_limit REAL,
    low_low_limit REAL,
    deadband REAL DEFAULT 0,
    rate_of_change REAL DEFAULT 0,                  -- 변화율 임계값
    
    -- 디지털 알람 설정
    trigger_condition VARCHAR(20),                  -- 'on_true', 'on_false', 'on_change', 'on_rising', 'on_falling'
    
    -- 스크립트 기반 알람
    condition_script TEXT,                          -- JavaScript 조건식
    message_script TEXT,                            -- JavaScript 메시지 생성
    
    -- 메시지 커스터마이징
    message_config TEXT,                            -- JSON 형태
    message_template TEXT,                          -- 기본 메시지 템플릿
    
    -- 우선순위
    severity VARCHAR(20) DEFAULT 'medium',          -- 'critical', 'high', 'medium', 'low', 'info'
    priority INTEGER DEFAULT 100,
    
    -- 자동 처리
    auto_acknowledge INTEGER DEFAULT 0,
    acknowledge_timeout_min INTEGER DEFAULT 0,
    auto_clear INTEGER DEFAULT 1,
    
    -- 억제 규칙
    suppression_rules TEXT,                         -- JSON 형태
    
    -- 알림 설정
    notification_enabled INTEGER DEFAULT 1,
    notification_delay_sec INTEGER DEFAULT 0,
    notification_repeat_interval_min INTEGER DEFAULT 0,
    notification_channels TEXT,                     -- JSON 배열 ['email', 'sms', 'mq', 'webhook']
    notification_recipients TEXT,                   -- JSON 배열
    
    -- 상태
    is_enabled INTEGER DEFAULT 1,
    is_latched INTEGER DEFAULT 0,                   -- 래치 알람 (수동 리셋 필요)
    
    -- 타임스탬프
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER, 
    template_id INTEGER, 
    rule_group VARCHAR(36), 
    created_by_template INTEGER DEFAULT 0, 
    last_template_update DATETIME, 
    
    -- 에스컬레이션 설정 (현재 DB에 추가된 컬럼들)
    escalation_enabled INTEGER DEFAULT 0,
    escalation_max_level INTEGER DEFAULT 3,
    escalation_rules TEXT DEFAULT NULL,             -- JSON 형태 에스컬레이션 규칙
    
    -- 분류 및 태깅 시스템
    category VARCHAR(50) DEFAULT NULL,              -- 'process', 'system', 'safety', 'custom', 'general'
    tags TEXT DEFAULT NULL, is_deleted BOOLEAN DEFAULT 0,                         -- JSON 배열 형태 ['tag1', 'tag2', 'tag3']
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id)
);
INSERT INTO alarm_rules VALUES(1,1,'Temperature_High_Alarm','PLC 온도 과열 알람','data_point',4,'','analog',NULL,35.0,15.0,NULL,2.0,0.0,'','',NULL,NULL,'온도 알람: {value}°C (임계값: {limit}°C)','high',100,0,0,1,NULL,1,0,0,NULL,NULL,0,0,'2026-01-16 01:13:27','2026-01-17 03:49:01',NULL,NULL,NULL,0,NULL,0,3,NULL,'process','["temperature","plc","production"]',0);
INSERT INTO alarm_rules VALUES(2,1,'Motor_Current_Overload','모터 전류 과부하 알람','data_point',3,NULL,'analog',NULL,30.0,NULL,NULL,1.0,0.0,NULL,NULL,NULL,NULL,'모터 과부하: {value}A (한계: {limit}A)','critical',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-01-16 01:13:27','2026-01-16 01:13:27',NULL,NULL,NULL,0,NULL,0,3,NULL,'process','["current", "motor", "safety"]',0);
INSERT INTO alarm_rules VALUES(3,1,'Emergency_Stop_Active','비상정지 버튼 활성화 알람','data_point',5,NULL,'digital',NULL,NULL,NULL,NULL,0.0,0.0,NULL,NULL,NULL,NULL,'🚨 비상정지 활성화됨!','critical',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-01-16 01:13:27','2026-01-16 01:13:27',NULL,NULL,NULL,0,NULL,0,3,NULL,'safety','["emergency", "stop", "critical"]',0);
INSERT INTO alarm_rules VALUES(4,1,'HVAC_Zone1_Temperature','HVAC 구역1 온도 알람','data_point',13,NULL,'analog',NULL,28.0,18.0,NULL,1.5,0.0,NULL,NULL,NULL,NULL,'Zone1 온도 이상: {value}°C','medium',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-01-16 01:13:27','2026-01-17 04:33:11',NULL,NULL,NULL,0,NULL,0,3,NULL,'hvac','["temperature", "zone1", "hvac"]',0);
INSERT INTO alarm_rules VALUES(9,1,'E2E-Verify-Template-1768632082540 (4)','Created by Antigravity for E2E verification','data_point',4,NULL,'threshold',NULL,99.5,10.0,NULL,0.0,0.0,NULL,NULL,NULL,NULL,'E2E VERIFY: {device_name} value is {value}','high',100,0,0,0,NULL,1,0,0,NULL,NULL,1,0,'2026-01-17 06:41:22','2026-01-17 06:41:22',1,8,'E2E_VERIFICATION_GROUP',1,NULL,0,3,NULL,'test','null',0);
INSERT INTO alarm_rules VALUES(10,1,'E2E_Test_Rule_1768715940629',NULL,'virtual_point',17,NULL,'analog',NULL,40.0,NULL,NULL,0.0,0.0,NULL,NULL,NULL,NULL,NULL,'high',100,0,0,0,NULL,0,0,0,NULL,NULL,1,0,'2026-01-18 05:59:00','2026-01-18 05:59:00',1,NULL,NULL,0,NULL,0,3,NULL,NULL,'[]',0);
INSERT INTO alarm_rules VALUES(11,1,'E2E_Test_Rule_1768715997716',NULL,'virtual_point',18,NULL,'analog',NULL,40.0,NULL,NULL,0.0,0.0,NULL,NULL,NULL,NULL,NULL,'high',100,0,0,0,NULL,0,0,0,NULL,NULL,1,0,'2026-01-18 05:59:57','2026-01-18 05:59:57',1,NULL,NULL,0,NULL,0,3,NULL,NULL,'[]',0);
INSERT INTO alarm_rules VALUES(12,1,'E2E_Test_Rule_1768716013871',NULL,'virtual_point',19,NULL,'analog',NULL,40.0,NULL,NULL,0.0,0.0,NULL,NULL,NULL,NULL,NULL,'high',100,0,0,0,NULL,0,0,0,NULL,NULL,1,0,'2026-01-18 06:00:13','2026-01-18 06:00:13',1,NULL,NULL,0,NULL,0,3,NULL,NULL,'[]',0);
INSERT INTO alarm_rules VALUES(13,1,'E2E_Test_Rule_1768716099440',NULL,'virtual_point',20,NULL,'analog',NULL,40.0,NULL,NULL,0.0,0.0,NULL,NULL,NULL,NULL,NULL,'high',100,0,0,0,NULL,0,0,0,NULL,NULL,1,0,'2026-01-18 06:01:39','2026-01-18 06:01:39',1,NULL,NULL,0,NULL,0,3,NULL,NULL,'[]',0);
INSERT INTO alarm_rules VALUES(14,1,'E2E_Test_Rule_1768728571641',NULL,'virtual_point',21,NULL,'analog',NULL,40.0,NULL,NULL,0.0,0.0,NULL,NULL,NULL,NULL,NULL,'high',100,0,0,0,NULL,0,0,0,NULL,NULL,1,0,'2026-01-18 09:29:31','2026-01-18 09:29:31',1,NULL,NULL,0,NULL,0,3,NULL,NULL,'[]',0);
CREATE TABLE alarm_occurrences (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_id INTEGER NOT NULL,
    tenant_id INTEGER NOT NULL,
    
    -- 발생 정보
    occurrence_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    trigger_value TEXT,
    trigger_condition TEXT,
    alarm_message TEXT,
    severity VARCHAR(20),
    
    -- 알람 상태
    state VARCHAR(20) DEFAULT 'active',             -- 'active', 'acknowledged', 'cleared'
    
    -- Acknowledge 정보
    acknowledged_time DATETIME,
    acknowledged_by INTEGER,
    acknowledge_comment TEXT,
    
    -- Clear 정보 (cleared_by 필드 추가!)
    cleared_time DATETIME,
    cleared_value TEXT,
    clear_comment TEXT,
    cleared_by INTEGER,                             -- ⭐ 누락된 필드 추가!
    
    -- 알림 정보
    notification_sent INTEGER DEFAULT 0,
    notification_time DATETIME,
    notification_count INTEGER DEFAULT 0,
    notification_result TEXT,
    
    -- 추가 컨텍스트
    context_data TEXT,
    source_name VARCHAR(100),
    location VARCHAR(200),
    
    -- 타임스탬프
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 디바이스/포인트 정보
    device_id INTEGER,                              -- 정수형
    point_id INTEGER,
    
    -- 분류 및 태깅 시스템
    category VARCHAR(50) DEFAULT NULL,
    tags TEXT DEFAULT NULL,
    
    FOREIGN KEY (rule_id) REFERENCES alarm_rules(id) ON DELETE CASCADE,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (acknowledged_by) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY (cleared_by) REFERENCES users(id) ON DELETE SET NULL      -- ⭐ 추가!
);
INSERT INTO alarm_occurrences VALUES(1,5,1,'2026-01-16 02:28:11','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 02:28:11','2026-01-16 02:28:11',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(2,1,1,'2026-01-16 02:28:11','3.618600209616183e-38','LOW','Temperature_High_Alarm: LOW (Value: 0.00)','HIGH','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 02:28:11','2026-01-16 02:28:11',1,4,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(3,3,1,'2026-01-16 02:28:12','1005.0',NULL,'Emergency_Stop_Active:  (Value: 1005.00)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 02:28:12','2026-01-16 02:28:12',1,5,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(4,5,1,'2026-01-16 03:14:30','1.3753561025405069e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:14:30','2026-01-16 03:14:30',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(5,1,1,'2026-01-16 03:14:30','4.536955171196095e-38','LOW','Temperature_High_Alarm: LOW (Value: 0.00)','HIGH','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:14:30','2026-01-16 03:14:30',1,4,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(6,3,1,'2026-01-16 03:14:30','1005.0',NULL,'Emergency_Stop_Active:  (Value: 1005.00)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:14:30','2026-01-16 03:14:30',1,5,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(7,5,1,'2026-01-16 03:20:50','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:20:50','2026-01-16 03:20:50',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(8,3,1,'2026-01-16 03:20:50','1005.0',NULL,'Emergency_Stop_Active:  (Value: 1005.00)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:20:50','2026-01-16 03:20:50',1,5,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(9,1,1,'2026-01-16 03:21:15','35.0','HIGH','Temperature_High_Alarm: HIGH (Value: 35.00)','HIGH','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:21:15','2026-01-16 03:21:15',1,4,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(10,2,1,'2026-01-16 03:21:17','30.1','HIGH','Motor_Current_Overload: HIGH (Value: 30.10)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:21:17','2026-01-16 03:21:17',1,3,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(11,5,1,'2026-01-16 03:22:01','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:01','2026-01-16 03:22:01',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(12,5,1,'2026-01-16 03:22:02','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:02','2026-01-16 03:22:02',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(13,5,1,'2026-01-16 03:22:03','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:03','2026-01-16 03:22:03',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(14,5,1,'2026-01-16 03:22:04','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:04','2026-01-16 03:22:04',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(15,5,1,'2026-01-16 03:22:05','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:05','2026-01-16 03:22:05',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(16,5,1,'2026-01-16 03:22:06','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:06','2026-01-16 03:22:06',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(17,5,1,'2026-01-16 03:22:07','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:07','2026-01-16 03:22:07',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(18,5,1,'2026-01-16 03:22:08','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:08','2026-01-16 03:22:08',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(19,5,1,'2026-01-16 03:22:09','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:09','2026-01-16 03:22:09',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(20,5,1,'2026-01-16 03:22:10','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:10','2026-01-16 03:22:10',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(21,5,1,'2026-01-16 03:22:11','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:11','2026-01-16 03:22:11',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(22,5,1,'2026-01-16 03:22:12','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:12','2026-01-16 03:22:12',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(23,5,1,'2026-01-16 03:22:13','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:13','2026-01-16 03:22:13',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(24,5,1,'2026-01-16 03:22:14','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:14','2026-01-16 03:22:14',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(25,5,1,'2026-01-16 03:22:15','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:15','2026-01-16 03:22:15',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(26,5,1,'2026-01-16 03:22:16','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:16','2026-01-16 03:22:16',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(27,5,1,'2026-01-16 03:22:17','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:17','2026-01-16 03:22:17',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(28,5,1,'2026-01-16 03:22:18','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:18','2026-01-16 03:22:18',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(29,5,1,'2026-01-16 03:22:19','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:19','2026-01-16 03:22:19',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(30,5,1,'2026-01-16 03:22:20','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:20','2026-01-16 03:22:20',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(31,5,1,'2026-01-16 03:22:21','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:21','2026-01-16 03:22:21',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(32,5,1,'2026-01-16 03:22:22','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:22','2026-01-16 03:22:22',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(33,5,1,'2026-01-16 03:22:23','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:23','2026-01-16 03:22:23',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(34,5,1,'2026-01-16 03:22:24','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:24','2026-01-16 03:22:24',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(35,5,1,'2026-01-16 03:22:25','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:25','2026-01-16 03:22:25',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(36,5,1,'2026-01-16 03:22:26','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:26','2026-01-16 03:22:26',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(37,5,1,'2026-01-16 03:22:27','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:27','2026-01-16 03:22:27',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(38,5,1,'2026-01-16 03:22:28','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:28','2026-01-16 03:22:28',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(39,5,1,'2026-01-16 03:22:29','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:29','2026-01-16 03:22:29',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(40,5,1,'2026-01-16 03:22:30','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:30','2026-01-16 03:22:30',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(41,5,1,'2026-01-16 03:22:31','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:31','2026-01-16 03:22:31',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(42,5,1,'2026-01-16 03:22:32','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:32','2026-01-16 03:22:32',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(43,5,1,'2026-01-16 03:22:33','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:33','2026-01-16 03:22:33',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(44,5,1,'2026-01-16 03:22:34','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:34','2026-01-16 03:22:34',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(45,5,1,'2026-01-16 03:22:35','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:35','2026-01-16 03:22:35',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(46,5,1,'2026-01-16 03:22:36','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:36','2026-01-16 03:22:36',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(47,5,1,'2026-01-16 03:22:37','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:37','2026-01-16 03:22:37',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(48,5,1,'2026-01-16 03:22:38','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:38','2026-01-16 03:22:38',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(49,5,1,'2026-01-16 03:22:39','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:39','2026-01-16 03:22:39',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(50,5,1,'2026-01-16 03:22:40','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:40','2026-01-16 03:22:40',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(51,5,1,'2026-01-16 03:22:41','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:41','2026-01-16 03:22:41',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(52,5,1,'2026-01-16 03:22:42','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:42','2026-01-16 03:22:42',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(53,5,1,'2026-01-16 03:22:43','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:43','2026-01-16 03:22:43',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(54,5,1,'2026-01-16 03:22:44','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:44','2026-01-16 03:22:44',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(55,5,1,'2026-01-16 03:22:45','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:45','2026-01-16 03:22:45',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(56,5,1,'2026-01-16 03:22:46','1.3753553850756931e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:22:46','2026-01-16 03:22:46',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(57,5,1,'2026-01-16 03:23:28','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:28','2026-01-16 03:23:28',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(58,5,1,'2026-01-16 03:23:29','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:29','2026-01-16 03:23:29',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(59,5,1,'2026-01-16 03:23:30','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:30','2026-01-16 03:23:30',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(60,5,1,'2026-01-16 03:23:31','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:31','2026-01-16 03:23:31',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(61,5,1,'2026-01-16 03:23:32','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:32','2026-01-16 03:23:32',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(62,5,1,'2026-01-16 03:23:33','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:33','2026-01-16 03:23:33',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(63,5,1,'2026-01-16 03:23:34','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:34','2026-01-16 03:23:34',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(64,5,1,'2026-01-16 03:23:35','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:35','2026-01-16 03:23:35',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(65,5,1,'2026-01-16 03:23:36','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:36','2026-01-16 03:23:36',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(66,5,1,'2026-01-16 03:23:37','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:37','2026-01-16 03:23:37',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(67,5,1,'2026-01-16 03:23:38','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:38','2026-01-16 03:23:38',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(68,5,1,'2026-01-16 03:23:39','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:39','2026-01-16 03:23:39',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(69,5,1,'2026-01-16 03:23:40','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:40','2026-01-16 03:23:40',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(70,5,1,'2026-01-16 03:23:41','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:41','2026-01-16 03:23:41',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(71,5,1,'2026-01-16 03:23:42','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:42','2026-01-16 03:23:42',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(72,5,1,'2026-01-16 03:23:44','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:44','2026-01-16 03:23:44',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(73,5,1,'2026-01-16 03:23:45','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:45','2026-01-16 03:23:45',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(74,5,1,'2026-01-16 03:23:46','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:46','2026-01-16 03:23:46',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(75,5,1,'2026-01-16 03:23:47','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:47','2026-01-16 03:23:47',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(76,5,1,'2026-01-16 03:23:48','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:48','2026-01-16 03:23:48',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(77,5,1,'2026-01-16 03:23:49','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:49','2026-01-16 03:23:49',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(78,5,1,'2026-01-16 03:23:50','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:50','2026-01-16 03:23:50',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(79,5,1,'2026-01-16 03:23:51','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:51','2026-01-16 03:23:51',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(80,5,1,'2026-01-16 03:23:52','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:52','2026-01-16 03:23:52',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(81,5,1,'2026-01-16 03:23:53','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:53','2026-01-16 03:23:53',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(82,5,1,'2026-01-16 03:23:54','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:54','2026-01-16 03:23:54',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(83,5,1,'2026-01-16 03:23:55','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:55','2026-01-16 03:23:55',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(84,5,1,'2026-01-16 03:23:56','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:56','2026-01-16 03:23:56',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(85,5,1,'2026-01-16 03:23:57','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:57','2026-01-16 03:23:57',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(86,5,1,'2026-01-16 03:23:58','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:58','2026-01-16 03:23:58',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(87,5,1,'2026-01-16 03:23:59','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:23:59','2026-01-16 03:23:59',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(88,5,1,'2026-01-16 03:24:00','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:00','2026-01-16 03:24:00',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(89,5,1,'2026-01-16 03:24:01','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:01','2026-01-16 03:24:01',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(90,5,1,'2026-01-16 03:24:02','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:02','2026-01-16 03:24:02',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(91,5,1,'2026-01-16 03:24:03','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:03','2026-01-16 03:24:03',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(92,5,1,'2026-01-16 03:24:04','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:04','2026-01-16 03:24:04',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(93,5,1,'2026-01-16 03:24:05','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:05','2026-01-16 03:24:05',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(94,5,1,'2026-01-16 03:24:06','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:06','2026-01-16 03:24:06',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(95,5,1,'2026-01-16 03:24:07','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:07','2026-01-16 03:24:07',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(96,5,1,'2026-01-16 03:24:08','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:08','2026-01-16 03:24:08',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(97,5,1,'2026-01-16 03:24:09','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:09','2026-01-16 03:24:09',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(98,5,1,'2026-01-16 03:24:10','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:10','2026-01-16 03:24:10',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(99,5,1,'2026-01-16 03:24:11','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:11','2026-01-16 03:24:11',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(100,5,1,'2026-01-16 03:24:12','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:12','2026-01-16 03:24:12',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(101,5,1,'2026-01-16 03:24:13','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:13','2026-01-16 03:24:13',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(102,5,1,'2026-01-16 03:24:14','1.3753553850756931e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:14','2026-01-16 03:24:14',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(103,5,1,'2026-01-16 03:24:57','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:57','2026-01-16 03:24:57',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(104,5,1,'2026-01-16 03:24:58','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:58','2026-01-16 03:24:58',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(105,5,1,'2026-01-16 03:24:59','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:24:59','2026-01-16 03:24:59',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(106,5,1,'2026-01-16 03:25:00','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:00','2026-01-16 03:25:00',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(107,5,1,'2026-01-16 03:25:01','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:01','2026-01-16 03:25:01',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(108,5,1,'2026-01-16 03:25:02','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:02','2026-01-16 03:25:02',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(109,5,1,'2026-01-16 03:25:03','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:03','2026-01-16 03:25:03',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(110,5,1,'2026-01-16 03:25:04','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:04','2026-01-16 03:25:04',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(111,5,1,'2026-01-16 03:25:05','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:05','2026-01-16 03:25:05',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(112,5,1,'2026-01-16 03:25:06','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:06','2026-01-16 03:25:06',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(113,5,1,'2026-01-16 03:25:07','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:07','2026-01-16 03:25:07',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(114,5,1,'2026-01-16 03:25:08','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:08','2026-01-16 03:25:08',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(115,5,1,'2026-01-16 03:25:09','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:09','2026-01-16 03:25:09',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(116,5,1,'2026-01-16 03:25:10','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:10','2026-01-16 03:25:10',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(117,5,1,'2026-01-16 03:25:11','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:11','2026-01-16 03:25:11',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(118,5,1,'2026-01-16 03:25:12','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:12','2026-01-16 03:25:12',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(119,5,1,'2026-01-16 03:25:13','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:13','2026-01-16 03:25:13',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(120,5,1,'2026-01-16 03:25:14','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:14','2026-01-16 03:25:14',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(121,5,1,'2026-01-16 03:25:15','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:15','2026-01-16 03:25:15',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(122,5,1,'2026-01-16 03:25:16','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:16','2026-01-16 03:25:16',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(123,5,1,'2026-01-16 03:25:17','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:17','2026-01-16 03:25:17',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(124,5,1,'2026-01-16 03:25:18','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:18','2026-01-16 03:25:18',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(125,5,1,'2026-01-16 03:25:19','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:19','2026-01-16 03:25:19',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(126,5,1,'2026-01-16 03:25:20','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:20','2026-01-16 03:25:20',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(127,5,1,'2026-01-16 03:25:21','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:21','2026-01-16 03:25:21',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(128,5,1,'2026-01-16 03:25:22','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:22','2026-01-16 03:25:22',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(129,5,1,'2026-01-16 03:25:23','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:23','2026-01-16 03:25:23',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(130,5,1,'2026-01-16 03:25:24','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:24','2026-01-16 03:25:24',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(131,5,1,'2026-01-16 03:25:25','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:25','2026-01-16 03:25:25',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(132,5,1,'2026-01-16 03:25:26','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:26','2026-01-16 03:25:26',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(133,5,1,'2026-01-16 03:25:28','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:28','2026-01-16 03:25:28',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(134,5,1,'2026-01-16 03:25:29','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:29','2026-01-16 03:25:29',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(135,5,1,'2026-01-16 03:25:30','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:30','2026-01-16 03:25:30',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(136,5,1,'2026-01-16 03:25:31','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:31','2026-01-16 03:25:31',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(137,5,1,'2026-01-16 03:25:32','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:32','2026-01-16 03:25:32',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(138,5,1,'2026-01-16 03:25:33','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:33','2026-01-16 03:25:33',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(139,5,1,'2026-01-16 03:25:34','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:34','2026-01-16 03:25:34',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(140,5,1,'2026-01-16 03:25:35','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:35','2026-01-16 03:25:35',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(141,5,1,'2026-01-16 03:25:36','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:36','2026-01-16 03:25:36',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(142,5,1,'2026-01-16 03:25:38','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:38','2026-01-16 03:25:38',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(143,5,1,'2026-01-16 03:25:39','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:39','2026-01-16 03:25:39',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(144,5,1,'2026-01-16 03:25:40','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:40','2026-01-16 03:25:40',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(145,5,1,'2026-01-16 03:25:41','1.3753553850756931e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:25:41','2026-01-16 03:25:41',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(146,5,1,'2026-01-16 03:26:25','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:26:25','2026-01-16 03:26:25',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(147,5,1,'2026-01-16 03:26:26','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:26:26','2026-01-16 03:26:26',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(148,5,1,'2026-01-16 03:26:27','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:26:27','2026-01-16 03:26:27',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(149,5,1,'2026-01-16 03:26:28','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:26:28','2026-01-16 03:26:28',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(150,5,1,'2026-01-16 03:26:29','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:26:29','2026-01-16 03:26:29',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(151,5,1,'2026-01-16 03:26:30','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:26:30','2026-01-16 03:26:30',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(152,5,1,'2026-01-16 03:26:31','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:26:31','2026-01-16 03:26:31',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(153,5,1,'2026-01-16 03:26:32','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:26:32','2026-01-16 03:26:32',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(154,5,1,'2026-01-16 03:26:33','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:26:33','2026-01-16 03:26:33',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(155,5,1,'2026-01-16 03:26:34','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:26:34','2026-01-16 03:26:34',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(156,5,1,'2026-01-16 03:26:35','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:26:35','2026-01-16 03:26:35',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(157,5,1,'2026-01-16 03:26:36','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:26:36','2026-01-16 03:26:36',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(158,5,1,'2026-01-16 03:26:37','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:26:37','2026-01-16 03:26:37',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(159,5,1,'2026-01-16 03:28:07','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:28:07','2026-01-16 03:28:07',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(160,3,1,'2026-01-16 03:28:07','1005.0',NULL,'Emergency_Stop_Active:  (Value: 1005.00)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:28:07','2026-01-16 03:28:07',1,5,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(161,1,1,'2026-01-16 03:28:36','35.0','HIGH','Temperature_High_Alarm: HIGH (Value: 35.00)','HIGH','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:28:36','2026-01-16 03:28:36',1,4,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(162,2,1,'2026-01-16 03:28:38','30.1','HIGH','Motor_Current_Overload: HIGH (Value: 30.10)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:28:38','2026-01-16 03:28:38',1,3,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(163,5,1,'2026-01-16 03:29:21','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:21','2026-01-16 03:29:21',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(164,5,1,'2026-01-16 03:29:22','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:22','2026-01-16 03:29:22',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(165,5,1,'2026-01-16 03:29:24','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:24','2026-01-16 03:29:24',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(166,5,1,'2026-01-16 03:29:25','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:25','2026-01-16 03:29:25',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(167,5,1,'2026-01-16 03:29:26','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:26','2026-01-16 03:29:26',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(168,5,1,'2026-01-16 03:29:27','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:27','2026-01-16 03:29:27',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(169,5,1,'2026-01-16 03:29:28','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:28','2026-01-16 03:29:28',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(170,5,1,'2026-01-16 03:29:29','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:29','2026-01-16 03:29:29',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(171,5,1,'2026-01-16 03:29:30','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:30','2026-01-16 03:29:30',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(172,5,1,'2026-01-16 03:29:31','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:31','2026-01-16 03:29:31',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(173,5,1,'2026-01-16 03:29:32','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:32','2026-01-16 03:29:32',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(174,5,1,'2026-01-16 03:29:33','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:33','2026-01-16 03:29:33',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(175,5,1,'2026-01-16 03:29:34','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:34','2026-01-16 03:29:34',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(176,5,1,'2026-01-16 03:29:35','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:35','2026-01-16 03:29:35',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(177,5,1,'2026-01-16 03:29:36','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:36','2026-01-16 03:29:36',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(178,5,1,'2026-01-16 03:29:37','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:37','2026-01-16 03:29:37',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(179,5,1,'2026-01-16 03:29:38','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:38','2026-01-16 03:29:38',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(180,5,1,'2026-01-16 03:29:39','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:39','2026-01-16 03:29:39',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(181,5,1,'2026-01-16 03:29:40','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:40','2026-01-16 03:29:40',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(182,5,1,'2026-01-16 03:29:41','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:41','2026-01-16 03:29:41',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(183,5,1,'2026-01-16 03:29:42','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:42','2026-01-16 03:29:42',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(184,5,1,'2026-01-16 03:29:43','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:43','2026-01-16 03:29:43',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(185,5,1,'2026-01-16 03:29:44','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:44','2026-01-16 03:29:44',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(186,5,1,'2026-01-16 03:29:45','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:45','2026-01-16 03:29:45',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(187,5,1,'2026-01-16 03:29:46','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:46','2026-01-16 03:29:46',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(188,5,1,'2026-01-16 03:29:47','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:47','2026-01-16 03:29:47',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(189,5,1,'2026-01-16 03:29:48','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:48','2026-01-16 03:29:48',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(190,5,1,'2026-01-16 03:29:49','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:49','2026-01-16 03:29:49',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(191,5,1,'2026-01-16 03:29:50','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:50','2026-01-16 03:29:50',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(192,5,1,'2026-01-16 03:29:51','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:51','2026-01-16 03:29:51',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(193,5,1,'2026-01-16 03:29:52','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:52','2026-01-16 03:29:52',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(194,5,1,'2026-01-16 03:29:53','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:53','2026-01-16 03:29:53',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(195,5,1,'2026-01-16 03:29:54','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:54','2026-01-16 03:29:54',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(196,5,1,'2026-01-16 03:29:55','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:55','2026-01-16 03:29:55',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(197,5,1,'2026-01-16 03:29:56','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:56','2026-01-16 03:29:56',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(198,5,1,'2026-01-16 03:29:57','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:57','2026-01-16 03:29:57',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(199,5,1,'2026-01-16 03:29:58','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:58','2026-01-16 03:29:58',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(200,5,1,'2026-01-16 03:29:59','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:29:59','2026-01-16 03:29:59',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(201,5,1,'2026-01-16 03:30:00','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:00','2026-01-16 03:30:00',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(202,5,1,'2026-01-16 03:30:01','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:01','2026-01-16 03:30:01',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(203,5,1,'2026-01-16 03:30:02','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:02','2026-01-16 03:30:02',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(204,5,1,'2026-01-16 03:30:03','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:03','2026-01-16 03:30:03',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(205,5,1,'2026-01-16 03:30:04','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:04','2026-01-16 03:30:04',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(206,5,1,'2026-01-16 03:30:05','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:05','2026-01-16 03:30:05',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(207,5,1,'2026-01-16 03:30:06','1.3753553850756931e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:06','2026-01-16 03:30:06',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(208,5,1,'2026-01-16 03:30:49','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:49','2026-01-16 03:30:49',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(209,5,1,'2026-01-16 03:30:50','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:50','2026-01-16 03:30:50',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(210,5,1,'2026-01-16 03:30:51','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:51','2026-01-16 03:30:51',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(211,5,1,'2026-01-16 03:30:52','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:52','2026-01-16 03:30:52',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(212,5,1,'2026-01-16 03:30:53','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:53','2026-01-16 03:30:53',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(213,5,1,'2026-01-16 03:30:54','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:54','2026-01-16 03:30:54',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(214,5,1,'2026-01-16 03:30:55','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:55','2026-01-16 03:30:55',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(215,5,1,'2026-01-16 03:30:56','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:56','2026-01-16 03:30:56',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(216,5,1,'2026-01-16 03:30:57','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:57','2026-01-16 03:30:57',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(217,5,1,'2026-01-16 03:30:58','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:58','2026-01-16 03:30:58',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(218,5,1,'2026-01-16 03:30:59','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:30:59','2026-01-16 03:30:59',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(219,5,1,'2026-01-16 03:31:00','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:00','2026-01-16 03:31:00',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(220,5,1,'2026-01-16 03:31:01','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:01','2026-01-16 03:31:01',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(221,5,1,'2026-01-16 03:31:02','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:02','2026-01-16 03:31:02',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(222,5,1,'2026-01-16 03:31:03','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:03','2026-01-16 03:31:03',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(223,5,1,'2026-01-16 03:31:04','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:04','2026-01-16 03:31:04',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(224,5,1,'2026-01-16 03:31:05','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:05','2026-01-16 03:31:05',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(225,5,1,'2026-01-16 03:31:06','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:06','2026-01-16 03:31:06',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(226,5,1,'2026-01-16 03:31:07','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:07','2026-01-16 03:31:07',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(227,5,1,'2026-01-16 03:31:08','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:08','2026-01-16 03:31:08',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(228,5,1,'2026-01-16 03:31:09','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:09','2026-01-16 03:31:09',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(229,5,1,'2026-01-16 03:31:10','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:10','2026-01-16 03:31:10',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(230,5,1,'2026-01-16 03:31:11','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:11','2026-01-16 03:31:11',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(231,5,1,'2026-01-16 03:31:12','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:12','2026-01-16 03:31:12',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(232,5,1,'2026-01-16 03:31:13','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:13','2026-01-16 03:31:13',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(233,5,1,'2026-01-16 03:31:14','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:14','2026-01-16 03:31:14',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(234,5,1,'2026-01-16 03:31:15','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:15','2026-01-16 03:31:15',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(235,5,1,'2026-01-16 03:31:16','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:16','2026-01-16 03:31:16',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(236,5,1,'2026-01-16 03:31:17','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:17','2026-01-16 03:31:17',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(237,5,1,'2026-01-16 03:31:18','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:18','2026-01-16 03:31:18',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(238,5,1,'2026-01-16 03:31:20','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:20','2026-01-16 03:31:20',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(239,5,1,'2026-01-16 03:31:21','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:21','2026-01-16 03:31:21',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(240,5,1,'2026-01-16 03:31:23','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:23','2026-01-16 03:31:23',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(241,5,1,'2026-01-16 03:31:24','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:24','2026-01-16 03:31:24',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(242,5,1,'2026-01-16 03:31:25','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:25','2026-01-16 03:31:25',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(243,5,1,'2026-01-16 03:31:26','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:26','2026-01-16 03:31:26',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(244,5,1,'2026-01-16 03:31:28','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:28','2026-01-16 03:31:28',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(245,5,1,'2026-01-16 03:31:29','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:29','2026-01-16 03:31:29',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(246,5,1,'2026-01-16 03:31:31','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:31','2026-01-16 03:31:31',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(247,5,1,'2026-01-16 03:31:32','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:32','2026-01-16 03:31:32',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(248,5,1,'2026-01-16 03:31:33','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:33','2026-01-16 03:31:33',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(249,5,1,'2026-01-16 03:31:34','1.3753553850756931e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:31:34','2026-01-16 03:31:34',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(250,5,1,'2026-01-16 03:32:17','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:17','2026-01-16 03:32:17',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(251,5,1,'2026-01-16 03:32:18','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:18','2026-01-16 03:32:18',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(252,5,1,'2026-01-16 03:32:19','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:19','2026-01-16 03:32:19',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(253,5,1,'2026-01-16 03:32:20','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:20','2026-01-16 03:32:20',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(254,5,1,'2026-01-16 03:32:21','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:21','2026-01-16 03:32:21',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(255,5,1,'2026-01-16 03:32:22','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:22','2026-01-16 03:32:22',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(256,5,1,'2026-01-16 03:32:23','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:23','2026-01-16 03:32:23',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(257,5,1,'2026-01-16 03:32:24','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:24','2026-01-16 03:32:24',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(258,5,1,'2026-01-16 03:32:25','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:25','2026-01-16 03:32:25',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(259,5,1,'2026-01-16 03:32:26','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:26','2026-01-16 03:32:26',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(260,5,1,'2026-01-16 03:32:27','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:27','2026-01-16 03:32:27',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(261,5,1,'2026-01-16 03:32:29','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:29','2026-01-16 03:32:29',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(262,5,1,'2026-01-16 03:32:30','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:30','2026-01-16 03:32:30',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(263,5,1,'2026-01-16 03:32:31','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:31','2026-01-16 03:32:31',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(264,5,1,'2026-01-16 03:32:32','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:32','2026-01-16 03:32:32',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(265,5,1,'2026-01-16 03:32:33','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:33','2026-01-16 03:32:33',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(266,5,1,'2026-01-16 03:32:34','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:34','2026-01-16 03:32:34',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(267,5,1,'2026-01-16 03:32:35','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:35','2026-01-16 03:32:35',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(268,5,1,'2026-01-16 03:32:36','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:36','2026-01-16 03:32:36',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(269,5,1,'2026-01-16 03:32:37','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:37','2026-01-16 03:32:37',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(270,5,1,'2026-01-16 03:32:38','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:38','2026-01-16 03:32:38',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(271,5,1,'2026-01-16 03:32:39','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:39','2026-01-16 03:32:39',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(272,5,1,'2026-01-16 03:32:40','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:40','2026-01-16 03:32:40',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(273,5,1,'2026-01-16 03:32:41','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:41','2026-01-16 03:32:41',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(274,5,1,'2026-01-16 03:32:42','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:42','2026-01-16 03:32:42',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(275,5,1,'2026-01-16 03:32:43','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:43','2026-01-16 03:32:43',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(276,5,1,'2026-01-16 03:32:45','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:45','2026-01-16 03:32:45',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(277,5,1,'2026-01-16 03:32:46','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:46','2026-01-16 03:32:46',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(278,5,1,'2026-01-16 03:32:47','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:47','2026-01-16 03:32:47',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(279,5,1,'2026-01-16 03:32:48','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:48','2026-01-16 03:32:48',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(280,5,1,'2026-01-16 03:32:49','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:49','2026-01-16 03:32:49',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(281,5,1,'2026-01-16 03:32:50','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:50','2026-01-16 03:32:50',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(282,5,1,'2026-01-16 03:32:51','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:51','2026-01-16 03:32:51',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(283,5,1,'2026-01-16 03:32:52','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:52','2026-01-16 03:32:52',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(284,5,1,'2026-01-16 03:32:53','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:53','2026-01-16 03:32:53',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(285,5,1,'2026-01-16 03:32:54','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:54','2026-01-16 03:32:54',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(286,5,1,'2026-01-16 03:32:55','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:55','2026-01-16 03:32:55',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(287,5,1,'2026-01-16 03:32:56','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:56','2026-01-16 03:32:56',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(288,5,1,'2026-01-16 03:32:57','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:57','2026-01-16 03:32:57',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(289,5,1,'2026-01-16 03:32:58','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:58','2026-01-16 03:32:58',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(290,5,1,'2026-01-16 03:32:59','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:32:59','2026-01-16 03:32:59',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(291,5,1,'2026-01-16 03:33:00','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:33:00','2026-01-16 03:33:00',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(292,5,1,'2026-01-16 03:33:01','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:33:01','2026-01-16 03:33:01',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(293,5,1,'2026-01-16 03:33:02','1.3753553850756931e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:33:02','2026-01-16 03:33:02',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(294,5,1,'2026-01-16 03:33:46','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:33:46','2026-01-16 03:33:46',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(295,5,1,'2026-01-16 03:33:47','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:33:47','2026-01-16 03:33:47',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(296,5,1,'2026-01-16 03:33:49','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:33:49','2026-01-16 03:33:49',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(297,5,1,'2026-01-16 03:33:50','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:33:50','2026-01-16 03:33:50',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(298,5,1,'2026-01-16 03:33:52','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:33:52','2026-01-16 03:33:52',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(299,5,1,'2026-01-16 03:33:53','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:33:53','2026-01-16 03:33:53',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(300,5,1,'2026-01-16 03:33:54','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:33:54','2026-01-16 03:33:54',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(301,5,1,'2026-01-16 03:33:55','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:33:55','2026-01-16 03:33:55',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(302,5,1,'2026-01-16 03:33:56','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:33:56','2026-01-16 03:33:56',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(303,5,1,'2026-01-16 03:33:59','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:33:59','2026-01-16 03:33:59',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(304,5,1,'2026-01-16 03:34:00','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:00','2026-01-16 03:34:00',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(305,5,1,'2026-01-16 03:34:01','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:01','2026-01-16 03:34:01',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(306,5,1,'2026-01-16 03:34:02','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:02','2026-01-16 03:34:02',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(307,5,1,'2026-01-16 03:34:03','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:03','2026-01-16 03:34:03',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(308,5,1,'2026-01-16 03:34:04','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:04','2026-01-16 03:34:04',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(309,5,1,'2026-01-16 03:34:05','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:05','2026-01-16 03:34:05',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(310,5,1,'2026-01-16 03:34:06','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:06','2026-01-16 03:34:06',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(311,5,1,'2026-01-16 03:34:07','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:07','2026-01-16 03:34:07',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(312,5,1,'2026-01-16 03:34:08','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:08','2026-01-16 03:34:08',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(313,5,1,'2026-01-16 03:34:09','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:09','2026-01-16 03:34:09',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(314,5,1,'2026-01-16 03:34:10','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:10','2026-01-16 03:34:10',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(315,5,1,'2026-01-16 03:34:11','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:11','2026-01-16 03:34:11',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(316,5,1,'2026-01-16 03:34:12','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:12','2026-01-16 03:34:12',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(317,5,1,'2026-01-16 03:34:13','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:13','2026-01-16 03:34:13',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(318,5,1,'2026-01-16 03:34:14','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:14','2026-01-16 03:34:14',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(319,5,1,'2026-01-16 03:34:15','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:15','2026-01-16 03:34:15',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(320,5,1,'2026-01-16 03:34:16','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:16','2026-01-16 03:34:16',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(321,5,1,'2026-01-16 03:34:17','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:17','2026-01-16 03:34:17',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(322,5,1,'2026-01-16 03:34:18','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:18','2026-01-16 03:34:18',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(323,5,1,'2026-01-16 03:34:19','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:19','2026-01-16 03:34:19',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(324,5,1,'2026-01-16 03:34:20','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:20','2026-01-16 03:34:20',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(325,5,1,'2026-01-16 03:34:21','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:21','2026-01-16 03:34:21',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(326,5,1,'2026-01-16 03:34:22','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:22','2026-01-16 03:34:22',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(327,5,1,'2026-01-16 03:34:23','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:23','2026-01-16 03:34:23',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(328,5,1,'2026-01-16 03:34:24','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:24','2026-01-16 03:34:24',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(329,5,1,'2026-01-16 03:34:26','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:26','2026-01-16 03:34:26',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(330,5,1,'2026-01-16 03:34:27','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:27','2026-01-16 03:34:27',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(331,5,1,'2026-01-16 03:34:28','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:28','2026-01-16 03:34:28',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(332,5,1,'2026-01-16 03:34:29','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:29','2026-01-16 03:34:29',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(333,5,1,'2026-01-16 03:34:30','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:30','2026-01-16 03:34:30',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(334,5,1,'2026-01-16 03:34:31','1.3753553850756931e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:34:31','2026-01-16 03:34:31',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(335,5,1,'2026-01-16 03:35:13','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:13','2026-01-16 03:35:13',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(336,5,1,'2026-01-16 03:35:14','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:14','2026-01-16 03:35:14',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(337,5,1,'2026-01-16 03:35:15','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:15','2026-01-16 03:35:15',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(338,5,1,'2026-01-16 03:35:16','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:16','2026-01-16 03:35:16',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(339,5,1,'2026-01-16 03:35:17','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:17','2026-01-16 03:35:17',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(340,5,1,'2026-01-16 03:35:18','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:18','2026-01-16 03:35:18',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(341,5,1,'2026-01-16 03:35:19','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:19','2026-01-16 03:35:19',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(342,5,1,'2026-01-16 03:35:20','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:20','2026-01-16 03:35:20',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(343,5,1,'2026-01-16 03:35:21','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:21','2026-01-16 03:35:21',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(344,5,1,'2026-01-16 03:35:22','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:22','2026-01-16 03:35:22',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(345,5,1,'2026-01-16 03:35:23','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:23','2026-01-16 03:35:23',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(346,5,1,'2026-01-16 03:35:24','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:24','2026-01-16 03:35:24',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(347,5,1,'2026-01-16 03:35:25','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:25','2026-01-16 03:35:25',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(348,5,1,'2026-01-16 03:35:26','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:26','2026-01-16 03:35:26',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(349,5,1,'2026-01-16 03:35:27','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:27','2026-01-16 03:35:27',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(350,5,1,'2026-01-16 03:35:28','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:28','2026-01-16 03:35:28',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(351,5,1,'2026-01-16 03:35:29','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:29','2026-01-16 03:35:29',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(352,5,1,'2026-01-16 03:35:30','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:30','2026-01-16 03:35:30',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(353,5,1,'2026-01-16 03:35:31','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:31','2026-01-16 03:35:31',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(354,5,1,'2026-01-16 03:35:32','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:32','2026-01-16 03:35:32',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(355,5,1,'2026-01-16 03:35:33','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:33','2026-01-16 03:35:33',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(356,5,1,'2026-01-16 03:35:34','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:34','2026-01-16 03:35:34',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(357,5,1,'2026-01-16 03:35:35','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:35','2026-01-16 03:35:35',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(358,5,1,'2026-01-16 03:35:36','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:36','2026-01-16 03:35:36',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(359,5,1,'2026-01-16 03:35:37','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:37','2026-01-16 03:35:37',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(360,5,1,'2026-01-16 03:35:38','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:38','2026-01-16 03:35:38',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(361,5,1,'2026-01-16 03:35:39','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:39','2026-01-16 03:35:39',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(362,5,1,'2026-01-16 03:35:40','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:40','2026-01-16 03:35:40',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(363,5,1,'2026-01-16 03:35:41','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:41','2026-01-16 03:35:41',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(364,5,1,'2026-01-16 03:35:42','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:42','2026-01-16 03:35:42',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(365,5,1,'2026-01-16 03:35:43','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:43','2026-01-16 03:35:43',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(366,5,1,'2026-01-16 03:35:44','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:44','2026-01-16 03:35:44',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(367,5,1,'2026-01-16 03:35:45','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:45','2026-01-16 03:35:45',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(368,5,1,'2026-01-16 03:35:46','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:46','2026-01-16 03:35:46',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(369,5,1,'2026-01-16 03:35:47','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:47','2026-01-16 03:35:47',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(370,5,1,'2026-01-16 03:35:48','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:48','2026-01-16 03:35:48',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(371,5,1,'2026-01-16 03:35:49','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:49','2026-01-16 03:35:49',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(372,5,1,'2026-01-16 03:35:51','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:51','2026-01-16 03:35:51',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(373,5,1,'2026-01-16 03:35:52','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:52','2026-01-16 03:35:52',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(374,5,1,'2026-01-16 03:35:54','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:54','2026-01-16 03:35:54',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(375,5,1,'2026-01-16 03:35:55','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:55','2026-01-16 03:35:55',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(376,5,1,'2026-01-16 03:35:57','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:57','2026-01-16 03:35:57',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(377,5,1,'2026-01-16 03:35:58','1.3753553850756931e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:35:58','2026-01-16 03:35:58',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(378,5,1,'2026-01-16 03:36:42','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:42','2026-01-16 03:36:42',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(379,5,1,'2026-01-16 03:36:43','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:43','2026-01-16 03:36:43',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(380,5,1,'2026-01-16 03:36:44','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:44','2026-01-16 03:36:44',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(381,5,1,'2026-01-16 03:36:45','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:45','2026-01-16 03:36:45',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(382,5,1,'2026-01-16 03:36:46','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:46','2026-01-16 03:36:46',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(383,5,1,'2026-01-16 03:36:47','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:47','2026-01-16 03:36:47',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(384,5,1,'2026-01-16 03:36:48','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:48','2026-01-16 03:36:48',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(385,5,1,'2026-01-16 03:36:49','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:49','2026-01-16 03:36:49',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(386,5,1,'2026-01-16 03:36:50','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:50','2026-01-16 03:36:50',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(387,5,1,'2026-01-16 03:36:51','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:51','2026-01-16 03:36:51',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(388,5,1,'2026-01-16 03:36:52','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:52','2026-01-16 03:36:52',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(389,5,1,'2026-01-16 03:36:53','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:53','2026-01-16 03:36:53',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(390,5,1,'2026-01-16 03:36:54','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:54','2026-01-16 03:36:54',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(391,5,1,'2026-01-16 03:36:55','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:55','2026-01-16 03:36:55',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(392,5,1,'2026-01-16 03:36:56','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:56','2026-01-16 03:36:56',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(393,5,1,'2026-01-16 03:36:57','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:57','2026-01-16 03:36:57',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(394,5,1,'2026-01-16 03:36:58','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:58','2026-01-16 03:36:58',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(395,5,1,'2026-01-16 03:36:59','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:36:59','2026-01-16 03:36:59',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(396,5,1,'2026-01-16 03:37:00','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:00','2026-01-16 03:37:00',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(397,5,1,'2026-01-16 03:37:01','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:01','2026-01-16 03:37:01',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(398,5,1,'2026-01-16 03:37:02','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:02','2026-01-16 03:37:02',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(399,5,1,'2026-01-16 03:37:03','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:03','2026-01-16 03:37:03',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(400,5,1,'2026-01-16 03:37:04','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:04','2026-01-16 03:37:04',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(401,5,1,'2026-01-16 03:37:05','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:05','2026-01-16 03:37:05',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(402,5,1,'2026-01-16 03:37:06','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:06','2026-01-16 03:37:06',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(403,5,1,'2026-01-16 03:37:07','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:07','2026-01-16 03:37:07',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(404,5,1,'2026-01-16 03:37:08','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:08','2026-01-16 03:37:08',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(405,5,1,'2026-01-16 03:37:09','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:09','2026-01-16 03:37:09',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(406,5,1,'2026-01-16 03:37:10','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:10','2026-01-16 03:37:10',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(407,5,1,'2026-01-16 03:37:11','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:11','2026-01-16 03:37:11',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(408,5,1,'2026-01-16 03:37:12','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:12','2026-01-16 03:37:12',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(409,5,1,'2026-01-16 03:37:13','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:13','2026-01-16 03:37:13',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(410,5,1,'2026-01-16 03:37:14','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:14','2026-01-16 03:37:14',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(411,5,1,'2026-01-16 03:37:15','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:15','2026-01-16 03:37:15',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(412,5,1,'2026-01-16 03:37:16','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:16','2026-01-16 03:37:16',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(413,5,1,'2026-01-16 03:37:17','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:17','2026-01-16 03:37:17',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(414,5,1,'2026-01-16 03:37:18','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:18','2026-01-16 03:37:18',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(415,5,1,'2026-01-16 03:37:19','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:19','2026-01-16 03:37:19',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(416,5,1,'2026-01-16 03:37:20','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:20','2026-01-16 03:37:20',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(417,5,1,'2026-01-16 03:37:21','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:21','2026-01-16 03:37:21',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(418,5,1,'2026-01-16 03:37:22','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:22','2026-01-16 03:37:22',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(419,5,1,'2026-01-16 03:37:23','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:23','2026-01-16 03:37:23',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(420,5,1,'2026-01-16 03:37:24','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:24','2026-01-16 03:37:24',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(421,5,1,'2026-01-16 03:37:25','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:25','2026-01-16 03:37:25',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(422,5,1,'2026-01-16 03:37:26','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:26','2026-01-16 03:37:26',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(423,5,1,'2026-01-16 03:37:27','1.3753553850756931e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:37:27','2026-01-16 03:37:27',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(424,5,1,'2026-01-16 03:38:10','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:10','2026-01-16 03:38:10',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(425,5,1,'2026-01-16 03:38:12','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:12','2026-01-16 03:38:12',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(426,5,1,'2026-01-16 03:38:13','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:13','2026-01-16 03:38:13',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(427,5,1,'2026-01-16 03:38:15','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:15','2026-01-16 03:38:15',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(428,5,1,'2026-01-16 03:38:16','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:16','2026-01-16 03:38:16',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(429,5,1,'2026-01-16 03:38:18','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:18','2026-01-16 03:38:18',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(430,5,1,'2026-01-16 03:38:19','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:19','2026-01-16 03:38:19',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(431,5,1,'2026-01-16 03:38:21','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:21','2026-01-16 03:38:21',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(432,5,1,'2026-01-16 03:38:22','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:22','2026-01-16 03:38:22',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(433,5,1,'2026-01-16 03:38:23','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:23','2026-01-16 03:38:23',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(434,5,1,'2026-01-16 03:38:25','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:25','2026-01-16 03:38:25',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(435,5,1,'2026-01-16 03:38:26','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:26','2026-01-16 03:38:26',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(436,5,1,'2026-01-16 03:38:28','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:28','2026-01-16 03:38:28',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(437,5,1,'2026-01-16 03:38:30','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:30','2026-01-16 03:38:30',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(438,5,1,'2026-01-16 03:38:31','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:31','2026-01-16 03:38:31',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(439,5,1,'2026-01-16 03:38:32','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:32','2026-01-16 03:38:32',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(440,5,1,'2026-01-16 03:38:33','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:33','2026-01-16 03:38:33',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(441,5,1,'2026-01-16 03:38:34','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:34','2026-01-16 03:38:34',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(442,5,1,'2026-01-16 03:38:35','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:35','2026-01-16 03:38:35',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(443,5,1,'2026-01-16 03:38:36','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:36','2026-01-16 03:38:36',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(444,5,1,'2026-01-16 03:38:37','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:37','2026-01-16 03:38:37',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(445,5,1,'2026-01-16 03:38:38','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:38','2026-01-16 03:38:38',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(446,5,1,'2026-01-16 03:38:39','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:39','2026-01-16 03:38:39',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(447,5,1,'2026-01-16 03:38:40','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:40','2026-01-16 03:38:40',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(448,5,1,'2026-01-16 03:38:41','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:41','2026-01-16 03:38:41',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(449,5,1,'2026-01-16 03:38:42','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:42','2026-01-16 03:38:42',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(450,5,1,'2026-01-16 03:38:43','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:43','2026-01-16 03:38:43',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(451,5,1,'2026-01-16 03:38:44','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:44','2026-01-16 03:38:44',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(452,5,1,'2026-01-16 03:38:45','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:45','2026-01-16 03:38:45',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(453,5,1,'2026-01-16 03:38:46','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:46','2026-01-16 03:38:46',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(454,5,1,'2026-01-16 03:38:47','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:47','2026-01-16 03:38:47',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(455,5,1,'2026-01-16 03:38:48','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:48','2026-01-16 03:38:48',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(456,5,1,'2026-01-16 03:38:49','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:49','2026-01-16 03:38:49',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(457,5,1,'2026-01-16 03:38:50','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:50','2026-01-16 03:38:50',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(458,5,1,'2026-01-16 03:38:51','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:51','2026-01-16 03:38:51',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(459,5,1,'2026-01-16 03:38:52','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:52','2026-01-16 03:38:52',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(460,5,1,'2026-01-16 03:38:53','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:53','2026-01-16 03:38:53',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(461,5,1,'2026-01-16 03:38:55','1.3753553850756931e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:38:55','2026-01-16 03:38:55',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(462,5,1,'2026-01-16 03:39:38','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:38','2026-01-16 03:39:38',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(463,5,1,'2026-01-16 03:39:39','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:39','2026-01-16 03:39:39',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(464,5,1,'2026-01-16 03:39:41','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:41','2026-01-16 03:39:41',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(465,5,1,'2026-01-16 03:39:42','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:42','2026-01-16 03:39:42',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(466,5,1,'2026-01-16 03:39:44','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:44','2026-01-16 03:39:44',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(467,5,1,'2026-01-16 03:39:45','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:45','2026-01-16 03:39:45',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(468,5,1,'2026-01-16 03:39:46','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:46','2026-01-16 03:39:46',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(469,5,1,'2026-01-16 03:39:48','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:48','2026-01-16 03:39:48',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(470,5,1,'2026-01-16 03:39:49','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:49','2026-01-16 03:39:49',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(471,5,1,'2026-01-16 03:39:52','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:52','2026-01-16 03:39:52',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(472,5,1,'2026-01-16 03:39:53','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:53','2026-01-16 03:39:53',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(473,5,1,'2026-01-16 03:39:55','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:55','2026-01-16 03:39:55',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(474,5,1,'2026-01-16 03:39:56','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:56','2026-01-16 03:39:56',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(475,5,1,'2026-01-16 03:39:57','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:57','2026-01-16 03:39:57',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(476,5,1,'2026-01-16 03:39:58','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:39:58','2026-01-16 03:39:58',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(477,5,1,'2026-01-16 03:40:00','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:00','2026-01-16 03:40:00',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(478,5,1,'2026-01-16 03:40:01','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:01','2026-01-16 03:40:01',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(479,5,1,'2026-01-16 03:40:03','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:03','2026-01-16 03:40:03',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(480,5,1,'2026-01-16 03:40:04','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:04','2026-01-16 03:40:04',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(481,5,1,'2026-01-16 03:40:05','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:05','2026-01-16 03:40:05',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(482,5,1,'2026-01-16 03:40:06','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:06','2026-01-16 03:40:06',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(483,5,1,'2026-01-16 03:40:08','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:08','2026-01-16 03:40:08',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(484,5,1,'2026-01-16 03:40:09','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:09','2026-01-16 03:40:09',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(485,5,1,'2026-01-16 03:40:10','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:10','2026-01-16 03:40:10',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(486,5,1,'2026-01-16 03:40:12','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:12','2026-01-16 03:40:12',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(487,5,1,'2026-01-16 03:40:13','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:13','2026-01-16 03:40:13',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(488,5,1,'2026-01-16 03:40:14','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:14','2026-01-16 03:40:14',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(489,5,1,'2026-01-16 03:40:15','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:15','2026-01-16 03:40:15',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(490,5,1,'2026-01-16 03:40:16','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:16','2026-01-16 03:40:16',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(491,5,1,'2026-01-16 03:40:17','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:17','2026-01-16 03:40:17',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(492,5,1,'2026-01-16 03:40:18','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:18','2026-01-16 03:40:18',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(493,5,1,'2026-01-16 03:40:19','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:19','2026-01-16 03:40:19',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(494,5,1,'2026-01-16 03:40:20','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:20','2026-01-16 03:40:20',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(495,5,1,'2026-01-16 03:40:21','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:21','2026-01-16 03:40:21',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(496,5,1,'2026-01-16 03:40:22','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:22','2026-01-16 03:40:22',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(497,5,1,'2026-01-16 03:40:23','1.3753553850756931e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:40:23','2026-01-16 03:40:23',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(498,5,1,'2026-01-16 03:41:06','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:06','2026-01-16 03:41:06',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(499,5,1,'2026-01-16 03:41:07','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:07','2026-01-16 03:41:07',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(500,5,1,'2026-01-16 03:41:08','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:08','2026-01-16 03:41:08',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(501,5,1,'2026-01-16 03:41:09','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:09','2026-01-16 03:41:09',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(502,5,1,'2026-01-16 03:41:10','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:10','2026-01-16 03:41:10',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(503,5,1,'2026-01-16 03:41:11','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:11','2026-01-16 03:41:11',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(504,5,1,'2026-01-16 03:41:12','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:12','2026-01-16 03:41:12',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(505,5,1,'2026-01-16 03:41:13','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:13','2026-01-16 03:41:13',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(506,5,1,'2026-01-16 03:41:14','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:14','2026-01-16 03:41:14',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(507,5,1,'2026-01-16 03:41:15','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:15','2026-01-16 03:41:15',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(508,5,1,'2026-01-16 03:41:16','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:16','2026-01-16 03:41:16',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(509,5,1,'2026-01-16 03:41:17','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:17','2026-01-16 03:41:17',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(510,5,1,'2026-01-16 03:41:18','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:18','2026-01-16 03:41:18',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(511,5,1,'2026-01-16 03:41:19','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:19','2026-01-16 03:41:19',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(512,5,1,'2026-01-16 03:41:20','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:20','2026-01-16 03:41:20',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(513,5,1,'2026-01-16 03:41:21','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:21','2026-01-16 03:41:21',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(514,5,1,'2026-01-16 03:41:23','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:23','2026-01-16 03:41:23',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(515,5,1,'2026-01-16 03:41:24','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:24','2026-01-16 03:41:24',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(516,5,1,'2026-01-16 03:41:25','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:25','2026-01-16 03:41:25',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(517,5,1,'2026-01-16 03:41:26','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:26','2026-01-16 03:41:26',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(518,5,1,'2026-01-16 03:41:27','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:27','2026-01-16 03:41:27',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(519,5,1,'2026-01-16 03:41:28','1.375353232681252e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:28','2026-01-16 03:41:28',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(520,5,1,'2026-01-16 03:41:29','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:29','2026-01-16 03:41:29',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(521,5,1,'2026-01-16 03:41:30','1.3753534120474554e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:30','2026-01-16 03:41:30',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(522,5,1,'2026-01-16 03:41:31','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:31','2026-01-16 03:41:31',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(523,5,1,'2026-01-16 03:41:32','1.3753535914136588e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:32','2026-01-16 03:41:32',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(524,5,1,'2026-01-16 03:41:33','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:33','2026-01-16 03:41:33',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(525,5,1,'2026-01-16 03:41:34','1.3753537707798622e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:34','2026-01-16 03:41:34',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(526,5,1,'2026-01-16 03:41:35','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:35','2026-01-16 03:41:35',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(527,5,1,'2026-01-16 03:41:36','1.3753539501460657e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:36','2026-01-16 03:41:36',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(528,5,1,'2026-01-16 03:41:37','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:37','2026-01-16 03:41:37',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(529,5,1,'2026-01-16 03:41:38','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:38','2026-01-16 03:41:38',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(530,5,1,'2026-01-16 03:41:39','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:39','2026-01-16 03:41:39',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(531,5,1,'2026-01-16 03:41:40','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:40','2026-01-16 03:41:40',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(532,5,1,'2026-01-16 03:41:41','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:41','2026-01-16 03:41:41',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(533,5,1,'2026-01-16 03:41:42','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:42','2026-01-16 03:41:42',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(534,5,1,'2026-01-16 03:41:43','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:43','2026-01-16 03:41:43',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(535,5,1,'2026-01-16 03:41:44','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:44','2026-01-16 03:41:44',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(536,5,1,'2026-01-16 03:41:45','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:45','2026-01-16 03:41:45',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(537,5,1,'2026-01-16 03:41:46','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:46','2026-01-16 03:41:46',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(538,5,1,'2026-01-16 03:41:47','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:47','2026-01-16 03:41:47',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(539,5,1,'2026-01-16 03:41:48','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:48','2026-01-16 03:41:48',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(540,5,1,'2026-01-16 03:41:49','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:49','2026-01-16 03:41:49',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(541,5,1,'2026-01-16 03:41:50','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:50','2026-01-16 03:41:50',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(542,5,1,'2026-01-16 03:41:51','1.3753553850756931e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:41:51','2026-01-16 03:41:51',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(543,5,1,'2026-01-16 03:42:34','1.3753552057094897e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:42:34','2026-01-16 03:42:34',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(544,5,1,'2026-01-16 03:42:35','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:42:35','2026-01-16 03:42:35',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(545,5,1,'2026-01-16 03:42:36','1.3753550263432863e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:42:36','2026-01-16 03:42:36',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(546,5,1,'2026-01-16 03:42:37','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:42:37','2026-01-16 03:42:37',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(547,5,1,'2026-01-16 03:42:38','1.3753548469770828e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:42:38','2026-01-16 03:42:38',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(548,5,1,'2026-01-16 03:42:39','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','acknowledged','2026-01-16 10:27:22',1,'시스템 확인',NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:42:39','2026-01-16 10:27:22',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(549,5,1,'2026-01-16 03:42:40','1.3753546676108794e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','acknowledged','2026-01-16 10:27:18',1,'시스템 확인',NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:42:40','2026-01-16 10:27:18',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(550,5,1,'2026-01-16 03:42:41','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','cleared','2026-01-16 10:27:15',1,'시스템 확인','2026-01-16 10:30:50',NULL,'상황 종료 및 조치 완료',1,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:42:41','2026-01-16 10:30:50',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(551,5,1,'2026-01-16 03:42:42','1.375354488244676e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','cleared','2026-01-16 05:50:16',1,'확인됨','2026-01-16 10:30:48',NULL,'상황 종료 및 조치 완료',1,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:42:42','2026-01-16 10:30:48',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(552,5,1,'2026-01-16 03:42:43','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','cleared','2026-01-16 05:13:26',1,'System Admin acknowledged','2026-01-16 10:30:44',NULL,'상황 종료 및 조치 완료',1,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:42:43','2026-01-16 10:30:44',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(553,5,1,'2026-01-16 03:42:44','1.3753543088784725e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','cleared','2026-01-16 04:45:53',1,'System Admin acknowledged','2026-01-16 10:27:05',NULL,'상황 종료 및 조치 완료',1,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:42:44','2026-01-16 10:27:05',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(554,5,1,'2026-01-16 03:42:45','1.3753541295122691e-36','LOW','Production_Line_Speed: LOW (Value: 0.00)','MEDIUM','cleared','2026-01-16 04:45:43',1,'System Admin acknowledged','2026-01-16 05:13:38',NULL,'System Admin cleared',1,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-16 03:42:45','2026-01-16 05:13:38',1,2,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(555,2,1,'2026-01-17 06:53:31','32.300000000000004','HIGH','Motor_Current_Overload: HIGH (Value: 32.30)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-17 06:53:31','2026-01-17 06:53:31',1,3,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(556,9,1,'2026-01-17 06:53:31','110.0','HIGH','E2E-Verify-Template-1768632082540 (4): HIGH (Value: 110.00)','HIGH','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-17 06:53:31','2026-01-17 06:53:31',1,4,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(557,3,1,'2026-01-17 06:53:31','1005.0',NULL,'Emergency_Stop_Active:  (Value: 1005.00)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-17 06:53:31','2026-01-17 06:53:31',1,5,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(558,2,1,'2026-01-17 07:01:39','30.3','HIGH','Motor_Current_Overload: HIGH (Value: 30.30)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-17 07:01:39','2026-01-17 07:01:39',1,3,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(559,3,1,'2026-01-17 07:01:39','1005.0',NULL,'Emergency_Stop_Active:  (Value: 1005.00)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-17 07:01:39','2026-01-17 07:01:39',1,5,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(560,2,1,'2026-01-18 05:22:34','31.3','HIGH','Motor_Current_Overload: HIGH (Value: 31.30)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-18 05:22:34','2026-01-18 05:22:34',1,3,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(561,3,1,'2026-01-18 05:22:34','1005.0',NULL,'Emergency_Stop_Active:  (Value: 1005.00)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-18 05:22:34','2026-01-18 05:22:34',1,5,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(562,2,1,'2026-01-18 05:54:57','31.3','HIGH','Motor_Current_Overload: HIGH (Value: 31.30)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-18 05:54:57','2026-01-18 05:54:57',1,3,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(563,3,1,'2026-01-18 05:54:57','1005.0',NULL,'Emergency_Stop_Active:  (Value: 1005.00)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-18 05:54:57','2026-01-18 05:54:57',1,5,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(564,3,1,'2026-01-18 09:20:04','1005.0',NULL,'Emergency_Stop_Active:  (Value: 1005.00)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-18 09:20:04','2026-01-18 09:20:04',1,5,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(565,2,1,'2026-01-18 09:20:46','30.1','HIGH','Motor_Current_Overload: HIGH (Value: 30.10)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-18 09:20:46','2026-01-18 09:20:46',1,3,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(566,2,1,'2026-01-18 09:22:54','30.1','HIGH','Motor_Current_Overload: HIGH (Value: 30.10)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-18 09:22:54','2026-01-18 09:22:54',1,3,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(567,3,1,'2026-01-18 09:22:54','1005.0',NULL,'Emergency_Stop_Active:  (Value: 1005.00)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-18 09:22:54','2026-01-18 09:22:54',1,5,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(568,2,1,'2026-01-18 09:23:44','30.3','HIGH','Motor_Current_Overload: HIGH (Value: 30.30)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-18 09:23:44','2026-01-18 09:23:44',1,3,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(569,3,1,'2026-01-18 09:23:44','1005.0',NULL,'Emergency_Stop_Active:  (Value: 1005.00)','CRITICAL','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-18 09:23:44','2026-01-18 09:23:44',1,5,NULL,NULL);
INSERT INTO alarm_occurrences VALUES(570,13,1,'2026-01-18 09:23:44','40.3','HIGH','E2E_Test_Rule_1768716099440: HIGH (Value: 40.30)','HIGH','ACTIVE',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','Unknown','Unknown Location','2026-01-18 09:23:44','2026-01-18 09:23:44',0,20,NULL,NULL);
CREATE TABLE alarm_rule_templates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 템플릿 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    category VARCHAR(50),                           -- 'temperature', 'pressure', 'flow' 등
    
    -- 템플릿 조건 설정
    condition_type VARCHAR(50) NOT NULL,            -- 'threshold', 'range', 'digital' 등
    condition_template TEXT NOT NULL,               -- "> {threshold}°C" 형태의 템플릿
    default_config TEXT NOT NULL,                   -- JSON 형태의 기본 설정값
    
    -- 알람 기본 설정
    severity VARCHAR(20) DEFAULT 'warning',
    message_template TEXT,                          -- "{device_name}에서 {condition} 발생"
    
    -- 적용 대상 제한
    applicable_data_types TEXT,                     -- JSON 배열: ["temperature", "analog"]
    applicable_device_types TEXT,                   -- JSON 배열: ["modbus_rtu", "mqtt"]
    applicable_units TEXT,                          -- JSON 배열: ["°C", "bar", "rpm"]
    
    -- 🔥 추가: 알림 설정 (C++ 코드 호환용)
    notification_enabled INTEGER DEFAULT 1,        -- ✅ 누락된 컬럼 추가!
    
    -- 템플릿 메타데이터
    industry VARCHAR(50),                           -- 'manufacturing', 'hvac', 'water_treatment'
    equipment_type VARCHAR(50),                     -- 'pump', 'motor', 'sensor'
    usage_count INTEGER DEFAULT 0,                 -- 사용 횟수 (인기도 측정)
    is_active INTEGER DEFAULT 1,
    is_system_template INTEGER DEFAULT 0,           -- 시스템 기본 템플릿 여부
    
    -- 태깅 시스템
    tags TEXT DEFAULT NULL,                         -- JSON 배열 형태
    
    -- 타임스탬프
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER, `template_type` varchar(20) default 'simple',
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, name)
);
INSERT INTO alarm_rule_templates VALUES(6,1,'Test Template 1768625937466_1','Verification Script Test','temperature','threshold','Standard Threshold','{"high_limit":80,"high_high_limit":95,"deadband":2.5}','critical','{device} temp is {value}','["temperature","analog"]',NULL,NULL,1,NULL,NULL,0,1,0,NULL,'2026-01-17 04:58:57','2026-01-17 04:58:57',NULL,'simple');
INSERT INTO alarm_rule_templates VALUES(7,1,'Test Template 1768625937466_2','Verification Script Test','temperature','threshold','Standard Threshold','{"high_limit":80,"high_high_limit":95,"deadband":2.5}','critical','{device} temp is {value}','["temperature","analog"]',NULL,NULL,1,NULL,NULL,0,1,0,NULL,'2026-01-17 04:58:57','2026-01-17 04:58:57',NULL,'simple');
INSERT INTO alarm_rule_templates VALUES(8,1,'E2E-Verify-Template-1768632082540','Created by Antigravity for E2E verification','test','threshold','value > {high_limit}','{"high_limit":99.5,"low_limit":10,"deadband":1}','high','E2E VERIFY: {device_name} value is {value}','["INT16","analog"]',NULL,NULL,1,NULL,NULL,1,1,0,NULL,'2026-01-17 06:41:22','2026-01-17 06:41:22',NULL,'simple');
CREATE TABLE javascript_functions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 함수 정보
    name VARCHAR(100) NOT NULL,
    display_name VARCHAR(100),
    description TEXT,
    function_code TEXT NOT NULL,
    
    -- 함수 메타데이터
    category VARCHAR(50),                           -- 'math', 'logic', 'time', 'conversion'
    parameters TEXT,                                -- JSON 배열 형태 파라미터 정의
    return_type VARCHAR(20) DEFAULT 'number',       -- 'number', 'boolean', 'string'
    
    -- 사용 통계
    usage_count INTEGER DEFAULT 0,
    last_used DATETIME,
    
    -- 예제 및 문서
    example_usage TEXT,
    test_cases TEXT,                                -- JSON 형태 테스트 케이스들
    
    -- 상태
    is_enabled INTEGER DEFAULT 1,
    is_system_function INTEGER DEFAULT 0,
    
    -- 타임스탬프
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, name)
);
INSERT INTO javascript_functions VALUES(1,1,'average',NULL,'Calculate average of values','function average(...values) { return values.reduce((a, b) => a + b, 0) / values.length; }','math','[{"name": "values", "type": "number[]", "required": true}]','number',0,NULL,NULL,NULL,1,0,'2026-01-16 01:13:27','2026-01-16 01:13:27',NULL);
INSERT INTO javascript_functions VALUES(2,1,'oeeCalculation',NULL,'Calculate Overall Equipment Effectiveness','function oeeCalculation(availability, performance, quality) { return (availability / 100) * (performance / 100) * (quality / 100) * 100; }','engineering','[{"name": "availability", "type": "number"}, {"name": "performance", "type": "number"}, {"name": "quality", "type": "number"}]','number',0,NULL,NULL,NULL,1,0,'2026-01-16 01:13:27','2026-01-16 01:13:27',NULL);
INSERT INTO javascript_functions VALUES(3,1,'powerFactorCorrection',NULL,'Calculate power factor correction','function powerFactorCorrection(activePower, reactivePower) { return activePower / Math.sqrt(activePower * activePower + reactivePower * reactivePower); }','electrical','[{"name": "activePower", "type": "number"}, {"name": "reactivePower", "type": "number"}]','number',0,NULL,NULL,NULL,1,0,'2026-01-16 01:13:27','2026-01-16 01:13:27',NULL);
INSERT INTO javascript_functions VALUES(4,1,'productionEfficiency',NULL,'Calculate production efficiency for automotive line','function productionEfficiency(actual, target, hours) { return (actual / target) * 100; }','custom','[{"name": "actual", "type": "number"}, {"name": "target", "type": "number"}, {"name": "hours", "type": "number"}]','number',0,NULL,NULL,NULL,1,0,'2026-01-16 01:13:27','2026-01-16 01:13:27',NULL);
INSERT INTO javascript_functions VALUES(5,1,'energyIntensity',NULL,'Calculate energy intensity per unit','function energyIntensity(totalEnergy, productionCount) { return productionCount > 0 ? totalEnergy / productionCount : 0; }','custom','[{"name": "totalEnergy", "type": "number"}, {"name": "productionCount", "type": "number"}]','number',0,NULL,NULL,NULL,1,0,'2026-01-16 01:13:27','2026-01-16 01:13:27',NULL);
CREATE TABLE recipes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 레시피 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    recipe_type VARCHAR(50) DEFAULT 'alarm_condition', -- 'alarm_condition', 'data_processing'
    
    -- 레시피 구조
    steps TEXT NOT NULL,                            -- JSON 배열 형태의 단계들
    variables TEXT,                                 -- JSON 객체 형태의 변수 정의
    
    -- 실행 환경
    execution_context VARCHAR(20) DEFAULT 'sync',   -- 'sync', 'async', 'scheduled'
    timeout_ms INTEGER DEFAULT 5000,
    
    -- 상태 및 통계
    is_active INTEGER DEFAULT 1,
    execution_count INTEGER DEFAULT 0,
    success_count INTEGER DEFAULT 0,
    error_count INTEGER DEFAULT 0,
    avg_execution_time_ms REAL DEFAULT 0,
    last_executed DATETIME,
    last_error TEXT,
    
    -- 타임스탬프
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, name)
);
CREATE TABLE schedules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 스케줄 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 스케줄 타입 및 설정
    schedule_type VARCHAR(20) NOT NULL,             -- 'cron', 'interval', 'once'
    cron_expression VARCHAR(100),                   -- "0 */5 * * * *" (5분마다)
    interval_seconds INTEGER,                       -- interval 타입용
    start_time DATETIME,                            -- 시작 시간
    end_time DATETIME,                              -- 종료 시간 (선택사항)
    
    -- 실행 대상
    target_type VARCHAR(20) NOT NULL,               -- 'alarm_rule', 'function', 'recipe'
    target_id INTEGER NOT NULL,
    
    -- 실행 통계
    is_enabled INTEGER DEFAULT 1,
    execution_count INTEGER DEFAULT 0,
    success_count INTEGER DEFAULT 0,
    error_count INTEGER DEFAULT 0,
    last_executed DATETIME,
    next_execution DATETIME,
    last_error TEXT,
    
    -- 타임스탬프
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, name)
);
CREATE TABLE virtual_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 🔥 유연한 범위 설정 (현재 DB와 일치)
    scope_type VARCHAR(20) NOT NULL DEFAULT 'tenant',   -- tenant, site, device
    site_id INTEGER,
    device_id INTEGER,
    
    -- 🔥 가상포인트 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    formula TEXT NOT NULL,                              -- JavaScript 계산식
    data_type VARCHAR(20) NOT NULL DEFAULT 'float',
    unit VARCHAR(20),
    
    -- 🔥 계산 설정
    calculation_interval INTEGER DEFAULT 1000,          -- ms
    calculation_trigger VARCHAR(20) DEFAULT 'timer',    -- timer, onchange, manual
    is_enabled INTEGER DEFAULT 1,
    
    -- 🔥 메타데이터
    category VARCHAR(50),
    tags TEXT,                                          -- JSON 배열
    
    -- 🔥🔥🔥 확장 필드 (v3.0.0) - 현재 DB에 추가되어 있는 컬럼들
    execution_type VARCHAR(20) DEFAULT 'javascript',
    dependencies TEXT,                                  -- JSON 형태로 저장
    cache_duration_ms INTEGER DEFAULT 0,
    error_handling VARCHAR(20) DEFAULT 'return_null',
    last_error TEXT,
    execution_count INTEGER DEFAULT 0,
    avg_execution_time_ms REAL DEFAULT 0.0,
    last_execution_time DATETIME,
    
    -- 🔥 고급 설정 (신규 추가)
    script_library_id INTEGER,                         -- 스크립트 라이브러리 참조
    timeout_ms INTEGER DEFAULT 5000,                   -- 계산 타임아웃
    max_execution_time_ms INTEGER DEFAULT 10000,       -- 최대 실행 시간
    retry_count INTEGER DEFAULT 3,                     -- 실패 시 재시도 횟수
    
    -- 🔥 품질 관리
    quality_check_enabled INTEGER DEFAULT 1,
    result_validation TEXT,                             -- JSON: 결과 검증 규칙
    outlier_detection_enabled INTEGER DEFAULT 0,
    
    -- 🔥 로깅 설정
    log_enabled INTEGER DEFAULT 1,
    log_interval_ms INTEGER DEFAULT 5000,
    log_inputs INTEGER DEFAULT 0,                      -- 입력값 로깅 여부
    log_errors INTEGER DEFAULT 1,                      -- 에러 로깅 여부
    
    -- 🔥 알람 연동
    alarm_enabled INTEGER DEFAULT 0,
    high_limit REAL,
    low_limit REAL,
    deadband REAL DEFAULT 0.0,
    
    -- 🔥 감사 필드
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP, is_deleted BOOLEAN DEFAULT 0,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    FOREIGN KEY (script_library_id) REFERENCES script_library(id) ON DELETE SET NULL,
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    -- 🔥 범위별 제약조건
    CONSTRAINT chk_scope_consistency CHECK (
        (scope_type = 'tenant' AND site_id IS NULL AND device_id IS NULL) OR
        (scope_type = 'site' AND site_id IS NOT NULL AND device_id IS NULL) OR
        (scope_type = 'device' AND site_id IS NOT NULL AND device_id IS NOT NULL)
    ),
    CONSTRAINT chk_scope_type CHECK (scope_type IN ('tenant', 'site', 'device')),
    CONSTRAINT chk_data_type CHECK (data_type IN ('bool', 'int', 'float', 'double', 'string')),
    CONSTRAINT chk_calculation_trigger CHECK (calculation_trigger IN ('timer', 'onchange', 'manual', 'event')),
    CONSTRAINT chk_execution_type CHECK (execution_type IN ('javascript', 'formula', 'aggregation', 'external')),
    CONSTRAINT chk_error_handling CHECK (error_handling IN ('return_null', 'return_zero', 'return_previous', 'throw_error'))
);
INSERT INTO virtual_points VALUES(1,1,'site',1,NULL,'Production_Efficiency','Overall production efficiency calculation','const production = getValue("Production_Count"); return (production / 20000) * 100;','float','%',5000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27',0);
INSERT INTO virtual_points VALUES(4,2,'site',3,NULL,'Assembly_Throughput','Assembly line throughput','const cycleTime = 45; return 3600 / cycleTime;','float','units/hour',5000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27',0);
INSERT INTO virtual_points VALUES(5,3,'site',5,NULL,'Demo_Performance','Demo performance index','return Math.sin(Date.now() / 10000) * 50 + 75;','float','%',2000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27',0);
INSERT INTO virtual_points VALUES(6,4,'site',6,NULL,'Test_Metric','Test calculation metric','return Math.random() * 100;','float','%',3000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-16 01:13:27','2026-01-16 01:13:27',0);
INSERT INTO virtual_points VALUES(7,1,'tenant',NULL,NULL,'평균 온실 온도','A동과 B동의 평균 온도 계산','return (inputs.temp_A + inputs.temp_B) / 2;','float',NULL,1000,'timer',1,'온도',NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 02:22:01','2026-01-18 04:45:25',0);
INSERT INTO virtual_points VALUES(8,1,'tenant',NULL,NULL,'총 전력 사용량','메인 배전반 총 전력 합계','return pwr_1 + pwr_2 + pwr_3;','float',NULL,1000,'timer',1,'전력',NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 02:22:01','2026-01-18 02:22:01',0);
INSERT INTO virtual_points VALUES(9,1,'tenant',NULL,NULL,'펌프 가동률','24시간 기준 펌프 가동 효율','return (pump_on_time / 1440) * 100;','float',NULL,1000,'timer',1,'유지보수',NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 02:22:01','2026-01-18 02:22:01',0);
INSERT INTO virtual_points VALUES(10,1,'tenant',NULL,NULL,'라인 A 압력 편차','설정값 대비 현재 압력 차이','return (inputs.temp_A + inputs.temp_B) / 2;','float',NULL,1000,'onchange',0,'압력',NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 02:22:01','2026-01-18 04:40:47',0);
INSERT INTO virtual_points VALUES(11,1,'tenant',NULL,NULL,'임시 테스트 포인트','삭제 테스트용 임시 데이터','return "test";','string',NULL,1000,'manual',0,'Custom',NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 02:22:01','2026-01-18 02:30:09',1);
INSERT INTO virtual_points VALUES(12,1,'tenant',NULL,NULL,'AUTO_TEST_1768712486269','Created by verification script','return 100;','float',NULL,1000,'timer',1,'calculation','[]','javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 05:01:26','2026-01-18 05:01:26',0);
INSERT INTO virtual_points VALUES(13,1,'tenant',NULL,NULL,'AUTO_TEST_1768712523655','Created by verification script','return 100;','float',NULL,1000,'timer',1,'calculation','[]','javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 05:02:03','2026-01-18 05:02:03',0);
INSERT INTO virtual_points VALUES(14,1,'tenant',NULL,NULL,'AUTO_TEST_1768712558118','Created by verification script','return 100;','float',NULL,1000,'timer',1,'calculation','[]','javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 05:02:38','2026-01-18 05:02:38',0);
INSERT INTO virtual_points VALUES(15,1,'tenant',NULL,NULL,'SIMPLE_TEST',NULL,'return 1;','float',NULL,1000,'timer',1,'calculation',NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 05:03:20','2026-01-18 05:03:20',1);
INSERT INTO virtual_points VALUES(16,1,'tenant',NULL,NULL,'SIMPLE_TEST_1768713305794',NULL,'return 1;','float',NULL,1000,'timer',1,'calculation',NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 05:15:05','2026-01-18 05:15:05',1);
INSERT INTO virtual_points VALUES(17,1,'device',1,1,'E2E_Test_VP_1768715940314',NULL,'return inputs.motor_current + 10;','float',NULL,1000,'onchange',1,'test',NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 05:59:00','2026-01-18 05:59:00',0);
INSERT INTO virtual_points VALUES(18,1,'device',1,1,'E2E_Test_VP_1768715997668',NULL,'return inputs.motor_current + 10;','float',NULL,1000,'onchange',1,'test',NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 05:59:57','2026-01-18 05:59:57',0);
INSERT INTO virtual_points VALUES(19,1,'device',1,1,'E2E_Test_VP_1768716013795',NULL,'return inputs.motor_current + 10;','float',NULL,1000,'onchange',1,'test',NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 06:00:13','2026-01-18 06:00:13',0);
INSERT INTO virtual_points VALUES(20,1,'device',1,1,'E2E_Test_VP_1768716099392',NULL,'motor_current + 10','float',NULL,1000,'onchange',1,'test',NULL,'javascript','{"inputs": [{"point_id": 3, "variable": "motor_current"}]}',0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 06:01:39','2026-01-18 06:01:39',0);
INSERT INTO virtual_points VALUES(21,1,'device',1,1,'Updated_VP_21_v2',NULL,'motor_current * 3','float',NULL,1000,'onchange',0,'test',NULL,'javascript','{"inputs":[{"variable":"motor_current","point_id":3},{"variable":"temp_val","point_id":1}]}',0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 09:29:31','2026-01-18 09:33:34',0);
INSERT INTO virtual_points VALUES(22,1,'device',1,1,'Dependencies_Test_VP_1768728861917_updated',NULL,'var1 + var2','float',NULL,1000,'timer',0,'calculation',NULL,'javascript','{"inputs":[{"variable":"var1","point_id":5}]}',0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,NULL,'2026-01-18 09:34:21','2026-01-18 09:34:21',0);
CREATE TABLE virtual_point_inputs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    variable_name VARCHAR(50) NOT NULL,                -- 계산식에서 사용할 변수명 (예: temp1, motor_power)
    
    -- 🔥 다양한 소스 타입 지원
    source_type VARCHAR(20) NOT NULL,                  -- data_point, virtual_point, constant, formula, system
    source_id INTEGER,                                 -- data_points.id 또는 virtual_points.id
    constant_value REAL,                               -- source_type이 'constant'일 때
    source_formula TEXT,                               -- source_type이 'formula'일 때 (간단한 수식)
    
    -- 🔥 데이터 처리 옵션
    data_processing VARCHAR(20) DEFAULT 'current',     -- current, average, min, max, sum, count, stddev
    time_window_seconds INTEGER,                       -- data_processing이 'current'가 아닐 때
    
    -- 🔥 데이터 변환
    scaling_factor REAL DEFAULT 1.0,
    scaling_offset REAL DEFAULT 0.0,
    unit_conversion TEXT,                              -- JSON: 단위 변환 규칙
    
    -- 🔥 품질 관리
    quality_filter VARCHAR(20) DEFAULT 'good_only',    -- good_only, any, good_or_uncertain
    default_value REAL,                                -- 품질이 나쁠 때 사용할 기본값
    
    -- 🔥 캐싱 설정
    cache_enabled INTEGER DEFAULT 1,
    cache_duration_ms INTEGER DEFAULT 1000,
    
    -- 🔥 메타데이터
    description TEXT,
    is_required INTEGER DEFAULT 1,                     -- 필수 입력 여부
    sort_order INTEGER DEFAULT 0,
    
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    UNIQUE(virtual_point_id, variable_name),
    
    -- 🔥 제약조건
    CONSTRAINT chk_source_type CHECK (source_type IN ('data_point', 'virtual_point', 'constant', 'formula', 'system')),
    CONSTRAINT chk_data_processing CHECK (data_processing IN ('current', 'average', 'min', 'max', 'sum', 'count', 'stddev', 'median')),
    CONSTRAINT chk_quality_filter CHECK (quality_filter IN ('good_only', 'any', 'good_or_uncertain'))
);
INSERT INTO virtual_point_inputs VALUES(1,12,'defaultInput','constant',NULL,0.0,NULL,'current',NULL,1.0,0.0,NULL,'good_only',NULL,1,1000,NULL,1,0,'2026-01-18 05:01:26','2026-01-18 05:01:26');
INSERT INTO virtual_point_inputs VALUES(2,13,'defaultInput','constant',NULL,0.0,NULL,'current',NULL,1.0,0.0,NULL,'good_only',NULL,1,1000,NULL,1,0,'2026-01-18 05:02:03','2026-01-18 05:02:03');
INSERT INTO virtual_point_inputs VALUES(3,14,'defaultInput','constant',NULL,0.0,NULL,'current',NULL,1.0,0.0,NULL,'good_only',NULL,1,1000,NULL,1,0,'2026-01-18 05:02:38','2026-01-18 05:02:38');
INSERT INTO virtual_point_inputs VALUES(4,15,'defaultInput','constant',NULL,0.0,NULL,'current',NULL,1.0,0.0,NULL,'good_only',NULL,1,1000,NULL,1,0,'2026-01-18 05:03:20','2026-01-18 05:03:20');
INSERT INTO virtual_point_inputs VALUES(5,16,'defaultInput','constant',NULL,0.0,NULL,'current',NULL,1.0,0.0,NULL,'good_only',NULL,1,1000,NULL,1,0,'2026-01-18 05:15:05','2026-01-18 05:15:05');
INSERT INTO virtual_point_inputs VALUES(6,17,'motor_current','data_point',3,NULL,NULL,'current',NULL,1.0,0.0,NULL,'good_only',NULL,1,1000,NULL,1,0,'2026-01-18 05:59:00','2026-01-18 05:59:00');
INSERT INTO virtual_point_inputs VALUES(7,18,'motor_current','data_point',3,NULL,NULL,'current',NULL,1.0,0.0,NULL,'good_only',NULL,1,1000,NULL,1,0,'2026-01-18 05:59:57','2026-01-18 05:59:57');
INSERT INTO virtual_point_inputs VALUES(8,19,'motor_current','data_point',3,NULL,NULL,'current',NULL,1.0,0.0,NULL,'good_only',NULL,1,1000,NULL,1,0,'2026-01-18 06:00:13','2026-01-18 06:00:13');
INSERT INTO virtual_point_inputs VALUES(9,20,'motor_current','data_point',3,NULL,NULL,'current',NULL,1.0,0.0,NULL,'good_only',NULL,1,1000,NULL,1,0,'2026-01-18 06:01:39','2026-01-18 06:01:39');
INSERT INTO virtual_point_inputs VALUES(11,21,'motor_current','data_point',3,NULL,NULL,'current',NULL,1.0,0.0,NULL,'good_only',NULL,1,1000,NULL,1,0,'2026-01-18 09:33:34','2026-01-18 09:33:34');
INSERT INTO virtual_point_inputs VALUES(12,21,'temp_val','data_point',1,NULL,NULL,'current',NULL,1.0,0.0,NULL,'good_only',NULL,1,1000,NULL,1,0,'2026-01-18 09:33:34','2026-01-18 09:33:34');
INSERT INTO virtual_point_inputs VALUES(15,22,'var1','data_point',5,NULL,NULL,'current',NULL,1.0,0.0,NULL,'good_only',NULL,1,1000,NULL,1,0,'2026-01-18 09:34:21','2026-01-18 09:34:21');
CREATE TABLE virtual_point_values (
    virtual_point_id INTEGER PRIMARY KEY,
    
    -- 🔥 계산 결과값
    value REAL,
    string_value TEXT,                                 -- 문자열 타입 가상포인트용
    raw_result TEXT,                                   -- JSON: 원시 계산 결과
    
    -- 🔥 품질 정보
    quality VARCHAR(20) DEFAULT 'good',
    quality_code INTEGER DEFAULT 1,
    
    -- 🔥 계산 정보
    last_calculated DATETIME DEFAULT CURRENT_TIMESTAMP,
    calculation_duration_ms INTEGER,                   -- 계산 소요 시간
    calculation_error TEXT,                            -- 계산 오류 메시지
    input_values TEXT,                                 -- JSON: 계산에 사용된 입력값들 (디버깅용)
    
    -- 🔥 상태 정보
    is_stale INTEGER DEFAULT 0,                       -- 값이 오래되었는지 여부
    staleness_threshold_ms INTEGER DEFAULT 30000,     -- 만료 임계값
    
    -- 🔥 통계 정보
    calculation_count INTEGER DEFAULT 0,              -- 총 계산 횟수
    error_count INTEGER DEFAULT 0,                    -- 에러 발생 횟수
    success_rate REAL DEFAULT 100.0,                  -- 성공률 (%)
    
    -- 🔥 알람 상태
    alarm_state VARCHAR(20) DEFAULT 'normal',         -- normal, high, low, critical
    alarm_active INTEGER DEFAULT 0,
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    CONSTRAINT chk_quality CHECK (quality IN ('good', 'bad', 'uncertain', 'calculation_error', 'input_error')),
    CONSTRAINT chk_alarm_state CHECK (alarm_state IN ('normal', 'high', 'low', 'critical', 'warning'))
);
INSERT INTO virtual_point_values VALUES(12,NULL,NULL,NULL,'uncertain',1,'2026-01-18 05:01:26',0,NULL,NULL,1,30000,0,0,100.0,'normal',0);
INSERT INTO virtual_point_values VALUES(13,NULL,NULL,NULL,'uncertain',1,'2026-01-18 05:02:03',0,NULL,NULL,1,30000,0,0,100.0,'normal',0);
INSERT INTO virtual_point_values VALUES(14,NULL,NULL,NULL,'uncertain',1,'2026-01-18 05:02:38',0,NULL,NULL,1,30000,0,0,100.0,'normal',0);
INSERT INTO virtual_point_values VALUES(15,NULL,NULL,NULL,'uncertain',1,'2026-01-18 05:03:20',0,NULL,NULL,1,30000,0,0,100.0,'normal',0);
INSERT INTO virtual_point_values VALUES(16,NULL,NULL,NULL,'uncertain',1,'2026-01-18 05:15:05',0,NULL,NULL,1,30000,0,0,100.0,'normal',0);
INSERT INTO virtual_point_values VALUES(17,NULL,NULL,NULL,'uncertain',1,'2026-01-18 05:59:00',0,NULL,NULL,1,30000,0,0,100.0,'normal',0);
INSERT INTO virtual_point_values VALUES(18,NULL,NULL,NULL,'uncertain',1,'2026-01-18 05:59:57',0,NULL,NULL,1,30000,0,0,100.0,'normal',0);
INSERT INTO virtual_point_values VALUES(19,NULL,NULL,NULL,'uncertain',1,'2026-01-18 06:00:13',0,NULL,NULL,1,30000,0,0,100.0,'normal',0);
INSERT INTO virtual_point_values VALUES(20,NULL,NULL,NULL,'uncertain',1,'2026-01-18 06:01:39',0,NULL,NULL,1,30000,0,0,100.0,'normal',0);
INSERT INTO virtual_point_values VALUES(21,NULL,NULL,NULL,'uncertain',1,'2026-01-18 09:33:34',0,NULL,NULL,1,30000,0,0,100.0,'normal',0);
INSERT INTO virtual_point_values VALUES(22,NULL,NULL,NULL,'uncertain',1,'2026-01-18 09:34:21',0,NULL,NULL,1,30000,0,0,100.0,'normal',0);
CREATE TABLE virtual_point_execution_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    
    -- 🔥 실행 정보
    execution_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    execution_duration_ms INTEGER,
    execution_id VARCHAR(50),                          -- 실행 세션 ID
    
    -- 🔥 결과 정보
    result_value TEXT,                                 -- JSON 형태
    result_type VARCHAR(20),                           -- success, error, timeout, cancelled
    input_snapshot TEXT,                               -- JSON 형태: 실행 시점 입력값들
    
    -- 🔥 상태 정보
    success INTEGER DEFAULT 1,
    error_message TEXT,
    error_code VARCHAR(20),
    
    -- 🔥 성능 정보
    memory_usage_kb INTEGER,
    cpu_time_ms INTEGER,
    
    -- 🔥 메타데이터
    trigger_source VARCHAR(20),                       -- timer, manual, onchange, api
    user_id INTEGER,                                   -- 수동 실행한 사용자
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_result_type CHECK (result_type IN ('success', 'error', 'timeout', 'cancelled')),
    CONSTRAINT chk_trigger_source CHECK (trigger_source IN ('timer', 'manual', 'onchange', 'api', 'system'))
);
INSERT INTO virtual_point_execution_history VALUES(1,12,'2026-01-18 05:01:26',0,NULL,'{"action":"created","status":"initialized"}','success',NULL,1,NULL,NULL,NULL,NULL,'system',NULL);
INSERT INTO virtual_point_execution_history VALUES(2,13,'2026-01-18 05:02:03',0,NULL,'{"action":"created","status":"initialized"}','success',NULL,1,NULL,NULL,NULL,NULL,'system',NULL);
INSERT INTO virtual_point_execution_history VALUES(3,14,'2026-01-18 05:02:38',0,NULL,'{"action":"created","status":"initialized"}','success',NULL,1,NULL,NULL,NULL,NULL,'system',NULL);
INSERT INTO virtual_point_execution_history VALUES(4,15,'2026-01-18 05:03:20',0,NULL,'{"action":"created","status":"initialized"}','success',NULL,1,NULL,NULL,NULL,NULL,'system',NULL);
INSERT INTO virtual_point_execution_history VALUES(5,16,'2026-01-18 05:15:05',0,NULL,'{"action":"created","status":"initialized"}','success',NULL,1,NULL,NULL,NULL,NULL,'system',NULL);
INSERT INTO virtual_point_execution_history VALUES(6,17,'2026-01-18 05:59:00',0,NULL,'{"action":"created","status":"initialized"}','success',NULL,1,NULL,NULL,NULL,NULL,'system',NULL);
INSERT INTO virtual_point_execution_history VALUES(7,18,'2026-01-18 05:59:57',0,NULL,'{"action":"created","status":"initialized"}','success',NULL,1,NULL,NULL,NULL,NULL,'system',NULL);
INSERT INTO virtual_point_execution_history VALUES(8,19,'2026-01-18 06:00:13',0,NULL,'{"action":"created","status":"initialized"}','success',NULL,1,NULL,NULL,NULL,NULL,'system',NULL);
INSERT INTO virtual_point_execution_history VALUES(9,20,'2026-01-18 06:01:39',0,NULL,'{"action":"created","status":"initialized"}','success',NULL,1,NULL,NULL,NULL,NULL,'system',NULL);
INSERT INTO virtual_point_execution_history VALUES(10,21,'2026-01-18 09:29:31',0,NULL,'{"action":"created","status":"initialized"}','success',NULL,1,NULL,NULL,NULL,NULL,'system',NULL);
INSERT INTO virtual_point_execution_history VALUES(11,21,'2026-01-18 09:33:34',0,NULL,'{"action":"updated","status":"completed"}','success',NULL,1,NULL,NULL,NULL,NULL,'manual',NULL);
INSERT INTO virtual_point_execution_history VALUES(12,22,'2026-01-18 09:34:21',0,NULL,'{"action":"created","status":"initialized"}','success',NULL,1,NULL,NULL,NULL,NULL,'system',NULL);
INSERT INTO virtual_point_execution_history VALUES(13,22,'2026-01-18 09:34:21',0,NULL,'{"action":"updated","status":"completed"}','success',NULL,1,NULL,NULL,NULL,NULL,'manual',NULL);
CREATE TABLE virtual_point_dependencies (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    
    -- 🔥 의존성 대상
    depends_on_type VARCHAR(20) NOT NULL,             -- 'data_point' or 'virtual_point'
    depends_on_id INTEGER NOT NULL,
    
    -- 🔥 의존성 메타데이터
    dependency_level INTEGER DEFAULT 1,               -- 의존성 깊이 (순환 참조 방지용)
    is_critical INTEGER DEFAULT 1,                    -- 중요 의존성 여부
    fallback_value REAL,                              -- 의존성 실패 시 대체값
    
    -- 🔥 상태 정보
    is_active INTEGER DEFAULT 1,
    last_checked DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    UNIQUE(virtual_point_id, depends_on_type, depends_on_id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_depends_on_type CHECK (depends_on_type IN ('data_point', 'virtual_point', 'system_variable'))
);
CREATE TABLE script_library (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL DEFAULT 0,              -- 0 = 시스템 전역
    
    -- 🔥 스크립트 정보
    category VARCHAR(50) NOT NULL,                     -- math, logic, engineering, conversion, custom
    name VARCHAR(100) NOT NULL,
    display_name VARCHAR(100),
    description TEXT,
    
    -- 🔥 스크립트 코드
    script_code TEXT NOT NULL,
    script_language VARCHAR(20) DEFAULT 'javascript',
    
    -- 🔥 함수 시그니처
    parameters TEXT,                                   -- JSON 형태: [{"name": "x", "type": "number", "required": true}]
    return_type VARCHAR(20) DEFAULT 'number',
    
    -- 🔥 메타데이터
    tags TEXT,                                         -- JSON 배열
    example_usage TEXT,
    documentation TEXT,
    
    -- 🔥 버전 관리
    version VARCHAR(20) DEFAULT '1.0.0',
    is_system INTEGER DEFAULT 0,                      -- 시스템 제공 함수
    is_deprecated INTEGER DEFAULT 0,
    
    -- 🔥 사용 통계
    usage_count INTEGER DEFAULT 0,
    rating REAL DEFAULT 0.0,
    
    -- 🔥 상태 정보
    is_active INTEGER DEFAULT 1,
    is_public INTEGER DEFAULT 0,                      -- 다른 테넌트 사용 가능
    
    -- 🔥 감사 정보
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    UNIQUE(tenant_id, name),
    
    -- 🔥 제약조건
    CONSTRAINT chk_category CHECK (category IN ('math', 'logic', 'engineering', 'conversion', 'utility', 'custom')),
    CONSTRAINT chk_return_type CHECK (return_type IN ('number', 'string', 'boolean', 'object', 'array', 'void'))
);
CREATE TABLE script_library_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    script_id INTEGER NOT NULL,
    
    -- 🔥 버전 정보
    version_number VARCHAR(20) NOT NULL,
    script_code TEXT NOT NULL,
    change_log TEXT,
    
    -- 🔥 호환성 정보
    breaking_change INTEGER DEFAULT 0,
    deprecated INTEGER DEFAULT 0,
    min_system_version VARCHAR(20),
    
    -- 🔥 감사 정보
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (script_id) REFERENCES script_library(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id)
);
CREATE TABLE script_usage_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    script_id INTEGER NOT NULL,
    virtual_point_id INTEGER,
    tenant_id INTEGER NOT NULL,
    
    -- 🔥 사용 정보
    usage_context VARCHAR(50),                         -- 'virtual_point', 'alarm', 'manual', 'test'
    execution_time_ms INTEGER,
    success INTEGER DEFAULT 1,
    error_message TEXT,
    
    -- 🔥 감사 정보
    used_by INTEGER,
    used_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (script_id) REFERENCES script_library(id) ON DELETE CASCADE,
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE SET NULL,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (used_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_usage_context CHECK (usage_context IN ('virtual_point', 'alarm', 'manual', 'test', 'system'))
);
CREATE TABLE script_templates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 🔥 템플릿 정보
    category VARCHAR(50) NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    template_code TEXT NOT NULL,
    
    -- 🔥 템플릿 변수
    variables TEXT,                                    -- JSON 형태: 템플릿 변수 정의
    
    -- 🔥 적용 범위
    industry VARCHAR(50),                              -- manufacturing, building, energy, etc.
    equipment_type VARCHAR(50),                        -- motor, pump, sensor, etc.
    use_case VARCHAR(50),                              -- efficiency, monitoring, control, etc.
    
    -- 🔥 메타데이터
    difficulty_level VARCHAR(20) DEFAULT 'beginner',   -- beginner, intermediate, advanced
    tags TEXT,                                         -- JSON 배열
    documentation TEXT,
    
    -- 🔥 상태 정보
    is_active INTEGER DEFAULT 1,
    popularity_score INTEGER DEFAULT 0,
    
    -- 🔥 감사 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 🔥 제약조건
    CONSTRAINT chk_difficulty_level CHECK (difficulty_level IN ('beginner', 'intermediate', 'advanced'))
);
CREATE TABLE system_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER,                               -- NULL = 시스템 전체 로그
    user_id INTEGER,
    
    -- 🔥 로그 기본 정보
    log_level VARCHAR(10) NOT NULL,                  -- DEBUG, INFO, WARN, ERROR, FATAL
    module VARCHAR(50) NOT NULL,                     -- collector, backend, frontend, database, alarm, etc.
    component VARCHAR(50),                           -- 세부 컴포넌트명
    message TEXT NOT NULL,
    
    -- 🔥 컨텍스트 정보
    ip_address VARCHAR(45),
    user_agent TEXT,
    request_id VARCHAR(50),                          -- 요청 추적 ID
    session_id VARCHAR(100),                         -- 세션 ID
    
    -- 🔥 실행 컨텍스트
    thread_id VARCHAR(20),                           -- 스레드 ID
    process_id INTEGER,                              -- 프로세스 ID
    correlation_id VARCHAR(50),                      -- 상관관계 ID (분산 추적)
    
    -- 🔥 에러 정보 (ERROR/FATAL 레벨용)
    error_code VARCHAR(20),
    stack_trace TEXT,
    exception_type VARCHAR(100),
    
    -- 🔥 성능 정보
    execution_time_ms INTEGER,                       -- 실행 시간
    memory_usage_kb INTEGER,                         -- 메모리 사용량
    cpu_time_ms INTEGER,                             -- CPU 시간
    
    -- 🔥 메타데이터
    details TEXT,                                    -- JSON 형태 (추가 세부 정보)
    tags TEXT,                                       -- JSON 배열 (검색 태그)
    
    -- 🔥 위치 정보
    source_file VARCHAR(255),                        -- 소스 파일명
    source_line INTEGER,                             -- 소스 라인 번호
    source_function VARCHAR(100),                    -- 함수명
    
    -- 🔥 감사 정보
    hostname VARCHAR(100),                           -- 로그 생성 호스트
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE SET NULL,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL,
    
    -- 🔥 제약조건
    CONSTRAINT chk_log_level CHECK (log_level IN ('DEBUG', 'INFO', 'WARN', 'ERROR', 'FATAL'))
);
INSERT INTO system_logs VALUES(1,NULL,NULL,'INFO','database',NULL,'Complete initial data loading with C++ schema compatibility',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"tables_populated": 12, "protocols": 5, "devices": 11, "data_points": 17, "current_values": 17, "device_settings": 6, "device_status": 6, "virtual_points": 6, "alarm_rules": 7, "js_functions": 5, "schema_version": "2.1.0"}',NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27');
INSERT INTO system_logs VALUES(2,NULL,NULL,'INFO','protocols',NULL,'Protocol support initialized',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"modbus_tcp": 1, "bacnet": 2, "mqtt": 3, "ethernet_ip": 4, "modbus_rtu": 5}',NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27');
INSERT INTO system_logs VALUES(3,NULL,NULL,'INFO','devices',NULL,'Sample devices created with protocol_id foreign keys and complete settings',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"smart_factory": 6, "global_manufacturing": 2, "demo": 2, "test": 1, "total": 11, "all_configured": true}',NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27');
INSERT INTO system_logs VALUES(4,NULL,NULL,'INFO','datapoints',NULL,'Data points created with C++ SQLQueries.h compatibility',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"total_points": 17, "log_enabled": 17, "data_types": ["BOOL", "UINT16", "UINT32", "FLOAT32"], "schema_compatible": true}',NULL,NULL,NULL,NULL,NULL,'2026-01-16 01:13:27');
CREATE TABLE user_activities (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER,
    tenant_id INTEGER,
    
    -- 🔥 활동 정보
    action VARCHAR(50) NOT NULL,                     -- login, logout, create, update, delete, view, export, etc.
    resource_type VARCHAR(50),                       -- device, data_point, alarm, user, system_setting, etc.
    resource_id INTEGER,
    resource_name VARCHAR(100),                      -- 리소스 이름 (빠른 검색용)
    
    -- 🔥 변경 정보 (create/update/delete 액션용)
    old_values TEXT,                                 -- JSON 형태 (변경 전 값)
    new_values TEXT,                                 -- JSON 형태 (변경 후 값)
    changed_fields TEXT,                             -- JSON 배열 (변경된 필드 목록)
    
    -- 🔥 요청 정보
    http_method VARCHAR(10),                         -- GET, POST, PUT, DELETE
    endpoint VARCHAR(255),                           -- API 엔드포인트
    query_params TEXT,                               -- JSON 형태 (쿼리 파라미터)
    request_body TEXT,                               -- JSON 형태 (요청 본문)
    response_code INTEGER,                           -- HTTP 응답 코드
    
    -- 🔥 세션 정보
    session_id VARCHAR(100),
    ip_address VARCHAR(45),
    user_agent TEXT,
    referer VARCHAR(255),
    
    -- 🔥 지리적 정보
    country VARCHAR(2),                              -- 국가 코드
    city VARCHAR(100),
    timezone VARCHAR(50),
    
    -- 🔥 결과 정보
    success INTEGER DEFAULT 1,                      -- 성공 여부
    error_message TEXT,                              -- 에러 메시지
    warning_messages TEXT,                           -- JSON 배열 (경고 메시지들)
    
    -- 🔥 성능 정보
    processing_time_ms INTEGER,                      -- 처리 시간
    affected_rows INTEGER,                           -- 영향받은 레코드 수
    
    -- 🔥 보안 정보
    risk_score INTEGER DEFAULT 0,                   -- 위험도 점수 (0-100)
    security_flags TEXT,                             -- JSON 배열 (보안 플래그)
    
    -- 🔥 메타데이터
    details TEXT,                                    -- JSON 형태 (추가 정보)
    tags TEXT,                                       -- JSON 배열 (분류 태그)
    
    -- 🔥 감사 정보
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE SET NULL,
    
    -- 🔥 제약조건
    CONSTRAINT chk_action CHECK (action IN ('login', 'logout', 'create', 'read', 'update', 'delete', 'view', 'export', 'import', 'execute', 'approve', 'reject')),
    CONSTRAINT chk_http_method CHECK (http_method IN ('GET', 'POST', 'PUT', 'PATCH', 'DELETE', 'HEAD', 'OPTIONS'))
);
CREATE TABLE communication_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER,
    data_point_id INTEGER,
    
    -- 🔥 통신 기본 정보
    direction VARCHAR(10) NOT NULL,                  -- request, response, notification
    protocol VARCHAR(20) NOT NULL,                   -- MODBUS_TCP, MODBUS_RTU, MQTT, BACNET, OPCUA
    operation VARCHAR(50),                           -- read_holding_registers, write_single_coil, publish, subscribe, etc.
    
    -- 🔥 프로토콜별 정보
    function_code INTEGER,                           -- Modbus 함수 코드
    address INTEGER,                                 -- 시작 주소
    register_count INTEGER,                          -- 레지스터 수
    slave_id INTEGER,                                -- Modbus 슬레이브 ID
    topic VARCHAR(255),                              -- MQTT 토픽
    qos INTEGER,                                     -- MQTT QoS
    
    -- 🔥 통신 데이터
    raw_data TEXT,                                   -- 원시 통신 데이터 (HEX)
    decoded_data TEXT,                               -- 해석된 데이터 (JSON)
    data_size INTEGER,                               -- 데이터 크기 (바이트)
    
    -- 🔥 결과 정보
    success INTEGER,                                 -- 성공 여부
    error_code INTEGER,                              -- 에러 코드
    error_message TEXT,                              -- 에러 메시지
    retry_count INTEGER DEFAULT 0,                  -- 재시도 횟수
    
    -- 🔥 성능 정보
    response_time INTEGER,                           -- 응답 시간 (밀리초)
    queue_time_ms INTEGER,                           -- 큐 대기 시간
    processing_time_ms INTEGER,                      -- 처리 시간
    network_latency_ms INTEGER,                      -- 네트워크 지연
    
    -- 🔥 품질 정보
    signal_strength INTEGER,                         -- 신호 강도 (무선 통신용)
    packet_loss_rate REAL,                          -- 패킷 손실률
    bit_error_rate REAL,                            -- 비트 에러율
    
    -- 🔥 세션 정보
    connection_id VARCHAR(50),                       -- 연결 ID
    session_id VARCHAR(50),                          -- 세션 ID
    sequence_number INTEGER,                         -- 시퀀스 번호
    
    -- 🔥 메타데이터
    details TEXT,                                    -- JSON 형태 (추가 정보)
    tags TEXT,                                       -- JSON 배열
    
    -- 🔥 감사 정보
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    edge_server_id INTEGER,                          -- 통신을 수행한 엣지 서버
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    FOREIGN KEY (data_point_id) REFERENCES data_points(id) ON DELETE SET NULL,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
    
    -- 🔥 제약조건
    CONSTRAINT chk_direction CHECK (direction IN ('request', 'response', 'notification', 'heartbeat')),
    CONSTRAINT chk_protocol CHECK (protocol IN ('MODBUS_TCP', 'MODBUS_RTU', 'MQTT', 'BACNET', 'OPCUA', 'ETHERNET_IP', 'HTTP'))
);
CREATE TABLE data_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    point_id INTEGER NOT NULL,
    
    -- 🔥 값 정보
    value DECIMAL(15,4),                             -- 숫자 값
    string_value TEXT,                               -- 문자열 값
    bool_value INTEGER,                              -- 불린 값 (0 또는 1)
    raw_value DECIMAL(15,4),                         -- 스케일링 전 원시값
    
    -- 🔥 품질 정보
    quality VARCHAR(20),                             -- good, bad, uncertain, not_connected
    quality_code INTEGER,                            -- 숫자 품질 코드
    
    -- 🔥 시간 정보
    timestamp DATETIME NOT NULL,                     -- 값 타임스탬프
    source_timestamp DATETIME,                       -- 소스 타임스탬프
    
    -- 🔥 메타데이터
    change_type VARCHAR(20) DEFAULT 'value_change',  -- value_change, quality_change, manual_entry, calculated
    source VARCHAR(50) DEFAULT 'collector',          -- collector, manual, calculated, imported
    
    -- 🔥 압축 정보
    is_compressed INTEGER DEFAULT 0,                -- 압축 저장 여부
    compression_method VARCHAR(20),                  -- 압축 방법
    original_size INTEGER,                           -- 원본 크기
    
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    CONSTRAINT chk_quality CHECK (quality IN ('good', 'bad', 'uncertain', 'not_connected', 'device_failure', 'sensor_failure')),
    CONSTRAINT chk_change_type CHECK (change_type IN ('value_change', 'quality_change', 'manual_entry', 'calculated', 'imported')),
    CONSTRAINT chk_source CHECK (source IN ('collector', 'manual', 'calculated', 'imported', 'simulated'))
);
CREATE TABLE virtual_point_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    
    -- 🔥 계산 결과
    value DECIMAL(15,4),
    string_value TEXT,
    quality VARCHAR(20),
    
    -- 🔥 계산 정보
    calculation_time_ms INTEGER,                     -- 계산 소요 시간
    input_values TEXT,                               -- JSON: 계산에 사용된 입력값들
    formula_used TEXT,                               -- 사용된 수식
    
    -- 🔥 시간 정보
    timestamp DATETIME NOT NULL,
    calculation_timestamp DATETIME,                  -- 계산 시작 시간
    
    -- 🔥 메타데이터
    trigger_reason VARCHAR(50),                      -- timer, value_change, manual, api
    error_message TEXT,                              -- 계산 에러 메시지
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    CONSTRAINT chk_vp_quality CHECK (quality IN ('good', 'bad', 'uncertain', 'calculation_error')),
    CONSTRAINT chk_trigger_reason CHECK (trigger_reason IN ('timer', 'value_change', 'manual', 'api', 'system'))
);
CREATE TABLE alarm_event_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    occurrence_id INTEGER NOT NULL,
    rule_id INTEGER NOT NULL,
    tenant_id INTEGER NOT NULL,
    
    -- 🔥 이벤트 정보
    event_type VARCHAR(30) NOT NULL,                -- triggered, acknowledged, cleared, escalated, suppressed, etc.
    previous_state VARCHAR(20),                     -- 이전 상태
    new_state VARCHAR(20),                          -- 새로운 상태
    
    -- 🔥 값 정보
    trigger_value TEXT,                             -- JSON: 트리거 값
    threshold_value TEXT,                           -- JSON: 임계값
    deviation REAL,                                 -- 편차
    
    -- 🔥 사용자 정보
    user_id INTEGER,                                -- 액션을 수행한 사용자
    user_comment TEXT,                              -- 사용자 코멘트
    
    -- 🔥 시스템 정보
    auto_action INTEGER DEFAULT 0,                 -- 자동 액션 여부
    escalation_level INTEGER DEFAULT 0,            -- 에스컬레이션 단계
    notification_sent INTEGER DEFAULT 0,           -- 알림 발송 여부
    
    -- 🔥 메타데이터
    details TEXT,                                   -- JSON: 추가 세부 정보
    context_data TEXT,                              -- JSON: 컨텍스트 데이터
    
    -- 🔥 감사 정보
    event_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    source_system VARCHAR(50) DEFAULT 'collector',  -- 이벤트 소스
    
    FOREIGN KEY (occurrence_id) REFERENCES alarm_occurrences(id) ON DELETE CASCADE,
    FOREIGN KEY (rule_id) REFERENCES alarm_rules(id) ON DELETE CASCADE,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL,
    
    -- 🔥 제약조건
    CONSTRAINT chk_event_type CHECK (event_type IN ('triggered', 'acknowledged', 'cleared', 'escalated', 'suppressed', 'shelved', 'unshelved', 'expired', 'updated')),
    CONSTRAINT chk_prev_state CHECK (previous_state IN ('active', 'acknowledged', 'cleared', 'suppressed', 'shelved', 'expired')),
    CONSTRAINT chk_new_state CHECK (new_state IN ('active', 'acknowledged', 'cleared', 'suppressed', 'shelved', 'expired'))
);
CREATE TABLE performance_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 🔥 성능 카테고리
    metric_category VARCHAR(50) NOT NULL,           -- system, database, network, application
    metric_name VARCHAR(100) NOT NULL,              -- cpu_usage, memory_usage, response_time, etc.
    metric_value REAL NOT NULL,
    metric_unit VARCHAR(20),                        -- %, MB, ms, count, etc.
    
    -- 🔥 컨텍스트 정보
    hostname VARCHAR(100),
    process_name VARCHAR(100),
    component VARCHAR(50),                          -- collector, backend, database, etc.
    
    -- 🔥 샘플링 정보
    sample_interval_sec INTEGER DEFAULT 60,         -- 샘플링 간격
    aggregation_type VARCHAR(20) DEFAULT 'instant', -- instant, average, min, max, sum
    sample_count INTEGER DEFAULT 1,                 -- 집계된 샘플 수
    
    -- 🔥 메타데이터
    details TEXT,                                   -- JSON: 추가 메트릭 정보
    tags TEXT,                                      -- JSON 배열: 태그
    
    -- 🔥 시간 정보
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 🔥 제약조건
    CONSTRAINT chk_metric_category CHECK (metric_category IN ('system', 'database', 'network', 'application', 'security')),
    CONSTRAINT chk_aggregation_type CHECK (aggregation_type IN ('instant', 'average', 'min', 'max', 'sum', 'count'))
);
CREATE TABLE audit_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER,
    user_id INTEGER,
    
    -- 🔥 액션 정보
    action VARCHAR(50) NOT NULL,                     -- CREATE, UPDATE, DELETE, EXECUTE, etc.
    entity_type VARCHAR(50) NOT NULL,                -- DEVICE, DATA_POINT, PROTOCOL, SITE, USER, etc.
    entity_id INTEGER,
    entity_name VARCHAR(100),
    
    -- 🔥 변경 상세
    old_value TEXT,                                  -- JSON 형태 (변경 전)
    new_value TEXT,                                  -- JSON 형태 (변경 후)
    change_summary TEXT,                             -- 변경 사항 요약
    
    -- 🔥 요청 정보
    ip_address VARCHAR(45),
    user_agent TEXT,
    request_id VARCHAR(50),
    
    -- 🔥 메타데이터
    severity VARCHAR(20) DEFAULT 'info',             -- info, warning, critical
    details TEXT,                                    -- JSON 형태
    
    -- 🔥 감사 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL
);
CREATE TABLE export_profiles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL UNIQUE,
    description TEXT,
    is_enabled BOOLEAN DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by VARCHAR(50),
    point_count INTEGER DEFAULT 0,
    last_exported_at DATETIME
);
CREATE TABLE export_profile_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER NOT NULL,
    point_id INTEGER NOT NULL,
    display_order INTEGER DEFAULT 0,
    display_name VARCHAR(200),
    is_enabled BOOLEAN DEFAULT 1,
    added_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    added_by VARCHAR(50),
    
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE CASCADE,
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    UNIQUE(profile_id, point_id)
);
CREATE TABLE protocol_services (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER NOT NULL,
    service_type VARCHAR(20) NOT NULL,
    service_name VARCHAR(100) NOT NULL,
    description TEXT,
    is_enabled BOOLEAN DEFAULT 1,
    config TEXT NOT NULL,
    active_connections INTEGER DEFAULT 0,
    total_requests INTEGER DEFAULT 0,
    last_request_at DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE CASCADE
);
CREATE TABLE protocol_mappings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    service_id INTEGER NOT NULL,
    point_id INTEGER NOT NULL,
    external_identifier VARCHAR(200) NOT NULL,
    external_name VARCHAR(200),
    external_description VARCHAR(500),
    conversion_config TEXT,
    protocol_config TEXT,
    is_enabled BOOLEAN DEFAULT 1,
    read_count INTEGER DEFAULT 0,
    write_count INTEGER DEFAULT 0,
    last_read_at DATETIME,
    last_write_at DATETIME,
    error_count INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (service_id) REFERENCES protocol_services(id) ON DELETE CASCADE,
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    UNIQUE(service_id, external_identifier)
);
CREATE TABLE payload_templates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL UNIQUE,
    system_type VARCHAR(50) NOT NULL,
    description TEXT,
    template_json TEXT NOT NULL,
    is_active BOOLEAN DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
INSERT INTO payload_templates VALUES(1,'Insite 기본 템플릿','insite','Insite 빌딩 모니터링 시스템용 기본 템플릿',replace('{\n    "building_id": "{{building_id}}",\n    "controlpoint": "{{target_field_name}}",\n    "description": "{{target_description}}",\n    "value": "{{converted_value}}",\n    "time": "{{timestamp_iso8601}}",\n    "status": "{{alarm_status}}"\n}','\n',char(10)),1,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO payload_templates VALUES(2,'HDC 기본 템플릿','hdc','HDC 빌딩 시스템용 기본 템플릿',replace('{\n    "building_id": "{{building_id}}",\n    "point_id": "{{target_field_name}}",\n    "data": {\n        "value": "{{converted_value}}",\n        "timestamp": "{{timestamp_unix_ms}}"\n    },\n    "metadata": {\n        "description": "{{target_description}}",\n        "alarm_status": "{{alarm_status}}",\n        "source": "PulseOne-ExportGateway"\n    }\n}','\n',char(10)),1,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO payload_templates VALUES(3,'BEMS 기본 템플릿','bems','BEMS 에너지 관리 시스템용 기본 템플릿',replace('{\n    "buildingId": "{{building_id}}",\n    "sensorName": "{{target_field_name}}",\n    "sensorValue": "{{converted_value}}",\n    "timestamp": "{{timestamp_iso8601}}",\n    "description": "{{target_description}}",\n    "alarmLevel": "{{alarm_status}}"\n}','\n',char(10)),1,'2026-01-16 01:13:27','2026-01-16 01:13:27');
INSERT INTO payload_templates VALUES(4,'Generic 기본 템플릿','custom','일반 범용 템플릿',replace('{\n    "building_id": "{{building_id}}",\n    "point_name": "{{point_name}}",\n    "value": "{{value}}",\n    "converted_value": "{{converted_value}}",\n    "timestamp": "{{timestamp_iso8601}}",\n    "alarm_flag": "{{alarm_flag}}",\n    "status": "{{status}}",\n    "description": "{{description}}",\n    "alarm_status": "{{alarm_status}}",\n    "mapped_field": "{{target_field_name}}",\n    "mapped_description": "{{target_description}}",\n    "source": "PulseOne-ExportGateway"\n}','\n',char(10)),1,'2026-01-16 01:13:27','2026-01-16 01:13:27');
CREATE TABLE export_targets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER,
    
    -- 기본 정보
    name VARCHAR(100) NOT NULL UNIQUE,
    target_type VARCHAR(20) NOT NULL,
    description TEXT,
    
    -- 설정 정보
    config TEXT NOT NULL,
    is_enabled BOOLEAN DEFAULT 1,
    
    -- 🆕 템플릿 참조 (v2.2 추가)
    template_id INTEGER,
    
    -- 전송 옵션
    export_mode VARCHAR(20) DEFAULT 'on_change',
    export_interval INTEGER DEFAULT 0,
    batch_size INTEGER DEFAULT 100,
    
    -- 메타 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE SET NULL,
    FOREIGN KEY (template_id) REFERENCES payload_templates(id) ON DELETE SET NULL
);
CREATE TABLE export_target_mappings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    target_id INTEGER NOT NULL,
    point_id INTEGER NOT NULL,
    target_field_name VARCHAR(200),
    target_description VARCHAR(500),
    conversion_config TEXT,
    is_enabled BOOLEAN DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (target_id) REFERENCES export_targets(id) ON DELETE CASCADE,
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    UNIQUE(target_id, point_id)
);
CREATE TABLE export_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 기본 분류
    log_type VARCHAR(20) NOT NULL,
    
    -- 관계 (FK)
    service_id INTEGER,
    target_id INTEGER,
    mapping_id INTEGER,
    point_id INTEGER,
    
    -- 데이터
    source_value TEXT,
    converted_value TEXT,
    
    -- 결과
    status VARCHAR(20) NOT NULL,
    http_status_code INTEGER,
    
    -- 에러 정보
    error_message TEXT,
    error_code VARCHAR(50),
    response_data TEXT,
    
    -- 성능 정보
    processing_time_ms INTEGER,
    
    -- 메타 정보
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    client_info TEXT,
    
    FOREIGN KEY (service_id) REFERENCES protocol_services(id) ON DELETE SET NULL,
    FOREIGN KEY (target_id) REFERENCES export_targets(id) ON DELETE SET NULL,
    FOREIGN KEY (mapping_id) REFERENCES protocol_mappings(id) ON DELETE SET NULL
);
CREATE TABLE export_schedules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER,
    target_id INTEGER NOT NULL,
    schedule_name VARCHAR(100) NOT NULL,
    description TEXT,
    cron_expression VARCHAR(100) NOT NULL,
    timezone VARCHAR(50) DEFAULT 'UTC',
    data_range VARCHAR(20) DEFAULT 'day',
    lookback_periods INTEGER DEFAULT 1,
    is_enabled BOOLEAN DEFAULT 1,
    last_run_at DATETIME,
    last_status VARCHAR(20),
    next_run_at DATETIME,
    total_runs INTEGER DEFAULT 0,
    successful_runs INTEGER DEFAULT 0,
    failed_runs INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE SET NULL,
    FOREIGN KEY (target_id) REFERENCES export_targets(id) ON DELETE CASCADE
);
CREATE TABLE virtual_point_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    point_id INTEGER NOT NULL,
    action VARCHAR(50) NOT NULL,
    previous_state TEXT,
    new_state TEXT,
    user_id INTEGER,
    details TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (point_id) REFERENCES virtual_points(id) ON DELETE CASCADE
);
INSERT INTO virtual_point_logs VALUES(6,10,'CREATE',NULL,NULL,NULL,'가상포인트 생성됨 (Initial creation)','2026-01-16 04:40:47');
INSERT INTO virtual_point_logs VALUES(7,10,'UPDATE',NULL,NULL,NULL,'실행 주기 변경: 500ms -> 1000ms','2026-01-17 04:40:47');
INSERT INTO virtual_point_logs VALUES(8,10,'DISABLE',NULL,NULL,NULL,'연산 일시 정지 (Maintenance Check)','2026-01-17 23:40:47');
INSERT INTO virtual_point_logs VALUES(9,10,'ENABLE',NULL,NULL,NULL,'연산 재개 (System Online)','2026-01-18 03:40:47');
INSERT INTO virtual_point_logs VALUES(10,10,'UPDATE',NULL,NULL,NULL,'수식 변경: (temp_A + temp_B) / 2','2026-01-18 04:35:47');
INSERT INTO virtual_point_logs VALUES(16,7,'CREATE',NULL,NULL,NULL,'가상포인트 생성됨 (Initial creation)','2026-01-16 04:45:25');
INSERT INTO virtual_point_logs VALUES(17,7,'UPDATE',NULL,NULL,NULL,'실행 주기 변경: 500ms -> 1000ms','2026-01-17 04:45:25');
INSERT INTO virtual_point_logs VALUES(18,7,'DISABLE',NULL,NULL,NULL,'연산 일시 정지 (Maintenance Check)','2026-01-17 23:45:25');
INSERT INTO virtual_point_logs VALUES(19,7,'ENABLE',NULL,NULL,NULL,'연산 재개 (System Online)','2026-01-18 03:45:25');
INSERT INTO virtual_point_logs VALUES(20,7,'UPDATE',NULL,NULL,NULL,'수식 변경: (temp_A + temp_B) / 2','2026-01-18 04:40:25');
INSERT INTO virtual_point_logs VALUES(21,12,'CREATE',NULL,'{"id":12,"tenant_id":1,"scope_type":"tenant","site_id":null,"device_id":null,"name":"AUTO_TEST_1768712486269","description":"Created by verification script","formula":"return 100;","data_type":"float","unit":null,"calculation_interval":1000,"calculation_trigger":"timer","is_enabled":true,"category":"calculation","tags":[],"execution_type":"javascript","dependencies":[],"cache_duration_ms":0,"error_handling":"return_null","last_error":null,"execution_count":0,"avg_execution_time_ms":0,"last_execution_time":null,"script_library_id":null,"timeout_ms":5000,"max_execution_time_ms":10000,"retry_count":3,"quality_check_enabled":1,"result_validation":null,"outlier_detection_enabled":0,"log_enabled":1,"log_interval_ms":5000,"log_inputs":0,"log_errors":1,"alarm_enabled":0,"high_limit":null,"low_limit":null,"deadband":0,"created_by":null,"created_at":"2026-01-18 05:01:26","updated_at":"2026-01-18 05:01:26","is_deleted":0,"inputs":[{"id":1,"virtual_point_id":12,"variable_name":"defaultInput","source_type":"constant","source_id":null,"constant_value":0,"source_formula":null,"data_processing":"current","time_window_seconds":null,"scaling_factor":1,"scaling_offset":0,"unit_conversion":null,"quality_filter":"good_only","default_value":null,"cache_enabled":1,"cache_duration_ms":1000,"description":null,"is_required":1,"sort_order":0,"created_at":"2026-01-18 05:01:26","updated_at":"2026-01-18 05:01:26"}],"currentValue":{"virtual_point_id":12,"value":null,"string_value":null,"raw_result":null,"quality":"uncertain","quality_code":1,"last_calculated":"2026-01-18 05:01:26","calculation_duration_ms":0,"calculation_error":null,"input_values":null,"is_stale":1,"staleness_threshold_ms":30000,"calculation_count":0,"error_count":0,"success_rate":100,"alarm_state":"normal","alarm_active":0}}',NULL,'Virtual Point Created','2026-01-18 05:01:26');
INSERT INTO virtual_point_logs VALUES(22,13,'CREATE',NULL,'{"id":13,"tenant_id":1,"scope_type":"tenant","site_id":null,"device_id":null,"name":"AUTO_TEST_1768712523655","description":"Created by verification script","formula":"return 100;","data_type":"float","unit":null,"calculation_interval":1000,"calculation_trigger":"timer","is_enabled":true,"category":"calculation","tags":[],"execution_type":"javascript","dependencies":[],"cache_duration_ms":0,"error_handling":"return_null","last_error":null,"execution_count":0,"avg_execution_time_ms":0,"last_execution_time":null,"script_library_id":null,"timeout_ms":5000,"max_execution_time_ms":10000,"retry_count":3,"quality_check_enabled":1,"result_validation":null,"outlier_detection_enabled":0,"log_enabled":1,"log_interval_ms":5000,"log_inputs":0,"log_errors":1,"alarm_enabled":0,"high_limit":null,"low_limit":null,"deadband":0,"created_by":null,"created_at":"2026-01-18 05:02:03","updated_at":"2026-01-18 05:02:03","is_deleted":0,"inputs":[{"id":2,"virtual_point_id":13,"variable_name":"defaultInput","source_type":"constant","source_id":null,"constant_value":0,"source_formula":null,"data_processing":"current","time_window_seconds":null,"scaling_factor":1,"scaling_offset":0,"unit_conversion":null,"quality_filter":"good_only","default_value":null,"cache_enabled":1,"cache_duration_ms":1000,"description":null,"is_required":1,"sort_order":0,"created_at":"2026-01-18 05:02:03","updated_at":"2026-01-18 05:02:03"}],"currentValue":{"virtual_point_id":13,"value":null,"string_value":null,"raw_result":null,"quality":"uncertain","quality_code":1,"last_calculated":"2026-01-18 05:02:03","calculation_duration_ms":0,"calculation_error":null,"input_values":null,"is_stale":1,"staleness_threshold_ms":30000,"calculation_count":0,"error_count":0,"success_rate":100,"alarm_state":"normal","alarm_active":0}}',NULL,'Virtual Point Created','2026-01-18 05:02:03');
INSERT INTO virtual_point_logs VALUES(23,14,'CREATE',NULL,'{"id":14,"tenant_id":1,"scope_type":"tenant","site_id":null,"device_id":null,"name":"AUTO_TEST_1768712558118","description":"Created by verification script","formula":"return 100;","data_type":"float","unit":null,"calculation_interval":1000,"calculation_trigger":"timer","is_enabled":true,"category":"calculation","tags":[],"execution_type":"javascript","dependencies":[],"cache_duration_ms":0,"error_handling":"return_null","last_error":null,"execution_count":0,"avg_execution_time_ms":0,"last_execution_time":null,"script_library_id":null,"timeout_ms":5000,"max_execution_time_ms":10000,"retry_count":3,"quality_check_enabled":1,"result_validation":null,"outlier_detection_enabled":0,"log_enabled":1,"log_interval_ms":5000,"log_inputs":0,"log_errors":1,"alarm_enabled":0,"high_limit":null,"low_limit":null,"deadband":0,"created_by":null,"created_at":"2026-01-18 05:02:38","updated_at":"2026-01-18 05:02:38","is_deleted":0,"inputs":[{"id":3,"virtual_point_id":14,"variable_name":"defaultInput","source_type":"constant","source_id":null,"constant_value":0,"source_formula":null,"data_processing":"current","time_window_seconds":null,"scaling_factor":1,"scaling_offset":0,"unit_conversion":null,"quality_filter":"good_only","default_value":null,"cache_enabled":1,"cache_duration_ms":1000,"description":null,"is_required":1,"sort_order":0,"created_at":"2026-01-18 05:02:38","updated_at":"2026-01-18 05:02:38"}],"currentValue":{"virtual_point_id":14,"value":null,"string_value":null,"raw_result":null,"quality":"uncertain","quality_code":1,"last_calculated":"2026-01-18 05:02:38","calculation_duration_ms":0,"calculation_error":null,"input_values":null,"is_stale":1,"staleness_threshold_ms":30000,"calculation_count":0,"error_count":0,"success_rate":100,"alarm_state":"normal","alarm_active":0}}',NULL,'Virtual Point Created','2026-01-18 05:02:38');
INSERT INTO virtual_point_logs VALUES(24,15,'CREATE',NULL,'{"id":15,"tenant_id":1,"scope_type":"tenant","site_id":null,"device_id":null,"name":"SIMPLE_TEST","description":null,"formula":"return 1;","data_type":"float","unit":null,"calculation_interval":1000,"calculation_trigger":"timer","is_enabled":true,"category":"calculation","tags":[],"execution_type":"javascript","dependencies":[],"cache_duration_ms":0,"error_handling":"return_null","last_error":null,"execution_count":0,"avg_execution_time_ms":0,"last_execution_time":null,"script_library_id":null,"timeout_ms":5000,"max_execution_time_ms":10000,"retry_count":3,"quality_check_enabled":1,"result_validation":null,"outlier_detection_enabled":0,"log_enabled":1,"log_interval_ms":5000,"log_inputs":0,"log_errors":1,"alarm_enabled":0,"high_limit":null,"low_limit":null,"deadband":0,"created_by":null,"created_at":"2026-01-18 05:03:20","updated_at":"2026-01-18 05:03:20","is_deleted":0,"inputs":[{"id":4,"virtual_point_id":15,"variable_name":"defaultInput","source_type":"constant","source_id":null,"constant_value":0,"source_formula":null,"data_processing":"current","time_window_seconds":null,"scaling_factor":1,"scaling_offset":0,"unit_conversion":null,"quality_filter":"good_only","default_value":null,"cache_enabled":1,"cache_duration_ms":1000,"description":null,"is_required":1,"sort_order":0,"created_at":"2026-01-18 05:03:20","updated_at":"2026-01-18 05:03:20"}],"currentValue":{"virtual_point_id":15,"value":null,"string_value":null,"raw_result":null,"quality":"uncertain","quality_code":1,"last_calculated":"2026-01-18 05:03:20","calculation_duration_ms":0,"calculation_error":null,"input_values":null,"is_stale":1,"staleness_threshold_ms":30000,"calculation_count":0,"error_count":0,"success_rate":100,"alarm_state":"normal","alarm_active":0}}',NULL,'Virtual Point Created','2026-01-18 05:03:20');
INSERT INTO virtual_point_logs VALUES(25,15,'DELETE','{"id":15,"tenant_id":1,"scope_type":"tenant","site_id":null,"device_id":null,"name":"SIMPLE_TEST","description":null,"formula":"return 1;","data_type":"float","unit":null,"calculation_interval":1000,"calculation_trigger":"timer","is_enabled":true,"category":"calculation","tags":[],"execution_type":"javascript","dependencies":[],"cache_duration_ms":0,"error_handling":"return_null","last_error":null,"execution_count":0,"avg_execution_time_ms":0,"last_execution_time":null,"script_library_id":null,"timeout_ms":5000,"max_execution_time_ms":10000,"retry_count":3,"quality_check_enabled":1,"result_validation":null,"outlier_detection_enabled":0,"log_enabled":1,"log_interval_ms":5000,"log_inputs":0,"log_errors":1,"alarm_enabled":0,"high_limit":null,"low_limit":null,"deadband":0,"created_by":null,"created_at":"2026-01-18 05:03:20","updated_at":"2026-01-18 05:03:20","is_deleted":0,"inputs":[{"id":4,"virtual_point_id":15,"variable_name":"defaultInput","source_type":"constant","source_id":null,"constant_value":0,"source_formula":null,"data_processing":"current","time_window_seconds":null,"scaling_factor":1,"scaling_offset":0,"unit_conversion":null,"quality_filter":"good_only","default_value":null,"cache_enabled":1,"cache_duration_ms":1000,"description":null,"is_required":1,"sort_order":0,"created_at":"2026-01-18 05:03:20","updated_at":"2026-01-18 05:03:20"}],"currentValue":{"virtual_point_id":15,"value":null,"string_value":null,"raw_result":null,"quality":"uncertain","quality_code":1,"last_calculated":"2026-01-18 05:03:20","calculation_duration_ms":0,"calculation_error":null,"input_values":null,"is_stale":1,"staleness_threshold_ms":30000,"calculation_count":0,"error_count":0,"success_rate":100,"alarm_state":"normal","alarm_active":0}}','{"is_deleted":true}',NULL,'Virtual Point Soft Deleted','2026-01-18 05:03:20');
INSERT INTO virtual_point_logs VALUES(26,16,'CREATE',NULL,'{"id":16,"tenant_id":1,"scope_type":"tenant","site_id":null,"device_id":null,"name":"SIMPLE_TEST_1768713305794","description":null,"formula":"return 1;","data_type":"float","unit":null,"calculation_interval":1000,"calculation_trigger":"timer","is_enabled":true,"category":"calculation","tags":[],"execution_type":"javascript","dependencies":[],"cache_duration_ms":0,"error_handling":"return_null","last_error":null,"execution_count":0,"avg_execution_time_ms":0,"last_execution_time":null,"script_library_id":null,"timeout_ms":5000,"max_execution_time_ms":10000,"retry_count":3,"quality_check_enabled":1,"result_validation":null,"outlier_detection_enabled":0,"log_enabled":1,"log_interval_ms":5000,"log_inputs":0,"log_errors":1,"alarm_enabled":0,"high_limit":null,"low_limit":null,"deadband":0,"created_by":null,"created_at":"2026-01-18 05:15:05","updated_at":"2026-01-18 05:15:05","is_deleted":0,"inputs":[{"id":5,"virtual_point_id":16,"variable_name":"defaultInput","source_type":"constant","source_id":null,"constant_value":0,"source_formula":null,"data_processing":"current","time_window_seconds":null,"scaling_factor":1,"scaling_offset":0,"unit_conversion":null,"quality_filter":"good_only","default_value":null,"cache_enabled":1,"cache_duration_ms":1000,"description":null,"is_required":1,"sort_order":0,"created_at":"2026-01-18 05:15:05","updated_at":"2026-01-18 05:15:05"}],"currentValue":{"virtual_point_id":16,"value":null,"string_value":null,"raw_result":null,"quality":"uncertain","quality_code":1,"last_calculated":"2026-01-18 05:15:05","calculation_duration_ms":0,"calculation_error":null,"input_values":null,"is_stale":1,"staleness_threshold_ms":30000,"calculation_count":0,"error_count":0,"success_rate":100,"alarm_state":"normal","alarm_active":0}}',NULL,'Virtual Point Created','2026-01-18 05:15:05');
INSERT INTO virtual_point_logs VALUES(27,16,'DELETE','{"id":16,"tenant_id":1,"scope_type":"tenant","site_id":null,"device_id":null,"name":"SIMPLE_TEST_1768713305794","description":null,"formula":"return 1;","data_type":"float","unit":null,"calculation_interval":1000,"calculation_trigger":"timer","is_enabled":true,"category":"calculation","tags":[],"execution_type":"javascript","dependencies":[],"cache_duration_ms":0,"error_handling":"return_null","last_error":null,"execution_count":0,"avg_execution_time_ms":0,"last_execution_time":null,"script_library_id":null,"timeout_ms":5000,"max_execution_time_ms":10000,"retry_count":3,"quality_check_enabled":1,"result_validation":null,"outlier_detection_enabled":0,"log_enabled":1,"log_interval_ms":5000,"log_inputs":0,"log_errors":1,"alarm_enabled":0,"high_limit":null,"low_limit":null,"deadband":0,"created_by":null,"created_at":"2026-01-18 05:15:05","updated_at":"2026-01-18 05:15:05","is_deleted":0,"inputs":[{"id":5,"virtual_point_id":16,"variable_name":"defaultInput","source_type":"constant","source_id":null,"constant_value":0,"source_formula":null,"data_processing":"current","time_window_seconds":null,"scaling_factor":1,"scaling_offset":0,"unit_conversion":null,"quality_filter":"good_only","default_value":null,"cache_enabled":1,"cache_duration_ms":1000,"description":null,"is_required":1,"sort_order":0,"created_at":"2026-01-18 05:15:05","updated_at":"2026-01-18 05:15:05"}],"currentValue":{"virtual_point_id":16,"value":null,"string_value":null,"raw_result":null,"quality":"uncertain","quality_code":1,"last_calculated":"2026-01-18 05:15:05","calculation_duration_ms":0,"calculation_error":null,"input_values":null,"is_stale":1,"staleness_threshold_ms":30000,"calculation_count":0,"error_count":0,"success_rate":100,"alarm_state":"normal","alarm_active":0}}','{"is_deleted":true}',NULL,'Virtual Point Soft Deleted','2026-01-18 05:15:05');
INSERT INTO virtual_point_logs VALUES(28,17,'CREATE',NULL,'{"id":17,"tenant_id":1,"scope_type":"device","site_id":1,"device_id":1,"name":"E2E_Test_VP_1768715940314","description":null,"formula":"return inputs.motor_current + 10;","data_type":"float","unit":null,"calculation_interval":1000,"calculation_trigger":"onchange","is_enabled":true,"category":"test","tags":[],"execution_type":"javascript","dependencies":[],"cache_duration_ms":0,"error_handling":"return_null","last_error":null,"execution_count":0,"avg_execution_time_ms":0,"last_execution_time":null,"script_library_id":null,"timeout_ms":5000,"max_execution_time_ms":10000,"retry_count":3,"quality_check_enabled":1,"result_validation":null,"outlier_detection_enabled":0,"log_enabled":1,"log_interval_ms":5000,"log_inputs":0,"log_errors":1,"alarm_enabled":0,"high_limit":null,"low_limit":null,"deadband":0,"created_by":null,"created_at":"2026-01-18 05:59:00","updated_at":"2026-01-18 05:59:00","is_deleted":0,"inputs":[{"id":6,"virtual_point_id":17,"variable_name":"motor_current","source_type":"data_point","source_id":3,"constant_value":null,"source_formula":null,"data_processing":"current","time_window_seconds":null,"scaling_factor":1,"scaling_offset":0,"unit_conversion":null,"quality_filter":"good_only","default_value":null,"cache_enabled":1,"cache_duration_ms":1000,"description":null,"is_required":1,"sort_order":0,"created_at":"2026-01-18 05:59:00","updated_at":"2026-01-18 05:59:00"}],"currentValue":{"virtual_point_id":17,"value":null,"string_value":null,"raw_result":null,"quality":"uncertain","quality_code":1,"last_calculated":"2026-01-18 05:59:00","calculation_duration_ms":0,"calculation_error":null,"input_values":null,"is_stale":1,"staleness_threshold_ms":30000,"calculation_count":0,"error_count":0,"success_rate":100,"alarm_state":"normal","alarm_active":0}}',NULL,'Virtual Point Created','2026-01-18 05:59:00');
INSERT INTO virtual_point_logs VALUES(29,18,'CREATE',NULL,'{"id":18,"tenant_id":1,"scope_type":"device","site_id":1,"device_id":1,"name":"E2E_Test_VP_1768715997668","description":null,"formula":"return inputs.motor_current + 10;","data_type":"float","unit":null,"calculation_interval":1000,"calculation_trigger":"onchange","is_enabled":true,"category":"test","tags":[],"execution_type":"javascript","dependencies":[],"cache_duration_ms":0,"error_handling":"return_null","last_error":null,"execution_count":0,"avg_execution_time_ms":0,"last_execution_time":null,"script_library_id":null,"timeout_ms":5000,"max_execution_time_ms":10000,"retry_count":3,"quality_check_enabled":1,"result_validation":null,"outlier_detection_enabled":0,"log_enabled":1,"log_interval_ms":5000,"log_inputs":0,"log_errors":1,"alarm_enabled":0,"high_limit":null,"low_limit":null,"deadband":0,"created_by":null,"created_at":"2026-01-18 05:59:57","updated_at":"2026-01-18 05:59:57","is_deleted":0,"inputs":[{"id":7,"virtual_point_id":18,"variable_name":"motor_current","source_type":"data_point","source_id":3,"constant_value":null,"source_formula":null,"data_processing":"current","time_window_seconds":null,"scaling_factor":1,"scaling_offset":0,"unit_conversion":null,"quality_filter":"good_only","default_value":null,"cache_enabled":1,"cache_duration_ms":1000,"description":null,"is_required":1,"sort_order":0,"created_at":"2026-01-18 05:59:57","updated_at":"2026-01-18 05:59:57"}],"currentValue":{"virtual_point_id":18,"value":null,"string_value":null,"raw_result":null,"quality":"uncertain","quality_code":1,"last_calculated":"2026-01-18 05:59:57","calculation_duration_ms":0,"calculation_error":null,"input_values":null,"is_stale":1,"staleness_threshold_ms":30000,"calculation_count":0,"error_count":0,"success_rate":100,"alarm_state":"normal","alarm_active":0}}',NULL,'Virtual Point Created','2026-01-18 05:59:57');
INSERT INTO virtual_point_logs VALUES(30,19,'CREATE',NULL,'{"id":19,"tenant_id":1,"scope_type":"device","site_id":1,"device_id":1,"name":"E2E_Test_VP_1768716013795","description":null,"formula":"return inputs.motor_current + 10;","data_type":"float","unit":null,"calculation_interval":1000,"calculation_trigger":"onchange","is_enabled":true,"category":"test","tags":[],"execution_type":"javascript","dependencies":[],"cache_duration_ms":0,"error_handling":"return_null","last_error":null,"execution_count":0,"avg_execution_time_ms":0,"last_execution_time":null,"script_library_id":null,"timeout_ms":5000,"max_execution_time_ms":10000,"retry_count":3,"quality_check_enabled":1,"result_validation":null,"outlier_detection_enabled":0,"log_enabled":1,"log_interval_ms":5000,"log_inputs":0,"log_errors":1,"alarm_enabled":0,"high_limit":null,"low_limit":null,"deadband":0,"created_by":null,"created_at":"2026-01-18 06:00:13","updated_at":"2026-01-18 06:00:13","is_deleted":0,"inputs":[{"id":8,"virtual_point_id":19,"variable_name":"motor_current","source_type":"data_point","source_id":3,"constant_value":null,"source_formula":null,"data_processing":"current","time_window_seconds":null,"scaling_factor":1,"scaling_offset":0,"unit_conversion":null,"quality_filter":"good_only","default_value":null,"cache_enabled":1,"cache_duration_ms":1000,"description":null,"is_required":1,"sort_order":0,"created_at":"2026-01-18 06:00:13","updated_at":"2026-01-18 06:00:13"}],"currentValue":{"virtual_point_id":19,"value":null,"string_value":null,"raw_result":null,"quality":"uncertain","quality_code":1,"last_calculated":"2026-01-18 06:00:13","calculation_duration_ms":0,"calculation_error":null,"input_values":null,"is_stale":1,"staleness_threshold_ms":30000,"calculation_count":0,"error_count":0,"success_rate":100,"alarm_state":"normal","alarm_active":0}}',NULL,'Virtual Point Created','2026-01-18 06:00:13');
INSERT INTO virtual_point_logs VALUES(31,20,'CREATE',NULL,'{"id":20,"tenant_id":1,"scope_type":"device","site_id":1,"device_id":1,"name":"E2E_Test_VP_1768716099392","description":null,"formula":"return inputs.motor_current + 10;","data_type":"float","unit":null,"calculation_interval":1000,"calculation_trigger":"onchange","is_enabled":true,"category":"test","tags":[],"execution_type":"javascript","dependencies":[],"cache_duration_ms":0,"error_handling":"return_null","last_error":null,"execution_count":0,"avg_execution_time_ms":0,"last_execution_time":null,"script_library_id":null,"timeout_ms":5000,"max_execution_time_ms":10000,"retry_count":3,"quality_check_enabled":1,"result_validation":null,"outlier_detection_enabled":0,"log_enabled":1,"log_interval_ms":5000,"log_inputs":0,"log_errors":1,"alarm_enabled":0,"high_limit":null,"low_limit":null,"deadband":0,"created_by":null,"created_at":"2026-01-18 06:01:39","updated_at":"2026-01-18 06:01:39","is_deleted":0,"inputs":[{"id":9,"virtual_point_id":20,"variable_name":"motor_current","source_type":"data_point","source_id":3,"constant_value":null,"source_formula":null,"data_processing":"current","time_window_seconds":null,"scaling_factor":1,"scaling_offset":0,"unit_conversion":null,"quality_filter":"good_only","default_value":null,"cache_enabled":1,"cache_duration_ms":1000,"description":null,"is_required":1,"sort_order":0,"created_at":"2026-01-18 06:01:39","updated_at":"2026-01-18 06:01:39"}],"currentValue":{"virtual_point_id":20,"value":null,"string_value":null,"raw_result":null,"quality":"uncertain","quality_code":1,"last_calculated":"2026-01-18 06:01:39","calculation_duration_ms":0,"calculation_error":null,"input_values":null,"is_stale":1,"staleness_threshold_ms":30000,"calculation_count":0,"error_count":0,"success_rate":100,"alarm_state":"normal","alarm_active":0}}',NULL,'Virtual Point Created','2026-01-18 06:01:39');
INSERT INTO virtual_point_logs VALUES(32,21,'CREATE',NULL,'{"id":21,"tenant_id":1,"scope_type":"device","site_id":1,"device_id":1,"name":"E2E_Test_VP_1768728571584","description":null,"formula":"motor_current + 10","data_type":"float","unit":null,"calculation_interval":1000,"calculation_trigger":"onchange","is_enabled":true,"category":"test","tags":[],"execution_type":"javascript","dependencies":[],"cache_duration_ms":0,"error_handling":"return_null","last_error":null,"execution_count":0,"avg_execution_time_ms":0,"last_execution_time":null,"script_library_id":null,"timeout_ms":5000,"max_execution_time_ms":10000,"retry_count":3,"quality_check_enabled":1,"result_validation":null,"outlier_detection_enabled":0,"log_enabled":1,"log_interval_ms":5000,"log_inputs":0,"log_errors":1,"alarm_enabled":0,"high_limit":null,"low_limit":null,"deadband":0,"created_by":null,"created_at":"2026-01-18 09:29:31","updated_at":"2026-01-18 09:29:31","is_deleted":0,"inputs":[{"id":10,"virtual_point_id":21,"variable_name":"motor_current","source_type":"data_point","source_id":3,"constant_value":null,"source_formula":null,"data_processing":"current","time_window_seconds":null,"scaling_factor":1,"scaling_offset":0,"unit_conversion":null,"quality_filter":"good_only","default_value":null,"cache_enabled":1,"cache_duration_ms":1000,"description":null,"is_required":1,"sort_order":0,"created_at":"2026-01-18 09:29:31","updated_at":"2026-01-18 09:29:31"}],"currentValue":{"virtual_point_id":21,"value":null,"string_value":null,"raw_result":null,"quality":"uncertain","quality_code":1,"last_calculated":"2026-01-18 09:29:31","calculation_duration_ms":0,"calculation_error":null,"input_values":null,"is_stale":1,"staleness_threshold_ms":30000,"calculation_count":0,"error_count":0,"success_rate":100,"alarm_state":"normal","alarm_active":0}}',NULL,'Virtual Point Created','2026-01-18 09:29:31');
CREATE TABLE export_gateways (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    ip_address VARCHAR(45),
    port INTEGER DEFAULT 8080,
    status VARCHAR(20) DEFAULT 'pending',
    last_seen DATETIME,
    last_heartbeat DATETIME,
    version VARCHAR(20),
    config TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_deleted INTEGER DEFAULT 0,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);
/****** CORRUPTION ERROR *******/
DELETE FROM sqlite_sequence;
INSERT INTO sqlite_sequence VALUES('payload_templates',4);
INSERT INTO sqlite_sequence VALUES('schema_versions',1);
INSERT INTO sqlite_sequence VALUES('tenants',4);
INSERT INTO sqlite_sequence VALUES('sites',6);
INSERT INTO sqlite_sequence VALUES('edge_servers',6);
INSERT INTO sqlite_sequence VALUES('protocols',13);
INSERT INTO sqlite_sequence VALUES('manufacturers',5);
INSERT INTO sqlite_sequence VALUES('device_models',4);
INSERT INTO sqlite_sequence VALUES('template_devices',4);
INSERT INTO sqlite_sequence VALUES('template_data_points',22);
INSERT INTO sqlite_sequence VALUES('devices',11);
INSERT INTO sqlite_sequence VALUES('data_points',17);
INSERT INTO sqlite_sequence VALUES('virtual_points',22);
INSERT INTO sqlite_sequence VALUES('alarm_rules',14);
INSERT INTO sqlite_sequence VALUES('javascript_functions',5);
INSERT INTO sqlite_sequence VALUES('system_logs',4);
INSERT INTO sqlite_sequence VALUES('alarm_occurrences',570);
INSERT INTO sqlite_sequence VALUES('users',1);
INSERT INTO sqlite_sequence VALUES('alarm_rule_templates',8);
INSERT INTO sqlite_sequence VALUES('virtual_point_logs',32);
INSERT INTO sqlite_sequence VALUES('virtual_point_inputs',15);
INSERT INTO sqlite_sequence VALUES('virtual_point_execution_history',13);
CREATE INDEX idx_tenants_company_code ON tenants(company_code);
CREATE INDEX idx_tenants_active ON tenants(is_active);
CREATE INDEX idx_tenants_subscription ON tenants(subscription_status);
CREATE INDEX idx_tenants_domain ON tenants(domain);
CREATE INDEX idx_edge_servers_tenant ON edge_servers(tenant_id);
CREATE INDEX idx_edge_servers_status ON edge_servers(status);
CREATE INDEX idx_edge_servers_last_seen ON edge_servers(last_seen DESC);
CREATE INDEX idx_edge_servers_token ON edge_servers(registration_token);
CREATE INDEX idx_edge_servers_hostname ON edge_servers(hostname);
CREATE INDEX idx_system_settings_category ON system_settings(category);
CREATE INDEX idx_system_settings_public ON system_settings(is_public);
CREATE INDEX idx_system_settings_updated ON system_settings(updated_at DESC);
CREATE INDEX idx_users_tenant ON users(tenant_id);
CREATE INDEX idx_users_role ON users(role);
CREATE INDEX idx_users_active ON users(is_active);
CREATE INDEX idx_users_email ON users(email);
CREATE INDEX idx_users_username ON users(username);
CREATE INDEX idx_users_last_login ON users(last_login DESC);
CREATE INDEX idx_users_employee_id ON users(employee_id);
CREATE INDEX idx_user_sessions_user ON user_sessions(user_id);
CREATE INDEX idx_user_sessions_token ON user_sessions(token_hash);
CREATE INDEX idx_user_sessions_expires ON user_sessions(expires_at);
CREATE INDEX idx_user_sessions_active ON user_sessions(is_active);
CREATE INDEX idx_sites_tenant ON sites(tenant_id);
CREATE INDEX idx_sites_parent ON sites(parent_site_id);
CREATE INDEX idx_sites_type ON sites(site_type);
CREATE INDEX idx_sites_hierarchy ON sites(hierarchy_level);
CREATE INDEX idx_sites_active ON sites(is_active);
CREATE INDEX idx_sites_code ON sites(tenant_id, code);
CREATE INDEX idx_sites_edge_server ON sites(edge_server_id);
CREATE INDEX idx_sites_hierarchy_path ON sites(hierarchy_path);
CREATE INDEX idx_user_favorites_user ON user_favorites(user_id);
CREATE INDEX idx_user_favorites_target ON user_favorites(target_type, target_id);
CREATE INDEX idx_user_favorites_sort ON user_favorites(user_id, sort_order);
CREATE INDEX idx_user_notification_user ON user_notification_settings(user_id);
CREATE INDEX idx_device_groups_tenant ON device_groups(tenant_id);
CREATE INDEX idx_device_groups_site ON device_groups(site_id);
CREATE INDEX idx_device_groups_parent ON device_groups(parent_group_id);
CREATE INDEX idx_device_groups_type ON device_groups(group_type);
CREATE INDEX idx_driver_plugins_protocol ON driver_plugins(protocol_type);
CREATE INDEX idx_driver_plugins_enabled ON driver_plugins(is_enabled);
CREATE INDEX idx_devices_tenant ON devices(tenant_id);
CREATE INDEX idx_devices_site ON devices(site_id);
CREATE INDEX idx_devices_group ON devices(device_group_id);
CREATE INDEX idx_devices_edge_server ON devices(edge_server_id);
CREATE INDEX idx_devices_protocol ON devices(protocol_id);
CREATE INDEX idx_devices_type ON devices(device_type);
CREATE INDEX idx_devices_enabled ON devices(is_enabled);
CREATE INDEX idx_devices_name ON devices(tenant_id, name);
CREATE INDEX idx_device_settings_device_id ON device_settings(device_id);
CREATE INDEX idx_device_settings_polling ON device_settings(polling_interval_ms);
CREATE INDEX idx_device_status_connection ON device_status(connection_status);
CREATE INDEX idx_device_status_last_comm ON device_status(last_communication DESC);
CREATE INDEX idx_data_points_device ON data_points(device_id);
CREATE INDEX idx_data_points_enabled ON data_points(is_enabled);
CREATE INDEX idx_data_points_address ON data_points(device_id, address);
CREATE INDEX idx_data_points_type ON data_points(data_type);
CREATE INDEX idx_data_points_log_enabled ON data_points(log_enabled);
CREATE INDEX idx_data_points_name ON data_points(device_id, name);
CREATE INDEX idx_data_points_group ON data_points(group_name);
CREATE INDEX idx_current_values_timestamp ON current_values(value_timestamp DESC);
CREATE INDEX idx_current_values_quality ON current_values(quality_code);
CREATE INDEX idx_current_values_updated ON current_values(updated_at DESC);
CREATE INDEX idx_current_values_alarm ON current_values(alarm_active);
CREATE INDEX idx_current_values_quality_name ON current_values(quality);
CREATE INDEX idx_template_devices_model ON template_devices(model_id);
CREATE INDEX idx_template_data_points_template ON template_data_points(template_device_id);
CREATE INDEX idx_manufacturers_name ON manufacturers(name);
CREATE INDEX idx_device_models_manufacturer ON device_models(manufacturer_id);
CREATE INDEX idx_alarm_rules_tenant ON alarm_rules(tenant_id);
CREATE INDEX idx_alarm_rules_target ON alarm_rules(target_type, target_id);
CREATE INDEX idx_alarm_rules_enabled ON alarm_rules(is_enabled);
CREATE INDEX idx_alarm_rules_template_id ON alarm_rules(template_id);
CREATE INDEX idx_alarm_rules_rule_group ON alarm_rules(rule_group);
CREATE INDEX idx_alarm_rules_created_by_template ON alarm_rules(created_by_template);
CREATE INDEX idx_alarm_rules_category ON alarm_rules(category);
CREATE INDEX idx_alarm_rules_tags ON alarm_rules(tags);
CREATE INDEX idx_alarm_occurrences_rule ON alarm_occurrences(rule_id);
CREATE INDEX idx_alarm_occurrences_state ON alarm_occurrences(state);
CREATE INDEX idx_alarm_occurrences_time ON alarm_occurrences(occurrence_time DESC);
CREATE INDEX idx_alarm_occurrences_device_id ON alarm_occurrences(device_id);
CREATE INDEX idx_alarm_occurrences_point_id ON alarm_occurrences(point_id);
CREATE INDEX idx_alarm_occurrences_rule_device ON alarm_occurrences(rule_id, device_id);
CREATE INDEX idx_alarm_occurrences_category ON alarm_occurrences(category);
CREATE INDEX idx_alarm_occurrences_tags ON alarm_occurrences(tags);
CREATE INDEX idx_alarm_occurrences_acknowledged_by ON alarm_occurrences(acknowledged_by);
CREATE INDEX idx_alarm_occurrences_cleared_by ON alarm_occurrences(cleared_by);
CREATE INDEX idx_alarm_occurrences_cleared_time ON alarm_occurrences(cleared_time DESC);
CREATE INDEX idx_alarm_templates_tenant ON alarm_rule_templates(tenant_id);
CREATE INDEX idx_alarm_templates_category ON alarm_rule_templates(category);
CREATE INDEX idx_alarm_templates_active ON alarm_rule_templates(is_active);
CREATE INDEX idx_alarm_templates_system ON alarm_rule_templates(is_system_template);
CREATE INDEX idx_alarm_templates_usage ON alarm_rule_templates(usage_count DESC);
CREATE INDEX idx_alarm_templates_name ON alarm_rule_templates(tenant_id, name);
CREATE INDEX idx_alarm_templates_tags ON alarm_rule_templates(tags);
CREATE INDEX idx_js_functions_tenant ON javascript_functions(tenant_id);
CREATE INDEX idx_js_functions_category ON javascript_functions(category);
CREATE INDEX idx_recipes_tenant ON recipes(tenant_id);
CREATE INDEX idx_recipes_active ON recipes(is_active);
CREATE INDEX idx_schedules_tenant ON schedules(tenant_id);
CREATE INDEX idx_schedules_enabled ON schedules(is_enabled);
CREATE INDEX idx_virtual_points_tenant ON virtual_points(tenant_id);
CREATE INDEX idx_virtual_points_scope ON virtual_points(scope_type);
CREATE INDEX idx_virtual_points_site ON virtual_points(site_id);
CREATE INDEX idx_virtual_points_device ON virtual_points(device_id);
CREATE INDEX idx_virtual_points_enabled ON virtual_points(is_enabled);
CREATE INDEX idx_virtual_points_category ON virtual_points(category);
CREATE INDEX idx_virtual_points_trigger ON virtual_points(calculation_trigger);
CREATE INDEX idx_virtual_points_name ON virtual_points(tenant_id, name);
CREATE INDEX idx_vp_inputs_virtual_point ON virtual_point_inputs(virtual_point_id);
CREATE INDEX idx_vp_inputs_source ON virtual_point_inputs(source_type, source_id);
CREATE INDEX idx_vp_inputs_variable ON virtual_point_inputs(virtual_point_id, variable_name);
CREATE INDEX idx_vp_values_calculated ON virtual_point_values(last_calculated DESC);
CREATE INDEX idx_vp_values_quality ON virtual_point_values(quality);
CREATE INDEX idx_vp_values_stale ON virtual_point_values(is_stale);
CREATE INDEX idx_vp_values_alarm ON virtual_point_values(alarm_active);
CREATE INDEX idx_vp_execution_history_vp_id ON virtual_point_execution_history(virtual_point_id);
CREATE INDEX idx_vp_execution_history_time ON virtual_point_execution_history(execution_time DESC);
CREATE INDEX idx_vp_execution_history_result ON virtual_point_execution_history(result_type);
CREATE INDEX idx_vp_execution_history_trigger ON virtual_point_execution_history(trigger_source);
CREATE INDEX idx_vp_dependencies_vp_id ON virtual_point_dependencies(virtual_point_id);
CREATE INDEX idx_vp_dependencies_depends_on ON virtual_point_dependencies(depends_on_type, depends_on_id);
CREATE INDEX idx_vp_dependencies_active ON virtual_point_dependencies(is_active);
CREATE INDEX idx_script_library_tenant ON script_library(tenant_id);
CREATE INDEX idx_script_library_category ON script_library(category);
CREATE INDEX idx_script_library_active ON script_library(is_active);
CREATE INDEX idx_script_library_system ON script_library(is_system);
CREATE INDEX idx_script_library_name ON script_library(tenant_id, name);
CREATE INDEX idx_script_library_usage ON script_library(usage_count DESC);
CREATE INDEX idx_script_versions_script ON script_library_versions(script_id);
CREATE INDEX idx_script_versions_version ON script_library_versions(script_id, version_number);
CREATE INDEX idx_script_usage_script ON script_usage_history(script_id);
CREATE INDEX idx_script_usage_vp ON script_usage_history(virtual_point_id);
CREATE INDEX idx_script_usage_tenant ON script_usage_history(tenant_id);
CREATE INDEX idx_script_usage_time ON script_usage_history(used_at DESC);
CREATE INDEX idx_script_usage_context ON script_usage_history(usage_context);
CREATE INDEX idx_script_templates_category ON script_templates(category);
CREATE INDEX idx_script_templates_industry ON script_templates(industry);
CREATE INDEX idx_script_templates_equipment ON script_templates(equipment_type);
CREATE INDEX idx_script_templates_active ON script_templates(is_active);
CREATE INDEX idx_script_templates_difficulty ON script_templates(difficulty_level);
CREATE INDEX idx_script_templates_popularity ON script_templates(popularity_score DESC);
CREATE INDEX idx_system_logs_tenant ON system_logs(tenant_id);
CREATE INDEX idx_system_logs_level ON system_logs(log_level);
CREATE INDEX idx_system_logs_module ON system_logs(module);
CREATE INDEX idx_system_logs_created ON system_logs(created_at DESC);
CREATE INDEX idx_system_logs_user ON system_logs(user_id);
CREATE INDEX idx_system_logs_request ON system_logs(request_id);
CREATE INDEX idx_system_logs_session ON system_logs(session_id);
CREATE INDEX idx_user_activities_user ON user_activities(user_id);
CREATE INDEX idx_user_activities_tenant ON user_activities(tenant_id);
CREATE INDEX idx_user_activities_timestamp ON user_activities(timestamp DESC);
CREATE INDEX idx_user_activities_action ON user_activities(action);
CREATE INDEX idx_user_activities_resource ON user_activities(resource_type, resource_id);
CREATE INDEX idx_user_activities_session ON user_activities(session_id);
CREATE INDEX idx_user_activities_success ON user_activities(success);
CREATE INDEX idx_communication_logs_device ON communication_logs(device_id);
CREATE INDEX idx_communication_logs_timestamp ON communication_logs(timestamp DESC);
CREATE INDEX idx_communication_logs_protocol ON communication_logs(protocol);
CREATE INDEX idx_communication_logs_success ON communication_logs(success);
CREATE INDEX idx_communication_logs_direction ON communication_logs(direction);
CREATE INDEX idx_communication_logs_data_point ON communication_logs(data_point_id);
CREATE INDEX idx_data_history_point_time ON data_history(point_id, timestamp DESC);
CREATE INDEX idx_data_history_timestamp ON data_history(timestamp DESC);
CREATE INDEX idx_data_history_quality ON data_history(quality);
CREATE INDEX idx_data_history_source ON data_history(source);
CREATE INDEX idx_data_history_change_type ON data_history(change_type);
CREATE INDEX idx_virtual_point_history_point_time ON virtual_point_history(virtual_point_id, timestamp DESC);
CREATE INDEX idx_virtual_point_history_timestamp ON virtual_point_history(timestamp DESC);
CREATE INDEX idx_virtual_point_history_quality ON virtual_point_history(quality);
CREATE INDEX idx_virtual_point_history_trigger ON virtual_point_history(trigger_reason);
CREATE INDEX idx_alarm_event_logs_occurrence ON alarm_event_logs(occurrence_id);
CREATE INDEX idx_alarm_event_logs_rule ON alarm_event_logs(rule_id);
CREATE INDEX idx_alarm_event_logs_tenant ON alarm_event_logs(tenant_id);
CREATE INDEX idx_alarm_event_logs_time ON alarm_event_logs(event_time DESC);
CREATE INDEX idx_alarm_event_logs_type ON alarm_event_logs(event_type);
CREATE INDEX idx_alarm_event_logs_user ON alarm_event_logs(user_id);
CREATE INDEX idx_performance_logs_timestamp ON performance_logs(timestamp DESC);
CREATE INDEX idx_performance_logs_category ON performance_logs(metric_category);
CREATE INDEX idx_performance_logs_name ON performance_logs(metric_name);
CREATE INDEX idx_performance_logs_hostname ON performance_logs(hostname);
CREATE INDEX idx_performance_logs_component ON performance_logs(component);
CREATE INDEX idx_performance_logs_category_name_time ON performance_logs(metric_category, metric_name, timestamp DESC);
CREATE INDEX idx_audit_logs_tenant ON audit_logs(tenant_id);
CREATE INDEX idx_audit_logs_user ON audit_logs(user_id);
CREATE INDEX idx_audit_logs_entity ON audit_logs(entity_type, entity_id);
CREATE INDEX idx_audit_logs_action ON audit_logs(action);
CREATE INDEX idx_audit_logs_created ON audit_logs(created_at DESC);
CREATE INDEX idx_profiles_enabled ON export_profiles(is_enabled);
CREATE INDEX idx_profiles_created ON export_profiles(created_at DESC);
CREATE INDEX idx_profile_points_profile ON export_profile_points(profile_id);
CREATE INDEX idx_profile_points_point ON export_profile_points(point_id);
CREATE INDEX idx_profile_points_order ON export_profile_points(profile_id, display_order);
CREATE INDEX idx_protocol_services_profile ON protocol_services(profile_id);
CREATE INDEX idx_protocol_services_type ON protocol_services(service_type);
CREATE INDEX idx_protocol_services_enabled ON protocol_services(is_enabled);
CREATE INDEX idx_protocol_mappings_service ON protocol_mappings(service_id);
CREATE INDEX idx_protocol_mappings_point ON protocol_mappings(point_id);
CREATE INDEX idx_protocol_mappings_identifier ON protocol_mappings(service_id, external_identifier);
CREATE INDEX idx_payload_templates_system ON payload_templates(system_type);
CREATE INDEX idx_payload_templates_active ON payload_templates(is_active);
CREATE INDEX idx_export_targets_type ON export_targets(target_type);
CREATE INDEX idx_export_targets_profile ON export_targets(profile_id);
CREATE INDEX idx_export_targets_enabled ON export_targets(is_enabled);
CREATE INDEX idx_export_targets_name ON export_targets(name);
CREATE INDEX idx_export_targets_template ON export_targets(template_id);
CREATE INDEX idx_export_target_mappings_target ON export_target_mappings(target_id);
CREATE INDEX idx_export_target_mappings_point ON export_target_mappings(point_id);
CREATE INDEX idx_export_logs_type ON export_logs(log_type);
CREATE INDEX idx_export_logs_timestamp ON export_logs(timestamp DESC);
CREATE INDEX idx_export_logs_status ON export_logs(status);
CREATE INDEX idx_export_logs_service ON export_logs(service_id);
CREATE INDEX idx_export_logs_target ON export_logs(target_id);
CREATE INDEX idx_export_logs_target_time ON export_logs(target_id, timestamp DESC);
CREATE INDEX idx_export_schedules_enabled ON export_schedules(is_enabled);
CREATE INDEX idx_export_schedules_next_run ON export_schedules(next_run_at);
CREATE INDEX idx_export_schedules_target ON export_schedules(target_id);
CREATE VIEW v_export_targets_with_templates AS
SELECT 
    t.id,
    t.profile_id,
    t.name,
    t.target_type,
    t.description,
    t.is_enabled,
    t.config,
    t.template_id,
    t.export_mode,
    t.export_interval,
    t.batch_size,
    t.created_at,
    t.updated_at,
    
    -- 템플릿 정보 (LEFT JOIN)
    p.name as template_name,
    p.system_type as template_system_type,
    p.template_json,
    p.is_active as template_is_active
    
FROM export_targets t
LEFT JOIN payload_templates p ON t.template_id = p.id
;
CREATE VIEW v_export_targets_stats_24h AS
SELECT 
    t.id,
    t.name,
    t.target_type,
    t.is_enabled,
    t.description,
    
    COALESCE(COUNT(l.id), 0) as total_exports_24h,
    COALESCE(SUM(CASE WHEN l.status = 'success' THEN 1 ELSE 0 END), 0) as successful_exports_24h,
    COALESCE(SUM(CASE WHEN l.status = 'failure' THEN 1 ELSE 0 END), 0) as failed_exports_24h,
    
    CASE 
        WHEN COUNT(l.id) > 0 THEN 
            ROUND((SUM(CASE WHEN l.status = 'success' THEN 1 ELSE 0 END) * 100.0) / COUNT(l.id), 2)
        ELSE 0 
    END as success_rate_24h,
    
    ROUND(AVG(CASE WHEN l.status = 'success' THEN l.processing_time_ms END), 2) as avg_time_ms_24h,
    
    MAX(CASE WHEN l.status = 'success' THEN l.timestamp END) as last_success_at,
    MAX(CASE WHEN l.status = 'failure' THEN l.timestamp END) as last_failure_at,
    
    t.export_mode,
    t.created_at,
    t.updated_at
    
FROM export_targets t
LEFT JOIN export_logs l ON t.id = l.target_id 
    AND l.timestamp > datetime('now', '-24 hours')
    AND l.log_type = 'export'
GROUP BY t.id;
CREATE VIEW v_export_targets_stats_all AS
SELECT 
    t.id,
    t.name,
    t.target_type,
    t.is_enabled,
    
    COALESCE(COUNT(l.id), 0) as total_exports,
    COALESCE(SUM(CASE WHEN l.status = 'success' THEN 1 ELSE 0 END), 0) as successful_exports,
    COALESCE(SUM(CASE WHEN l.status = 'failure' THEN 1 ELSE 0 END), 0) as failed_exports,
    
    CASE 
        WHEN COUNT(l.id) > 0 THEN 
            ROUND((SUM(CASE WHEN l.status = 'success' THEN 1 ELSE 0 END) * 100.0) / COUNT(l.id), 2)
        ELSE 0 
    END as success_rate_all,
    
    ROUND(AVG(CASE WHEN l.status = 'success' THEN l.processing_time_ms END), 2) as avg_time_ms_all,
    
    MIN(l.timestamp) as first_export_at,
    MAX(l.timestamp) as last_export_at,
    
    t.created_at
    
FROM export_targets t
LEFT JOIN export_logs l ON t.id = l.target_id 
    AND l.log_type = 'export'
GROUP BY t.id;
CREATE VIEW v_export_profiles_detail AS
SELECT 
    p.id,
    p.name,
    p.description,
    p.is_enabled,
    COUNT(pp.id) as point_count,
    COUNT(CASE WHEN pp.is_enabled = 1 THEN 1 END) as active_point_count,
    p.created_at,
    p.updated_at
FROM export_profiles p
LEFT JOIN export_profile_points pp ON p.id = pp.profile_id
GROUP BY p.id;
CREATE VIEW v_protocol_services_detail AS
SELECT 
    ps.id,
    ps.profile_id,
    p.name as profile_name,
    ps.service_type,
    ps.service_name,
    ps.is_enabled,
    COUNT(pm.id) as mapping_count,
    COUNT(CASE WHEN pm.is_enabled = 1 THEN 1 END) as active_mapping_count,
    ps.active_connections,
    ps.total_requests,
    ps.last_request_at
FROM protocol_services ps
LEFT JOIN export_profiles p ON ps.profile_id = p.id
LEFT JOIN protocol_mappings pm ON ps.id = pm.service_id
GROUP BY ps.id;
CREATE TRIGGER tr_export_profiles_update
AFTER UPDATE ON export_profiles
BEGIN
    UPDATE export_profiles 
    SET updated_at = CURRENT_TIMESTAMP 
    WHERE id = NEW.id;
END;
CREATE TRIGGER tr_export_targets_update
AFTER UPDATE ON export_targets
BEGIN
    UPDATE export_targets 
    SET updated_at = CURRENT_TIMESTAMP 
    WHERE id = NEW.id;
END;
CREATE TRIGGER tr_payload_templates_update
AFTER UPDATE ON payload_templates
BEGIN
    UPDATE payload_templates 
    SET updated_at = CURRENT_TIMESTAMP 
    WHERE id = NEW.id;
END;
CREATE TRIGGER tr_profile_points_insert
AFTER INSERT ON export_profile_points
BEGIN
    UPDATE export_profiles 
    SET point_count = (
        SELECT COUNT(*) 
        FROM export_profile_points 
        WHERE profile_id = NEW.profile_id
    )
    WHERE id = NEW.profile_id;
END;
CREATE TRIGGER tr_profile_points_delete
AFTER DELETE ON export_profile_points
BEGIN
    UPDATE export_profiles 
    SET point_count = (
        SELECT COUNT(*) 
        FROM export_profile_points 
        WHERE profile_id = OLD.profile_id
    )
    WHERE id = OLD.profile_id;
END;
CREATE INDEX idx_virtual_point_logs_point_id ON virtual_point_logs(point_id);
CREATE INDEX idx_virtual_point_logs_created_at ON virtual_point_logs(created_at);
ROLLBACK; -- due to errors
