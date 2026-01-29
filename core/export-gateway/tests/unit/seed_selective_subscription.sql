-- Selective Subscription Test Data Seed Script

-- 1. Ensure schema exists (Basic tables from seed_data.sql)
-- (Assuming tables already created or will be created by this script)

-- 2. Clean existing test data (Optional, for repeatability)
DELETE FROM export_profile_assignments WHERE gateway_id = 10;
DELETE FROM export_profiles WHERE id = 100;
DELETE FROM export_targets WHERE id = 1000;
DELETE FROM export_target_mappings WHERE target_id = 1000;
DELETE FROM data_points WHERE id IN (9001, 9002);
DELETE FROM devices WHERE id IN (8001, 8002);

-- 3. Insert Devices
-- DEV_001: Assigned Device
INSERT INTO devices (id, tenant_id, site_id, edge_server_id, name, device_type, protocol_id, endpoint, is_enabled, config)
VALUES (8001, 1, 1, 1, 'VERIFY_DEV_001', 'MODBUS', 1, '127.0.0.1:502', 1, '{}');

-- DEV_002: Not Assigned Device
INSERT INTO devices (id, tenant_id, site_id, edge_server_id, name, device_type, protocol_id, endpoint, is_enabled, config)
VALUES (8002, 1, 1, 1, 'VERIFY_DEV_002', 'MODBUS', 1, '127.0.0.1:503', 1, '{}');

-- 4. Insert Data Points
-- Point for DEV_001
INSERT INTO data_points (id, device_id, name, address, data_type, is_enabled)
VALUES (9001, 8001, 'POINT_ASSIGND', 100, 'INT16', 1);

-- Point for DEV_002
INSERT INTO data_points (id, device_id, name, address, data_type, is_enabled)
VALUES (9002, 8002, 'POINT_IGNORED', 100, 'INT16', 1);

-- 5. Insert Export Profile
INSERT INTO export_profiles (id, name, is_enabled)
VALUES (100, 'Selective Test Profile', 1);

-- 6. Insert Export Target
INSERT INTO export_targets (id, profile_id, name, target_type, config, is_enabled)
VALUES (1000, 100, 'Test Target', 'file', '{"path":"/tmp/test.json"}', 1);

-- 7. Insert Export Target Mapping (Target 1000 -> Point 9001 (DEV_001))
INSERT INTO export_target_mappings (target_id, point_id, is_enabled)
VALUES (1000, 9001, 1);

-- 8. Assign Profile to Gateway 10
-- Note: gateway_id in export_profile_assignments references edge_servers.id (based on schema)
-- Let's make sure edge_server with id=10 exists
INSERT OR IGNORE INTO edge_servers (id, tenant_id, server_name, server_type, status)
VALUES (10, 1, 'Test Gateway 10', 'gateway', 'active');

INSERT INTO export_profile_assignments (profile_id, gateway_id, is_active)
VALUES (100, 10, 1);

-- Verification Query (What the code will run)
-- SELECT DISTINCT dp.device_id FROM data_points dp ...
