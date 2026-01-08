#!/bin/bash
# =============================================================================
# PulseOne 데이터베이스 전체 리포트 생성 스크립트
# 모든 테이블과 데이터를 한번에 조회하여 로그 파일로 저장
# =============================================================================

DB_FILE="/app/data/db/pulseone.db"
REPORT_FILE="/app/backend/database_report_$(date +%Y%m%d_%H%M%S).log"

echo "🔍 PulseOne 데이터베이스 전체 리포트 생성 중..."
echo "📊 데이터베이스: $DB_FILE"
echo "📄 리포트 파일: $REPORT_FILE"
echo ""

# 리포트 헤더
cat > $REPORT_FILE << 'EOL'
===============================================================================
🗄️ PulseOne 데이터베이스 전체 리포트
===============================================================================
생성 시간: $(date '+%Y-%m-%d %H:%M:%S')
데이터베이스: /app/data/db/pulseone.db
===============================================================================

EOL

# 실제 생성 시간 삽입
sed -i "s/\$(date.*)/$(date '+%Y-%m-%d %H:%M:%S')/" $REPORT_FILE

# 1. 기본 정보
echo "📋 1. 데이터베이스 기본 정보" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE
sqlite3 $DB_FILE << 'EOF' >> $REPORT_FILE
.headers on
.mode table

-- 데이터베이스 파일 정보
SELECT 'Database File' as info, '/app/data/db/pulseone.db' as value
UNION ALL
SELECT 'SQLite Version', sqlite_version()
UNION ALL  
SELECT 'Schema Version', COALESCE((SELECT version FROM schema_versions ORDER BY id DESC LIMIT 1), 'Unknown');

EOF

echo "" >> $REPORT_FILE

# 2. 전체 테이블 목록
echo "📋 2. 전체 테이블 목록" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE
sqlite3 $DB_FILE << 'EOF' >> $REPORT_FILE
.headers on
.mode table

SELECT name as table_name FROM sqlite_master 
WHERE type='table' AND name NOT LIKE 'sqlite_%' 
ORDER BY name;

EOF

echo "" >> $REPORT_FILE

# 3. 테이블별 레코드 수
echo "📊 3. 테이블별 레코드 수" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE
sqlite3 $DB_FILE << 'EOF' >> $REPORT_FILE
.headers on
.mode table

SELECT 
    'tenants' as table_name, COUNT(*) as record_count FROM tenants
UNION ALL SELECT 'users', COUNT(*) FROM users
UNION ALL SELECT 'sites', COUNT(*) FROM sites
UNION ALL SELECT 'edge_servers', COUNT(*) FROM edge_servers
UNION ALL SELECT 'device_groups', COUNT(*) FROM device_groups
UNION ALL SELECT 'devices', COUNT(*) FROM devices
UNION ALL SELECT 'device_settings', COUNT(*) FROM device_settings
UNION ALL SELECT 'device_status', COUNT(*) FROM device_status
UNION ALL SELECT 'driver_plugins', COUNT(*) FROM driver_plugins
UNION ALL SELECT 'data_points', COUNT(*) FROM data_points
UNION ALL SELECT 'current_values', COUNT(*) FROM current_values
UNION ALL SELECT 'virtual_points', COUNT(*) FROM virtual_points
UNION ALL SELECT 'virtual_point_dependencies', COUNT(*) FROM virtual_point_dependencies
UNION ALL SELECT 'alarm_definitions', COUNT(*) FROM alarm_definitions
UNION ALL SELECT 'alarm_occurrences', COUNT(*) FROM alarm_occurrences
UNION ALL SELECT 'alarm_rules', COUNT(*) FROM alarm_rules
ORDER BY record_count DESC;

EOF

echo "" >> $REPORT_FILE

# 4. 테넌트 정보
echo "👥 4. 테넌트(회사) 정보" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE
sqlite3 $DB_FILE << 'EOF' >> $REPORT_FILE
.headers on
.mode table

SELECT id, company_name, company_code, domain, subscription_plan, is_active, created_at 
FROM tenants;

EOF

echo "" >> $REPORT_FILE

# 5. 사이트 정보
echo "🏭 5. 사이트 정보" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE
sqlite3 $DB_FILE << 'EOF' >> $REPORT_FILE
.headers on
.mode table

SELECT id, tenant_id, parent_site_id, name, code, site_type, location, is_active 
FROM sites 
ORDER BY tenant_id, name;

EOF

echo "" >> $REPORT_FILE

# 6. 사용자 정보
echo "👤 6. 사용자 정보" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE
sqlite3 $DB_FILE << 'EOF' >> $REPORT_FILE
.headers on
.mode table

SELECT id, tenant_id, username, email, full_name, role, is_active, last_login 
FROM users 
ORDER BY tenant_id, role;

EOF

echo "" >> $REPORT_FILE

# 7. 디바이스 기본 정보
echo "📱 7. 디바이스 기본 정보" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE
sqlite3 $DB_FILE << 'EOF' >> $REPORT_FILE
.headers on
.mode table

SELECT id, tenant_id, site_id, name, device_type, protocol_type, endpoint, is_enabled, created_at
FROM devices 
ORDER BY tenant_id, site_id, name;

EOF

echo "" >> $REPORT_FILE

# 8. 디바이스 상태
echo "📊 8. 디바이스 상태" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE
sqlite3 $DB_FILE << 'EOF' >> $REPORT_FILE
.headers on
.mode table

SELECT device_id, connection_status, last_communication, error_count, last_error, response_time
FROM device_status;

EOF

echo "" >> $REPORT_FILE

# 9. 데이터 포인트 정보
echo "📊 9. 데이터 포인트 정보 (상위 20개)" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE
sqlite3 $DB_FILE << 'EOF' >> $REPORT_FILE
.headers on
.mode table

SELECT id, device_id, name, description, address, data_type, unit, is_enabled
FROM data_points 
ORDER BY device_id, address
LIMIT 20;

EOF

echo "" >> $REPORT_FILE

# 10. 현재값 정보
echo "💾 10. 현재값 정보 (상위 20개)" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE
sqlite3 $DB_FILE << 'EOF' >> $REPORT_FILE
.headers on
.mode table

SELECT point_id, current_value, quality, value_timestamp, updated_at
FROM current_values
ORDER BY point_id
LIMIT 20;

EOF

echo "" >> $REPORT_FILE

# 11. 통계 정보
echo "📈 11. 테넌트별 디바이스 통계" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE
sqlite3 $DB_FILE << 'EOF' >> $REPORT_FILE
.headers on
.mode table

SELECT 
    t.company_name,
    COUNT(d.id) as device_count,
    SUM(CASE WHEN d.is_enabled = 1 THEN 1 ELSE 0 END) as enabled_devices,
    COUNT(DISTINCT d.site_id) as sites_count,
    COUNT(DISTINCT d.protocol_type) as protocol_types
FROM tenants t
LEFT JOIN devices d ON t.id = d.tenant_id
GROUP BY t.id, t.company_name
ORDER BY device_count DESC;

EOF

echo "" >> $REPORT_FILE

# 12. 프로토콜별 분포
echo "🔌 12. 프로토콜별 디바이스 분포" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE
sqlite3 $DB_FILE << 'EOF' >> $REPORT_FILE
.headers on
.mode table

SELECT 
    protocol_type,
    COUNT(*) as device_count,
    SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_count
FROM devices 
GROUP BY protocol_type
ORDER BY device_count DESC;

EOF

echo "" >> $REPORT_FILE

# 13. 디바이스 상세 조인 정보 (API와 동일한 쿼리)
echo "🔍 13. 디바이스 상세 정보 (API 쿼리와 동일)" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE
sqlite3 $DB_FILE << 'EOF' >> $REPORT_FILE
.headers on
.mode table

SELECT 
    d.id,
    d.name as device_name,
    d.device_type,
    d.protocol_type,
    d.endpoint,
    d.is_enabled,
    
    -- 사이트 정보
    s.name as site_name,
    COALESCE(s.code, 'SITE' || s.id) as site_code,
    
    -- 디바이스 설정
    ds.polling_interval_ms,
    ds.connection_timeout_ms,
    ds.keep_alive_enabled,
    
    -- 디바이스 상태
    dst.connection_status,
    dst.last_communication,
    dst.error_count,
    dst.response_time,
    
    -- 그룹 정보
    dg.name as group_name,
    dg.group_type,
    
    -- 통계
    COUNT(dp.id) as data_point_count
    
FROM devices d
LEFT JOIN sites s ON d.site_id = s.id
LEFT JOIN device_settings ds ON d.id = ds.device_id
LEFT JOIN device_status dst ON d.id = dst.device_id
LEFT JOIN device_groups dg ON d.device_group_id = dg.id
LEFT JOIN data_points dp ON d.id = dp.device_id
GROUP BY d.id
ORDER BY d.name;

EOF

echo "" >> $REPORT_FILE

# 14. 스키마 정보
echo "🛠️ 14. 주요 테이블 스키마 정보" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE

# 주요 테이블들의 스키마 정보
for table in "tenants" "sites" "devices" "data_points" "device_status" "device_settings"
do
    echo "--- $table 테이블 스키마 ---" >> $REPORT_FILE
    sqlite3 $DB_FILE ".schema $table" >> $REPORT_FILE
    echo "" >> $REPORT_FILE
done

# 리포트 완료
echo "===============================================================================" >> $REPORT_FILE
echo "🎉 리포트 생성 완료: $(date '+%Y-%m-%d %H:%M:%S')" >> $REPORT_FILE
echo "===============================================================================" >> $REPORT_FILE

# 결과 출력
echo "✅ 리포트 생성 완료!"
echo "📄 파일 위치: $REPORT_FILE"
echo "📊 파일 크기: $(du -h $REPORT_FILE | cut -f1)"
echo ""
echo "🔍 리포트 내용 미리보기:"
echo "----------------------------------------"
head -30 $REPORT_FILE
echo "----------------------------------------"
echo "... (전체 내용은 파일에서 확인)"
echo ""
echo "💡 전체 리포트 보기:"
echo "   cat $REPORT_FILE"
echo "   less $REPORT_FILE"
echo "   tail -f $REPORT_FILE"