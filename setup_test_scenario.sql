-- setup_test_scenario.sql
-- 0. Ensure Export Gateway and Targets exist
INSERT OR IGNORE INTO export_gateways (id, tenant_id, name, description, ip_address, status) 
VALUES (1, 1, 'Local Export Gateway', 'Default Local Gateway', '127.0.0.1', 'active');

INSERT OR IGNORE INTO export_targets (id, name, target_type, config, export_mode, template_id)
VALUES (3, 'Local Archive File', 'file', '{"base_path": "/app/logs/export", "file_format": "json"}', 'alarm', 5);

-- Disable all devices except the test one
UPDATE devices SET is_enabled = 0 WHERE id != 12;

-- 1. Clean up existing test device and mappings
DELETE FROM export_target_mappings WHERE point_id IN (SELECT id FROM data_points WHERE device_id = 12);
DELETE FROM data_points WHERE device_id = 12;
DELETE FROM devices WHERE id = 12;

-- 2. Create test device
INSERT INTO devices (
    id, tenant_id, site_id, name, device_type, protocol_id, endpoint, config, is_enabled, edge_server_id, polling_interval
) VALUES (
    12, 1, 1, 'TEST-MODBUS-INSITE', 'PLC', 1, '172.18.0.4:50502', '{"slave_id": 1}', 1, 1, 2000
);

-- 3. Insert Data Points
INSERT INTO data_points (id, device_id, name, address, mapping_key, data_type, access_mode, alarm_enabled)
VALUES (28, 12, 'WLS.PV', 100, 'ENVS_1.FIFR_1.VDSDC_1:WLS.PV', 'BOOL', 'read', 1);

INSERT INTO data_points (id, device_id, name, address, mapping_key, data_type, access_mode, alarm_enabled)
VALUES (29, 12, 'WLS.SSS', 200, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SSS', 'INT32', 'read', 0);

INSERT INTO data_points (id, device_id, name, address, mapping_key, data_type, access_mode, alarm_enabled)
VALUES (30, 12, 'WLS.SBV', 201, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SBV', 'INT32', 'read', 0);

INSERT INTO data_points (id, device_id, name, address, mapping_key, data_type, access_mode, alarm_enabled)
VALUES (31, 12, 'WLS.SRS', 101, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SRS', 'BOOL', 'read', 1);

INSERT INTO data_points (id, device_id, name, address, mapping_key, data_type, access_mode, alarm_enabled)
VALUES (32, 12, 'WLS.SCS', 102, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SCS', 'BOOL', 'read', 1);

-- 4. Setup Alarm Rules
DELETE FROM alarm_rules WHERE target_type = 'data_point' AND target_id IN (28, 29, 30, 31, 32);
INSERT INTO alarm_rules (tenant_id, name, target_type, target_id, alarm_type, trigger_condition, severity, is_enabled)
VALUES (1, 'Fire Detection (PV)', 'data_point', 28, 'digital', 'on_true', 'critical', 1);

INSERT INTO alarm_rules (tenant_id, name, target_type, target_id, alarm_type, trigger_condition, severity, is_enabled)
VALUES (1, 'Recognition Status (SRS)', 'data_point', 31, 'digital', 'on_true', 'high', 1);

INSERT INTO alarm_rules (tenant_id, name, target_type, target_id, alarm_type, trigger_condition, severity, is_enabled)
VALUES (1, 'Communication Status (SCS)', 'data_point', 32, 'digital', 'on_true', 'high', 1);

-- 5. Setup Export Target Mappings for ALL Targets
INSERT OR REPLACE INTO export_target_mappings (target_id, point_id, target_field_name, site_id, is_enabled)
SELECT t.id, 28, 'ENVS_1.FIFR_1.VDSDC_1:WLS.PV', 280, 1 FROM export_targets t;

INSERT OR REPLACE INTO export_target_mappings (target_id, point_id, target_field_name, site_id, is_enabled)
SELECT t.id, 29, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SSS', 280, 1 FROM export_targets t;

INSERT OR REPLACE INTO export_target_mappings (target_id, point_id, target_field_name, site_id, is_enabled)
SELECT t.id, 30, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SBV', 280, 1 FROM export_targets t;

INSERT OR REPLACE INTO export_target_mappings (target_id, point_id, target_field_name, site_id, is_enabled)
SELECT t.id, 31, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SRS', 280, 1 FROM export_targets t;

INSERT OR REPLACE INTO export_target_mappings (target_id, point_id, target_field_name, site_id, is_enabled)
SELECT t.id, 32, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SCS', 280, 1 FROM export_targets t;

-- 6. Update Profile JSON
UPDATE export_profiles 
SET data_points = '[{"id":28,"name":"WLS.PV","device":"TEST-MODBUS-INSITE","address":100,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.PV"},{"id":29,"name":"WLS.SSS","device":"TEST-MODBUS-INSITE","address":200,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SSS"},{"id":30,"name":"WLS.SBV","device":"TEST-MODBUS-INSITE","address":201,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SBV"},{"id":31,"name":"WLS.SRS","device":"TEST-MODBUS-INSITE","address":101,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SRS"},{"id":32,"name":"WLS.SCS","device":"TEST-MODBUS-INSITE","address":102,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SCS"}]'
WHERE id = 1;
