// =============================================================================
// backend/lib/database/repositories/VirtualPointRepository.js
// 가상포인트 리포지토리 - 쿼리 완전 분리
// =============================================================================

const BaseRepository = require('./BaseRepository');
const VirtualPointQueries = require('../queries/VirtualPointQueries');

class VirtualPointRepository extends BaseRepository {
  constructor() {
    super('virtual_points');
  }

  // 가상포인트 목록 조회 (필터링 지원)
  async findAll(filters = {}) {
    try {
      let query = VirtualPointQueries.getVirtualPointsList();
      const params = [];

      // 필터 적용 (쿼리 분리)
      if (filters.tenant_id) {
        query += VirtualPointQueries.addTenantFilter();
        params.push(filters.tenant_id);
      }

      if (filters.is_enabled !== undefined) {
        query += VirtualPointQueries.addEnabledFilter();
        params.push(filters.is_enabled ? 1 : 0);
      }

      if (filters.search) {
        query += VirtualPointQueries.addSearchFilter();
        const searchTerm = `%${filters.search}%`;
        params.push(searchTerm, searchTerm);
      }

      // 그룹화 및 정렬 (쿼리 분리)
      query += VirtualPointQueries.getGroupByAndOrder();

      if (filters.limit) {
        query += VirtualPointQueries.addLimit();
        params.push(parseInt(filters.limit));
      }

      const results = await this.executeQuery(query, params);
      return results.map(vp => this.parseVirtualPoint(vp));
    } catch (error) {
      console.error('VirtualPointRepository.findAll error:', error);
      return [];
    }
  }

  // 가상포인트 상세 조회 (쿼리 분리)
  async findById(id) {
    try {
      const results = await this.executeQuery(VirtualPointQueries.getVirtualPointById(), [id]);
      
      if (results.length === 0) {
        return null;
      }

      return this.parseVirtualPoint(results[0]);
    } catch (error) {
      console.error('VirtualPointRepository.findById error:', error);
      return null;
    }
  }

  // 가상포인트 생성 (쿼리 분리)
  async create(virtualPointData, inputs = []) {
    try {
      // 필수 필드 검증
      if (!virtualPointData || !virtualPointData.name) {
        throw new Error('Missing required field: name');
      }

      console.log('Creating virtual point:', virtualPointData.name);

      // 쿼리 분리 - VirtualPointQueries 사용
      const result = await this.executeQuery(VirtualPointQueries.createVirtualPointSimple(), [
        virtualPointData.tenant_id || 1,
        virtualPointData.name,
        virtualPointData.formula || '',
        virtualPointData.description || null,
        virtualPointData.data_type || 'float',
        virtualPointData.unit || null,
        virtualPointData.calculation_trigger || 'timer',
        virtualPointData.is_enabled !== false ? 1 : 0,
        virtualPointData.category || 'calculation'
      ]);

      const virtualPointId = result.lastID || result.insertId;
      console.log('Virtual point created with ID:', virtualPointId);

      return await this.findById(virtualPointId);
    } catch (error) {
      console.error('VirtualPointRepository.create error:', error);
      throw error;
    }
  }

  // 가상포인트 업데이트 (쿼리 분리)
  async update(id, virtualPointData, inputs = null) {
    try {
      // 쿼리 분리 - VirtualPointQueries 사용
      const result = await this.executeQuery(VirtualPointQueries.updateVirtualPointSimple(), [
        virtualPointData.name,
        virtualPointData.formula,
        virtualPointData.description,
        virtualPointData.data_type,
        virtualPointData.unit,
        virtualPointData.calculation_trigger,
        virtualPointData.is_enabled ? 1 : 0,
        virtualPointData.category,
        id
      ]);

      return await this.findById(id);
    } catch (error) {
      console.error('VirtualPointRepository.update error:', error);
      throw error;
    }
  }

  // 가상포인트 삭제 (쿼리 분리)
  async delete(id) {
    try {
      // 쿼리 분리 - VirtualPointQueries 사용
      const result = await this.executeQuery(VirtualPointQueries.deleteVirtualPoint(), [id]);
      return result.changes > 0 || result.affectedRows > 0;
    } catch (error) {
      console.error('VirtualPointRepository.delete error:', error);
      return false;
    }
  }

  // 통계 메서드들 (쿼리 분리)
  async getStatsByCategory(tenantId) {
    try {
      // 쿼리 분리 - VirtualPointQueries 사용
      return await this.executeQuery(VirtualPointQueries.getStatsByCategorySimple(), [tenantId]);
    } catch (error) {
      console.error('VirtualPointRepository.getStatsByCategory error:', error);
      return [];
    }
  }

  async getPerformanceStats(tenantId) {
    try {
      // 쿼리 분리 - VirtualPointQueries 사용
      const stats = await this.executeQuery(VirtualPointQueries.getPerformanceStatsSimple(), [tenantId]);
      return stats[0] || { total_points: 0, enabled_points: 0 };
    } catch (error) {
      console.error('VirtualPointRepository.getPerformanceStats error:', error);
      return { total_points: 0, enabled_points: 0 };
    }
  }

  // 순환 의존성 검사 (단순화)
  async checkCircularDependency(virtualPointId, dependsOnId) {
    try {
      return virtualPointId === dependsOnId;
    } catch (error) {
      console.error('VirtualPointRepository.checkCircularDependency error:', error);
      return false;
    }
  }

  // =============================================================================
  // 헬퍼 메소드들 (쿼리 없음 - 로직만)
  // =============================================================================

  parseVirtualPoint(vp) {
    return {
      ...vp,
      is_enabled: !!vp.is_enabled,
      tags: vp.tags ? JSON.parse(vp.tags) : [],
      dependencies: vp.dependencies ? JSON.parse(vp.dependencies) : [],
      input_values: vp.input_values ? JSON.parse(vp.input_values) : null
    };
  }

  parseVirtualPointInput(input) {
    return {
      ...input,
      constant_value: input.constant_value ? parseFloat(input.constant_value) : null
    };
  }
}   
module.exports = VirtualPointRepository;
