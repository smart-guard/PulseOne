-- =============================================================================
-- backend/lib/database/schemas/06-log-tables.sql
-- 로깅 시스템 테이블 (SQLite 버전) - 2025-08-14 최신 업데이트
-- PulseOne v2.1.0 완전 호환, 포괄적 감사 및 로깅 지원
-- =============================================================================

-- =============================================================================
-- 🔥🔥🔥 시스템 로그 테이블 - 전체 시스템 로그 중앙화
-- =============================================================================
CREATE TABLE IF NOT EXISTS system_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER,                               -- NULL = 시스템 전체 로그
    user_id INTEGER,
    
    -- 🔥 로그 기본 정보
    log_level VARCHAR(10) NOT NULL,                  -- DEBUG, INFO, WARN, ERROR, FATAL
    module VARCHAR(50) NOT NULL,                     -- collector, backend, frontend, database, alarm, etc.
    component VARCHAR(50),                           -- 세부 컴포넌트명
    message TEXT NOT NULL,
    
    -- 🔥 컨텍스트 정보
    ip_address VARCHAR(45),
    user_agent TEXT,
    request_id VARCHAR(50),                          -- 요청 추적 ID
    session_id VARCHAR(100),                         -- 세션 ID
    
    -- 🔥 실행 컨텍스트
    thread_id VARCHAR(20),                           -- 스레드 ID
    process_id INTEGER,                              -- 프로세스 ID
    correlation_id VARCHAR(50),                      -- 상관관계 ID (분산 추적)
    
    -- 🔥 에러 정보 (ERROR/FATAL 레벨용)
    error_code VARCHAR(20),
    stack_trace TEXT,
    exception_type VARCHAR(100),
    
    -- 🔥 성능 정보
    execution_time_ms INTEGER,                       -- 실행 시간
    memory_usage_kb INTEGER,                         -- 메모리 사용량
    cpu_time_ms INTEGER,                             -- CPU 시간
    
    -- 🔥 메타데이터
    details TEXT,                                    -- JSON 형태 (추가 세부 정보)
    tags TEXT,                                       -- JSON 배열 (검색 태그)
    
    -- 🔥 위치 정보
    source_file VARCHAR(255),                        -- 소스 파일명
    source_line INTEGER,                             -- 소스 라인 번호
    source_function VARCHAR(100),                    -- 함수명
    
    -- 🔥 감사 정보
    hostname VARCHAR(100),                           -- 로그 생성 호스트
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE SET NULL,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL,
    
    -- 🔥 제약조건
    CONSTRAINT chk_log_level CHECK (log_level IN ('DEBUG', 'INFO', 'WARN', 'ERROR', 'FATAL'))
);

-- =============================================================================
-- 사용자 활동 로그 - 상세한 감사 추적
-- =============================================================================
CREATE TABLE IF NOT EXISTS user_activities (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER,
    tenant_id INTEGER,
    
    -- 🔥 활동 정보
    action VARCHAR(50) NOT NULL,                     -- login, logout, create, update, delete, view, export, etc.
    resource_type VARCHAR(50),                       -- device, data_point, alarm, user, system_setting, etc.
    resource_id INTEGER,
    resource_name VARCHAR(100),                      -- 리소스 이름 (빠른 검색용)
    
    -- 🔥 변경 정보 (create/update/delete 액션용)
    old_values TEXT,                                 -- JSON 형태 (변경 전 값)
    new_values TEXT,                                 -- JSON 형태 (변경 후 값)
    changed_fields TEXT,                             -- JSON 배열 (변경된 필드 목록)
    
    -- 🔥 요청 정보
    http_method VARCHAR(10),                         -- GET, POST, PUT, DELETE
    endpoint VARCHAR(255),                           -- API 엔드포인트
    query_params TEXT,                               -- JSON 형태 (쿼리 파라미터)
    request_body TEXT,                               -- JSON 형태 (요청 본문)
    response_code INTEGER,                           -- HTTP 응답 코드
    
    -- 🔥 세션 정보
    session_id VARCHAR(100),
    ip_address VARCHAR(45),
    user_agent TEXT,
    referer VARCHAR(255),
    
    -- 🔥 지리적 정보
    country VARCHAR(2),                              -- 국가 코드
    city VARCHAR(100),
    timezone VARCHAR(50),
    
    -- 🔥 결과 정보
    success INTEGER DEFAULT 1,                      -- 성공 여부
    error_message TEXT,                              -- 에러 메시지
    warning_messages TEXT,                           -- JSON 배열 (경고 메시지들)
    
    -- 🔥 성능 정보
    processing_time_ms INTEGER,                      -- 처리 시간
    affected_rows INTEGER,                           -- 영향받은 레코드 수
    
    -- 🔥 보안 정보
    risk_score INTEGER DEFAULT 0,                   -- 위험도 점수 (0-100)
    security_flags TEXT,                             -- JSON 배열 (보안 플래그)
    
    -- 🔥 메타데이터
    details TEXT,                                    -- JSON 형태 (추가 정보)
    tags TEXT,                                       -- JSON 배열 (분류 태그)
    
    -- 🔥 감사 정보
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE SET NULL,
    
    -- 🔥 제약조건
    CONSTRAINT chk_action CHECK (action IN ('login', 'logout', 'create', 'read', 'update', 'delete', 'view', 'export', 'import', 'execute', 'approve', 'reject')),
    CONSTRAINT chk_http_method CHECK (http_method IN ('GET', 'POST', 'PUT', 'PATCH', 'DELETE', 'HEAD', 'OPTIONS'))
);

-- =============================================================================
-- 통신 로그 테이블 - 프로토콜 통신 상세 기록
-- =============================================================================
CREATE TABLE IF NOT EXISTS communication_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER,
    data_point_id INTEGER,
    
    -- 🔥 통신 기본 정보
    direction VARCHAR(10) NOT NULL,                  -- request, response, notification
    protocol VARCHAR(20) NOT NULL,                   -- MODBUS_TCP, MODBUS_RTU, MQTT, BACNET, OPCUA
    operation VARCHAR(50),                           -- read_holding_registers, write_single_coil, publish, subscribe, etc.
    
    -- 🔥 프로토콜별 정보
    function_code INTEGER,                           -- Modbus 함수 코드
    address INTEGER,                                 -- 시작 주소
    register_count INTEGER,                          -- 레지스터 수
    slave_id INTEGER,                                -- Modbus 슬레이브 ID
    topic VARCHAR(255),                              -- MQTT 토픽
    qos INTEGER,                                     -- MQTT QoS
    
    -- 🔥 통신 데이터
    raw_data TEXT,                                   -- 원시 통신 데이터 (HEX)
    decoded_data TEXT,                               -- 해석된 데이터 (JSON)
    data_size INTEGER,                               -- 데이터 크기 (바이트)
    
    -- 🔥 결과 정보
    success INTEGER,                                 -- 성공 여부
    error_code INTEGER,                              -- 에러 코드
    error_message TEXT,                              -- 에러 메시지
    retry_count INTEGER DEFAULT 0,                  -- 재시도 횟수
    
    -- 🔥 성능 정보
    response_time INTEGER,                           -- 응답 시간 (밀리초)
    queue_time_ms INTEGER,                           -- 큐 대기 시간
    processing_time_ms INTEGER,                      -- 처리 시간
    network_latency_ms INTEGER,                      -- 네트워크 지연
    
    -- 🔥 품질 정보
    signal_strength INTEGER,                         -- 신호 강도 (무선 통신용)
    packet_loss_rate REAL,                          -- 패킷 손실률
    bit_error_rate REAL,                            -- 비트 에러율
    
    -- 🔥 세션 정보
    connection_id VARCHAR(50),                       -- 연결 ID
    session_id VARCHAR(50),                          -- 세션 ID
    sequence_number INTEGER,                         -- 시퀀스 번호
    
    -- 🔥 메타데이터
    details TEXT,                                    -- JSON 형태 (추가 정보)
    tags TEXT,                                       -- JSON 배열
    
    -- 🔥 감사 정보
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    edge_server_id INTEGER,                          -- 통신을 수행한 엣지 서버
    
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
    FOREIGN KEY (data_point_id) REFERENCES data_points(id) ON DELETE SET NULL,
    FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
    
    -- 🔥 제약조건
    CONSTRAINT chk_direction CHECK (direction IN ('request', 'response', 'notification', 'heartbeat')),
    CONSTRAINT chk_protocol CHECK (protocol IN ('MODBUS_TCP', 'MODBUS_RTU', 'MQTT', 'BACNET', 'OPCUA', 'ETHERNET_IP', 'HTTP'))
);

-- =============================================================================
-- 🔥🔥🔥 데이터 히스토리 테이블 - 시계열 데이터 저장
-- =============================================================================
CREATE TABLE IF NOT EXISTS data_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    point_id INTEGER NOT NULL,
    
    -- 🔥 값 정보
    value DECIMAL(15,4),                             -- 숫자 값
    string_value TEXT,                               -- 문자열 값
    bool_value INTEGER,                              -- 불린 값 (0 또는 1)
    raw_value DECIMAL(15,4),                         -- 스케일링 전 원시값
    
    -- 🔥 품질 정보
    quality VARCHAR(20),                             -- good, bad, uncertain, not_connected
    quality_code INTEGER,                            -- 숫자 품질 코드
    
    -- 🔥 시간 정보
    timestamp DATETIME NOT NULL,                     -- 값 타임스탬프
    source_timestamp DATETIME,                       -- 소스 타임스탬프
    
    -- 🔥 메타데이터
    change_type VARCHAR(20) DEFAULT 'value_change',  -- value_change, quality_change, manual_entry, calculated
    source VARCHAR(50) DEFAULT 'collector',          -- collector, manual, calculated, imported
    
    -- 🔥 압축 정보
    is_compressed INTEGER DEFAULT 0,                -- 압축 저장 여부
    compression_method VARCHAR(20),                  -- 압축 방법
    original_size INTEGER,                           -- 원본 크기
    
    FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    CONSTRAINT chk_quality CHECK (quality IN ('good', 'bad', 'uncertain', 'not_connected', 'device_failure', 'sensor_failure')),
    CONSTRAINT chk_change_type CHECK (change_type IN ('value_change', 'quality_change', 'manual_entry', 'calculated', 'imported')),
    CONSTRAINT chk_source CHECK (source IN ('collector', 'manual', 'calculated', 'imported', 'simulated'))
);

-- =============================================================================
-- 가상 포인트 히스토리
-- =============================================================================
CREATE TABLE IF NOT EXISTS virtual_point_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    virtual_point_id INTEGER NOT NULL,
    
    -- 🔥 계산 결과
    value DECIMAL(15,4),
    string_value TEXT,
    quality VARCHAR(20),
    
    -- 🔥 계산 정보
    calculation_time_ms INTEGER,                     -- 계산 소요 시간
    input_values TEXT,                               -- JSON: 계산에 사용된 입력값들
    formula_used TEXT,                               -- 사용된 수식
    
    -- 🔥 시간 정보
    timestamp DATETIME NOT NULL,
    calculation_timestamp DATETIME,                  -- 계산 시작 시간
    
    -- 🔥 메타데이터
    trigger_reason VARCHAR(50),                      -- timer, value_change, manual, api
    error_message TEXT,                              -- 계산 에러 메시지
    
    FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
    
    -- 🔥 제약조건
    CONSTRAINT chk_vp_quality CHECK (quality IN ('good', 'bad', 'uncertain', 'calculation_error')),
    CONSTRAINT chk_trigger_reason CHECK (trigger_reason IN ('timer', 'value_change', 'manual', 'api', 'system'))
);

-- =============================================================================
-- 알람 이벤트 로그 (상세한 알람 활동 기록)
-- =============================================================================
CREATE TABLE IF NOT EXISTS alarm_event_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    occurrence_id INTEGER NOT NULL,
    rule_id INTEGER NOT NULL,
    tenant_id INTEGER NOT NULL,
    
    -- 🔥 이벤트 정보
    event_type VARCHAR(30) NOT NULL,                -- triggered, acknowledged, cleared, escalated, suppressed, etc.
    previous_state VARCHAR(20),                     -- 이전 상태
    new_state VARCHAR(20),                          -- 새로운 상태
    
    -- 🔥 값 정보
    trigger_value TEXT,                             -- JSON: 트리거 값
    threshold_value TEXT,                           -- JSON: 임계값
    deviation REAL,                                 -- 편차
    
    -- 🔥 사용자 정보
    user_id INTEGER,                                -- 액션을 수행한 사용자
    user_comment TEXT,                              -- 사용자 코멘트
    
    -- 🔥 시스템 정보
    auto_action INTEGER DEFAULT 0,                 -- 자동 액션 여부
    escalation_level INTEGER DEFAULT 0,            -- 에스컬레이션 단계
    notification_sent INTEGER DEFAULT 0,           -- 알림 발송 여부
    
    -- 🔥 메타데이터
    details TEXT,                                   -- JSON: 추가 세부 정보
    context_data TEXT,                              -- JSON: 컨텍스트 데이터
    
    -- 🔥 감사 정보
    event_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    source_system VARCHAR(50) DEFAULT 'collector',  -- 이벤트 소스
    
    FOREIGN KEY (occurrence_id) REFERENCES alarm_occurrences(id) ON DELETE CASCADE,
    FOREIGN KEY (rule_id) REFERENCES alarm_rules(id) ON DELETE CASCADE,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL,
    
    -- 🔥 제약조건
    CONSTRAINT chk_event_type CHECK (event_type IN ('triggered', 'acknowledged', 'cleared', 'escalated', 'suppressed', 'shelved', 'unshelved', 'expired', 'updated')),
    CONSTRAINT chk_prev_state CHECK (previous_state IN ('active', 'acknowledged', 'cleared', 'suppressed', 'shelved', 'expired')),
    CONSTRAINT chk_new_state CHECK (new_state IN ('active', 'acknowledged', 'cleared', 'suppressed', 'shelved', 'expired'))
);

-- =============================================================================
-- 시스템 성능 로그 (성능 모니터링)
-- =============================================================================
CREATE TABLE IF NOT EXISTS performance_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- 🔥 성능 카테고리
    metric_category VARCHAR(50) NOT NULL,           -- system, database, network, application
    metric_name VARCHAR(100) NOT NULL,              -- cpu_usage, memory_usage, response_time, etc.
    metric_value REAL NOT NULL,
    metric_unit VARCHAR(20),                        -- %, MB, ms, count, etc.
    
    -- 🔥 컨텍스트 정보
    hostname VARCHAR(100),
    process_name VARCHAR(100),
    component VARCHAR(50),                          -- collector, backend, database, etc.
    
    -- 🔥 샘플링 정보
    sample_interval_sec INTEGER DEFAULT 60,         -- 샘플링 간격
    aggregation_type VARCHAR(20) DEFAULT 'instant', -- instant, average, min, max, sum
    sample_count INTEGER DEFAULT 1,                 -- 집계된 샘플 수
    
    -- 🔥 메타데이터
    details TEXT,                                   -- JSON: 추가 메트릭 정보
    tags TEXT,                                      -- JSON 배열: 태그
    
    -- 🔥 시간 정보
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 🔥 제약조건
    CONSTRAINT chk_metric_category CHECK (metric_category IN ('system', 'database', 'network', 'application', 'security')),
    CONSTRAINT chk_aggregation_type CHECK (aggregation_type IN ('instant', 'average', 'min', 'max', 'sum', 'count'))
);

-- =============================================================================
-- 인덱스 생성 (성능 최적화)
-- =============================================================================

-- system_logs 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_system_logs_tenant ON system_logs(tenant_id);
CREATE INDEX IF NOT EXISTS idx_system_logs_level ON system_logs(log_level);
CREATE INDEX IF NOT EXISTS idx_system_logs_module ON system_logs(module);
CREATE INDEX IF NOT EXISTS idx_system_logs_created ON system_logs(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_system_logs_user ON system_logs(user_id);
CREATE INDEX IF NOT EXISTS idx_system_logs_request ON system_logs(request_id);
CREATE INDEX IF NOT EXISTS idx_system_logs_session ON system_logs(session_id);

-- user_activities 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_user_activities_user ON user_activities(user_id);
CREATE INDEX IF NOT EXISTS idx_user_activities_tenant ON user_activities(tenant_id);
CREATE INDEX IF NOT EXISTS idx_user_activities_timestamp ON user_activities(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_user_activities_action ON user_activities(action);
CREATE INDEX IF NOT EXISTS idx_user_activities_resource ON user_activities(resource_type, resource_id);
CREATE INDEX IF NOT EXISTS idx_user_activities_session ON user_activities(session_id);
CREATE INDEX IF NOT EXISTS idx_user_activities_success ON user_activities(success);

-- communication_logs 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_communication_logs_device ON communication_logs(device_id);
CREATE INDEX IF NOT EXISTS idx_communication_logs_timestamp ON communication_logs(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_communication_logs_protocol ON communication_logs(protocol);
CREATE INDEX IF NOT EXISTS idx_communication_logs_success ON communication_logs(success);
CREATE INDEX IF NOT EXISTS idx_communication_logs_direction ON communication_logs(direction);
CREATE INDEX IF NOT EXISTS idx_communication_logs_data_point ON communication_logs(data_point_id);

-- data_history 테이블 인덱스 (시계열 데이터 최적화)
CREATE INDEX IF NOT EXISTS idx_data_history_point_time ON data_history(point_id, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_data_history_timestamp ON data_history(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_data_history_quality ON data_history(quality);
CREATE INDEX IF NOT EXISTS idx_data_history_source ON data_history(source);
CREATE INDEX IF NOT EXISTS idx_data_history_change_type ON data_history(change_type);

-- virtual_point_history 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_virtual_point_history_point_time ON virtual_point_history(virtual_point_id, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_virtual_point_history_timestamp ON virtual_point_history(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_virtual_point_history_quality ON virtual_point_history(quality);
CREATE INDEX IF NOT EXISTS idx_virtual_point_history_trigger ON virtual_point_history(trigger_reason);

-- alarm_event_logs 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_alarm_event_logs_occurrence ON alarm_event_logs(occurrence_id);
CREATE INDEX IF NOT EXISTS idx_alarm_event_logs_rule ON alarm_event_logs(rule_id);
CREATE INDEX IF NOT EXISTS idx_alarm_event_logs_tenant ON alarm_event_logs(tenant_id);
CREATE INDEX IF NOT EXISTS idx_alarm_event_logs_time ON alarm_event_logs(event_time DESC);
CREATE INDEX IF NOT EXISTS idx_alarm_event_logs_type ON alarm_event_logs(event_type);
CREATE INDEX IF NOT EXISTS idx_alarm_event_logs_user ON alarm_event_logs(user_id);

-- performance_logs 테이블 인덱스
CREATE INDEX IF NOT EXISTS idx_performance_logs_timestamp ON performance_logs(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_performance_logs_category ON performance_logs(metric_category);
CREATE INDEX IF NOT EXISTS idx_performance_logs_name ON performance_logs(metric_name);
CREATE INDEX IF NOT EXISTS idx_performance_logs_hostname ON performance_logs(hostname);
CREATE INDEX IF NOT EXISTS idx_performance_logs_component ON performance_logs(component);
CREATE INDEX IF NOT EXISTS idx_performance_logs_category_name_time ON performance_logs(metric_category, metric_name, timestamp DESC);