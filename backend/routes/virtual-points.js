// =============================================================================
// backend/routes/virtual-points.js
// 가상포인트 API 라우트 - routes/alarms.js 패턴 준수
// =============================================================================

const express = require('express');
const router = express.Router();
const VirtualPointRepository = require('../lib/database/repositories/VirtualPointRepository');

// Repository 인스턴스 생성
const virtualPointRepo = new VirtualPointRepository();

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
      tenant_id: req.query.tenant_id,
      site_id: req.query.site_id,
      device_id: req.query.device_id,
      scope_type: req.query.scope_type,
      is_enabled: req.query.is_enabled !== undefined ? req.query.is_enabled === 'true' : undefined,
      category: req.query.category,
      calculation_trigger: req.query.calculation_trigger,
      search: req.query.search,
      limit: req.query.limit
    };

    // undefined 값 제거
    Object.keys(filters).forEach(key => filters[key] === undefined && delete filters[key]);

    const virtualPoints = await virtualPointRepo.findAll(filters);

    res.json({
      success: true,
      data: virtualPoints,
      count: virtualPoints.length
    });
  } catch (error) {
    console.error('Error fetching virtual points:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch virtual points',
      details: error.message
    });
  }
});

/**
 * GET /api/virtual-points/:id
 * 가상포인트 상세 조회
 */
router.get('/:id', async (req, res) => {
  try {
    const { id } = req.params;
    const virtualPoint = await virtualPointRepo.findById(id);

    if (!virtualPoint) {
      return res.status(404).json({
        success: false,
        error: 'Virtual point not found'
      });
    }

    res.json({
      success: true,
      data: virtualPoint
    });
  } catch (error) {
    console.error('Error fetching virtual point:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch virtual point',
      details: error.message
    });
  }
});

/**
 * POST /api/virtual-points
 * 가상포인트 생성
 */
router.post('/', async (req, res) => {
  try {
    const { virtualPoint, inputs = [] } = req.body;

    // 필수 필드 검증
    if (!virtualPoint.name || !virtualPoint.formula || !virtualPoint.tenant_id) {
      return res.status(400).json({
        success: false,
        error: 'Missing required fields: name, formula, tenant_id'
      });
    }

    // 순환 의존성 검사 (입력에 virtual_point가 있는 경우)
    for (const input of inputs) {
      if (input.source_type === 'virtual_point' && input.source_id) {
        const hasCircular = await virtualPointRepo.checkCircularDependency(input.source_id, virtualPoint.id);
        if (hasCircular) {
          return res.status(400).json({
            success: false,
            error: `Circular dependency detected with virtual point ${input.source_id}`
          });
        }
      }
    }

    const createdVirtualPoint = await virtualPointRepo.create(virtualPoint, inputs);

    res.status(201).json({
      success: true,
      data: createdVirtualPoint,
      message: 'Virtual point created successfully'
    });
  } catch (error) {
    console.error('Error creating virtual point:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to create virtual point',
      details: error.message
    });
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
    const existing = await virtualPointRepo.findById(id);
    if (!existing) {
      return res.status(404).json({
        success: false,
        error: 'Virtual point not found'
      });
    }

    // 순환 의존성 검사 (입력이 업데이트되는 경우)
    if (inputs) {
      for (const input of inputs) {
        if (input.source_type === 'virtual_point' && input.source_id && input.source_id != id) {
          const hasCircular = await virtualPointRepo.checkCircularDependency(input.source_id, id);
          if (hasCircular) {
            return res.status(400).json({
              success: false,
              error: `Circular dependency detected with virtual point ${input.source_id}`
            });
          }
        }
      }
    }

    const updatedVirtualPoint = await virtualPointRepo.update(id, virtualPoint, inputs);

    res.json({
      success: true,
      data: updatedVirtualPoint,
      message: 'Virtual point updated successfully'
    });
  } catch (error) {
    console.error('Error updating virtual point:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to update virtual point',
      details: error.message
    });
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
    const existing = await virtualPointRepo.findById(id);
    if (!existing) {
      return res.status(404).json({
        success: false,
        error: 'Virtual point not found'
      });
    }

    const deleted = await virtualPointRepo.delete(id);

    if (deleted) {
      res.json({
        success: true,
        message: 'Virtual point deleted successfully'
      });
    } else {
      res.status(500).json({
        success: false,
        error: 'Failed to delete virtual point'
      });
    }
  } catch (error) {
    console.error('Error deleting virtual point:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to delete virtual point',
      details: error.message
    });
  }
});

// =============================================================================
// 가상포인트 관리 API
// =============================================================================

/**
 * GET /api/virtual-points/:id/dependencies
 * 가상포인트 의존성 조회
 */
router.get('/:id/dependencies', async (req, res) => {
  try {
    const { id } = req.params;
    const dependencies = await virtualPointRepo.getDependencies(id);

    res.json({
      success: true,
      data: dependencies
    });
  } catch (error) {
    console.error('Error fetching dependencies:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch dependencies',
      details: error.message
    });
  }
});

/**
 * GET /api/virtual-points/:id/history
 * 가상포인트 실행 이력 조회
 */
router.get('/:id/history', async (req, res) => {
  try {
    const { id } = req.params;
    const limit = parseInt(req.query.limit) || 50;
    
    const history = await virtualPointRepo.getExecutionHistory(id, limit);

    res.json({
      success: true,
      data: history,
      count: history.length
    });
  } catch (error) {
    console.error('Error fetching execution history:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch execution history',
      details: error.message
    });
  }
});

/**
 * POST /api/virtual-points/:id/test
 * 가상포인트 계산 테스트
 */
router.post('/:id/test', async (req, res) => {
  try {
    const { id } = req.params;
    const { testInputs = {} } = req.body;

    // 가상포인트 조회
    const virtualPoint = await virtualPointRepo.findById(id);
    if (!virtualPoint) {
      return res.status(404).json({
        success: false,
        error: 'Virtual point not found'
      });
    }

    // 간단한 JavaScript 실행 환경 시뮬레이션
    const startTime = Date.now();
    let result, error;

    try {
      // 입력값 준비
      const inputs = {};
      virtualPoint.inputs.forEach(input => {
        if (testInputs[input.variable_name] !== undefined) {
          inputs[input.variable_name] = testInputs[input.variable_name];
        } else if (input.constant_value !== null) {
          inputs[input.variable_name] = input.constant_value;
        } else {
          inputs[input.variable_name] = 0; // 기본값
        }
      });

      // 간단한 수식 평가 (실제 구현에서는 안전한 JavaScript 실행 환경 필요)
      const formula = virtualPoint.formula;
      const evalFunction = new Function(...Object.keys(inputs), `return ${formula}`);
      result = evalFunction(...Object.values(inputs));

    } catch (evalError) {
      error = evalError.message;
    }

    const executionTime = Date.now() - startTime;

    res.json({
      success: true,
      data: {
        result,
        error,
        executionTime,
        inputs: testInputs,
        formula: virtualPoint.formula
      }
    });
  } catch (error) {
    console.error('Error testing virtual point:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to test virtual point',
      details: error.message
    });
  }
});

/**
 * POST /api/virtual-points/:id/execute
 * 가상포인트 수동 실행
 */
router.post('/:id/execute', async (req, res) => {
  try {
    const { id } = req.params;

    // 가상포인트 조회
    const virtualPoint = await virtualPointRepo.findById(id);
    if (!virtualPoint) {
      return res.status(404).json({
        success: false,
        error: 'Virtual point not found'
      });
    }

    if (!virtualPoint.is_enabled) {
      return res.status(400).json({
        success: false,
        error: 'Virtual point is disabled'
      });
    }

    // 실행 시뮬레이션 (실제 구현에서는 가상포인트 엔진에서 처리)
    const startTime = Date.now();
    let result, error;

    try {
      // 여기서 실제 가상포인트 계산 로직 호출
      result = `Executed at ${new Date().toISOString()}`;
    } catch (execError) {
      error = execError.message;
    }

    const executionTime = Date.now() - startTime;

    // 실행 이력 추가
    await virtualPointRepo.addExecutionHistory(
      id,
      executionTime,
      { value: result },
      {},
      !error,
      error
    );

    // 통계 업데이트
    await virtualPointRepo.updateExecutionStats(id, executionTime, error);

    res.json({
      success: true,
      data: {
        result,
        error,
        executionTime,
        timestamp: new Date().toISOString()
      },
      message: 'Virtual point executed successfully'
    });
  } catch (error) {
    console.error('Error executing virtual point:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to execute virtual point',
      details: error.message
    });
  }
});

// =============================================================================
// 통계 및 모니터링 API
// =============================================================================

/**
 * GET /api/virtual-points/stats/category
 * 카테고리별 가상포인트 통계
 */
router.get('/stats/category', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id;
    
    if (!tenantId) {
      return res.status(400).json({
        success: false,
        error: 'tenant_id is required'
      });
    }

    const stats = await virtualPointRepo.getStatsByCategory(tenantId);

    res.json({
      success: true,
      data: stats
    });
  } catch (error) {
    console.error('Error fetching category stats:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch category stats',
      details: error.message
    });
  }
});

/**
 * GET /api/virtual-points/stats/performance
 * 가상포인트 성능 통계
 */
router.get('/stats/performance', async (req, res) => {
  try {
    const tenantId = req.query.tenant_id;
    
    if (!tenantId) {
      return res.status(400).json({
        success: false,
        error: 'tenant_id is required'
      });
    }

    const stats = await virtualPointRepo.getPerformanceStats(tenantId);

    res.json({
      success: true,
      data: stats
    });
  } catch (error) {
    console.error('Error fetching performance stats:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch performance stats',
      details: error.message
    });
  }
});

/**
 * PUT /api/virtual-points/:id/value
 * 가상포인트 값 수동 업데이트 (테스트용)
 */
router.put('/:id/value', async (req, res) => {
  try {
    const { id } = req.params;
    const { value, stringValue, quality = 'good' } = req.body;

    await virtualPointRepo.updateValue(id, value, stringValue, quality, null, null);

    res.json({
      success: true,
      message: 'Virtual point value updated successfully'
    });
  } catch (error) {
    console.error('Error updating virtual point value:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to update virtual point value',
      details: error.message
    });
  }
});

module.exports = router;