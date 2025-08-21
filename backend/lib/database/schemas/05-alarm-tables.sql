-- =============================================================================
-- backend/lib/database/schemas/05-alarm-tables.sql
-- 알람 시스템 테이블 (SQLite 버전) - 2025-08-21 최신 업데이트
-- PulseOne v2.1.0 완전 호환, 현재 DB와 100% 동기화
-- =============================================================================

-- 주의: 기존 alarm_definitions, active_alarms, alarm_events는 더 이상 사용하지 않음
-- 새로운 구조: alarm_rules + alarm_occurrences + 고급 기능 테이블들

-- =============================================================================
-- 알람 규칙 테이블 - 현재 DB 구조와 완전 일치 (category, tags 추가)
-- =============================================================================
CREATE TABLE IF NOT EXISTS alarm_rules (
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
    /* 예시:
    {
        "0": {"text": "이상 발생", "severity": "critical"},
        "1": {"text": "정상 복구", "severity": "info"},
        "high_high": {"text": "매우 높음: {value}{unit}", "severity": "critical"}
    }
    */
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
    
    -- 🆕 분류 및 태깅 시스템 (2025-08-21 추가)
    category VARCHAR(50) DEFAULT NULL,              -- 'process', 'system', 'safety', 'custom', 'general'
    tags TEXT DEFAULT NULL,                         -- JSON 배열 형태 ['tag1', 'tag2', 'tag3']
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id)
);

-- =============================================================================
-- 알람 발생 이력 - 현재 DB 구조와 완전 일치
-- =============================================================================
CREATE TABLE IF NOT EXISTS alarm_occurrences (
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
    
    -- Clear 정보
    cleared_time DATETIME,
    cleared_value TEXT,
    clear_comment TEXT,
    
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
    
    -- 현재 DB에 추가된 컬럼들
    device_id TEXT,                                 -- 추가된 디바이스 ID (텍스트)
    point_id INTEGER,                               -- 추가된 포인트 ID
    
    -- 🆕 분류 및 태깅 시스템 (2025-08-21 추가) - 규칙에서 복사
    category VARCHAR(50) DEFAULT NULL,              -- 규칙의 category 복사
    tags TEXT DEFAULT NULL,                         -- 규칙의 tags 복사
    
    FOREIGN KEY (rule_id) REFERENCES alarm_rules(id) ON DELETE CASCADE,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (acknowledged_by) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY (cleared_by) REFERENCES users(id) ON DELETE SET NULL
);

-- =============================================================================
-- 알람 규칙 템플릿 - 현재 DB에 존재하는 테이블
-- =============================================================================
CREATE TABLE IF NOT EXISTS alarm_rule_templates (
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
    applicable_device_types TEXT,                   -- JSON 배열: ["sensor", "plc"]
    
    -- 알림 기본 설정
    notification_enabled INTEGER DEFAULT 1,
    email_notification INTEGER DEFAULT 0,
    sms_notification INTEGER DEFAULT 0,
    auto_acknowledge INTEGER DEFAULT 0,
    auto_clear INTEGER DEFAULT 0,
    
    -- 메타데이터
    usage_count INTEGER DEFAULT 0,                  -- 사용된 횟수
    is_active INTEGER DEFAULT 1,
    is_system_template INTEGER DEFAULT 0,           -- 시스템 기본 템플릿 여부
    
    -- 🆕 분류 및 태깅 시스템 (2025-08-21 추가) - 템플릿 태깅
    tags TEXT DEFAULT NULL,                         -- JSON 배열 형태 ['tag1', 'tag2', 'tag3']
    
    -- 감사 정보
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 제약조건
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, name)
);

-- =============================================================================
-- 현재 DB에 존재하는 관련 테이블들
-- =============================================================================

-- JavaScript 함수 라이브러리 (알람 스크립트용)
CREATE TABLE IF NOT EXISTS javascript_functions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    category VARCHAR(50),                           -- 'math', 'logic', 'engineering', 'custom'
    function_code TEXT NOT NULL,
    parameters TEXT,                                -- JSON 배열 파라미터 정의
    return_type VARCHAR(20),
    is_system INTEGER DEFAULT 0,                   -- 시스템 제공 함수
    is_enabled INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id),
    UNIQUE(tenant_id, name)
);

-- 레시피 관리 (설비 제어 프로필)
CREATE TABLE IF NOT EXISTS recipes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    category VARCHAR(50),
    
    -- 레시피 데이터
    setpoints TEXT NOT NULL,                        -- JSON 형태
    /* 예시:
    {
        "points": [
            {"point_id": 1, "value": 100, "unit": "℃"},
            {"point_id": 2, "value": 50, "unit": "bar"}
        ]
    }
    */
    
    validation_rules TEXT,                          -- JSON 형태
    
    -- 메타데이터
    version INTEGER DEFAULT 1,
    is_active INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id)
);

-- 스케줄러 (시간 기반 작업 관리)
CREATE TABLE IF NOT EXISTS schedules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 스케줄 타입
    schedule_type VARCHAR(20) NOT NULL,             -- 'time_based', 'event_based', 'condition_based'
    
    -- 실행 대상
    action_type VARCHAR(50) NOT NULL,               -- 'write_value', 'execute_recipe', 'run_script', 'generate_report'
    action_config TEXT NOT NULL,                    -- JSON 형태
    
    -- 스케줄 설정
    cron_expression VARCHAR(100),                   -- time_based인 경우
    trigger_condition TEXT,                         -- condition_based인 경우
    
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

-- =============================================================================
-- 인덱스 생성 (성능 최적화)
-- =============================================================================

-- alarm_rules 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_rules_tenant ON alarm_rules(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_target ON alarm_rules(target_type, target_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_enabled ON alarm_rules(is_enabled);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_template_id ON alarm_rules(template_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_rule_group ON alarm_rules(rule_group);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_created_by_template ON alarm_rules(created_by_template);

-- 🆕 새로 추가된 컬럼들에 대한 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_rules_category ON alarm_rules(category);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_tags ON alarm_rules(tags);

-- alarm_occurrences 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_rule ON alarm_occurrences(rule_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_state ON alarm_occurrences(state);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_time ON alarm_occurrences(occurrence_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_device_id ON alarm_occurrences(device_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_point_id ON alarm_occurrences(point_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_rule_device ON alarm_occurrences(rule_id, device_id);

-- 🆕 alarm_occurrences 분류 컬럼 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_category ON alarm_occurrences(category);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_tags ON alarm_occurrences(tags);

-- alarm_rule_templates 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_templates_tenant ON alarm_rule_templates(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_category ON alarm_rule_templates(category);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_active ON alarm_rule_templates(is_active);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_system ON alarm_rule_templates(is_system_template);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_usage ON alarm_rule_templates(usage_count DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_name ON alarm_rule_templates(tenant_id, name);

-- 🆕 alarm_rule_templates tags 컬럼 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_templates_tags ON alarm_rule_templates(tags);

-- javascript_functions 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_js_functions_tenant ON javascript_functions(tenant_id);
CREATE INDEX IF NOT EXISTS idx_js_functions_category ON javascript_functions(category);

-- recipes 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_recipes_tenant ON recipes(tenant_id);
CREATE INDEX IF NOT EXISTS idx_recipes_active ON recipes(is_active);

-- schedules 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_schedules_tenant ON schedules(tenant_id);
CREATE INDEX IF NOT EXISTS idx_schedules_enabled ON schedules(is_enabled);