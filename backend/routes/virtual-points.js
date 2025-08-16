// =============================================================================
// backend/routes/virtual-points.js 
// 가상포인트 API 라우트 - 올바른 Repository 임포트
// =============================================================================

const express = require('express');
const router = express.Router();

// 올바른 Repository 임포트 경로
const VirtualPointRepository = require('../lib/database/repositories/VirtualPointRepository');

// Repository 인스턴스 생성
const virtualPointRepo = new VirtualPointRepository();

// 공통 응답 함수
const createResponse = (success, data, message, error_code = null) => ({
    success,
    data,
    message,
    error_code,
    timestamp: new Date().toISOString()
});

// =============================================================================
// 가상포인트 CRUD API
// =============================================================================

/**
 * GET /api/virtual-points
 * 가상포인트 목록 조회
 */
router.get('/', async (req, res) => {
  try {
    const filters = {
      tenant_id: req.query.tenant_id || 1,
      site_id: req.query.site_id,
      device_id: req.query.device_id,
      scope_type: req.query.scope_type,
      is_enabled: req.query.is_enabled !== undefined ? req.query.is_enabled === 'true' : undefined,
      category: req.query.category,
      calculation_trigger: req.query.calculation_trigger,
      search: req.query.search,
      limit: req.query.limit ? parseInt(req.query.limit) : 50
    };

    // undefined 값 제거
    Object.keys(filters).forEach(key => filters[key] === undefined && delete filters[key]);

    console.log('Virtual points filters:', filters);

    const virtualPoints = await virtualPointRepo.findAll(filters);

    res.json(createResponse(true, virtualPoints, `Found ${virtualPoints.length} virtual points`));
  } catch (error) {
    console.error('Error fetching virtual points:', error);
    res.status(500).json(createResponse(false, null, 'Failed to fetch virtual points', 'FETCH_ERROR'));
  }
});

/**
 * GET /api/virtual-points/:id
 * 가상포인트 상세 조회
 */
router.get('/:id', async (req, res) => {
  try {
    const { id } = req.params;
    const virtualPoint = await virtualPointRepo.findById(parseInt(id));

    if (!virtualPoint) {
      return res.status(404).json(createResponse(false, null, 'Virtual point not found', 'NOT_FOUND'));
    }

    res.json(createResponse(true, virtualPoint, 'Virtual point retrieved successfully'));
  } catch (error) {
    console.error('Error fetching virtual point:', error);
    res.status(500).json(createResponse(false, null, 'Failed to fetch virtual point', 'FETCH_ERROR'));
  }
});

/**
 * POST /api/virtual-points
 * 가상포인트 생성
 */
router.post('/', async (req, res) => {
  try {
    const { virtualPoint, inputs = [] } = req.body;

    console.log('Creating virtual point:', virtualPoint);

    // 필수 필드 검증
    if (!virtualPoint || !virtualPoint.name) {
      return res.status(400).json(createResponse(false, null, 'Missing required fields: name', 'VALIDATION_ERROR'));
    }

    const createdVirtualPoint = await virtualPointRepo.create(virtualPoint, inputs);

    res.status(201).json(createResponse(true, createdVirtualPoint, 'Virtual point created successfully'));
  } catch (error) {
    console.error('Error creating virtual point:', error);
    res.status(500).json(createResponse(false, null, 'Failed to create virtual point', 'CREATE_ERROR'));
  }
});

/**
 * PUT /api/virtual-points/:id
 * 가상포인트 업데이트
 */
router.put('/:id', async (req, res) => {
  try {
    const { id } = req.params;
    const { virtualPoint, inputs = null } = req.body;

    // 존재 확인
    const existing = await virtualPointRepo.findById(parseInt(id));
    if (!existing) {
      return res.status(404).json(createResponse(false, null, 'Virtual point not found', 'NOT_FOUND'));
    }

    const updatedVirtualPoint = await virtualPointRepo.update(parseInt(id), virtualPoint, inputs);

    res.json(createResponse(true, updatedVirtualPoint, 'Virtual point updated successfully'));
  } catch (error) {
    console.error('Error updating virtual point:', error);
    res.status(500).json(createResponse(false, null, 'Failed to update virtual point', 'UPDATE_ERROR'));
  }
});

/**
 * DELETE /api/virtual-points/:id
 * 가상포인트 삭제
 */
router.delete('/:id', async (req, res) => {
  try {
    const { id } = req.params;

    // 존재 확인
    const existing = await virtualPointRepo.findById(parseInt(id));
    if (!existing) {
      return res.status(404).json(createResponse(false, null, 'Virtual point not found', 'NOT_FOUND'));
    }

    const deleted = await virtualPointRepo.delete(parseInt(id));

    if (deleted) {
      res.json(createResponse(true, null, 'Virtual point deleted successfully'));
    } else {
      res.status(500).json(createResponse(false, null, 'Failed to delete virtual point', 'DELETE_ERROR'));
    }
  } catch (error) {
    console.error('Error deleting virtual point:', error);
    res.status(500).json(createResponse(false, null, 'Failed to delete virtual point', 'DELETE_ERROR'));
  }
});

// =============================================================================
// 통계 API
// =============================================================================

/**
 * GET /api/virtual-points/stats/category
 * 카테고리별 가상포인트 통계
 */
router.get('/stats/category', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id || 1;
    const stats = await virtualPointRepo.getStatsByCategory(tenantId);

    res.json(createResponse(true, stats, 'Category statistics retrieved successfully'));
  } catch (error) {
    console.error('Error fetching category stats:', error);
    res.status(500).json(createResponse(false, null, 'Failed to fetch category stats', 'STATS_ERROR'));
  }
});

/**
 * GET /api/virtual-points/stats/performance
 * 가상포인트 성능 통계
 */
router.get('/stats/performance', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id || 1;
    const stats = await virtualPointRepo.getPerformanceStats(tenantId);

    res.json(createResponse(true, stats, 'Performance statistics retrieved successfully'));
  } catch (error) {
    console.error('Error fetching performance stats:', error);
    res.status(500).json(createResponse(false, null, 'Failed to fetch performance stats', 'STATS_ERROR'));
  }
});

module.exports = router;
