-- backend/lib/database/schemas/indexes.sql
-- 멀티테넌트 시스템을 위한 성능 최적화 인덱스

-- ===============================================================================
-- 시스템 레벨 테이블 인덱스 (public 스키마)
-- ===============================================================================

-- 시스템 관리자 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_system_admins_username ON system_admins(username);
CREATE INDEX IF NOT EXISTS idx_system_admins_email ON system_admins(email);
CREATE INDEX IF NOT EXISTS idx_system_admins_active ON system_admins(is_active);
CREATE INDEX IF NOT EXISTS idx_system_admins_last_login ON system_admins(last_login DESC);

-- 테넌트 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_tenants_company_code ON tenants(company_code);
CREATE INDEX IF NOT EXISTS idx_tenants_domain ON tenants(domain);
CREATE INDEX IF NOT EXISTS idx_tenants_subscription_plan ON tenants(subscription_plan);
CREATE INDEX IF NOT EXISTS idx_tenants_subscription_status ON tenants(subscription_status);
CREATE INDEX IF NOT EXISTS idx_tenants_active ON tenants(is_active);
CREATE INDEX IF NOT EXISTS idx_tenants_trial_end ON tenants(trial_end_date);
CREATE INDEX IF NOT EXISTS idx_tenants_created_at ON tenants(created_at DESC);

-- 복합 인덱스 (테넌트 상태 조회용)
CREATE INDEX IF NOT EXISTS idx_tenants_status_plan ON tenants(subscription_status, subscription_plan);
CREATE INDEX IF NOT EXISTS idx_tenants_active_plan ON tenants(is_active, subscription_plan);

-- Edge 서버 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_edge_servers_tenant ON edge_servers(tenant_id);
CREATE INDEX IF NOT EXISTS idx_edge_servers_status ON edge_servers(status);
CREATE INDEX IF NOT EXISTS idx_edge_servers_last_seen ON edge_servers(last_seen DESC);
CREATE INDEX IF NOT EXISTS idx_edge_servers_registration_token ON edge_servers(registration_token);
CREATE INDEX IF NOT EXISTS idx_edge_servers_activation_code ON edge_servers(activation_code);

-- 복합 인덱스 (Edge 서버 모니터링용)
CREATE INDEX IF NOT EXISTS idx_edge_servers_tenant_status ON edge_servers(tenant_id, status);
CREATE INDEX IF NOT EXISTS idx_edge_servers_status_last_seen ON edge_servers(status, last_seen DESC);

-- 사용량 메트릭 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_usage_metrics_tenant_date ON usage_metrics(tenant_id, measurement_date DESC);
CREATE INDEX IF NOT EXISTS idx_usage_metrics_edge_server ON usage_metrics(edge_server_id);
CREATE INDEX IF NOT EXISTS idx_usage_metrics_type ON usage_metrics(metric_type);
CREATE INDEX IF NOT EXISTS idx_usage_metrics_date ON usage_metrics(measurement_date DESC);

-- 복합 인덱스 (사용량 분석용)
CREATE INDEX IF NOT EXISTS idx_usage_metrics_tenant_type_date ON usage_metrics(tenant_id, metric_type, measurement_date DESC);
CREATE INDEX IF NOT EXISTS idx_usage_metrics_edge_type_date ON usage_metrics(edge_server_id, metric_type, measurement_date DESC);
CREATE INDEX IF NOT EXISTS idx_usage_metrics_hourly ON usage_metrics(tenant_id, metric_type, measurement_date, measurement_hour);

-- 과금 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_billing_records_tenant ON billing_records(tenant_id);
CREATE INDEX IF NOT EXISTS idx_billing_records_period_start ON billing_records(billing_period_start DESC);
CREATE INDEX IF NOT EXISTS idx_billing_records_period_end ON billing_records(billing_period_end DESC);
CREATE INDEX IF NOT EXISTS idx_billing_records_status ON billing_records(status);
CREATE INDEX IF NOT EXISTS idx_billing_records_invoice_number ON billing_records(invoice_number);
CREATE INDEX IF NOT EXISTS idx_billing_records_payment_date ON billing_records(payment_date DESC);

-- 복합 인덱스 (과금 조회용)
CREATE INDEX IF NOT EXISTS idx_billing_records_tenant_period ON billing_records(tenant_id, billing_period_start DESC);
CREATE INDEX IF NOT EXISTS idx_billing_records_status_period ON billing_records(status, billing_period_start DESC);

-- 시스템 설정 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_system_settings_category ON system_settings(category);
CREATE INDEX IF NOT EXISTS idx_system_settings_public ON system_settings(is_public);
CREATE INDEX IF NOT EXISTS idx_system_settings_updated_at ON system_settings(updated_at DESC);

-- 드라이버 플러그인 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_driver_plugins_protocol ON driver_plugins(protocol_type);
CREATE INDEX IF NOT EXISTS idx_driver_plugins_version ON driver_plugins(version);
CREATE INDEX IF NOT EXISTS idx_driver_plugins_enabled ON driver_plugins(is_enabled);
CREATE INDEX IF NOT EXISTS idx_driver_plugins_installed_at ON driver_plugins(installed_at DESC);

-- 복합 인덱스 (드라이버 관리용)
CREATE INDEX IF NOT EXISTS idx_driver_plugins_protocol_version ON driver_plugins(protocol_type, version);
CREATE INDEX IF NOT EXISTS idx_driver_plugins_enabled_protocol ON driver_plugins(is_enabled, protocol_type);

-- 스키마 마이그레이션 관련 인덱스
CREATE INDEX IF NOT EXISTS idx_schema_migrations_version ON schema_migrations(version);
CREATE INDEX IF NOT EXISTS idx_schema_migrations_executed_at ON schema_migrations(executed_at DESC);

-- ===============================================================================
-- 시스템 뷰용 최적화 인덱스
-- ===============================================================================

-- 테넌트 요약 뷰 최적화
CREATE INDEX IF NOT EXISTS idx_tenants_summary_optimization ON tenants(is_active, subscription_status, trial_end_date);

-- Edge 서버 요약 뷰 최적화
CREATE INDEX IF NOT EXISTS idx_edge_servers_summary_optimization ON edge_servers(status, last_seen, tenant_id);

-- 월별 사용량 요약 뷰 최적화
CREATE INDEX IF NOT EXISTS idx_usage_monthly_optimization ON usage_metrics(measurement_date, tenant_id, metric_type);

-- 과금 요약 뷰 최적화
CREATE INDEX IF NOT EXISTS idx_billing_summary_optimization ON billing_records(billing_period_end, status, tenant_id);

-- ===============================================================================
-- 대시보드 성능 최적화 인덱스
-- ===============================================================================

-- 시스템 대시보드용 인덱스
CREATE INDEX IF NOT EXISTS idx_dashboard_active_tenants ON tenants(is_active) WHERE is_active = {{TRUE}};
CREATE INDEX IF NOT EXISTS idx_dashboard_trial_tenants ON tenants(subscription_status) WHERE subscription_status = 'trial';
CREATE INDEX IF NOT EXISTS idx_dashboard_active_edges ON edge_servers(status) WHERE status = 'active';
CREATE INDEX IF NOT EXISTS idx_dashboard_online_edges ON edge_servers(status, last_seen) WHERE status = 'active';
CREATE INDEX IF NOT EXISTS idx_dashboard_enabled_drivers ON driver_plugins(is_enabled) WHERE is_enabled = {{TRUE}};
CREATE INDEX IF NOT EXISTS idx_dashboard_pending_bills ON billing_records(status) WHERE status = 'pending';
CREATE INDEX IF NOT EXISTS idx_dashboard_expired_trials ON tenants(subscription_status, trial_end_date) WHERE subscription_status = 'trial';

-- ===============================================================================
-- 검색 및 필터링 최적화 인덱스
-- ===============================================================================

-- 텍스트 검색 최적화 (대소문자 무시)
CREATE INDEX IF NOT EXISTS idx_tenants_company_name_lower ON tenants(LOWER(company_name));
CREATE INDEX IF NOT EXISTS idx_edge_servers_name_lower ON edge_servers(LOWER(server_name));
CREATE INDEX IF NOT EXISTS idx_edge_servers_factory_lower ON edge_servers(LOWER(factory_name));

-- 날짜 범위 검색 최적화
CREATE INDEX IF NOT EXISTS idx_tenants_created_range ON tenants(created_at) WHERE created_at >= '2024-01-01';
CREATE INDEX IF NOT EXISTS idx_edge_servers_last_seen_range ON edge_servers(last_seen) WHERE last_seen IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_usage_metrics_recent ON usage_metrics(measurement_date) WHERE measurement_date >= date('now', '-3 months');

-- ===============================================================================
-- 보고서 및 분석용 인덱스
-- ===============================================================================

-- 월별 사용량 분석용
CREATE INDEX IF NOT EXISTS idx_usage_monthly_analysis ON usage_metrics(
    strftime('%Y-%m', measurement_date),
    tenant_id,
    metric_type
);

-- 일별 사용량 분석용
CREATE INDEX IF NOT EXISTS idx_usage_daily_analysis ON usage_metrics(
    measurement_date,
    tenant_id,
    metric_type,
    metric_value
);

-- 테넌트별 성장률 분석용
CREATE INDEX IF NOT EXISTS idx_tenants_growth_analysis ON tenants(
    subscription_plan,
    created_at,
    is_active
);

-- Edge 서버 가동률 분석용
CREATE INDEX IF NOT EXISTS idx_edge_uptime_analysis ON edge_servers(
    tenant_id,
    status,
    last_seen,
    created_at
);

-- 과금 트렌드 분석용
CREATE INDEX IF NOT EXISTS idx_billing_trend_analysis ON billing_records(
    billing_period_start,
    subscription_plan,
    total_amount,
    status
) WHERE status = 'paid';

-- ===============================================================================
-- 관리 및 유지보수용 인덱스
-- ===============================================================================

-- 데이터 정리용 인덱스
CREATE INDEX IF NOT EXISTS idx_cleanup_old_usage ON usage_metrics(measurement_date) WHERE measurement_date < date('now', '-1 year');
CREATE INDEX IF NOT EXISTS idx_cleanup_old_billing ON billing_records(billing_period_start) WHERE billing_period_start < date('now', '-2 years');

-- 만료된 세션 정리용 (테넌트별 테이블이므로 예시)
-- CREATE INDEX IF NOT EXISTS idx_cleanup_expired_sessions ON user_sessions(expires_at) WHERE expires_at < CURRENT_TIMESTAMP;

-- 비활성 리소스 식별용
CREATE INDEX IF NOT EXISTS idx_inactive_tenants ON tenants(is_active, last_login_attempt) WHERE is_active = {{FALSE}};
CREATE INDEX IF NOT EXISTS idx_stale_edge_servers ON edge_servers(status, last_seen) WHERE status = 'active' AND last_seen < datetime('now', '-1 day');

-- ===============================================================================
-- 모니터링 및 알림용 인덱스
-- ===============================================================================

-- 시스템 상태 모니터링용
CREATE INDEX IF NOT EXISTS idx_monitoring_tenant_health ON tenants(
    is_active,
    subscription_status,
    trial_end_date
) WHERE is_active = {{TRUE}};

-- Edge 서버 상태 모니터링용
CREATE INDEX IF NOT EXISTS idx_monitoring_edge_health ON edge_servers(
    status,
    last_seen,
    tenant_id
) WHERE status IN ('active', 'inactive');

-- 사용량 임계값 모니터링용
CREATE INDEX IF NOT EXISTS idx_monitoring_usage_limits ON usage_metrics(
    tenant_id,
    metric_type,
    metric_value,
    measurement_date
) WHERE measurement_date >= date('now', '-7 days');

-- 과금 알림용
CREATE INDEX IF NOT EXISTS idx_monitoring_billing_alerts ON billing_records(
    status,
    billing_period_end,
    tenant_id
) WHERE status = 'pending';

-- ===============================================================================
-- API 성능 최적화 인덱스
-- ===============================================================================

-- 테넌트 조회 API 최적화
CREATE INDEX IF NOT EXISTS idx_api_tenant_lookup ON tenants(company_code, is_active);
CREATE INDEX IF NOT EXISTS idx_api_tenant_domain_lookup ON tenants(domain, is_active);

-- Edge 서버 조회 API 최적화
CREATE INDEX IF NOT EXISTS idx_api_edge_by_tenant ON edge_servers(tenant_id, status, server_name);
CREATE INDEX IF NOT EXISTS idx_api_edge_by_token ON edge_servers(registration_token, status);

-- 사용량 조회 API 최적화
CREATE INDEX IF NOT EXISTS idx_api_usage_by_tenant_period ON usage_metrics(
    tenant_id,
    measurement_date,
    metric_type
) WHERE measurement_date >= date('now', '-30 days');

-- 과금 조회 API 최적화
CREATE INDEX IF NOT EXISTS idx_api_billing_by_tenant ON billing_records(
    tenant_id,
    billing_period_start DESC,
    status
);

-- ===============================================================================
-- 백업 및 아카이브용 인덱스
-- ===============================================================================

-- 아카이브 대상 식별용
CREATE INDEX IF NOT EXISTS idx_archive_old_usage ON usage_metrics(measurement_date) 
WHERE measurement_date < date('now', '-6 months');

CREATE INDEX IF NOT EXISTS idx_archive_completed_billing ON billing_records(billing_period_start, status) 
WHERE status = 'paid' AND billing_period_start < date('now', '-1 year');

-- 백업 우선순위용
CREATE INDEX IF NOT EXISTS idx_backup_priority_tenants ON tenants(subscription_plan, is_active) 
WHERE subscription_plan = 'enterprise' AND is_active = {{TRUE}};

-- ===============================================================================
-- 파티션 테이블 지원 인덱스 (향후 확장용)
-- ===============================================================================

-- 월별 파티션 지원을 위한 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_usage_partition_monthly ON usage_metrics(
    strftime('%Y-%m', measurement_date),
    tenant_id,
    metric_type,
    measurement_date
);

-- 테넌트별 파티션 지원을 위한 복합 인덱스
CREATE INDEX IF NOT EXISTS idx_usage_partition_tenant ON usage_metrics(
    tenant_id,
    measurement_date,
    metric_type
);

-- ===============================================================================
-- 성능 모니터링용 인덱스
-- ===============================================================================

-- 느린 쿼리 식별용
CREATE INDEX IF NOT EXISTS idx_perf_large_tenants ON tenants(max_edge_servers, max_data_points) 
WHERE max_edge_servers > 10 OR max_data_points > 10000;

-- 높은 사용량 테넌트 식별용
CREATE INDEX IF NOT EXISTS idx_perf_high_usage ON usage_metrics(tenant_id, metric_value, measurement_date) 
WHERE metric_value > 100000;

-- 복잡한 조인 최적화용
CREATE INDEX IF NOT EXISTS idx_perf_tenant_edge_join ON edge_servers(tenant_id, status, created_at);
CREATE INDEX IF NOT EXISTS idx_perf_edge_usage_join ON usage_metrics(edge_server_id, measurement_date);

-- ===============================================================================
-- 완료 메시지
-- ===============================================================================
-- All performance optimization indexes created successfully
-- Total indexes: 80+ for optimal multi-tenant system performance