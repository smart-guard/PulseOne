-- =============================================================================
-- backend/lib/database/schemas/04-virtual-points.sql
-- 가상 포인트 테이블 (SQLite 버전)
-- =============================================================================

-- 가상포인트 테이블
CREATE TABLE IF NOT EXISTS virtual_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 유연한 범위 설정
    scope_type VARCHAR(20) NOT NULL, -- tenant, site, device
    site_id INTEGER,     -- scope_type이 'site'이거나 'device'일 때
    device_id INTEGER,   -- scope_type이 'device'일 때만
    
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

-- 가상포인트 입력 매핑
CREATE TABLE IF NOT EXISTS virtual_point_inputs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    variable_name VARCHAR(50) NOT NULL, -- 계산식에서 사용할 변수명
    
    -- 다양한 소스 타입 지원
    source_type VARCHAR(20) NOT NULL, -- data_point, virtual_point, constant, formula
    source_id INTEGER, -- data_points.id 또는 virtual_points.id
    constant_value DECIMAL(15,4), -- source_type이 'constant'일 때
    source_formula TEXT, -- source_type이 'formula'일 때
    
    -- 데이터 처리 옵션
    data_processing VARCHAR(20) DEFAULT 'current', -- current, average, min, max, sum
    time_window_seconds INTEGER, -- data_processing이 'current'가 아닐 때
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    UNIQUE(virtual_point_id, variable_name),
    CONSTRAINT chk_source_type CHECK (source_type IN ('data_point', 'virtual_point', 'constant', 'formula'))
);

-- 가상포인트 현재값
CREATE TABLE IF NOT EXISTS virtual_point_values (
    virtual_point_id INTEGER PRIMARY KEY,
    value DECIMAL(15,4),
    quality VARCHAR(20) DEFAULT 'good',
    last_calculated DATETIME DEFAULT CURRENT_TIMESTAMP,
    calculation_error TEXT, -- 계산 오류 메시지
    input_values TEXT, -- JSON: 계산에 사용된 입력값들 (디버깅용)
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE
);
