-- ============================================================================
-- 20260205051000_add_broker_type_to_protocol_instances.sql
-- 브로커 타입 구분을 위한 컬럼 추가 (INTERNAL, EXTERNAL)
-- ============================================================================

ALTER TABLE protocol_instances ADD COLUMN broker_type VARCHAR(20) DEFAULT 'INTERNAL';
