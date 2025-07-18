-- backend/lib/database/schemas/tenant-functions.sql
-- 테넌트 스키마 생성 및 관리 함수들

-- ===============================================================================
-- 업데이트 시간 자동 갱신 함수 (공통)
-- ===============================================================================
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ language 'plpgsql';

-- ===============================================================================
-- 테넌트 스키마 생성 함수
-- ===============================================================================
CREATE OR REPLACE FUNCTION create_tenant_schema(tenant_code TEXT)
RETURNS VOID AS $$
DECLARE
    schema_name TEXT := 'tenant_' || tenant_code;
BEGIN
    -- 스키마 생성
    EXECUTE format('CREATE SCHEMA IF NOT EXISTS %I', schema_name);
    
    -- 테넌트별 테이블들 생성
    PERFORM create_tenant_tables(schema_name);
    
    -- 인덱스 생성
    PERFORM create_tenant_indexes(schema_name);
    
    -- 트리거 생성
    PERFORM create_tenant_triggers(schema_name);
    
    RAISE NOTICE 'Tenant schema % created successfully', schema_name;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- 테넌트 테이블 생성 함수
-- ===============================================================================
CREATE OR REPLACE FUNCTION create_tenant_tables(schema_name TEXT)
RETURNS VOID AS $$
BEGIN
    -- 사용자 테이블
    EXECUTE format('
        CREATE TABLE %I.users (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            username VARCHAR(50) UNIQUE NOT NULL,
            email VARCHAR(100) UNIQUE NOT NULL,
            password_hash VARCHAR(255) NOT NULL,
            full_name VARCHAR(100),
            role VARCHAR(20) NOT NULL DEFAULT ''engineer'',
            department VARCHAR(100),
            factory_access {{TEXT}}, -- JSON array of accessible factory IDs
            
            -- 권한 설정
            permissions {{TEXT}} DEFAULT ''["view_dashboard"]'', -- JSON array
            
            -- 상태 정보
            is_active {{BOOLEAN}} DEFAULT {{TRUE}},
            last_login {{TIMESTAMP}},
            password_reset_token VARCHAR(255),
            password_reset_expires {{TIMESTAMP}},
            
            created_at {{TIMESTAMP}},
            updated_at {{TIMESTAMP}},
            
            CONSTRAINT chk_role CHECK (role IN (''company_admin'', ''factory_admin'', ''engineer'', ''operator'', ''viewer''))
        );
    ', schema_name);

    -- 사용자 세션 테이블
    EXECUTE format('
        CREATE TABLE %I.user_sessions (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            user_id {{UUID}} NOT NULL REFERENCES %I.users(id) ON DELETE CASCADE,
            token_hash VARCHAR(255) NOT NULL,
            ip_address VARCHAR(45),
            user_agent {{TEXT}},
            expires_at {{TIMESTAMP}} NOT NULL,
            created_at {{TIMESTAMP}}
        );
    ', schema_name, schema_name);

    -- 공장 테이블
    EXECUTE format('
        CREATE TABLE %I.factories (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            name VARCHAR(100) NOT NULL,
            code VARCHAR(20) NOT NULL, -- factory code within company
            description {{TEXT}},
            location VARCHAR(200),
            timezone VARCHAR(50) DEFAULT ''UTC'',
            
            -- 연락처 정보
            manager_name VARCHAR(100),
            manager_email VARCHAR(100),
            manager_phone VARCHAR(20),
            
            -- Edge 서버 정보
            edge_server_id {{UUID}}, -- references public.edge_servers(id)
            
            is_active {{BOOLEAN}} DEFAULT {{TRUE}},
            created_at {{TIMESTAMP}},
            updated_at {{TIMESTAMP}},
            
            UNIQUE(code)
        );
    ', schema_name);

    -- 디바이스 그룹 테이블
    EXECUTE format('
        CREATE TABLE %I.device_groups (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            factory_id {{UUID}} NOT NULL REFERENCES %I.factories(id) ON DELETE CASCADE,
            name VARCHAR(100) NOT NULL,
            description {{TEXT}},
            parent_group_id {{UUID}} REFERENCES %I.device_groups(id) ON DELETE SET NULL,
            created_at {{TIMESTAMP}}
        );
    ', schema_name, schema_name, schema_name);

    -- 디바이스 테이블
    EXECUTE format('
        CREATE TABLE %I.devices (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            factory_id {{UUID}} NOT NULL REFERENCES %I.factories(id) ON DELETE CASCADE,
            device_group_id {{UUID}} REFERENCES %I.device_groups(id) ON DELETE SET NULL,
            
            -- 디바이스 기본 정보
            name VARCHAR(100) NOT NULL,
            description {{TEXT}},
            device_type VARCHAR(50), -- PLC, RTU, Sensor, Gateway, HMI
            manufacturer VARCHAR(100),
            model VARCHAR(100),
            serial_number VARCHAR(100),
            
            -- 통신 설정
            protocol_type VARCHAR(50) NOT NULL,
            endpoint VARCHAR(255) NOT NULL,
            config {{TEXT}} NOT NULL, -- JSON
            
            -- 수집 설정
            polling_interval INTEGER DEFAULT 1000,
            timeout INTEGER DEFAULT 3000,
            retry_count INTEGER DEFAULT 3,
            
            -- 상태 정보
            is_enabled {{BOOLEAN}} DEFAULT {{TRUE}},
            installation_date DATE,
            last_maintenance DATE,
            
            created_by {{UUID}} REFERENCES %I.users(id),
            created_at {{TIMESTAMP}},
            updated_at {{TIMESTAMP}},
            
            CONSTRAINT chk_polling_interval CHECK (polling_interval > 0),
            CONSTRAINT chk_timeout CHECK (timeout > 0)
        );
    ', schema_name, schema_name, schema_name, schema_name);

    -- 디바이스 상태 테이블
    EXECUTE format('
        CREATE TABLE %I.device_status (
            device_id {{UUID}} PRIMARY KEY REFERENCES %I.devices(id) ON DELETE CASCADE,
            connection_status VARCHAR(20) NOT NULL DEFAULT ''disconnected'',
            last_communication {{TIMESTAMP}},
            error_count INTEGER DEFAULT 0,
            last_error {{TEXT}},
            response_time INTEGER,
            
            -- 추가 진단 정보
            firmware_version VARCHAR(50),
            hardware_info {{TEXT}}, -- JSON
            diagnostic_data {{TEXT}}, -- JSON
            
            updated_at {{TIMESTAMP}},
            
            CONSTRAINT chk_connection_status CHECK (connection_status IN (''connected'', ''disconnected'', ''error'', ''maintenance''))
        );
    ', schema_name, schema_name);

    -- 데이터 포인트 테이블
    EXECUTE format('
        CREATE TABLE %I.data_points (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            device_id {{UUID}} NOT NULL REFERENCES %I.devices(id) ON DELETE CASCADE,
            
            -- 포인트 기본 정보
            name VARCHAR(100) NOT NULL,
            description {{TEXT}},
            address INTEGER NOT NULL,
            data_type VARCHAR(20) NOT NULL,
            access_mode VARCHAR(10) DEFAULT ''read'',
            
            -- 엔지니어링 정보
            unit VARCHAR(20),
            scaling_factor DECIMAL(10,4) DEFAULT 1.0,
            scaling_offset DECIMAL(10,4) DEFAULT 0.0,
            min_value DECIMAL(15,4),
            max_value DECIMAL(15,4),
            
            -- 수집 설정
            is_enabled {{BOOLEAN}} DEFAULT {{TRUE}},
            scan_rate INTEGER, -- override device polling interval
            deadband DECIMAL(10,4) DEFAULT 0,
            
            -- 메타데이터
            config {{TEXT}}, -- JSON
            tags {{TEXT}}, -- JSON array
            
            created_at {{TIMESTAMP}},
            updated_at {{TIMESTAMP}},
            
            UNIQUE(device_id, address),
            CONSTRAINT chk_data_type CHECK (data_type IN (''bool'', ''int16'', ''int32'', ''uint16'', ''uint32'', ''float'', ''double'', ''string'')),
            CONSTRAINT chk_access_mode CHECK (access_mode IN (''read'', ''write'', ''readwrite''))
        );
    ', schema_name, schema_name);

    -- 현재값 테이블
    EXECUTE format('
        CREATE TABLE %I.current_values (
            point_id {{UUID}} PRIMARY KEY REFERENCES %I.data_points(id) ON DELETE CASCADE,
            value DECIMAL(15,4),
            raw_value DECIMAL(15,4),
            string_value {{TEXT}},
            quality VARCHAR(20) DEFAULT ''good'',
            timestamp {{TIMESTAMP}},
            
            CONSTRAINT chk_quality CHECK (quality IN (''good'', ''bad'', ''uncertain'', ''not_connected''))
        );
    ', schema_name, schema_name);

    -- 가상 포인트 테이블
    EXECUTE format('
        CREATE TABLE %I.virtual_points (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            factory_id {{UUID}} REFERENCES %I.factories(id) ON DELETE CASCADE,
            
            -- 가상 포인트 정보
            name VARCHAR(100) NOT NULL,
            description {{TEXT}},
            formula {{TEXT}} NOT NULL,
            data_type VARCHAR(20) NOT NULL DEFAULT ''float'',
            unit VARCHAR(20),
            
            -- 계산 설정
            calculation_interval INTEGER DEFAULT 1000,
            is_enabled {{BOOLEAN}} DEFAULT {{TRUE}},
            
            -- 메타데이터
            category VARCHAR(50),
            tags {{TEXT}}, -- JSON array
            
            created_by {{UUID}} REFERENCES %I.users(id),
            created_at {{TIMESTAMP}},
            updated_at {{TIMESTAMP}}
        );
    ', schema_name, schema_name, schema_name);

    -- 가상 포인트 입력 매핑
    EXECUTE format('
        CREATE TABLE %I.virtual_point_inputs (
            id {{UUID}} PRIMARY KEY DEFAULT uuid_generate_v4(),
            virtual_point_id {{UUID}} NOT NULL REFERENCES %I.virtual_points(id) ON DELETE CASCADE,
            variable_name VARCHAR(50) NOT NULL,
            source_type VARCHAR(20) NOT NULL,
            source_id {{UUID}},
            constant_value DECIMAL(15,4),
            
            UNIQUE(virtual_point_id, variable_name),
            CONSTRAINT chk_source_type CHECK (source_type IN (''data_point'', ''virtual_point'', ''constant''))
        );
    ', schema_name, schema_name);

    -- 가상 포인트 현재값
    EXECUTE format('
        CREATE TABLE %I.virtual_point_values (
            virtual_point_id {{UUID}} PRIMARY KEY REFERENCES %I.virtual_points(id) ON DELETE CASCADE,
            value DECIMAL(15,4),
            quality VARCHAR(20) DEFAULT ''good'',
            last_calculated {{TIMESTAMP}},
            
            CONSTRAINT chk_vp_quality CHECK (quality IN (''good'', ''bad'', ''uncertain''))
        );
    ', schema_name, schema_name);

    RAISE NOTICE 'Core tables created for schema %', schema_name;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- 테넌트 삭제 함수
-- ===============================================================================
CREATE OR REPLACE FUNCTION drop_tenant_schema(tenant_code TEXT)
RETURNS VOID AS $$
DECLARE
    schema_name TEXT := 'tenant_' || tenant_code;
BEGIN
    EXECUTE format('DROP SCHEMA IF EXISTS %I CASCADE', schema_name);
    RAISE NOTICE 'Tenant schema % dropped', schema_name;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- 새 테넌트 등록 함수
-- ===============================================================================
CREATE OR REPLACE FUNCTION register_new_tenant(
    p_company_name TEXT,
    p_company_code TEXT,
    p_contact_email TEXT,
    p_contact_name TEXT DEFAULT NULL,
    p_subscription_plan TEXT DEFAULT 'starter'
)
RETURNS {{UUID}} AS $$
DECLARE
    new_tenant_id {{UUID}};
    schema_name TEXT;
BEGIN
    -- 회사 코드 중복 체크
    IF EXISTS (SELECT 1 FROM tenants WHERE company_code = p_company_code) THEN
        RAISE EXCEPTION 'Company code % already exists', p_company_code;
    END IF;
    
    -- 이메일 중복 체크
    IF EXISTS (SELECT 1 FROM tenants WHERE contact_email = p_contact_email) THEN
        RAISE EXCEPTION 'Contact email % already exists', p_contact_email;
    END IF;
    
    -- 테넌트 생성
    INSERT INTO tenants (
        company_name, company_code, domain, 
        contact_name, contact_email, subscription_plan,
        trial_end_date
    ) VALUES (
        p_company_name, p_company_code, 
        p_company_code || '.pulseone.com',
        p_contact_name, p_contact_email, p_subscription_plan,
        datetime('now', '+30 days') -- SQLite: CURRENT_TIMESTAMP + INTERVAL '30 days' for PostgreSQL
    ) RETURNING id INTO new_tenant_id;
    
    -- 테넌트 스키마 생성
    PERFORM create_tenant_schema(p_company_code);
    
    -- 기본 관리자 사용자 생성
    schema_name := 'tenant_' || p_company_code;
    EXECUTE format('
        INSERT INTO %I.users (username, email, password_hash, full_name, role, permissions)
        VALUES ($1, $2, $3, $4, $5, $6)
    ', schema_name) 
    USING 
        'admin', 
        p_contact_email, 
        '$2b$10$K7L/8X.WP4.r3V9.zN1k6.1J2.3m9R8k.Hj7F.2s4P.Q9L.5X8Y6a', -- 기본 비밀번호: admin123!
        COALESCE(p_contact_name, 'Administrator'),
        'company_admin',
        '["manage_company", "manage_factories", "manage_users", "view_all_data"]';
    
    -- 기본 공장 생성
    EXECUTE format('
        INSERT INTO %I.factories (name, code, description)
        VALUES ($1, $2, $3)
    ', schema_name)
    USING 
        'Main Factory',
        'main',
        'Default factory for ' || p_company_name;
    
    RAISE NOTICE 'New tenant % registered with ID %', p_company_name, new_tenant_id;
    RETURN new_tenant_id;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- Edge 서버 등록 함수
-- ===============================================================================
CREATE OR REPLACE FUNCTION register_edge_server(
    p_tenant_id {{UUID}},
    p_server_name TEXT,
    p_factory_name TEXT DEFAULT NULL,
    p_location TEXT DEFAULT NULL
)
RETURNS TEXT AS $$
DECLARE
    registration_token TEXT;
    activation_code TEXT;
BEGIN
    -- 등록 토큰 생성 (UUID 기반)
    registration_token := uuid_generate_v4()::TEXT;
    
    -- 활성화 코드 생성 (6자리 숫자)
    activation_code := LPAD((RANDOM() * 999999)::INTEGER::TEXT, 6, '0');
    
    -- Edge 서버 등록
    INSERT INTO edge_servers (
        tenant_id, server_name, factory_name, location,
        registration_token, activation_code, status
    ) VALUES (
        p_tenant_id, p_server_name, p_factory_name, p_location,
        registration_token, activation_code, 'pending'
    );
    
    RETURN registration_token;
END;
$$ LANGUAGE plpgsql;

-- ===============================================================================
-- 사용량 기록 함수
-- ===============================================================================
CREATE OR REPLACE FUNCTION record_usage(
    p_tenant_id {{UUID}},
    p_edge_server_id {{UUID}} DEFAULT NULL,
    p_metric_type TEXT,
    p_metric_value BIGINT,
    p_measurement_date DATE DEFAULT CURRENT_DATE,
    p_measurement_hour INTEGER DEFAULT NULL
)
RETURNS VOID AS $$
BEGIN
    INSERT INTO usage_metrics (
        tenant_id, edge_server_id, metric_type, 
        metric_value, measurement_date, measurement_hour
    ) VALUES (
        p_tenant_id, p_edge_server_id, p_metric_type,
        p_metric_value, p_measurement_date, p_measurement_hour
    )
    ON CONFLICT (tenant_id, edge_server_id, metric_type, measurement_date, measurement_hour)
    DO UPDATE SET 
        metric_value = usage_metrics.metric_value + EXCLUDED.metric_value,
        created_at = CURRENT_TIMESTAMP;
END;
$$ LANGUAGE plpgsql;