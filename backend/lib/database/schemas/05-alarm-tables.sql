-- =============================================================================
-- backend/lib/database/schemas/05-alarm-tables.sql
-- 알람 시스템 테이블 (SQLite 버전) - 현재 DB와 동기화된 최신 버전
-- =============================================================================

-- 🔥 주의: 기존 alarm_definitions, active_alarms, alarm_events는 더 이상 사용하지 않음
-- 🔥 새로운 구조: alarm_rules + alarm_occurrences

-- 알람 규칙 테이블 (현재 DB 구조와 완전 일치)
CREATE TABLE IF NOT EXISTS alarm_rules (
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

-- 알람 발생 이력 (현재 DB 구조와 완전 일치)
CREATE TABLE IF NOT EXISTS alarm_occurrences (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_id INTEGER NOT NULL,
    tenant_id INTEGER NOT NULL,
    
    -- 발생 정보
    occurrence_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    trigger_value TEXT,  -- JSON 형태
    trigger_condition TEXT,
    alarm_message TEXT,
    severity VARCHAR(20),
    
    -- 상태
    state VARCHAR(20) DEFAULT 'active',  -- 'active', 'acknowledged', 'cleared', 'suppressed', 'shelved'
    
    -- Acknowledge 정보
    acknowledged_time DATETIME,
    acknowledged_by INTEGER,
    acknowledge_comment TEXT,
    
    -- Clear 정보
    cleared_time DATETIME,
    cleared_value TEXT,  -- JSON 형태
    clear_comment TEXT,
    
    -- 알림 정보
    notification_sent INTEGER DEFAULT 0,
    notification_time DATETIME,
    notification_count INTEGER DEFAULT 0,
    notification_result TEXT,  -- JSON 형태
    
    -- 추가 컨텍스트
    context_data TEXT,  -- JSON 형태
    source_name VARCHAR(100),  -- 포인트명 등
    location VARCHAR(200),  -- 위치 정보
    
    FOREIGN KEY (rule_id) REFERENCES alarm_rules(id) ON DELETE CASCADE,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (acknowledged_by) REFERENCES users(id)
);

-- 🔥 v3.0.0 추가 테이블들 (현재 DB에 존재하는 고급 기능 테이블들)

-- JavaScript 함수 라이브러리
CREATE TABLE IF NOT EXISTS javascript_functions (
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

-- 레시피 관리
CREATE TABLE IF NOT EXISTS recipes (
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

-- 스케줄러
CREATE TABLE IF NOT EXISTS schedules (
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

-- 스크립트 라이브러리 관련 테이블들 (현재 DB에 존재)
CREATE TABLE IF NOT EXISTS script_library (
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

CREATE TABLE IF NOT EXISTS script_library_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    script_id INTEGER NOT NULL,
    version_number VARCHAR(20) NOT NULL,
    script_code TEXT NOT NULL,
    change_log TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (script_id) REFERENCES script_library(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS script_usage_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    script_id INTEGER NOT NULL,
    virtual_point_id INTEGER,
    tenant_id INTEGER NOT NULL,
    usage_context VARCHAR(50), -- 'virtual_point', 'alarm', 'manual'
    used_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (script_id) REFERENCES script_library(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS script_templates (
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

-- 인덱스들 (현재 DB에 존재하는 인덱스들)
CREATE INDEX IF NOT EXISTS idx_alarm_rules_tenant ON alarm_rules(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_target ON alarm_rules(target_type, target_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_enabled ON alarm_rules(is_enabled);

CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_rule ON alarm_occurrences(rule_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_state ON alarm_occurrences(state);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_time ON alarm_occurrences(occurrence_time DESC);

CREATE INDEX IF NOT EXISTS idx_js_functions_tenant ON javascript_functions(tenant_id);
CREATE INDEX IF NOT EXISTS idx_js_functions_category ON javascript_functions(category);

CREATE INDEX IF NOT EXISTS idx_recipes_tenant ON recipes(tenant_id);
CREATE INDEX IF NOT EXISTS idx_recipes_active ON recipes(is_active);

CREATE INDEX IF NOT EXISTS idx_schedules_tenant ON schedules(tenant_id);
CREATE INDEX IF NOT EXISTS idx_schedules_enabled ON schedules(is_enabled);