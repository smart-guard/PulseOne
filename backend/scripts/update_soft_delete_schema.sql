-- Add is_deleted to manufacturers
ALTER TABLE manufacturers ADD COLUMN is_deleted INTEGER DEFAULT 0;

-- Add is_deleted to device_models
ALTER TABLE device_models ADD COLUMN is_deleted INTEGER DEFAULT 0;

-- Add is_deleted to template_devices
ALTER TABLE template_devices ADD COLUMN is_deleted INTEGER DEFAULT 0;

-- Add is_deleted to data_points
ALTER TABLE data_points ADD COLUMN is_deleted INTEGER DEFAULT 0;

-- Add is_deleted to sites
ALTER TABLE sites ADD COLUMN is_deleted INTEGER DEFAULT 0;

-- Add is_deleted to tenants
ALTER TABLE tenants ADD COLUMN is_deleted INTEGER DEFAULT 0;

-- Add is_deleted to protocols
ALTER TABLE protocols ADD COLUMN is_deleted INTEGER DEFAULT 0;

-- Add is_deleted to device_status (if missing)
-- ALTER TABLE device_status ADD COLUMN is_deleted INTEGER DEFAULT 0; 

-- Add is_deleted to alarm_rules
ALTER TABLE alarm_rules ADD COLUMN is_deleted INTEGER DEFAULT 0;
