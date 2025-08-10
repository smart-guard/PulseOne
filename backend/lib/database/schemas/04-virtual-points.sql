-- =============================================================================
-- backend/lib/database/schemas/04-virtual-points.sql
-- 가상 포인트 테이블 (SQLite 버전) - 현재 DB와 동기화된 최신 버전
-- =============================================================================

-- 가상포인트 테이블 (현재 DB 구조와 일치)
CREATE TABLE IF NOT EXISTS virtual_points (
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
    
    -- 🔥 확장 필드 (v3.0.0) - 현재 DB에 추가되어 있는 컬럼들
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
    FOREIGN KEY (created_by) REFERENCES users(id),
    
    -- 범위별 제약조건
    CONSTRAINT chk_scope_consistency CHECK (
        (scope_type = 'tenant' AND site_id IS NULL AND device_id IS NULL) OR
        (scope_type = 'site' AND site_id IS NOT NULL AND device_id IS NULL) OR
        (scope_type = 'device' AND site_id IS NOT NULL AND device_id IS NOT NULL)
    ),
    CONSTRAINT chk_scope_type CHECK (scope_type IN ('tenant', 'site', 'device'))
);

-- 가상포인트 입력 매핑 (현재 DB 구조와 일치)
CREATE TABLE IF NOT EXISTS virtual_point_inputs (
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

-- 가상포인트 현재값 (현재 DB 구조와 일치)
CREATE TABLE IF NOT EXISTS virtual_point_values (
    virtual_point_id INTEGER PRIMARY KEY,
    value REAL,
    string_value TEXT, -- 문자열 타입 가상포인트용
    quality VARCHAR(20) DEFAULT 'good',
    last_calculated DATETIME DEFAULT CURRENT_TIMESTAMP,
    calculation_error TEXT, -- 계산 오류 메시지
    input_values TEXT, -- JSON: 계산에 사용된 입력값들 (디버깅용)
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE
);

-- 🔥 v3.0.0 추가 테이블들 (현재 DB에 존재하는 테이블들)

-- 가상포인트 실행 이력
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

-- 의존성 관리 테이블
CREATE TABLE IF NOT EXISTS virtual_point_dependencies (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    depends_on_type VARCHAR(20) NOT NULL,  -- 'data_point' or 'virtual_point'
    depends_on_id INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    UNIQUE(virtual_point_id, depends_on_type, depends_on_id)
);

-- 인덱스들 (현재 DB에 존재하는 인덱스들)
CREATE INDEX IF NOT EXISTS idx_virtual_points_tenant ON virtual_points(tenant_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_scope ON virtual_points(scope_type);
CREATE INDEX IF NOT EXISTS idx_virtual_points_site ON virtual_points(site_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_device ON virtual_points(device_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_enabled ON virtual_points(is_enabled);
CREATE INDEX IF NOT EXISTS idx_virtual_points_category ON virtual_points(category);
CREATE INDEX IF NOT EXISTS idx_virtual_points_trigger ON virtual_points(calculation_trigger);

CREATE INDEX IF NOT EXISTS idx_vp_inputs_virtual_point ON virtual_point_inputs(virtual_point_id);
CREATE INDEX IF NOT EXISTS idx_vp_inputs_source ON virtual_point_inputs(source_type, source_id);

CREATE INDEX IF NOT EXISTS idx_vp_values_calculated ON virtual_point_values(last_calculated DESC);

CREATE INDEX IF NOT EXISTS idx_vp_execution_history_vp_id ON virtual_point_execution_history(virtual_point_id);
CREATE INDEX IF NOT EXISTS idx_vp_execution_history_time ON virtual_point_execution_history(execution_time DESC);

CREATE INDEX IF NOT EXISTS idx_vp_dependencies_vp_id ON virtual_point_dependencies(virtual_point_id);
CREATE INDEX IF NOT EXISTS idx_vp_dependencies_depends_on ON virtual_point_dependencies(depends_on_type, depends_on_id);