-- Seed more diverse and complex Template Devices (Master Models)
-- Table Structure: manufacturers -> device_models -> template_devices -> template_data_points

-- 1. LS Electric iS7 Inverter (Modbus RTU)
INSERT OR IGNORE INTO device_models (manufacturer_id, name, model_number, device_type, manual_url, description)
VALUES (1, 'iS7 Series', 'iS7', 'INVERTER', 'https://www.lselectric.co.kr/products/industrial-automation/inverter/is7', 'High-performance inverter for motor control. Factory Default Slave ID: 1, 9600bps, 8N1.');

INSERT INTO template_devices (model_id, name, description, protocol_id, config, polling_interval)
SELECT id, 'iS7 High Performance Inverter', 'LS Electric iS7 Series for Motor Control', 2, '{"slave_id": 1, "baud_rate": 9600, "parity": "N"}', 1000
FROM device_models WHERE name = 'iS7 Series' AND manufacturer_id = 1;

-- Data points for iS7
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit, scaling_factor)
SELECT id, 'Operation State', 1, 'UINT16', 'read', '', 1.0 FROM template_devices WHERE name = 'iS7 High Performance Inverter';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit, scaling_factor)
SELECT id, 'Output Frequency', 2, 'UINT16', 'read', 'Hz', 0.01 FROM template_devices WHERE name = 'iS7 High Performance Inverter';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit, scaling_factor)
SELECT id, 'Output Current', 3, 'UINT16', 'read', 'A', 0.1 FROM template_devices WHERE name = 'iS7 High Performance Inverter';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit, scaling_factor)
SELECT id, 'Frequency Command', 10, 'UINT16', 'read_write', 'Hz', 0.01 FROM template_devices WHERE name = 'iS7 High Performance Inverter';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, is_writable)
SELECT id, 'Forward Run', 0, 'BOOLEAN', 'write', 1 FROM template_devices WHERE name = 'iS7 High Performance Inverter';

-- 2. Siemens S7-1200 Pro (Modbus TCP) - 50+ points
INSERT OR IGNORE INTO device_models (manufacturer_id, name, model_number, device_type, manual_url, description)
VALUES (2, 'S7-1200 Series', 'S7-1200', 'PLC', 'https://support.industry.siemens.com/cs/ww/en/view/109741593', 'Compact modular PLC system. Supports Modbus TCP communication via Unit ID 1.');

INSERT INTO template_devices (model_id, name, description, protocol_id, config, polling_interval)
SELECT id, 'S7-1200 Heavy Duty Controller', 'Siemens PLC with massive IO mapping', 1, '{"unit_id": 1}', 500
FROM device_models WHERE name = 'S7-1200 Series' AND manufacturer_id = 2;

-- Bulk points for S7-1200 (50 points)
-- Part 1: Sensors (20 points)
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 01', 2001, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 02', 2002, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 03', 2003, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 04', 2004, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 05', 2005, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 06', 2006, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 07', 2007, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 08', 2008, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 09', 2009, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 10', 2010, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 11', 2011, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 12', 2012, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 13', 2013, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 14', 2014, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 15', 2015, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 16', 2016, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 17', 2017, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 18', 2018, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 19', 2019, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Sensor Node 20', 2020, 'INT16', 'read', 'unit' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';

-- Part 2: Status bits & Control (31+ points)
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Internal Temp ' || printf('%02d', i), 5000 + i, 'INT16', 'read', '°C'
FROM template_devices JOIN (SELECT 1 AS i UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9 UNION SELECT 10)
WHERE name = 'S7-1200 Heavy Duty Controller';

INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT id, 'Relay Group ' || i, 6000 + i, 'BOOLEAN', 'read', ''
FROM template_devices JOIN (SELECT 'A' AS i UNION SELECT 'B' UNION SELECT 'C' UNION SELECT 'D' UNION SELECT 'E' UNION SELECT 'F' UNION SELECT 'G' UNION SELECT 'H')
WHERE name = 'S7-1200 Heavy Duty Controller';

INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode, scaling_factor)
SELECT id, 'Cycle Counter ' || i, 7000 + i, 'UINT32', 'read', 1.0
FROM template_devices JOIN (SELECT 1 AS i UNION SELECT 3 UNION SELECT 5 UNION SELECT 7 UNION SELECT 9 UNION SELECT 11 UNION SELECT 13 UNION SELECT 15 UNION SELECT 17 UNION SELECT 19)
WHERE name = 'S7-1200 Heavy Duty Controller';

INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode)
SELECT id, 'Emergency Stop', 9000, 'BOOLEAN', 'read' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode)
SELECT id, 'Fault Reset', 9001, 'BOOLEAN', 'write' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';
INSERT INTO template_data_points (template_device_id, name, address, data_type, access_mode)
SELECT id, 'System Start', 9002, 'BOOLEAN', 'write' FROM template_devices WHERE name = 'S7-1200 Heavy Duty Controller';

-- 3. OPC UA CNC Machine (OPC_UA)
INSERT OR IGNORE INTO device_models (manufacturer_id, name, model_number, device_type, manual_url, description)
VALUES (3, 'CNC Mill Series', 'MillMaster 5000', 'MACHINE', 'https://example.com/manuals/millmaster5000', 'Industrial CNC Milling machine with native OPC UA server support.');

INSERT INTO template_devices (model_id, name, description, protocol_id, config, polling_interval)
SELECT id, 'MillMaster 5000 CNC', 'High-end CNC Milling Machine via OPC UA', 5, '{"node_id_format": "ns=2;s="}', 200
FROM device_models WHERE name = 'CNC Mill Series' AND manufacturer_id = 3;

INSERT INTO template_data_points (template_device_id, name, address_string, data_type, access_mode, unit, address)
SELECT id, 'Spindle Speed', 'Machine/Spindle/Speed', 'FLOAT32', 'read', 'RPM', 0 FROM template_devices WHERE name = 'MillMaster 5000 CNC';
INSERT INTO template_data_points (template_device_id, name, address_string, data_type, access_mode, unit, address)
SELECT id, 'X-Axis Position', 'Machine/Axes/X/Position', 'FLOAT32', 'read', 'mm', 0 FROM template_devices WHERE name = 'MillMaster 5000 CNC';

-- 4. Honeywell BACnet Controller (BACNET)
INSERT OR IGNORE INTO device_models (manufacturer_id, name, model_number, device_type, manual_url, description)
VALUES (9, 'ComfortPoint Series', 'CP-V1', 'HVAC', 'https://buildings.honeywell.com/us/en/products/building-automation/controllers/comfortpoint-open', 'Building automation controller for HVAC units.');

INSERT INTO template_devices (model_id, name, description, protocol_id, config, polling_interval)
SELECT id, 'Honeywell ComfortPoint', 'BACnet/IP HVAC Controller', 4, '{"device_instance": 1234}', 5000
FROM device_models WHERE name = 'ComfortPoint Series' AND manufacturer_id = 9;

INSERT INTO template_data_points (template_device_id, name, address_string, data_type, access_mode, unit, address)
SELECT id, 'Room Temp', 'AnalogInput:1', 'FLOAT32', 'read', '°C', 0 FROM template_devices WHERE name = 'Honeywell ComfortPoint';

-- 5. Inactive Experimental Model
INSERT OR IGNORE INTO device_models (manufacturer_id, name, model_number, device_type)
VALUES (10, 'Exp Proto Series', 'PROTO-V1', 'EXPERIMENTAL');

INSERT INTO template_devices (model_id, name, description, protocol_id, config, polling_interval, is_public)
SELECT id, 'Experimental Prototype V1', 'Maintenance mode - do not use', 1, '{}', 1000, 0
FROM device_models WHERE name = 'Exp Proto Series' AND manufacturer_id = 10;
