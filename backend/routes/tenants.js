const express = require('express');
const router = express.Router();
const TenantService = require('../lib/services/TenantService');

const tenantService = new TenantService();

/**
 * @route GET /api/tenants
 * @desc 모든 테넌트 목록 조회
 */
router.get('/', async (req, res) => {
    const options = {
        isActive: req.query.isActive === 'true' ? true : (req.query.isActive === 'false' ? false : undefined),
        search: req.query.search,
        page: req.query.page,
        limit: req.query.limit,
        includeDeleted: req.query.includeDeleted === 'true',
        onlyDeleted: req.query.onlyDeleted === 'true'
    };

    const result = await tenantService.getAllTenants(options);
    res.status(result.success ? 200 : 500).json(result);
});

/**
 * @route GET /api/tenants/stats
 * @desc 테넌트 통계 요약 조회
 */
router.get('/stats', async (req, res) => {
    const result = await tenantService.getGlobalStatistics();
    res.status(result.success ? 200 : 500).json(result);
});

/**
 * @route GET /api/tenants/:id
 * @desc 특정 테넌트 상세 조회
 */
router.get('/:id', async (req, res) => {
    const result = await tenantService.getTenantById(req.params.id);
    res.status(result.success ? 200 : 404).json(result);
});

/**
 * @route POST /api/tenants
 * @desc 새로운 테넌트 생성
 */
router.post('/', async (req, res) => {
    const result = await tenantService.createTenant(req.body);
    res.status(result.success ? 201 : 400).json(result);
});

/**
 * @route PUT /api/tenants/:id
 * @desc 테넌트 정보 업데이트
 */
router.put('/:id', async (req, res) => {
    const result = await tenantService.updateTenant(req.params.id, req.body);
    res.status(result.success ? 200 : 400).json(result);
});

/**
 * @route DELETE /api/tenants/:id
 * @desc 테넌트 삭제 (소프트 삭제)
 */
router.delete('/:id', async (req, res) => {
    const result = await tenantService.deleteTenant(req.params.id);
    res.status(result.success ? 200 : 400).json(result);
});

/**
 * @route POST /api/tenants/:id/restore
 * @desc 테넌트 복구
 */
router.post('/:id/restore', async (req, res) => {
    const result = await tenantService.restoreTenant(req.params.id);
    res.status(result.success ? 200 : 400).json(result);
});

module.exports = router;
