SELECT id, server_type, subscription_mode FROM edge_servers WHERE id=6;
UPDATE edge_servers SET subscription_mode='all' WHERE id=6;
SELECT id, server_type, subscription_mode FROM edge_servers WHERE id=6;
