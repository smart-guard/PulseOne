-- =============================================================================
-- expand_master_models_v2.sql
-- Comprehensive Data Expansion and Usage Context
-- =============================================================================

-- 1. Add Data Points for all templates that were previously empty
-- Siemens S7-1200 Expanded (ID: 14)
INSERT OR IGNORE INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit) VALUES
(14, 'CPU_Status', 1, 'UINT16', 'read', ''),
(14, 'Memory_Usage', 10, 'UINT16', 'read', '%'),
(14, 'Temperature', 20, 'FLOAT32', 'read', '°C'),
(14, 'Input_DI0', 100, 'BOOL', 'read', ''),
(14, 'Output_DQ0', 200, 'BOOL', 'read_write', '');

-- LS iS7 Inverter Full (ID: 13)
INSERT OR IGNORE INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit) VALUES
(13, 'Frequency_Command', 1, 'UINT16', 'read_write', 'Hz'),
(13, 'Output_Frequency', 10, 'UINT16', 'read', 'Hz'),
(13, 'Output_Current', 11, 'UINT16', 'read', 'A'),
(13, 'DC_Link_Voltage', 12, 'UINT16', 'read', 'V'),
(13, 'Drive_Status', 13, 'UINT16', 'read', ''),
(13, 'Error_Code', 14, 'UINT16', 'read', '');

-- Omron NX1P Machine Status (ID: 6)
INSERT OR IGNORE INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit) VALUES
(6, 'Sys_RunMode', 0, 'BOOL', 'read', ''),
(6, 'Sys_Error', 1, 'BOOL', 'read', ''),
(6, 'Cycle_Time_Avg', 10, 'UINT32', 'read', 'us'),
(6, 'Total_Counter', 100, 'UINT32', 'read', 'pcs');

-- Keyence KV-8000 (ID: 7)
INSERT OR IGNORE INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit) VALUES
(7, 'CPU_RUN', 0, 'BOOL', 'read', ''),
(7, 'CPU_ERR', 1, 'BOOL', 'read', ''),
(7, 'R000_Start', 1000, 'BOOL', 'read_write', ''),
(7, 'DM000_Result', 2000, 'INT16', 'read', '');

-- Mitsubishi FX5U Basic Monitoring (ID: 5)
-- Already has some, adding more to ensure 10+
INSERT OR IGNORE INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit) VALUES
(5, 'Input_X01', 1, 'BOOL', 'read', ''),
(5, 'Input_X02', 2, 'BOOL', 'read', ''),
(5, 'Input_X03', 3, 'BOOL', 'read', ''),
(5, 'Output_Y01', 101, 'BOOL', 'read_write', ''),
(5, 'Output_Y02', 102, 'BOOL', 'read_write', ''),
(5, 'D0_Internal_Value', 200, 'INT16', 'read_write', '');

-- Add for others (generic)
INSERT OR IGNORE INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit) VALUES
(10, 'Freq_Cmd', 1, 'UINT16', 'read_write', 'Hz'), (10, 'Out_Freq', 2, 'UINT16', 'read', 'Hz'),
(11, 'Run_Status', 1, 'BOOL', 'read', ''), (11, 'Main_Temp', 2, 'FLOAT32', 'read', '°C'),
(12, 'Sys_Ready', 1, 'BOOL', 'read', ''), (12, 'Prod_Count', 2, 'UINT32', 'read', 'pcs'),
(15, 'Logic_Active', 1, 'BOOL', 'read', ''), (15, 'Process_Step', 2, 'UINT16', 'read', ''),
(16, 'Diag_Code', 1, 'UINT16', 'read', ''), (16, 'Health_Score', 2, 'UINT16', 'read', '%'),
(17, 'Load_Peak', 1, 'FLOAT32', 'read', 'kW'), (17, 'Efficiency', 2, 'FLOAT32', 'read', '%'),
(18, 'Billing_Data', 1, 'FLOAT32', 'read', 'kWh'), (18, 'Peak_Demand', 2, 'FLOAT32', 'read', 'kW'),
(19, 'Primary_Active', 1, 'BOOL', 'read', ''), (19, 'Sync_Status', 2, 'BOOL', 'read', '');


-- 2. Add Test Devices to verify 'In Use' status
-- Assuming site_id 1 exists (common in seeds)
INSERT OR IGNORE INTO devices (name, tenant_id, site_id, device_type, manufacturer, model, protocol_id, endpoint, config, template_device_id) VALUES
('Factory_PLC_1', 1, 1, 'PLC', 'Mitsubishi Electric', 'MELSEC FX5U', 1, '192.168.0.101:502', '{"slave_id": 1}', 5),
('Storage_Inverter_1', 1, 1, 'INVERTER', 'ABB', 'ACS880', 1, '192.168.0.110:502', '{"slave_id": 1}', 8),
('Main_Energy_Meter', 1, 1, 'METER', 'Schneider Electric', 'iEM3000', 1, '192.168.0.120:502', '{"slave_id": 1}', 9);

-- 3. Add more diverse Protocols if they don't exist
INSERT OR IGNORE INTO protocols (id, protocol_type, display_name, description, default_port, category) VALUES
(5, 'S7_TCP', 'Siemens S7', 'S7 Communication protocol for Siemens PLCs', 102, 'industrial'),
(6, 'ETHERNET_IP', 'EtherNet/IP', 'Industrial Ethernet protocol using CIP', 44818, 'industrial');

-- 4. Add some specific templates for these protocols
INSERT OR IGNORE INTO template_devices (id, model_id, name, description, protocol_id, config) VALUES
(21, 3, 'S7-1200 Native S7', 'Monitoring Siemens S7-1200 via S7 Protocol', 5, '{"rack": 0, "slot": 1}'),
(22, 11, 'S7-1500 Native S7', 'Monitoring Siemens S7-1500 via S7 Protocol', 5, '{"rack": 0, "slot": 1}'),
(23, 6, 'NX1P EtherNet/IP', 'Omron NX1P Machine Monitoring via EtherNet/IP', 6, '{"input_size": 32, "output_size": 32}');

INSERT OR IGNORE INTO template_device_settings (template_device_id, polling_interval_ms, max_retry_count)
SELECT id, 1000, 3 FROM template_devices WHERE id >= 21;
