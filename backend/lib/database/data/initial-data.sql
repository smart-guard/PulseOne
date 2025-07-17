-- backend/lib/database/data/initial-data.sql
-- PulseOne 기초 필수 데이터 (시스템 운영에 반드시 필요한 데이터)

-- 기본 시스템 관리자 생성 (비밀번호: admin123!)
INSERT OR IGNORE INTO system_admins (username, email, password_hash, full_name, is_active)
VALUES ('admin', 'admin@pulseone.com', '$2b$10$rOzJKe.TvXj8K0Xv4mK4XOE8K4K4K4K4K4K4K4K4K4K4K4K4K4K4K4', 'System Administrator', {{TRUE}});

-- 기본 시스템 설정
INSERT OR IGNORE INTO system_settings (key_name, value, description, category, is_public)
VALUES 
    ('data_retention_days', '365', 'Default data retention period in days', 'storage', {{TRUE}}),
    ('max_edge_servers_starter', '3', 'Maximum edge servers for starter plan', 'billing', {{FALSE}}),
    ('max_edge_servers_professional', '10', 'Maximum edge servers for professional plan', 'billing', {{FALSE}}),
    ('max_edge_servers_enterprise', '-1', 'Maximum edge servers for enterprise plan (-1 = unlimited)', 'billing', {{FALSE}}),
    ('max_data_points_starter', '1000', 'Maximum data points for starter plan', 'billing', {{FALSE}}),
    ('max_data_points_professional', '10000', 'Maximum data points for professional plan', 'billing', {{FALSE}}),
    ('max_data_points_enterprise', '-1', 'Maximum data points for enterprise plan (-1 = unlimited)', 'billing', {{FALSE}}),
    ('default_polling_interval', '1000', 'Default polling interval in milliseconds', 'collection', {{TRUE}}),
    ('system_version', '1.0.0', 'Current system version', 'system', {{TRUE}}),
    ('maintenance_mode', 'false', 'System maintenance mode', 'system', {{FALSE}}),
    ('registration_enabled', 'true', 'Allow new tenant registration', 'system', {{FALSE}}),
    ('log_level', 'info', 'System log level', 'system', {{FALSE}}),
    ('enable_packet_logging', 'false', 'Enable communication packet logging', 'logging', {{TRUE}}),
    ('backup_enabled', 'true', 'Enable automatic backups', 'backup', {{FALSE}}),
    ('backup_retention_days', '30', 'Backup retention period in days', 'backup', {{FALSE}}),
    ('email_notifications_enabled', 'true', 'Enable email notifications', 'notifications', {{TRUE}}),
    ('sms_notifications_enabled', 'false', 'Enable SMS notifications', 'notifications', {{TRUE}});

-- 기본 드라이버 플러그인 정보 (빌드된 드라이버가 있을 때)
INSERT OR IGNORE INTO driver_plugins (name, protocol_type, version, file_path, description, author, is_enabled)
VALUES 
    ('Modbus TCP Driver', 'modbus', '1.0.0', '/drivers/modbus/ModbusDriver.so', 'Standard Modbus TCP/IP driver', 'PulseOne Team', {{TRUE}}),
    ('MQTT Client Driver', 'mqtt', '1.0.0', '/drivers/mqtt/MQTTDriver.so', 'MQTT client driver for IoT devices', 'PulseOne Team', {{TRUE}}),
    ('BACnet Driver', 'bacnet', '1.0.0', '/drivers/bacnet/BACnetDriver.so', 'BACnet protocol driver', 'PulseOne Team', {{TRUE}});

-- 기본 테넌트 (데모/테스트용) - 실제 운영에서는 제거 가능
INSERT OR IGNORE INTO tenants (
    company_name, company_code, domain, 
    contact_name, contact_email, 
    subscription_plan, subscription_status,
    max_edge_servers, max_data_points, max_users,
    is_active
) VALUES (
    'Demo Company', 'demo', 'demo.pulseone.com', 
    'Demo Manager', 'demo@example.com', 
    'professional', 'active',
    5, 5000, 10,
    {{TRUE}}
);