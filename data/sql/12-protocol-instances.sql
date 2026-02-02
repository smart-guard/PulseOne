
-- ============================================================================
-- 12-protocol-instances.sql
-- 프로토콜 인스턴스 관리를 위한 테이블 (MQTT vhost 등 대응)
-- ============================================================================

CREATE TABLE IF NOT EXISTS protocol_instances (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    protocol_id INTEGER NOT NULL,
    instance_name VARCHAR(100) NOT NULL,
    description TEXT,
    
    -- 인스턴스별 연결 정보 (vhost, broker ID/PW 등)
    vhost VARCHAR(50) DEFAULT '/',
    api_key VARCHAR(100),
    api_key_updated_at DATETIME,
    
    connection_params TEXT,             -- JSON: 인스턴스 전용 상세 파라미터
    
    -- 상태 및 관리
    is_enabled INTEGER DEFAULT 1,
    status VARCHAR(20) DEFAULT 'STOPPED', -- RUNNING, STOPPED, ERROR
    
    -- 메타데이터
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    tenant_id INTEGER REFERENCES tenants(id) ON DELETE SET NULL,
    
    FOREIGN KEY (protocol_id) REFERENCES protocols(id) ON DELETE CASCADE
);

-- 기존 devices 테이블에 protocol_instance_id 컬럼 추가 (비어있을 시 기본값 처리)
-- SQLite는 ALTER TABLE ADD COLUMN 시 FK 제약을 직접 추가하기 까다롭지만 컬럼 자체는 추가 가능
-- 차후 마이그레이션 도구에서 처리하도록 추천하나, 여기서는 구조만 정의
ALTER TABLE devices ADD COLUMN protocol_instance_id INTEGER REFERENCES protocol_instances(id);
