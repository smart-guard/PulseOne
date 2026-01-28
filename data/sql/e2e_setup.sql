-- ============================================================================
-- PulseOne E2E Test Setup - HDCL-CSP Operations (Exact Restoration)
-- ============================================================================

PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

-- 0. Structural Integrity
CREATE TABLE IF NOT EXISTS export_profile_assignments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER NOT NULL,
    gateway_id INTEGER NOT NULL,
    is_active INTEGER DEFAULT 1,
    assigned_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE CASCADE,
    FOREIGN KEY (gateway_id) REFERENCES edge_servers(id) ON DELETE CASCADE
);

-- 1. Identity Registry
INSERT OR REPLACE INTO edge_servers (id, tenant_id, site_id, server_name, server_type, status, config, is_deleted)
VALUES (6, 1, 1, 'Insite Gateway', 'gateway', 'active', '{"auto_start": true}', 0);

-- 2. Monitoring Targets (TEST-PLC-001)
INSERT OR REPLACE INTO devices (id, tenant_id, site_id, edge_server_id, name, device_type, protocol_id, endpoint, config, is_enabled, is_deleted)
VALUES (11, 1, 1, 1, 'TEST-PLC-001', 'PLC', 1, '172.18.0.10:50502', '{"slave_id": 1}', 1, 0);

-- 3. Operational Data Points (IDs matching S1 and standard schema)
INSERT OR REPLACE INTO data_points (id, device_id, name, description, address, data_type, is_enabled) VALUES 
(1, 11, 'Production_Count', '생산 수량', 1000, 'INT32', 1),
(5, 11, 'Emergency_Stop', '긴급 정지 상태', 1004, 'BOOL', 1),
(6, 11, 'Screen_Status', 'HMI 화면 상태', 1005, 'INT16', 1),
(7, 11, 'Active_Alarms', '현재 활성 알람 수', 1006, 'INT16', 1),
(8, 11, 'User_Level', '현재 로그인 레벨', 1007, 'INT16', 1),
(18, 11, 'WLS.PV', '방재실 수위 현재값', 100, 'BOOL', 1);

-- 4. Export Profile
INSERT OR REPLACE INTO export_profiles (id, name, description, is_enabled)
VALUES (3, 'insite 알람셋', 'HDCL-CSP 운영 환경 알람셋 프로파일', 1);

-- 5. Export Target: Insite Cloud (HTTP Gateway)
INSERT OR REPLACE INTO export_targets (id, profile_id, name, target_type, config, is_enabled, template_id, export_mode)
VALUES (11, 3, 'Insite Cloud (HTTP)', 'http', 
'{
    "url": "http://host.docker.internal:3000/api/mock/insite",
    "method": "POST",
    "headers": {
        "Content-Type": "application/json",
        "x-api-key": "HDCL_E2E_TEST_KEY"
    },
    "timeout": 5000,
    "retry": 3
}', 1, 1, 'on_change');

-- 6. Profile Points Assignment
INSERT OR REPLACE INTO export_profile_points (profile_id, point_id, is_enabled) VALUES
(3, 1, 1), (3, 5, 1), (3, 6, 1), (3, 7, 1), (3, 8, 1), (3, 18, 1);

-- 7. Precision Field Mappings (Target Keys)
INSERT OR REPLACE INTO export_target_mappings (target_id, point_id, target_field_name, target_description, is_enabled) VALUES
(11, 5, 'emergency_stop', 'Emergency Stop - 긴급정지', 1),
(11, 7, 'active_alarms', 'Active Alarms - 활성 알람', 1),
(11, 1, 'production_count', 'Production Count - 생산 수량', 1),
(11, 6, 'screen_status', 'Screen Status - 화면 상태', 1),
(11, 8, 'user_level', 'User Level - 사용자 레벨', 1),
(11, 18, 'ENVS_1.FIFR_1.VDSDC_1:WLS.PV', '방재실 수위 센서', 1);

-- 8. Gateway Assignment
INSERT OR REPLACE INTO export_profile_assignments (profile_id, gateway_id, is_active)
VALUES (3, 6, 1);

-- 9. Alarm Rule: Precise Trigger
INSERT OR REPLACE INTO alarm_rules (id, tenant_id, name, target_type, target_id, alarm_type, trigger_condition, severity, is_enabled)
VALUES (8, 1, 'E2E_Test_WLS_PV', 'data_point', 18, 'digital', 'on_true', 'high', 1);

COMMIT;
