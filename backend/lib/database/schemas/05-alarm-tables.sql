-- =============================================================================
-- backend/lib/database/schemas/05-alarm-tables.sql
-- 알람 시스템 테이블 (SQLite 버전)
-- =============================================================================

-- 알람 정의 테이블
CREATE TABLE IF NOT EXISTS alarm_definitions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER, -- 특정 사이트에 속할 수도 있음
    data_point_id INTEGER,
    virtual_point_id INTEGER,
    
    -- 알람 기본 정보
    alarm_name VARCHAR(100) NOT NULL,
    description TEXT,
    severity VARCHAR(20) DEFAULT 'medium', -- low, medium, high, critical
    
    -- 조건 설정
    condition_type VARCHAR(20) NOT NULL, -- threshold, range, change, timeout, discrete
    threshold_value DECIMAL(15,4),
    high_limit DECIMAL(15,4),
    low_limit DECIMAL(15,4),
    timeout_seconds INTEGER,
    
    -- 알람 동작
    is_enabled INTEGER DEFAULT 1,
    auto_acknowledge INTEGER DEFAULT 0,
    delay_seconds INTEGER DEFAULT 0,
    
    -- 메시지 설정
    message_template TEXT,
    
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE SET NULL,
    FOREIGN KEY (data_point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE
);

-- 활성 알람 테이블
CREATE TABLE IF NOT EXISTS active_alarms (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    alarm_definition_id INTEGER NOT NULL,
    
    -- 알람 발생 정보
    triggered_value DECIMAL(15,4),
    message TEXT NOT NULL,
    triggered_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 알람 응답 정보
    acknowledged_at DATETIME,
    acknowledged_by INTEGER,
    acknowledgment_comment TEXT,
    
    -- 상태
    is_active INTEGER DEFAULT 1,
    
    FOREIGN KEY (alarm_definition_id) REFERENCES alarm_definitions(id) ON DELETE CASCADE,
    FOREIGN KEY (acknowledged_by) REFERENCES users(id)
);

-- 알람 이벤트 히스토리
CREATE TABLE IF NOT EXISTS alarm_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    alarm_definition_id INTEGER NOT NULL,
    
    -- 이벤트 정보
    event_type VARCHAR(20) NOT NULL, -- triggered, acknowledged, cleared, disabled
    triggered_value DECIMAL(15,4),
    message TEXT,
    event_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    user_id INTEGER,
    
    FOREIGN KEY (alarm_definition_id) REFERENCES alarm_definitions(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id)
);
