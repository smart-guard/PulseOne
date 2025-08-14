-- =============================================================================
-- backend/lib/database/schemas/05-alarm-tables.sql
-- 알람 시스템 테이블 (SQLite 버전) - 2025-08-14 최신 업데이트
-- PulseOne v2.1.0 완전 호환, 현재 DB와 100% 동기화
-- =============================================================================

-- 🔥 주의: 기존 alarm_definitions, active_alarms, alarm_events는 더 이상 사용하지 않음
-- 🔥 새로운 구조: alarm_rules + alarm_occurrences + 고급 기능 테이블들

-- =============================================================================
-- 🔥🔥🔥 알람 규칙 테이블 - 현재 DB 구조와 완전 일치
-- =============================================================================
CREATE TABLE IF NOT EXISTS alarm_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 🔥 기본 알람 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 🔥 대상 정보 (AlarmRuleEntity.h와 완전 일치)
    target_type VARCHAR(20) NOT NULL,                  -- 'data_point', 'virtual_point', 'device', 'group', 'script'
    target_id INTEGER,
    target_group VARCHAR(100),                         -- 그룹명 직접 지정
    
    -- 🔥 알람 타입 및 심각도
    alarm_type VARCHAR(20) NOT NULL,                   -- 'analog', 'digital', 'script'
    severity VARCHAR(20) DEFAULT 'medium',             -- 'critical', 'high', 'medium', 'low', 'info'
    priority INTEGER DEFAULT 100,                     -- 우선순위 (낮을수록 높은 우선순위)
    
    -- 🔥🔥🔥 아날로그 알람 설정 (히스테리시스 완전 지원)
    high_high_limit REAL,                             -- 매우 높음 임계값
    high_limit REAL,                                  -- 높음 임계값
    low_limit REAL,                                   -- 낮음 임계값
    low_low_limit REAL,                               -- 매우 낮음 임계값
    deadband REAL DEFAULT 0,                          -- 히스테리시스 (중요!)
    rate_of_change REAL DEFAULT 0,                    -- 변화율 임계값 (단위시간당)
    
    -- 🔥🔥🔥 디지털 알람 설정
    trigger_condition VARCHAR(20),                    -- 'on_true', 'on_false', 'on_change', 'on_rising', 'on_falling'
    trigger_value INTEGER,                            -- 디지털 트리거 값 (0 또는 1)
    
    -- 🔥🔥🔥 스크립트 기반 알람
    condition_script TEXT,                            -- JavaScript 조건식
    message_script TEXT,                              -- JavaScript 메시지 생성
    script_library_id INTEGER,                       -- 스크립트 라이브러리 참조
    script_timeout_ms INTEGER DEFAULT 5000,           -- 스크립트 실행 타임아웃
    
    -- 🔥 메시지 커스터마이징 (JSON 기반)
    message_config TEXT,                              -- JSON 형태
    /* 예시:
    {
        "0": {"text": "이상 발생", "severity": "critical"},
        "1": {"text": "정상 복구", "severity": "info"},
        "high_high": {"text": "매우 높음: {value}{unit}", "severity": "critical"}
    }
    */
    message_template TEXT,                            -- 기본 메시지 템플릿
    
    -- 🔥 자동 처리 설정
    auto_acknowledge INTEGER DEFAULT 0,
    acknowledge_timeout_min INTEGER DEFAULT 0,        -- 자동 확인 타임아웃
    auto_clear INTEGER DEFAULT 1,                     -- 조건 해제 시 자동 클리어
    clear_timeout_min INTEGER DEFAULT 0,              -- 자동 클리어 타임아웃
    
    -- 🔥 억제 규칙 (JSON 기반)
    suppression_rules TEXT,                           -- JSON 형태
    /* 예시:
    {
        "time_based": [
            {"start": "22:00", "end": "06:00", "days": ["SAT", "SUN"]}
        ],
        "condition_based": [
            {"point_id": 123, "condition": "value == 0", "description": "메인 펌프 정지 시"}
        ]
    }
    */
    
    -- 🔥 알림 설정 (멀티채널 지원)
    notification_enabled INTEGER DEFAULT 1,
    notification_delay_sec INTEGER DEFAULT 0,         -- 알림 지연 시간
    notification_repeat_interval_min INTEGER DEFAULT 0, -- 반복 알림 간격 (0 = 반복 없음)
    notification_channels TEXT,                       -- JSON 배열 ['email', 'sms', 'slack', 'teams', 'webhook']
    notification_recipients TEXT,                     -- JSON 배열 (사용자/그룹 ID)
    
    -- 🔥 에스컬레이션 설정
    escalation_enabled INTEGER DEFAULT 0,
    escalation_delay_min INTEGER DEFAULT 30,          -- 에스컬레이션 지연 시간
    escalation_recipients TEXT,                       -- JSON 배열 (상급자 목록)
    max_escalation_level INTEGER DEFAULT 3,           -- 최대 에스컬레이션 단계
    
    -- 🔥 고급 설정
    is_enabled INTEGER DEFAULT 1,
    is_latched INTEGER DEFAULT 0,                     -- 래치 알람 (수동 리셋 필요)
    is_shelved INTEGER DEFAULT 0,                     -- 일시 정지
    shelve_expires_at DATETIME,                       -- 정지 해제 시간
    
    -- 🔥 카운터 및 통계
    occurrence_count INTEGER DEFAULT 0,               -- 총 발생 횟수
    last_triggered DATETIME,                          -- 마지막 발생 시간
    avg_duration_min REAL DEFAULT 0.0,                -- 평균 지속 시간
    max_duration_min REAL DEFAULT 0.0,                -- 최대 지속 시간
    
    -- 🔥 메타데이터
    tags TEXT,                                        -- JSON 배열
    metadata TEXT,                                    -- JSON 객체
    custom_fields TEXT,                               -- JSON 객체 (사용자 정의 필드)
    
    -- 🔥 감사 정보
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_modified_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (script_library_id) REFERENCES script_library(id) ON DELETE SET NULL,
    FOREIGN KEY (created_by) REFERENCES users(id),
    FOREIGN KEY (last_modified_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_target_type CHECK (target_type IN ('data_point', 'virtual_point', 'device', 'group', 'script')),
    CONSTRAINT chk_alarm_type CHECK (alarm_type IN ('analog', 'digital', 'script')),
    CONSTRAINT chk_severity CHECK (severity IN ('critical', 'high', 'medium', 'low', 'info')),
    CONSTRAINT chk_trigger_condition CHECK (trigger_condition IN ('on_true', 'on_false', 'on_change', 'on_rising', 'on_falling'))
);

-- =============================================================================
-- 🔥🔥🔥 알람 발생 이력 - 현재 DB 구조와 완전 일치
-- =============================================================================
CREATE TABLE IF NOT EXISTS alarm_occurrences (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_id INTEGER NOT NULL,
    tenant_id INTEGER NOT NULL,
    
    -- 🔥 발생 정보 (AlarmOccurrenceEntity.h와 완전 일치)
    occurrence_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    trigger_value TEXT,                               -- JSON 형태 (실제 측정값)
    trigger_condition TEXT,                           -- 트리거된 조건 설명
    alarm_message TEXT,                               -- 생성된 알람 메시지
    severity VARCHAR(20),                             -- 규칙에서 복사됨
    
    -- 🔥 알람 상태 (완전한 상태 기계)
    state VARCHAR(20) DEFAULT 'active',               -- 'active', 'acknowledged', 'cleared', 'suppressed', 'shelved'
    previous_state VARCHAR(20),                       -- 이전 상태 (상태 변화 추적)
    state_changed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 🔥 Acknowledge 정보 (사용자 확인)
    acknowledged_time DATETIME,
    acknowledged_by INTEGER,
    acknowledge_comment TEXT,
    acknowledge_method VARCHAR(20),                   -- 'manual', 'auto', 'system'
    
    -- 🔥 Clear 정보 (알람 해제)
    cleared_time DATETIME,
    cleared_value TEXT,                               -- JSON 형태 (해제 시점 값)
    clear_comment TEXT,
    clear_method VARCHAR(20),                         -- 'auto', 'manual', 'timeout'
    cleared_by INTEGER,
    
    -- 🔥 알림 정보 (멀티채널 지원)
    notification_sent INTEGER DEFAULT 0,
    notification_time DATETIME,
    notification_count INTEGER DEFAULT 0,            -- 총 알림 발송 횟수
    notification_result TEXT,                        -- JSON 형태 (채널별 발송 결과)
    last_notification_time DATETIME,                 -- 마지막 알림 시간
    
    -- 🔥 에스컬레이션 정보
    escalation_level INTEGER DEFAULT 0,              -- 현재 에스컬레이션 단계
    escalation_time DATETIME,                        -- 에스컬레이션 시간
    escalation_result TEXT,                          -- JSON 형태 (에스컬레이션 결과)
    
    -- 🔥 추가 컨텍스트 (풍부한 정보)
    context_data TEXT,                               -- JSON 형태 (발생 시점 컨텍스트)
    source_name VARCHAR(100),                        -- 포인트명 등
    location VARCHAR(200),                           -- 위치 정보
    related_data_points TEXT,                        -- JSON 배열 (관련 데이터포인트들)
    
    -- 🔥 성능 정보
    detection_latency_ms INTEGER,                    -- 감지 지연 시간
    processing_time_ms INTEGER,                      -- 처리 시간
    
    -- 🔥 품질 정보
    data_quality VARCHAR(20) DEFAULT 'good',         -- 트리거 시점 데이터 품질
    confidence_level REAL DEFAULT 1.0,               -- 알람 신뢰도 (0.0-1.0)
    
    FOREIGN KEY (rule_id) REFERENCES alarm_rules(id) ON DELETE CASCADE,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (acknowledged_by) REFERENCES users(id),
    FOREIGN KEY (cleared_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_state CHECK (state IN ('active', 'acknowledged', 'cleared', 'suppressed', 'shelved', 'expired')),
    CONSTRAINT chk_acknowledge_method CHECK (acknowledge_method IN ('manual', 'auto', 'system')),
    CONSTRAINT chk_clear_method CHECK (clear_method IN ('auto', 'manual', 'timeout', 'system')),
    CONSTRAINT chk_data_quality CHECK (data_quality IN ('good', 'bad', 'uncertain', 'not_connected'))
);

-- =============================================================================
-- 🔥🔥🔥 고급 알람 기능 테이블들 (현재 DB에 존재하는 테이블들)
-- =============================================================================

-- JavaScript 함수 라이브러리 (알람 스크립트용)
CREATE TABLE IF NOT EXISTS javascript_functions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 🔥 함수 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    category VARCHAR(50),                             -- 'math', 'logic', 'engineering', 'alarm', 'custom'
    
    -- 🔥 함수 코드
    function_code TEXT NOT NULL,
    parameters TEXT,                                  -- JSON 배열: 파라미터 정의
    return_type VARCHAR(20),                          -- 반환 타입
    
    -- 🔥 상태 정보
    is_system INTEGER DEFAULT 0,                     -- 시스템 제공 함수
    is_enabled INTEGER DEFAULT 1,
    is_tested INTEGER DEFAULT 0,                     -- 테스트 완료 여부
    
    -- 🔥 사용 통계
    usage_count INTEGER DEFAULT 0,
    last_used DATETIME,
    error_count INTEGER DEFAULT 0,
    
    -- 🔥 메타데이터
    tags TEXT,                                       -- JSON 배열
    examples TEXT,                                   -- JSON 배열: 사용 예제
    documentation TEXT,
    
    -- 🔥 감사 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    UNIQUE(tenant_id, name),
    
    -- 🔥 제약조건
    CONSTRAINT chk_js_category CHECK (category IN ('math', 'logic', 'engineering', 'alarm', 'utility', 'custom'))
);

-- 레시피 관리 (설비 제어 프로필)
CREATE TABLE IF NOT EXISTS recipes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 🔥 레시피 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    category VARCHAR(50),                            -- production, maintenance, startup, shutdown
    
    -- 🔥 레시피 데이터
    setpoints TEXT NOT NULL,                         -- JSON 형태
    /* 예시:
    {
        "points": [
            {"point_id": 1, "value": 100, "unit": "℃", "ramp_rate": 5.0},
            {"point_id": 2, "value": 50, "unit": "bar", "hold_time": 30}
        ],
        "sequence": [
            {"step": 1, "action": "ramp_to_setpoint", "duration": 300},
            {"step": 2, "action": "hold", "duration": 600}
        ]
    }
    */
    
    -- 🔥 검증 및 안전
    validation_rules TEXT,                           -- JSON 형태: 안전 제약조건
    safety_limits TEXT,                              -- JSON 형태: 안전 한계값
    
    -- 🔥 실행 정보
    estimated_duration_min INTEGER,                  -- 예상 실행 시간
    requires_approval INTEGER DEFAULT 0,             -- 승인 필요 여부
    approved_by INTEGER,                             -- 승인자
    approved_at DATETIME,                            -- 승인 시간
    
    -- 🔥 상태 정보
    version INTEGER DEFAULT 1,
    is_active INTEGER DEFAULT 0,                    -- 현재 활성 레시피
    is_validated INTEGER DEFAULT 0,                 -- 검증 완료
    last_executed DATETIME,                         -- 마지막 실행 시간
    execution_count INTEGER DEFAULT 0,              -- 실행 횟수
    
    -- 🔥 메타데이터
    tags TEXT,                                      -- JSON 배열
    metadata TEXT,                                  -- JSON 객체
    
    -- 🔥 감사 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (approved_by) REFERENCES users(id),
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_recipe_category CHECK (category IN ('production', 'maintenance', 'startup', 'shutdown', 'emergency', 'test'))
);

-- 스케줄러 (시간 기반 작업 관리)
CREATE TABLE IF NOT EXISTS schedules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 🔥 스케줄 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 🔥 스케줄 타입
    schedule_type VARCHAR(20) NOT NULL,             -- 'time_based', 'event_based', 'condition_based'
    
    -- 🔥 실행 대상
    action_type VARCHAR(50) NOT NULL,               -- 'write_value', 'execute_recipe', 'run_script', 'generate_report', 'send_notification'
    action_config TEXT NOT NULL,                    -- JSON 형태 (실행할 작업 정의)
    
    -- 🔥 스케줄 설정
    cron_expression VARCHAR(100),                   -- time_based인 경우 (예: "0 8 * * MON-FRI")
    trigger_condition TEXT,                         -- condition_based인 경우
    trigger_events TEXT,                            -- JSON 배열: event_based인 경우
    
    -- 🔥 실행 옵션
    retry_on_failure INTEGER DEFAULT 1,
    max_retries INTEGER DEFAULT 3,
    timeout_seconds INTEGER DEFAULT 300,
    
    -- 🔥 제약조건
    max_concurrent_executions INTEGER DEFAULT 1,
    skip_if_previous_running INTEGER DEFAULT 1,     -- 이전 실행이 진행 중이면 건너뛰기
    
    -- 🔥 상태 정보
    is_enabled INTEGER DEFAULT 1,
    is_paused INTEGER DEFAULT 0,
    last_execution_time DATETIME,
    next_execution_time DATETIME,
    execution_count INTEGER DEFAULT 0,
    success_count INTEGER DEFAULT 0,
    failure_count INTEGER DEFAULT 0,
    
    -- 🔥 감사 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_schedule_type CHECK (schedule_type IN ('time_based', 'event_based', 'condition_based')),
    CONSTRAINT chk_action_type CHECK (action_type IN ('write_value', 'execute_recipe', 'run_script', 'generate_report', 'send_notification', 'backup_data', 'system_maintenance'))
);

-- 알람 그룹 관리 (관련 알람들을 그룹화)
CREATE TABLE IF NOT EXISTS alarm_groups (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 🔥 그룹 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    group_type VARCHAR(20) DEFAULT 'functional',     -- functional, location, equipment, severity
    
    -- 🔥 그룹 설정
    suppress_redundant_alarms INTEGER DEFAULT 1,     -- 중복 알람 억제
    escalation_enabled INTEGER DEFAULT 0,            -- 그룹 단위 에스컬레이션
    notification_consolidation INTEGER DEFAULT 1,    -- 알림 통합
    
    -- 🔥 상태 정보
    is_active INTEGER DEFAULT 1,
    priority INTEGER DEFAULT 100,
    
    -- 🔥 감사 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_alarm_group_type CHECK (group_type IN ('functional', 'location', 'equipment', 'severity'))
);

-- 알람 그룹 멤버십
CREATE TABLE IF NOT EXISTS alarm_group_members (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    group_id INTEGER NOT NULL,
    rule_id INTEGER NOT NULL,
    
    -- 🔥 멤버십 정보
    member_role VARCHAR(20) DEFAULT 'member',        -- member, leader, observer
    priority_override INTEGER,                       -- 그룹 내 우선순위 오버라이드
    
    -- 🔥 감사 정보
    added_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    added_by INTEGER,
    
    FOREIGN KEY (group_id) REFERENCES alarm_groups(id) ON DELETE CASCADE,
    FOREIGN KEY (rule_id) REFERENCES alarm_rules(id) ON DELETE CASCADE,
    FOREIGN KEY (added_by) REFERENCES users(id),
    
    UNIQUE(group_id, rule_id),
    
    -- 🔥 제약조건
    CONSTRAINT chk_member_role CHECK (member_role IN ('member', 'leader', 'observer'))
);

-- =============================================================================
-- 인덱스 생성 (성능 최적화)
-- =============================================================================

-- alarm_rules 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_rules_tenant ON alarm_rules(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_target ON alarm_rules(target_type, target_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_enabled ON alarm_rules(is_enabled);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_type ON alarm_rules(alarm_type);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_severity ON alarm_rules(severity);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_name ON alarm_rules(tenant_id, name);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_priority ON alarm_rules(priority);

-- alarm_occurrences 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_rule ON alarm_occurrences(rule_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_tenant ON alarm_occurrences(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_state ON alarm_occurrences(state);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_time ON alarm_occurrences(occurrence_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_severity ON alarm_occurrences(severity);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_active ON alarm_occurrences(state, occurrence_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_acknowledged ON alarm_occurrences(acknowledged_time DESC);

-- javascript_functions 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_js_functions_tenant ON javascript_functions(tenant_id);
CREATE INDEX IF NOT EXISTS idx_js_functions_category ON javascript_functions(category);
CREATE INDEX IF NOT EXISTS idx_js_functions_enabled ON javascript_functions(is_enabled);
CREATE INDEX IF NOT EXISTS idx_js_functions_system ON javascript_functions(is_system);
CREATE INDEX IF NOT EXISTS idx_js_functions_usage ON javascript_functions(usage_count DESC);

-- recipes 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_recipes_tenant ON recipes(tenant_id);
CREATE INDEX IF NOT EXISTS idx_recipes_active ON recipes(is_active);
CREATE INDEX IF NOT EXISTS idx_recipes_category ON recipes(category);
CREATE INDEX IF NOT EXISTS idx_recipes_name ON recipes(tenant_id, name);

-- schedules 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_schedules_tenant ON schedules(tenant_id);
CREATE INDEX IF NOT EXISTS idx_schedules_enabled ON schedules(is_enabled);
CREATE INDEX IF NOT EXISTS idx_schedules_type ON schedules(schedule_type);
CREATE INDEX IF NOT EXISTS idx_schedules_next_exec ON schedules(next_execution_time);
CREATE INDEX IF NOT EXISTS idx_schedules_last_exec ON schedules(last_execution_time DESC);

-- alarm_groups 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_groups_tenant ON alarm_groups(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_groups_type ON alarm_groups(group_type);
CREATE INDEX IF NOT EXISTS idx_alarm_groups_active ON alarm_groups(is_active);

-- alarm_group_members 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_group_members_group ON alarm_group_members(group_id);
CREATE INDEX IF NOT EXISTS idx_alarm_group_members_rule ON alarm_group_members(rule_id);
CREATE INDEX IF NOT EXISTS idx_alarm_group_members_role ON alarm_group_members(member_role);