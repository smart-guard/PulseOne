-- Nike Building Simulator Setup (Corrected Schema)
-- 1. Register Device
INSERT OR REPLACE INTO devices (id, tenant_id, site_id, name, description, device_type, manufacturer, model, protocol_id, endpoint, config, is_enabled)
VALUES (100, 1, 3, 'NIKE-SIM-001', 'Nike Building Fire Receiver Simulator', 'PLC', 'Simulator', 'Modbus-TCP-V1', 1, '127.0.0.1:50502', '{"slave_id": 1}', 1);

-- 2. Register Digital Points (Coils)
INSERT OR REPLACE INTO data_points (id, device_id, name, description, address, data_type, is_enabled)
VALUES 
(201, 100, 'ENVS_1.FIFR_1.VDSDC_1:WLS.PV', '1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 현재값 이상', 100, 'Digital', 1),
(204, 100, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SRS', '1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 센서 인식상태 이상', 101, 'Digital', 1),
(205, 100, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SCS', '1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 센서 통신상태 이상', 102, 'Digital', 1);

-- 3. Register Analog Points (Holdings)
INSERT OR REPLACE INTO data_points (id, device_id, name, description, address, data_type, is_enabled)
VALUES 
(202, 100, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SSS', '1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 센서 신호강도 이상', 200, 'Analog', 1),
(203, 100, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SBV', '1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 센서 배터리전압 이상', 201, 'Analog', 1);
