-- =============================================================================
-- 15-backups-table.sql
-- =============================================================================

CREATE TABLE IF NOT EXISTS backups (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    filename TEXT NOT NULL UNIQUE,
    type TEXT DEFAULT 'full', -- 'full', 'incremental', 'differential'
    status TEXT DEFAULT 'completed', -- 'completed', 'running', 'failed'
    size INTEGER DEFAULT 0,
    location TEXT DEFAULT '/app/data/backup',
    created_at TIMESTAMP DEFAULT (datetime('now', 'localtime')),
    created_by TEXT,
    description TEXT,
    duration INTEGER, -- seconds
    is_deleted INTEGER DEFAULT 0
);

-- 초기 인덱스
CREATE INDEX IF NOT EXISTS idx_backups_created_at ON backups(created_at);
CREATE INDEX IF NOT EXISTS idx_backups_status ON backups(status);
