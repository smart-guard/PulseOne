-- =============================================================================
-- data/sql/14-sample-users.sql
-- RBAC 시뮬레이션을 위한 샘플 사용자 데이터
-- =============================================================================

INSERT OR IGNORE INTO users (
    tenant_id, username, email, password_hash, full_name, role, department, position, is_active
) VALUES 
-- 1. 테넌트 1 (Smart Factory Korea) 관리자
(1, 'sfk_admin', 'admin@smartfactory.co.kr', '$2b$10$EpjXWzO2yzcaEUPPP5PRLOJHWLgH6Cq7.V/vV5V6V7V8V9V0V1V2V', '강지훈', 'company_admin', '관리팀', '팀장', 1),

-- 2. 테넌트 1 (Smart Factory Korea) 엔지니어
(1, 'sfk_engineer', 'engineer1@smartfactory.co.kr', '$2b$10$EpjXWzO2yzcaEUPPP5PRLOJHWLgH6Cq7.V/vV5V6V7V8V9V0V1V2V', '이동욱', 'engineer', '기술지원팀', '대리', 1),

-- 3. 테넌트 1 (Smart Factory Korea) 운전원
(1, 'sfk_operator', 'operator1@smartfactory.co.kr', '$2b$10$EpjXWzO2yzcaEUPPP5PRLOJHWLgH6Cq7.V/vV5V6V7V8V9V0V1V2V', '김철수', 'operator', '생산1팀', '사원', 1),

-- 4. 테넌트 1 (Smart Factory Korea) 조회전용
(1, 'sfk_viewer', 'viewer1@smartfactory.co.kr', '$2b$10$EpjXWzO2yzcaEUPPP5PRLOJHWLgH6Cq7.V/vV5V6V7V8V9V0V1V2V', '박영희', 'viewer', '기능관리팀', '사원', 1),

-- 5. 테넌트 2 (Global Manufacturing Inc) 매니저
(2, 'gmi_manager', 'manager@globalmfg.com', '$2b$10$EpjXWzO2yzcaEUPPP5PRLOJHWLgH6Cq7.V/vV5V6V7V8V9V0V1V2V', 'John Smith', 'manager', 'Operations', 'Manager', 1),

-- 6. 테넌트 2 (Global Manufacturing Inc) 엔지니어
(2, 'gmi_engineer', 'eng@globalmfg.com', '$2b$10$EpjXWzO2yzcaEUPPP5PRLOJHWLgH6Cq7.V/vV5V6V7V8V9V0V1V2V', 'Jane Doe', 'engineer', 'Maintenance', 'Senior Engineer', 1);
