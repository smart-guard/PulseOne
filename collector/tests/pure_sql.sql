
        CREATE TABLE IF NOT EXISTS alarm_occurrences (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            rule_id INTEGER NOT NULL,
            tenant_id INTEGER NOT NULL,
            occurrence_time DATETIME DEFAULT CURRENT_TIMESTAMP,
            trigger_value TEXT,
            trigger_condition TEXT,
            alarm_message TEXT,
            severity VARCHAR(20),
            state VARCHAR(20) DEFAULT 'active',
            acknowledged_time DATETIME,
            acknowledged_by INTEGER,
            acknowledge_comment TEXT,
            cleared_time DATETIME,
            cleared_value TEXT,
            clear_comment TEXT,
            notification_sent INTEGER DEFAULT 0,
            notification_time DATETIME,
            notification_count INTEGER DEFAULT 0,
            notification_result TEXT,
            context_data TEXT,
            source_name VARCHAR(100),
            location VARCHAR(200),
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    
