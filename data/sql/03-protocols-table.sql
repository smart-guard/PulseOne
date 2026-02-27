-- ============================================================================
-- backend/lib/database/schemas/08-protocols-table.sql
-- 프로토콜 정보 전용 테이블 - 하드코딩 제거
-- ============================================================================

-- =============================================================================
-- 프로토콜 정보 테이블 
-- =============================================================================
CREATE TABLE IF NOT EXISTS protocols (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 기본 정보
    protocol_type VARCHAR(50) NOT NULL UNIQUE,      -- MODBUS_TCP, MODBUS_RTU, MQTT, etc.
    display_name VARCHAR(100) NOT NULL,             -- "Modbus TCP", "MQTT", etc.
    description TEXT,                               -- 상세 설명
    
    -- 네트워크 정보
    default_port INTEGER,                           -- 기본 포트 (502, 1883, etc.)
    uses_serial INTEGER DEFAULT 0,                 -- 시리얼 통신 사용 여부
    requires_broker INTEGER DEFAULT 0,             -- 브로커 필요 여부 (MQTT 등)
    
    -- 기능 지원 정보 (JSON)
    supported_operations TEXT,                      -- ["read_coils", etc.]
    supported_data_types TEXT,                      -- ["boolean", "int16", "float32", etc.]
    connection_params TEXT,                         -- 연결에 필요한 파라미터 스키마
    capabilities TEXT DEFAULT '{}',                 -- 프로토콜별 특수 역량 (JSON)
    
    -- 설정 정보
    default_polling_interval INTEGER DEFAULT 1000, -- 기본 수집 주기 (ms)
    default_timeout INTEGER DEFAULT 5000,          -- 기본 타임아웃 (ms)
    max_concurrent_connections INTEGER DEFAULT 1,   -- 최대 동시 연결 수
    
    -- 상태 정보
    is_enabled INTEGER DEFAULT 1,                  -- 프로토콜 활성화 여부
    is_deprecated INTEGER DEFAULT 0,               -- 사용 중단 예정
    min_firmware_version VARCHAR(20),              -- 최소 펌웨어 버전
    
    -- 분류 정보
    category VARCHAR(50),                           -- industrial, iot, building_automation, etc.
    vendor VARCHAR(100),                            -- 제조사/개발사
    standard_reference VARCHAR(100),               -- 표준 문서 참조
    
    -- 메타데이터
    created_at DATETIME DEFAULT (datetime('now', 'localtime')),
    updated_at DATETIME DEFAULT (datetime('now', 'localtime')),
    
    -- 제약조건
    CONSTRAINT chk_category CHECK (category IN ('industrial', 'iot', 'building_automation', 'network', 'web'))
);

