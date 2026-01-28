-- E2E Verification Seed Data
-- Fixes missing WLS.PV and TEST-PLC-001 Device

BEGIN TRANSACTION;

-- 1. Tenant & Site
INSERT INTO tenants (id, company_name, company_code) VALUES (1, 'Test Tenant', 'TEST001') ON CONFLICT(id) DO NOTHING;
INSERT INTO sites (id, tenant_id, name, code, site_type) VALUES (1, 1, 'Test Site', 'SITE001', 'factory') ON CONFLICT(id) DO NOTHING;

-- 2. Protocols (Ensure Modbus exists)
INSERT INTO protocols (id, protocol_type, display_name) VALUES (1, 'MODBUS_TCP', 'Modbus TCP') ON CONFLICT(id) DO NOTHING;

-- 3. Edge Server (Collector) - Use docker networking for IP
INSERT INTO edge_servers (id, tenant_id, server_name, status, ip_address) 
VALUES (1, 1, 'Main Collector', 'active', 'host.docker.internal') ON CONFLICT(id) DO NOTHING;

-- 4. Device: TEST-PLC-001 (Listening to Simulator)
-- Simulator is at 'docker-simulator-modbus-1:50502' (from Logs)
INSERT INTO devices (id, tenant_id, site_id, edge_server_id, name, device_type, protocol_id, endpoint, is_enabled, config) 
VALUES (1, 1, 1, 1, 'TEST-PLC-001', 'PLC', 1, 'docker-simulator-modbus-1:50502', 1, '{"slave_id": 1}') ON CONFLICT(id) DO UPDATE SET endpoint='docker-simulator-modbus-1:50502';

-- 5. Data Point: WLS.PV (ID 18, Address 100)
-- Address 100 is confirming toggling in logs
INSERT INTO data_points (id, device_id, name, address, data_type, is_enabled, polling_interval_ms) 
VALUES (18, 1, 'WLS.PV', 100, 'UINT16', 1, 1000) ON CONFLICT(id) DO UPDATE SET name='WLS.PV', address=100;

-- 6. Alarm Rule (High Limit > 0.5)
-- Point toggles 0 and 1. 1 > 0.5 should trigger alarm.
INSERT INTO alarm_rules (tenant_id, name, target_type, target_id, alarm_type, high_limit, severity, is_enabled) 
VALUES (1, 'High Water Level', 'data_point', 18, 'high_limit', 0.5, 'critical', 1) ON CONFLICT DO NOTHING;

-- 7. Export Setup Skipped (using SQLite manual config)
COMMIT;
