-- =============================================================================
-- PulseOne 스키마 업데이트 - device_settings 테이블 생성
-- =============================================================================

-- 1. 디바이스 설정 전용 테이블 생성
CREATE TABLE IF NOT EXISTS device_settings (
    device_id INTEGER PRIMARY KEY,
    polling_interval_ms INTEGER DEFAULT 1000,
    connection_timeout_ms INTEGER DEFAULT 10000,
    max_retry_count INTEGER DEFAULT 3,
    retry_interval_ms INTEGER DEFAULT 5000,
    backoff_time_ms INTEGER DEFAULT 60000,
    keep_alive_enabled INTEGER DEFAULT 1,
    keep_alive_interval_s INTEGER DEFAULT 30,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- 2. current_values 테이블에 updated_at 컬럼 추가
ALTER TABLE current_values ADD COLUMN updated_at DATETIME DEFAULT CURRENT_TIMESTAMP;

-- 3. 기존 디바이스들의 기본 설정 생성
INSERT INTO device_settings (device_id)
SELECT id FROM devices WHERE id NOT IN (SELECT device_id FROM device_settings);

-- 4. 기존 current_values에 updated_at 값 설정
UPDATE current_values SET updated_at = timestamp WHERE updated_at IS NULL;

-- 5. 인덱스 생성
CREATE INDEX IF NOT EXISTS idx_current_values_updated_at ON current_values(updated_at);
CREATE INDEX IF NOT EXISTS idx_device_settings_device_id ON device_settings(device_id);
