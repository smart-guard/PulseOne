// =============================================================================
// backend/lib/database/queries/VirtualPointQueries.js
// 가상포인트 SQL 쿼리 완전 분리본 - 모든 쿼리 포함
// =============================================================================

class VirtualPointQueries {
  // ==========================================================================
  // 🔍 조회 쿼리들
  // ==========================================================================

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

  // 상세 조회
  static getVirtualPointById() {
    return `
      SELECT * FROM virtual_points WHERE id = ?
    `;
  }

  // 입력 매핑 조회
  static getInputsByVirtualPointId() {
    return `
      SELECT id, virtual_point_id, variable_name, source_type, source_id, 
             constant_value, source_formula, is_required, sort_order, created_at
      FROM virtual_point_inputs 
      WHERE virtual_point_id = ? 
      ORDER BY sort_order, id
    `;
  }

  // 현재값 조회
  static getCurrentValue() {
    return `
      SELECT virtual_point_id, value, string_value, quality, 
             last_calculated, calculation_duration_ms, calculation_error
      FROM virtual_point_values 
      WHERE virtual_point_id = ?
    `;
  }

  // 의존성 조회
  static getDependencies() {
    return `
      SELECT id, depends_on_type, depends_on_id, dependency_level, 
             is_critical, fallback_value, is_active
      FROM virtual_point_dependencies 
      WHERE virtual_point_id = ? AND is_active = 1
      ORDER BY dependency_level, id
    `;
  }

  // ==========================================================================
  // 📝 필터 조건들
  // ==========================================================================

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

  // ==========================================================================
  // 🛠️ 생성 쿼리들
  // ==========================================================================

  // 가상포인트 메인 생성
  static createVirtualPointSimple() {
    return `
      INSERT INTO virtual_points (
        tenant_id, name, formula, description, data_type, unit,
        calculation_trigger, is_enabled, category, created_at, updated_at
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'), datetime('now'))
    `;
  }

  // 초기값 생성
  static createInitialValue() {
    return `
      INSERT INTO virtual_point_values 
      (virtual_point_id, value, quality, last_calculated, calculation_duration_ms, is_stale) 
      VALUES (?, NULL, 'initialization', datetime('now'), 0, 1)
    `;
  }

  // 입력 매핑 생성
  static createInput() {
    return `
      INSERT INTO virtual_point_inputs 
      (virtual_point_id, variable_name, source_type, source_id, constant_value, 
       source_formula, is_required, sort_order) 
      VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    `;
  }

  // 기본 입력 매핑 생성
  static createDefaultInput() {
    return `
      INSERT INTO virtual_point_inputs 
      (virtual_point_id, variable_name, source_type, constant_value, is_required, sort_order) 
      VALUES (?, 'defaultInput', 'constant', 0, 1, 0)
    `;
  }

  // 의존성 생성
  static createDependency() {
    return `
      INSERT INTO virtual_point_dependencies 
      (virtual_point_id, depends_on_type, depends_on_id, dependency_level, is_critical, is_active) 
      VALUES (?, ?, ?, ?, ?, 1)
    `;
  }

  // 초기 실행 이력 생성
  static createInitialExecutionHistory() {
    return `
      INSERT INTO virtual_point_execution_history 
      (virtual_point_id, execution_time, execution_duration_ms, result_type, 
       result_value, trigger_source, success) 
      VALUES (?, datetime('now'), 0, 'success', '{"action": "created", "status": "initialized"}', 'system', 1)
    `;
  }

  // ==========================================================================
  // 🔄 업데이트 쿼리들
  // ==========================================================================

  // 가상포인트 메인 업데이트
  static updateVirtualPointSimple() {
    return `
      UPDATE virtual_points SET
        name = ?, formula = ?, description = ?, data_type = ?,
        unit = ?, calculation_trigger = ?, is_enabled = ?,
        category = ?, updated_at = datetime('now')
      WHERE id = ?
    `;
  }

  // 입력 매핑 삭제 (업데이트를 위한)
  static deleteInputsByVirtualPointId() {
    return `
      DELETE FROM virtual_point_inputs WHERE virtual_point_id = ?
    `;
  }

  // 현재값 무효화
  static invalidateCurrentValue() {
    return `
      UPDATE virtual_point_values 
      SET quality = 'pending_update', is_stale = 1, last_calculated = datetime('now')
      WHERE virtual_point_id = ?
    `;
  }

  // 업데이트 이력 추가
  static createUpdateHistory() {
    return `
      INSERT INTO virtual_point_execution_history 
      (virtual_point_id, execution_time, execution_duration_ms, result_type, 
       result_value, trigger_source, success) 
      VALUES (?, datetime('now'), 0, 'success', '{"action": "updated", "status": "completed"}', 'manual', 1)
    `;
  }
/**
   * 가상포인트 활성화/비활성화만 업데이트 (토글용)
   */
  static updateEnabledOnly() {
    return `
      UPDATE virtual_points 
      SET is_enabled = ?, updated_at = datetime('now')
      WHERE id = ?
    `;
  }

  /**
   * 가상포인트 설정만 업데이트 (이름/수식 제외)
   */
  static updateSettingsOnly() {
    return `
      UPDATE virtual_points 
      SET calculation_interval = ?, calculation_trigger = ?, 
          priority = ?, description = ?, unit = ?, data_type = ?, 
          category = ?, updated_at = datetime('now')
      WHERE id = ?
    `;
  }

  /**
   * 가상포인트 현재값 업데이트
   */
  static updateCurrentValue() {
    return `
      UPDATE virtual_point_values 
      SET value = ?, quality = ?, last_calculated = datetime('now'),
          calculation_duration_ms = ?, is_stale = 0
      WHERE virtual_point_id = ?
    `;
  }

  /**
   * 가상포인트 실행 통계 업데이트
   */
  static updateExecutionStats() {
    return `
      UPDATE virtual_points 
      SET execution_count = execution_count + 1,
          avg_execution_time_ms = (
            CASE WHEN execution_count > 0 
            THEN (avg_execution_time_ms * execution_count + ?) / (execution_count + 1)
            ELSE ?
            END
          ),
          last_execution_time = datetime('now'),
          updated_at = datetime('now')
      WHERE id = ?
    `;
  }

  /**
   * 가상포인트 에러 정보 업데이트
   */
  static updateLastError() {
    return `
      UPDATE virtual_points 
      SET last_error = ?, updated_at = datetime('now')
      WHERE id = ?
    `;
  }

  /**
   * 가상포인트 마지막 계산 시간만 업데이트
   */
  static updateLastCalculated() {
    return `
      UPDATE virtual_points 
      SET last_execution_time = datetime('now'), updated_at = datetime('now')
      WHERE id = ?
    `;
  }
  // ==========================================================================
  // 🗑️ 삭제 쿼리들 (CASCADE DELETE 순서) - 컬럼명 수정
  // ==========================================================================

  // 1. 실행 이력 삭제 - 올바른 컬럼명 사용
  static deleteExecutionHistory() {
    return `
      DELETE FROM virtual_point_execution_history WHERE virtual_point_id = ?
    `;
  }

  // 2. 의존성 삭제 - 올바른 컬럼명 사용  
  static deleteDependencies() {
    return `
      DELETE FROM virtual_point_dependencies WHERE virtual_point_id = ?
    `;
  }

  // 3. 현재값 삭제 - 올바른 컬럼명 사용
  static deleteValues() {
    return `
      DELETE FROM virtual_point_values WHERE virtual_point_id = ?
    `;
  }

  // 4. 입력 매핑 삭제 - 올바른 컬럼명 사용
  static deleteInputs() {
    return `
      DELETE FROM virtual_point_inputs WHERE virtual_point_id = ?
    `;
  }

  // 5. 알람 발생에서 참조 제거 - 올바른 컬럼명 사용
  static nullifyAlarmOccurrences() {
    return `
      UPDATE alarm_occurrences SET point_id = NULL 
      WHERE point_id = ? AND EXISTS (
        SELECT 1 FROM alarm_rules 
        WHERE alarm_rules.id = alarm_occurrences.rule_id 
        AND target_type = 'virtual_point'
      )
    `;
  }

  // 6. 알람 룰 삭제 - 올바른 컬럼명 사용
  static deleteAlarmRules() {
    return `
      DELETE FROM alarm_rules 
      WHERE target_type = 'virtual_point' AND target_id = ?
    `;
  }

  // 7. 다른 VP에서 이 VP를 참조하는 입력 제거
  static deleteOtherVirtualPointInputReferences() {
    return `
      DELETE FROM virtual_point_inputs WHERE source_type = ? AND source_id = ?
    `;
  }

  // 8. 다른 VP에서 이 VP를 참조하는 의존성 제거
  static deleteOtherVirtualPointDependencyReferences() {
    return `
      DELETE FROM virtual_point_dependencies WHERE depends_on_type = ? AND depends_on_id = ?
    `;
  }

  // 9. 가상포인트 메인 삭제
  static deleteVirtualPoint() {
    return `
      DELETE FROM virtual_points WHERE id = ?
    `;
  }

  // ==========================================================================
  // 📊 통계 쿼리들
  // ==========================================================================

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

  // ==========================================================================
  // 🧹 정리 쿼리들 (개발자 도구)
  // ==========================================================================

  // 고아 입력 매핑 삭제
  static cleanupOrphanedInputs() {
    return `
      DELETE FROM virtual_point_inputs 
      WHERE virtual_point_id NOT IN (SELECT id FROM virtual_points)
    `;
  }

  // 고아 값 레코드 삭제
  static cleanupOrphanedValues() {
    return `
      DELETE FROM virtual_point_values 
      WHERE virtual_point_id NOT IN (SELECT id FROM virtual_points)
    `;
  }

  // 고아 의존성 레코드 삭제
  static cleanupOrphanedDependencies() {
    return `
      DELETE FROM virtual_point_dependencies 
      WHERE virtual_point_id NOT IN (SELECT id FROM virtual_points)
    `;
  }

  // 고아 실행 이력 삭제
  static cleanupOrphanedExecutionHistory() {
    return `
      DELETE FROM virtual_point_execution_history 
      WHERE virtual_point_id NOT IN (SELECT id FROM virtual_points)
    `;
  }

  // 고아 알람 발생 레코드 정리
  static cleanupOrphanedAlarmOccurrences() {
    return `
      UPDATE alarm_occurrences 
      SET virtual_point_id = NULL 
      WHERE virtual_point_id NOT IN (SELECT id FROM virtual_points)
    `;
  }

  // ==========================================================================
  // 🔍 검증 쿼리들
  // ==========================================================================

  // 존재 확인
  static checkExists() {
    return `
      SELECT id, name FROM virtual_points WHERE id = ?
    `;
  }

  // 이름 중복 확인
  static checkNameDuplicate() {
    return `
      SELECT id FROM virtual_points WHERE tenant_id = ? AND name = ? AND id != ?
    `;
  }

  // 고아 레코드 확인 (데이터 일관성 검사)
  static checkOrphanedInputs() {
    return `
      SELECT COUNT(*) as count 
      FROM virtual_point_inputs vpi 
      LEFT JOIN virtual_points vp ON vpi.virtual_point_id = vp.id 
      WHERE vp.id IS NULL
    `;
  }

  static checkOrphanedValues() {
    return `
      SELECT COUNT(*) as count 
      FROM virtual_point_values vpv 
      LEFT JOIN virtual_points vp ON vpv.virtual_point_id = vp.id 
      WHERE vp.id IS NULL
    `;
  }

  static checkOrphanedDependencies() {
    return `
      SELECT COUNT(*) as count 
      FROM virtual_point_dependencies vpd 
      LEFT JOIN virtual_points vp ON vpd.virtual_point_id = vp.id 
      WHERE vp.id IS NULL
    `;
  }

  // ==========================================================================
  // 🛠️ 트랜잭션 관련
  // ==========================================================================

  static beginTransaction() {
    return `BEGIN TRANSACTION`;
  }

  static commitTransaction() {
    return `COMMIT`;
  }

  static rollbackTransaction() {
    return `ROLLBACK`;
  }
}

module.exports = VirtualPointQueries;