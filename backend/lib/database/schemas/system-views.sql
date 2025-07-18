-- backend/lib/database/schemas/system-views.sql
-- 시스템 레벨 뷰 및 관리 함수들

-- ===============================================================================
-- 시스템 뷰 생성 (관리자용)
-- ===============================================================================

-- 테넌트 요약 뷰
CREATE OR REPLACE VIEW tenant_summary AS
SELECT 
    t.id,
    t.company_name,
    t.company_code,
    t.subscription_plan,
    t.subscription_status,
    t.max_edge_servers,
    t.max_data_points,
    t.max_users,
    COUNT(es.id) as active_edge_servers,
    t.created_at,
    CASE 
        WHEN t.trial_end_date IS NOT NULL AND t.trial_end_date < CURRENT_TIMESTAMP 
        THEN {{TRUE}}
        ELSE {{FALSE}}
    END as trial_expired,
    CASE
        WHEN t.subscription_status = 'active' AND t.is_active = {{TRUE}} THEN 'healthy'
        WHEN t.subscription_status = 'trial' AND t.trial_end_date > CURRENT_TIMESTAMP THEN 'trial'
        WHEN t.subscription_status = 'suspended' OR t.is_active = {{FALSE}} THEN 'suspended'
        ELSE 'warning'
    END as health_status
FROM tenants t
LEFT JOIN edge_servers es ON t.id = es.tenant_id AND es.status = 'active'
GROUP BY t.id, t.company_name, t.company_code, t.subscription_plan, 
         t.subscription_status, t.max_edge_servers, t.max_data_points, 
         t.max_users, t.created_at, t.trial_end_date, t.subscription_status, t.is_active;

-- Edge 서버 상태 요약 뷰
CREATE OR REPLACE VIEW edge_server_summary AS
SELECT 
    es.id,
    es.server_name,
    es.factory_name,
    es.location,
    es.status,
    es.last_seen,
    es.version,
    t.company_name,
    t.company_code,
    CASE 
        WHEN es.last_seen IS NULL THEN 'never_connected'
        WHEN es.last_seen < datetime('now', '-5 minutes') THEN 'offline'  -- SQLite
        -- WHEN es.last_seen < CURRENT_TIMESTAMP - INTERVAL '5 minutes' THEN 'offline'  -- PostgreSQL
        ELSE 'online'
    END as connectivity_status,
    CASE 
        WHEN es.last_seen IS NULL THEN NULL
        ELSE (julianday('now') - julianday(es.last_seen)) * 24 * 60  -- SQLite minutes calculation
        -- ELSE EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - es.last_seen))/60  -- PostgreSQL
    END as minutes_since_last_seen
FROM edge_servers es
JOIN tenants t ON es.tenant_id = t.id
WHERE es.status = 'active';

-- 월별 사용량 요약 뷰
CREATE OR REPLACE VIEW monthly_usage_summary AS
SELECT 
    um.tenant_id,
    t.company_name,
    strftime('%Y-%m', um.measurement_date) as month,  -- SQLite
    -- DATE_TRUNC('month', um.measurement_date) as month,  -- PostgreSQL
    um.metric_type,
    SUM(um.metric_value) as total_usage,
    AVG(um.metric_value) as avg_daily_usage,
    MAX(um.metric_value) as peak_daily_usage
FROM usage_metrics um
JOIN tenants t ON um.tenant_id = t.id
WHERE um.measurement_date >= date('now', '-12 months')  -- SQLite
-- WHERE um.measurement_date >= CURRENT_DATE - INTERVAL '12 months'  -- PostgreSQL
GROUP BY um.tenant_id, t.company_name, strftime('%Y-%m', um.measurement_date), um.metric_type
-- GROUP BY um.tenant_id, t.company_name, DATE_TRUNC('month', um.measurement_date), um.metric_type  -- PostgreSQL
ORDER BY month DESC, t.company_name, um.metric_type;

-- 과금 요약 뷰
CREATE OR REPLACE VIEW billing_summary AS
SELECT 
    br.tenant_id,
    t.company_name,
    t.subscription_plan,
    br.billing_period_start,
    br.billing_period_end,
    br.total_amount,
    br.status,
    br.invoice_number,
    br.payment_date,
    CASE 
        WHEN br.status = 'pending' AND br.billing_period_end < date('now', '-30 days') THEN 'overdue'
        WHEN br.status = 'pending' AND br.billing_period_end < date('now') THEN 'due'
        WHEN br.status = 'paid' THEN 'paid'
        ELSE br.status
    END as payment_status
FROM billing_records br
JOIN tenants t ON br.tenant_id = t.id
ORDER BY br.billing_period_start DESC;

-- 시스템 상태 대시보드 뷰
CREATE OR REPLACE VIEW system_dashboard AS
SELECT 
    (SELECT COUNT(*) FROM tenants WHERE is_active = {{TRUE}}) as total_active_tenants,
    (SELECT COUNT(*) FROM tenants WHERE subscription_status = 'trial') as trial_tenants,
    (SELECT COUNT(*) FROM edge_servers WHERE status = 'active') as total_active_edge_servers,
    (SELECT COUNT(*) FROM edge_servers WHERE status = 'active' AND last_seen > datetime('now', '-5 minutes')) as online_edge_servers,
    (SELECT COUNT(*) FROM driver_plugins WHERE is_enabled = {{TRUE}}) as active_driver_plugins,
    (SELECT COUNT(*) FROM billing_records WHERE status = 'pending') as pending_invoices,
    (SELECT COALESCE(SUM(total_amount), 0) FROM billing_records WHERE status = 'pending') as pending_revenue,
    (SELECT COUNT(*) FROM tenants WHERE trial_end_date < CURRENT_TIMESTAMP AND subscription_status = 'trial') as expired_trials;

-- ===============================================================================
-- 과금 관련 함수들
-- ===============================================================================

-- 월별 과금 계산 함수 (SQLite 호환)
CREATE OR REPLACE FUNCTION calculate_monthly_bill(
    p_tenant_id TEXT,  -- SQLite uses TEXT for UUID
    p_billing_month DATE
)
RETURNS DECIMAL AS $$
DECLARE
    tenant_plan TEXT;
    base_fee DECIMAL := 0;
    edge_server_count INTEGER;
    data_points_used BIGINT;
    api_requests_used BIGINT;
    total_fee DECIMAL := 0;
    
    -- 요금표
    starter_base_fee DECIMAL := 99.00;
    professional_base_fee DECIMAL := 299.00;
    enterprise_base_fee DECIMAL := 999.00;
    
    edge_server_fee_per_unit DECIMAL := 50.00;
    data_points_overage_fee DECIMAL := 0.01; -- per 1000 data points
    api_requests_overage_fee DECIMAL := 0.001; -- per 1000 requests
BEGIN
    -- 테넌트 플랜 조회
    SELECT subscription_plan INTO tenant_plan
    FROM tenants 
    WHERE id = p_tenant_id;
    
    -- 기본 요금 설정
    CASE tenant_plan
        WHEN 'starter' THEN base_fee := starter_base_fee;
        WHEN 'professional' THEN base_fee := professional_base_fee;
        WHEN 'enterprise' THEN base_fee := enterprise_base_fee;
        ELSE base_fee := 0;
    END CASE;
    
    -- Edge 서버 수 계산
    SELECT COUNT(*) INTO edge_server_count
    FROM edge_servers
    WHERE tenant_id = p_tenant_id 
      AND status = 'active'
      AND created_at <= date(p_billing_month, '+1 month');
    
    -- 사용량 계산
    SELECT 
        COALESCE(SUM(CASE WHEN metric_type = 'data_points' THEN metric_value ELSE 0 END), 0),
        COALESCE(SUM(CASE WHEN metric_type = 'api_requests' THEN metric_value ELSE 0 END), 0)
    INTO data_points_used, api_requests_used
    FROM usage_metrics
    WHERE tenant_id = p_tenant_id
      AND measurement_date >= p_billing_month
      AND measurement_date < date(p_billing_month, '+1 month');
    
    -- 총 요금 계산
    total_fee := base_fee + (edge_server_count * edge_server_fee_per_unit);
    
    -- 오버리지 요금 (starter, professional 플랜만)
    IF tenant_plan != 'enterprise' THEN
        DECLARE
            max_data_points INTEGER;
            max_api_requests INTEGER := 100000; -- 기본 API 요청 한도
        BEGIN
            -- 플랜별 데이터 포인트 한도
            IF tenant_plan = 'starter' THEN
                max_data_points := 100000; -- 월 10만 포인트
            ELSE
                max_data_points := 1000000; -- 월 100만 포인트
            END IF;
            
            -- 데이터 포인트 오버리지
            IF data_points_used > max_data_points THEN
                total_fee := total_fee + ((data_points_used - max_data_points) / 1000.0) * data_points_overage_fee;
            END IF;
            
            -- API 요청 오버리지
            IF api_requests_used > max_api_requests THEN
                total_fee := total_fee + ((api_requests_used - max_api_requests) / 1000.0) * api_requests_overage_fee;
            END IF;
        END;
    END IF;
    
    RETURN total_fee;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- 테넌트별 통계 함수
-- ===============================================================================
CREATE OR REPLACE FUNCTION get_tenant_statistics(p_tenant_id TEXT)
RETURNS TABLE(
    metric_name TEXT,
    metric_value BIGINT,
    last_updated TIMESTAMP
) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        'active_edge_servers'::TEXT,
        COUNT(*)::BIGINT,
        MAX(es.last_seen)
    FROM edge_servers es
    WHERE es.tenant_id = p_tenant_id AND es.status = 'active'
    
    UNION ALL
    
    SELECT 
        'total_data_points_today'::TEXT,
        COALESCE(SUM(um.metric_value), 0)::BIGINT,
        MAX(um.created_at)
    FROM usage_metrics um
    WHERE um.tenant_id = p_tenant_id 
      AND um.metric_type = 'data_points'
      AND um.measurement_date = CURRENT_DATE
    
    UNION ALL
    
    SELECT 
        'api_requests_today'::TEXT,
        COALESCE(SUM(um.metric_value), 0)::BIGINT,
        MAX(um.created_at)
    FROM usage_metrics um
    WHERE um.tenant_id = p_tenant_id 
      AND um.metric_type = 'api_requests'
      AND um.measurement_date = CURRENT_DATE;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- 데이터 정리 및 유지보수 함수들
-- ===============================================================================

-- 오래된 로그 정리 함수
CREATE OR REPLACE FUNCTION cleanup_old_logs(retention_days INTEGER DEFAULT 90)
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER := 0;
    tenant_rec RECORD;
    schema_name TEXT;
    total_deleted INTEGER := 0;
BEGIN
    -- 각 테넌트 스키마의 로그 정리
    FOR tenant_rec IN SELECT company_code FROM tenants WHERE is_active = {{TRUE}} LOOP
        schema_name := 'tenant_' || tenant_rec.company_code;
        
        -- 시스템 로그 정리
        EXECUTE format('
            DELETE FROM %I.system_logs 
            WHERE timestamp < datetime(''now'', ''-%s days'')
        ', schema_name, retention_days);
        
        GET DIAGNOSTICS deleted_count = ROW_COUNT;
        total_deleted := total_deleted + deleted_count;
        
        -- 통신 로그 정리 (더 짧은 보관 기간)
        EXECUTE format('
            DELETE FROM %I.communication_logs 
            WHERE timestamp < datetime(''now'', ''-%s days'')
        ', schema_name, LEAST(retention_days, 30));
        
        GET DIAGNOSTICS deleted_count = ROW_COUNT;
        total_deleted := total_deleted + deleted_count;
        
        -- 사용자 활동 로그 정리
        EXECUTE format('
            DELETE FROM %I.user_activities 
            WHERE timestamp < datetime(''now'', ''-%s days'')
        ', schema_name, retention_days);
        
        GET DIAGNOSTICS deleted_count = ROW_COUNT;
        total_deleted := total_deleted + deleted_count;
        
    END LOOP;
    
    RETURN total_deleted;
END;
$$ LANGUAGE plpgsql;

-- 테넌트 통계 업데이트 함수
CREATE OR REPLACE FUNCTION update_tenant_statistics()
RETURNS VOID AS $$
BEGIN
    -- 각 테넌트의 Edge 서버 상태 업데이트
    UPDATE edge_servers SET status = 'inactive'
    WHERE status = 'active' 
      AND last_seen < datetime('now', '-1 hour');
    
    -- 만료된 체험판 상태 업데이트
    UPDATE tenants SET subscription_status = 'trial_expired'
    WHERE subscription_status = 'trial'
      AND trial_end_date < CURRENT_TIMESTAMP;
      
    RAISE NOTICE 'Tenant statistics updated';
END;
$$ LANGUAGE plpgsql;

-- 사용량 집계 함수 (일일 → 월별)
CREATE OR REPLACE FUNCTION aggregate_monthly_usage()
RETURNS VOID AS $$
DECLARE
    last_month DATE;
BEGIN
    last_month := date('now', 'start of month', '-1 month');
    
    -- 월별 사용량 집계 (필요시 별도 테이블에 저장)
    -- 현재는 뷰로 처리하므로 실제 집계 작업 없음
    
    RAISE NOTICE 'Monthly usage aggregation completed for %', last_month;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- 시스템 상태 확인 함수
-- ===============================================================================
CREATE OR REPLACE FUNCTION get_system_health()
RETURNS TABLE(
    component TEXT,
    status TEXT,
    message TEXT,
    last_check TIMESTAMP
) AS $$
BEGIN
    RETURN QUERY
    -- 테넌트 상태
    SELECT 
        'tenants'::TEXT,
        CASE WHEN COUNT(*) > 0 THEN 'healthy' ELSE 'warning' END::TEXT,
        'Total active tenants: ' || COUNT(*)::TEXT,
        CURRENT_TIMESTAMP
    FROM tenants WHERE is_active = {{TRUE}}
    
    UNION ALL
    
    -- Edge 서버 상태
    SELECT 
        'edge_servers'::TEXT,
        CASE 
            WHEN COUNT(*) = 0 THEN 'warning'
            WHEN COUNT(CASE WHEN last_seen > datetime('now', '-5 minutes') THEN 1 END) = COUNT(*) THEN 'healthy'
            ELSE 'degraded'
        END::TEXT,
        'Online servers: ' || COUNT(CASE WHEN last_seen > datetime('now', '-5 minutes') THEN 1 END)::TEXT || '/' || COUNT(*)::TEXT,
        CURRENT_TIMESTAMP
    FROM edge_servers WHERE status = 'active'
    
    UNION ALL
    
    -- 드라이버 플러그인 상태
    SELECT 
        'driver_plugins'::TEXT,
        CASE WHEN COUNT(*) > 0 THEN 'healthy' ELSE 'error' END::TEXT,
        'Active driver plugins: ' || COUNT(*)::TEXT,
        CURRENT_TIMESTAMP
    FROM driver_plugins WHERE is_enabled = {{TRUE}};
END;
$$ LANGUAGE plpgsql;