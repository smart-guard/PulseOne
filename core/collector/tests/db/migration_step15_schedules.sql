-- Step 15: Device Schedule Persistence Schema
-- Create table for storing device-specific schedules (e.g., BACnet Weekly_Schedule)

CREATE TABLE IF NOT EXISTS device_schedules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    point_id INTEGER, -- Optional: link to a specific point if applicable
    schedule_type TEXT NOT NULL, -- e.g., 'BACNET_WEEKLY'
    schedule_data TEXT NOT NULL, -- JSON string representation
    is_synced INTEGER DEFAULT 0, -- 0: Pending, 1: Synced
    last_sync_time DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- Index for faster lookup by device
CREATE INDEX IF NOT EXISTS idx_device_schedules_device_id ON device_schedules(device_id);
-- Index for pending syncs
CREATE INDEX IF NOT EXISTS idx_device_schedules_sync ON device_schedules(is_synced) WHERE is_synced = 0;
