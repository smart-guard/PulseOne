-- =============================================================================
-- PulseOne 스키마 업데이트 - 가상포인트 & 알람 시스템 고도화
-- Date: 2025-01-01
-- Version: 3.0.0
-- =============================================================================

-- =============================================================================
-- 1. 가상포인트 테이블 확장
-- =============================================================================

-- 기존 virtual_points 테이블 백업
CREATE TABLE IF NOT EXISTS virtual_points_backup AS SELECT * FROM virtual_points;

-- virtual_points 테이블 확장
ALTER TABLE virtual_points 
ADD COLUMN IF NOT EXISTS execution_type VARCHAR(20) DEFAULT 'javascript',
ADD COLUMN IF NOT EXISTS dependencies TEXT,  -- JSON 형태로 저장 (SQLite)
ADD COLUMN IF NOT EXISTS cache_duration_ms INTEGER DEFAULT 0,
ADD COLUMN IF NOT EXISTS error_handling VARCHAR(20) DEFAULT 'return_null',
ADD COLUMN IF NOT EXISTS last_error TEXT,
ADD COLUMN IF NOT EXISTS execution_count INTEGER DEFAULT 0,
ADD COLUMN IF NOT EXISTS avg_execution_time_ms REAL DEFAULT 0.0,
ADD COLUMN IF NOT EXISTS last_execution_time DATETIME;

-- 가상포인트 실행 이력 테이블 (신규)
CREATE TABLE IF NOT EXISTS virtual_point_execution_history (
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

-- 가상포인트 의존성 테이블 (신규)
CREATE TABLE IF NOT EXISTS virtual_point_dependencies (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    depends_on_type VARCHAR(20) NOT NULL,  -- 'data_point' or 'virtual_point'
    depends_on_id INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    UNIQUE(virtual_point_id, depends_on_type, depends_on_id)
);

-- =============================================================================
-- 2. 알람 시스템 재설계
-- =============================================================================

-- 기존 alarm_definitions 백업
CREATE TABLE IF NOT EXISTS alarm_definitions_backup AS SELECT * FROM alarm_definitions;

-- 알람 규칙 테이블 (완전 재설계)
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

-- 알람 발생 이력 (확장)
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

-- =============================================================================
-- 3. JavaScript 함수 라이브러리 테이블
-- =============================================================================

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

-- =============================================================================
-- 4. 레시피 관리 (SCADA 고급 기능)
-- =============================================================================

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

-- =============================================================================
-- 5. 스케줄 정의
-- =============================================================================

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

-- =============================================================================
-- 6. 인덱스 생성
-- =============================================================================

-- 가상포인트 인덱스
CREATE INDEX IF NOT EXISTS idx_vp_execution_history_vp_id ON virtual_point_execution_history(virtual_point_id);
CREATE INDEX IF NOT EXISTS idx_vp_execution_history_time ON virtual_point_execution_history(execution_time DESC);
CREATE INDEX IF NOT EXISTS idx_vp_dependencies_vp_id ON virtual_point_dependencies(virtual_point_id);
CREATE INDEX IF NOT EXISTS idx_vp_dependencies_depends_on ON virtual_point_dependencies(depends_on_type, depends_on_id);

-- 알람 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_rules_tenant ON alarm_rules(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_target ON alarm_rules(target_type, target_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_enabled ON alarm_rules(is_enabled);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_rule ON alarm_occurrences(rule_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_state ON alarm_occurrences(state);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_time ON alarm_occurrences(occurrence_time DESC);

-- JavaScript 함수 인덱스
CREATE INDEX IF NOT EXISTS idx_js_functions_tenant ON javascript_functions(tenant_id);
CREATE INDEX IF NOT EXISTS idx_js_functions_category ON javascript_functions(category);

-- 레시피 인덱스
CREATE INDEX IF NOT EXISTS idx_recipes_tenant ON recipes(tenant_id);
CREATE INDEX IF NOT EXISTS idx_recipes_active ON recipes(is_active);

-- 스케줄 인덱스
CREATE INDEX IF NOT EXISTS idx_schedules_tenant ON schedules(tenant_id);
CREATE INDEX IF NOT EXISTS idx_schedules_enabled ON schedules(is_enabled);

-- =============================================================================
-- 7. 초기 데이터 - JavaScript 시스템 함수
-- =============================================================================

INSERT OR IGNORE INTO javascript_functions (tenant_id, name, category, function_code, is_system) VALUES
(0, 'avg', 'math', 'function avg(...values) { const valid = values.filter(v => v !== null); return valid.length > 0 ? valid.reduce((a, b) => a + b, 0) / valid.length : null; }', 1),
(0, 'max', 'math', 'function max(...values) { const valid = values.filter(v => v !== null); return valid.length > 0 ? Math.max(...valid) : null; }', 1),
(0, 'min', 'math', 'function min(...values) { const valid = values.filter(v => v !== null); return valid.length > 0 ? Math.min(...valid) : null; }', 1),
(0, 'clamp', 'math', 'function clamp(value, min, max) { return Math.min(Math.max(value, min), max); }', 1),
(0, 'scale', 'math', 'function scale(value, inMin, inMax, outMin, outMax) { return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin; }', 1),
(0, 'and', 'logic', 'function and(...values) { return values.every(v => v === true || v === 1); }', 1),
(0, 'or', 'logic', 'function or(...values) { return values.some(v => v === true || v === 1); }', 1),
(0, 'risingEdge', 'logic', 'function risingEdge(current, previous) { return !previous && current; }', 1),
(0, 'fallingEdge', 'logic', 'function fallingEdge(current, previous) { return previous && !current; }', 1),
(0, 'oee', 'engineering', 'function oee(availability, performance, quality) { return (availability * performance * quality) / 10000; }', 1);

-- =============================================================================
-- 8. 마이그레이션 완료 메시지
-- =============================================================================
SELECT '✅ PulseOne 스키마 업데이트 완료 - Version 3.0.0' as message;