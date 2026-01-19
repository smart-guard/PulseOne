-- populate_templates.sql
-- Seed data for alarm_rule_templates table

-- Temperature Templates
INSERT INTO alarm_rule_templates (
    tenant_id, name, description, category, template_type, condition_type, 
    condition_template, default_config, severity, message_template, 
    applicable_data_types, is_active, is_system_template
) VALUES (
    1,
    'Temperature High Warning', 
    'Generates a warning when temperature exceeds a threshold.', 
    'temperature', 
    'simple', 
    'threshold', 
    'value > {high_limit}', 
    '{"high_limit": 50, "deadband": 2}', 
    'high', 
    'Warning: Temperature is high at {{value}}°C (Limit: {{high_limit}}°C)', 
    '["FLOAT", "DOUBLE", "INT32", "UINT32"]', 
    1, 
    1
);

INSERT INTO alarm_rule_templates (
    tenant_id, name, description, category, template_type, condition_type, 
    condition_template, default_config, severity, message_template, 
    applicable_data_types, is_active, is_system_template
) VALUES (
    1,
    'Temperature Critical High', 
    'Generates a critical alarm when temperature exceeds critical threshold.', 
    'temperature', 
    'simple', 
    'threshold', 
    'value > {high_limit}', 
    '{"high_limit": 70, "deadband": 5}', 
    'critical', 
    'CRITICAL: Temperature reached {{value}}°C! Immediate action required!', 
    '["FLOAT", "DOUBLE", "INT32", "UINT32"]', 
    1, 
    1
);

-- Pressure Templates
INSERT INTO alarm_rule_templates (
    tenant_id, name, description, category, template_type, condition_type, 
    condition_template, default_config, severity, message_template, 
    applicable_data_types, is_active, is_system_template
) VALUES (
    1,
    'Pressure Overload', 
    'Critical alarm for high pressure.', 
    'pressure', 
    'simple', 
    'threshold', 
    'value > {high_limit}', 
    '{"high_limit": 10.0, "deadband": 0.5}', 
    'critical', 
    'DANGER: Pressure overload detected: {{value}} bar', 
    '["FLOAT", "DOUBLE"]', 
    1, 
    1
);

-- Operational Status (Digital)
INSERT INTO alarm_rule_templates (
    tenant_id, name, description, category, template_type, condition_type, 
    condition_template, default_config, severity, message_template, 
    applicable_data_types, is_active, is_system_template
) VALUES (
    1,
    'Device Fault Status', 
    'Alarm when device reports a fault status (Digital 1).', 
    'electrical', 
    'simple', 
    'digital', 
    'value == {trigger_condition}', 
    '{"trigger_condition": 1}', 
    'critical', 
    'FAULT: Device reports error status!', 
    '["BOOLEAN", "BIT", "INT16"]', 
    1, 
    1
);

-- Flow Rate Templates
INSERT INTO alarm_rule_templates (
    tenant_id, name, description, category, template_type, condition_type, 
    condition_template, default_config, severity, message_template, 
    applicable_data_types, is_active, is_system_template
) VALUES (
    1,
    'Low Flow Warning', 
    'Warning when flow rate drops below minimum.', 
    'flow', 
    'simple', 
    'threshold', 
    'value < {low_limit}', 
    '{"low_limit": 5.0, "deadband": 0.2}', 
    'medium', 
    'Warning: Low flow detected: {{value}} L/min', 
    '["FLOAT", "DOUBLE"]', 
    1, 
    1
);
