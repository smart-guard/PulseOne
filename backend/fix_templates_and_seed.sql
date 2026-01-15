-- Ensure Tables Exist
CREATE TABLE IF NOT EXISTS manufacturers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL UNIQUE,
    description TEXT,
    website VARCHAR(255),
    logo_url VARCHAR(255),
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS device_models (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    manufacturer_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    model_number VARCHAR(100),
    device_type VARCHAR(50),
    description TEXT,
    image_url VARCHAR(255),
    manual_url VARCHAR(255),
    metadata TEXT,
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (manufacturer_id) REFERENCES manufacturers(id) ON DELETE CASCADE,
    UNIQUE(manufacturer_id, name)
);

CREATE TABLE IF NOT EXISTS template_devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    model_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    protocol_id INTEGER NOT NULL,
    config TEXT NOT NULL,
    polling_interval INTEGER DEFAULT 1000,
    timeout INTEGER DEFAULT 3000,
    is_public INTEGER DEFAULT 1,
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (model_id) REFERENCES device_models(id) ON DELETE CASCADE,
    FOREIGN KEY (protocol_id) REFERENCES protocols(id) ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS template_data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    template_device_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    address INTEGER NOT NULL,
    address_string VARCHAR(255),
    data_type VARCHAR(20) NOT NULL,
    access_mode VARCHAR(10) DEFAULT 'read',
    unit VARCHAR(50),
    scaling_factor REAL DEFAULT 1.0,
    is_writable INTEGER DEFAULT 0,
    is_active INTEGER DEFAULT 1,
    sort_order INTEGER DEFAULT 0,
    metadata TEXT,
    FOREIGN KEY (template_device_id) REFERENCES template_devices(id) ON DELETE CASCADE
);

-- Seed Data
-- Manufacturers
INSERT OR IGNORE INTO manufacturers (id, name, description, is_active) VALUES 
(1, 'Siemens', 'German multinational conglomerate', 1),
(2, 'Rockwell Automation', 'American provider of industrial automation', 1),
(3, 'Schneider Electric', 'French multinational company', 1),
(4, 'Mitsubishi Electric', 'Japanese multinational electronics', 1),
(5, 'LS Electric', 'South Korean provider of electric power', 1);

-- Device Models
INSERT OR IGNORE INTO device_models (id, manufacturer_id, name, device_type, is_active) VALUES 
(1, 1, 'S7-1500', 'PLC', 1),
(2, 2, 'PowerFlex 525', 'Inverter', 1),
(3, 3, 'PowerLogic PM8000', 'Power Meter', 1),
(4, 4, 'MELSEC iQ-R', 'PLC', 1),
(5, 5, 'XGT Series', 'PLC', 1);

-- Template Devices
INSERT OR IGNORE INTO template_devices (id, model_id, name, description, protocol_id, config, polling_interval, timeout, is_public, created_by) VALUES
(1, 3, 'PM8000 (Modbus TCP)', 'Power Meter Template', 1, '{"unit_id": 1}', 1000, 3000, 1, 1),
(2, 2, 'PowerFlex 525 (EtherNet/IP)', 'Standard Drive Template', 4, '{}', 500, 2000, 1, 1),
(3, 1, 'Siemens S7-1500 (Modbus TCP)', 'Standard template for S7-1500 via Modbus TCP', 1, '{"unit_id": 1}', 1000, 3000, 1, 1);

-- Massive Data Points for PM8000 (Template ID 1) - Generating 50 points
INSERT OR IGNORE INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit) VALUES
(1, 'Voltage_A_N', 3000, 'float', 'read', 'V'),
(1, 'Voltage_B_N', 3002, 'float', 'read', 'V'),
(1, 'Voltage_C_N', 3004, 'float', 'read', 'V'),
(1, 'Voltage_A_B', 3020, 'float', 'read', 'V'),
(1, 'Voltage_B_C', 3022, 'float', 'read', 'V'),
(1, 'Voltage_C_A', 3024, 'float', 'read', 'V'),
(1, 'Current_A', 3006, 'float', 'read', 'A'),
(1, 'Current_B', 3008, 'float', 'read', 'A'),
(1, 'Current_C', 3010, 'float', 'read', 'A'),
(1, 'Current_N', 3012, 'float', 'read', 'A'),
(1, 'Current_G', 3014, 'float', 'read', 'A'),
(1, 'Active_Power_Total', 3054, 'float', 'read', 'kW'),
(1, 'Reactive_Power_Total', 3062, 'float', 'read', 'kVAR'),
(1, 'Apparent_Power_Total', 3070, 'float', 'read', 'kVA'),
(1, 'Power_Factor_Total', 3078, 'float', 'read', ''),
(1, 'Frequency', 3108, 'float', 'read', 'Hz'),
(1, 'Active_Energy_Import', 3204, 'double', 'read', 'kWh'),
(1, 'Active_Energy_Export', 3208, 'double', 'read', 'kWh'),
(1, 'Reactive_Energy_Import', 3212, 'double', 'read', 'kVARh'),
(1, 'Reactive_Energy_Export', 3216, 'double', 'read', 'kVARh'),
(1, 'Apparent_Energy_Total', 3220, 'double', 'read', 'kVAh'),
(1, 'Voltage_Unbalance', 3130, 'float', 'read', '%'),
(1, 'Current_Unbalance', 3132, 'float', 'read', '%'),
(1, 'THD_Voltage', 3134, 'float', 'read', '%'),
(1, 'THD_Current', 3136, 'float', 'read', '%'),
(1, 'Demand_Current_A', 3300, 'float', 'read', 'A'),
(1, 'Demand_Current_B', 3302, 'float', 'read', 'A'),
(1, 'Demand_Current_C', 3304, 'float', 'read', 'A'),
(1, 'Peak_Demand_Current_A', 3310, 'float', 'read', 'A'),
(1, 'Peak_Demand_Current_B', 3312, 'float', 'read', 'A'),
(1, 'Peak_Demand_Current_C', 3314, 'float', 'read', 'A'),
(1, 'Demand_Active_Power', 3330, 'float', 'read', 'kW'),
(1, 'Peak_Demand_Active_Power', 3340, 'float', 'read', 'kW'),
(1, 'Digital_Input_1', 100, 'boolean', 'read', ''),
(1, 'Digital_Input_2', 101, 'boolean', 'read', ''),
(1, 'Digital_Input_3', 102, 'boolean', 'read', ''),
(1, 'Digital_Input_4', 103, 'boolean', 'read', ''),
(1, 'Digital_Output_1', 200, 'boolean', 'read_write', ''),
(1, 'Digital_Output_2', 201, 'boolean', 'read_write', ''),
(1, 'Alarm_Over_Voltage', 500, 'boolean', 'read', ''),
(1, 'Alarm_Under_Voltage', 501, 'boolean', 'read', ''),
(1, 'Alarm_Over_Current', 502, 'boolean', 'read', ''),
(1, 'Alarm_Phase_Loss', 503, 'boolean', 'read', ''),
(1, 'Alarm_Freq_High', 504, 'boolean', 'read', ''),
(1, 'Alarm_Freq_Low', 505, 'boolean', 'read', ''),
(1, 'Temp_Internal', 600, 'float', 'read', 'C'),
(1, 'Battery_Voltage', 602, 'float', 'read', 'V'),
(1, 'Operating_Hours', 610, 'uint32', 'read', 'h'),
(1, 'Device_Status_Word', 700, 'uint32', 'read', ''),
(1, 'Error_Code', 702, 'uint32', 'read', '');

-- Massive Data Points for PowerFlex 525 (Template ID 2)
INSERT OR IGNORE INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit) VALUES
(2, 'Output_Frequency', 1, 'float', 'read', 'Hz'),
(2, 'Command_Frequency', 2, 'float', 'read_write', 'Hz'),
(2, 'Output_Current', 3, 'float', 'read', 'A'),
(2, 'Output_Voltage', 4, 'float', 'read', 'V'),
(2, 'DC_Bus_Voltage', 5, 'float', 'read', 'V'),
(2, 'Drive_Status', 6, 'uint32', 'read', ''),
(2, 'Fault_Code', 7, 'uint32', 'read', ''),
(2, 'Process_Display', 8, 'float', 'read', ''),
(2, 'Control_Source', 12, 'uint32', 'read', ''),
(2, 'Contrl_Word', 100, 'uint32', 'write', ''),
(2, 'Dig_In_Status', 13, 'uint32', 'read', ''),
(2, 'Dig_Out_Status', 14, 'uint32', 'read', ''),
(2, 'Elapsed_Run_Time', 18, 'uint32', 'read', 'h'),
(2, 'Drive_Temp', 19, 'float', 'read', 'C'),
(2, 'Torque_Current', 20, 'float', 'read', 'A'),
(2, 'Speed_Feedback', 21, 'float', 'read', 'RPM'),
(2, 'Speed_Reference', 22, 'float', 'read', 'RPM'),
(2, 'Output_Power', 23, 'float', 'read', 'kW'),
(2, 'Power_Factor', 24, 'float', 'read', ''),
(2, 'Motor_Voltage', 25, 'float', 'read', 'V'),
(2, 'Motor_Current', 26, 'float', 'read', 'A'),
(2, 'Accel_Time_1', 40, 'float', 'read_write', 's'),
(2, 'Decel_Time_1', 41, 'float', 'read_write', 's'),
(2, 'Min_Frequency', 42, 'float', 'read_write', 'Hz'),
(2, 'Max_Frequency', 43, 'float', 'read_write', 'Hz'),
(2, 'Stop_Mode', 44, 'uint32', 'read_write', ''),
(2, 'Start_Source', 45, 'uint32', 'read_write', ''),
(2, 'Speed_Ref_Sel', 46, 'uint32', 'read_write', ''),
(2, 'Preset_Freq_0', 50, 'float', 'read_write', 'Hz'),
(2, 'Preset_Freq_1', 51, 'float', 'read_write', 'Hz'),
(2, 'Preset_Freq_2', 52, 'float', 'read_write', 'Hz'),
(2, 'Preset_Freq_3', 53, 'float', 'read_write', 'Hz'),
(2, 'Preset_Freq_4', 54, 'float', 'read_write', 'Hz'),
(2, 'Preset_Freq_5', 55, 'float', 'read_write', 'Hz'),
(2, 'Preset_Freq_6', 56, 'float', 'read_write', 'Hz'),
(2, 'Preset_Freq_7', 57, 'float', 'read_write', 'Hz'),
(2, 'Relay_Out_Sel', 60, 'uint32', 'read_write', ''),
(2, 'Opt_Out_Sel', 61, 'uint32', 'read_write', ''),
(2, 'Analog_Out_Sel', 62, 'uint32', 'read_write', ''),
(2, 'Fly_Start_En', 63, 'boolean', 'read_write', ''),
(2, 'Auto_Rstrt_Tries', 64, 'uint32', 'read_write', ''),
(2, 'Auto_Rstrt_Delay', 65, 'float', 'read_write', 's'),
(2, 'Rev_Disable', 66, 'boolean', 'read_write', ''),
(2, 'Flying_Start_Gain', 67, 'uint32', 'read_write', ''),
(2, 'Flux_Up_Time', 68, 'float', 'read_write', 's'),
(2, 'Comm_Format', 69, 'uint32', 'read_write', ''),
(2, 'Data_In_A1', 70, 'uint32', 'read_write', ''),
(2, 'Data_In_A2', 71, 'uint32', 'read_write', ''),
(2, 'Data_Out_A1', 72, 'uint32', 'read_write', ''),
(2, 'Data_Out_A2', 73, 'uint32', 'read_write', '');

-- Generate additional 50 dummy points for each template to ensure pagination works
WITH RECURSIVE cnt(x) AS (
  SELECT 1
  UNION ALL
  SELECT x+1 FROM cnt WHERE x<50
)
INSERT OR IGNORE INTO template_data_points (template_device_id, name, address, data_type, access_mode, unit)
SELECT 
  t.id as template_device_id,
  'Test_Register_' || printf('%03d', cnt.x) as name,
  10000 + cnt.x as address,
  'UINT16' as data_type,
  'read' as access_mode,
  '' as unit
FROM template_devices t
CROSS JOIN cnt;
