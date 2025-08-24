// =============================================================================
// backend/routes/virtual-points.js
// 가상포인트 API 라우트 완성본 - 실제 DB 스키마 반영 + 토글 API 포함
// =============================================================================

const express = require('express');
const router = express.Router();

// 새로운 VirtualPointRepository 임포트
const VirtualPointRepository = require('../lib/database/repositories/VirtualPointRepository');

// Repository 인스턴스 생성
const virtualPointRepo = new VirtualPointRepository();

// 공통 응답 함수 (DeviceRepository 패턴과 동일)
const createResponse = (success, data, message, error_code = null) => ({
  success,
  data,
  message,
  error_code,
  timestamp: new Date().toISOString()
});

// =============================================================================
// 우선순위 높은 라우트들 먼저 등록 (/:id 보다 앞에!)
// =============================================================================

/**
 * GET /api/virtual-points/stats/category
 * 카테고리별 가상포인트 통계
 */
router.get('/stats/category', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id || 1;
    
    console.log(`카테고리별 통계 조회: 테넌트 ${tenantId}`);

    const stats = await virtualPointRepo.getStatsByCategory(tenantId);

    console.log(`카테고리별 통계 조회 완료: ${stats.length}개 카테고리`);

    res.json(createResponse(
      true, 
      stats, 
      'Category statistics retrieved successfully'
    ));

  } catch (error) {
    console.error('카테고리별 통계 조회 실패:', error);
    res.status(500).json(createResponse(
      false, 
      null, 
      'Failed to fetch category stats', 
      'STATS_ERROR'
    ));
  }
});

/**
 * GET /api/virtual-points/stats/performance
 * 가상포인트 성능 통계
 */
router.get('/stats/performance', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id || 1;
    
    console.log(`성능 통계 조회: 테넌트 ${tenantId}`);

    const stats = await virtualPointRepo.getPerformanceStats(tenantId);

    console.log(`성능 통계 조회 완료:`, stats);

    res.json(createResponse(
      true, 
      stats, 
      'Performance statistics retrieved successfully'
    ));

  } catch (error) {
    console.error('성능 통계 조회 실패:', error);
    res.status(500).json(createResponse(
      false, 
      null, 
      'Failed to fetch performance stats', 
      'STATS_ERROR'
    ));
  }
});

/**
 * GET /api/virtual-points/stats/summary
 * 가상포인트 전체 요약 통계 (DeviceRepository 패턴)
 */
router.get('/stats/summary', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id || 1;
    
    console.log(`전체 요약 통계 조회: 테넌트 ${tenantId}`);

    // 여러 통계를 한 번에 조회
    const [categoryStats, performanceStats] = await Promise.all([
      virtualPointRepo.getStatsByCategory(tenantId),
      virtualPointRepo.getPerformanceStats(tenantId)
    ]);

    const summary = {
      total_virtual_points: performanceStats.total_points || 0,
      enabled_virtual_points: performanceStats.enabled_points || 0,
      disabled_virtual_points: (performanceStats.total_points || 0) - (performanceStats.enabled_points || 0),
      category_distribution: categoryStats,
      last_updated: new Date().toISOString()
    };

    console.log(`전체 요약 통계 조회 완료:`, summary);

    res.json(createResponse(
      true, 
      summary, 
      'Summary statistics retrieved successfully'
    ));

  } catch (error) {
    console.error('전체 요약 통계 조회 실패:', error);
    res.status(500).json(createResponse(
      false, 
      null, 
      'Failed to fetch summary stats', 
      'STATS_ERROR'
    ));
  }
});

/**
 * POST /api/virtual-points/admin/cleanup-orphaned
 * 고아 레코드 정리 (개발자 도구)
 */
router.post('/admin/cleanup-orphaned', async (req, res) => {
  try {
    console.log('고아 레코드 정리 요청');

    const cleanupResults = await virtualPointRepo.cleanupOrphanedRecords();

    const totalCleaned = cleanupResults.reduce((sum, result) => sum + result.cleaned, 0);

    console.log(`고아 레코드 정리 완료: 총 ${totalCleaned}개 정리`);

    res.json(createResponse(
      true, 
      {
        cleanup_results: cleanupResults,
        total_cleaned: totalCleaned
      }, 
      `Cleaned up ${totalCleaned} orphaned records`
    ));

  } catch (error) {
    console.error('고아 레코드 정리 실패:', error);
    res.status(500).json(createResponse(
      false, 
      null, 
      'Failed to cleanup orphaned records', 
      'CLEANUP_ERROR'
    ));
  }
});

// =============================================================================
// PATCH 라우트들 (/:id 앞에 등록!) - 토글 API 포함
// =============================================================================

/**
 * PATCH /api/virtual-points/:id/toggle
 * 가상포인트 활성화/비활성화 토글 (is_enabled만 업데이트!)
 */
router.patch('/:id/toggle', async (req, res) => {
  try {
    const { id } = req.params;
    const { is_enabled } = req.body;
    const tenantId = req.body.tenant_id || req.query.tenant_id || 1;

    console.log(`가상포인트 ${id} 활성화 토글: ${is_enabled}, 테넌트: ${tenantId}`);

    // 존재 확인
    const existing = await virtualPointRepo.findById(parseInt(id), tenantId);
    if (!existing) {
      console.log(`가상포인트 ID ${id} 찾을 수 없음`);
      return res.status(404).json(createResponse(false, null, 'Virtual point not found', 'NOT_FOUND'));
    }

    console.log(`토글 대상: ${existing.name} (현재: ${existing.is_enabled} -> 변경: ${is_enabled})`);

    // 새로운 토글 전용 메소드 사용
    const updatedVirtualPoint = await virtualPointRepo.updateEnabledStatus(parseInt(id), is_enabled, tenantId);

    console.log(`가상포인트 ${id} 활성화 상태 변경 완료`);
    
    res.json(createResponse(true, {
      id: parseInt(id),
      name: updatedVirtualPoint.name,
      is_enabled: updatedVirtualPoint.is_enabled,
      updated_at: updatedVirtualPoint.updated_at
    }, `Virtual point ${is_enabled ? 'enabled' : 'disabled'} successfully`));

  } catch (error) {
    console.error(`가상포인트 ${req.params.id} 토글 실패:`, error.message);
    res.status(500).json(createResponse(false, null, `Failed to toggle virtual point: ${error.message}`, 'VIRTUAL_POINT_TOGGLE_ERROR'));
  }
});

/**
 * PATCH /api/virtual-points/:id/settings
 * 가상포인트 설정만 업데이트 (name 건드리지 않음)
 */
router.patch('/:id/settings', async (req, res) => {
  try {
    const { id } = req.params;
    const settings = req.body; // is_enabled, calculation_interval 등만
    const tenantId = settings.tenant_id || req.query.tenant_id || 1;

    console.log(`가상포인트 ${id} 설정 업데이트:`, settings);

    // 존재 확인
    const existing = await virtualPointRepo.findById(parseInt(id), tenantId);
    if (!existing) {
      console.log(`가상포인트 ID ${id} 찾을 수 없음`);
      return res.status(404).json(createResponse(false, null, 'Virtual point not found', 'NOT_FOUND'));
    }

    // 허용된 설정 필드만 업데이트 (name, formula는 제외)
    const allowedFields = [
      'is_enabled', 'calculation_interval', 'calculation_trigger', 
      'priority', 'description', 'unit', 'data_type', 'category'
    ];

    const filteredSettings = {};
    Object.keys(settings).forEach(key => {
      if (allowedFields.includes(key)) {
        filteredSettings[key] = settings[key];
      }
    });

    if (Object.keys(filteredSettings).length === 0) {
      return res.status(400).json(createResponse(false, null, 'No valid settings provided', 'VALIDATION_ERROR'));
    }

    // 기존 데이터와 병합
    const updatedData = {
      ...existing,
      ...filteredSettings,
      updated_at: new Date().toISOString()
    };

    // 설정만 업데이트 (inputs null로 전달)
    const updatedVirtualPoint = await virtualPointRepo.updateVirtualPoint(
      parseInt(id), 
      updatedData, 
      null,  // inputs를 null로 전달해서 기존 입력 유지
      tenantId
    );

    console.log(`가상포인트 ${id} 설정 업데이트 완료`);
    
    res.json(createResponse(true, {
      id: parseInt(id),
      updated_settings: filteredSettings,
      updated_at: updatedVirtualPoint.updated_at
    }, 'Virtual point settings updated successfully'));

  } catch (error) {
    console.error(`가상포인트 ${req.params.id} 설정 업데이트 실패:`, error.message);
    res.status(500).json(createResponse(false, null, `Failed to update settings: ${error.message}`, 'VIRTUAL_POINT_SETTINGS_ERROR'));
  }
});

/**
 * POST /api/virtual-points/:id/execute
 * 가상포인트 실행 (프론트엔드 요청 대응 - 임시 구현)
 */
router.post('/:id/execute', async (req, res) => {
  try {
    const { id } = req.params;
    
    console.log(`가상포인트 실행 요청: ID ${id} (현재 미구현)`);

    // 현재는 404 응답 (프론트엔드에서 예상하는 동작)
    res.status(404).json(createResponse(
      false, 
      null, 
      'Virtual point execution not implemented yet', 
      'NOT_IMPLEMENTED'
    ));

  } catch (error) {
    console.error('가상포인트 실행 실패:', error);
    res.status(500).json(createResponse(
      false, 
      null, 
      'Failed to execute virtual point', 
      'EXECUTION_ERROR'
    ));
  }
});

/**
 * POST /api/virtual-points/test
 * 스크립트 테스트 (프론트엔드 요청 대응 - 임시 구현)
 */
router.post('/test', async (req, res) => {
  try {
    console.log('스크립트 테스트 요청 (현재 미구현)');

    // 현재는 404 응답 (프론트엔드에서 예상하는 동작)
    res.status(404).json(createResponse(
      false, 
      null, 
      'Script testing not implemented yet', 
      'NOT_IMPLEMENTED'
    ));

  } catch (error) {
    console.error('스크립트 테스트 실패:', error);
    res.status(500).json(createResponse(
      false, 
      null, 
      'Failed to test script', 
      'TEST_ERROR'
    ));
  }
});

// =============================================================================
// 기본 CRUD API - 특수 라우트들 다음에 등록
// =============================================================================

/**
 * GET /api/virtual-points
 * 가상포인트 목록 조회 (페이징 지원)
 */
router.get('/', async (req, res) => {
  try {
    console.log('가상포인트 목록 조회 요청:', req.query);

    // 필터 구성 (DeviceRepository 패턴과 동일)
    const filters = {
      tenant_id: req.query.tenant_id || 1,
      site_id: req.query.site_id,
      device_id: req.query.device_id,
      scope_type: req.query.scope_type,
      is_enabled: req.query.is_enabled !== undefined ? req.query.is_enabled === 'true' : undefined,
      category: req.query.category,
      calculation_trigger: req.query.calculation_trigger,
      search: req.query.search,
      limit: req.query.limit ? parseInt(req.query.limit) : 50,
      page: req.query.page ? parseInt(req.query.page) : 1
    };

    // undefined 값 제거
    Object.keys(filters).forEach(key => filters[key] === undefined && delete filters[key]);

    console.log('적용된 필터:', filters);

    // DeviceRepository 패턴 사용: findAllVirtualPoints 호출
    const result = await virtualPointRepo.findAllVirtualPoints(filters);

    console.log(`${result.items.length}개 가상포인트 조회 완료`);

    // 프론트엔드 호환성: data를 배열로 직접 전달
    res.json(createResponse(
      true, 
      result.items,  // 배열 형식으로 전달 (프론트엔드 호환)
      `Found ${result.items.length} virtual points`
    ));

  } catch (error) {
    console.error('가상포인트 목록 조회 실패:', error);
    res.status(500).json(createResponse(
      false, 
      null, 
      'Failed to fetch virtual points', 
      'FETCH_ERROR'
    ));
  }
});

/**
 * POST /api/virtual-points
 * 가상포인트 생성 (완전 생성 - 관련 테이블 포함)
 */
router.post('/', async (req, res) => {
  try {
    const { virtualPoint, inputs = [] } = req.body;
    const tenantId = req.body.tenant_id || virtualPoint?.tenant_id || 1;

    console.log('가상포인트 생성 요청:', {
      name: virtualPoint?.name,
      inputsCount: inputs.length,
      tenantId
    });

    // 필수 필드 검증
    if (!virtualPoint || !virtualPoint.name) {
      console.log('필수 필드 누락: name');
      return res.status(400).json(createResponse(
        false, 
        null, 
        'Missing required fields: name', 
        'VALIDATION_ERROR'
      ));
    }

    // formula 필드 매핑 (프론트엔드 호환성)
    if (virtualPoint.expression && !virtualPoint.formula) {
      virtualPoint.formula = virtualPoint.expression;
    }

    // DeviceRepository 패턴 사용: createVirtualPoint
    const createdVirtualPoint = await virtualPointRepo.createVirtualPoint(
      virtualPoint, 
      inputs, 
      tenantId
    );

    console.log(`가상포인트 생성 성공: ${createdVirtualPoint.name} (ID: ${createdVirtualPoint.id})`);

    res.status(201).json(createResponse(
      true, 
      createdVirtualPoint, 
      'Virtual point created successfully'
    ));

  } catch (error) {
    console.error('가상포인트 생성 실패:', error);
    res.status(500).json(createResponse(
      false, 
      null, 
      `Failed to create virtual point: ${error.message}`, 
      'CREATE_ERROR'
    ));
  }
});

/**
 * GET /api/virtual-points/:id
 * 가상포인트 상세 조회 (관련 데이터 포함)
 */
router.get('/:id', async (req, res) => {
  try {
    const { id } = req.params;
    const tenantId = req.query.tenant_id || 1;

    console.log(`가상포인트 상세 조회: ID ${id}, 테넌트 ${tenantId}`);

    // DeviceRepository 패턴 사용
    const virtualPoint = await virtualPointRepo.findById(parseInt(id), tenantId);

    if (!virtualPoint) {
      console.log(`가상포인트 ID ${id} 찾을 수 없음`);
      return res.status(404).json(createResponse(
        false, 
        null, 
        'Virtual point not found', 
        'NOT_FOUND'
      ));
    }

    console.log(`가상포인트 조회 성공: ${virtualPoint.name}`);

    res.json(createResponse(
      true, 
      virtualPoint, 
      'Virtual point retrieved successfully'
    ));

  } catch (error) {
    console.error('가상포인트 상세 조회 실패:', error);
    res.status(500).json(createResponse(
      false, 
      null, 
      'Failed to fetch virtual point', 
      'FETCH_ERROR'
    ));
  }
});

/**
 * PUT /api/virtual-points/:id
 * 가상포인트 업데이트 (관련 테이블 포함)
 */
router.put('/:id', async (req, res) => {
  try {
    const { id } = req.params;
    const { virtualPoint, inputs = null } = req.body;
    const tenantId = req.body.tenant_id || virtualPoint?.tenant_id || 1;

    console.log(`가상포인트 업데이트 요청: ID ${id}`, {
      name: virtualPoint?.name,
      hasInputs: inputs !== null,
      tenantId
    });

    // 존재 확인 (DeviceRepository 패턴)
    const existing = await virtualPointRepo.findById(parseInt(id), tenantId);
    if (!existing) {
      console.log(`가상포인트 ID ${id} 찾을 수 없음`);
      return res.status(404).json(createResponse(
        false, 
        null, 
        'Virtual point not found', 
        'NOT_FOUND'
      ));
    }

    // formula 필드 매핑 (프론트엔드 호환성)
    if (virtualPoint.expression && !virtualPoint.formula) {
      virtualPoint.formula = virtualPoint.expression;
    }

    // DeviceRepository 패턴 사용: updateVirtualPoint
    const updatedVirtualPoint = await virtualPointRepo.updateVirtualPoint(
      parseInt(id), 
      virtualPoint, 
      inputs, 
      tenantId
    );

    console.log(`가상포인트 업데이트 성공: ${updatedVirtualPoint.name}`);

    res.json(createResponse(
      true, 
      updatedVirtualPoint, 
      'Virtual point updated successfully'
    ));

  } catch (error) {
    console.error('가상포인트 업데이트 실패:', error);
    res.status(500).json(createResponse(
      false, 
      null, 
      `Failed to update virtual point: ${error.message}`, 
      'UPDATE_ERROR'
    ));
  }
});

/**
 * DELETE /api/virtual-points/:id
 * 가상포인트 삭제 (CASCADE DELETE)
 */
router.delete('/:id', async (req, res) => {
  try {
    const { id } = req.params;
    const tenantId = req.query.tenant_id || 1;

    console.log(`가상포인트 삭제 요청: ID ${id}, 테넌트 ${tenantId}`);

    // 존재 확인
    const existing = await virtualPointRepo.findById(parseInt(id), tenantId);
    if (!existing) {
      console.log(`가상포인트 ID ${id} 찾을 수 없음`);
      return res.status(404).json(createResponse(
        false, 
        null, 
        'Virtual point not found', 
        'NOT_FOUND'
      ));
    }

    console.log(`삭제 대상 확인: ${existing.name} (ID: ${id})`);

    // DeviceRepository 패턴 사용: deleteById (CASCADE DELETE)
    const deleted = await virtualPointRepo.deleteById(parseInt(id), tenantId);

    if (deleted) {
      console.log(`가상포인트 삭제 성공: ${existing.name} (ID: ${id})`);
      res.json(createResponse(
        true, 
        { id: parseInt(id), name: existing.name, deleted: true }, 
        'Virtual point deleted successfully'
      ));
    } else {
      console.log(`가상포인트 삭제 실패: ID ${id}`);
      res.status(500).json(createResponse(
        false, 
        null, 
        'Failed to delete virtual point - no records affected', 
        'DELETE_ERROR'
      ));
    }

  } catch (error) {
    console.error('가상포인트 삭제 실패:', error);
    res.status(500).json(createResponse(
      false, 
      null, 
      `Failed to delete virtual point: ${error.message}`, 
      'DELETE_ERROR'
    ));
  }
});

module.exports = router;