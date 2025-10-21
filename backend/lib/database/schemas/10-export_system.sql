-- ============================================================================
-- PulseOne Export System - Database Schema (SQLite)
-- 
-- 파일명: 10-export_system.sql
-- 목적: Export Gateway 및 Protocol Server를 위한 데이터베이스 스키마
-- 버전: 2.0 (리팩토링)
-- 작성일: 2025-10-21
--
-- 주요 변경사항 (v1.0 → v2.0):
--   - export_targets: 통계 필드 제거 (설정만 보관)
--   - export_logs: 확장 (모든 전송 로그 저장)
--   - VIEW 추가: 실시간 통계 집계
--
-- 적용 방법:
--   sqlite3 /app/data/pulseone.db < 10-export_system.sql
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

CREATE INDEX IF NOT EXISTS idx_profiles_enabled ON export_profiles(is_enabled);
CREATE INDEX IF NOT EXISTS idx_profiles_created ON export_profiles(created_at DESC);

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

CREATE INDEX IF NOT EXISTS idx_profile_points_profile ON export_profile_points(profile_id);
CREATE INDEX IF NOT EXISTS idx_profile_points_point ON export_profile_points(point_id);
CREATE INDEX IF NOT EXISTS idx_profile_points_order ON export_profile_points(profile_id, display_order);

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

CREATE INDEX IF NOT EXISTS idx_protocol_services_profile ON protocol_services(profile_id);
CREATE INDEX IF NOT EXISTS idx_protocol_services_type ON protocol_services(service_type);
CREATE INDEX IF NOT EXISTS idx_protocol_services_enabled ON protocol_services(is_enabled);

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

CREATE INDEX IF NOT EXISTS idx_protocol_mappings_service ON protocol_mappings(service_id);
CREATE INDEX IF NOT EXISTS idx_protocol_mappings_point ON protocol_mappings(point_id);
CREATE INDEX IF NOT EXISTS idx_protocol_mappings_identifier ON protocol_mappings(service_id, external_identifier);

-- ============================================================================
-- 5. export_targets (외부 전송 타겟 - 설정만)
-- ============================================================================

CREATE TABLE IF NOT EXISTS export_targets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER,
    
    -- 기본 정보
    name VARCHAR(100) NOT NULL UNIQUE,
    target_type VARCHAR(20) NOT NULL,
    description TEXT,
    
    -- 설정 정보
    config TEXT NOT NULL,
    is_enabled BOOLEAN DEFAULT 1,
    
    -- 전송 옵션
    export_mode VARCHAR(20) DEFAULT 'on_change',
    export_interval INTEGER DEFAULT 0,
    batch_size INTEGER DEFAULT 100,
    
    -- 메타 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_export_targets_type ON export_targets(target_type);
CREATE INDEX IF NOT EXISTS idx_export_targets_profile ON export_targets(profile_id);
CREATE INDEX IF NOT EXISTS idx_export_targets_enabled ON export_targets(is_enabled);
CREATE INDEX IF NOT EXISTS idx_export_targets_name ON export_targets(name);

-- ============================================================================
-- 6. export_target_mappings (Export Target별 매핑)
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

CREATE INDEX IF NOT EXISTS idx_export_target_mappings_target ON export_target_mappings(target_id);
CREATE INDEX IF NOT EXISTS idx_export_target_mappings_point ON export_target_mappings(point_id);

-- ============================================================================
-- 7. export_logs (전송 로그 - 확장)
-- ============================================================================

CREATE TABLE IF NOT EXISTS export_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 기본 분류
    log_type VARCHAR(20) NOT NULL,
    
    -- 관계 (FK)
    service_id INTEGER,
    target_id INTEGER,
    mapping_id INTEGER,
    point_id INTEGER,
    
    -- 데이터
    source_value TEXT,
    converted_value TEXT,
    
    -- 결과
    status VARCHAR(20) NOT NULL,
    http_status_code INTEGER,
    
    -- 에러 정보
    error_message TEXT,
    error_code VARCHAR(50),
    response_data TEXT,
    
    -- 성능 정보
    processing_time_ms INTEGER,
    
    -- 메타 정보
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    client_info TEXT,
    
    FOREIGN KEY (service_id) REFERENCES protocol_services(id) ON DELETE SET NULL,
    FOREIGN KEY (target_id) REFERENCES export_targets(id) ON DELETE SET NULL,
    FOREIGN KEY (mapping_id) REFERENCES protocol_mappings(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_export_logs_type ON export_logs(log_type);
CREATE INDEX IF NOT EXISTS idx_export_logs_timestamp ON export_logs(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_export_logs_status ON export_logs(status);
CREATE INDEX IF NOT EXISTS idx_export_logs_service ON export_logs(service_id);
CREATE INDEX IF NOT EXISTS idx_export_logs_target ON export_logs(target_id);
CREATE INDEX IF NOT EXISTS idx_export_logs_target_time ON export_logs(target_id, timestamp DESC);

-- ============================================================================
-- 8. export_schedules (예약 작업)
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

CREATE INDEX IF NOT EXISTS idx_export_schedules_enabled ON export_schedules(is_enabled);
CREATE INDEX IF NOT EXISTS idx_export_schedules_next_run ON export_schedules(next_run_at);

-- ============================================================================
-- 뷰 (View) - 통계 조회용
-- ============================================================================

-- 최근 24시간 통계
CREATE VIEW IF NOT EXISTS v_export_targets_stats_24h AS
SELECT 
    t.id,
    t.name,
    t.target_type,
    t.is_enabled,
    t.description,
    
    -- 최근 24시간 통계
    COALESCE(COUNT(l.id), 0) as total_exports_24h,
    COALESCE(SUM(CASE WHEN l.status = 'success' THEN 1 ELSE 0 END), 0) as successful_exports_24h,
    COALESCE(SUM(CASE WHEN l.status = 'failure' THEN 1 ELSE 0 END), 0) as failed_exports_24h,
    
    -- 성공률 (%)
    CASE 
        WHEN COUNT(l.id) > 0 THEN 
            ROUND((SUM(CASE WHEN l.status = 'success' THEN 1 ELSE 0 END) * 100.0) / COUNT(l.id), 2)
        ELSE 0 
    END as success_rate_24h,
    
    -- 평균 응답 시간
    ROUND(AVG(CASE WHEN l.status = 'success' THEN l.processing_time_ms END), 2) as avg_time_ms_24h,
    
    -- 마지막 정보
    MAX(CASE WHEN l.status = 'success' THEN l.timestamp END) as last_success_at,
    MAX(CASE WHEN l.status = 'failure' THEN l.timestamp END) as last_failure_at,
    
    t.export_mode,
    t.created_at,
    t.updated_at
    
FROM export_targets t
LEFT JOIN export_logs l ON t.id = l.target_id 
    AND l.timestamp > datetime('now', '-24 hours')
    AND l.log_type = 'export'
GROUP BY t.id;

-- 전체 누적 통계
CREATE VIEW IF NOT EXISTS v_export_targets_stats_all AS
SELECT 
    t.id,
    t.name,
    t.target_type,
    t.is_enabled,
    
    -- 전체 통계
    COALESCE(COUNT(l.id), 0) as total_exports,
    COALESCE(SUM(CASE WHEN l.status = 'success' THEN 1 ELSE 0 END), 0) as successful_exports,
    COALESCE(SUM(CASE WHEN l.status = 'failure' THEN 1 ELSE 0 END), 0) as failed_exports,
    
    -- 성공률
    CASE 
        WHEN COUNT(l.id) > 0 THEN 
            ROUND((SUM(CASE WHEN l.status = 'success' THEN 1 ELSE 0 END) * 100.0) / COUNT(l.id), 2)
        ELSE 0 
    END as success_rate_all,
    
    ROUND(AVG(CASE WHEN l.status = 'success' THEN l.processing_time_ms END), 2) as avg_time_ms_all,
    
    MIN(l.timestamp) as first_export_at,
    MAX(l.timestamp) as last_export_at,
    
    t.created_at
    
FROM export_targets t
LEFT JOIN export_logs l ON t.id = l.target_id 
    AND l.log_type = 'export'
GROUP BY t.id;

-- 프로파일 상세 정보
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

-- 프로토콜 서비스 상세
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

CREATE TRIGGER IF NOT EXISTS tr_export_targets_update
AFTER UPDATE ON export_targets
BEGIN
    UPDATE export_targets 
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
-- 초기 테스트 데이터 (개발환경용)
-- ============================================================================

INSERT OR IGNORE INTO export_profiles (name, description) VALUES 
    ('건물 1층 센서 데이터', '1층의 온도, 습도, CO2 센서 데이터'),
    ('공조기 실시간 데이터', 'AHU-01의 운전 상태 및 센서값');

-- ============================================================================
-- 9. value_conversion_rules (값 변환 규칙 - 상세)
-- ============================================================================

CREATE TABLE IF NOT EXISTS value_conversion_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    mapping_id INTEGER NOT NULL,
    
    -- 변환 규칙
    source_value VARCHAR(100) NOT NULL,      -- "open", "true", "25.5"
    target_value VARCHAR(100) NOT NULL,      -- "1", "ON", "77.9"
    
    -- 메타 정보
    description TEXT,                         -- "문 열림 → 1로 변환"
    priority INTEGER DEFAULT 0,               -- 매칭 우선순위 (높을수록 우선)
    
    -- 메타
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (mapping_id) REFERENCES export_target_mappings(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_value_rules_mapping ON value_conversion_rules(mapping_id);
CREATE INDEX IF NOT EXISTS idx_value_rules_source ON value_conversion_rules(mapping_id, source_value);

-- ============================================================================
-- 완료 메시지
-- ============================================================================

SELECT '✅ PulseOne Export System 스키마 생성 완료!' as message;
SELECT '📊 통계는 VIEW를 통해 조회하세요 (v_export_targets_stats_24h, v_export_targets_stats_all)' as note;
SELECT '🔄 값 변환은 value_conversion_rules 테이블을 사용하세요' as tip;