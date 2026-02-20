CREATE TABLE schema_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    version VARCHAR(20) NOT NULL,
    applied_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    description TEXT
);
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
CREATE TABLE edge_servers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 🔥 서버 식별 정보
    server_name VARCHAR(100) NOT NULL,
    server_type VARCHAR(20) DEFAULT 'collector',          -- collector, gateway
    description TEXT,
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
    site_id INTEGER,
    max_devices INTEGER DEFAULT 100,
    max_data_points INTEGER DEFAULT 1000, subscription_mode TEXT DEFAULT 'all',
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE SET NULL,
    
    -- 🔥 제약조건
    CONSTRAINT chk_edge_status CHECK (status IN ('pending', 'active', 'inactive', 'maintenance', 'error'))
);
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
    last_activity DATETIME, is_deleted TINYINT DEFAULT 0,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_role CHECK (role IN ('system_admin', 'company_admin', 'site_admin', 'engineer', 'operator', 'viewer')),
    CONSTRAINT chk_theme CHECK (theme IN ('light', 'dark', 'auto'))
);
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
    auto_registration_enabled INTEGER DEFAULT 0,
    
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
CREATE TABLE data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    
    -- 🔥 기본 식별 정보 (Struct DataPoint와 완전 일치)
    name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 🔥 주소 정보 (Struct DataPoint와 완전 일치)
    address INTEGER NOT NULL,                           -- uint32_t address
    address_string VARCHAR(255),                        -- std::string address_string (별칭)
    mapping_key VARCHAR(255),                           -- std::string mapping_key
    
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
    
    is_deleted BOOLEAN DEFAULT 0, alarm_priority VARCHAR(20) DEFAULT 'medium',
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    UNIQUE(device_id, address),
    CONSTRAINT chk_data_type CHECK (data_type IN ('BOOL', 'INT8', 'UINT8', 'INT16', 'UINT16', 'INT32', 'UINT32', 'INT64', 'UINT64', 'FLOAT32', 'FLOAT64', 'STRING', 'UNKNOWN')),
    CONSTRAINT chk_access_mode CHECK (access_mode IN ('read', 'write', 'read_write')),
    CONSTRAINT chk_retention_policy CHECK (retention_policy IN ('standard', 'extended', 'minimal', 'custom'))
);
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
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP, is_deleted TINYINT DEFAULT 0,
    FOREIGN KEY (model_id) REFERENCES device_models(id) ON DELETE CASCADE,
    FOREIGN KEY (protocol_id) REFERENCES protocols(id) ON DELETE RESTRICT
);
CREATE TABLE template_device_settings (
    template_device_id INTEGER PRIMARY KEY,
    polling_interval_ms INTEGER DEFAULT 1000,
    connection_timeout_ms INTEGER DEFAULT 10000,
    read_timeout_ms INTEGER DEFAULT 5000,
    write_timeout_ms INTEGER DEFAULT 5000,
    max_retry_count INTEGER DEFAULT 3,
    FOREIGN KEY (template_device_id) REFERENCES template_devices(id) ON DELETE CASCADE
);
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
    metadata TEXT, scaling_offset REAL DEFAULT 0.0, protocol_params TEXT, mapping_key VARCHAR(255),                                       -- JSON 형태
    FOREIGN KEY (template_device_id) REFERENCES template_devices(id) ON DELETE CASCADE
);
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
    tags TEXT DEFAULT NULL,                         -- JSON 배열 형태 ['tag1', 'tag2', 'tag3']
    
    is_deleted BOOLEAN DEFAULT 0,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id)
);
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
CREATE TABLE alarm_rule_templates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 템플릿 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    category VARCHAR(50),                           -- 'temperature', 'pressure', 'flow' 등
    
    -- 템플릿 조건 설정
    template_type VARCHAR(20) DEFAULT 'simple',     -- 'simple', 'advanced', 'script'
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
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, name)
);
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
    
    is_deleted BOOLEAN DEFAULT 0,
    
    -- 🔥 감사 필드
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
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
    last_exported_at DATETIME,
    data_points TEXT DEFAULT '[]'
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
    
    -- 추가 필드
    gateway_id INTEGER,
    sent_payload TEXT,
    
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
    action VARCHAR(50) NOT NULL,          -- CREATE, UPDATE, DELETE, RESTORE, EXECUTE, TOGGLE
    previous_state TEXT,                  -- JSON string of previous state
    new_state TEXT,                       -- JSON string of new state (if applicable)
    user_id INTEGER,                      -- Optional: ID of the user performing the action
    details TEXT,                         -- Optional: detailed message or reason
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (point_id) REFERENCES virtual_points(id) ON DELETE CASCADE
);
CREATE TABLE export_targets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER,
    name VARCHAR(100) NOT NULL UNIQUE,
    target_type VARCHAR(20) NOT NULL,
    description TEXT,
    is_enabled BOOLEAN DEFAULT 1,
    config TEXT NOT NULL,
    template_id INTEGER,
    export_mode VARCHAR(20) DEFAULT 'on_change',
    export_interval INTEGER DEFAULT 0,
    batch_size INTEGER DEFAULT 100,
    execution_delay_ms INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE SET NULL,
    FOREIGN KEY (template_id) REFERENCES payload_templates(id) ON DELETE SET NULL
);
CREATE TABLE export_target_mappings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    target_id INTEGER NOT NULL,
    point_id INTEGER,
    site_id INTEGER,
    target_field_name VARCHAR(200),
    target_description VARCHAR(500),
    conversion_config TEXT,
    is_enabled BOOLEAN DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (target_id) REFERENCES export_targets(id) ON DELETE CASCADE,
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    UNIQUE(target_id, point_id)
);
CREATE TABLE export_profile_assignments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER NOT NULL,
    gateway_id INTEGER NOT NULL,
    is_active INTEGER DEFAULT 1,
    assigned_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    tenant_id INTEGER REFERENCES tenants(id) ON DELETE CASCADE,       -- NULL = 시스템 관리자 전역 할당
    site_id INTEGER REFERENCES sites(id) ON DELETE SET NULL,          -- NULL = 테넌트 공용
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE CASCADE,
    FOREIGN KEY (gateway_id) REFERENCES edge_servers(id) ON DELETE CASCADE
);
CREATE TABLE protocol_instances (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    protocol_id INTEGER NOT NULL,
    instance_name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 인스턴스별 연결 정보 (vhost, broker ID/PW 등)
    vhost VARCHAR(50) DEFAULT '/',
    api_key VARCHAR(100),
    api_key_updated_at DATETIME,
    
    connection_params TEXT,             -- JSON: 인스턴스 전용 상세 파라미터
    
    -- 상태 및 관리
    is_enabled INTEGER DEFAULT 1,
    status VARCHAR(20) DEFAULT 'STOPPED', -- RUNNING, STOPPED, ERROR
    
    -- 메타데이터
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (protocol_id) REFERENCES protocols(id) ON DELETE CASCADE
);
CREATE TABLE permissions (
    id VARCHAR(50) PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    category VARCHAR(50),
    resource VARCHAR(50),
    actions TEXT,            -- JSON 배열: ["read", "write", "delete"]
    is_system INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE roles (
    id VARCHAR(50) PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    is_system INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE role_permissions (
    role_id VARCHAR(50),
    permission_id VARCHAR(50),
    PRIMARY KEY (role_id, permission_id),
    FOREIGN KEY (role_id) REFERENCES roles(id) ON DELETE CASCADE,
    FOREIGN KEY (permission_id) REFERENCES permissions(id) ON DELETE CASCADE
);
CREATE TABLE backups (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, filename TEXT NOT NULL UNIQUE, type TEXT DEFAULT 'full', status TEXT DEFAULT 'completed', size INTEGER DEFAULT 0, location TEXT DEFAULT '/app/data/backup', created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, created_by TEXT, description TEXT, duration INTEGER, is_deleted INTEGER DEFAULT 0);
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
CREATE INDEX idx_export_logs_type ON export_logs(log_type);
CREATE INDEX idx_export_logs_timestamp ON export_logs(timestamp DESC);
CREATE INDEX idx_export_logs_status ON export_logs(status);
CREATE INDEX idx_export_logs_service ON export_logs(service_id);
CREATE INDEX idx_export_logs_target ON export_logs(target_id);
CREATE INDEX idx_export_logs_target_time ON export_logs(target_id, timestamp DESC);
CREATE INDEX idx_export_schedules_enabled ON export_schedules(is_enabled);
CREATE INDEX idx_export_schedules_next_run ON export_schedules(next_run_at);
CREATE INDEX idx_export_schedules_target ON export_schedules(target_id);
CREATE INDEX idx_virtual_point_logs_point_id ON virtual_point_logs(point_id);
CREATE INDEX idx_virtual_point_logs_created_at ON virtual_point_logs(created_at);
CREATE INDEX idx_export_targets_type ON export_targets(target_type);
CREATE INDEX idx_export_targets_profile ON export_targets(profile_id);
CREATE INDEX idx_export_targets_enabled ON export_targets(is_enabled);
CREATE INDEX idx_export_targets_name ON export_targets(name);
CREATE INDEX idx_export_targets_template ON export_targets(template_id);
CREATE INDEX idx_export_target_mappings_target ON export_target_mappings(target_id);
CREATE INDEX idx_export_target_mappings_point ON export_target_mappings(point_id);
CREATE INDEX idx_export_schedules_target_id ON export_schedules(target_id);
CREATE INDEX idx_export_schedules_is_enabled ON export_schedules(is_enabled);
CREATE INDEX idx_export_schedules_next_run_at ON export_schedules(next_run_at);
CREATE INDEX idx_backups_created_at ON backups(created_at);
CREATE INDEX idx_backups_status ON backups(status);
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
/* v_export_targets_with_templates(id,profile_id,name,target_type,description,is_enabled,config,template_id,export_mode,export_interval,batch_size,created_at,updated_at,template_name,template_system_type,template_json,template_is_active) */;
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
GROUP BY t.id
/* v_export_targets_stats_24h(id,name,target_type,is_enabled,description,total_exports_24h,successful_exports_24h,failed_exports_24h,success_rate_24h,avg_time_ms_24h,last_success_at,last_failure_at,export_mode,created_at,updated_at) */;
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
GROUP BY t.id
/* v_export_targets_stats_all(id,name,target_type,is_enabled,total_exports,successful_exports,failed_exports,success_rate_all,avg_time_ms_all,first_export_at,last_export_at,created_at) */;
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
GROUP BY p.id
/* v_export_profiles_detail(id,name,description,is_enabled,point_count,active_point_count,created_at,updated_at) */;
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
GROUP BY ps.id
/* v_protocol_services_detail(id,profile_id,profile_name,service_type,service_name,is_enabled,mapping_count,active_mapping_count,active_connections,total_requests,last_request_at) */;
CREATE TRIGGER tr_export_profiles_update
AFTER UPDATE ON export_profiles
BEGIN
    UPDATE export_profiles 
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
