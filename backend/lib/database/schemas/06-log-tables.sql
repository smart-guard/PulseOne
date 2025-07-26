-- =============================================================================
-- backend/lib/database/schemas/06-log-tables.sql
-- 로깅 시스템 테이블 (SQLite 버전)
-- =============================================================================

-- 시스템 로그 테이블
CREATE TABLE IF NOT EXISTS system_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER,
    user_id INTEGER,
    
    -- 로그 정보
    log_level VARCHAR(10) NOT NULL, -- DEBUG, INFO, WARN, ERROR, FATAL
    module VARCHAR(50) NOT NULL,
    message TEXT NOT NULL,
    
    -- 컨텍스트 정보
    ip_address VARCHAR(45),
    user_agent TEXT,
    request_id VARCHAR(50),
    
    -- 메타데이터
    details TEXT, -- JSON 형태
    
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE SET NULL,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL
);

-- 사용자 활동 로그
CREATE TABLE IF NOT EXISTS user_activities (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER,
    
    -- 활동 정보
    action VARCHAR(50) NOT NULL, -- login, logout, create, update, delete
    resource_type VARCHAR(50), -- device, data_point, alarm, user
    resource_id INTEGER,
    
    -- 세부 정보
    details TEXT, -- JSON 형태
    ip_address VARCHAR(45),
    user_agent TEXT,
    
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL
);

-- 통신 로그 테이블
CREATE TABLE IF NOT EXISTS communication_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER,
    
    -- 통신 정보
    direction VARCHAR(10) NOT NULL, -- request, response
    protocol VARCHAR(20) NOT NULL,
    raw_data TEXT, -- 원시 통신 데이터
    decoded_data TEXT, -- 해석된 데이터 (JSON)
    
    -- 결과 정보
    success INTEGER,
    error_message TEXT,
    response_time INTEGER, -- 밀리초
    
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- 데이터 히스토리 테이블 (시계열)
CREATE TABLE IF NOT EXISTS data_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    point_id INTEGER NOT NULL,
    value DECIMAL(15,4),
    raw_value DECIMAL(15,4),
    quality VARCHAR(20),
    timestamp DATETIME NOT NULL,
    
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE
);

-- 가상 포인트 히스토리
CREATE TABLE IF NOT EXISTS virtual_point_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    value DECIMAL(15,4),
    quality VARCHAR(20),
    timestamp DATETIME NOT NULL,
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE
);
