// ============================================================================
// backend/routes/devices.js (Clean Version)
// ============================================================================

const express = require('express');
const router = express.Router();
const DeviceService = require('../lib/services/DeviceService');
const {
    authenticateToken,
    tenantIsolation,
    validateTenantStatus
} = require('../middleware/tenantIsolation');

// 글로벌 미들웨어 적용
router.use(authenticateToken);
router.use(tenantIsolation);
router.use(validateTenantStatus);

const DeviceGroupService = require('../lib/services/DeviceGroupService');

// 🔍 디바이스 목록 조회
router.get('/', async (req, res) => {
    try {
        const { tenantId } = req;
        let groupId = req.query.device_group_id;

        // 계층 구조 지원: 상위 그룹 선택 시 하위 그룹 장치도 포함
        if (groupId && groupId !== 'all') {
            const descendantRes = await DeviceGroupService.getDescendantIds(groupId, tenantId);
            if (descendantRes.success) {
                groupId = descendantRes.data;
            }
        }

        const options = {
            ...req.query,
            tenantId,
            page: parseInt(req.query.page) || 1,
            limit: parseInt(req.query.limit) || 25,
            groupId: groupId === 'all' ? undefined : groupId,
            includeRtuRelations: req.query.include_rtu_relations === 'true',
            includeCollectorStatus: req.query.include_collector_status === 'true'
        };

        const result = await DeviceService.getDevices(options);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message, error: 'DEVICES_FETCH_ERROR' });
    }
});

// 📝 디바이스 대량 업데이트 (그룹 이동 등)
router.put('/bulk', async (req, res) => {
    try {
        const { ids, data } = req.body;
        const { tenantId, user } = req;

        if (!ids || !Array.isArray(ids)) {
            return res.status(400).json({ success: false, message: 'Invalid device IDs' });
        }

        const result = await DeviceService.bulkUpdateDevices(ids, data, tenantId, user);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message, error: 'BULK_UPDATE_ERROR' });
    }
});

// 🗑️ 디바이스 대량 삭제
router.delete('/bulk', async (req, res) => {
    try {
        const { ids } = req.body;
        const { tenantId, user } = req;

        if (!ids || !Array.isArray(ids)) {
            return res.status(400).json({ success: false, message: 'Invalid device IDs' });
        }

        const result = await DeviceService.bulkDeleteDevices(ids, tenantId, user);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 🌳 디바이스 트리 구조 API
router.get('/tree-structure', async (req, res) => {
    try {
        const { tenantId, isSystemAdmin } = req;
        const result = await DeviceService.getDeviceTree({
            tenantId,
            isSystemAdmin,
            includeDataPoints: req.query.include_data_points === 'true',
            includeRealtime: req.query.include_realtime === 'true'
        });
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 📊 디바이스 통계 조회
router.get('/statistics', async (req, res) => {
    try {
        const { tenantId } = req;
        const result = await DeviceService.getDeviceStatistics(tenantId);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 📋 네트워크 스캔 결과 조회
router.get('/scan/results', async (req, res) => {
    try {
        const { tenantId } = req;
        const { since, protocol } = req.query;
        const result = await DeviceService.getScanResults({
            tenantId,
            since,
            protocol
        });
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 📡 네트워크 스캔 요청
router.post('/scan', async (req, res) => {
    try {
        const { tenantId } = req;
        const result = await DeviceService.scanNetwork({
            ...req.body,
            tenantId
        });
        res.status(result.data?.status === 'started' || result.data?.status === 'scan_started' ? 202 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 📋 프로토콜 목록 조회 (디바이스 생성/수정용)
router.get('/protocols', async (req, res) => {
    try {
        const result = await DeviceService.getAvailableProtocols();
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 🔍 디바이스 상세 조회
router.get('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const result = await DeviceService.getDeviceById(parseInt(id), tenantId);
        res.status(result.success ? 200 : (result.message === 'Device not found' ? 404 : 500)).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 🔍 디바이스 데이터 포인트 조회
router.get('/:id/data-points', async (req, res) => {
    try {
        const { id } = req.params;
        const result = await DeviceService.getDeviceDataPoints(parseInt(id), req.query);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// ➕ 디바이스 생성
router.post('/', async (req, res) => {
    try {
        const { tenantId, user } = req;
        const result = await DeviceService.createDevice(req.body, tenantId, user);
        res.status(result.success ? 201 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 📝 디바이스 업데이트
router.put('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId, user } = req;
        const result = await DeviceService.updateDevice(parseInt(id), req.body, tenantId, user);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 🗑️ 디바이스 삭제
router.delete('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId, user } = req;
        const result = await DeviceService.delete(parseInt(id), tenantId, user);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// ♻️ 디바이스 복구
router.post('/:id/restore', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId, user } = req;
        const result = await DeviceService.restore(parseInt(id), tenantId, user);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 🚀 Collector 제어 (Start/Stop/Restart)
router.post('/:id/:action(start|stop|restart)', async (req, res) => {
    try {
        const { id, action } = req.params;
        const { tenantId } = req;
        const result = await DeviceService.executeAction(id, action, req.body, tenantId);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// ⚡ 디지털 출력 제어 (DO)
router.post('/:id/digital/:outputId/control', async (req, res) => {
    try {
        const { id, outputId } = req.params;
        const { state, options } = req.body;
        const { tenantId } = req;
        const result = await DeviceService.controlDigitalOutput(id, outputId, state, options, tenantId);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// ⚡ 아날로그 출력 제어 (AO)
router.post('/:id/analog/:outputId/control', async (req, res) => {
    try {
        const { id, outputId } = req.params;
        const { value, options } = req.body;
        const { tenantId } = req;
        const result = await DeviceService.controlAnalogOutput(id, outputId, value, options, tenantId);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 🩺 연결 진단
router.post('/:id/diagnose', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const result = await DeviceService.diagnoseConnection(parseInt(id), tenantId);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * POST /api/devices/:id/test-connection
 * 디바이스 연결 테스트 (HMI-001 대응)
 */
router.post('/:id/test-connection', async (req, res) => {
    try {
        const { id } = req.params;
        const result = await DeviceService.diagnoseConnection(id, req.tenantId);
        res.json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message, error: 'TEST_CONNECTION_ERROR' });
    }
});

module.exports = router;