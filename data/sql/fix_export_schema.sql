-- Schema Migration for Export Gateway Features

-- 1. Add missing column to export_profiles
ALTER TABLE export_profiles ADD COLUMN data_points TEXT DEFAULT '[]';

-- 2. Create export_profile_assignments table
CREATE TABLE IF NOT EXISTS export_profile_assignments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER NOT NULL,
    gateway_id INTEGER NOT NULL,
    is_active INTEGER DEFAULT 1,
    assigned_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE CASCADE,
    FOREIGN KEY (gateway_id) REFERENCES edge_servers(id) ON DELETE CASCADE
);
