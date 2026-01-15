// =============================================================================
// backend/routes/groups.js
// 장치 그룹(Logical Groups) 관리 라우트
// =============================================================================

const express = require('express');
const router = express.Router();
const DeviceGroupService = require('../lib/services/DeviceGroupService');
const logger = require('../lib/utils/LogManager');

// 모든 그룹 조회 (트리 구조용)
router.get('/', async (req, res) => {
    try {
        // Service expects (tenantId, { tree: true }) for tree structure
        const result = await DeviceGroupService.getAllGroups(req.tenantId, { tree: true });
        // Result is already standardized { success: true, data: ... }
        if (!result.success) {
            return res.status(500).json(result);
        }
        res.json(result);
    } catch (error) {
        logger.api('ERROR', 'Failed to get group tree', { error: error.message });
        res.status(500).json({ success: false, error: error.message });
    }
});

// 그룹 상세 조회
router.get('/:id', async (req, res) => {
    try {
        const result = await DeviceGroupService.getGroupDetail(req.params.id, req.tenantId);
        if (!result.success) {
            return res.status(result.message.includes('not found') ? 404 : 500).json(result);
        }
        res.json(result);
    } catch (error) {
        res.status(error.message.includes('not found') ? 404 : 500).json({ success: false, error: error.message });
    }
});

// 그룹 생성
router.post('/', async (req, res) => {
    try {
        const result = await DeviceGroupService.createGroup(req.body, req.tenantId);
        if (!result.success) {
            return res.status(400).json(result);
        }
        res.status(201).json({ success: true, data: result.data });
    } catch (error) {
        res.status(400).json({ success: false, error: error.message });
    }
});

// 그룹 수정
router.put('/:id', async (req, res) => {
    try {
        const result = await DeviceGroupService.updateGroup(req.params.id, req.body, req.tenantId);
        if (!result.success) {
            return res.status(400).json(result);
        }
        res.json({ success: true, data: result.data });
    } catch (error) {
        res.status(400).json({ success: false, error: error.message });
    }
});

// 그룹 삭제
router.delete('/:id', async (req, res) => {
    try {
        const result = await DeviceGroupService.deleteGroup(req.params.id, req.tenantId);
        if (!result.success) {
            return res.status(400).json(result);
        }
        res.json({ success: true, data: result.data });
    } catch (error) {
        res.status(400).json({ success: false, error: error.message });
    }
});

// 그룹 내 장치 목록 조회
router.get('/:id/devices', async (req, res) => {
    try {
        const DeviceService = require('../lib/services/DeviceService');
        const devices = await DeviceService.getDevices({
            groupId: req.params.id,
            tenantId: req.tenantId
        });
        res.json({ success: true, data: devices });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

module.exports = router;
