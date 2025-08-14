// =============================================================================
// backend/lib/database/VirtualPointQueries.js
// 가상포인트 SQL 쿼리 모음 - AlarmQueries.js 패턴 준수
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
        vp.updated_at,
        COUNT(vpi.id) as input_count,
        vpv.value as current_value,
        vpv.string_value as current_string_value,
        vpv.quality,
        vpv.last_calculated,
        vpv.calculation_error
      FROM virtual_points vp
      LEFT JOIN virtual_point_inputs vpi ON vp.id = vpi.virtual_point_id
      LEFT JOIN virtual_point_values vpv ON vp.id = vpv.virtual_point_id
      WHERE 1=1
    `;
  }

  // 필터 조건 추가
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

  // 그룹화 및 정렬
  static getGroupByAndOrder() {
    return ` GROUP BY vp.id ORDER BY vp.created_at DESC`;
  }

  static addLimit() {
    return ` LIMIT ?`;
  }

  // 가상포인트 상세 조회
  static getVirtualPointById() {
    return `
      SELECT 
        vp.*,
        vpv.value as current_value,
        vpv.string_value as current_string_value,
        vpv.quality,
        vpv.last_calculated,
        vpv.calculation_error,
        vpv.input_values
      FROM virtual_points vp
      LEFT JOIN virtual_point_values vpv ON vp.id = vpv.virtual_point_id
      WHERE vp.id = ?
    `;
  }

  // 가상포인트 입력 매핑 조회
  static getVirtualPointInputs() {
    return `
      SELECT 
        vpi.*,
        CASE 
          WHEN vpi.source_type = 'data_point' THEN dp.name
          WHEN vpi.source_type = 'virtual_point' THEN vp2.name
          ELSE NULL
        END as source_name,
        CASE 
          WHEN vpi.source_type = 'data_point' THEN dp.unit
          WHEN vpi.source_type = 'virtual_point' THEN vp2.unit
          ELSE NULL
        END as source_unit
      FROM virtual_point_inputs vpi
      LEFT JOIN data_points dp ON vpi.source_type = 'data_point' AND vpi.source_id = dp.id
      LEFT JOIN virtual_points vp2 ON vpi.source_type = 'virtual_point' AND vpi.source_id = vp2.id
      WHERE vpi.virtual_point_id = ?
      ORDER BY vpi.variable_name
    `;
  }

  // 가상포인트 의존성 조회
  static getVirtualPointDependencies() {
    return `
      SELECT 
        vpd.*,
        CASE 
          WHEN vpd.depends_on_type = 'data_point' THEN dp.name
          WHEN vpd.depends_on_type = 'virtual_point' THEN vp2.name
        END as dependency_name
      FROM virtual_point_dependencies vpd
      LEFT JOIN data_points dp ON vpd.depends_on_type = 'data_point' AND vpd.depends_on_id = dp.id
      LEFT JOIN virtual_points vp2 ON vpd.depends_on_type = 'virtual_point' AND vpd.depends_on_id = vp2.id
      WHERE vpd.virtual_point_id = ?
    `;
  }

  // 가상포인트 실행 이력 조회
  static getExecutionHistory() {
    return `
      SELECT *
      FROM virtual_point_execution_history
      WHERE virtual_point_id = ?
      ORDER BY execution_time DESC
      LIMIT ?
    `;
  }

  // 가상포인트 생성
  static createVirtualPoint() {
    return `
      INSERT INTO virtual_points (
        tenant_id, scope_type, site_id, device_id,
        name, description, formula, data_type, unit,
        calculation_interval, calculation_trigger, is_enabled,
        category, tags, execution_type, dependencies,
        cache_duration_ms, error_handling, created_by
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `;
  }

  // 가상포인트 업데이트
  static updateVirtualPoint() {
    return `
      UPDATE virtual_points SET
        name = ?, description = ?, formula = ?, data_type = ?,
        unit = ?, calculation_interval = ?, calculation_trigger = ?,
        is_enabled = ?, category = ?, tags = ?, execution_type = ?,
        dependencies = ?, cache_duration_ms = ?, error_handling = ?,
        updated_at = CURRENT_TIMESTAMP
      WHERE id = ?
    `;
  }

  // 가상포인트 삭제
  static deleteVirtualPoint() {
    return `DELETE FROM virtual_points WHERE id = ?`;
  }

  // 입력 매핑 생성
  static createVirtualPointInput() {
    return `
      INSERT INTO virtual_point_inputs (
        virtual_point_id, variable_name, source_type, source_id,
        constant_value, source_formula, data_processing, time_window_seconds
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    `;
  }

  // 입력 매핑 삭제
  static deleteVirtualPointInputs() {
    return `DELETE FROM virtual_point_inputs WHERE virtual_point_id = ?`;
  }

  // 가상포인트 값 업데이트
  static upsertVirtualPointValue() {
    return `
      INSERT OR REPLACE INTO virtual_point_values (
        virtual_point_id, value, string_value, quality,
        last_calculated, calculation_error, input_values
      ) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP, ?, ?)
    `;
  }

  // 실행 통계 업데이트
  static updateExecutionStats() {
    return `
      UPDATE virtual_points SET
        execution_count = execution_count + 1,
        avg_execution_time_ms = (avg_execution_time_ms * execution_count + ?) / (execution_count + 1),
        last_execution_time = CURRENT_TIMESTAMP,
        last_error = ?
      WHERE id = ?
    `;
  }

  // 실행 이력 추가
  static addExecutionHistory() {
    return `
      INSERT INTO virtual_point_execution_history (
        virtual_point_id, execution_duration_ms, result_value,
        input_snapshot, success, error_message
      ) VALUES (?, ?, ?, ?, ?, ?)
    `;
  }

  // 데이터 포인트 현재값 조회 (가상포인트 계산용)
  static getCurrentValues() {
    return `
      SELECT 
        dp.id,
        dp.name,
        cv.current_value,
        cv.value_type,
        cv.quality,
        cv.value_timestamp
      FROM data_points dp
      LEFT JOIN current_values cv ON dp.id = cv.point_id
      WHERE dp.id IN (${Array(50).fill('?').join(',')})
    `;
  }

  // 카테고리별 가상포인트 수 조회
  static getVirtualPointCountByCategory() {
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

  // 실행 성능 통계
  static getPerformanceStats() {
    return `
      SELECT 
        AVG(avg_execution_time_ms) as avg_execution_time,
        MAX(avg_execution_time_ms) as max_execution_time,
        COUNT(*) as total_points,
        SUM(execution_count) as total_executions,
        SUM(CASE WHEN last_error IS NOT NULL THEN 1 ELSE 0 END) as error_count
      FROM virtual_points
      WHERE tenant_id = ? AND is_enabled = 1
    `;
  }

  // 의존성 순환 검사용 쿼리
  static checkCircularDependency() {
    return `
      WITH RECURSIVE dependency_tree AS (
        SELECT virtual_point_id, depends_on_type, depends_on_id, 1 as level
        FROM virtual_point_dependencies
        WHERE virtual_point_id = ?
        
        UNION ALL
        
        SELECT vpd.virtual_point_id, vpd.depends_on_type, vpd.depends_on_id, dt.level + 1
        FROM virtual_point_dependencies vpd
        INNER JOIN dependency_tree dt ON vpd.virtual_point_id = dt.depends_on_id
        WHERE dt.depends_on_type = 'virtual_point' AND dt.level < 10
      )
      SELECT COUNT(*) as circular_count
      FROM dependency_tree
      WHERE depends_on_type = 'virtual_point' AND depends_on_id = ?
    `;
  }
}

module.exports = VirtualPointQueries;