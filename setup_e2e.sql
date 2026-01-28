-- E2E Setup Script
INSERT INTO virtual_points (tenant_id, site_id, device_id, scope_type, name, formula, data_type, is_enabled, execution_type, calculation_trigger) 
VALUES (1, 1, 1, 'device', 'NIKE_FIRE_VP_LOGIC', 'fire_raw * 10', 'float', 1, 'javascript', 'onchange');

INSERT INTO virtual_point_inputs (virtual_point_id, variable_name, source_type, source_id) 
SELECT id, 'fire_raw', 'data_point', 20 FROM virtual_points WHERE name='NIKE_FIRE_VP_LOGIC' ORDER BY id DESC LIMIT 1;

INSERT INTO alarm_rules (tenant_id, name, target_type, target_id, alarm_type, high_limit, is_enabled, severity) 
SELECT 1, 'VP_FIRE_ALARM', 'virtual_point', id, 'analog', 5.0, 1, 'critical' FROM virtual_points WHERE name='NIKE_FIRE_VP_LOGIC' ORDER BY id DESC LIMIT 1;

INSERT INTO export_targets (name, target_type, config, is_enabled, export_mode) 
VALUES ('Local Archive File', 'file', '{"base_path": "/app/logs/export", "file_format": "json"}', 1, 'alarm');
