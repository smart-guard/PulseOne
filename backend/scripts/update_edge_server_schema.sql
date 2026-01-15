-- Migration: Add is_deleted and is_enabled to edge_servers
-- Date: 2026-01-12

-- 1. is_enabled 컬럼 추가 (기존 데이터는 1로 설정)
ALTER TABLE edge_servers ADD COLUMN is_enabled INTEGER DEFAULT 1;

-- 2. is_deleted 컬럼 추가 (기존 데이터는 0으로 설정)
ALTER TABLE edge_servers ADD COLUMN is_deleted INTEGER DEFAULT 0;

-- 인덱스 추가 (조회 성능 최적화)
CREATE INDEX idx_edge_servers_tenant_deleted ON edge_servers(tenant_id, is_deleted);
