-- BLE Beacon Test Data

-- 1. Insert BLE Protocol
INSERT INTO protocols (
    name, display_name, protocol_type, default_port, description,
    uses_serial, requires_broker, supported_operations, supported_data_types, connection_params,
    category, default_polling_interval, default_timeout, max_concurrent_connections, is_deprecated
) VALUES (
    'BLE_BEACON', 'Bluetooth Low Energy Beacon', 'ble_beacon', NULL, 'BLE Beacon Scanner (iBeacon/Eddystone)',
    0, 0, '["read"]', '["INT16", "STRING"]', '{"adapter": "default", "scan_window_ms": 100}',
    'wireless', 1000, 5000, 100, 0
);

-- 2. Insert BLE Gateway Device (Scanner)
-- Endpoint is the local adapter name (e.g., hci0 or default)
INSERT INTO devices (
    tenant_id, site_id, device_group_id, edge_server_id,
    name, description, device_type, manufacturer, model, serial_number,
    protocol_id, endpoint, config,
    polling_interval, timeout, retry_count, is_enabled
) VALUES (
    1, 1, 1, 1,
    'BLE-GATEWAY-001', 'Lobby Beacon Scanner', 'GATEWAY', 'Generic', 'BlueZ-Adapter', 'BLE001',
    (SELECT id FROM protocols WHERE name = 'BLE_BEACON'), 'default', 
    '{"scan_interval_ms": 200, "rssi_filter": -90}',
    1000, 2000, 0, 1
);

-- 3. Insert Data Points (Beacons)
-- Address String = Beacon UUID (or MAC)
-- Address = 0 (Unused)
INSERT INTO data_points (
    device_id, name, description, address, address_string, data_type,
    access_mode, unit, scaling_factor, is_enabled, polling_interval_ms,
    log_enabled, group_name, tags
) VALUES 
-- Beacon 1 (Simulated Presence)
(
    (SELECT id FROM devices WHERE name = 'BLE-GATEWAY-001'),
    'Lobby_Beacon_RSSI', 'Lobby Entrance Beacon RSSI', 10001, '74278BDA-B644-4520-8F0C-720EAF059935', 'INT16',
    'read', 'dBm', 1.0, 1, 1000,
    1, 'presence', '["rssi", "lobby"]'
),
-- Beacon 2 (Simulated Asset)
(
    (SELECT id FROM devices WHERE name = 'BLE-GATEWAY-001'),
    'Asset_Tag_RSSI', 'Asset Tag 001 RSSI', 10002, 'E2C56DB5-DFFB-48D2-B060-D0F5A71096E0', 'INT16',
    'read', 'dBm', 1.0, 1, 2000,
    1, 'asset_tracking', '["rssi", "asset"]'
);
