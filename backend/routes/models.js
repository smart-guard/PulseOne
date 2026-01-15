const express = require('express');
const router = express.Router();
const DeviceModelService = require('../lib/services/DeviceModelService');

const deviceModelService = new DeviceModelService();

/**
 * @route GET /api/models
 * @desc 모든 디바이스 모델 목록 조회
 */
router.get('/', async (req, res) => {
    const options = {
        manufacturerId: req.query.manufacturerId || req.query.manufacturer_id,
        deviceType: req.query.deviceType || req.query.device_type,
        isActive: req.query.isActive === 'true' ? true : (req.query.isActive === 'false' ? false : undefined),
        search: req.query.search,
        page: req.query.page,
        limit: req.query.limit,
        sort_by: req.query.sort_by,
        sort_order: req.query.sort_order
    };

    const result = await deviceModelService.getAllModels(options);
    res.status(result.success ? 200 : 500).json(result);
});

/**
 * @route GET /api/models/:id
 * @desc 특정 디바이스 모델 상세 조회
 */
router.get('/:id', async (req, res) => {
    const result = await deviceModelService.getModelById(req.params.id);
    res.status(result.success ? 200 : (result.message.includes('찾을 수 없습니다') ? 404 : 500)).json(result);
});

/**
 * @route POST /api/models
 * @desc 새로운 디바이스 모델 생성
 */
router.post('/', async (req, res) => {
    const result = await deviceModelService.createModel(req.body);
    res.status(result.success ? 201 : 400).json(result);
});

/**
 * @route PUT /api/models/:id
 * @desc 디바이스 모델 정보 업데이트
 */
router.put('/:id', async (req, res) => {
    const result = await deviceModelService.updateModel(req.params.id, req.body);
    res.status(result.success ? 200 : 400).json(result);
});

/**
 * @route DELETE /api/models/:id
 * @desc 디바이스 모델 삭제
 */
router.delete('/:id', async (req, res) => {
    const result = await deviceModelService.deleteModel(req.params.id);
    res.status(result.success ? 200 : 400).json(result);
});

module.exports = router;
