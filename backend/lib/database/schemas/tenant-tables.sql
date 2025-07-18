-- backend/lib/database/schemas/tenant-tables.sql
-- 테넌트별 알람 및 로그 테이블 (스키마 생성 함수에 추가)

-- ===============================================================================
-- 테넌트 알람 및 로그 테이블 생성 함수 (tenant-functions.sql에 추가할 내용)
-- ===============================================================================

-- 이 함수는 create_tenant_tables 함수에 추가되어야 합니다
CREATE OR REPLACE FUNCTION create_tenant_alarm_and_log_tables(schema_name TEXT)
RETURNS VOID AS $$
BEGIN
    -- 알람 설정 테이블
    EXECUTE format('
        CREATE TABLE %I.alarm_configs (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            factory_id {{UUID}} REFERENCES %I.factories(id) ON DELETE CASCADE,
            
            -- 알람 기본 정보
            name VARCHAR(100) NOT NULL,
            description {{TEXT}},
            source_type VARCHAR(20) NOT NULL,
            source_id {{UUID}} NOT NULL,
            alarm_type VARCHAR(20) NOT NULL,
            
            -- 알람 설정
            enabled {{BOOLEAN}} DEFAULT {{TRUE}},
            priority VARCHAR(10) DEFAULT ''medium'',
            auto_acknowledge {{BOOLEAN}} DEFAULT {{FALSE}},
            
            -- 아날로그 알람 설정
            high_limit DECIMAL(15,4),
            low_limit DECIMAL(15,4),
            deadband DECIMAL(15,4) DEFAULT 0,
            
            -- 디지털 알람 설정
            trigger_value DECIMAL(15,4),
            
            -- 메시지 설정
            message_template {{TEXT}} NOT NULL,
            
            -- 알림 설정
            email_enabled {{BOOLEAN}} DEFAULT {{FALSE}},
            email_recipients {{TEXT}}, -- JSON array
            sms_enabled {{BOOLEAN}} DEFAULT {{FALSE}},
            sms_recipients {{TEXT}}, -- JSON array
            
            created_by {{UUID}} REFERENCES %I.users(id),
            created_at {{TIMESTAMP}},
            updated_at {{TIMESTAMP}},
            
            CONSTRAINT chk_alarm_source_type CHECK (source_type IN (''data_point'', ''virtual_point'')),
            CONSTRAINT chk_alarm_type CHECK (alarm_type IN (''high'', ''low'', ''deviation'', ''discrete'')),
            CONSTRAINT chk_alarm_priority CHECK (priority IN (''low'', ''medium'', ''high'', ''critical''))
        );
    ', schema_name, schema_name, schema_name);

    -- 활성 알람 테이블
    EXECUTE format('
        CREATE TABLE %I.active_alarms (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            alarm_config_id {{UUID}} NOT NULL REFERENCES %I.alarm_configs(id) ON DELETE CASCADE,
            
            -- 알람 발생 정보
            triggered_value DECIMAL(15,4),
            message {{TEXT}} NOT NULL,
            triggered_at {{TIMESTAMP}},
            
            -- 알람 응답 정보
            acknowledged_at {{TIMESTAMP}},
            acknowledged_by {{UUID}} REFERENCES %I.users(id),
            acknowledgment_comment {{TEXT}},
            
            -- 상태
            is_active {{BOOLEAN}} DEFAULT {{TRUE}}
        );
    ', schema_name, schema_name, schema_name);

    -- 알람 히스토리 테이블
    EXECUTE format('
        CREATE TABLE %I.alarm_history (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            alarm_config_id {{UUID}} NOT NULL REFERENCES %I.alarm_configs(id) ON DELETE CASCADE,
            
            -- 이벤트 정보
            event_type VARCHAR(20) NOT NULL,
            triggered_value DECIMAL(15,4),
            message {{TEXT}},
            event_time {{TIMESTAMP}},
            user_id {{UUID}} REFERENCES %I.users(id),
            
            CONSTRAINT chk_event_type CHECK (event_type IN (''triggered'', ''acknowledged'', ''cleared'', ''disabled''))
        );
    ', schema_name, schema_name, schema_name);

    -- 시스템 로그 테이블
    EXECUTE format('
        CREATE TABLE %I.system_logs (
            id {{AUTO_INCREMENT}},
            level VARCHAR(10) NOT NULL,
            category VARCHAR(50) NOT NULL,
            message {{TEXT}} NOT NULL,
            details {{TEXT}}, -- JSON
            source VARCHAR(100),
            user_id {{UUID}} REFERENCES %I.users(id),
            timestamp {{TIMESTAMP}},
            
            CONSTRAINT chk_log_level CHECK (level IN (''debug'', ''info'', ''warn'', ''error'', ''fatal''))
        );
    ', schema_name, schema_name);

    -- 사용자 활동 로그
    EXECUTE format('
        CREATE TABLE %I.user_activities (
            id {{AUTO_INCREMENT}},
            user_id {{UUID}} REFERENCES %I.users(id) ON DELETE SET NULL,
            action VARCHAR(50) NOT NULL,
            resource_type VARCHAR(50),
            resource_id {{UUID}},
            details {{TEXT}}, -- JSON
            ip_address VARCHAR(45),
            user_agent {{TEXT}},
            timestamp {{TIMESTAMP}}
        );
    ', schema_name, schema_name);

    -- 통신 로그 테이블
    EXECUTE format('
        CREATE TABLE %I.communication_logs (
            id {{AUTO_INCREMENT}},
            device_id {{UUID}} REFERENCES %I.devices(id) ON DELETE CASCADE,
            direction VARCHAR(10) NOT NULL,
            protocol VARCHAR(20) NOT NULL,
            raw_data {{TEXT}}, -- BYTEA for PostgreSQL, TEXT for others
            decoded_data {{TEXT}}, -- JSON
            success {{BOOLEAN}},
            error_message {{TEXT}},
            response_time INTEGER,
            timestamp {{TIMESTAMP}},
            
            CONSTRAINT chk_direction CHECK (direction IN (''request'', ''response''))
        );
    ', schema_name, schema_name);

    -- 데이터 히스토리 테이블 (파티셔닝 고려)
    EXECUTE format('
        CREATE TABLE %I.data_history (
            id {{AUTO_INCREMENT}},
            point_id {{UUID}} NOT NULL REFERENCES %I.data_points(id) ON DELETE CASCADE,
            value DECIMAL(15,4),
            raw_value DECIMAL(15,4),
            quality VARCHAR(20),
            timestamp {{TIMESTAMP}} NOT NULL
        );
    ', schema_name, schema_name);

    -- 가상 포인트 히스토리
    EXECUTE format('
        CREATE TABLE %I.virtual_point_history (
            id {{AUTO_INCREMENT}},
            virtual_point_id {{UUID}} NOT NULL REFERENCES %I.virtual_points(id) ON DELETE CASCADE,
            value DECIMAL(15,4),
            quality VARCHAR(20),
            timestamp {{TIMESTAMP}} NOT NULL
        );
    ', schema_name, schema_name);

    -- 프로젝트 테이블 (테넌트별)
    EXECUTE format('
        CREATE TABLE %I.projects (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            factory_id {{UUID}} REFERENCES %I.factories(id) ON DELETE CASCADE,
            name VARCHAR(100) NOT NULL,
            description {{TEXT}},
            config {{TEXT}}, -- JSON
            is_active {{BOOLEAN}} DEFAULT {{TRUE}},
            created_by {{UUID}} REFERENCES %I.users(id),
            created_at {{TIMESTAMP}},
            updated_at {{TIMESTAMP}}
        );
    ', schema_name, schema_name, schema_name);

    -- 대시보드 설정 테이블
    EXECUTE format('
        CREATE TABLE %I.dashboards (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            factory_id {{UUID}} REFERENCES %I.factories(id) ON DELETE CASCADE,
            name VARCHAR(100) NOT NULL,
            description {{TEXT}},
            layout {{TEXT}} NOT NULL, -- JSON dashboard layout
            widgets {{TEXT}} NOT NULL, -- JSON widgets configuration
            is_default {{BOOLEAN}} DEFAULT {{FALSE}},
            is_public {{BOOLEAN}} DEFAULT {{FALSE}},
            created_by {{UUID}} REFERENCES %I.users(id),
            created_at {{TIMESTAMP}},
            updated_at {{TIMESTAMP}}
        );
    ', schema_name, schema_name, schema_name);

    -- 리포트 템플릿 테이블
    EXECUTE format('
        CREATE TABLE %I.report_templates (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            factory_id {{UUID}} REFERENCES %I.factories(id) ON DELETE CASCADE,
            name VARCHAR(100) NOT NULL,
            description {{TEXT}},
            report_type VARCHAR(50) NOT NULL, -- hourly, daily, weekly, monthly, custom
            template_config {{TEXT}} NOT NULL, -- JSON template configuration
            data_sources {{TEXT}} NOT NULL, -- JSON data sources
            is_enabled {{BOOLEAN}} DEFAULT {{TRUE}},
            schedule_config {{TEXT}}, -- JSON scheduling configuration
            created_by {{UUID}} REFERENCES %I.users(id),
            created_at {{TIMESTAMP}},
            updated_at {{TIMESTAMP}},
            
            CONSTRAINT chk_report_type CHECK (report_type IN (''hourly'', ''daily'', ''weekly'', ''monthly'', ''custom''))
        );
    ', schema_name, schema_name, schema_name);

    -- 리포트 생성 이력 테이블
    EXECUTE format('
        CREATE TABLE %I.report_history (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            template_id {{UUID}} REFERENCES %I.report_templates(id) ON DELETE CASCADE,
            file_path VARCHAR(500),
            file_size INTEGER,
            status VARCHAR(20) DEFAULT ''generating'', -- generating, completed, failed
            start_time {{TIMESTAMP}},
            end_time {{TIMESTAMP}},
            error_message {{TEXT}},
            generated_by {{UUID}} REFERENCES %I.users(id),
            created_at {{TIMESTAMP}},
            
            CONSTRAINT chk_report_status CHECK (status IN (''generating'', ''completed'', ''failed''))
        );
    ', schema_name, schema_name, schema_name);

    -- 알림 채널 테이블
    EXECUTE format('
        CREATE TABLE %I.notification_channels (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            factory_id {{UUID}} REFERENCES %I.factories(id) ON DELETE CASCADE,
            name VARCHAR(100) NOT NULL,
            channel_type VARCHAR(20) NOT NULL, -- email, sms, webhook, slack, teams
            config {{TEXT}} NOT NULL, -- JSON channel configuration
            
            -- 필터 설정
            severity_filter {{TEXT}}, -- JSON array: ["critical", "high"]
            category_filter {{TEXT}}, -- JSON array: ["process", "safety"]
            time_filter {{TEXT}}, -- JSON: 시간대 필터
            
            is_active {{BOOLEAN}} DEFAULT {{TRUE}},
            last_used_at {{TIMESTAMP}},
            created_by {{UUID}} REFERENCES %I.users(id),
            created_at {{TIMESTAMP}},
            updated_at {{TIMESTAMP}},
            
            CONSTRAINT chk_channel_type CHECK (channel_type IN (''email'', ''sms'', ''webhook'', ''slack'', ''teams''))
        );
    ', schema_name, schema_name, schema_name);

    -- 알림 이력 테이블
    EXECUTE format('
        CREATE TABLE %I.notification_logs (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            alarm_instance_id {{UUID}},
            notification_channel_id {{UUID}} NOT NULL REFERENCES %I.notification_channels(id) ON DELETE CASCADE,
            
            -- 알림 정보
            notification_type VARCHAR(20) NOT NULL, -- alarm_trigger, alarm_acknowledge, alarm_clear
            message {{TEXT}},
            
            -- 전송 정보
            sent_at {{TIMESTAMP}},
            status VARCHAR(20) NOT NULL, -- pending, sent, failed, delivered
            error_message {{TEXT}},
            delivery_confirmed_at {{TIMESTAMP}},
            
            -- 재시도 정보
            retry_count INTEGER DEFAULT 0,
            next_retry_at {{TIMESTAMP}},
            
            FOREIGN KEY (alarm_instance_id) REFERENCES %I.active_alarms(id) ON DELETE CASCADE,
            CONSTRAINT chk_notification_type CHECK (notification_type IN (''alarm_trigger'', ''alarm_acknowledge'', ''alarm_clear'')),
            CONSTRAINT chk_notification_status CHECK (status IN (''pending'', ''sent'', ''failed'', ''delivered''))
        );
    ', schema_name, schema_name, schema_name);

    RAISE NOTICE 'Alarm and log tables created for schema %', schema_name;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- 테넌트 인덱스 생성 함수
-- ===============================================================================
CREATE OR REPLACE FUNCTION create_tenant_indexes(schema_name TEXT)
RETURNS VOID AS $$
BEGIN
    -- 사용자 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_users_email ON %I.users(email)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_users_role ON %I.users(role)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_users_active ON %I.users(is_active)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 공장 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_factories_code ON %I.factories(code)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_factories_active ON %I.factories(is_active)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 디바이스 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_devices_factory ON %I.devices(factory_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_devices_protocol ON %I.devices(protocol_type)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_devices_enabled ON %I.devices(is_enabled)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_devices_group ON %I.devices(device_group_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 데이터 포인트 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_data_points_device ON %I.data_points(device_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_data_points_enabled ON %I.data_points(is_enabled)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_data_points_type ON %I.data_points(data_type)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 현재값 조회 최적화
    EXECUTE format('CREATE INDEX idx_%s_current_values_timestamp ON %I.current_values(timestamp DESC)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 가상 포인트 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_virtual_points_factory ON %I.virtual_points(factory_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_virtual_points_enabled ON %I.virtual_points(is_enabled)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_virtual_point_inputs_vp ON %I.virtual_point_inputs(virtual_point_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 알람 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_alarm_configs_factory ON %I.alarm_configs(factory_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_alarm_configs_enabled ON %I.alarm_configs(enabled)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_alarm_configs_priority ON %I.alarm_configs(priority)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_active_alarms_config ON %I.active_alarms(alarm_config_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_active_alarms_active ON %I.active_alarms(is_active) WHERE is_active = {{TRUE}}', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_alarm_history_config_time ON %I.alarm_history(alarm_config_id, event_time DESC)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 로그 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_system_logs_timestamp ON %I.system_logs(timestamp DESC)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_system_logs_level ON %I.system_logs(level)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_system_logs_category ON %I.system_logs(category)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_user_activities_user_time ON %I.user_activities(user_id, timestamp DESC)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_communication_logs_device_time ON %I.communication_logs(device_id, timestamp DESC)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 히스토리 데이터 조회 최적화
    EXECUTE format('CREATE INDEX idx_%s_data_history_point_time ON %I.data_history(point_id, timestamp DESC)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_virtual_point_history_point_time ON %I.virtual_point_history(virtual_point_id, timestamp DESC)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 프로젝트 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_projects_factory ON %I.projects(factory_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_projects_active ON %I.projects(is_active)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 대시보드 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_dashboards_factory ON %I.dashboards(factory_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_dashboards_default ON %I.dashboards(is_default)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 리포트 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_report_templates_factory ON %I.report_templates(factory_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_report_templates_enabled ON %I.report_templates(is_enabled)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_report_history_template ON %I.report_history(template_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    -- 알림 관련 인덱스
    EXECUTE format('CREATE INDEX idx_%s_notification_channels_factory ON %I.notification_channels(factory_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_notification_channels_active ON %I.notification_channels(is_active)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_notification_logs_channel ON %I.notification_logs(notification_channel_id)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    EXECUTE format('CREATE INDEX idx_%s_notification_logs_status ON %I.notification_logs(status)', 
                  replace(schema_name, 'tenant_', ''), schema_name);
    
    RAISE NOTICE 'All indexes created for schema %', schema_name;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- 테넌트 트리거 생성 함수
-- ===============================================================================
CREATE OR REPLACE FUNCTION create_tenant_triggers(schema_name TEXT)
RETURNS VOID AS $$
BEGIN
    -- 업데이트 시간 자동 갱신 트리거들
    EXECUTE format('
        CREATE TRIGGER update_%s_users_updated_at 
        BEFORE UPDATE ON %I.users
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_factories_updated_at 
        BEFORE UPDATE ON %I.factories
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_devices_updated_at 
        BEFORE UPDATE ON %I.devices
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_data_points_updated_at 
        BEFORE UPDATE ON %I.data_points
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_virtual_points_updated_at 
        BEFORE UPDATE ON %I.virtual_points
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_alarm_configs_updated_at 
        BEFORE UPDATE ON %I.alarm_configs
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_projects_updated_at 
        BEFORE UPDATE ON %I.projects
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_dashboards_updated_at 
        BEFORE UPDATE ON %I.dashboards
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_report_templates_updated_at 
        BEFORE UPDATE ON %I.report_templates
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    EXECUTE format('
        CREATE TRIGGER update_%s_notification_channels_updated_at 
        BEFORE UPDATE ON %I.notification_channels
        FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
    ', replace(schema_name, 'tenant_', ''), schema_name);

    RAISE NOTICE 'All triggers created for schema %', schema_name;
END;
$$ LANGUAGE plpgsql;