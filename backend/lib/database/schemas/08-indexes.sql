-- =============================================================================
-- backend/lib/database/schemas/07-indexes.sql
-- 성능 최적화 인덱스 (SQLite 버전) - 2025-08-14 최신 업데이트
-- PulseOne v2.1.0 완전 최적화 - 실시간 조회 및 대용량 데이터 지원
-- =============================================================================

-- =============================================================================
-- 🔥🔥🔥 핵심 테이블 복합 인덱스 (고성능 쿼리 최적화)
-- =============================================================================

-- 테넌트 관련 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_tenants_status_plan ON tenants(subscription_status, subscription_plan);
CREATE INDEX IF NOT EXISTS idx_tenants_active_billing ON tenants(is_active, next_billing_date);
CREATE INDEX IF NOT EXISTS idx_tenants_trial_end ON tenants(trial_end_date) WHERE trial_end_date IS NOT NULL;

-- Edge 서버 관련 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_edge_servers_tenant_status ON edge_servers(tenant_id, status);
CREATE INDEX IF NOT EXISTS idx_edge_servers_heartbeat_status ON edge_servers(last_heartbeat DESC, status);
CREATE INDEX IF NOT EXISTS idx_edge_servers_performance ON edge_servers(cpu_usage, memory_usage, status);
CREATE INDEX IF NOT EXISTS idx_edge_servers_location ON edge_servers(tenant_id, factory_name, location);

-- 사용자 관련 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_users_tenant_role_active ON users(tenant_id, role, is_active);
CREATE INDEX IF NOT EXISTS idx_users_login_activity ON users(last_login DESC, is_active);
CREATE INDEX IF NOT EXISTS idx_users_security_status ON users(is_locked, failed_login_attempts, is_active);
CREATE INDEX IF NOT EXISTS idx_users_email_verification ON users(email_verified, is_active);

-- 사용자 세션 관련 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_user_sessions_user_active ON user_sessions(user_id, is_active);
CREATE INDEX IF NOT EXISTS idx_user_sessions_expires_active ON user_sessions(expires_at DESC, is_active);
CREATE INDEX IF NOT EXISTS idx_user_sessions_token_expires ON user_sessions(token_hash, expires_at);

-- 사이트 관련 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_sites_tenant_hierarchy ON sites(tenant_id, hierarchy_level, sort_order);
CREATE INDEX IF NOT EXISTS idx_sites_parent_type ON sites(parent_site_id, site_type);
CREATE INDEX IF NOT EXISTS idx_sites_edge_active ON sites(edge_server_id, is_active);
CREATE INDEX IF NOT EXISTS idx_sites_location_active ON sites(tenant_id, country, city, is_active);

-- =============================================================================
-- 🔥🔥🔥 디바이스 및 데이터포인트 고성능 인덱스
-- =============================================================================

-- 디바이스 그룹 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_device_groups_tenant_site_type ON device_groups(tenant_id, site_id, group_type);
CREATE INDEX IF NOT EXISTS idx_device_groups_parent_active ON device_groups(parent_group_id, is_active);

-- 디바이스 관련 복합 인덱스 (핵심 성능 최적화)
CREATE INDEX IF NOT EXISTS idx_devices_tenant_site_enabled ON devices(tenant_id, site_id, is_enabled);
CREATE INDEX IF NOT EXISTS idx_devices_protocol_type_enabled ON devices(protocol_type, device_type, is_enabled);
CREATE INDEX IF NOT EXISTS idx_devices_edge_enabled ON devices(edge_server_id, is_enabled);
CREATE INDEX IF NOT EXISTS idx_devices_group_protocol ON devices(device_group_id, protocol_type);
CREATE INDEX IF NOT EXISTS idx_devices_simulation_enabled ON devices(is_simulation_mode, is_enabled);

-- 디바이스 상태 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_device_status_connection_comm ON device_status(connection_status, last_communication DESC);
CREATE INDEX IF NOT EXISTS idx_device_status_error_response ON device_status(error_count, response_time);
CREATE INDEX IF NOT EXISTS idx_device_status_performance ON device_status(throughput_ops_per_sec DESC, uptime_percentage DESC);

-- 데이터 포인트 관련 핵심 인덱스 (🔥 가장 중요한 성능 최적화)
CREATE INDEX IF NOT EXISTS idx_data_points_device_enabled_log ON data_points(device_id, is_enabled, log_enabled);
CREATE INDEX IF NOT EXISTS idx_data_points_type_enabled ON data_points(data_type, is_enabled);
CREATE INDEX IF NOT EXISTS idx_data_points_group_type ON data_points(group_name, data_type);
CREATE INDEX IF NOT EXISTS idx_data_points_polling_enabled ON data_points(polling_interval_ms, is_enabled);
CREATE INDEX IF NOT EXISTS idx_data_points_log_interval ON data_points(log_enabled, log_interval_ms);
CREATE INDEX IF NOT EXISTS idx_data_points_alarm_enabled ON data_points(alarm_enabled, is_enabled);
CREATE INDEX IF NOT EXISTS idx_data_points_writable_enabled ON data_points(is_writable, is_enabled);

-- 현재값 관련 핵심 인덱스 (🔥 실시간 조회 최적화)
CREATE INDEX IF NOT EXISTS idx_current_values_quality_timestamp ON current_values(quality_code, value_timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_current_values_alarm_active ON current_values(alarm_active, alarm_state);
CREATE INDEX IF NOT EXISTS idx_current_values_type_quality ON current_values(value_type, quality);
CREATE INDEX IF NOT EXISTS idx_current_values_updated_quality ON current_values(updated_at DESC, quality_code);

-- =============================================================================
-- 🔥🔥🔥 가상포인트 시스템 고성능 인덱스
-- =============================================================================

-- 가상포인트 관련 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_virtual_points_tenant_scope_enabled ON virtual_points(tenant_id, scope_type, is_enabled);
CREATE INDEX IF NOT EXISTS idx_virtual_points_site_category_enabled ON virtual_points(site_id, category, is_enabled);
CREATE INDEX IF NOT EXISTS idx_virtual_points_trigger_interval ON virtual_points(calculation_trigger, calculation_interval);
CREATE INDEX IF NOT EXISTS idx_virtual_points_execution_type_enabled ON virtual_points(execution_type, is_enabled);
CREATE INDEX IF NOT EXISTS idx_virtual_points_script_library ON virtual_points(script_library_id, is_enabled);

-- 가상포인트 입력 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_vp_inputs_vp_source_type ON virtual_point_inputs(virtual_point_id, source_type, source_id);
CREATE INDEX IF NOT EXISTS idx_vp_inputs_source_processing ON virtual_point_inputs(source_type, source_id, data_processing);
CREATE INDEX IF NOT EXISTS idx_vp_inputs_variable_required ON virtual_point_inputs(virtual_point_id, variable_name, is_required);

-- 가상포인트 값 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_vp_values_calculated_quality ON virtual_point_values(last_calculated DESC, quality);
CREATE INDEX IF NOT EXISTS idx_vp_values_stale_threshold ON virtual_point_values(is_stale, staleness_threshold_ms);
CREATE INDEX IF NOT EXISTS idx_vp_values_alarm_quality ON virtual_point_values(alarm_active, quality_code);

-- 가상포인트 실행 이력 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_vp_execution_vp_time_result ON virtual_point_execution_history(virtual_point_id, execution_time DESC, result_type);
CREATE INDEX IF NOT EXISTS idx_vp_execution_trigger_success ON virtual_point_execution_history(trigger_source, success, execution_time DESC);
CREATE INDEX IF NOT EXISTS idx_vp_execution_duration_time ON virtual_point_execution_history(execution_duration_ms DESC, execution_time DESC);

-- =============================================================================
-- 🔥🔥🔥 알람 시스템 고성능 인덱스
-- =============================================================================

-- 알람 규칙 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_rules_tenant_target_enabled ON alarm_rules(tenant_id, target_type, target_id, is_enabled);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_type_severity_enabled ON alarm_rules(alarm_type, severity, is_enabled);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_priority_enabled ON alarm_rules(priority, is_enabled);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_notification_enabled ON alarm_rules(notification_enabled, is_enabled);
CREATE INDEX IF NOT EXISTS idx_alarm_rules_shelved_enabled ON alarm_rules(is_shelved, is_enabled);

-- 알람 발생 복합 인덱스 (🔥 실시간 알람 대시보드 최적화)
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_tenant_state_time ON alarm_occurrences(tenant_id, state, occurrence_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_rule_state_time ON alarm_occurrences(rule_id, state, occurrence_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_severity_state_time ON alarm_occurrences(severity, state, occurrence_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_acknowledged_time ON alarm_occurrences(acknowledged_time DESC) WHERE acknowledged_time IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_cleared_time ON alarm_occurrences(cleared_time DESC) WHERE cleared_time IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_notification_count ON alarm_occurrences(notification_count, last_notification_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_escalation_level ON alarm_occurrences(escalation_level, escalation_time DESC);

-- 알람 그룹 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_groups_tenant_type_active ON alarm_groups(tenant_id, group_type, is_active);
CREATE INDEX IF NOT EXISTS idx_alarm_group_members_group_role ON alarm_group_members(group_id, member_role);

-- =============================================================================
-- 🔥🔥🔥 로깅 시스템 고성능 인덱스
-- =============================================================================

-- 시스템 로그 복합 인덱스 (🔥 로그 검색 최적화)
CREATE INDEX IF NOT EXISTS idx_system_logs_level_module_time ON system_logs(log_level, module, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_system_logs_tenant_level_time ON system_logs(tenant_id, log_level, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_system_logs_user_module_time ON system_logs(user_id, module, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_system_logs_request_correlation ON system_logs(request_id, correlation_id);
CREATE INDEX IF NOT EXISTS idx_system_logs_error_time ON system_logs(created_at DESC) WHERE log_level IN ('ERROR', 'FATAL');

-- 사용자 활동 복합 인덱스 (🔥 감사 로그 최적화)
CREATE INDEX IF NOT EXISTS idx_user_activities_tenant_action_time ON user_activities(tenant_id, action, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_user_activities_user_resource_time ON user_activities(user_id, resource_type, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_user_activities_resource_action_time ON user_activities(resource_type, resource_id, action, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_user_activities_success_time ON user_activities(success, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_user_activities_risk_time ON user_activities(risk_score DESC, timestamp DESC);

-- 통신 로그 복합 인덱스 (🔥 프로토콜 성능 분석 최적화)
CREATE INDEX IF NOT EXISTS idx_communication_logs_device_protocol_time ON communication_logs(device_id, protocol, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_communication_logs_protocol_success_time ON communication_logs(protocol, success, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_communication_logs_direction_operation_time ON communication_logs(direction, operation, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_communication_logs_response_time_success ON communication_logs(response_time DESC, success);
CREATE INDEX IF NOT EXISTS idx_communication_logs_error_protocol_time ON communication_logs(protocol, timestamp DESC) WHERE success = 0;

-- =============================================================================
-- 🔥🔥🔥 시계열 데이터 최적화 인덱스
-- =============================================================================

-- 데이터 히스토리 복합 인덱스 (🔥 시계열 쿼리 최적화)
CREATE INDEX IF NOT EXISTS idx_data_history_point_timestamp_quality ON data_history(point_id, timestamp DESC, quality);
CREATE INDEX IF NOT EXISTS idx_data_history_timestamp_source ON data_history(timestamp DESC, source);
CREATE INDEX IF NOT EXISTS idx_data_history_quality_timestamp ON data_history(quality, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_data_history_change_type_time ON data_history(change_type, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_data_history_compressed_time ON data_history(is_compressed, timestamp DESC);

-- 가상포인트 히스토리 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_vp_history_point_timestamp_quality ON virtual_point_history(virtual_point_id, timestamp DESC, quality);
CREATE INDEX IF NOT EXISTS idx_vp_history_trigger_timestamp ON virtual_point_history(trigger_reason, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_vp_history_calculation_time ON virtual_point_history(calculation_time_ms DESC, timestamp DESC);

-- 알람 이벤트 로그 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_event_logs_occurrence_type_time ON alarm_event_logs(occurrence_id, event_type, event_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_event_logs_rule_type_time ON alarm_event_logs(rule_id, event_type, event_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_event_logs_tenant_type_time ON alarm_event_logs(tenant_id, event_type, event_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_event_logs_user_time ON alarm_event_logs(user_id, event_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_event_logs_escalation_time ON alarm_event_logs(escalation_level, event_time DESC);

-- 성능 로그 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_performance_logs_category_name_timestamp ON performance_logs(metric_category, metric_name, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_performance_logs_hostname_component_time ON performance_logs(hostname, component, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_performance_logs_value_time ON performance_logs(metric_value DESC, timestamp DESC);

-- =============================================================================
-- 🔥🔥🔥 스크립트 라이브러리 최적화 인덱스
-- =============================================================================

-- 스크립트 라이브러리 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_script_library_tenant_category_active ON script_library(tenant_id, category, is_active);
CREATE INDEX IF NOT EXISTS idx_script_library_system_public_active ON script_library(is_system, is_public, is_active);
CREATE INDEX IF NOT EXISTS idx_script_library_usage_rating ON script_library(usage_count DESC, rating DESC);
CREATE INDEX IF NOT EXISTS idx_script_library_version_active ON script_library(version, is_active);

-- 스크립트 사용 이력 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_script_usage_script_context_time ON script_usage_history(script_id, usage_context, used_at DESC);
CREATE INDEX IF NOT EXISTS idx_script_usage_tenant_success_time ON script_usage_history(tenant_id, success, used_at DESC);
CREATE INDEX IF NOT EXISTS idx_script_usage_vp_time ON script_usage_history(virtual_point_id, used_at DESC);

-- =============================================================================
-- 🔥🔥🔥 특수 목적 최적화 인덱스
-- =============================================================================

-- 즐겨찾기 최적화
CREATE INDEX IF NOT EXISTS idx_user_favorites_user_type_target ON user_favorites(user_id, target_type, target_id);
CREATE INDEX IF NOT EXISTS idx_user_favorites_user_sort ON user_favorites(user_id, sort_order);

-- 알림 설정 최적화
CREATE INDEX IF NOT EXISTS idx_user_notification_email_enabled ON user_notification_settings(email_enabled, alarm_notifications);
CREATE INDEX IF NOT EXISTS idx_user_notification_sms_enabled ON user_notification_settings(sms_enabled, alarm_notifications);

-- 레시피 관리 최적화
CREATE INDEX IF NOT EXISTS idx_recipes_tenant_category_active ON recipes(tenant_id, category, is_active);
CREATE INDEX IF NOT EXISTS idx_recipes_validation_approval ON recipes(is_validated, requires_approval);
CREATE INDEX IF NOT EXISTS idx_recipes_execution_count ON recipes(execution_count DESC, last_executed DESC);

-- 스케줄러 최적화
CREATE INDEX IF NOT EXISTS idx_schedules_tenant_type_enabled ON schedules(tenant_id, schedule_type, is_enabled);
CREATE INDEX IF NOT EXISTS idx_schedules_next_execution ON schedules(next_execution_time) WHERE is_enabled = 1 AND is_paused = 0;
CREATE INDEX IF NOT EXISTS idx_schedules_action_type_enabled ON schedules(action_type, is_enabled);

-- JavaScript 함수 최적화
CREATE INDEX IF NOT EXISTS idx_js_functions_tenant_category_enabled ON javascript_functions(tenant_id, category, is_enabled);
CREATE INDEX IF NOT EXISTS idx_js_functions_system_usage ON javascript_functions(is_system, usage_count DESC);

-- =============================================================================
-- 🔥🔥🔥 부분 인덱스 (조건부 인덱스) - SQLite 고급 최적화
-- =============================================================================

-- 활성 레코드만을 위한 부분 인덱스 (저장공간 최적화)
CREATE INDEX IF NOT EXISTS idx_devices_active_only ON devices(tenant_id, site_id, protocol_type) WHERE is_enabled = 1;
CREATE INDEX IF NOT EXISTS idx_data_points_log_enabled_only ON data_points(device_id, log_interval_ms) WHERE log_enabled = 1;
CREATE INDEX IF NOT EXISTS idx_virtual_points_enabled_only ON virtual_points(tenant_id, calculation_trigger) WHERE is_enabled = 1;
CREATE INDEX IF NOT EXISTS idx_alarm_rules_enabled_only ON alarm_rules(tenant_id, target_type, target_id) WHERE is_enabled = 1;

-- 에러 레코드를 위한 부분 인덱스
CREATE INDEX IF NOT EXISTS idx_communication_logs_errors_only ON communication_logs(device_id, protocol, timestamp DESC) WHERE success = 0;
CREATE INDEX IF NOT EXISTS idx_device_status_errors_only ON device_status(device_id, error_count DESC) WHERE error_count > 0;

-- 알람 활성 레코드만을 위한 부분 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_active_only ON alarm_occurrences(tenant_id, severity, occurrence_time DESC) WHERE state = 'active';
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_unacknowledged ON alarm_occurrences(rule_id, occurrence_time DESC) WHERE acknowledged_time IS NULL;

-- 최근 데이터를 위한 부분 인덱스 (성능 최적화)
CREATE INDEX IF NOT EXISTS idx_current_values_recent_good ON current_values(point_id, value_timestamp DESC) WHERE quality_code = 1;
CREATE INDEX IF NOT EXISTS idx_data_history_recent_month ON data_history(point_id, timestamp DESC) WHERE timestamp >= datetime('now', '-1 month');

-- =============================================================================
-- 🔥🔥🔥 전문 검색 인덱스 (FTS - Full Text Search)
-- =============================================================================

-- 가상 테이블을 이용한 전문 검색 (SQLite FTS5)
-- 시스템 로그 메시지 검색용
CREATE VIRTUAL TABLE IF NOT EXISTS system_logs_fts USING fts5(
    message, 
    module, 
    error_message,
    content='system_logs',
    content_rowid='id'
);

-- 사용자 활동 검색용
CREATE VIRTUAL TABLE IF NOT EXISTS user_activities_fts USING fts5(
    resource_name,
    old_values,
    new_values,
    error_message,
    content='user_activities',
    content_rowid='id'
);

-- 알람 메시지 검색용
CREATE VIRTUAL TABLE IF NOT EXISTS alarm_occurrences_fts USING fts5(
    alarm_message,
    trigger_condition,
    acknowledge_comment,
    clear_comment,
    content='alarm_occurrences',
    content_rowid='id'
);

-- =============================================================================
-- 🔥🔥🔥 인덱스 유지보수 및 최적화 스크립트
-- =============================================================================

-- 인덱스 통계 업데이트 (성능 향상을 위해 주기적으로 실행)
-- ANALYZE;

-- 인덱스 사용 통계 확인 쿼리 (개발/튜닝 시 사용)
/*
-- 인덱스 사용 현황 확인
SELECT name, tbl_name, sql FROM sqlite_master WHERE type = 'index' AND tbl_name LIKE '%alarm%' ORDER BY tbl_name, name;

-- 테이블별 인덱스 개수 확인
SELECT tbl_name, COUNT(*) as index_count FROM sqlite_master WHERE type = 'index' GROUP BY tbl_name ORDER BY index_count DESC;

-- 미사용 인덱스 찾기 (PRAGMA 명령 사용)
-- PRAGMA index_info(index_name);
-- PRAGMA index_list(table_name);
*/

-- =============================================================================
-- 📝 인덱스 설계 원칙 및 가이드라인
-- =============================================================================
/*
🎯 인덱스 설계 원칙:
1. WHERE 절에 자주 사용되는 컬럼 우선
2. ORDER BY 절에 사용되는 컬럼 포함
3. JOIN 조건에 사용되는 컬럼 우선
4. 카디널리티가 높은 컬럼을 앞쪽에 배치
5. 복합 인덱스는 3-4개 컬럼까지만 권장

🔥 고성능 쿼리 패턴:
- 실시간 대시보드: tenant_id + 상태 + 시간순
- 시계열 조회: point_id + timestamp DESC
- 알람 모니터링: state + severity + occurrence_time DESC
- 감사 로그: user_id/resource + action + timestamp DESC

⚡ 성능 최적화 팁:
- 부분 인덱스로 저장공간 절약
- 복합 인덱스로 커버링 인덱스 구현
- FTS5로 전문 검색 성능 향상
- ANALYZE로 통계 업데이트
*/