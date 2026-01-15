-- Seed Protocol Connection Schemas
-- This enables the Frontend to dynamically render required fields for each protocol

UPDATE protocols SET connection_params = '{
  "unit_id": { "type": "number", "label": "Unit ID", "default": 1, "required": true, "placeholder": "1" }
}' WHERE protocol_type = 'MODBUS_TCP';

UPDATE protocols SET connection_params = '{
  "slave_id": { "type": "number", "label": "Slave ID", "default": 1, "required": true, "min": 1, "max": 247 },
  "baud_rate": { "type": "select", "label": "Baud Rate", "options": [9600, 19200, 38400, 57600, 115200], "default": 9600 },
  "parity": { "type": "select", "label": "Parity", "options": ["N", "E", "O"], "default": "N" },
  "data_bits": { "type": "select", "label": "Data Bits", "options": [7, 8], "default": 8 },
  "stop_bits": { "type": "select", "label": "Stop Bits", "options": [1, 2], "default": 1 }
}' WHERE protocol_type = 'MODBUS_RTU';

UPDATE protocols SET connection_params = '{
  "client_id": { "type": "string", "label": "Client ID", "placeholder": "leave empty for auto", "required": false },
  "topic_prefix": { "type": "string", "label": "Topic Prefix", "default": "", "required": false },
  "use_tls": { "type": "boolean", "label": "Use TLS", "default": false }
}' WHERE protocol_type = 'MQTT';

UPDATE protocols SET connection_params = '{
  "device_instance": { "type": "number", "label": "Device Instance", "default": 1, "required": true, "placeholder": "1" }
}' WHERE protocol_type = 'BACNET';

UPDATE protocols SET connection_params = '{
  "node_id_format": { "type": "string", "label": "Node ID Format", "default": "ns=2;s=", "required": false, "placeholder": "ns=2;s=" }
}' WHERE protocol_type = 'OPC_UA';

UPDATE protocols SET connection_params = '{
  "bridge_port": { "type": "number", "label": "Bridge Port", "default": 9090, "required": false, "placeholder": "9090" }
}' WHERE protocol_type = 'ROS';
