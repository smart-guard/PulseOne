-- Virtual Point Data Seeding for ID 7 ("평균 온실 온도")

-- 1. Update Formula (Add inputs. prefix for Dependency Map)
UPDATE virtual_points
SET 
    formula = 'return (inputs.temp_A + inputs.temp_B) / 2;',
    
    updated_at = datetime('now')
WHERE id = 7;

-- 2. Clear old logs for this point
DELETE FROM virtual_point_logs WHERE point_id = 7;

-- 3. Insert Audit Logs
INSERT INTO virtual_point_logs (point_id, action, details, created_at)
VALUES 
    (7, 'CREATE', '가상포인트 생성됨 (Initial creation)', datetime('now', '-2 days')),
    (7, 'UPDATE', '실행 주기 변경: 500ms -> 1000ms', datetime('now', '-1 day')),
    (7, 'DISABLE', '연산 일시 정지 (Maintenance Check)', datetime('now', '-5 hours')),
    (7, 'ENABLE', '연산 재개 (System Online)', datetime('now', '-1 hour')),
    (7, 'UPDATE', '수식 변경: (temp_A + temp_B) / 2', datetime('now', '-5 minutes'));
