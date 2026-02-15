-- FINAL Corrected Seed E2E Test Data for BLE and ROS (v5)
-- This script sets up the protocol instances, devices, and data points required for full E2E simulation.

-- 1. Setup Protocols
INSERT OR IGNORE INTO protocols (id, protocol_type, display_name, category) VALUES (16, 'BLE_BEACON', 'BLE Beacon', 'iot');
INSERT OR IGNORE INTO protocols (id, protocol_type, display_name, category) VALUES (19, 'ROS_BRIDGE', 'ROS Bridge', 'industrial');

-- 2. Setup Protocol Instances
INSERT OR IGNORE INTO protocol_instances (id, protocol_id, instance_name, is_enabled, status) 
VALUES (1601, 16, 'Main BLE Scanner', 1, 'RUNNING');

INSERT OR IGNORE INTO protocol_instances (id, protocol_id, instance_name, is_enabled, status) 
VALUES (1901, 19, 'Robot Bridge', 1, 'RUNNING');

-- 3. Setup Devices
-- Columns: id, tenant_id, site_id, name, protocol_id, endpoint, config, device_type, is_enabled, protocol_instance_id
INSERT OR IGNORE INTO devices (id, tenant_id, site_id, name, protocol_id, endpoint, config, device_type, is_enabled, protocol_instance_id)
VALUES (16001, 1, 1, 'Simulated Beacon 1', 16, '0.0.0.0', '{"sim_port": "5555"}', 'SENSOR', 1, 1601);

INSERT OR IGNORE INTO devices (id, tenant_id, site_id, name, protocol_id, endpoint, config, device_type, is_enabled, protocol_instance_id)
VALUES (19001, 1, 1, 'Mock Robot Arm', 19, 'host.docker.internal', '{"port": "9090"}', 'ROBOT', 1, 1901);

-- 4. Setup Data Points
INSERT OR IGNORE INTO data_points (id, device_id, name, address, data_type, access_mode)
VALUES (1600101, 16001, 'RSSI', '74278BDA-B644-4520-8F0C-720EAF059935', 'INT16', 'read');

INSERT OR IGNORE INTO data_points (id, device_id, name, address, data_type, access_mode)
VALUES (1900101, 19001, 'Battery Voltage', '/robot/telemetry:battery_voltage', 'FLOAT32', 'read');

INSERT OR IGNORE INTO data_points (id, device_id, name, address, data_type, access_mode)
VALUES (1900102, 19001, 'Linear Velocity', '/robot/telemetry:linear_velocity', 'FLOAT32', 'read');

INSERT OR IGNORE INTO data_points (id, device_id, name, address, data_type, access_mode)
VALUES (1900103, 19001, 'Status', '/robot/telemetry:status', 'STRING', 'read');

-- 5. Export Profile for E2E Verification
INSERT OR IGNORE INTO export_profiles (id, name, description, is_enabled)
VALUES (999, 'E2E Verification Export', 'E2E Test Profile', 1);

-- Fixed: Correct column count (4 columns, 4 values)
INSERT OR IGNORE INTO export_profile_points (profile_id, point_id, display_name, is_enabled) 
VALUES (999, 1600101, 'ble_rssi', 1);
INSERT OR IGNORE INTO export_profile_points (profile_id, point_id, display_name, is_enabled) 
VALUES (999, 1900101, 'robot_battery', 1);
