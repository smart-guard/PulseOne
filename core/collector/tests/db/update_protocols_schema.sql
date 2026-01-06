-- =============================================================================
-- Update protocols table to match ProtocolEntity schema
-- =============================================================================

-- Add missing columns to protocols table
ALTER TABLE protocols ADD COLUMN uses_serial INTEGER DEFAULT 0;
ALTER TABLE protocols ADD COLUMN requires_broker INTEGER DEFAULT 0;
ALTER TABLE protocols ADD COLUMN supported_operations TEXT;
ALTER TABLE protocols ADD COLUMN supported_data_types TEXT;
ALTER TABLE protocols ADD COLUMN connection_params TEXT;
ALTER TABLE protocols ADD COLUMN default_config TEXT;
ALTER TABLE protocols ADD COLUMN security_level VARCHAR(20);
ALTER TABLE protocols ADD COLUMN max_connections INTEGER;
ALTER TABLE protocols ADD COLUMN timeout_ms INTEGER;
ALTER TABLE protocols ADD COLUMN retry_count INTEGER;
ALTER TABLE protocols ADD COLUMN min_firmware_version VARCHAR(20);
ALTER TABLE protocols ADD COLUMN category VARCHAR(50);
ALTER TABLE protocols ADD COLUMN vendor VARCHAR(100);
ALTER TABLE protocols ADD COLUMN standard_reference VARCHAR(100);

-- Update protocol records with proper values
UPDATE protocols SET 
    uses_serial = 0,
    requires_broker = 0,
    supported_operations = '["read", "write"]',
    supported_data_types = '["UINT16", "INT16", "UINT32", "INT32", "FLOAT32"]',
    connection_params = '{}',
    category = 'industrial',
    timeout_ms = 3000,
    retry_count = 3
WHERE name = 'MODBUS_TCP';

UPDATE protocols SET 
    uses_serial = 1,
    requires_broker = 0,
    supported_operations = '["read", "write"]',
    supported_data_types = '["UINT16", "INT16", "UINT32", "INT32", "FLOAT32"]',
    connection_params = '{"baud_rate": 9600, "parity": "N", "data_bits": 8, "stop_bits": 1}',
    category = 'industrial',
    timeout_ms = 3000,
    retry_count = 3
WHERE name = 'MODBUS_RTU';

UPDATE protocols SET 
    uses_serial = 0,
    requires_broker = 1,
    supported_operations = '["publish", "subscribe"]',
    supported_data_types = '["STRING", "JSON", "BINARY"]',
    connection_params = '{"qos": 1, "clean_session": true}',
    category = 'iot',
    timeout_ms = 5000,
    retry_count = 3
WHERE name = 'MQTT';

UPDATE protocols SET 
    uses_serial = 0,
    requires_broker = 0,
    supported_operations = '["read", "write", "subscribe"]',
    supported_data_types = '["ANALOG", "BINARY", "MULTISTATE"]',
    connection_params = '{}',
    category = 'building_automation',
    timeout_ms = 5000,
    retry_count = 3
WHERE name = 'BACNET';

UPDATE protocols SET 
    uses_serial = 0,
    requires_broker = 0,
    supported_operations = '["read", "write", "subscribe"]',
    supported_data_types = '["BOOLEAN", "INT32", "FLOAT", "STRING"]',
    connection_params = '{}',
    category = 'industrial',
    timeout_ms = 5000,
    retry_count = 3
WHERE name = 'OPCUA';

-- Verify
SELECT id, name, uses_serial, requires_broker, category FROM protocols;
