BEGIN TRANSACTION;

-- 1. Insert Profile (ID 1)
INSERT OR REPLACE INTO export_profiles (id, name, description, data_points, is_enabled, created_at, updated_at)
VALUES (1, 'Demo Export Profile', 'Auto-generated for testing', '[]', 1, datetime('now'), datetime('now'));

-- 2. Insert Target (Profile ID 1)
INSERT OR REPLACE INTO export_targets (id, profile_id, name, target_type, config, export_mode, export_interval, batch_size, is_enabled, created_at, updated_at)
VALUES (1, 1, 'Local MQTT Broker', 'mqtt', '{"host":"localhost","port":1883,"topic":"data/export"}', 'realtime', 10, 100, 1, datetime('now'), datetime('now'));

-- 3. Insert Mapping (Target ID 1, Point ID 1)
INSERT OR REPLACE INTO export_target_mappings (id, target_id, point_id, target_field_name, target_description, conversion_config, is_enabled, created_at)
VALUES (1, 1, 1, 'temp_sensor_01', 'Primary Temperature', '{"factor":1}', 1, datetime('now'));

-- 4. Assign Profile to Gateway (Gateway ID 6, Profile ID 1)
-- Check if assignment exists first to avoid duplicates if no unique constraint
DELETE FROM export_profile_assignments WHERE profile_id = 1 AND gateway_id = 6;
INSERT INTO export_profile_assignments (profile_id, gateway_id, is_active, assigned_at)
VALUES (1, 6, 1, datetime('now'));

COMMIT;
