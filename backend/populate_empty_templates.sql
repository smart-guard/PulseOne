-- Populate missing template data points for better demo/usage
-- Only for templates that have 0 points

WITH RECURSIVE cnt(x) AS (
  SELECT 1
  UNION ALL
  SELECT x+1 FROM cnt WHERE x<10
)
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit, scaling_factor, scaling_offset)
SELECT 
  t.id as template_device_id,
  CASE 
    WHEN t.protocol_id = 1 THEN 'Modbus_Register_' || printf('%02d', cnt.x)
    WHEN t.protocol_id = 4 THEN 'BACnet_Object_' || printf('%02d', cnt.x)
    WHEN t.protocol_id = 5 THEN 'OPCUA_Node_' || printf('%02d', cnt.x)
    ELSE 'Point_' || printf('%02d', cnt.x)
  END as name,
  40000 + cnt.x as address,
  'FLOAT32' as data_type,
  'read' as access_mode,
  'unit' as unit,
  1.0 as scaling_factor,
  0.0 as scaling_offset
FROM template_devices t
CROSS JOIN cnt
WHERE t.id NOT IN (SELECT DISTINCT template_device_id FROM template_data_points);
