-- =============================================================================
-- devices & sites 테이블 스키마 업데이트 (Soft Delete 지원)
-- =============================================================================

-- 1단계: devices 테이블에 is_deleted 컬럼 추가
ALTER TABLE devices ADD COLUMN is_deleted INTEGER DEFAULT 0;

-- 2단계: sites 테이블에 is_deleted 컬럼 추가
ALTER TABLE sites ADD COLUMN is_deleted INTEGER DEFAULT 0;

-- 3단계: 인덱스 추가 (조회 성능 최적화)
CREATE INDEX IF NOT EXISTS idx_devices_deleted ON devices(is_deleted);
CREATE INDEX IF NOT EXISTS idx_sites_deleted ON sites(is_deleted);

-- =============================================================================
-- 데이터 포인트 활성화 통계 뷰 업데이트 (필요 시)
-- =============================================================================
