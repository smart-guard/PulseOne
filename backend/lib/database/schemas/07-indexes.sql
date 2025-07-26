-- =============================================================================
-- backend/lib/database/schemas/07-indexes.sql
-- 성능 최적화 인덱스 (SQLite 버전)
-- =============================================================================

-- 핵심 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_tenants_company_code ON tenants(company_code);
CREATE INDEX IF NOT EXISTS idx_tenants_active ON tenants(is_active);

-- Edge 서버 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_edge_servers_tenant ON edge_servers(tenant_id);
CREATE INDEX IF NOT EXISTS idx_edge_servers_status ON edge_servers(status);
CREATE INDEX IF NOT EXISTS idx_edge_servers_last_seen ON edge_servers(last_seen);

-- 통합 사용자 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_users_tenant ON users(tenant_id);
CREATE INDEX IF NOT EXISTS idx_users_role ON users(role);
CREATE INDEX IF NOT EXISTS idx_users_active ON users(is_active);
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);

-- 사이트 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_sites_tenant ON sites(tenant_id);
CREATE INDEX IF NOT EXISTS idx_sites_parent ON sites(parent_site_id);
CREATE INDEX IF NOT EXISTS idx_sites_type ON sites(site_type);
CREATE INDEX IF NOT EXISTS idx_sites_hierarchy ON sites(hierarchy_level);
CREATE INDEX IF NOT EXISTS idx_sites_active ON sites(is_active);
CREATE INDEX IF NOT EXISTS idx_sites_code ON sites(tenant_id, code);

-- 디바이스 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_device_groups_tenant ON device_groups(tenant_id);
CREATE INDEX IF NOT EXISTS idx_device_groups_site ON device_groups(site_id);
CREATE INDEX IF NOT EXISTS idx_device_groups_parent ON device_groups(parent_group_id);

CREATE INDEX IF NOT EXISTS idx_devices_tenant ON devices(tenant_id);
CREATE INDEX IF NOT EXISTS idx_devices_site ON devices(site_id);
CREATE INDEX IF NOT EXISTS idx_devices_group ON devices(device_group_id);
CREATE INDEX IF NOT EXISTS idx_devices_edge_server ON devices(edge_server_id);
CREATE INDEX IF NOT EXISTS idx_devices_protocol ON devices(protocol_type);
CREATE INDEX IF NOT EXISTS idx_devices_type ON devices(device_type);
CREATE INDEX IF NOT EXISTS idx_devices_enabled ON devices(is_enabled);

-- 데이터 포인트 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_data_points_device ON data_points(device_id);
CREATE INDEX IF NOT EXISTS idx_data_points_enabled ON data_points(is_enabled);
CREATE INDEX IF NOT EXISTS idx_data_points_type ON data_points(data_type);
CREATE INDEX IF NOT EXISTS idx_data_points_address ON data_points(device_id, address);

-- 현재값 조회 최적화
CREATE INDEX IF NOT EXISTS idx_current_values_timestamp ON current_values(timestamp DESC);

-- 가상 포인트 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_virtual_points_tenant ON virtual_points(tenant_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_scope ON virtual_points(scope_type);
CREATE INDEX IF NOT EXISTS idx_virtual_points_site ON virtual_points(site_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_device ON virtual_points(device_id);
CREATE INDEX IF NOT EXISTS idx_virtual_points_enabled ON virtual_points(is_enabled);
CREATE INDEX IF NOT EXISTS idx_virtual_points_category ON virtual_points(category);

-- 가상포인트 입력 매핑 인덱스
CREATE INDEX IF NOT EXISTS idx_vp_inputs_virtual_point ON virtual_point_inputs(virtual_point_id);
CREATE INDEX IF NOT EXISTS idx_vp_inputs_source ON virtual_point_inputs(source_type, source_id);

-- 알람 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_definitions_tenant ON alarm_definitions(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_definitions_site ON alarm_definitions(site_id);
CREATE INDEX IF NOT EXISTS idx_alarm_definitions_enabled ON alarm_definitions(is_enabled);
CREATE INDEX IF NOT EXISTS idx_alarm_definitions_severity ON alarm_definitions(severity);

CREATE INDEX IF NOT EXISTS idx_active_alarms_definition ON active_alarms(alarm_definition_id);
CREATE INDEX IF NOT EXISTS idx_active_alarms_active ON active_alarms(is_active);
CREATE INDEX IF NOT EXISTS idx_active_alarms_triggered ON active_alarms(triggered_at);

CREATE INDEX IF NOT EXISTS idx_alarm_events_definition ON alarm_events(alarm_definition_id);
CREATE INDEX IF NOT EXISTS idx_alarm_events_time ON alarm_events(event_time DESC);

-- 로그 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_system_logs_tenant ON system_logs(tenant_id);
CREATE INDEX IF NOT EXISTS idx_system_logs_level ON system_logs(log_level);
CREATE INDEX IF NOT EXISTS idx_system_logs_module ON system_logs(module);
CREATE INDEX IF NOT EXISTS idx_system_logs_created ON system_logs(created_at DESC);

CREATE INDEX IF NOT EXISTS idx_user_activities_user ON user_activities(user_id);
CREATE INDEX IF NOT EXISTS idx_user_activities_timestamp ON user_activities(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_user_activities_action ON user_activities(action);

CREATE INDEX IF NOT EXISTS idx_communication_logs_device ON communication_logs(device_id);
CREATE INDEX IF NOT EXISTS idx_communication_logs_timestamp ON communication_logs(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_communication_logs_protocol ON communication_logs(protocol);

-- 히스토리 데이터 조회 최적화
CREATE INDEX IF NOT EXISTS idx_data_history_point_time ON data_history(point_id, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_virtual_point_history_point_time ON virtual_point_history(virtual_point_id, timestamp DESC);
