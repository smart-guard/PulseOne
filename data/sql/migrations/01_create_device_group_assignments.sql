CREATE TABLE IF NOT EXISTS device_group_assignments (
    device_id INTEGER NOT NULL,
    group_id INTEGER NOT NULL,
    is_primary INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (device_id, group_id),
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    FOREIGN KEY (group_id) REFERENCES device_groups(id) ON DELETE CASCADE
);
