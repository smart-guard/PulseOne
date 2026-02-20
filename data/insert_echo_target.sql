INSERT OR IGNORE INTO export_targets (tenant_id, name, target_type, config, is_enabled)
VALUES (1, 'local-echo', 'HTTP',
  '[{"url":"http://host.docker.internal:9999","method":"POST","headers":{"Content-Type":"application/json"}}]',
  1);
SELECT id, name FROM export_targets WHERE name='local-echo';
