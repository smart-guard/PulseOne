-- Virtual Point Data Seeding for Demo
-- 1. Create table if missing (Applying schema change dynamically)
CREATE TABLE IF NOT EXISTS virtual_point_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    point_id INTEGER NOT NULL,
    action VARCHAR(50) NOT NULL,
    previous_state TEXT,
    new_state TEXT,
    user_id INTEGER,
    details TEXT,
    created_at DATETIME DEFAULT (datetime('now', 'localtime')),
    FOREIGN KEY (point_id) REFERENCES virtual_points(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_virtual_point_logs_point_id ON virtual_point_logs(point_id);
CREATE INDEX IF NOT EXISTS idx_virtual_point_logs_created_at ON virtual_point_logs(created_at);

-- 2. Update existing virtual point formula for Dependency Map
-- Using formula column only as expression column is not in DB
UPDATE virtual_points
SET 
    formula = 'return (inputs.temp_A + inputs.temp_B) / 2;',
    updated_at = datetime('now', 'localtime')
WHERE id = (SELECT id FROM virtual_points LIMIT 1);

-- 3. Clear old logs for this point
DELETE FROM virtual_point_logs 
WHERE point_id = (SELECT id FROM virtual_points LIMIT 1);

-- 4. Insert Audit Logs
INSERT INTO virtual_point_logs (point_id, action, details, created_at)
VALUES 
    ((SELECT id FROM virtual_points LIMIT 1), 'CREATE', '가상포인트 생성됨 (Initial creation)', datetime('now', '-2 days')),
    ((SELECT id FROM virtual_points LIMIT 1), 'UPDATE', '실행 주기 변경: 500ms -> 1000ms', datetime('now', '-1 day')),
    ((SELECT id FROM virtual_points LIMIT 1), 'DISABLE', '연산 일시 정지 (Maintenance Check)', datetime('now', '-5 hours')),
    ((SELECT id FROM virtual_points LIMIT 1), 'ENABLE', '연산 재개 (System Online)', datetime('now', '-1 hour')),
    ((SELECT id FROM virtual_points LIMIT 1), 'UPDATE', '수식 변경: (temp_A + temp_B) / 2', datetime('now', '-5 minutes'));

-- Check result
SELECT 'Seeding Complete. Logs inserted: ' || count(*) FROM virtual_point_logs WHERE point_id = (SELECT id FROM virtual_points LIMIT 1);
