CREATE TABLE schema_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    version VARCHAR(20) NOT NULL,
    applied_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    description TEXT
);
CREATE TABLE sqlite_sequence(name,seq);
CREATE TABLE tenants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    company_name VARCHAR(100) NOT NULL UNIQUE,
    company_code VARCHAR(20) NOT NULL UNIQUE,
    domain VARCHAR(100) UNIQUE,
    
    -- 연락처 정보
    contact_name VARCHAR(100),
    contact_email VARCHAR(100),
    contact_phone VARCHAR(20),
    
    -- 구독 정보
    subscription_plan VARCHAR(20) DEFAULT 'starter',
    subscription_status VARCHAR(20) DEFAULT 'active',
    max_edge_servers INTEGER DEFAULT 3,
    max_data_points INTEGER DEFAULT 1000,
    max_users INTEGER DEFAULT 5,
    
    -- 상태 정보
    is_active INTEGER DEFAULT 1,
    trial_end_date DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE sites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    location VARCHAR(200),
    description TEXT,
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);
CREATE TABLE edge_servers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 서버 식별 정보
    server_name VARCHAR(100) NOT NULL,
    factory_name VARCHAR(100),
    location VARCHAR(200),
    
    -- 네트워크 정보
    ip_address VARCHAR(45),
    mac_address VARCHAR(17),
    hostname VARCHAR(100),
    
    -- 등록 정보
    registration_token VARCHAR(255) UNIQUE,
    activation_code VARCHAR(50),
    
    -- 상태 정보
    status VARCHAR(20) DEFAULT 'pending',
    last_seen DATETIME,
    version VARCHAR(20),
    
    -- 설정 정보
    config TEXT,
    sync_enabled INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);
CREATE TABLE device_groups (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    parent_group_id INTEGER,
    group_type VARCHAR(50) DEFAULT 'functional', -- functional, physical, protocol
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (parent_group_id) REFERENCES device_groups(id) ON DELETE SET NULL
);
CREATE TABLE driver_plugins (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL,
    protocol_type VARCHAR(50) NOT NULL,
    version VARCHAR(20) NOT NULL,
    file_path VARCHAR(255) NOT NULL,
    description TEXT,
    
    -- 플러그인 정보
    author VARCHAR(100),
    api_version INTEGER DEFAULT 1,
    is_enabled INTEGER DEFAULT 1,
    config_schema TEXT, -- JSON 스키마
    
    -- 라이선스 정보
    license_type VARCHAR(20) DEFAULT 'free',
    license_expires DATETIME,
    
    installed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    UNIQUE(protocol_type, version)
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
    device_type VARCHAR(50) NOT NULL, -- PLC, HMI, SENSOR, GATEWAY, METER, CONTROLLER
    manufacturer VARCHAR(100),
    model VARCHAR(100),
    serial_number VARCHAR(100),
    
    -- 프로토콜 설정
    protocol_type VARCHAR(50) NOT NULL, -- MODBUS_TCP, MODBUS_RTU, MQTT, BACNET, OPCUA
    endpoint VARCHAR(255) NOT NULL, -- IP:Port 또는 연결 문자열
    config TEXT NOT NULL, -- JSON 형태 프로토콜별 설정
    
    -- 수집 설정 (기본값만, 세부 설정은 device_settings 테이블 참조)
    polling_interval INTEGER DEFAULT 1000,
    timeout INTEGER DEFAULT 3000,
    retry_count INTEGER DEFAULT 3,
    
    -- 상태 정보
    is_enabled INTEGER DEFAULT 1,
    installation_date DATE,
    last_maintenance DATE,
    
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (device_group_id) REFERENCES device_groups(id) ON DELETE SET NULL,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL
);
CREATE TABLE device_settings (
    device_id INTEGER PRIMARY KEY,
    
    -- 폴링 및 타이밍 설정
    polling_interval_ms INTEGER DEFAULT 1000,
    scan_rate_override INTEGER, -- 개별 디바이스 스캔 주기 오버라이드
    
    -- 연결 및 통신 설정
    connection_timeout_ms INTEGER DEFAULT 10000,
    read_timeout_ms INTEGER DEFAULT 5000,
    write_timeout_ms INTEGER DEFAULT 5000,
    
    -- 재시도 정책
    max_retry_count INTEGER DEFAULT 3,
    retry_interval_ms INTEGER DEFAULT 5000,
    backoff_multiplier DECIMAL(3,2) DEFAULT 1.5, -- 지수 백오프
    backoff_time_ms INTEGER DEFAULT 60000,
    max_backoff_time_ms INTEGER DEFAULT 300000, -- 최대 5분
    
    -- Keep-alive 설정
    keep_alive_enabled INTEGER DEFAULT 1,
    keep_alive_interval_s INTEGER DEFAULT 30,
    keep_alive_timeout_s INTEGER DEFAULT 10,
    
    -- 데이터 품질 관리
    data_validation_enabled INTEGER DEFAULT 1,
    outlier_detection_enabled INTEGER DEFAULT 0,
    deadband_enabled INTEGER DEFAULT 1,
    
    -- 로깅 및 진단
    detailed_logging_enabled INTEGER DEFAULT 0,
    performance_monitoring_enabled INTEGER DEFAULT 1,
    diagnostic_mode_enabled INTEGER DEFAULT 0,
    
    -- 메타데이터
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_by INTEGER,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);
CREATE TABLE device_status (
    device_id INTEGER PRIMARY KEY,
    connection_status VARCHAR(20) NOT NULL DEFAULT 'disconnected',
    last_communication DATETIME,
    error_count INTEGER DEFAULT 0,
    last_error TEXT,
    response_time INTEGER,
    
    -- 추가 진단 정보
    firmware_version VARCHAR(50),
    hardware_info TEXT, -- JSON 형태
    diagnostic_data TEXT, -- JSON 형태
    
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);
CREATE INDEX idx_tenants_code ON tenants(company_code);
CREATE INDEX idx_sites_tenant ON sites(tenant_id);
CREATE INDEX idx_edge_servers_tenant ON edge_servers(tenant_id);
CREATE INDEX idx_device_groups_tenant_site ON device_groups(tenant_id, site_id);
CREATE INDEX idx_devices_tenant ON devices(tenant_id);
CREATE INDEX idx_devices_site ON devices(site_id);
CREATE INDEX idx_devices_group ON devices(device_group_id);
CREATE INDEX idx_devices_edge_server ON devices(edge_server_id);
CREATE INDEX idx_devices_protocol ON devices(protocol_type);
CREATE INDEX idx_devices_type ON devices(device_type);
CREATE INDEX idx_devices_enabled ON devices(is_enabled);
CREATE INDEX idx_device_settings_device_id ON device_settings(device_id);
CREATE INDEX idx_device_settings_polling ON device_settings(polling_interval_ms);
CREATE TABLE test_table (id INTEGER);
CREATE TABLE data_points_backup(
  id INT,
  device_id INT,
  name TEXT,
  description TEXT,
  address INT,
  data_type TEXT,
  access_mode TEXT,
  unit TEXT,
  scaling_factor NUM,
  scaling_offset NUM,
  min_value NUM,
  max_value NUM,
  is_enabled INT,
  scan_rate INT,
  deadband NUM,
  config TEXT,
  tags TEXT,
  created_at NUM,
  updated_at NUM
);
CREATE TABLE current_values_backup(
  point_id INT,
  value NUM,
  raw_value NUM,
  string_value TEXT,
  quality TEXT,
  timestamp NUM,
  updated_at NUM
);
CREATE TABLE data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    
    -- 🔥 기본 식별 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 🔥 주소 정보 (Struct DataPoint와 일치)
    address INTEGER NOT NULL,                    -- uint32_t address
    address_string VARCHAR(255),                 -- std::string address_string
    
    -- 🔥 데이터 타입 및 접근성 (Struct DataPoint와 일치)
    data_type VARCHAR(20) NOT NULL DEFAULT 'UNKNOWN',  -- std::string data_type
    access_mode VARCHAR(10) DEFAULT 'read',             -- std::string access_mode
    is_enabled INTEGER DEFAULT 1,                       -- bool is_enabled
    is_writable INTEGER DEFAULT 0,                      -- bool is_writable
    
    -- 🔥 엔지니어링 단위 및 스케일링 (Struct DataPoint와 일치)
    unit VARCHAR(50),                            -- std::string unit
    scaling_factor REAL DEFAULT 1.0,            -- double scaling_factor
    scaling_offset REAL DEFAULT 0.0,            -- double scaling_offset
    min_value REAL DEFAULT 0.0,                 -- double min_value
    max_value REAL DEFAULT 0.0,                 -- double max_value
    
    -- 🔥🔥🔥 로깅 및 수집 설정 (중요! 이전에 없던 컬럼들)
    log_enabled INTEGER DEFAULT 1,              -- bool log_enabled ✅
    log_interval_ms INTEGER DEFAULT 0,          -- uint32_t log_interval_ms ✅
    log_deadband REAL DEFAULT 0.0,              -- double log_deadband ✅
    polling_interval_ms INTEGER DEFAULT 0,      -- uint32_t polling_interval_ms
    
    -- 🔥🔥🔥 메타데이터 (중요! 이전에 없던 컬럼들)
    group_name VARCHAR(50),                      -- std::string group
    tags TEXT,                                   -- std::string tags (JSON 배열) ✅
    metadata TEXT,                               -- std::string metadata (JSON 객체) ✅
    protocol_params TEXT,                        -- map<string,string> protocol_params (JSON)
    
    -- 🔥 시간 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    UNIQUE(device_id, address)
);
CREATE TABLE current_values (
    point_id INTEGER PRIMARY KEY,
    
    -- 🔥 실제 값 (JSON으로 DataVariant 저장)
    current_value TEXT,                          -- JSON: {"value": 123.45}
    raw_value TEXT,                              -- JSON: {"value": 12345} (스케일링 전)
    value_type VARCHAR(10) DEFAULT 'double',     -- bool, int16, uint16, int32, uint32, float, double, string
    
    -- 🔥 데이터 품질 및 타임스탬프
    quality_code INTEGER DEFAULT 0,             -- DataQuality enum 값
    quality VARCHAR(20) DEFAULT 'not_connected', -- 텍스트 표현
    
    -- 🔥 타임스탬프들
    value_timestamp DATETIME,                   -- 값 변경 시간
    quality_timestamp DATETIME,                 -- 품질 변경 시간  
    last_log_time DATETIME,                     -- 마지막 로깅 시간
    last_read_time DATETIME,                    -- 마지막 읽기 시간
    last_write_time DATETIME,                   -- 마지막 쓰기 시간
    
    -- 🔥 통계 카운터들
    read_count INTEGER DEFAULT 0,               -- 읽기 횟수
    write_count INTEGER DEFAULT 0,              -- 쓰기 횟수
    error_count INTEGER DEFAULT 0,              -- 에러 횟수
    
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE
);
CREATE INDEX idx_data_points_device ON data_points(device_id);
CREATE INDEX idx_data_points_enabled ON data_points(is_enabled);
CREATE INDEX idx_data_points_address ON data_points(device_id, address);
CREATE INDEX idx_data_points_type ON data_points(data_type);
CREATE INDEX idx_data_points_log_enabled ON data_points(log_enabled);
CREATE INDEX idx_current_values_timestamp ON current_values(value_timestamp DESC);
CREATE INDEX idx_current_values_quality ON current_values(quality_code);
CREATE INDEX idx_current_values_updated ON current_values(updated_at DESC);
CREATE INDEX idx_current_values_updated_at ON current_values(updated_at);
CREATE TABLE virtual_point_execution_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    execution_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    execution_duration_ms INTEGER,
    result_value TEXT,  -- JSON 형태
    input_snapshot TEXT,  -- JSON 형태
    success INTEGER DEFAULT 1,
    error_message TEXT,
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE
);
CREATE TABLE virtual_point_dependencies (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    depends_on_type VARCHAR(20) NOT NULL,  -- 'data_point' or 'virtual_point'
    depends_on_id INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    UNIQUE(virtual_point_id, depends_on_type, depends_on_id)
);
CREATE TABLE alarm_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 대상 정보
    target_type VARCHAR(20) NOT NULL,  -- 'data_point', 'virtual_point', 'group'
    target_id INTEGER,
    target_group VARCHAR(100),
    
    -- 알람 타입
    alarm_type VARCHAR(20) NOT NULL,  -- 'analog', 'digital', 'script'
    
    -- 아날로그 알람 설정
    high_high_limit REAL,
    high_limit REAL,
    low_limit REAL,
    low_low_limit REAL,
    deadband REAL DEFAULT 0,
    rate_of_change REAL DEFAULT 0,  -- 변화율 임계값
    
    -- 디지털 알람 설정
    trigger_condition VARCHAR(20),  -- 'on_true', 'on_false', 'on_change', 'on_rising', 'on_falling'
    
    -- 스크립트 기반 알람
    condition_script TEXT,  -- JavaScript 조건식
    message_script TEXT,    -- JavaScript 메시지 생성
    
    -- 메시지 커스터마이징
    message_config TEXT,  -- JSON 형태
    /* 예시:
    {
        "0": {"text": "이상 발생", "severity": "critical"},
        "1": {"text": "정상 복구", "severity": "info"},
        "high_high": {"text": "매우 높음: {value}{unit}", "severity": "critical"}
    }
    */
    message_template TEXT,  -- 기본 메시지 템플릿
    
    -- 우선순위
    severity VARCHAR(20) DEFAULT 'medium',  -- 'critical', 'high', 'medium', 'low', 'info'
    priority INTEGER DEFAULT 100,
    
    -- 자동 처리
    auto_acknowledge INTEGER DEFAULT 0,
    acknowledge_timeout_min INTEGER DEFAULT 0,
    auto_clear INTEGER DEFAULT 1,
    
    -- 억제 규칙
    suppression_rules TEXT,  -- JSON 형태
    /* 예시:
    {
        "time_based": [
            {"start": "22:00", "end": "06:00", "days": ["SAT", "SUN"]}
        ],
        "condition_based": [
            {"point_id": 123, "condition": "value == 0"}
        ]
    }
    */
    
    -- 알림 설정
    notification_enabled INTEGER DEFAULT 1,
    notification_delay_sec INTEGER DEFAULT 0,
    notification_repeat_interval_min INTEGER DEFAULT 0,
    notification_channels TEXT,  -- JSON 배열 ['email', 'sms', 'mq', 'webhook']
    notification_recipients TEXT,  -- JSON 배열
    
    -- 상태
    is_enabled INTEGER DEFAULT 1,
    is_latched INTEGER DEFAULT 0,  -- 래치 알람 (수동 리셋 필요)
    
    -- 타임스탬프
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id)
);
CREATE TABLE javascript_functions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    category VARCHAR(50),  -- 'math', 'logic', 'engineering', 'custom'
    function_code TEXT NOT NULL,
    parameters TEXT,  -- JSON 배열 파라미터 정의
    return_type VARCHAR(20),
    is_system INTEGER DEFAULT 0,  -- 시스템 제공 함수
    is_enabled INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id),
    UNIQUE(tenant_id, name)
);
CREATE TABLE recipes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    category VARCHAR(50),
    
    -- 레시피 데이터
    setpoints TEXT NOT NULL,  -- JSON 형태
    /* 예시:
    {
        "points": [
            {"point_id": 1, "value": 100, "unit": "℃"},
            {"point_id": 2, "value": 50, "unit": "bar"}
        ]
    }
    */
    
    validation_rules TEXT,  -- JSON 형태
    
    -- 메타데이터
    version INTEGER DEFAULT 1,
    is_active INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id)
);
CREATE TABLE schedules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 스케줄 타입
    schedule_type VARCHAR(20) NOT NULL,  -- 'time_based', 'event_based', 'condition_based'
    
    -- 실행 대상
    action_type VARCHAR(50) NOT NULL,  -- 'write_value', 'execute_recipe', 'run_script', 'generate_report'
    action_config TEXT NOT NULL,  -- JSON 형태
    
    -- 스케줄 설정
    cron_expression VARCHAR(100),  -- time_based인 경우
    trigger_condition TEXT,  -- condition_based인 경우
    
    -- 실행 옵션
    retry_on_failure INTEGER DEFAULT 1,
    max_retries INTEGER DEFAULT 3,
    timeout_seconds INTEGER DEFAULT 300,
    
    -- 상태
    is_enabled INTEGER DEFAULT 1,
    last_execution_time DATETIME,
    next_execution_time DATETIME,
    
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);
CREATE INDEX idx_vp_execution_history_vp_id ON virtual_point_execution_history(virtual_point_id);
CREATE INDEX idx_vp_execution_history_time ON virtual_point_execution_history(execution_time DESC);
CREATE INDEX idx_vp_dependencies_vp_id ON virtual_point_dependencies(virtual_point_id);
CREATE INDEX idx_vp_dependencies_depends_on ON virtual_point_dependencies(depends_on_type, depends_on_id);
CREATE INDEX idx_alarm_rules_tenant ON alarm_rules(tenant_id);
CREATE INDEX idx_alarm_rules_target ON alarm_rules(target_type, target_id);
CREATE INDEX idx_alarm_rules_enabled ON alarm_rules(is_enabled);
CREATE INDEX idx_js_functions_tenant ON javascript_functions(tenant_id);
CREATE INDEX idx_js_functions_category ON javascript_functions(category);
CREATE INDEX idx_recipes_tenant ON recipes(tenant_id);
CREATE INDEX idx_recipes_active ON recipes(is_active);
CREATE INDEX idx_schedules_tenant ON schedules(tenant_id);
CREATE INDEX idx_schedules_enabled ON schedules(is_enabled);
CREATE TABLE virtual_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 유연한 범위 설정
    scope_type VARCHAR(20) NOT NULL DEFAULT 'tenant', -- tenant, site, device
    site_id INTEGER,
    device_id INTEGER,
    
    -- 가상포인트 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    formula TEXT NOT NULL, -- JavaScript 계산식
    data_type VARCHAR(20) NOT NULL DEFAULT 'float',
    unit VARCHAR(20),
    
    -- 계산 설정
    calculation_interval INTEGER DEFAULT 1000, -- ms
    calculation_trigger VARCHAR(20) DEFAULT 'timer', -- timer, onchange, manual
    is_enabled INTEGER DEFAULT 1,
    
    -- 메타데이터
    category VARCHAR(50),
    tags TEXT, -- JSON 배열
    
    -- 확장 필드 (v3.0.0)
    execution_type VARCHAR(20) DEFAULT 'javascript',
    dependencies TEXT,  -- JSON 형태로 저장
    cache_duration_ms INTEGER DEFAULT 0,
    error_handling VARCHAR(20) DEFAULT 'return_null',
    last_error TEXT,
    execution_count INTEGER DEFAULT 0,
    avg_execution_time_ms REAL DEFAULT 0.0,
    last_execution_time DATETIME,
    
    -- 감사 필드
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    
    -- 범위별 제약조건
    CONSTRAINT chk_scope_consistency CHECK (
        (scope_type = 'tenant' AND site_id IS NULL AND device_id IS NULL) OR
        (scope_type = 'site' AND site_id IS NOT NULL AND device_id IS NULL) OR
        (scope_type = 'device' AND site_id IS NOT NULL AND device_id IS NOT NULL)
    ),
    CONSTRAINT chk_scope_type CHECK (scope_type IN ('tenant', 'site', 'device'))
);
CREATE TABLE virtual_point_inputs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    variable_name VARCHAR(50) NOT NULL, -- 계산식에서 사용할 변수명 (예: temp1, motor_power)
    
    -- 다양한 소스 타입 지원
    source_type VARCHAR(20) NOT NULL, -- data_point, virtual_point, constant, formula
    source_id INTEGER, -- data_points.id 또는 virtual_points.id
    constant_value REAL, -- source_type이 'constant'일 때
    source_formula TEXT, -- source_type이 'formula'일 때 (간단한 수식)
    
    -- 데이터 처리 옵션
    data_processing VARCHAR(20) DEFAULT 'current', -- current, average, min, max, sum
    time_window_seconds INTEGER, -- data_processing이 'current'가 아닐 때
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    UNIQUE(virtual_point_id, variable_name),
    CONSTRAINT chk_source_type CHECK (source_type IN ('data_point', 'virtual_point', 'constant', 'formula'))
);
CREATE TABLE virtual_point_values (
    virtual_point_id INTEGER PRIMARY KEY,
    value REAL,
    string_value TEXT, -- 문자열 타입 가상포인트용
    quality VARCHAR(20) DEFAULT 'good',
    last_calculated DATETIME DEFAULT CURRENT_TIMESTAMP,
    calculation_error TEXT, -- 계산 오류 메시지
    input_values TEXT, -- JSON: 계산에 사용된 입력값들 (디버깅용)
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE
);
CREATE INDEX idx_virtual_points_tenant ON virtual_points(tenant_id);
CREATE INDEX idx_virtual_points_scope ON virtual_points(scope_type);
CREATE INDEX idx_virtual_points_site ON virtual_points(site_id);
CREATE INDEX idx_virtual_points_device ON virtual_points(device_id);
CREATE INDEX idx_virtual_points_enabled ON virtual_points(is_enabled);
CREATE INDEX idx_virtual_points_category ON virtual_points(category);
CREATE INDEX idx_virtual_points_trigger ON virtual_points(calculation_trigger);
CREATE INDEX idx_vp_inputs_virtual_point ON virtual_point_inputs(virtual_point_id);
CREATE INDEX idx_vp_inputs_source ON virtual_point_inputs(source_type, source_id);
CREATE INDEX idx_vp_values_calculated ON virtual_point_values(last_calculated DESC);
CREATE TABLE script_library (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL DEFAULT 0,
    category VARCHAR(50) NOT NULL,
    name VARCHAR(100) NOT NULL,
    display_name VARCHAR(100),
    description TEXT,
    script_code TEXT NOT NULL,
    parameters TEXT, -- JSON 형태
    return_type VARCHAR(20) DEFAULT 'number',
    tags TEXT, -- JSON 배열
    example_usage TEXT,
    is_system INTEGER DEFAULT 0,
    usage_count INTEGER DEFAULT 0,
    rating REAL DEFAULT 0.0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(tenant_id, name)
);
CREATE TABLE script_library_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    script_id INTEGER NOT NULL,
    version_number VARCHAR(20) NOT NULL,
    script_code TEXT NOT NULL,
    change_log TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (script_id) REFERENCES script_library(id) ON DELETE CASCADE
);
CREATE TABLE script_usage_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    script_id INTEGER NOT NULL,
    virtual_point_id INTEGER,
    tenant_id INTEGER NOT NULL,
    usage_context VARCHAR(50), -- 'virtual_point', 'alarm', 'manual'
    used_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (script_id) REFERENCES script_library(id) ON DELETE CASCADE
);
CREATE TABLE script_templates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    category VARCHAR(50) NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    template_code TEXT NOT NULL,
    variables TEXT, -- JSON 형태
    industry VARCHAR(50),
    equipment_type VARCHAR(50),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE test_alarm_occurrences (
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
    notification_sent INTEGER DEFAULT 0,
    notification_time DATETIME,
    notification_count INTEGER DEFAULT 0,
    notification_result TEXT,
    context_data TEXT,
    source_name VARCHAR(100),
    location VARCHAR(200),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE alarm_occurrences (
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
    notification_sent INTEGER DEFAULT 0,
    notification_time DATETIME,
    notification_count INTEGER DEFAULT 0,
    notification_result TEXT,
    context_data TEXT,
    source_name VARCHAR(100),
    location VARCHAR(200),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
, device_id TEXT, point_id INTEGER);
CREATE INDEX idx_alarm_occurrences_rule ON alarm_occurrences(rule_id);
CREATE INDEX idx_alarm_occurrences_state ON alarm_occurrences(state);
CREATE INDEX idx_alarm_occurrences_time ON alarm_occurrences(occurrence_time DESC);
CREATE INDEX idx_alarm_occurrences_device_id ON alarm_occurrences(device_id);
CREATE INDEX idx_alarm_occurrences_point_id ON alarm_occurrences(point_id);
CREATE INDEX idx_alarm_occurrences_rule_device ON alarm_occurrences(rule_id, device_id);
