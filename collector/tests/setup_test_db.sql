-- =============================================================================
-- collector/tests/setup_test_db.sql
-- PulseOne Collector 테스트용 SQLite 데이터베이스 스키마 + 테스트 데이터
-- =============================================================================

-- 1. 스키마 버전 관리 테이블
CREATE TABLE IF NOT EXISTS schema_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    version VARCHAR(20) NOT NULL,
    applied_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    description TEXT
);

-- 2. 테넌트(회사) 테이블
CREATE TABLE IF NOT EXISTS tenants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    company_name VARCHAR(100) NOT NULL UNIQUE,
    company_code VARCHAR(20) NOT NULL UNIQUE,
    domain VARCHAR(100) UNIQUE,
    
    -- 연락처 정보
    contact_name VARCHAR(100),
    contact_email VARCHAR(100),
    contact_phone VARCHAR(20),
    
    -- 구독 정보
    subscription_plan VARCHAR(20) DEFAULT 'starter',
    subscription_status VARCHAR(20) DEFAULT 'active',
    max_edge_servers INTEGER DEFAULT 3,
    max_data_points INTEGER DEFAULT 1000,
    max_users INTEGER DEFAULT 5,
    
    -- 상태 정보
    is_active INTEGER DEFAULT 1,
    trial_end_date DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 3. 사이트 테이블
CREATE TABLE IF NOT EXISTS sites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    location VARCHAR(200),
    description TEXT,
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);

-- 4. Edge 서버 등록 테이블
CREATE TABLE IF NOT EXISTS edge_servers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    
    -- 서버 식별 정보
    server_name VARCHAR(100) NOT NULL,
    factory_name VARCHAR(100),
    location VARCHAR(200),
    
    -- 네트워크 정보
    ip_address VARCHAR(45),
    mac_address VARCHAR(17),
    hostname VARCHAR(100),
    
    -- 등록 정보
    registration_token VARCHAR(255) UNIQUE,
    activation_code VARCHAR(50),
    
    -- 상태 정보
    status VARCHAR(20) DEFAULT 'pending',
    last_seen DATETIME,
    version VARCHAR(20),
    
    -- 설정 정보
    config TEXT,
    sync_enabled INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE
);

-- 5. 디바이스 그룹 테이블
CREATE TABLE IF NOT EXISTS device_groups (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    parent_group_id INTEGER,
    group_type VARCHAR(50) DEFAULT 'functional', -- functional, physical, protocol
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (parent_group_id) REFERENCES device_groups(id) ON DELETE SET NULL
);

-- 6. 드라이버 플러그인 테이블
CREATE TABLE IF NOT EXISTS driver_plugins (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL,
    protocol_type VARCHAR(50) NOT NULL,
    version VARCHAR(20) NOT NULL,
    file_path VARCHAR(255) NOT NULL,
    description TEXT,
    
    -- 플러그인 정보
    author VARCHAR(100),
    api_version INTEGER DEFAULT 1,
    is_enabled INTEGER DEFAULT 1,
    config_schema TEXT, -- JSON 스키마
    
    -- 라이선스 정보
    license_type VARCHAR(20) DEFAULT 'free',
    license_expires DATETIME,
    
    installed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    UNIQUE(protocol_type, version)
);

-- 7. 디바이스 테이블 (업데이트됨)
CREATE TABLE IF NOT EXISTS devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER NOT NULL,
    device_group_id INTEGER,
    edge_server_id INTEGER,
    
    -- 디바이스 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    device_type VARCHAR(50) NOT NULL, -- PLC, HMI, SENSOR, GATEWAY, METER, CONTROLLER
    manufacturer VARCHAR(100),
    model VARCHAR(100),
    serial_number VARCHAR(100),
    
    -- 프로토콜 설정
    protocol_type VARCHAR(50) NOT NULL, -- MODBUS_TCP, MODBUS_RTU, MQTT, BACNET, OPCUA
    endpoint VARCHAR(255) NOT NULL, -- IP:Port 또는 연결 문자열
    config TEXT NOT NULL, -- JSON 형태 프로토콜별 설정
    
    -- 수집 설정 (기본값만, 세부 설정은 device_settings 테이블 참조)
    polling_interval INTEGER DEFAULT 1000,
    timeout INTEGER DEFAULT 3000,
    retry_count INTEGER DEFAULT 3,
    
    -- 상태 정보
    is_enabled INTEGER DEFAULT 1,
    installation_date DATE,
    last_maintenance DATE,
    
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
    FOREIGN KEY (device_group_id) REFERENCES device_groups(id) ON DELETE SET NULL,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL
);

-- 8. 디바이스 세부 설정 테이블
CREATE TABLE IF NOT EXISTS device_settings (
    device_id INTEGER PRIMARY KEY,
    
    -- 폴링 및 타이밍 설정
    polling_interval_ms INTEGER DEFAULT 1000,
    scan_rate_override INTEGER, -- 개별 디바이스 스캔 주기 오버라이드
    
    -- 연결 및 통신 설정
    connection_timeout_ms INTEGER DEFAULT 10000,
    read_timeout_ms INTEGER DEFAULT 5000,
    write_timeout_ms INTEGER DEFAULT 5000,
    
    -- 재시도 정책
    max_retry_count INTEGER DEFAULT 3,
    retry_interval_ms INTEGER DEFAULT 5000,
    backoff_multiplier DECIMAL(3,2) DEFAULT 1.5, -- 지수 백오프
    backoff_time_ms INTEGER DEFAULT 60000,
    max_backoff_time_ms INTEGER DEFAULT 300000, -- 최대 5분
    
    -- Keep-alive 설정
    keep_alive_enabled INTEGER DEFAULT 1,
    keep_alive_interval_s INTEGER DEFAULT 30,
    keep_alive_timeout_s INTEGER DEFAULT 10,
    
    -- 데이터 품질 관리
    data_validation_enabled INTEGER DEFAULT 1,
    outlier_detection_enabled INTEGER DEFAULT 0,
    deadband_enabled INTEGER DEFAULT 1,
    
    -- 로깅 및 진단
    detailed_logging_enabled INTEGER DEFAULT 0,
    performance_monitoring_enabled INTEGER DEFAULT 1,
    diagnostic_mode_enabled INTEGER DEFAULT 0,
    
    -- 메타데이터
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_by INTEGER,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- 9. 디바이스 상태 테이블
CREATE TABLE IF NOT EXISTS device_status (
    device_id INTEGER PRIMARY KEY,
    connection_status VARCHAR(20) NOT NULL DEFAULT 'disconnected',
    last_communication DATETIME,
    error_count INTEGER DEFAULT 0,
    last_error TEXT,
    response_time INTEGER,
    
    -- 추가 진단 정보
    firmware_version VARCHAR(50),
    hardware_info TEXT, -- JSON 형태
    diagnostic_data TEXT, -- JSON 형태
    
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- 10. 데이터 포인트 테이블
CREATE TABLE IF NOT EXISTS data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    
    -- 포인트 기본 정보
    name VARCHAR(100) NOT NULL,
    description TEXT,
    address INTEGER NOT NULL,
    data_type VARCHAR(20) NOT NULL, -- bool, int16, int32, uint16, uint32, float, double, string
    access_mode VARCHAR(10) DEFAULT 'read', -- read, write, readwrite
    
    -- 엔지니어링 정보
    unit VARCHAR(20),
    scaling_factor DECIMAL(10,4) DEFAULT 1.0,
    scaling_offset DECIMAL(10,4) DEFAULT 0.0,
    min_value DECIMAL(15,4),
    max_value DECIMAL(15,4),
    
    -- 수집 설정
    is_enabled INTEGER DEFAULT 1,
    scan_rate INTEGER, -- 개별 스캔 주기 (디바이스 기본값 오버라이드)
    deadband DECIMAL(10,4) DEFAULT 0,
    
    -- 메타데이터
    config TEXT, -- JSON 형태
    tags TEXT, -- JSON 배열 형태
    
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    UNIQUE(device_id, address)
);

-- 11. 현재값 테이블
CREATE TABLE IF NOT EXISTS current_values (
    point_id INTEGER PRIMARY KEY,
    value DECIMAL(15,4),
    raw_value DECIMAL(15,4),
    string_value TEXT,
    quality VARCHAR(20) DEFAULT 'good', -- good, bad, uncertain, not_connected
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE
);

-- =============================================================================
-- 인덱스 생성
-- =============================================================================

-- 기본 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_tenants_code ON tenants(company_code);
CREATE INDEX IF NOT EXISTS idx_sites_tenant ON sites(tenant_id);
CREATE INDEX IF NOT EXISTS idx_edge_servers_tenant ON edge_servers(tenant_id);
CREATE INDEX IF NOT EXISTS idx_device_groups_tenant_site ON device_groups(tenant_id, site_id);

-- 디바이스 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_devices_tenant ON devices(tenant_id);
CREATE INDEX IF NOT EXISTS idx_devices_site ON devices(site_id);
CREATE INDEX IF NOT EXISTS idx_devices_group ON devices(device_group_id);
CREATE INDEX IF NOT EXISTS idx_devices_edge_server ON devices(edge_server_id);
CREATE INDEX IF NOT EXISTS idx_devices_protocol ON devices(protocol_type);
CREATE INDEX IF NOT EXISTS idx_devices_type ON devices(device_type);
CREATE INDEX IF NOT EXISTS idx_devices_enabled ON devices(is_enabled);

-- 데이터 포인트 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_data_points_device ON data_points(device_id);
CREATE INDEX IF NOT EXISTS idx_data_points_enabled ON data_points(is_enabled);
CREATE INDEX IF NOT EXISTS idx_data_points_type ON data_points(data_type);
CREATE INDEX IF NOT EXISTS idx_data_points_address ON data_points(device_id, address);

-- 현재값 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_current_values_timestamp ON current_values(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_current_values_updated_at ON current_values(updated_at);

-- 설정 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_device_settings_device_id ON device_settings(device_id);
CREATE INDEX IF NOT EXISTS idx_device_settings_polling ON device_settings(polling_interval_ms);

-- =============================================================================
-- 테스트 데이터 삽입
-- =============================================================================

-- 스키마 버전 등록
INSERT OR REPLACE INTO schema_versions (version, description) VALUES 
    ('1.0.0', 'Initial schema with test data');

-- 1. 테넌트 데이터 (2개 회사)
INSERT OR REPLACE INTO tenants (id, company_name, company_code, domain, contact_name, contact_email) VALUES
    (1, 'Smart Factory Inc', 'SF001', 'smartfactory.com', 'John Manager', 'john@smartfactory.com'),
    (2, 'Industrial IoT Co', 'IIC002', 'industrial-iot.com', 'Jane Director', 'jane@industrial-iot.com');

-- 2. 사이트 데이터 (3개 사이트)
INSERT OR REPLACE INTO sites (id, tenant_id, name, location, description) VALUES
    (1, 1, 'Seoul Main Factory', 'Seoul, South Korea', 'Primary manufacturing facility'),
    (2, 1, 'Busan Branch', 'Busan, South Korea', 'Secondary facility'),
    (3, 2, 'Incheon Plant', 'Incheon, South Korea', 'Industrial IoT test facility');

-- 3. Edge 서버 데이터 (2개)
INSERT OR REPLACE INTO edge_servers (id, tenant_id, server_name, factory_name, location, ip_address, status, version) VALUES
    (1, 1, 'EdgeServer-SF-001', 'Seoul Main Factory', 'Floor 1, Building A', '192.168.1.100', 'active', '1.0.0'),
    (2, 2, 'EdgeServer-IIC-001', 'Incheon Plant', 'Control Room', '192.168.2.100', 'active', '1.0.0');

-- 4. 디바이스 그룹 데이터 (3개)
INSERT OR REPLACE INTO device_groups (id, tenant_id, site_id, name, description, group_type) VALUES
    (1, 1, 1, 'Production Line A', 'Main production line equipment', 'functional'),
    (2, 1, 1, 'HVAC System', 'Building automation and HVAC', 'functional'),
    (3, 2, 3, 'Energy Monitoring', 'Power and energy measurement devices', 'functional');

-- 5. 드라이버 플러그인 데이터
INSERT OR REPLACE INTO driver_plugins (id, name, protocol_type, version, file_path, description, author, is_enabled) VALUES
    (1, 'Modbus TCP Driver', 'MODBUS_TCP', '1.0.0', '/usr/lib/pulseone/modbus_tcp.so', 'Standard Modbus TCP driver', 'PulseOne Team', 1),
    (2, 'MQTT Client Driver', 'MQTT', '1.0.0', '/usr/lib/pulseone/mqtt_client.so', 'MQTT client driver for IoT devices', 'PulseOne Team', 1),
    (3, 'BACnet Driver', 'BACNET', '1.0.0', '/usr/lib/pulseone/bacnet.so', 'BACnet protocol driver', 'PulseOne Team', 1);

-- 6. 디바이스 데이터 (5개 - 다양한 프로토콜)
INSERT OR REPLACE INTO devices (id, tenant_id, site_id, device_group_id, edge_server_id, name, description, device_type, manufacturer, model, protocol_type, endpoint, config, polling_interval, is_enabled) VALUES
    (1, 1, 1, 1, 1, 'PLC-001', 'Main Production PLC', 'PLC', 'Siemens', 'S7-1200', 'MODBUS_TCP', '192.168.1.10:502', '{"slave_id": 1, "timeout": 3000}', 1000, 1),
    (2, 1, 1, 1, 1, 'HMI-001', 'Operator Interface Panel', 'HMI', 'Schneider', 'Magelis', 'MODBUS_TCP', '192.168.1.11:502', '{"slave_id": 2, "timeout": 3000}', 2000, 1),
    (3, 1, 1, 2, 1, 'HVAC-CTRL-001', 'Main HVAC Controller', 'CONTROLLER', 'Honeywell', 'T7350', 'BACNET', '192.168.1.20', '{"device_id": 20, "network": 1}', 5000, 1),
    (4, 2, 3, 3, 2, 'METER-POWER-001', 'Main Power Meter', 'METER', 'Schneider', 'PM8000', 'MODBUS_TCP', '192.168.2.30:502', '{"slave_id": 10, "timeout": 5000}', 3000, 1),
    (5, 2, 3, 3, 2, 'SENSOR-TEMP-001', 'Temperature Sensor Array', 'SENSOR', 'Generic IoT', 'TempMon-100', 'MQTT', 'mqtt://192.168.2.50:1883', '{"topic": "sensors/temperature", "qos": 1}', 10000, 1);

-- 7. 디바이스 세부 설정 데이터
INSERT OR REPLACE INTO device_settings (device_id, polling_interval_ms, connection_timeout_ms, max_retry_count, keep_alive_enabled) VALUES
    (1, 1000, 5000, 3, 1),
    (2, 2000, 5000, 3, 1),
    (3, 5000, 10000, 2, 1),
    (4, 3000, 8000, 3, 1),
    (5, 10000, 15000, 2, 0); -- MQTT는 keep-alive 별도 설정

-- 8. 디바이스 상태 데이터
INSERT OR REPLACE INTO device_status (device_id, connection_status, last_communication, error_count, response_time) VALUES
    (1, 'connected', datetime('now', '-30 seconds'), 0, 50),
    (2, 'connected', datetime('now', '-45 seconds'), 1, 80),
    (3, 'connected', datetime('now', '-2 minutes'), 0, 120),
    (4, 'connected', datetime('now', '-1 minute'), 0, 65),
    (5, 'disconnected', datetime('now', '-10 minutes'), 3, NULL);

-- 9. 데이터 포인트 데이터 (16개 - 각 디바이스당 2-4개)
INSERT OR REPLACE INTO data_points (id, device_id, name, description, address, data_type, unit, scaling_factor, is_enabled) VALUES
    -- PLC-001 데이터 포인트 (4개)
    (1, 1, 'Production_Count', 'Total production count', 40001, 'uint32', 'pieces', 1.0, 1),
    (2, 1, 'Line_Speed', 'Production line speed', 40003, 'uint16', 'ppm', 0.1, 1),
    (3, 1, 'Motor_Status', 'Main motor running status', 10001, 'bool', '', 1.0, 1),
    (4, 1, 'Temperature', 'Process temperature', 30001, 'int16', '°C', 0.1, 1),
    
    -- HMI-001 데이터 포인트 (3개)
    (5, 2, 'Alarm_Count', 'Active alarm count', 40010, 'uint16', 'count', 1.0, 1),
    (6, 2, 'Operator_Present', 'Operator presence sensor', 10010, 'bool', '', 1.0, 1),
    (7, 2, 'Screen_Brightness', 'Display brightness level', 40012, 'uint16', '%', 1.0, 1),
    
    -- HVAC-CTRL-001 데이터 포인트 (3개)
    (8, 3, 'Room_Temperature', 'Current room temperature', 1, 'float', '°C', 1.0, 1),
    (9, 3, 'Setpoint_Temperature', 'Temperature setpoint', 2, 'float', '°C', 1.0, 1),
    (10, 3, 'Fan_Speed', 'HVAC fan speed', 3, 'uint16', 'rpm', 1.0, 1),
    
    -- METER-POWER-001 데이터 포인트 (3개)
    (11, 4, 'Voltage_L1', 'Phase 1 voltage', 40100, 'uint16', 'V', 0.1, 1),
    (12, 4, 'Current_L1', 'Phase 1 current', 40102, 'uint16', 'A', 0.01, 1),
    (13, 4, 'Power_Total', 'Total active power', 40104, 'uint32', 'kW', 0.001, 1),
    
    -- SENSOR-TEMP-001 데이터 포인트 (3개)
    (14, 5, 'Temp_Zone_1', 'Temperature zone 1', 1, 'float', '°C', 1.0, 1),
    (15, 5, 'Temp_Zone_2', 'Temperature zone 2', 2, 'float', '°C', 1.0, 1),
    (16, 5, 'Humidity', 'Relative humidity', 3, 'float', '%', 1.0, 1);

-- 10. 현재값 데이터 (시뮬레이션된 실시간 값들)
INSERT OR REPLACE INTO current_values (point_id, value, raw_value, quality, timestamp) VALUES
    -- PLC-001 현재값
    (1, 12543, 12543, 'good', datetime('now', '-10 seconds')),
    (2, 85.5, 855, 'good', datetime('now', '-10 seconds')),
    (3, 1, 1, 'good', datetime('now', '-10 seconds')),
    (4, 23.5, 235, 'good', datetime('now', '-10 seconds')),
    
    -- HMI-001 현재값
    (5, 2, 2, 'good', datetime('now', '-15 seconds')),
    (6, 1, 1, 'good', datetime('now', '-15 seconds')),
    (7, 75, 75, 'good', datetime('now', '-15 seconds')),
    
    -- HVAC-CTRL-001 현재값
    (8, 22.5, 22.5, 'good', datetime('now', '-30 seconds')),
    (9, 23.0, 23.0, 'good', datetime('now', '-30 seconds')),
    (10, 1200, 1200, 'good', datetime('now', '-30 seconds')),
    
    -- METER-POWER-001 현재값
    (11, 220.5, 2205, 'good', datetime('now', '-20 seconds')),
    (12, 15.25, 1525, 'good', datetime('now', '-20 seconds')),
    (13, 3.365, 3365, 'good', datetime('now', '-20 seconds')),
    
    -- SENSOR-TEMP-001 현재값 (연결 안됨 상태)
    (14, NULL, NULL, 'not_connected', datetime('now', '-10 minutes')),
    (15, NULL, NULL, 'not_connected', datetime('now', '-10 minutes')),
    (16, NULL, NULL, 'not_connected', datetime('now', '-10 minutes'));

-- =============================================================================
-- 테스트 데이터 검증 쿼리 (삽입 후 자동 실행)
-- =============================================================================

-- 생성된 데이터 요약 출력
.print "=== PulseOne 테스트 데이터베이스 생성 완료 ==="
.print ""

.print "📊 테이블별 레코드 수:"
SELECT 
    'tenants' as table_name, COUNT(*) as count FROM tenants
UNION ALL SELECT 'sites', COUNT(*) FROM sites
UNION ALL SELECT 'edge_servers', COUNT(*) FROM edge_servers  
UNION ALL SELECT 'device_groups', COUNT(*) FROM device_groups
UNION ALL SELECT 'driver_plugins', COUNT(*) FROM driver_plugins
UNION ALL SELECT 'devices', COUNT(*) FROM devices
UNION ALL SELECT 'device_settings', COUNT(*) FROM device_settings
UNION ALL SELECT 'device_status', COUNT(*) FROM device_status
UNION ALL SELECT 'data_points', COUNT(*) FROM data_points
UNION ALL SELECT 'current_values', COUNT(*) FROM current_values;

.print ""
.print "🏭 생성된 디바이스 목록:"
SELECT 
    d.id,
    d.name,
    d.protocol_type,
    d.device_type,
    ds.connection_status
FROM devices d
LEFT JOIN device_status ds ON d.id = ds.device_id
ORDER BY d.id;

.print ""
.print "📈 데이터 포인트 프로토콜별 분포:"
SELECT 
    d.protocol_type,
    COUNT(dp.id) as point_count
FROM devices d
LEFT JOIN data_points dp ON d.id = dp.device_id
GROUP BY d.protocol_type
ORDER BY point_count DESC;

.print ""
.print "✅ 테스트 데이터베이스 준비 완료 - collector/tests/bin/test.db"