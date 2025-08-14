// =============================================================================
// backend/lib/database/VirtualPointRepository.js
// 가상포인트 리포지토리 - AlarmOccurrenceRepository.js 패턴 준수
// =============================================================================

const DatabaseFactory = require('../connection/db');
const VirtualPointQueries = require('./VirtualPointQueries');

class VirtualPointRepository {
  constructor() {
    this.dbFactory = new DatabaseFactory();
  }

  // 가상포인트 목록 조회 (필터링 지원)
  async findAll(filters = {}) {
    try {
      let query = VirtualPointQueries.getVirtualPointsList();
      const params = [];

      // 필터 적용
      if (filters.tenant_id) {
        query += VirtualPointQueries.addTenantFilter();
        params.push(filters.tenant_id);
      }

      if (filters.site_id) {
        query += VirtualPointQueries.addSiteFilter();
        params.push(filters.site_id);
      }

      if (filters.device_id) {
        query += VirtualPointQueries.addDeviceFilter();
        params.push(filters.device_id);
      }

      if (filters.scope_type) {
        query += VirtualPointQueries.addScopeTypeFilter();
        params.push(filters.scope_type);
      }

      if (filters.is_enabled !== undefined) {
        query += VirtualPointQueries.addEnabledFilter();
        params.push(filters.is_enabled ? 1 : 0);
      }

      if (filters.category) {
        query += VirtualPointQueries.addCategoryFilter();
        params.push(filters.category);
      }

      if (filters.calculation_trigger) {
        query += VirtualPointQueries.addTriggerFilter();
        params.push(filters.calculation_trigger);
      }

      if (filters.search) {
        query += VirtualPointQueries.addSearchFilter();
        const searchTerm = `%${filters.search}%`;
        params.push(searchTerm, searchTerm);
      }

      // 그룹화 및 정렬
      query += VirtualPointQueries.getGroupByAndOrder();

      // 제한
      if (filters.limit) {
        query += VirtualPointQueries.addLimit();
        params.push(parseInt(filters.limit));
      }

      const results = await this.dbFactory.executeQuery(query, params);
      
      // JSON 필드 파싱
      return results.map(vp => this.parseVirtualPoint(vp));
    } catch (error) {
      console.error('Error finding virtual points:', error);
      throw error;
    }
  }

  // 가상포인트 상세 조회
  async findById(id) {
    try {
      const [virtualPoints, inputs] = await Promise.all([
        this.dbFactory.executeQuery(VirtualPointQueries.getVirtualPointById(), [id]),
        this.dbFactory.executeQuery(VirtualPointQueries.getVirtualPointInputs(), [id])
      ]);

      if (virtualPoints.length === 0) {
        return null;
      }

      const virtualPoint = this.parseVirtualPoint(virtualPoints[0]);
      virtualPoint.inputs = inputs.map(input => this.parseVirtualPointInput(input));

      return virtualPoint;
    } catch (error) {
      console.error('Error finding virtual point by ID:', error);
      throw error;
    }
  }

  // 가상포인트 생성
  async create(virtualPointData, inputs = []) {
    const connection = await this.dbFactory.getConnection();
    
    try {
      await connection.query('BEGIN');

      // 가상포인트 생성
      const vpParams = [
        virtualPointData.tenant_id,
        virtualPointData.scope_type || 'tenant',
        virtualPointData.site_id || null,
        virtualPointData.device_id || null,
        virtualPointData.name,
        virtualPointData.description || null,
        virtualPointData.formula,
        virtualPointData.data_type || 'float',
        virtualPointData.unit || null,
        virtualPointData.calculation_interval || 1000,
        virtualPointData.calculation_trigger || 'timer',
        virtualPointData.is_enabled !== false ? 1 : 0,
        virtualPointData.category || null,
        virtualPointData.tags ? JSON.stringify(virtualPointData.tags) : null,
        virtualPointData.execution_type || 'javascript',
        virtualPointData.dependencies ? JSON.stringify(virtualPointData.dependencies) : null,
        virtualPointData.cache_duration_ms || 0,
        virtualPointData.error_handling || 'return_null',
        virtualPointData.created_by || null
      ];

      const result = await connection.query(VirtualPointQueries.createVirtualPoint(), vpParams);
      const virtualPointId = result.insertId || result.lastInsertRowid;

      // 입력 매핑 생성
      if (inputs && inputs.length > 0) {
        await this.createInputs(connection, virtualPointId, inputs);
      }

      await connection.query('COMMIT');
      return await this.findById(virtualPointId);
    } catch (error) {
      await connection.query('ROLLBACK');
      console.error('Error creating virtual point:', error);
      throw error;
    } finally {
      connection.release();
    }
  }

  // 가상포인트 업데이트
  async update(id, virtualPointData, inputs = null) {
    const connection = await this.dbFactory.getConnection();
    
    try {
      await connection.query('BEGIN');

      // 가상포인트 업데이트
      const vpParams = [
        virtualPointData.name,
        virtualPointData.description || null,
        virtualPointData.formula,
        virtualPointData.data_type || 'float',
        virtualPointData.unit || null,
        virtualPointData.calculation_interval || 1000,
        virtualPointData.calculation_trigger || 'timer',
        virtualPointData.is_enabled !== false ? 1 : 0,
        virtualPointData.category || null,
        virtualPointData.tags ? JSON.stringify(virtualPointData.tags) : null,
        virtualPointData.execution_type || 'javascript',
        virtualPointData.dependencies ? JSON.stringify(virtualPointData.dependencies) : null,
        virtualPointData.cache_duration_ms || 0,
        virtualPointData.error_handling || 'return_null',
        id
      ];

      await connection.query(VirtualPointQueries.updateVirtualPoint(), vpParams);

      // 입력 매핑 업데이트 (제공된 경우)
      if (inputs !== null) {
        // 기존 입력 삭제
        await connection.query(VirtualPointQueries.deleteVirtualPointInputs(), [id]);
        
        // 새 입력 생성
        if (inputs.length > 0) {
          await this.createInputs(connection, id, inputs);
        }
      }

      await connection.query('COMMIT');
      return await this.findById(id);
    } catch (error) {
      await connection.query('ROLLBACK');
      console.error('Error updating virtual point:', error);
      throw error;
    } finally {
      connection.release();
    }
  }

  // 가상포인트 삭제
  async delete(id) {
    try {
      const result = await this.dbFactory.executeQuery(VirtualPointQueries.deleteVirtualPoint(), [id]);
      return result.affectedRows > 0 || result.changes > 0;
    } catch (error) {
      console.error('Error deleting virtual point:', error);
      throw error;
    }
  }

  // 가상포인트 의존성 조회
  async getDependencies(id) {
    try {
      const dependencies = await this.dbFactory.executeQuery(VirtualPointQueries.getVirtualPointDependencies(), [id]);
      return dependencies;
    } catch (error) {
      console.error('Error getting virtual point dependencies:', error);
      throw error;
    }
  }

  // 가상포인트 실행 이력 조회
  async getExecutionHistory(id, limit = 50) {
    try {
      const history = await this.dbFactory.executeQuery(VirtualPointQueries.getExecutionHistory(), [id, limit]);
      return history.map(h => ({
        ...h,
        result_value: h.result_value ? JSON.parse(h.result_value) : null,
        input_snapshot: h.input_snapshot ? JSON.parse(h.input_snapshot) : null
      }));
    } catch (error) {
      console.error('Error getting execution history:', error);
      throw error;
    }
  }

  // 가상포인트 값 업데이트
  async updateValue(id, value, stringValue, quality, calculationError, inputValues) {
    try {
      const params = [
        id,
        value,
        stringValue || null,
        quality || 'good',
        calculationError || null,
        inputValues ? JSON.stringify(inputValues) : null
      ];

      await this.dbFactory.executeQuery(VirtualPointQueries.upsertVirtualPointValue(), params);
    } catch (error) {
      console.error('Error updating virtual point value:', error);
      throw error;
    }
  }

  // 실행 통계 업데이트
  async updateExecutionStats(id, executionTimeMs, error = null) {
    try {
      await this.dbFactory.executeQuery(VirtualPointQueries.updateExecutionStats(), [executionTimeMs, error, id]);
    } catch (error) {
      console.error('Error updating execution stats:', error);
      throw error;
    }
  }

  // 실행 이력 추가
  async addExecutionHistory(id, executionTimeMs, resultValue, inputSnapshot, success, errorMessage) {
    try {
      const params = [
        id,
        executionTimeMs,
        resultValue ? JSON.stringify(resultValue) : null,
        inputSnapshot ? JSON.stringify(inputSnapshot) : null,
        success ? 1 : 0,
        errorMessage || null
      ];

      await this.dbFactory.executeQuery(VirtualPointQueries.addExecutionHistory(), params);
    } catch (error) {
      console.error('Error adding execution history:', error);
      throw error;
    }
  }

  // 카테고리별 통계
  async getStatsByCategory(tenantId) {
    try {
      const stats = await this.dbFactory.executeQuery(VirtualPointQueries.getVirtualPointCountByCategory(), [tenantId]);
      return stats;
    } catch (error) {
      console.error('Error getting stats by category:', error);
      throw error;
    }
  }

  // 성능 통계
  async getPerformanceStats(tenantId) {
    try {
      const stats = await this.dbFactory.executeQuery(VirtualPointQueries.getPerformanceStats(), [tenantId]);
      return stats[0] || {};
    } catch (error) {
      console.error('Error getting performance stats:', error);
      throw error;
    }
  }

  // 순환 의존성 검사
  async checkCircularDependency(virtualPointId, dependsOnId) {
    try {
      const result = await this.dbFactory.executeQuery(VirtualPointQueries.checkCircularDependency(), [virtualPointId, dependsOnId]);
      return result[0].circular_count > 0;
    } catch (error) {
      console.error('Error checking circular dependency:', error);
      throw error;
    }
  }

  // =============================================================================
  // 헬퍼 메소드들
  // =============================================================================

  // 입력 매핑 생성
  async createInputs(connection, virtualPointId, inputs) {
    for (const input of inputs) {
      const inputParams = [
        virtualPointId,
        input.variable_name,
        input.source_type,
        input.source_id || null,
        input.constant_value || null,
        input.source_formula || null,
        input.data_processing || 'current',
        input.time_window_seconds || null
      ];

      await connection.query(VirtualPointQueries.createVirtualPointInput(), inputParams);
    }
  }

  // 가상포인트 데이터 파싱
  parseVirtualPoint(vp) {
    return {
      ...vp,
      is_enabled: !!vp.is_enabled,
      tags: vp.tags ? JSON.parse(vp.tags) : [],
      dependencies: vp.dependencies ? JSON.parse(vp.dependencies) : [],
      input_values: vp.input_values ? JSON.parse(vp.input_values) : null
    };
  }

  // 가상포인트 입력 데이터 파싱
  parseVirtualPointInput(input) {
    return {
      ...input,
      constant_value: input.constant_value ? parseFloat(input.constant_value) : null
    };
  }
}

module.exports = VirtualPointRepository;