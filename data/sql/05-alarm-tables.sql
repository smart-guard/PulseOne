-- =============================================================================
-- backend/lib/database/schemas/05-alarm-tables.sql
-- 알람 시스템 테이블 (SQLite 버전) - device_id INTEGER 수정
-- PulseOne v2.1.0 완전 호환, 현재 DB와 100% 동기화
-- =============================================================================

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
    is_deleted INTEGER DEFAULT 0,

    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id)
);

-- =============================================================================
-- 알람 발생 이력 - device_id INTEGER로 수정
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
    is_deleted INTEGER DEFAULT 0,

    -- 타임스탬프
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, name)
);

-- =============================================================================
-- JavaScript 함수 라이브러리 (알람 조건용)
-- =============================================================================
CREATE TABLE IF NOT EXISTS javascript_functions (
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

-- =============================================================================
-- 레시피 시스템 (복잡한 알람 로직용)
-- =============================================================================
CREATE TABLE IF NOT EXISTS recipes (
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

-- =============================================================================
-- 스케줄 시스템 (시간 기반 알람용)
-- =============================================================================
CREATE TABLE IF NOT EXISTS schedules (
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

-- =============================================================================
-- 인덱스 생성
-- =============================================================================

-- alarm_rules 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_rules_tenant ON alarm_rules(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_target ON alarm_rules(target_type, target_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_enabled ON alarm_rules(is_enabled);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_template_id ON alarm_rules(template_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_rule_group ON alarm_rules(rule_group);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_created_by_template ON alarm_rules(created_by_template);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_category ON alarm_rules(category);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_tags ON alarm_rules(tags);

-- alarm_occurrences 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_rule ON alarm_occurrences(rule_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_state ON alarm_occurrences(state);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_time ON alarm_occurrences(occurrence_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_device_id ON alarm_occurrences(device_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_point_id ON alarm_occurrences(point_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_rule_device ON alarm_occurrences(rule_id, device_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_category ON alarm_occurrences(category);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_tags ON alarm_occurrences(tags);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_acknowledged_by ON alarm_occurrences(acknowledged_by);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_cleared_by ON alarm_occurrences(cleared_by);    -- ⭐ 추가!
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_cleared_time ON alarm_occurrences(cleared_time DESC);

-- alarm_rule_templates 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_templates_tenant ON alarm_rule_templates(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_category ON alarm_rule_templates(category);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_active ON alarm_rule_templates(is_active);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_system ON alarm_rule_templates(is_system_template);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_usage ON alarm_rule_templates(usage_count DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_templates_name ON alarm_rule_templates(tenant_id, name);
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