-- Clean up existing device and template data
DELETE FROM alarm_occurrences;
DELETE FROM alarm_event_logs;
DELETE FROM virtual_point_history;
DELETE FROM virtual_point_values;
DELETE FROM virtual_point_inputs;
DELETE FROM virtual_points;
DELETE FROM current_values;
DELETE FROM data_history;
DELETE FROM data_points;
DELETE FROM device_status;
DELETE FROM device_settings;
DELETE FROM devices;
DELETE FROM template_data_points;
DELETE FROM template_device_settings;
DELETE FROM template_devices;
DELETE FROM device_models;
DELETE FROM manufacturers;

-- Reset Auto Inc (optional but cleaner)
DELETE FROM sqlite_sequence WHERE name IN ('devices', 'data_points', 'device_models', 'template_devices', 'manufacturers', 'virtual_points', 'alarm_rules');

-- Seed Prerequisites
-- Tenants & Sites
INSERT OR IGNORE INTO tenants (id, company_name, company_code) VALUES (1, 'E2E Test Corp', 'E2E_CORP');
INSERT OR IGNORE INTO sites (id, tenant_id, name, code, site_type) VALUES (1, 1, 'Main Factory', 'FACTORY_01', 'factory');

-- Protocols
INSERT OR IGNORE INTO protocols (id, protocol_type, display_name, is_enabled) VALUES (1, 'MODBUS_TCP', 'Modbus TCP', 1);
INSERT OR IGNORE INTO protocols (id, protocol_type, display_name, is_enabled) VALUES (2, 'MODBUS_RTU', 'Modbus RTU', 1);

-- Manufacturers
INSERT INTO manufacturers (id, name, country, description) VALUES (1, 'PulseOne Industrial', 'Korea', 'Default Manufacturer for Testing');

-- Protocol Mappings (Ensure mappings exist for Modbus TCP)
-- This might be needed depending on how the UI/Backend handles protocol-specific fields
