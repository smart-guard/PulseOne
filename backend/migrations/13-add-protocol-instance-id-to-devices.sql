ALTER TABLE devices ADD COLUMN protocol_instance_id INTEGER REFERENCES protocol_instances(id) ON DELETE SET NULL;
