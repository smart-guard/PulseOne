-- =============================================================================
-- 04-indexes.sql - 인덱스 생성
-- =============================================================================

-- 디바이스 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_devices_tenant_id ON devices(tenant_id);
CREATE INDEX IF NOT EXISTS idx_devices_site_id ON devices(site_id);
CREATE INDEX IF NOT EXISTS idx_devices_protocol_type ON devices(protocol_type);
CREATE INDEX IF NOT EXISTS idx_devices_device_type ON devices(device_type);
CREATE INDEX IF NOT EXISTS idx_devices_is_enabled ON devices(is_enabled);

-- 데이터포인트 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_data_points_tenant_id ON data_points(tenant_id);
CREATE INDEX IF NOT EXISTS idx_data_points_device_id ON data_points(device_id);
CREATE INDEX IF NOT EXISTS idx_data_points_is_enabled ON data_points(is_enabled);

-- 가상포인트 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_virtual_points_tenant_id ON virtual_points(tenant_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_is_enabled ON virtual_points(is_enabled);

-- 알람 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_rules_tenant_id ON alarm_rules(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_device_id ON alarm_rules(device_id);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_is_enabled ON alarm_rules(is_enabled);

CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_tenant_id ON alarm_occurrences(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_state ON alarm_occurrences(state);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_severity ON alarm_occurrences(severity);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_occurrence_time ON alarm_occurrences(occurrence_time);

-- 사용자 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_users_tenant_id ON users(tenant_id);
CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
CREATE INDEX IF NOT EXISTS idx_users_is_active ON users(is_active);

-- 세션 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_user_sessions_user_id ON user_sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_user_sessions_session_token ON user_sessions(session_token);
CREATE INDEX IF NOT EXISTS idx_user_sessions_expires_at ON user_sessions(expires_at);

-- 디바이스 상태 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_device_status_connection_status ON device_status(connection_status);
CREATE INDEX IF NOT EXISTS idx_device_status_last_communication ON device_status(last_communication);

-- 사이트 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_sites_tenant_id ON sites(tenant_id);
CREATE INDEX IF NOT EXISTS idx_sites_edge_server_id ON sites(edge_server_id);
CREATE INDEX IF NOT EXISTS idx_sites_is_active ON sites(is_active);