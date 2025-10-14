-- ============================================================================
-- PulseOne Export System - Database Schema (SQLite)
-- 
-- 파일명: 001_export_system_schema.sql
-- 목적: Export Gateway 및 Protocol Server를 위한 데이터베이스 스키마
-- 버전: 1.0
-- 작성일: 2025-10-02
--
-- 적용 방법:
--   sqlite3 /app/data/pulseone.db < 001_export_system_schema.sql
-- ============================================================================

PRAGMA foreign_keys = ON;

-- ============================================================================
-- 1. export_profiles (내보낼 데이터 세트)
-- ============================================================================

CREATE TABLE IF NOT EXISTS export_profiles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL UNIQUE,
    description TEXT,
    is_enabled BOOLEAN DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by VARCHAR(50),
    point_count INTEGER DEFAULT 0,
    last_exported_at DATETIME
);

CREATE INDEX idx_profiles_enabled ON export_profiles(is_enabled);
CREATE INDEX idx_profiles_created ON export_profiles(created_at DESC);

-- ============================================================================
-- 2. export_profile_points (프로파일에 포함할 포인트들)
-- ============================================================================

CREATE TABLE IF NOT EXISTS export_profile_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER NOT NULL,
    point_id INTEGER NOT NULL,
    display_order INTEGER DEFAULT 0,
    display_name VARCHAR(200),
    is_enabled BOOLEAN DEFAULT 1,
    added_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    added_by VARCHAR(50),
    
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE CASCADE,
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    UNIQUE(profile_id, point_id)
);

CREATE INDEX idx_profile_points_profile ON export_profile_points(profile_id);
CREATE INDEX idx_profile_points_point ON export_profile_points(point_id);
CREATE INDEX idx_profile_points_order ON export_profile_points(profile_id, display_order);

-- ============================================================================
-- 3. protocol_services (프로토콜 서비스 설정)
-- ============================================================================

CREATE TABLE IF NOT EXISTS protocol_services (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER NOT NULL,
    service_type VARCHAR(20) NOT NULL,
    service_name VARCHAR(100) NOT NULL,
    description TEXT,
    is_enabled BOOLEAN DEFAULT 1,
    config TEXT NOT NULL,
    active_connections INTEGER DEFAULT 0,
    total_requests INTEGER DEFAULT 0,
    last_request_at DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE CASCADE
);

CREATE INDEX idx_protocol_services_profile ON protocol_services(profile_id);
CREATE INDEX idx_protocol_services_type ON protocol_services(service_type);
CREATE INDEX idx_protocol_services_enabled ON protocol_services(is_enabled);

-- ============================================================================
-- 4. protocol_mappings (프로토콜별 주소/경로 매핑)
-- ============================================================================

CREATE TABLE IF NOT EXISTS protocol_mappings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    service_id INTEGER NOT NULL,
    point_id INTEGER NOT NULL,
    external_identifier VARCHAR(200) NOT NULL,
    external_name VARCHAR(200),
    external_description VARCHAR(500),
    conversion_config TEXT,
    protocol_config TEXT,
    is_enabled BOOLEAN DEFAULT 1,
    read_count INTEGER DEFAULT 0,
    write_count INTEGER DEFAULT 0,
    last_read_at DATETIME,
    last_write_at DATETIME,
    error_count INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (service_id) REFERENCES protocol_services(id) ON DELETE CASCADE,
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    UNIQUE(service_id, external_identifier)
);

CREATE INDEX idx_protocol_mappings_service ON protocol_mappings(service_id);
CREATE INDEX idx_protocol_mappings_point ON protocol_mappings(point_id);
CREATE INDEX idx_protocol_mappings_identifier ON protocol_mappings(service_id, external_identifier);

-- ============================================================================
-- 5. export_targets (외부 전송 타겟 - Export Gateway용)
-- ============================================================================

CREATE TABLE IF NOT EXISTS export_targets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER,
    name VARCHAR(100) NOT NULL UNIQUE,
    target_type VARCHAR(20) NOT NULL,
    description TEXT,
    is_enabled BOOLEAN DEFAULT 1,
    config TEXT NOT NULL,
    export_mode VARCHAR(20) DEFAULT 'on_change',
    export_interval INTEGER DEFAULT 0,
    batch_size INTEGER DEFAULT 100,
    total_exports INTEGER DEFAULT 0,
    successful_exports INTEGER DEFAULT 0,
    failed_exports INTEGER DEFAULT 0,
    last_export_at DATETIME,
    last_success_at DATETIME,
    last_error TEXT,
    last_error_at DATETIME,
    avg_export_time_ms INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE SET NULL
);

CREATE INDEX idx_export_targets_type ON export_targets(target_type);
CREATE INDEX idx_export_targets_profile ON export_targets(profile_id);
CREATE INDEX idx_export_targets_enabled ON export_targets(is_enabled);

-- ============================================================================
-- 6. export_target_mappings (Export Target별 매핑 - 선택사항)
-- ============================================================================

CREATE TABLE IF NOT EXISTS export_target_mappings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    target_id INTEGER NOT NULL,
    point_id INTEGER NOT NULL,
    target_field_name VARCHAR(200),
    target_description VARCHAR(500),
    conversion_config TEXT,
    is_enabled BOOLEAN DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (target_id) REFERENCES export_targets(id) ON DELETE CASCADE,
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    UNIQUE(target_id, point_id)
);

CREATE INDEX idx_export_target_mappings_target ON export_target_mappings(target_id);
CREATE INDEX idx_export_target_mappings_point ON export_target_mappings(point_id);

-- ============================================================================
-- 7. export_logs (전송 로그)
-- ============================================================================

CREATE TABLE IF NOT EXISTS export_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    log_type VARCHAR(20) NOT NULL,
    service_id INTEGER,
    target_id INTEGER,
    mapping_id INTEGER,
    point_id INTEGER,
    source_value TEXT,
    converted_value TEXT,
    status VARCHAR(20) NOT NULL,
    error_message TEXT,
    error_code VARCHAR(50),
    response_data TEXT,
    http_status_code INTEGER,
    processing_time_ms INTEGER,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    client_info TEXT,
    
    FOREIGN KEY (service_id) REFERENCES protocol_services(id) ON DELETE SET NULL,
    FOREIGN KEY (target_id) REFERENCES export_targets(id) ON DELETE SET NULL,
    FOREIGN KEY (mapping_id) REFERENCES protocol_mappings(id) ON DELETE SET NULL
);

CREATE INDEX idx_export_logs_type ON export_logs(log_type);
CREATE INDEX idx_export_logs_timestamp ON export_logs(timestamp DESC);
CREATE INDEX idx_export_logs_status ON export_logs(status);
CREATE INDEX idx_export_logs_service ON export_logs(service_id);
CREATE INDEX idx_export_logs_target ON export_logs(target_id);

-- ============================================================================
-- 8. export_schedules (예약 작업 - 선택사항)
-- ============================================================================

CREATE TABLE IF NOT EXISTS export_schedules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER,
    target_id INTEGER NOT NULL,
    schedule_name VARCHAR(100) NOT NULL,
    description TEXT,
    cron_expression VARCHAR(100) NOT NULL,
    timezone VARCHAR(50) DEFAULT 'UTC',
    data_range VARCHAR(20) DEFAULT 'day',
    lookback_periods INTEGER DEFAULT 1,
    is_enabled BOOLEAN DEFAULT 1,
    last_run_at DATETIME,
    last_status VARCHAR(20),
    next_run_at DATETIME,
    total_runs INTEGER DEFAULT 0,
    successful_runs INTEGER DEFAULT 0,
    failed_runs INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE SET NULL,
    FOREIGN KEY (target_id) REFERENCES export_targets(id) ON DELETE CASCADE
);

CREATE INDEX idx_export_schedules_enabled ON export_schedules(is_enabled);
CREATE INDEX idx_export_schedules_next_run ON export_schedules(next_run_at);

-- ============================================================================
-- 뷰 (View) - 편의 기능
-- ============================================================================

CREATE VIEW IF NOT EXISTS v_export_profiles_detail AS
SELECT 
    p.id,
    p.name,
    p.description,
    p.is_enabled,
    COUNT(pp.id) as point_count,
    COUNT(CASE WHEN pp.is_enabled = 1 THEN 1 END) as active_point_count,
    p.created_at,
    p.updated_at
FROM export_profiles p
LEFT JOIN export_profile_points pp ON p.id = pp.profile_id
GROUP BY p.id;

CREATE VIEW IF NOT EXISTS v_protocol_services_detail AS
SELECT 
    ps.id,
    ps.profile_id,
    p.name as profile_name,
    ps.service_type,
    ps.service_name,
    ps.is_enabled,
    COUNT(pm.id) as mapping_count,
    COUNT(CASE WHEN pm.is_enabled = 1 THEN 1 END) as active_mapping_count,
    ps.active_connections,
    ps.total_requests,
    ps.last_request_at
FROM protocol_services ps
LEFT JOIN export_profiles p ON ps.profile_id = p.id
LEFT JOIN protocol_mappings pm ON ps.id = pm.service_id
GROUP BY ps.id;

CREATE VIEW IF NOT EXISTS v_export_targets_stats AS
SELECT 
    t.id,
    t.name,
    t.target_type,
    t.is_enabled,
    t.total_exports,
    t.successful_exports,
    t.failed_exports,
    CASE 
        WHEN t.total_exports > 0 THEN 
            ROUND((t.successful_exports * 100.0) / t.total_exports, 2)
        ELSE 0 
    END as success_rate,
    t.last_export_at,
    t.last_success_at,
    t.avg_export_time_ms
FROM export_targets t;

-- ============================================================================
-- 트리거 (Trigger)
-- ============================================================================

CREATE TRIGGER IF NOT EXISTS tr_export_profiles_update
AFTER UPDATE ON export_profiles
BEGIN
    UPDATE export_profiles 
    SET updated_at = CURRENT_TIMESTAMP 
    WHERE id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS tr_profile_points_insert
AFTER INSERT ON export_profile_points
BEGIN
    UPDATE export_profiles 
    SET point_count = (
        SELECT COUNT(*) 
        FROM export_profile_points 
        WHERE profile_id = NEW.profile_id
    )
    WHERE id = NEW.profile_id;
END;

CREATE TRIGGER IF NOT EXISTS tr_profile_points_delete
AFTER DELETE ON export_profile_points
BEGIN
    UPDATE export_profiles 
    SET point_count = (
        SELECT COUNT(*) 
        FROM export_profile_points 
        WHERE profile_id = OLD.profile_id
    )
    WHERE id = OLD.profile_id;
END;

-- ============================================================================
-- 초기 테스트 데이터
-- ============================================================================

INSERT INTO export_profiles (name, description) VALUES 
    ('건물 1층 센서 데이터', '1층의 온도, 습도, CO2 센서 데이터'),
    ('공조기 실시간 데이터', 'AHU-01의 운전 상태 및 센서값');

-- ============================================================================
-- 완료 메시지
-- ============================================================================

SELECT '✅ PulseOne Export System 스키마 생성 완료!' as message;