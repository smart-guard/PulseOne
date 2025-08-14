-- =============================================================================
-- alarm_occurrences 테이블에 device_id 컬럼 추가
-- =============================================================================

-- 1. device_id 컬럼 추가
ALTER TABLE alarm_occurrences ADD COLUMN device_id TEXT;

-- 2. point_id 컬럼도 추가 (더 정확한 매핑용)
ALTER TABLE alarm_occurrences ADD COLUMN point_id INTEGER;

-- 3. 기존 데이터에 device_id 값 업데이트 (rule_id를 통해 매핑)
UPDATE alarm_occurrences 
SET device_id = (
    SELECT CASE 
        WHEN ar.target_type = 'data_point' THEN 
            (SELECT dp.device_id FROM data_points dp WHERE dp.id = ar.target_id)
        WHEN ar.target_type = 'virtual_point' THEN 
            (SELECT vp.device_id FROM virtual_points vp WHERE vp.id = ar.target_id)
        ELSE NULL
    END
    FROM alarm_rules ar 
    WHERE ar.id = alarm_occurrences.rule_id
)
WHERE device_id IS NULL;

-- 4. point_id도 업데이트
UPDATE alarm_occurrences 
SET point_id = (
    SELECT ar.target_id
    FROM alarm_rules ar 
    WHERE ar.id = alarm_occurrences.rule_id 
    AND ar.target_type IN ('data_point', 'virtual_point')
)
WHERE point_id IS NULL;

-- 5. 인덱스 추가 (성능 최적화)
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_device_id ON alarm_occurrences(device_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_point_id ON alarm_occurrences(point_id);
CREATE INDEX IF NOT EXISTS idx_alarm_occurrences_rule_device ON alarm_occurrences(rule_id, device_id);

-- 6. 검증 쿼리 (실행 후 확인용)
SELECT 
    'Updated alarm_occurrences' as action,
    COUNT(*) as total_records,
    COUNT(device_id) as records_with_device_id,
    COUNT(point_id) as records_with_point_id
FROM alarm_occurrences;