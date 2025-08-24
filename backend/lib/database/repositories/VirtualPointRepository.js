// =============================================================================
// backend/lib/database/repositories/VirtualPointRepository.js
// 가상포인트 리포지토리 - DeviceRepository 패턴 준수
// =============================================================================

const DatabaseFactory = require('../DatabaseFactory');
const VirtualPointQueries = require('../queries/VirtualPointQueries');

class VirtualPointRepository {
  constructor() {
    this.dbFactory = new DatabaseFactory();
  }

  // ==========================================================================
  // 가상포인트 조회 메소드들
  // ==========================================================================

  /**
   * ID로 가상포인트 조회
   */
  async findById(id, tenantId = null) {
    try {
      console.log(`🧮 VirtualPointRepository.findById 호출: id=${id}, tenantId=${tenantId}`);
      
      let query = VirtualPointQueries.getVirtualPointById();
      const params = [id];

      if (tenantId) {
        query += ` AND tenant_id = ?`;
        params.push(tenantId);
      }

      console.log(`🔍 실행할 쿼리: ${query.substring(0, 100)}...`);
      console.log(`🔍 파라미터:`, params);

      const result = await this.dbFactory.executeQuery(query, params);
      const virtualPoints = Array.isArray(result) ? result : (result.rows || []);
      
      if (virtualPoints.length === 0) {
        console.log(`❌ 가상포인트 ID ${id} 찾을 수 없음`);
        return null;
      }
      
      console.log(`✅ 가상포인트 ID ${id} 조회 성공: ${virtualPoints[0].name}`);
      
      // 관련 데이터도 함께 조회
      const virtualPoint = this.parseVirtualPoint(virtualPoints[0]);
      virtualPoint.inputs = await this.getInputsByVirtualPoint(id);
      virtualPoint.currentValue = await this.getCurrentValue(id);
      virtualPoint.dependencies = await this.getDependencies(id);
      
      return virtualPoint;
      
    } catch (error) {
      console.error('❌ VirtualPointRepository.findById 오류:', error);
      throw new Error(`가상포인트 조회 실패: ${error.message}`);
    }
  }

  /**
   * 이름으로 가상포인트 조회
   */
  async findByName(name, tenantId = null) {
    try {
      console.log(`🧮 VirtualPointRepository.findByName 호출: name=${name}, tenantId=${tenantId}`);
      
      let query = VirtualPointQueries.getVirtualPointsList();
      query += ` AND vp.name = ?`;
      const params = [name];

      if (tenantId) {
        query += VirtualPointQueries.addTenantFilter();
        params.push(tenantId);
      }

      query += VirtualPointQueries.getGroupByAndOrder();

      console.log(`🔍 실행할 쿼리: ${query.substring(0, 100)}...`);
      console.log(`🔍 파라미터:`, params);

      const result = await this.dbFactory.executeQuery(query, params);
      const virtualPoints = Array.isArray(result) ? result : (result.rows || []);
      
      if (virtualPoints.length === 0) {
        console.log(`❌ 가상포인트 이름 '${name}' 찾을 수 없음`);
        return null;
      }
      
      console.log(`✅ 가상포인트 이름 '${name}' 조회 성공: ID ${virtualPoints[0].id}`);
      return this.parseVirtualPoint(virtualPoints[0]);
      
    } catch (error) {
      console.error('❌ VirtualPointRepository.findByName 오류:', error);
      throw new Error(`가상포인트 이름 조회 실패: ${error.message}`);
    }
  }

  /**
   * 가상포인트 목록 조회
   */
  async findAllVirtualPoints(filters = {}) {
    try {
      console.log('🔍 VirtualPointRepository.findAllVirtualPoints 호출:', filters);
      
      let query = VirtualPointQueries.getVirtualPointsList();
      const params = [];

      // 기본 tenant 필터 (필수)
      query += VirtualPointQueries.addTenantFilter();
      params.push(filters.tenantId || filters.tenant_id || 1);

      // 선택적 필터들
      if (filters.siteId || filters.site_id) {
        query += VirtualPointQueries.addSiteFilter();
        params.push(filters.siteId || filters.site_id);
      }

      if (filters.deviceId || filters.device_id) {
        query += VirtualPointQueries.addDeviceFilter();
        params.push(filters.deviceId || filters.device_id);
      }

      if (filters.scopeType || filters.scope_type) {
        query += VirtualPointQueries.addScopeTypeFilter();
        params.push(filters.scopeType || filters.scope_type);
      }

      if (filters.isEnabled !== undefined || filters.is_enabled !== undefined) {
        query += VirtualPointQueries.addEnabledFilter();
        params.push((filters.isEnabled !== undefined ? filters.isEnabled : filters.is_enabled) ? 1 : 0);
      }

      if (filters.category) {
        query += VirtualPointQueries.addCategoryFilter();
        params.push(filters.category);
      }

      if (filters.calculationTrigger || filters.calculation_trigger) {
        query += VirtualPointQueries.addTriggerFilter();
        params.push(filters.calculationTrigger || filters.calculation_trigger);
      }

      if (filters.search) {
        query += VirtualPointQueries.addSearchFilter();
        const searchTerm = `%${filters.search}%`;
        params.push(searchTerm, searchTerm);
      }

      // 정렬
      query += VirtualPointQueries.getGroupByAndOrder();

      // 페이징
      const limit = filters.limit || 25;
      const page = filters.page || 1;
      const offset = (page - 1) * limit;
      
      query += VirtualPointQueries.addLimit();
      params.push(limit);

      console.log('🔍 실행할 쿼리:', query.substring(0, 200) + '...');
      console.log('🔍 파라미터:', params.length + '개');

      const result = await this.dbFactory.executeQuery(query, params);
      console.log('🔍 Query result type:', typeof result);
      console.log('🔍 Query result keys:', Object.keys(result || {}));
      
      // 결과 처리 (다양한 DB 드라이버 대응)
      let virtualPoints = [];
      if (Array.isArray(result)) {
        virtualPoints = result;
      } else if (result && result.rows) {
        virtualPoints = result.rows;
      } else if (result && result.recordset) {
        virtualPoints = result.recordset;
      } else {
        console.warn('🔍 예상치 못한 쿼리 결과 구조:', result);
        virtualPoints = [];
      }

      console.log(`✅ ${virtualPoints.length}개 가상포인트 조회 완료`);

      // 데이터 파싱
      const parsedVirtualPoints = virtualPoints.map(vp => this.parseVirtualPoint(vp));

      // 페이징 정보 계산
      const totalCount = virtualPoints.length > 0 ? 
        (filters.page && filters.limit ? await this.getVirtualPointCount(filters) : virtualPoints.length) : 0;
      
      const pagination = {
        page: parseInt(page),
        limit: parseInt(limit),
        total_items: totalCount,
        has_next: page * limit < totalCount,
        has_prev: page > 1
      };

      return {
        items: parsedVirtualPoints,
        pagination: pagination
      };

    } catch (error) {
      console.error('❌ VirtualPointRepository.findAllVirtualPoints 오류:', error.message);
      console.error('❌ 스택:', error.stack);
      throw new Error(`가상포인트 목록 조회 실패: ${error.message}`);
    }
  }

  /**
   * 가상포인트 개수 조회
   */
  async getVirtualPointCount(filters = {}) {
    try {
      let query = `SELECT COUNT(*) as count FROM virtual_points WHERE 1=1`;
      const params = [];

      // 기본 tenant 필터
      query += ` AND tenant_id = ?`;
      params.push(filters.tenantId || filters.tenant_id || 1);

      // 추가 필터들
      if (filters.siteId || filters.site_id) {
        query += ` AND site_id = ?`;
        params.push(filters.siteId || filters.site_id);
      }

      if (filters.deviceId || filters.device_id) {
        query += ` AND device_id = ?`;
        params.push(filters.deviceId || filters.device_id);
      }

      if (filters.category) {
        query += ` AND category = ?`;
        params.push(filters.category);
      }

      if (filters.search) {
        query += ` AND (name LIKE ? OR description LIKE ?)`;
        const searchTerm = `%${filters.search}%`;
        params.push(searchTerm, searchTerm);
      }

      const result = await this.dbFactory.executeQuery(query, params);
      const countResult = Array.isArray(result) ? result[0] : (result.rows ? result.rows[0] : result);
      
      return countResult?.count || 0;
    } catch (error) {
      console.error('❌ 가상포인트 수 조회 실패:', error.message);
      return 0;
    }
  }

  // ==========================================================================
  // 가상포인트 CRUD 메소드들
  // ==========================================================================

  /**
   * 완전한 가상포인트 생성 (모든 관련 테이블)
   */
  async createVirtualPoint(virtualPointData, inputs = [], tenantId = null) {
    try {
      console.log(`🧮 VirtualPointRepository.createVirtualPoint: ${virtualPointData.name}`);
      
      // 트랜잭션 시작
      await this.dbFactory.executeQuery('BEGIN TRANSACTION');
      
      try {
        // 1. 메인 가상포인트 생성
        const virtualPointQuery = VirtualPointQueries.createVirtualPointSimple();
        const virtualPointParams = [
          tenantId || virtualPointData.tenant_id || 1,
          virtualPointData.name,
          virtualPointData.formula || 'return 0;',
          virtualPointData.description || null,
          virtualPointData.data_type || 'float',
          virtualPointData.unit || null,
          virtualPointData.calculation_trigger || 'timer',
          virtualPointData.is_enabled !== false ? 1 : 0,
          virtualPointData.category || 'calculation'
        ];
        
        const virtualPointResult = await this.dbFactory.executeQuery(virtualPointQuery, virtualPointParams);
        console.log('🔍 INSERT 결과 타입:', typeof virtualPointResult);
        console.log('🔍 INSERT 결과 키들:', Object.keys(virtualPointResult || {}));
        console.log('🔍 INSERT 결과 전체:', virtualPointResult);
        
        // SQLite에서 다양한 방식으로 ID 추출 시도
        let virtualPointId = null;
        
        if (virtualPointResult) {
          if (virtualPointResult.insertId) {
            virtualPointId = virtualPointResult.insertId;
            console.log('✅ insertId로 ID 획득:', virtualPointId);
          } else if (virtualPointResult.lastInsertRowid) {
            virtualPointId = virtualPointResult.lastInsertRowid;
            console.log('✅ lastInsertRowid로 ID 획득:', virtualPointId);
          } else if (virtualPointResult.changes && virtualPointResult.changes > 0) {
            console.log('🔍 changes 감지, last_insert_rowid() 쿼리 실행...');
            const idResult = await this.dbFactory.executeQuery('SELECT last_insert_rowid() as id');
            if (idResult && idResult.length > 0 && idResult[0].id) {
              virtualPointId = idResult[0].id;
              console.log('✅ last_insert_rowid() 쿼리로 ID 획득:', virtualPointId);
            }
          } else if (Array.isArray(virtualPointResult) && virtualPointResult.length > 0 && virtualPointResult[0].id) {
            virtualPointId = virtualPointResult[0].id;
            console.log('✅ 배열 결과에서 ID 획득:', virtualPointId);
          }
        }
        
        if (!virtualPointId) {
          console.error('❌ 모든 방법으로 ID 획득 실패, 이름으로 가상포인트 조회 시도...');
          const createdVirtualPoint = await this.findByName(virtualPointData.name, tenantId);
          if (createdVirtualPoint && createdVirtualPoint.id) {
            virtualPointId = createdVirtualPoint.id;
            console.log('✅ 이름으로 가상포인트 조회해서 ID 획득:', virtualPointId);
          } else {
            throw new Error('가상포인트 생성 실패: 모든 방법으로 ID를 얻을 수 없음');
          }
        }
        
        console.log(`✅ 가상포인트 생성 완료: ID ${virtualPointId}`);
        
        // 2. 초기값 생성
        try {
          await this.dbFactory.executeQuery(VirtualPointQueries.createInitialValue(), [virtualPointId]);
          console.log(`✅ 초기값 생성 완료: ID ${virtualPointId}`);
        } catch (valueError) {
          console.warn('⚠️ 초기값 생성 실패 (계속 진행):', valueError.message);
        }
        
        // 3. 입력 매핑 생성
        try {
          if (inputs && inputs.length > 0) {
            for (let i = 0; i < inputs.length; i++) {
              const input = inputs[i];
              await this.dbFactory.executeQuery(VirtualPointQueries.createInput(), [
                virtualPointId,
                input.variable_name,
                input.source_type || 'constant',
                input.source_id || null,
                input.constant_value || null,
                input.source_formula || null,
                input.is_required !== false ? 1 : 0,
                input.sort_order || i
              ]);
            }
            console.log(`✅ ${inputs.length}개 입력 매핑 생성 완료`);
          } else {
            // 기본 입력 매핑 생성
            await this.dbFactory.executeQuery(VirtualPointQueries.createDefaultInput(), [virtualPointId]);
            console.log(`✅ 기본 입력 매핑 생성 완료`);
          }
        } catch (inputError) {
          console.warn('⚠️ 입력 매핑 생성 실패 (계속 진행):', inputError.message);
        }
        
        // 4. 초기 실행 이력 생성
        try {
          await this.dbFactory.executeQuery(VirtualPointQueries.createInitialExecutionHistory(), [virtualPointId]);
          console.log(`✅ 초기 실행 이력 생성 완료: ID ${virtualPointId}`);
        } catch (historyError) {
          console.warn('⚠️ 초기 실행 이력 생성 실패 (계속 진행):', historyError.message);
        }
        
        // 트랜잭션 커밋
        await this.dbFactory.executeQuery('COMMIT');
        
        // 생성된 가상포인트 조회해서 반환
        const createdVirtualPoint = await this.findById(virtualPointId, tenantId);
        console.log(`🎉 완전한 가상포인트 생성 성공: ${virtualPointData.name} (ID: ${virtualPointId})`);
        
        return createdVirtualPoint;
        
      } catch (error) {
        // 트랜잭션 롤백
        await this.dbFactory.executeQuery('ROLLBACK');
        throw error;
      }
      
    } catch (error) {
      console.error('❌ VirtualPointRepository.createVirtualPoint 실패:', error.message);
      throw new Error(`완전한 가상포인트 생성 실패: ${error.message}`);
    }
  }

  /**
   * 가상포인트 업데이트
   */
  async updateVirtualPoint(id, virtualPointData, inputs = null, tenantId = null) {
    try {
      console.log(`🧮 VirtualPointRepository.updateVirtualPoint: ID ${id}`);
      
      // 존재 확인
      const existing = await this.findById(id, tenantId);
      if (!existing) {
        throw new Error(`가상포인트 ID ${id}가 존재하지 않습니다`);
      }

      // 트랜잭션 시작
      await this.dbFactory.executeQuery('BEGIN TRANSACTION');

      try {
        // 1. 메인 정보 업데이트
        await this.dbFactory.executeQuery(VirtualPointQueries.updateVirtualPointSimple(), [
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
        console.log('✅ 메인 정보 업데이트 완료');

        // 2. 입력 매핑 업데이트 (제공된 경우)
        if (inputs !== null && Array.isArray(inputs)) {
          // 기존 입력 삭제
          await this.dbFactory.executeQuery(VirtualPointQueries.deleteInputsByVirtualPointId(), [id]);
          
          // 새로운 입력 생성
          for (let i = 0; i < inputs.length; i++) {
            const input = inputs[i];
            await this.dbFactory.executeQuery(VirtualPointQueries.createInput(), [
              id,
              input.variable_name,
              input.source_type || 'constant',
              input.source_id || null,
              input.constant_value || null,
              input.source_formula || null,
              input.is_required !== false ? 1 : 0,
              input.sort_order || i
            ]);
          }
          console.log(`✅ ${inputs.length}개 입력 매핑 업데이트 완료`);
        }

        // 3. 현재값 무효화
        await this.dbFactory.executeQuery(VirtualPointQueries.invalidateCurrentValue(), [id]);
        console.log('✅ 현재값 무효화 완료');

        // 4. 업데이트 이력 추가
        await this.dbFactory.executeQuery(VirtualPointQueries.createUpdateHistory(), [id]);
        console.log('✅ 업데이트 이력 추가 완료');

        // 트랜잭션 커밋
        await this.dbFactory.executeQuery('COMMIT');
        
        console.log(`🎉 가상포인트 업데이트 완료: ID ${id}`);
        
        // 업데이트된 가상포인트 반환
        return await this.findById(id, tenantId);
        
      } catch (error) {
        // 트랜잭션 롤백
        await this.dbFactory.executeQuery('ROLLBACK');
        throw error;
      }
      
    } catch (error) {
      console.error('❌ VirtualPointRepository.updateVirtualPoint 실패:', error.message);
      throw new Error(`가상포인트 업데이트 실패: ${error.message}`);
    }
  }

  /**
   * 가상포인트 삭제 (CASCADE DELETE)
   */
  async deleteById(id, tenantId = null) {
    try {
      console.log(`🧮 VirtualPointRepository.deleteById: ID ${id}`);
      
      // 존재 확인
      const existing = await this.findById(id, tenantId);
      if (!existing) {
        console.log(`❌ 가상포인트 ID ${id}가 존재하지 않음`);
        return false;
      }

      console.log(`✅ 삭제 대상: ${existing.name} (ID: ${id})`);

      // 트랜잭션 시작
      await this.dbFactory.executeQuery('BEGIN TRANSACTION');

      try {
        // CASCADE DELETE 순서대로 실행
        const historyResult = await this.dbFactory.executeQuery(VirtualPointQueries.deleteExecutionHistory(), [id]);
        console.log(`✅ 1단계: 실행 이력 ${historyResult.changes || 0}개 삭제`);

        const depsResult = await this.dbFactory.executeQuery(VirtualPointQueries.deleteDependencies(), [id]);
        console.log(`✅ 2단계: 의존성 ${depsResult.changes || 0}개 삭제`);

        const valuesResult = await this.dbFactory.executeQuery(VirtualPointQueries.deleteValues(), [id]);
        console.log(`✅ 3단계: 현재값 ${valuesResult.changes || 0}개 삭제`);

        const inputsResult = await this.dbFactory.executeQuery(VirtualPointQueries.deleteInputs(), [id]);
        console.log(`✅ 4단계: 입력 매핑 ${inputsResult.changes || 0}개 삭제`);

        const alarmOccResult = await this.dbFactory.executeQuery(VirtualPointQueries.nullifyAlarmOccurrences(), [id]);
        console.log(`✅ 5단계: 알람 발생 ${alarmOccResult.changes || 0}개 정리`);

        const alarmRulesResult = await this.dbFactory.executeQuery(VirtualPointQueries.deleteAlarmRules(), [id]);
        console.log(`✅ 6단계: 알람 룰 ${alarmRulesResult.changes || 0}개 삭제`);

        const otherInputsResult = await this.dbFactory.executeQuery(
          VirtualPointQueries.deleteOtherVirtualPointInputReferences(), 
          ['virtual_point', id]
        );
        console.log(`✅ 7단계: 다른 VP 입력 참조 ${otherInputsResult.changes || 0}개 제거`);

        const otherDepsResult = await this.dbFactory.executeQuery(
          VirtualPointQueries.deleteOtherVirtualPointDependencyReferences(), 
          ['virtual_point', id]
        );
        console.log(`✅ 8단계: 다른 VP 의존성 참조 ${otherDepsResult.changes || 0}개 제거`);

        const mainResult = await this.dbFactory.executeQuery(VirtualPointQueries.deleteVirtualPoint(), [id]);
        console.log(`✅ 9단계: 가상포인트 본체 ${mainResult.changes || 0}개 삭제`);

        // 트랜잭션 커밋
        await this.dbFactory.executeQuery('COMMIT');

        const deletedCount = mainResult.changes || mainResult.affectedRows || 0;
        
        if (deletedCount > 0) {
          console.log(`🎉 가상포인트 ID ${id} 완전 삭제 성공!`);
          return true;
        } else {
          console.log(`❌ 가상포인트 본체 삭제 실패`);
          return false;
        }

      } catch (error) {
        // 트랜잭션 롤백
        await this.dbFactory.executeQuery('ROLLBACK');
        throw error;
      }
      
    } catch (error) {
      console.error('❌ VirtualPointRepository.deleteById 실패:', error.message);
      throw new Error(`가상포인트 삭제 실패: ${error.message}`);
    }
  }

  // ==========================================================================
  // 관련 데이터 조회 메소드들
  // ==========================================================================

  /**
   * 가상포인트의 입력 매핑 조회
   */
  async getInputsByVirtualPoint(virtualPointId) {
    try {
      const result = await this.dbFactory.executeQuery(VirtualPointQueries.getInputsByVirtualPointId(), [virtualPointId]);
      const inputs = Array.isArray(result) ? result : (result.rows || []);
      
      console.log(`✅ 가상포인트 ID ${virtualPointId}의 입력 매핑 ${inputs.length}개 조회 완료`);
      return inputs.map(input => this.parseVirtualPointInput(input));
      
    } catch (error) {
      console.error(`❌ getInputsByVirtualPoint 실패:`, error.message);
      return [];
    }
  }

  /**
   * 가상포인트의 현재값 조회
   */
  async getCurrentValue(virtualPointId) {
    try {
      const result = await this.dbFactory.executeQuery(VirtualPointQueries.getCurrentValue(), [virtualPointId]);
      const values = Array.isArray(result) ? result : (result.rows || []);
      
      return values.length > 0 ? values[0] : null;
      
    } catch (error) {
      console.error(`❌ getCurrentValue 실패:`, error.message);
      return null;
    }
  }

  /**
   * 가상포인트의 의존성 조회
   */
  async getDependencies(virtualPointId) {
    try {
      const result = await this.dbFactory.executeQuery(VirtualPointQueries.getDependencies(), [virtualPointId]);
      const dependencies = Array.isArray(result) ? result : (result.rows || []);
      
      return dependencies;
      
    } catch (error) {
      console.error(`❌ getDependencies 실패:`, error.message);
      return [];
    }
  }

  // ==========================================================================
  // 통계 및 분석 메소드들
  // ==========================================================================

  /**
   * 카테고리별 통계
   */
  async getStatsByCategory(tenantId) {
    try {
      const result = await this.dbFactory.executeQuery(VirtualPointQueries.getStatsByCategorySimple(), [tenantId]);
      const stats = Array.isArray(result) ? result : (result.rows || []);
      return stats;
    } catch (error) {
      console.error('getStatsByCategory error:', error);
      return [];
    }
  }

  /**
   * 성능 통계
   */
  async getPerformanceStats(tenantId) {
    try {
      const result = await this.dbFactory.executeQuery(VirtualPointQueries.getPerformanceStatsSimple(), [tenantId]);
      const stats = Array.isArray(result) ? result : (result.rows || []);
      return stats[0] || { total_points: 0, enabled_points: 0 };
    } catch (error) {
      console.error('getPerformanceStats error:', error);
      return { total_points: 0, enabled_points: 0 };
    }
  }

  /**
   * 고아 레코드 정리 (개발자 도구)
   */
  async cleanupOrphanedRecords() {
    try {
      console.log('🧹 고아 레코드 정리 시작...');

      await this.dbFactory.executeQuery('BEGIN TRANSACTION');

      try {
        const results = [];

        const orphanInputs = await this.dbFactory.executeQuery(VirtualPointQueries.cleanupOrphanedInputs());
        results.push({ table: 'virtual_point_inputs', cleaned: orphanInputs.changes || 0 });

        const orphanValues = await this.dbFactory.executeQuery(VirtualPointQueries.cleanupOrphanedValues());
        results.push({ table: 'virtual_point_values', cleaned: orphanValues.changes || 0 });

        const orphanDeps = await this.dbFactory.executeQuery(VirtualPointQueries.cleanupOrphanedDependencies());
        results.push({ table: 'virtual_point_dependencies', cleaned: orphanDeps.changes || 0 });

        const orphanHistory = await this.dbFactory.executeQuery(VirtualPointQueries.cleanupOrphanedExecutionHistory());
        results.push({ table: 'virtual_point_execution_history', cleaned: orphanHistory.changes || 0 });

        const orphanAlarms = await this.dbFactory.executeQuery(VirtualPointQueries.cleanupOrphanedAlarmOccurrences());
        results.push({ table: 'alarm_occurrences', cleaned: orphanAlarms.changes || 0 });

        await this.dbFactory.executeQuery('COMMIT');

        console.log('✅ 고아 레코드 정리 완료:', results);
        return results;

      } catch (error) {
        await this.dbFactory.executeQuery('ROLLBACK');
        throw error;
      }

    } catch (error) {
      console.error('❌ 고아 레코드 정리 실패:', error);
      throw error;
    }
  }

  // ==========================================================================
  // 헬퍼 메소드들
  // ==========================================================================

  /**
   * 가상포인트 데이터 파싱
   */
  parseVirtualPoint(vp) {
    if (!vp) return null;

    return {
      ...vp,
      is_enabled: !!vp.is_enabled,
      tags: vp.tags ? JSON.parse(vp.tags) : [],
      dependencies: vp.dependencies ? JSON.parse(vp.dependencies) : []
    };
  }

  /**
   * 가상포인트 입력 데이터 파싱
   */
  parseVirtualPointInput(input) {
    if (!input) return null;

    return {
      ...input,
      is_required: !!input.is_required,
      constant_value: input.constant_value ? parseFloat(input.constant_value) : null
    };
  }

  /**
   * 기본 통계 데이터 (에러 시 사용)
   */
  getDefaultStats(tenantId) {
    return {
      total_virtual_points: 0,
      enabled_virtual_points: 0,
      disabled_virtual_points: 0,
      category_distribution: [],
      trigger_distribution: [],
      last_updated: new Date().toISOString(),
      note: 'Default statistics due to query error'
    };
  }
}

module.exports = VirtualPointRepository;