-- 1. Ensure Protocol exists (Required for Device)
INSERT OR IGNORE INTO protocols (id, protocol_type, display_name, category) 
VALUES (1, 'MODBUS_TCP', 'Modbus TCP', 'industrial');

-- 2. Ensure Tenant exists (Required for Device, Alarm)
INSERT OR IGNORE INTO tenants (id, company_name, company_code) 
VALUES (1, 'Default Tenant', 'DEF');

-- 3. Ensure Device exists
-- device_type must be in ('PLC', 'HMI', 'SENSOR', 'GATEWAY', 'METER', 'CONTROLLER', 'ROBOT', 'INVERTER', 'DRIVE', 'SWITCH')
-- protocol_id=1
INSERT OR IGNORE INTO devices (
    id, name, device_type, site_id, protocol_id, endpoint, config, is_enabled, tenant_id
) VALUES (
    1, 'Simulation Device', 'PLC', 1, 1, 'simulator-modbus:502', '{}', 1, 1
);

-- 4. Insert Data Point (ID 101)
INSERT OR REPLACE INTO data_points (
    id, device_id, name, address, address_string, data_type, access_mode, 
    is_enabled, log_enabled, alarm_enabled
) VALUES (
    101, 1, 'Trigger Point', 100, '100', 'UINT16', 'read_write', 
    1, 1, 1
);

-- 5. Insert Alarm Rule
INSERT OR REPLACE INTO alarm_rules (
    id, tenant_id, name, target_type, target_id, alarm_type, 
    trigger_condition, severity, is_enabled
) VALUES (
    1, 1, 'Test Trigger Alarm', 'data_point', 101, 'digital', 
    'on_true', 'high', 1
);

-- 6. Insert Payload Template (Updated with user's structure + system_type)
INSERT OR REPLACE INTO payload_templates (
    id, name, system_type, template_json
) VALUES (
    1, 'Standard JSON Format', 'insite',
    '{
        "bd": "{{building_id}}", 
        "ty": "num", 
        "nm": "{{nm}}", 
        "vl": "{{vl}}", 
        "tm": "{{tm}}", 
        "st": "{{st}}", 
        "al": "{{al}}", 
        "des": "{{des}}",
        "il": "10",
        "xl": "_",
        "mi": [10, 20, 30],
        "mx": [80, 90, 100]
    }'
);

-- 7. Insert Export Profile ("insite 알람" causes conflict, using safe name)
INSERT OR REPLACE INTO export_profiles (id, name, description, is_enabled) 
VALUES (1, 'Test Profile Unique', 'Profile for Verification', 1);

-- 8. Assign Profile to Gateway 6
INSERT OR REPLACE INTO export_profile_assignments (profile_id, gateway_id, is_active)
VALUES (1, 6, 1);

-- 9. Insert Export Target
INSERT OR REPLACE INTO export_targets (
    id, name, target_type, profile_id, config, is_enabled, export_mode, template_id
) VALUES (
    1, 'Insite Cloud', 'http', 1, 
    '{"url":"http://host.docker.internal:3000/api/insite", "export_mode":"alarm", "format":"json"}', 
    1, 'alarm', 1
);

-- 10. Insert Export Target Mapping (building_id = 280)
INSERT OR REPLACE INTO export_target_mappings (
    target_id, point_id, target_field_name, target_description, building_id, is_enabled
) VALUES (
    1, 101, 'Bldg280_P101', 'Mapped Point for Bldg 280', 280, 1
);
