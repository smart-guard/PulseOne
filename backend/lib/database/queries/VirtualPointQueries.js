// =============================================================================
// backend/lib/database/queries/VirtualPointQueries.js
// 가상포인트 SQL 쿼리 모음 - 문법 오류 수정
// =============================================================================

class VirtualPointQueries {
  // 기본 가상포인트 목록 조회
  static getVirtualPointsList() {
    return `
      SELECT 
        vp.id,
        vp.tenant_id,
        vp.scope_type,
        vp.site_id,
        vp.device_id,
        vp.name,
        vp.description,
        vp.formula,
        vp.data_type,
        vp.unit,
        vp.calculation_interval,
        vp.calculation_trigger,
        vp.is_enabled,
        vp.category,
        vp.tags,
        vp.execution_type,
        vp.cache_duration_ms,
        vp.error_handling,
        vp.last_error,
        vp.execution_count,
        vp.avg_execution_time_ms,
        vp.last_execution_time,
        vp.created_at,
        vp.updated_at
      FROM virtual_points vp
      WHERE 1=1
    `;
  }

  // 필터 조건들
  static addTenantFilter() {
    return ` AND vp.tenant_id = ?`;
  }

  static addSiteFilter() {
    return ` AND vp.site_id = ?`;
  }

  static addDeviceFilter() {
    return ` AND vp.device_id = ?`;
  }

  static addScopeTypeFilter() {
    return ` AND vp.scope_type = ?`;
  }

  static addEnabledFilter() {
    return ` AND vp.is_enabled = ?`;
  }

  static addCategoryFilter() {
    return ` AND vp.category = ?`;
  }

  static addTriggerFilter() {
    return ` AND vp.calculation_trigger = ?`;
  }

  static addSearchFilter() {
    return ` AND (vp.name LIKE ? OR vp.description LIKE ?)`;
  }

  static addLimit() {
    return ` LIMIT ?`;
  }

  // 그룹화 및 정렬
  static getGroupByAndOrder() {
    return ` ORDER BY vp.created_at DESC`;
  }

  // 상세 조회
  static getVirtualPointById() {
    return `
      SELECT * FROM virtual_points WHERE id = ?
    `;
  }

  // 삭제
  static deleteVirtualPoint() {
    return `DELETE FROM virtual_points WHERE id = ?`;
  }

  // 단순 생성 쿼리
  static createVirtualPointSimple() {
    return `
      INSERT INTO virtual_points (
        tenant_id, name, formula, description, data_type, unit,
        calculation_trigger, is_enabled, category, created_at, updated_at
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'), datetime('now'))
    `;
  }

  // 단순 업데이트 쿼리
  static updateVirtualPointSimple() {
    return `
      UPDATE virtual_points SET
        name = ?, formula = ?, description = ?, data_type = ?,
        unit = ?, calculation_trigger = ?, is_enabled = ?,
        category = ?, updated_at = datetime('now')
      WHERE id = ?
    `;
  }

  // 카테고리별 통계
  static getStatsByCategorySimple() {
    return `
      SELECT 
        category,
        COUNT(*) as count,
        SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_count
      FROM virtual_points
      WHERE tenant_id = ?
      GROUP BY category
      ORDER BY count DESC
    `;
  }

  // 성능 통계
  static getPerformanceStatsSimple() {
    return `
      SELECT 
        COUNT(*) as total_points,
        SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_points
      FROM virtual_points
      WHERE tenant_id = ?
    `;
  }
}

module.exports = VirtualPointQueries;
