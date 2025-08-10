-- =============================================================================
-- 가상포인트 핵심 테이블 생성 스크립트
-- PulseOne v3.0.0 - Virtual Points Core Tables
-- =============================================================================

-- =============================================================================
-- 1. VIRTUAL_POINTS 메인 테이블 (가장 중요!)
-- =============================================================================
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

-- =============================================================================
-- 2. VIRTUAL_POINT_INPUTS 테이블 (다중 입력 매핑)
-- =============================================================================
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

-- =============================================================================
-- 3. VIRTUAL_POINT_VALUES 테이블 (현재값 저장)
-- =============================================================================
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

-- =============================================================================
-- 4. 인덱스 생성
-- =============================================================================
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

-- =============================================================================
-- 5. 샘플 데이터 (테스트용)
-- =============================================================================

-- 샘플 가상포인트: 평균 온도 계산
INSERT OR IGNORE INTO virtual_points (
    id, tenant_id, scope_type, site_id, 
    name, description, formula, data_type, unit,
    calculation_interval, is_enabled
) VALUES (
    1, 1, 'site', 1,
    'Average_Temperature', '3개 센서의 평균 온도',
    '// 평균 온도 계산
const validTemps = [inputs.temp1, inputs.temp2, inputs.temp3].filter(t => t != null);
return validTemps.length > 0 ? validTemps.reduce((a,b) => a+b) / validTemps.length : null;',
    'float', '°C',
    5000, 1
);

-- 가상포인트 입력 매핑 (3개 온도 센서)
INSERT OR IGNORE INTO virtual_point_inputs (virtual_point_id, variable_name, source_type, source_id) VALUES 
(1, 'temp1', 'data_point', 101),
(1, 'temp2', 'data_point', 102),
(1, 'temp3', 'data_point', 103);

-- 샘플 가상포인트: 총 전력 소비
INSERT OR IGNORE INTO virtual_points (
    id, tenant_id, scope_type, site_id,
    name, description, formula, data_type, unit,
    calculation_interval, is_enabled
) VALUES (
    2, 1, 'site', 1,
    'Total_Power_Consumption', '전체 전력 소비량',
    'return (inputs.motor1 || 0) + (inputs.motor2 || 0) + (inputs.hvac || 0) + (inputs.lighting || 0);',
    'float', 'kW',
    2000, 1
);

-- 전력 소비 입력 매핑
INSERT OR IGNORE INTO virtual_point_inputs (virtual_point_id, variable_name, source_type, source_id) VALUES 
(2, 'motor1', 'data_point', 201),
(2, 'motor2', 'data_point', 202),
(2, 'hvac', 'data_point', 203),
(2, 'lighting', 'constant', NULL);

-- lighting은 상수값 설정
UPDATE virtual_point_inputs 
SET constant_value = 15.5 
WHERE virtual_point_id = 2 AND variable_name = 'lighting';

-- 샘플 가상포인트: OEE 계산
INSERT OR IGNORE INTO virtual_points (
    id, tenant_id, scope_type, site_id,
    name, description, formula, data_type, unit,
    calculation_interval, is_enabled
) VALUES (
    3, 1, 'site', 1,
    'OEE_Line1', 'Line 1 OEE (Overall Equipment Effectiveness)',
    '// OEE = Availability × Performance × Quality
const availability = inputs.availability || 0;
const performance = inputs.performance || 0;
const quality = inputs.quality || 0;
return (availability * performance * quality) / 10000;',
    'float', '%',
    60000, 1
);

-- OEE 입력 매핑 (다른 가상포인트 참조)
INSERT OR IGNORE INTO virtual_point_inputs (virtual_point_id, variable_name, source_type, source_id) VALUES 
(3, 'availability', 'data_point', 301),
(3, 'performance', 'data_point', 302),
(3, 'quality', 'virtual_point', 4);  -- 다른 가상포인트 참조

-- =============================================================================
-- 완료 메시지
-- =============================================================================
SELECT '✅ 가상포인트 핵심 테이블 생성 완료' as message;
SELECT '📊 생성된 테이블:' as info;
SELECT '  - virtual_points (메인 테이블)' as table1;
SELECT '  - virtual_point_inputs (입력 매핑)' as table2;
SELECT '  - virtual_point_values (현재값)' as table3;
SELECT '' as blank;
SELECT '🔧 다음 명령으로 확인: sqlite3 db/pulseone_test.db ".tables"' as next_step;