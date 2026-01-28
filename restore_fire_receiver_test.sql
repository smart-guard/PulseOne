-- Restore Fire Receiver Test Data
BEGIN TRANSACTION;

-- 1. Create Site for BD 280 (if not exists)
-- The user mentioned BD 280 in previous mappings. In sites table, id 280 might match this.
INSERT OR IGNORE INTO sites (id, tenant_id, name, code, site_type) 
VALUES (280, 1, '1F 방재실', 'FIFR_1', 'room');

-- 2. Create Fire Receiver Device (FIFR_1)
-- Using ID 300 to avoid conflicts
INSERT OR IGNORE INTO devices (id, tenant_id, site_id, edge_server_id, name, device_type, protocol_id, endpoint, is_enabled, config)
VALUES (300, 1, 280, 1, '1F 소방수신기', 'FIRE_RECEIVER', 1, '172.18.0.4:50503', 1, '{"slave_id": 1}');

-- 3. Create Data Points for Wireless Sensors (WLS)
-- ENVS_1.FIFR_1.VDSDC_1:WLS.PV  ,1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 현재값 이상, Digital
INSERT OR IGNORE INTO data_points (id, device_id, name, address, data_type, is_enabled, mapping_key)
VALUES (3001, 300, '무선 센서 현재값', 3001, 'UINT16', 1, 'ENVS_1.FIFR_1.VDSDC_1:WLS.PV');

-- ENVS_1.FIFR_1.VDSDC_1:WLS.SSS ,1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 센서 신호강도 이상, Analog
INSERT OR IGNORE INTO data_points (id, device_id, name, address, data_type, is_enabled, mapping_key)
VALUES (3002, 300, '무선 센서 신호강도', 3002, 'FLOAT', 1, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SSS');

-- ENVS_1.FIFR_1.VDSDC_1:WLS.SBV ,1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 센서 배터리전압 이상, Analog
INSERT OR IGNORE INTO data_points (id, device_id, name, address, data_type, is_enabled, mapping_key)
VALUES (3003, 300, '무선 센서 배터리전압', 3003, 'FLOAT', 1, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SBV');

-- ENVS_1.FIFR_1.VDSDC_1:WLS.SRS ,1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 센서 인식상태 이상, Digital
INSERT OR IGNORE INTO data_points (id, device_id, name, address, data_type, is_enabled, mapping_key)
VALUES (3004, 300, '무선 센서 인식상태', 3004, 'UINT16', 1, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SRS');

-- ENVS_1.FIFR_1.VDSDC_1:WLS.SCS ,1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 센서 통신상태 이상, Digital
INSERT OR IGNORE INTO data_points (id, device_id, name, address, data_type, is_enabled, mapping_key)
VALUES (3005, 300, '무선 센서 통신상태', 3005, 'UINT16', 1, 'ENVS_1.FIFR_1.VDSDC_1:WLS.SCS');

-- 4. Map to Insite Target (Target ID 2)
-- We map these points to target 2 (Insite) using site_id 280 (BD 280)
INSERT OR IGNORE INTO export_target_mappings (target_id, point_id, site_id, target_field_name, is_enabled)
VALUES 
(2, 3001, 280, 'WLS.PV', 1),
(2, 3002, 280, 'WLS.SSS', 1),
(2, 3003, 280, 'WLS.SBV', 1),
(2, 3004, 280, 'WLS.SRS', 1),
(2, 3005, 280, 'WLS.SCS', 1);

-- 5. Update Profile (Profile ID 1) with new points
INSERT OR IGNORE INTO export_profile_points (profile_id, point_id)
VALUES 
(1, 3001),
(1, 3002),
(1, 3003),
(1, 3004),
(1, 3005);

COMMIT;
