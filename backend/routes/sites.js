// =============================================================================
// backend/routes/sites.js
// Site Management API Routes
// =============================================================================

const express = require('express');
const router = express.Router();
const SiteService = require('../lib/services/SiteService');
const { requireRole } = require('../middleware/tenantIsolation');

// 인증/테넌트 격리는 app.js에서 /api/* 전역으로 이미 적용됨
// router.use(authenticateToken); ← 중복 제거 (이중 적용 시 req.tenantId 덮어씌워짐)
// router.use(tenantIsolation);   ← 중복 제거

/**
 * @route   GET /api/sites
 * @desc    Get all sites for the current tenant
 * @access  Private
 */
router.get('/', async (req, res) => {
    const response = await SiteService.getAllSites(req.tenantId, req.query);
    res.status(response.success ? 200 : 400).json(response);
});

/**
 * @route   GET /api/sites/tree
 * @desc    Get site hierarchy tree
 * @access  Private
 */
router.get('/tree', async (req, res) => {
    const response = await SiteService.getHierarchyTree(req.tenantId);
    res.status(response.success ? 200 : 400).json(response);
});

/**
 * @route   GET /api/sites/:id
 * @desc    Get specific site by ID
 * @access  Private
 */
router.get('/:id', async (req, res) => {
    const response = await SiteService.getSiteById(req.params.id, req.tenantId);
    res.status(response.success ? 200 : 404).json(response);
});

/**
 * @route   GET /api/sites/:id/children
 * @desc    Get child sites for a parent site
 * @access  Private
 */
router.get('/:id/children', async (req, res) => {
    const response = await SiteService.getSitesByParentId(req.params.id, req.tenantId);
    res.status(response.success ? 200 : 400).json(response);
});

/**
 * @route   POST /api/sites
 * @desc    Create a new site
 * @access  Private (Admin/Manager)
 */
router.post('/', requireRole('system_admin', 'company_admin', 'site_manager'), async (req, res) => {
    const response = await SiteService.createSite(req.body, req.tenantId);
    res.status(response.success ? 201 : 400).json(response);
});

/**
 * @route   PUT /api/sites/:id
 * @desc    Update an existing site
 * @access  Private (Admin/Manager)
 */
router.put('/:id', requireRole('system_admin', 'company_admin', 'site_manager'), async (req, res) => {
    const response = await SiteService.updateSite(req.params.id, req.body, req.tenantId);
    res.status(response.success ? 200 : 400).json(response);
});

/**
 * @route   PATCH /api/sites/:id
 * @desc    Partially update a site
 * @access  Private (Admin/Manager)
 */
router.patch('/:id', requireRole('system_admin', 'company_admin', 'site_manager'), async (req, res) => {
    const response = await SiteService.patchSite(req.params.id, req.body, req.tenantId);
    res.status(response.success ? 200 : 400).json(response);
});

/**
 * @route   DELETE /api/sites/:id
 * @desc    Delete a site
 * @access  Private (Admin)
 */
router.delete('/:id', requireRole('system_admin', 'company_admin'), async (req, res) => {
    const response = await SiteService.deleteSite(req.params.id, req.tenantId);
    res.status(response.success ? 200 : 400).json(response);
});

/**
 * @route   POST /api/sites/:id/restore
 * @desc    Restore a deleted site
 * @access  Private (Admin)
 */
router.post('/:id/restore', requireRole('system_admin', 'company_admin'), async (req, res) => {
    const response = await SiteService.restoreSite(req.params.id, req.tenantId);
    res.status(response.success ? 200 : 400).json(response);
});

module.exports = router;
