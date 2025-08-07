-- =============================================================================
-- DB Migration Script - 기존 SQLite DB를 새 스키마로 업데이트
-- collector/data/pulseone_test.db 업데이트용
-- =============================================================================

-- 1단계: 기존 데이터 백업 (선택적)
-- =============================================================================
CREATE TABLE IF NOT EXISTS data_points_backup AS SELECT * FROM data_points;
CREATE TABLE IF NOT EXISTS current_values_backup AS SELECT * FROM current_values;

-- 2단계: 기존 테이블 삭제
-- =============================================================================
DROP TABLE IF EXISTS current_values;
DROP TABLE IF EXISTS data_points;

-- 프로토콜별 테이블들도 있다면 삭제 (혹시 있을 수 있으니)
DROP TABLE IF EXISTS modbus_point_config;
DROP TABLE IF EXISTS mqtt_point_config;
DROP TABLE IF EXISTS bacnet_point_config;

-- 3단계: 새로운 스키마로 테이블 재생성
-- =============================================================================

-- 🔥🔥🔥 새 data_points 테이블 (Struct DataPoint 완전 호환)
CREATE TABLE IF NOT EXISTS data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    
    -- 🔥 기본 식별 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 🔥 주소 정보 (Struct DataPoint와 일치)
    address INTEGER NOT NULL,                    -- uint32_t address
    address_string VARCHAR(255),                 -- std::string address_string
    
    -- 🔥 데이터 타입 및 접근성 (Struct DataPoint와 일치)
    data_type VARCHAR(20) NOT NULL DEFAULT 'UNKNOWN',  -- std::string data_type
    access_mode VARCHAR(10) DEFAULT 'read',             -- std::string access_mode
    is_enabled INTEGER DEFAULT 1,                       -- bool is_enabled
    is_writable INTEGER DEFAULT 0,                      -- bool is_writable
    
    -- 🔥 엔지니어링 단위 및 스케일링 (Struct DataPoint와 일치)
    unit VARCHAR(50),                            -- std::string unit
    scaling_factor REAL DEFAULT 1.0,            -- double scaling_factor
    scaling_offset REAL DEFAULT 0.0,            -- double scaling_offset
    min_value REAL DEFAULT 0.0,                 -- double min_value
    max_value REAL DEFAULT 0.0,                 -- double max_value
    
    -- 🔥🔥🔥 로깅 및 수집 설정 (중요! 이전에 없던 컬럼들)
    log_enabled INTEGER DEFAULT 1,              -- bool log_enabled ✅
    log_interval_ms INTEGER DEFAULT 0,          -- uint32_t log_interval_ms ✅
    log_deadband REAL DEFAULT 0.0,              -- double log_deadband ✅
    polling_interval_ms INTEGER DEFAULT 0,      -- uint32_t polling_interval_ms
    
    -- 🔥🔥🔥 메타데이터 (중요! 이전에 없던 컬럼들)
    group_name VARCHAR(50),                      -- std::string group
    tags TEXT,                                   -- std::string tags (JSON 배열) ✅
    metadata TEXT,                               -- std::string metadata (JSON 객체) ✅
    protocol_params TEXT,                        -- map<string,string> protocol_params (JSON)
    
    -- 🔥 시간 정보
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    UNIQUE(device_id, address)
);

-- 🔥🔥🔥 새 current_values 테이블 (JSON 기반 간소화)
CREATE TABLE IF NOT EXISTS current_values (
    point_id INTEGER PRIMARY KEY,
    
    -- 🔥 실제 값 (JSON으로 DataVariant 저장)
    current_value TEXT,                          -- JSON: {"value": 123.45}
    raw_value TEXT,                              -- JSON: {"value": 12345} (스케일링 전)
    value_type VARCHAR(10) DEFAULT 'double',     -- bool, int16, uint16, int32, uint32, float, double, string
    
    -- 🔥 데이터 품질 및 타임스탬프
    quality_code INTEGER DEFAULT 0,             -- DataQuality enum 값
    quality VARCHAR(20) DEFAULT 'not_connected', -- 텍스트 표현
    
    -- 🔥 타임스탬프들
    value_timestamp DATETIME,                   -- 값 변경 시간
    quality_timestamp DATETIME,                 -- 품질 변경 시간  
    last_log_time DATETIME,                     -- 마지막 로깅 시간
    last_read_time DATETIME,                    -- 마지막 읽기 시간
    last_write_time DATETIME,                   -- 마지막 쓰기 시간
    
    -- 🔥 통계 카운터들
    read_count INTEGER DEFAULT 0,               -- 읽기 횟수
    write_count INTEGER DEFAULT 0,              -- 쓰기 횟수
    error_count INTEGER DEFAULT 0,              -- 에러 횟수
    
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE
);

-- 4단계: 인덱스 생성
-- =============================================================================
CREATE INDEX IF NOT EXISTS idx_data_points_device ON data_points(device_id);
CREATE INDEX IF NOT EXISTS idx_data_points_enabled ON data_points(is_enabled);
CREATE INDEX IF NOT EXISTS idx_data_points_address ON data_points(device_id, address);
CREATE INDEX IF NOT EXISTS idx_data_points_type ON data_points(data_type);
CREATE INDEX IF NOT EXISTS idx_data_points_log_enabled ON data_points(log_enabled);

CREATE INDEX IF NOT EXISTS idx_current_values_timestamp ON current_values(value_timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_current_values_quality ON current_values(quality_code);
CREATE INDEX IF NOT EXISTS idx_current_values_updated ON current_values(updated_at DESC);

-- 5단계: 새로운 샘플 데이터 삽입
-- =============================================================================

-- 🔥🔥🔥 기존 devices 테이블 활용해서 새 data_points 생성
-- (기존 devices가 있다고 가정: PLC-001, HMI-001 등)

INSERT OR IGNORE INTO data_points (
    device_id, name, description, 
    address, address_string,
    data_type, access_mode, is_enabled, is_writable,
    unit, scaling_factor, scaling_offset, min_value, max_value,
    log_enabled, log_interval_ms, log_deadband, polling_interval_ms,
    group_name, tags, metadata, protocol_params
) VALUES 

-- Device ID 1 (첫 번째 디바이스)의 데이터 포인트들
(1, 'Production_Count', 'Production counter', 
 1001, '40001', 
 'uint32', 'read', 1, 0,
 'pcs', 1.0, 0.0, 0.0, 999999.0,
 1, 5000, 1.0, 1000,
 'production', '["production", "counter"]', 
 '{"location": "line_1", "criticality": "high"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 1, "byte_order": "big_endian"}'),

(1, 'Line_Speed', 'Line speed sensor', 
 1002, '40002', 
 'float', 'read', 1, 0,
 'm/min', 0.1, 0.0, 0.0, 100.0,
 1, 2000, 0.5, 1000,
 'production', '["speed", "monitoring"]', 
 '{"location": "line_1", "criticality": "medium"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 1, "byte_order": "big_endian"}'),

(1, 'Motor_Current', 'Motor current sensor', 
 1003, '40003', 
 'float', 'read', 1, 0,
 'A', 0.01, 0.0, 0.0, 50.0,
 1, 1000, 0.1, 500,
 'electrical', '["current", "motor"]', 
 '{"location": "motor_1", "criticality": "high"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 1, "byte_order": "big_endian"}'),

(1, 'Temperature', 'Process temperature', 
 1004, '40004', 
 'float', 'read', 1, 0,
 '°C', 0.1, -273.15, -40.0, 150.0,
 1, 3000, 0.2, 1000,
 'environmental', '["temperature", "process"]', 
 '{"location": "process_area", "criticality": "medium"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 1, "byte_order": "big_endian"}'),

(1, 'Emergency_Stop', 'Emergency stop button', 
 1005, '10001', 
 'bool', 'read', 1, 0,
 '', 1.0, 0.0, 0.0, 1.0,
 1, 500, 0.0, 100,
 'safety', '["emergency", "safety"]', 
 '{"location": "control_panel", "criticality": "critical"}',
 '{"protocol": "MODBUS_TCP", "function_code": 1, "slave_id": 1, "byte_order": "big_endian"}'),

-- Device ID 2 (두 번째 디바이스)의 데이터 포인트들  
(2, 'HMI_Status', 'HMI screen status', 
 2001, '40001', 
 'uint16', 'read', 1, 0,
 '', 1.0, 0.0, 0.0, 255.0,
 0, 10000, 1.0, 2000,
 'hmi', '["hmi", "status"]', 
 '{"screen_id": "main"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 2, "byte_order": "big_endian"}'),

(2, 'Alarm_Count', 'Active alarm count', 
 2002, '40002', 
 'uint16', 'read', 1, 0,
 'count', 1.0, 0.0, 0.0, 100.0,
 1, 5000, 1.0, 1000,
 'alarms', '["alarms", "monitoring"]', 
 '{"criticality": "medium"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 2, "byte_order": "big_endian"}'),

-- Device ID 3 (세 번째 디바이스)의 데이터 포인트들
(3, 'Pump_Pressure', 'Hydraulic pump pressure', 
 3001, '40001', 
 'float', 'read', 1, 0,
 'bar', 0.01, 0.0, 0.0, 250.0,
 1, 1000, 1.0, 500,
 'hydraulic', '["pressure", "pump"]', 
 '{"pump_id": "pump_1", "safety_limit": 200}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 3, "byte_order": "big_endian"}'),

(3, 'Flow_Rate', 'Hydraulic flow rate', 
 3002, '40002', 
 'float', 'read', 1, 0,
 'L/min', 0.1, 0.0, 0.0, 100.0,
 1, 2000, 0.5, 1000,
 'hydraulic', '["flow", "hydraulic"]', 
 '{"pump_id": "pump_1", "criticality": "medium"}',
 '{"protocol": "MODBUS_TCP", "function_code": 3, "slave_id": 3, "byte_order": "big_endian"}'),

-- Device ID 4 (네 번째 디바이스 - MQTT 예시)
(4, 'IoT_Temperature', 'IoT temperature sensor', 
 0, 'factory/zone1/temperature', 
 'float', 'read', 1, 0,
 '°C', 1.0, 0.0, -20.0, 80.0,
 1, 5000, 0.3, 0,
 'iot_sensors', '["iot", "temperature", "wireless"]', 
 '{"sensor_id": "TEMP001", "battery": 85}',
 '{"protocol": "MQTT", "topic": "factory/zone1/temperature", "qos": 1, "json_path": "$.value"}'),

(4, 'IoT_Humidity', 'IoT humidity sensor', 
 0, 'factory/zone1/humidity', 
 'float', 'read', 1, 0,
 '%RH', 1.0, 0.0, 0.0, 100.0,
 1, 5000, 1.0, 0,
 'iot_sensors', '["iot", "humidity", "wireless"]', 
 '{"sensor_id": "HUM001", "battery": 92}',
 '{"protocol": "MQTT", "topic": "factory/zone1/humidity", "qos": 1, "json_path": "$.value"}'),

-- Device ID 5 (다섯 번째 디바이스 - BACnet 예시)
(5, 'HVAC_Temperature', 'HVAC zone temperature', 
 1, 'AI:1', 
 'float', 'read', 1, 0,
 '°C', 1.0, 0.0, 10.0, 40.0,
 1, 10000, 0.5, 5000,
 'hvac', '["hvac", "temperature"]', 
 '{"zone_id": "zone_1", "comfort_range": "20-26"}',
 '{"protocol": "BACNET_IP", "object_type": 0, "object_instance": 1, "property_id": 85}');

-- 6단계: 현재값 초기화 (JSON 방식)
-- =============================================================================
INSERT OR IGNORE INTO current_values (
    point_id, current_value, value_type, raw_value, 
    quality_code, quality, value_timestamp, last_read_time,
    read_count, write_count, error_count
) VALUES 
-- Production_Count 현재값
(1, '{"value": 1234}', 'uint32', '{"value": 1234}', 1, 'good', datetime('now'), datetime('now'), 125, 0, 0),
-- Line_Speed 현재값
(2, '{"value": 15.5}', 'float', '{"value": 155}', 1, 'good', datetime('now'), datetime('now'), 234, 0, 0),
-- Motor_Current 현재값
(3, '{"value": 12.34}', 'float', '{"value": 1234}', 1, 'good', datetime('now'), datetime('now'), 189, 0, 0),
-- Temperature 현재값
(4, '{"value": 23.5}', 'float', '{"value": 235}', 1, 'good', datetime('now'), datetime('now'), 156, 0, 0),
-- Emergency_Stop 현재값
(5, '{"value": false}', 'bool', '{"value": 0}', 1, 'good', datetime('now'), datetime('now'), 345, 0, 0),
-- HMI_Status 현재값
(6, '{"value": 1}', 'uint16', '{"value": 1}', 1, 'good', datetime('now'), datetime('now'), 45, 0, 0),
-- Alarm_Count 현재값
(7, '{"value": 3}', 'uint16', '{"value": 3}', 1, 'good', datetime('now'), datetime('now'), 78, 0, 0),
-- Pump_Pressure 현재값
(8, '{"value": 125.67}', 'float', '{"value": 12567}', 1, 'good', datetime('now'), datetime('now'), 456, 0, 0),
-- Flow_Rate 현재값
(9, '{"value": 25.8}', 'float', '{"value": 258}', 1, 'good', datetime('now'), datetime('now'), 234, 0, 0),
-- IoT_Temperature 현재값
(10, '{"value": 24.8}', 'float', '{"value": 24.8}', 1, 'good', datetime('now'), datetime('now'), 345, 0, 0),
-- IoT_Humidity 현재값
(11, '{"value": 58.2}', 'float', '{"value": 58.2}', 1, 'good', datetime('now'), datetime('now'), 298, 0, 0),
-- HVAC_Temperature 현재값
(12, '{"value": 22.3}', 'float', '{"value": 22.3}', 1, 'good', datetime('now'), datetime('now'), 89, 0, 0);

-- 7단계: 스키마 버전 업데이트
-- =============================================================================
INSERT OR REPLACE INTO schema_versions (version, description) VALUES 
('2.1.0', 'Updated DataPoint schema with JSON protocol params');

-- 완료 메시지
SELECT '🎉 Database migration completed successfully!' as result;
SELECT 'DataPoint count: ' || COUNT(*) as data_points_created FROM data_points;
SELECT 'CurrentValue count: ' || COUNT(*) as current_values_created FROM current_values;