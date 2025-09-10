-- ============================================================================
-- backend/lib/database/schemas/08-protocols-table.sql
-- 프로토콜 정보 전용 테이블 - 하드코딩 제거
-- ============================================================================

-- =============================================================================
-- 프로토콜 정보 테이블 
-- =============================================================================
CREATE TABLE IF NOT EXISTS protocols (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 기본 정보
    protocol_type VARCHAR(50) NOT NULL UNIQUE,      -- MODBUS_TCP, MODBUS_RTU, MQTT, etc.
    display_name VARCHAR(100) NOT NULL,             -- "Modbus TCP", "MQTT", etc.
    description TEXT,                               -- 상세 설명
    
    -- 네트워크 정보
    default_port INTEGER,                           -- 기본 포트 (502, 1883, etc.)
    uses_serial INTEGER DEFAULT 0,                 -- 시리얼 통신 사용 여부
    requires_broker INTEGER DEFAULT 0,             -- 브로커 필요 여부 (MQTT 등)
    
    -- 기능 지원 정보 (JSON)
    supported_operations TEXT,                      -- ["read", "write", "subscribe", etc.]
    supported_data_types TEXT,                      -- ["boolean", "int16", "float32", etc.]
    connection_params TEXT,                         -- 연결에 필요한 파라미터 스키마
    
    -- 설정 정보
    default_polling_interval INTEGER DEFAULT 1000, -- 기본 폴링 간격 (ms)
    default_timeout INTEGER DEFAULT 5000,          -- 기본 타임아웃 (ms)
    max_concurrent_connections INTEGER DEFAULT 1,   -- 최대 동시 연결 수
    
    -- 상태 정보
    is_enabled INTEGER DEFAULT 1,                  -- 프로토콜 활성화 여부
    is_deprecated INTEGER DEFAULT 0,               -- 사용 중단 예정
    min_firmware_version VARCHAR(20),              -- 최소 펌웨어 버전
    
    -- 분류 정보
    category VARCHAR(50),                           -- industrial, iot, building_automation, etc.
    vendor VARCHAR(100),                            -- 제조사/개발사
    standard_reference VARCHAR(100),               -- 표준 문서 참조
    
    -- 메타데이터
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 제약조건
    CONSTRAINT chk_category CHECK (category IN ('industrial', 'iot', 'building_automation', 'network', 'web'))
);

-- =============================================================================
-- 프로토콜 기본 데이터 삽입
-- =============================================================================

-- 산업용 프로토콜
INSERT OR REPLACE INTO protocols (
    protocol_type, display_name, description, default_port, uses_serial, 
    supported_operations, supported_data_types, connection_params,
    default_polling_interval, default_timeout, category, vendor, standard_reference
) VALUES

-- Modbus TCP
('MODBUS_TCP', 'Modbus TCP', 
 'Modbus TCP/IP Protocol for Ethernet-based industrial networks',
 502, 0, 
 '["read_coils", "read_discrete_inputs", "read_holding_registers", "read_input_registers", "write_single_coil", "write_single_register", "write_multiple_coils", "write_multiple_registers"]',
 '["boolean", "int16", "uint16", "int32", "uint32", "float32"]',
 '{"host": {"type": "string", "required": true}, "port": {"type": "integer", "default": 502}, "slave_id": {"type": "integer", "default": 1, "range": [1, 247]}}',
 1000, 5000, 'industrial', 'Modbus Organization', 'MODBUS Application Protocol Specification V1.1b3'),

-- Modbus RTU
('MODBUS_RTU', 'Modbus RTU',
 'Modbus RTU Serial Protocol for RS-485/RS-232 networks',
 NULL, 1,
 '["read_coils", "read_discrete_inputs", "read_holding_registers", "read_input_registers", "write_single_coil", "write_single_register", "write_multiple_coils", "write_multiple_registers"]',
 '["boolean", "int16", "uint16", "int32", "uint32", "float32"]',
 '{"port": {"type": "string", "required": true}, "baud_rate": {"type": "integer", "default": 9600, "options": [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200]}, "data_bits": {"type": "integer", "default": 8, "options": [7, 8]}, "stop_bits": {"type": "integer", "default": 1, "options": [1, 2]}, "parity": {"type": "string", "default": "N", "options": ["N", "E", "O"]}, "slave_id": {"type": "integer", "default": 1, "range": [1, 247]}}',
 2000, 3000, 'industrial', 'Modbus Organization', 'MODBUS over serial line specification and implementation guide V1.02'),

-- MQTT
('MQTT', 'MQTT',
 'Message Queuing Telemetry Transport - Lightweight messaging protocol for IoT',
 1883, 0,
 '["publish", "subscribe", "unsubscribe", "ping", "connect", "disconnect"]',
 '["string", "json", "binary", "boolean", "integer", "float"]',
 '{"host": {"type": "string", "required": true}, "port": {"type": "integer", "default": 1883}, "client_id": {"type": "string", "required": false}, "username": {"type": "string", "required": false}, "password": {"type": "string", "required": false}, "keep_alive": {"type": "integer", "default": 60}, "clean_session": {"type": "boolean", "default": true}, "qos": {"type": "integer", "default": 0, "options": [0, 1, 2]}}',
 1000, 10000, 'iot', 'OASIS', 'MQTT Version 5.0'),

-- BACnet/IP
('BACNET', 'BACnet/IP',
 'Building Automation and Control Networks over IP',
 47808, 0,
 '["read_property", "write_property", "who_is", "i_am", "subscribe_cov", "device_discovery"]',
 '["boolean", "integer", "real", "enumerated", "string", "object_identifier"]',
 '{"host": {"type": "string", "required": true}, "port": {"type": "integer", "default": 47808}, "device_instance": {"type": "integer", "required": true, "range": [0, 4194303]}, "max_apdu_length": {"type": "integer", "default": 1476}}',
 5000, 10000, 'building_automation', 'ASHRAE', 'ANSI/ASHRAE Standard 135-2020'),

-- OPC UA
('OPC_UA', 'OPC UA',
 'OPC Unified Architecture - Industrial communication standard',
 4840, 0,
 '["browse", "read", "write", "subscribe", "method_call", "historical_data", "alarms_events"]',
 '["boolean", "sbyte", "byte", "int16", "uint16", "int32", "uint32", "int64", "uint64", "float", "double", "string", "datetime", "guid", "bytestring"]',
 '{"endpoint_url": {"type": "string", "required": true}, "security_mode": {"type": "string", "default": "None", "options": ["None", "Sign", "SignAndEncrypt"]}, "security_policy": {"type": "string", "default": "None"}, "username": {"type": "string", "required": false}, "password": {"type": "string", "required": false}, "certificate": {"type": "string", "required": false}}',
 1000, 15000, 'industrial', 'OPC Foundation', 'IEC 62541'),

-- Ethernet/IP
('ETHERNET_IP', 'Ethernet/IP',
 'Common Industrial Protocol over Ethernet',
 44818, 0,
 '["explicit_messaging", "implicit_messaging", "cip_routing", "get_attribute_single", "set_attribute_single"]',
 '["boolean", "sint", "int", "dint", "lint", "usint", "uint", "udint", "ulint", "real", "lreal", "string"]',
 '{"host": {"type": "string", "required": true}, "port": {"type": "integer", "default": 44818}, "slot": {"type": "integer", "default": 0}, "vendor_id": {"type": "integer", "required": false}, "device_type": {"type": "integer", "required": false}, "product_code": {"type": "integer", "required": false}}',
 1000, 8000, 'industrial', 'ODVA', 'The CIP Networks Library, Volume 1'),

-- PROFINET
('PROFINET', 'PROFINET',
 'Process Field Network - Industrial Ethernet standard',
 NULL, 0,
 '["real_time_data", "isochronous_real_time", "device_configuration", "diagnostics", "redundancy"]',
 '["boolean", "byte", "word", "dword", "sint", "int", "dint", "usint", "uint", "udint", "real", "string"]',
 '{"device_name": {"type": "string", "required": true}, "ip_address": {"type": "string", "required": true}, "update_time": {"type": "integer", "default": 1}, "watchdog_time": {"type": "integer", "default": 3}}',
 100, 5000, 'industrial', 'PROFIBUS & PROFINET International', 'IEC 61158 and IEC 61784'),

-- HTTP REST
('HTTP_REST', 'HTTP REST',
 'RESTful HTTP API for web-based device communication',
 80, 0,
 '["get", "post", "put", "patch", "delete", "head", "options"]',
 '["json", "xml", "string", "boolean", "integer", "float", "array", "object"]',
 '{"base_url": {"type": "string", "required": true}, "port": {"type": "integer", "default": 80}, "use_https": {"type": "boolean", "default": false}, "api_key": {"type": "string", "required": false}, "auth_type": {"type": "string", "default": "none", "options": ["none", "basic", "bearer", "api_key"]}, "headers": {"type": "object", "required": false}, "timeout": {"type": "integer", "default": 30000}}',
 5000, 30000, 'web', 'W3C/IETF', 'RFC 7231, RFC 7232, RFC 7233, RFC 7234, RFC 7235'),

-- SNMP
('SNMP', 'SNMP',
 'Simple Network Management Protocol for network device management',
 161, 0,
 '["get", "get_next", "get_bulk", "set", "trap", "inform", "walk"]',
 '["integer", "string", "oid", "ipaddress", "counter32", "gauge32", "timeticks", "counter64"]',
 '{"host": {"type": "string", "required": true}, "port": {"type": "integer", "default": 161}, "version": {"type": "string", "default": "2c", "options": ["1", "2c", "3"]}, "community": {"type": "string", "default": "public"}, "security_level": {"type": "string", "default": "noAuthNoPriv", "options": ["noAuthNoPriv", "authNoPriv", "authPriv"]}, "username": {"type": "string", "required": false}, "auth_protocol": {"type": "string", "required": false}, "auth_password": {"type": "string", "required": false}}',
 10000, 15000, 'network', 'IETF', 'RFC 3411-3418'),

-- DNP3
('DNP3', 'DNP3',
 'Distributed Network Protocol for SCADA systems',
 20000, 0,
 '["data_acquisition", "control_operations", "time_synchronization", "file_transfer", "secure_authentication"]',
 '["binary", "analog", "counter", "binary_output", "analog_output", "string"]',
 '{"host": {"type": "string", "required": true}, "port": {"type": "integer", "default": 20000}, "master_address": {"type": "integer", "default": 1}, "outstation_address": {"type": "integer", "default": 10}, "link_layer_confirmation": {"type": "boolean", "default": true}, "unsolicited_enabled": {"type": "boolean", "default": true}}',
 1000, 10000, 'industrial', 'DNP Users Group', 'IEEE 1815-2012'),

-- IEC 61850
('IEC61850', 'IEC 61850',
 'Communication standard for electrical substations',
 102, 0,
 '["logical_nodes", "data_objects", "reporting", "logging", "control_blocks", "sampled_values", "goose"]',
 '["boolean", "integer", "float", "string", "enumerated", "timestamp", "quality"]',
 '{"host": {"type": "string", "required": true}, "port": {"type": "integer", "default": 102}, "ap_title": {"type": "string", "required": false}, "ae_qualifier": {"type": "integer", "required": false}, "psel": {"type": "string", "required": false}, "ssel": {"type": "string", "required": false}, "tsel": {"type": "string", "required": false}}',
 1000, 10000, 'industrial', 'IEC', 'IEC 61850 series');

-- =============================================================================
-- 인덱스 생성 (성능 최적화)
-- =============================================================================
CREATE INDEX IF NOT EXISTS idx_protocols_type ON protocols(protocol_type);
CREATE INDEX IF NOT EXISTS idx_protocols_enabled ON protocols(is_enabled);
CREATE INDEX IF NOT EXISTS idx_protocols_category ON protocols(category);
CREATE INDEX IF NOT EXISTS idx_protocols_port ON protocols(default_port);

-- =============================================================================
-- 뷰 생성 (편의성)
-- =============================================================================

-- 활성화된 프로토콜만 보기
CREATE VIEW IF NOT EXISTS active_protocols AS
SELECT * FROM protocols 
WHERE is_enabled = 1 AND is_deprecated = 0
ORDER BY display_name;

-- 프로토콜 사용 통계 뷰
CREATE VIEW IF NOT EXISTS protocol_usage_stats AS
SELECT 
    p.protocol_type,
    p.display_name,
    COUNT(d.id) as device_count,
    COUNT(CASE WHEN d.is_enabled = 1 THEN 1 END) as enabled_devices,
    COUNT(CASE WHEN ds.connection_status = 'connected' THEN 1 END) as connected_devices
FROM protocols p
LEFT JOIN devices d ON d.protocol_type = p.protocol_type
LEFT JOIN device_status ds ON ds.device_id = d.id
WHERE p.is_enabled = 1
GROUP BY p.protocol_type, p.display_name
ORDER BY device_count DESC;