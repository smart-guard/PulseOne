-- =============================================================================
-- 03-alarm-tables.sql - 알람 관련 테이블
-- =============================================================================

-- 알람 룰 테이블
CREATE TABLE IF NOT EXISTS alarm_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    device_id INTEGER,
    data_point_id INTEGER,
    virtual_point_id INTEGER,
    condition_type VARCHAR(50) NOT NULL,
    condition_config TEXT NOT NULL,
    severity VARCHAR(20) DEFAULT 'warning',
    message_template TEXT,
    auto_acknowledge INTEGER DEFAULT 0,
    auto_clear INTEGER DEFAULT 0,
    acknowledgment_required INTEGER DEFAULT 1,
    escalation_time_minutes INTEGER DEFAULT 0,
    notification_enabled INTEGER DEFAULT 1,
    email_notification INTEGER DEFAULT 0,
    sms_notification INTEGER DEFAULT 0,
    is_enabled INTEGER DEFAULT 1,
    created_by INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    FOREIGN KEY (data_point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id) ON DELETE SET NULL,
    UNIQUE(tenant_id, name)
);

-- 알람 발생 테이블
CREATE TABLE IF NOT EXISTS alarm_occurrences (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    alarm_rule_id INTEGER NOT NULL,
    device_id INTEGER,
    data_point_id INTEGER,
    virtual_point_id INTEGER,
    severity VARCHAR(20) NOT NULL,
    message TEXT NOT NULL,
    trigger_value TEXT,
    condition_details TEXT,
    state VARCHAR(20) DEFAULT 'active',
    occurrence_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    acknowledgment_time DATETIME,
    acknowledged_by INTEGER,
    acknowledgment_note TEXT,
    clear_time DATETIME,
    cleared_by INTEGER,
    resolution_note TEXT,
    escalation_level INTEGER DEFAULT 0,
    notification_sent INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (alarm_rule_id) REFERENCES alarm_rules(id) ON DELETE CASCADE,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE SET NULL,
    FOREIGN KEY (data_point_id) REFERENCES data_points(id) ON DELETE SET NULL,
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE SET NULL,
    FOREIGN KEY (acknowledged_by) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY (cleared_by) REFERENCES users(id) ON DELETE SET NULL
);