const express = require('express');
const router = express.Router();
const ManufacturerService = require('../lib/services/ManufacturerService');

const manufacturerService = new ManufacturerService();

/**
 * @route GET /api/manufacturers
 * @desc 모든 제조사 목록 조회
 */
router.get('/', async (req, res) => {
    const options = {
        isActive: req.query.isActive === 'true' ? true : (req.query.isActive === 'false' ? false : undefined),
        search: req.query.search,
        page: req.query.page,
        limit: req.query.limit,
        sort_by: req.query.sort_by,
        sort_order: req.query.sort_order,
        includeDeleted: req.query.includeDeleted === 'true',
        onlyDeleted: req.query.onlyDeleted === 'true'
    };

    const result = await manufacturerService.getAllManufacturers(options);
    res.status(result.success ? 200 : 500).json(result);
});

/**
 * @route GET /api/manufacturers/stats
 * @desc 제조사 통계 정보 조회
 */
router.get('/stats', async (req, res) => {
    const result = await manufacturerService.getStatistics();
    res.status(result.success ? 200 : 500).json(result);
});

/**
 * @route GET /api/manufacturers/:id
 * @desc 특정 제조사 상세 조회
 */
router.get('/:id', async (req, res) => {
    const result = await manufacturerService.getManufacturerById(req.params.id);
    res.status(result.success ? 200 : (result.message.includes('찾을 수 없습니다') ? 404 : 500)).json(result);
});

/**
 * @route POST /api/manufacturers
 * @desc 새로운 제조사 생성
 */
router.post('/', async (req, res) => {
    const result = await manufacturerService.createManufacturer(req.body);
    res.status(result.success ? 201 : 400).json(result);
});

/**
 * @route PUT /api/manufacturers/:id
 * @desc 제조사 정보 업데이트
 */
router.put('/:id', async (req, res) => {
    const result = await manufacturerService.updateManufacturer(req.params.id, req.body);
    res.status(result.success ? 200 : 400).json(result);
});

/**
 * @route DELETE /api/manufacturers/:id
 * @desc 제조사 삭제
 */
router.delete('/:id', async (req, res) => {
    const result = await manufacturerService.deleteManufacturer(req.params.id);
    res.status(result.success ? 200 : 400).json(result);
});

/**
 * @route POST /api/manufacturers/:id/restore
 * @desc 제조사 복구
 */
router.post('/:id/restore', async (req, res) => {
    const result = await manufacturerService.restoreManufacturer(req.params.id);
    res.status(result.success ? 200 : 400).json(result);
});

module.exports = router;
