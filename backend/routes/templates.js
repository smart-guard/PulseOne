const express = require('express');
const router = express.Router();
const TemplateDeviceService = require('../lib/services/TemplateDeviceService');

const templateService = new TemplateDeviceService();

/**
 * @route GET /api/templates
 * @desc 모든 디바이스 템플릿 목록 조회
 */
router.get('/', async (req, res) => {
    const options = {
        page: req.query.page,
        limit: req.query.limit,
        search: req.query.search,
        modelId: req.query.modelId,
        protocolId: req.query.protocolId,
        manufacturerId: req.query.manufacturerId,
        usageStatus: req.query.usageStatus,
        isPublic: req.query.isPublic === 'true' ? true : (req.query.isPublic === 'false' ? false : undefined),
        sort_by: req.query.sort_by,
        sort_order: req.query.sort_order
    };

    const result = await templateService.getAllTemplates(options);
    res.status(result.success ? 200 : 500).json(result);
});

/**
 * @route GET /api/templates/:id
 * @desc 특정 템플릿 상세 조회 (데이터 포인트 포함)
 */
router.get('/:id', async (req, res) => {
    const result = await templateService.getTemplateDetail(req.params.id);
    res.status(result.success ? 200 : (result.message.includes('찾을 수 없습니다') ? 404 : 500)).json(result);
});

/**
 * @route POST /api/templates
 * @desc 새로운 템플릿 생성
 */
router.post('/', async (req, res) => {
    const result = await templateService.createTemplate(req.body);
    res.status(result.success ? 201 : 400).json(result);
});

/**
 * @route POST /api/templates/:id/instantiate
 * @desc 템플릿으로 디바이스 생성
 */
router.post('/:id/instantiate', async (req, res) => {
    const result = await templateService.instantiateFromTemplate(
        req.params.id,
        req.body,
        req.tenantId
    );
    res.status(result.success ? 201 : 400).json(result);
});

/**
 * @route POST /api/templates/:id/points
 * @desc 템플릿에 데이터 포인트 추가
 */
router.post('/:id/points', async (req, res) => {
    const result = await templateService.addDataPoint(req.params.id, req.body);
    res.status(result.success ? 201 : 400).json(result);
});

/**
 * @route PUT /api/templates/:id
 * @desc 템플릿 정보 업데이트
 */
router.put('/:id', async (req, res) => {
    const result = await templateService.updateTemplate(req.params.id, req.body);
    res.status(result.success ? 200 : 400).json(result);
});

/**
 * @route DELETE /api/templates/:id
 * @desc 템플릿 삭제
 */
router.delete('/:id', async (req, res) => {
    const result = await templateService.deleteTemplate(req.params.id);
    res.status(result.success ? 200 : 400).json(result);
});

module.exports = router;
