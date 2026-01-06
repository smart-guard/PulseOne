-- =============================================================================
-- Device Settings Test Data - Protocol-Specific Configurations
-- =============================================================================

-- Insert device_settings for each test device
INSERT INTO device_settings (
    device_id,
    -- Polling & Timing
    polling_interval_ms,
    scan_rate_override,
    -- Connection & Communication
    connection_timeout_ms,
    read_timeout_ms,
    write_timeout_ms,
    -- Retry Policy
    max_retry_count,
    retry_interval_ms,
    backoff_multiplier,
    backoff_time_ms,
    max_backoff_time_ms,
    -- Keep-alive
    keep_alive_enabled,
    keep_alive_interval_s,
    keep_alive_timeout_s,
    -- Data Quality
    data_validation_enabled,
    outlier_detection_enabled,
    deadband_enabled,
    -- Logging & Diagnostics
    detailed_logging_enabled,
    performance_monitoring_enabled,
    diagnostic_mode_enabled,
    -- Metadata
    created_at,
    updated_at
) VALUES

-- Device 1: PLC-001 (Modbus TCP) - Fast polling for PLC
(
    1,                    -- device_id
    1000,                 -- polling_interval_ms (1초)
    NULL,                 -- scan_rate_override
    5000,                 -- connection_timeout_ms
    3000,                 -- read_timeout_ms
    3000,                 -- write_timeout_ms
    3,                    -- max_retry_count
    2000,                 -- retry_interval_ms
    1.5,                  -- backoff_multiplier
    30000,                -- backoff_time_ms (30초)
    180000,               -- max_backoff_time_ms (3분)
    1,                    -- keep_alive_enabled
    30,                   -- keep_alive_interval_s
    10,                   -- keep_alive_timeout_s
    1,                    -- data_validation_enabled
    1,                    -- outlier_detection_enabled
    1,                    -- deadband_enabled
    0,                    -- detailed_logging_enabled
    1,                    -- performance_monitoring_enabled
    0,                    -- diagnostic_mode_enabled
    datetime('now'),
    datetime('now')
),

-- Device 2: RTU-TEMP-001 (Modbus RTU) - Slower polling for serial
(
    2,                    -- device_id
    5000,                 -- polling_interval_ms (5초)
    NULL,                 -- scan_rate_override
    10000,                -- connection_timeout_ms (longer for serial)
    5000,                 -- read_timeout_ms
    5000,                 -- write_timeout_ms
    5,                    -- max_retry_count (more retries for serial)
    5000,                 -- retry_interval_ms
    2.0,                  -- backoff_multiplier
    60000,                -- backoff_time_ms (1분)
    300000,               -- max_backoff_time_ms (5분)
    1,                    -- keep_alive_enabled
    60,                   -- keep_alive_interval_s (longer interval)
    15,                   -- keep_alive_timeout_s
    1,                    -- data_validation_enabled
    1,                    -- outlier_detection_enabled
    1,                    -- deadband_enabled
    1,                    -- detailed_logging_enabled (enable for debugging)
    1,                    -- performance_monitoring_enabled
    0,                    -- diagnostic_mode_enabled
    datetime('now'),
    datetime('now')
),

-- Device 3: MQTT-GATEWAY-001 (MQTT) - Moderate polling for IoT
(
    3,                    -- device_id
    10000,                -- polling_interval_ms (10초)
    NULL,                 -- scan_rate_override
    30000,                -- connection_timeout_ms (longer for MQTT)
    30000,                -- read_timeout_ms
    30000,                -- write_timeout_ms
    3,                    -- max_retry_count
    10000,                -- retry_interval_ms
    1.5,                  -- backoff_multiplier
    60000,                -- backoff_time_ms (1분)
    300000,               -- max_backoff_time_ms (5분)
    1,                    -- keep_alive_enabled
    60,                   -- keep_alive_interval_s (MQTT standard)
    20,                   -- keep_alive_timeout_s
    1,                    -- data_validation_enabled
    0,                    -- outlier_detection_enabled (less critical for IoT)
    1,                    -- deadband_enabled
    0,                    -- detailed_logging_enabled
    1,                    -- performance_monitoring_enabled
    0,                    -- diagnostic_mode_enabled
    datetime('now'),
    datetime('now')
);

-- Verification
SELECT 
    d.name as device_name,
    ds.polling_interval_ms,
    ds.connection_timeout_ms,
    ds.max_retry_count,
    ds.keep_alive_enabled
FROM devices d
JOIN device_settings ds ON d.id = ds.device_id
ORDER BY d.id;
