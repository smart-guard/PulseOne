const express = require('express');
const router = express.Router();

// VirtualPointService 임포트
const VirtualPointService = require('../lib/services/VirtualPointService');

// 공통 응답 함수
const createResponse = (success, data, message, error_code = null) => ({
    success,
    data,
    message,
    error_code,
    timestamp: new Date().toISOString()
});

// 미들웨어 (간단한 버전)
const authenticateToken = (req, res, next) => {
    req.user = { id: 1, tenant_id: 1, role: 'admin' };
    req.tenantId = 1;
    next();
};


// =============================================================================
// 통계 API
// =============================================================================

/**
 * GET /api/virtual-points/stats/category
 */
router.get('/stats/category', async (req, res) => {
    try {
        const result = await VirtualPointService.getStatsByCategory(req.tenantId);
        res.json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'STATS_ERROR'));
    }
});

/**
 * GET /api/virtual-points/stats/performance
 */
router.get('/stats/performance', async (req, res) => {
    try {
        const result = await VirtualPointService.getPerformanceStats(req.tenantId);
        res.json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'STATS_ERROR'));
    }
});

/**
 * GET /api/virtual-points/stats/summary
 */
router.get('/stats/summary', async (req, res) => {
    try {
        const [categoryStats, performanceStats] = await Promise.all([
            VirtualPointService.getStatsByCategory(req.tenantId),
            VirtualPointService.getPerformanceStats(req.tenantId)
        ]);

        const summary = {
            total_virtual_points: performanceStats.data.total_points || 0,
            enabled_virtual_points: performanceStats.data.enabled_points || 0,
            disabled_virtual_points: (performanceStats.data.total_points || 0) - (performanceStats.data.enabled_points || 0),
            category_distribution: categoryStats.data,
            last_updated: new Date().toISOString()
        };

        res.json(createResponse(true, summary, 'Summary statistics retrieved successfully'));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'STATS_ERROR'));
    }
});

// =============================================================================
// 메인 CRUD API
// =============================================================================

/**
 * GET /api/virtual-points
 */
router.get('/', async (req, res) => {
    try {
        const filters = {
            tenantId: req.tenantId,
            siteId: req.query.site_id,
            deviceId: req.query.device_id,
            scopeType: req.query.scope_type,
            is_enabled: req.query.is_enabled !== undefined ? req.query.is_enabled === 'true' : undefined,
            category: req.query.category,
            calculation_trigger: req.query.calculation_trigger,
            search: req.query.search,
            include_deleted: req.query.include_deleted === 'true',
            limit: parseInt(req.query.limit) || 50,
            page: parseInt(req.query.page) || 1
        };

        const result = await VirtualPointService.findAll(filters);
        // 프론트엔드 호환성을 위해 data.items를 직접 반환하거나 감싸서 반환
        res.json(createResponse(true, result.data.items, `Found ${result.data.items.length} virtual points` || result.message));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'FETCH_ERROR'));
    }
});

/**
 * POST /api/virtual-points
 */
router.post('/', async (req, res) => {
    try {
        const { virtualPoint, inputs = [] } = req.body;
        if (!virtualPoint || !virtualPoint.name) {
            return res.status(400).json(createResponse(false, null, 'Missing required fields: name', 'VALIDATION_ERROR'));
        }

        const result = await VirtualPointService.create(virtualPoint, inputs, req.tenantId);
        res.status(201).json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'CREATE_ERROR'));
    }
});

/**
 * GET /api/virtual-points/:id
 */
router.get('/:id', async (req, res) => {
    try {
        const result = await VirtualPointService.findById(req.params.id, req.tenantId);
        res.json(result);
    } catch (error) {
        const status = error.message.includes('찾을 수 없습니다') ? 404 : 500;
        res.status(status).json(createResponse(false, null, error.message, 'FETCH_ERROR'));
    }
});

/**
 * GET /api/virtual-points/:id/logs
 */
router.get('/:id/logs', async (req, res) => {
    try {
        const result = await VirtualPointService.getHistory(req.params.id, req.tenantId);
        res.json(result);
    } catch (error) {
        const status = error.message.includes('찾을 수 없습니다') ? 404 : 500;
        res.status(status).json(createResponse(false, null, error.message, 'FETCH_LOGS_ERROR'));
    }
});

/**
 * POST /api/virtual-points/:id/execute
 */
router.post('/:id/execute', async (req, res) => {
    try {
        const options = req.body || {};
        const result = await VirtualPointService.execute(req.params.id, options, req.tenantId);

        // 프론트엔드는 result.result 및 success 여부를 확인
        if (result.success) {
            res.json(createResponse(true, result, 'Execution successful'));
        } else {
            res.status(400).json(createResponse(false, result, result.error || 'Execution failed', 'EXECUTION_ERROR'));
        }
    } catch (error) {
        const status = error.message.includes('찾을 수 없습니다') ? 404 : 500;
        res.status(status).json(createResponse(false, null, error.message, 'EXECUTION_ERROR'));
    }
});

/**
 * PUT /api/virtual-points/:id
 */
router.put('/:id', async (req, res) => {
    try {
        const { virtualPoint, inputs = null } = req.body;
        const result = await VirtualPointService.update(req.params.id, virtualPoint, inputs, req.tenantId);
        res.json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'UPDATE_ERROR'));
    }
});

/**
 * DELETE /api/virtual-points/:id
 */
router.delete('/:id', async (req, res) => {
    try {
        const result = await VirtualPointService.delete(req.params.id, req.tenantId);
        res.json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'DELETE_ERROR'));
    }
});

/**
 * PATCH /api/virtual-points/:id/restore
 */
router.patch('/:id/restore', async (req, res) => {
    try {
        const result = await VirtualPointService.restore(req.params.id, req.tenantId);
        res.json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'RESTORE_ERROR'));
    }
});

/**
 * PATCH /api/virtual-points/:id/toggle
 */
router.patch('/:id/toggle', async (req, res) => {
    try {
        const { is_enabled } = req.body;
        const result = await VirtualPointService.toggleEnabled(req.params.id, is_enabled, req.tenantId);
        res.json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'TOGGLE_ERROR'));
    }
});

/**
 * POST /api/virtual-points/admin/cleanup-orphaned
 */
router.post('/admin/cleanup-orphaned', async (req, res) => {
    try {
        const result = await VirtualPointService.cleanupOrphaned(req.tenantId);
        res.json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'CLEANUP_ERROR'));
    }
});

module.exports = router;