-- backend/lib/database/data/initial-system-data.sql
-- 시스템 레벨 초기 데이터

-- ===============================================================================
-- 시스템 관리자 생성 (비밀번호: admin123!)
-- ===============================================================================
INSERT OR IGNORE INTO system_admins (username, email, password_hash, full_name) VALUES
('admin', 'admin@pulseone.com', '$2b$10$rOzJKe.TvXj8K0Xv4mK4XOE8K4K4K4K4K4K4K4K4K4K4K4K4K4K4K4', 'System Administrator'),
('support', 'support@pulseone.com', '$2b$10$rOzJKe.TvXj8K0Xv4mK4XOE8K4K4K4K4K4K4K4K4K4K4K4K4K4K4K4', 'Support Administrator');

-- ===============================================================================
-- 기본 시스템 설정
-- ===============================================================================
INSERT OR IGNORE INTO system_settings (key, value, description, category, is_public) VALUES
-- 스토리지 설정
('data_retention_days', '365', 'Default data retention period in days', 'storage', {{TRUE}}),
('backup_enabled', '{{TRUE}}', 'Enable automatic backups', 'storage', {{FALSE}}),
('backup_retention_days', '30', 'Backup retention period in days', 'storage', {{FALSE}}),

-- 과금 설정 (스타터 플랜)
('max_edge_servers_starter', '3', 'Maximum edge servers for starter plan', 'billing', {{FALSE}}),
('max_data_points_starter', '1000', 'Maximum data points for starter plan', 'billing', {{FALSE}}),
('max_users_starter', '5', 'Maximum users for starter plan', 'billing', {{FALSE}}),

-- 과금 설정 (프로페셔널 플랜)
('max_edge_servers_professional', '10', 'Maximum edge servers for professional plan', 'billing', {{FALSE}}),
('max_data_points_professional', '10000', 'Maximum data points for professional plan', 'billing', {{FALSE}}),
('max_users_professional', '25', 'Maximum users for professional plan', 'billing', {{FALSE}}),

-- 과금 설정 (엔터프라이즈 플랜)
('max_edge_servers_enterprise', '-1', 'Maximum edge servers for enterprise plan (-1 = unlimited)', 'billing', {{FALSE}}),
('max_data_points_enterprise', '-1', 'Maximum data points for enterprise plan (-1 = unlimited)', 'billing', {{FALSE}}),
('max_users_enterprise', '-1', 'Maximum users for enterprise plan (-1 = unlimited)', 'billing', {{FALSE}}),

-- 수집 설정
('default_polling_interval', '1000', 'Default polling interval in milliseconds', 'collection', {{TRUE}}),
('max_polling_interval', '60000', 'Maximum allowed polling interval in milliseconds', 'collection', {{TRUE}}),
('min_polling_interval', '100', 'Minimum allowed polling interval in milliseconds', 'collection', {{TRUE}}),

-- 시스템 설정
('system_version', '"1.0.0"', 'Current system version', 'system', {{TRUE}}),
('maintenance_mode', '{{FALSE}}', 'System maintenance mode', 'system', {{FALSE}}),
('registration_enabled', '{{TRUE}}', 'Allow new tenant registration', 'system', {{FALSE}}),
('max_tenants', '100', 'Maximum number of tenants', 'system', {{FALSE}}),

-- 로깅 설정
('log_level', '"info"', 'System log level', 'logging', {{FALSE}}),
('enable_packet_logging', '{{FALSE}}', 'Enable communication packet logging', 'logging', {{TRUE}}),
('log_retention_days', '30', 'Log retention period in days', 'logging', {{TRUE}}),
('max_log_file_size_mb', '100', 'Maximum log file size in MB', 'logging', {{TRUE}}),

-- 알림 설정
('email_notifications_enabled', '{{TRUE}}', 'Enable email notifications', 'notifications', {{TRUE}}),
('sms_notifications_enabled', '{{FALSE}}', 'Enable SMS notifications', 'notifications', {{TRUE}}),
('webhook_notifications_enabled', '{{TRUE}}', 'Enable webhook notifications', 'notifications', {{TRUE}}),
('max_notification_retry', '3', 'Maximum notification retry attempts', 'notifications', {{TRUE}}),

-- 보안 설정
('password_min_length', '8', 'Minimum password length', 'security', {{TRUE}}),
('password_require_uppercase', '{{TRUE}}', 'Require uppercase letters in password', 'security', {{TRUE}}),
('password_require_lowercase', '{{TRUE}}', 'Require lowercase letters in password', 'security', {{TRUE}}),
('password_require_numbers', '{{TRUE}}', 'Require numbers in password', 'security', {{TRUE}}),
('password_require_symbols', '{{FALSE}}', 'Require symbols in password', 'security', {{TRUE}}),
('session_timeout_minutes', '480', 'Session timeout in minutes (8 hours)', 'security', {{TRUE}}),
('max_login_attempts', '5', 'Maximum login attempts before lockout', 'security', {{TRUE}}),
('lockout_duration_minutes', '15', 'Account lockout duration in minutes', 'security', {{TRUE}}),

-- API 설정
('api_rate_limit_per_minute', '1000', 'API rate limit per minute per user', 'api', {{TRUE}}),
('api_request_timeout_seconds', '30', 'API request timeout in seconds', 'api', {{TRUE}}),
('api_max_payload_size_mb', '10', 'Maximum API payload size in MB', 'api', {{TRUE}}),

-- 성능 설정
('max_concurrent_connections', '1000', 'Maximum concurrent connections', 'performance', {{FALSE}}),
('database_connection_pool_size', '20', 'Database connection pool size', 'performance', {{FALSE}}),
('cache_ttl_seconds', '300', 'Cache time-to-live in seconds', 'performance', {{TRUE}}),

-- 모니터링 설정
('health_check_interval_seconds', '30', 'Health check interval in seconds', 'monitoring', {{TRUE}}),
('metric_collection_interval_seconds', '60', 'Metric collection interval in seconds', 'monitoring', {{TRUE}}),
('alert_cooldown_minutes', '5', 'Alert cooldown period in minutes', 'monitoring', {{TRUE}});

-- ===============================================================================
-- 기본 드라이버 플러그인 등록
-- ===============================================================================
INSERT OR IGNORE INTO driver_plugins (name, protocol_type, version, file_path, description, author, config_schema) VALUES
-- Modbus TCP 드라이버
(
    'Modbus TCP Driver',
    'modbus_tcp',
    '1.0.0',
    './plugins/libmodbus_tcp_driver.so',
    'Modbus TCP client driver using libmodbus',
    'PulseOne Team',
    '{
        "type": "object",
        "properties": {
            "client_id": {"type": "string", "maxLength": 50},
            "username": {"type": "string"},
            "password": {"type": "string"},
            "keep_alive": {"type": "integer", "minimum": 30, "maximum": 300},
            "qos": {"type": "integer", "enum": [0, 1, 2]},
            "clean_session": {"type": "boolean"},
            "ssl_enabled": {"type": "boolean"},
            "ssl_ca_cert": {"type": "string"},
            "ssl_client_cert": {"type": "string"},
            "ssl_client_key": {"type": "string"}
        },
        "required": ["client_id"]
    }'
),

-- BACnet 드라이버
(
    'BACnet IP Driver',
    'bacnet',
    '1.0.0',
    './plugins/libbacnet_ip_driver.so',
    'BACnet/IP communication driver',
    'PulseOne Team',
    '{
        "type": "object",
        "properties": {
            "device_instance": {"type": "integer", "minimum": 0, "maximum": 4194303},
            "port": {"type": "integer", "minimum": 1, "maximum": 65535, "default": 47808},
            "max_apdu": {"type": "integer", "enum": [50, 128, 206, 480, 1024, 1476]},
            "segmentation": {"type": "string", "enum": ["none", "transmit", "receive", "both"]},
            "timeout": {"type": "integer", "minimum": 1000, "maximum": 60000}
        },
        "required": ["device_instance"]
    }'
),

-- OPC-UA 드라이버
(
    'OPC-UA Client Driver',
    'opcua',
    '1.0.0',
    './plugins/libopcua_client_driver.so',
    'OPC-UA client communication driver',
    'PulseOne Team',
    '{
        "type": "object",
        "properties": {
            "security_mode": {"type": "string", "enum": ["none", "sign", "signAndEncrypt"]},
            "security_policy": {"type": "string", "enum": ["none", "basic128", "basic256", "basic256sha256"]},
            "authentication": {"type": "string", "enum": ["anonymous", "username", "certificate"]},
            "username": {"type": "string"},
            "password": {"type": "string"},
            "certificate_path": {"type": "string"},
            "private_key_path": {"type": "string"},
            "session_timeout": {"type": "integer", "minimum": 10000, "maximum": 3600000}
        }
    }'
);

-- ===============================================================================
-- 샘플 테넌트 생성 (개발/테스트용)
-- ===============================================================================
INSERT OR IGNORE INTO tenants (
    company_name, company_code, domain, 
    contact_name, contact_email, contact_phone,
    address, country, timezone,
    subscription_plan, max_edge_servers, max_data_points, max_users,
    billing_email
) VALUES 
-- Demo Company
(
    'Demo Manufacturing Co.',
    'demo',
    'demo.pulseone.com',
    'Demo Manager',
    'demo@demo-company.com',
    '+1-555-0123',
    '123 Industrial Avenue, Demo City, DC 12345, USA',
    'United States',
    'America/New_York',
    'professional',
    5,
    5000,
    10,
    'billing@demo-company.com'
),

-- Test Company
(
    'Test Corporation',
    'test',
    'test.pulseone.com',
    'Test Manager',
    'test@test-corp.com',
    '+1-555-0456',
    '456 Technology Blvd, Test City, TC 67890, USA',
    'United States',
    'America/Los_Angeles',
    'starter',
    3,
    1000,
    5,
    'accounting@test-corp.com'
),

-- Enterprise Sample
(
    'Global Industries Inc.',
    'global',
    'global.pulseone.com',
    'CTO Office',
    'cto@global-industries.com',
    '+1-555-0789',
    '789 Enterprise Plaza, Global City, GC 11111, USA',
    'United States',
    'UTC',
    'enterprise',
    -1,
    -1,
    -1,
    'finance@global-industries.com'
);

-- ===============================================================================
-- 샘플 Edge 서버 등록
-- ===============================================================================
INSERT OR IGNORE INTO edge_servers (
    tenant_id,
    server_name,
    factory_name,
    location,
    registration_token,
    activation_code,
    status,
    version,
    config
) VALUES 
-- Demo Company Edge 서버들
(
    (SELECT id FROM tenants WHERE company_code = 'demo' LIMIT 1),
    'Factory-A-Edge-01',
    'Demo Factory A',
    'Seoul, South Korea',
    'demo-edge-token-001',
    '123456',
    'active',
    '1.0.0',
    '{"cpu_cores": 4, "memory_gb": 8, "storage_gb": 256, "os": "Ubuntu 22.04"}'
),
(
    (SELECT id FROM tenants WHERE company_code = 'demo' LIMIT 1),
    'Factory-B-Edge-01',
    'Demo Factory B',
    'Busan, South Korea',
    'demo-edge-token-002',
    '234567',
    'active',
    '1.0.0',
    '{"cpu_cores": 8, "memory_gb": 16, "storage_gb": 512, "os": "Ubuntu 22.04"}'
),

-- Test Company Edge 서버
(
    (SELECT id FROM tenants WHERE company_code = 'test' LIMIT 1),
    'Main-Plant-Edge',
    'Test Main Plant',
    'Austin, Texas, USA',
    'test-edge-token-001',
    '345678',
    'active',
    '1.0.0',
    '{"cpu_cores": 2, "memory_gb": 4, "storage_gb": 128, "os": "Ubuntu 22.04"}'
),

-- Global Industries Edge 서버들
(
    (SELECT id FROM tenants WHERE company_code = 'global' LIMIT 1),
    'NA-Plant-Edge-01',
    'North America Plant 1',
    'Detroit, Michigan, USA',
    'global-edge-token-001',
    '456789',
    'active',
    '1.0.0',
    '{"cpu_cores": 16, "memory_gb": 32, "storage_gb": 1024, "os": "Ubuntu 22.04"}'
),
(
    (SELECT id FROM tenants WHERE company_code = 'global' LIMIT 1),
    'EU-Plant-Edge-01',
    'Europe Plant 1',
    'Stuttgart, Germany',
    'global-edge-token-002',
    '567890',
    'active',
    '1.0.0',
    '{"cpu_cores": 16, "memory_gb": 32, "storage_gb": 1024, "os": "Ubuntu 22.04"}'
),
(
    (SELECT id FROM tenants WHERE company_code = 'global' LIMIT 1),
    'APAC-Plant-Edge-01',
    'Asia Pacific Plant 1',
    'Shanghai, China',
    'global-edge-token-003',
    '678901',
    'active',
    '1.0.0',
    '{"cpu_cores": 12, "memory_gb": 24, "storage_gb": 512, "os": "Ubuntu 22.04"}'
);

-- ===============================================================================
-- 샘플 사용량 데이터 (지난 30일)
-- ===============================================================================
INSERT OR IGNORE INTO usage_metrics (
    tenant_id,
    edge_server_id,
    metric_type,
    metric_value,
    measurement_date,
    measurement_hour
) 
SELECT 
    t.id as tenant_id,
    es.id as edge_server_id,
    'data_points' as metric_type,
    CASE 
        WHEN t.company_code = 'demo' THEN 2500 + (RANDOM() * 1000)::INTEGER
        WHEN t.company_code = 'test' THEN 800 + (RANDOM() * 200)::INTEGER
        WHEN t.company_code = 'global' THEN 8000 + (RANDOM() * 2000)::INTEGER
    END as metric_value,
    DATE(datetime('now', '-' || day_offset || ' days')) as measurement_date,
    NULL as measurement_hour
FROM 
    tenants t
    CROSS JOIN edge_servers es ON es.tenant_id = t.id
    CROSS JOIN (
        SELECT generate_series(0, 29) as day_offset
    ) days
WHERE t.company_code IN ('demo', 'test', 'global');

-- ===============================================================================
-- 샘플 과금 레코드
-- ===============================================================================
INSERT OR IGNORE INTO billing_records (
    tenant_id,
    billing_period_start,
    billing_period_end,
    base_subscription_fee,
    edge_server_fee,
    data_points_fee,
    subtotal,
    total_amount,
    status,
    invoice_number
)
SELECT 
    t.id,
    DATE('now', 'start of month', '-1 month') as billing_period_start,
    DATE('now', 'start of month', '-1 day') as billing_period_end,
    CASE 
        WHEN t.subscription_plan = 'starter' THEN 99.00
        WHEN t.subscription_plan = 'professional' THEN 299.00
        WHEN t.subscription_plan = 'enterprise' THEN 999.00
    END as base_subscription_fee,
    (SELECT COUNT(*) FROM edge_servers WHERE tenant_id = t.id AND status = 'active') * 50.00 as edge_server_fee,
    0.00 as data_points_fee,
    CASE 
        WHEN t.subscription_plan = 'starter' THEN 99.00 + (SELECT COUNT(*) FROM edge_servers WHERE tenant_id = t.id AND status = 'active') * 50.00
        WHEN t.subscription_plan = 'professional' THEN 299.00 + (SELECT COUNT(*) FROM edge_servers WHERE tenant_id = t.id AND status = 'active') * 50.00
        WHEN t.subscription_plan = 'enterprise' THEN 999.00 + (SELECT COUNT(*) FROM edge_servers WHERE tenant_id = t.id AND status = 'active') * 50.00
    END as subtotal,
    CASE 
        WHEN t.subscription_plan = 'starter' THEN 99.00 + (SELECT COUNT(*) FROM edge_servers WHERE tenant_id = t.id AND status = 'active') * 50.00
        WHEN t.subscription_plan = 'professional' THEN 299.00 + (SELECT COUNT(*) FROM edge_servers WHERE tenant_id = t.id AND status = 'active') * 50.00
        WHEN t.subscription_plan = 'enterprise' THEN 999.00 + (SELECT COUNT(*) FROM edge_servers WHERE tenant_id = t.id AND status = 'active') * 50.00
    END as total_amount,
    'paid' as status,
    'INV-' || strftime('%Y%m', 'now', '-1 month') || '-' || UPPER(substr(t.id, 1, 8)) as invoice_number
FROM tenants t
WHERE t.company_code IN ('demo', 'test', 'global');

-- ===============================================================================
-- 스키마 마이그레이션 기록
-- ===============================================================================
INSERT OR IGNORE INTO schema_migrations (version, name) VALUES
(1, '001_create_system_tables'),
(2, '002_create_tenant_functions'),
(3, '003_create_billing_system'),
(4, '004_create_management_functions'),
(5, '005_create_driver_plugins'),
(6, '006_create_initial_data');

-- ===============================================================================
-- 완료 메시지 (시스템 로그는 테넌트별 테이블이므로 여기서는 제외)
-- ===============================================================================
-- System initialization completed message will be logged by the application
            "slave_id": {"type": "integer", "minimum": 1, "maximum": 247},
            "byte_order": {"type": "string", "enum": ["big_endian", "little_endian"]},
            "word_order": {"type": "string", "enum": ["big_endian", "little_endian"]},
            "connection_timeout": {"type": "integer", "minimum": 1000, "maximum": 60000},
            "response_timeout": {"type": "integer", "minimum": 100, "maximum": 10000}
        },
        "required": ["slave_id"]
    }'
),

-- Modbus RTU 드라이버
(
    'Modbus RTU Driver',
    'modbus_rtu',
    '1.0.0',
    './plugins/libmodbus_rtu_driver.so',
    'Modbus RTU serial communication driver',
    'PulseOne Team',
    '{
        "type": "object",
        "properties": {
            "slave_id": {"type": "integer", "minimum": 1, "maximum": 247},
            "baud_rate": {"type": "integer", "enum": [9600, 19200, 38400, 57600, 115200]},
            "data_bits": {"type": "integer", "enum": [7, 8]},
            "stop_bits": {"type": "integer", "enum": [1, 2]},
            "parity": {"type": "string", "enum": ["none", "even", "odd"]},
            "byte_order": {"type": "string", "enum": ["big_endian", "little_endian"]}
        },
        "required": ["slave_id", "baud_rate"]
    }'
),

-- MQTT 드라이버
(
    'MQTT Client Driver',
    'mqtt',
    '1.0.0',
    './plugins/libmqtt_client_driver.so',
    'MQTT client communication driver with QoS support',
    'PulseOne Team',
    '{
        "type": "object",
        "properties": {
            "client_id": {"type": "string", "maxLength": 50},
            "username": {"type": "string"},
            "password": {"type": "string"},
            "keep_alive": {"type": "integer", "minimum": 30, "maximum": 300},
            "qos": {"type": "integer", "enum": [0, 1, 2]},
            "clean_session": {"type": "boolean"},
            "ssl_enabled": {"type": "boolean"},
            "ssl_ca_cert": {"type": "string"},
            "ssl_client_cert": {"type": "string"},
            "ssl_client_key": {"type": "string"}
        },
        "required": ["client_id"]
    }'
),

-- BACnet 드라이버
(
    'BACnet IP Driver',
    'bacnet',
    '1.0.0',
    './plugins/libbacnet_ip_driver.so',
    'BACnet/IP communication driver',
    'PulseOne Team',
    '{
        "type": "object",
        "properties": {
            "device_instance": {"type": "integer", "minimum": 0, "maximum": 4194303},
            "port": {"type": "integer", "minimum": 1, "maximum": 65535, "default": 47808},
            "max_apdu": {"type": "integer", "enum": [50, 128, 206, 480, 1024, 1476]},
            "segmentation": {"type": "string", "enum": ["none", "transmit", "receive", "both"]},
            "timeout": {"type": "integer", "minimum": 1000, "maximum": 60000}
        },
        "required": ["device_instance"]
    }'
),

-- OPC-UA 드라이버
(
    'OPC-UA Client Driver',
    'opcua',
    '1.0.0',
    './plugins/libopcua_client_driver.so',
    'OPC-UA client communication driver',
    'PulseOne Team',
    '{
        "type": "object",
        "properties": {
            "security_mode": {"type": "string", "enum": ["none", "sign", "signAndEncrypt"]},
            "security_policy": {"type": "string", "enum": ["none", "basic128", "basic256", "basic256sha256"]},
            "authentication": {"type": "string", "enum": ["anonymous", "username", "certificate"]},
            "username": {"type": "string"},
            "password": {"type": "string"},
            "certificate_path": {"type": "string"},
            "private_key_path": {"type": "string"},
            "session_timeout": {"type": "integer", "minimum": 10000, "maximum": 3600000}
        }
    }'
);

-- ===============================================================================
-- 샘플 테넌트 생성 (개발/테스트용)
-- ===============================================================================
INSERT OR IGNORE INTO tenants (
    company_name, company_code, domain, 
    contact_name, contact_email, contact_phone,
    address, country, timezone,
    subscription_plan, max_edge_servers, max_data_points, max_users,
    billing_email
) VALUES 
-- Demo Company
(
    'Demo Manufacturing Co.',
    'demo',
    'demo.pulseone.com',
    'Demo Manager',
    'demo@demo-company.com',
    '+1-555-0123',
    '123 Industrial Avenue, Demo City, DC 12345, USA',
    'United States',
    'America/New_York',
    'professional',
    5,
    5000,
    10,
    'billing@demo-company.com'
),

-- Test Company
(
    'Test Corporation',
    'test',
    'test.pulseone.com',
    'Test Manager',
    'test@test-corp.com',
    '+1-555-0456',
    '456 Technology Blvd, Test City, TC 67890, USA',
    'United States',
    'America/Los_Angeles',
    'starter',
    3,
    1000,
    5,
    'accounting@test-corp.com'
),

-- Enterprise Sample
(
    'Global Industries Inc.',
    'global',
    'global.pulseone.com',
    'CTO Office',
    'cto@global-industries.com',
    '+1-555-0789',
    '789 Enterprise Plaza, Global City, GC 11111, USA',
    'United States',
    'UTC',
    'enterprise',
    -1,
    -1,
    -1,
    'finance@global-industries.com'
);

-- ===============================================================================
-- 샘플 Edge 서버 등록
-- ===============================================================================
INSERT OR IGNORE INTO edge_servers (
    tenant_id,
    server_name,
    factory_name,
    location,
    registration_token,
    activation_code,
    status,
    version,
    config
) VALUES 
-- Demo Company Edge 서버들
(
    (SELECT id FROM tenants WHERE company_code = 'demo' LIMIT 1),
    'Factory-A-Edge-01',
    'Demo Factory A',
    'Seoul, South Korea',
    'demo-edge-token-001',
    '123456',
    'active',
    '1.0.0',
    '{"cpu_cores": 4, "memory_gb": 8, "storage_gb": 256, "os": "Ubuntu 22.04"}'
),
(
    (SELECT id FROM tenants WHERE company_code = 'demo' LIMIT 1),
    'Factory-B-Edge-01',
    'Demo Factory B',
    'Busan, South Korea',
    'demo-edge-token-002',
    '234567',
    'active',
    '1.0.0',
    '{"cpu_cores": 8, "memory_gb": 16, "storage_gb": 512, "os": "Ubuntu 22.04"}'
),

-- Test Company Edge 서버
(
    (SELECT id FROM tenants WHERE company_code = 'test' LIMIT 1),
    'Main-Plant-Edge',
    'Test Main Plant',
    'Austin, Texas, USA',
    'test-edge-token-001',
    '345678',
    'active',
    '1.0.0',
    '{"cpu_cores": 2, "memory_gb": 4, "storage_gb": 128, "os": "Ubuntu 22.04"}'
),

-- Global Industries Edge 서버들
(
    (SELECT id FROM tenants WHERE company_code = 'global' LIMIT 1),
    'NA-Plant-Edge-01',
    'North America Plant 1',
    'Detroit, Michigan, USA',
    'global-edge-token-001',
    '456789',
    'active',
    '1.0.0',
    '{"cpu_cores": 16, "memory_gb": 32, "storage_gb": 1024, "os": "Ubuntu 22.04"}'
),
(
    (SELECT id FROM tenants WHERE company_code = 'global' LIMIT 1),
    'EU-Plant-Edge-01',
    'Europe Plant 1',
    'Stuttgart, Germany',
    'global-edge-token-002',
    '567890',
    'active',
    '1.0.0',
    '{"cpu_cores": 16, "memory_gb": 32, "storage_gb": 1024, "os": "Ubuntu 22.04"}'
),
(
    (SELECT id FROM tenants WHERE company_code = 'global' LIMIT 1),
    'APAC-Plant-Edge-01',
    'Asia Pacific Plant 1',
    'Shanghai, China',
    'global-edge-token-003',
    '678901',
    'active',
    '1.0.0',
    '{"cpu_cores": 12, "memory_gb": 24, "storage_gb": 512, "os": "Ubuntu 22.04"}'
);

-- ===============================================================================
-- 스키마 마이그레이션 기록
-- ===============================================================================
INSERT OR IGNORE INTO schema_migrations (version, name) VALUES
(1, '001_create_system_tables'),
(2, '002_create_tenant_functions'),
(3, '003_create_billing_system'),
(4, '004_create_management_functions'),
(5, '005_create_driver_plugins'),
(6, '006_create_initial_data');

-- ===============================================================================
-- 완료 메시지
-- ===============================================================================
-- System initialization completed message will be logged by the application