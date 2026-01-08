-- =============================================================================
-- backend/lib/database/schemas/04-virtual-points.sql
-- 가상 포인트 테이블 (SQLite 버전) - 2025-08-14 최신 업데이트
-- PulseOne v2.1.0 완전 호환, JavaScript 엔진 통합 완료
-- =============================================================================

-- =============================================================================
-- 🔥🔥🔥 가상포인트 테이블 - 현재 DB 구조와 완전 일치
-- =============================================================================
CREATE TABLE IF NOT EXISTS virtual_points (
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

-- =============================================================================
-- 가상포인트 입력 매핑 - 현재 DB 구조와 완전 일치
-- =============================================================================
CREATE TABLE IF NOT EXISTS virtual_point_inputs (
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

-- =============================================================================
-- 가상포인트 현재값 - 현재 DB 구조와 완전 일치
-- =============================================================================
CREATE TABLE IF NOT EXISTS virtual_point_values (
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

-- =============================================================================
-- 🔥🔥🔥 v3.0.0 고급 기능 테이블들 (현재 DB에 존재하는 테이블들)
-- =============================================================================

-- 가상포인트 실행 이력
CREATE TABLE IF NOT EXISTS virtual_point_execution_history (
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

-- 의존성 관리 테이블
CREATE TABLE IF NOT EXISTS virtual_point_dependencies (
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

-- =============================================================================
-- 스크립트 라이브러리 테이블 (가상포인트 공통 함수)
-- =============================================================================
CREATE TABLE IF NOT EXISTS script_library (
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

-- 스크립트 라이브러리 버전 관리
CREATE TABLE IF NOT EXISTS script_library_versions (
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

-- 스크립트 사용 이력
CREATE TABLE IF NOT EXISTS script_usage_history (
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

-- 스크립트 템플릿
CREATE TABLE IF NOT EXISTS script_templates (
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

-- =============================================================================
-- 인덱스 생성 (성능 최적화)
-- =============================================================================

-- virtual_points 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_virtual_points_tenant ON virtual_points(tenant_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_scope ON virtual_points(scope_type);
CREATE INDEX IF NOT EXISTS idx_virtual_points_site ON virtual_points(site_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_device ON virtual_points(device_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_enabled ON virtual_points(is_enabled);
CREATE INDEX IF NOT EXISTS idx_virtual_points_category ON virtual_points(category);
CREATE INDEX IF NOT EXISTS idx_virtual_points_trigger ON virtual_points(calculation_trigger);
CREATE INDEX IF NOT EXISTS idx_virtual_points_name ON virtual_points(tenant_id, name);

-- virtual_point_inputs 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_vp_inputs_virtual_point ON virtual_point_inputs(virtual_point_id);
CREATE INDEX IF NOT EXISTS idx_vp_inputs_source ON virtual_point_inputs(source_type, source_id);
CREATE INDEX IF NOT EXISTS idx_vp_inputs_variable ON virtual_point_inputs(virtual_point_id, variable_name);

-- virtual_point_values 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_vp_values_calculated ON virtual_point_values(last_calculated DESC);
CREATE INDEX IF NOT EXISTS idx_vp_values_quality ON virtual_point_values(quality);
CREATE INDEX IF NOT EXISTS idx_vp_values_stale ON virtual_point_values(is_stale);
CREATE INDEX IF NOT EXISTS idx_vp_values_alarm ON virtual_point_values(alarm_active);

-- virtual_point_execution_history 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_vp_execution_history_vp_id ON virtual_point_execution_history(virtual_point_id);
CREATE INDEX IF NOT EXISTS idx_vp_execution_history_time ON virtual_point_execution_history(execution_time DESC);
CREATE INDEX IF NOT EXISTS idx_vp_execution_history_result ON virtual_point_execution_history(result_type);
CREATE INDEX IF NOT EXISTS idx_vp_execution_history_trigger ON virtual_point_execution_history(trigger_source);

-- virtual_point_dependencies 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_vp_dependencies_vp_id ON virtual_point_dependencies(virtual_point_id);
CREATE INDEX IF NOT EXISTS idx_vp_dependencies_depends_on ON virtual_point_dependencies(depends_on_type, depends_on_id);
CREATE INDEX IF NOT EXISTS idx_vp_dependencies_active ON virtual_point_dependencies(is_active);

-- script_library 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_script_library_tenant ON script_library(tenant_id);
CREATE INDEX IF NOT EXISTS idx_script_library_category ON script_library(category);
CREATE INDEX IF NOT EXISTS idx_script_library_active ON script_library(is_active);
CREATE INDEX IF NOT EXISTS idx_script_library_system ON script_library(is_system);
CREATE INDEX IF NOT EXISTS idx_script_library_name ON script_library(tenant_id, name);
CREATE INDEX IF NOT EXISTS idx_script_library_usage ON script_library(usage_count DESC);

-- script_library_versions 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_script_versions_script ON script_library_versions(script_id);
CREATE INDEX IF NOT EXISTS idx_script_versions_version ON script_library_versions(script_id, version_number);

-- script_usage_history 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_script_usage_script ON script_usage_history(script_id);
CREATE INDEX IF NOT EXISTS idx_script_usage_vp ON script_usage_history(virtual_point_id);
CREATE INDEX IF NOT EXISTS idx_script_usage_tenant ON script_usage_history(tenant_id);
CREATE INDEX IF NOT EXISTS idx_script_usage_time ON script_usage_history(used_at DESC);
CREATE INDEX IF NOT EXISTS idx_script_usage_context ON script_usage_history(usage_context);

-- script_templates 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_script_templates_category ON script_templates(category);
CREATE INDEX IF NOT EXISTS idx_script_templates_industry ON script_templates(industry);
CREATE INDEX IF NOT EXISTS idx_script_templates_equipment ON script_templates(equipment_type);
CREATE INDEX IF NOT EXISTS idx_script_templates_active ON script_templates(is_active);
CREATE INDEX IF NOT EXISTS idx_script_templates_difficulty ON script_templates(difficulty_level);
CREATE INDEX IF NOT EXISTS idx_script_templates_popularity ON script_templates(popularity_score DESC);