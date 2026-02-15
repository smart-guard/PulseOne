-- FINAL Corrected Seed E2E Test Data for BLE and ROS (v12)
-- Independent collectors, fixed ROS mapping paths, and isolated alarms.

-- 1. Setup Protocols
INSERT OR REPLACE INTO protocols (id, protocol_type, display_name, category) VALUES (6, 'BLE_BEACON', 'BLE Beacon', 'iot');
INSERT OR REPLACE INTO protocols (id, protocol_type, display_name, category) VALUES (7, 'ROS', 'ROS Bridge', 'industrial');

-- 2. Setup Edge Servers for Isolation
INSERT OR REPLACE INTO edge_servers (id, tenant_id, server_name, status) VALUES (16, 1, 'BLE-Collector', 'active');
INSERT OR REPLACE INTO edge_servers (id, tenant_id, server_name, status) VALUES (19, 1, 'ROS-Collector', 'active');

-- 3. Setup Protocol Instances
INSERT OR REPLACE INTO protocol_instances (id, protocol_id, instance_name, is_enabled, status) 
VALUES (601, 6, 'Main BLE Scanner', 1, 'RUNNING');

INSERT OR REPLACE INTO protocol_instances (id, protocol_id, instance_name, is_enabled, status) 
VALUES (701, 7, 'Robot Bridge', 1, 'RUNNING');

-- 4. Setup Devices
INSERT OR REPLACE INTO devices (id, tenant_id, site_id, name, protocol_id, endpoint, config, device_type, is_enabled, protocol_instance_id, edge_server_id)
VALUES (16001, 1, 1, 'Simulated Beacon 1', 6, '127.0.0.1', '{"sim_port": "5555"}', 'SENSOR', 1, 601, 16);

INSERT OR REPLACE INTO devices (id, tenant_id, site_id, name, protocol_id, endpoint, config, device_type, is_enabled, protocol_instance_id, edge_server_id)
VALUES (19001, 1, 1, 'Mock Robot Arm', 7, '127.0.0.1', '{"port": "9090"}', 'ROBOT', 1, 701, 19);

-- 5. Setup Data Points (Correct ROS Mapping: address_string=Topic, mapping_key=Path without leading slash)
INSERT OR REPLACE INTO data_points (id, device_id, name, address, address_string, mapping_key, data_type, access_mode, is_enabled)
VALUES (1600101, 16001, 'RSSI', 101, '74278BDA-B644-4520-8F0C-720EAF059935', NULL, 'INT16', 'read', 1);

INSERT OR REPLACE INTO data_points (id, device_id, name, address, address_string, mapping_key, data_type, access_mode, is_enabled)
VALUES (1900101, 19001, 'Battery Voltage', 101, '/robot/telemetry', 'battery_voltage', 'FLOAT32', 'read', 1);

INSERT OR REPLACE INTO data_points (id, device_id, name, address, address_string, mapping_key, data_type, access_mode, is_enabled)
VALUES (1900102, 19001, 'Linear Velocity', 102, '/robot/telemetry', 'linear_velocity', 'FLOAT32', 'read', 1);

INSERT OR REPLACE INTO data_points (id, device_id, name, address, address_string, mapping_key, data_type, access_mode, is_enabled)
VALUES (1900103, 19001, 'Status', 103, '/robot/telemetry', 'status', 'STRING', 'read', 1);

-- 6. Setup Alarm Rules (Use data_point as target_type)
INSERT OR REPLACE INTO alarm_rules (id, tenant_id, name, target_type, target_id, alarm_type, low_limit, severity, is_enabled) 
VALUES (161, 1, 'BLE Low RSSI', 'data_point', 1600101, 'low', -60, 'HIGH', 1);

INSERT OR REPLACE INTO alarm_rules (id, tenant_id, name, target_type, target_id, alarm_type, low_limit, severity, is_enabled) 
VALUES (191, 1, 'ROS Low Battery', 'data_point', 1900101, 'low', 27, 'CRITICAL', 1);
