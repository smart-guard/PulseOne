-- Protocol Instances 테이블에 tenant_id 컬럼 추가
-- 멀티 테넌트 지원을 위해 각 인스턴스가 특정 테넌트에 속하거나(NULL일 경우 전체 공유) 할당되도록 함.
ALTER TABLE protocol_instances ADD COLUMN tenant_id INTEGER REFERENCES tenants(id) ON DELETE SET NULL;
