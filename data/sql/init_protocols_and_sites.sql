-- Protocol Sync Script
-- Aligning with PulseOne Collector Drivers

DELETE FROM protocols WHERE id IN (1, 2, 3, 4, 5, 6, 7, 8);

INSERT INTO protocols (id, protocol_type, display_name, description, is_enabled, created_at, updated_at) VALUES 
(1, 'MODBUS_TCP', 'Modbus TCP', 'Standard Modbus over TCP/IP', 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP),
(2, 'MODBUS_RTU', 'Modbus RTU', 'Standard Modbus over Serial/RTU', 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP),
(3, 'MQTT', 'MQTT v3.1.1', 'Message Queuing Telemetry Transport', 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP),
(4, 'BACNET', 'BACnet/IP', 'Building Automation and Control Networks', 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP),
(5, 'OPC_UA', 'OPC UA', 'OPC Unified Architecture', 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP),
(6, 'BLE_BEACON', 'BLE Beacon', 'Bluetooth Low Energy Beacon Scanning', 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP),
(7, 'ROS', 'ROS Bridge', 'Robot Operating System (ROS 1/2) Bridge', 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP),
(8, 'HTTP_REST', 'HTTP REST API', 'Generic HTTP/HTTPS REST API Client', 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP);

-- Sites ensure
-- Adding a default site for all existing tenants if they don't have one
INSERT INTO sites (tenant_id, name, code, site_type, is_active, created_at, updated_at)
SELECT id, '기본 사업장', 'S001', 'factory', 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP
FROM tenants
WHERE id NOT IN (SELECT DISTINCT tenant_id FROM sites);

-- If someone is using tenant 0 or 1 specifically and it's missing (failsafe)
INSERT OR IGNORE INTO sites (id, tenant_id, name, code, site_type, is_active) 
VALUES (100, 1, '본사 공장 (Default)', 'SITE001', 'factory', 1);
