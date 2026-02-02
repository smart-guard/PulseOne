const express = require('express');
const router = express.Router();
const ProtocolService = require('../lib/services/ProtocolService');
const {
    authenticateToken,
    tenantIsolation,
    validateTenantStatus
} = require('../middleware/tenantIsolation');

// 글로벌 미들웨어 적용
router.use(authenticateToken);
router.use(tenantIsolation);
router.use(validateTenantStatus);

// ============================================================================
// 정적 라우트
// ============================================================================

/**
 * GET /api/protocols/broker/status
 * MQTT 브로커 상태 조회
 */
router.get('/broker/status', async (req, res) => {
    try {
        const result = await ProtocolService.getBrokerStatus();
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * GET /api/protocols/statistics
 * 프로토콜 사용 통계 조회
 */
router.get('/statistics', async (req, res) => {
    try {
        const { tenantId } = req;
        const result = await ProtocolService.getProtocolStatistics(tenantId);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * GET /api/protocols/category/:category
 * 카테고리별 프로토콜 목록 조회
 */
router.get('/category/:category', async (req, res) => {
    try {
        const { category } = req.params;
        const result = await ProtocolService.getProtocolsByCategory(category);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// ============================================================================
// 📋 프로토콜 목록 조회 API
// ============================================================================

/**
 * GET /api/protocols
 * 프로토콜 목록 조회 (필터링 및 페이징 지원)
 */
router.get('/', async (req, res) => {
    try {
        const { tenantId } = req;
        const page = parseInt(req.query.page) || 1;
        const limit = parseInt(req.query.limit) || 25;
        const offset = (page - 1) * limit;

        const filters = {
            ...req.query,
            tenantId,
            limit,
            offset
        };

        const result = await ProtocolService.getProtocols(filters);

        // 페이징 정보 추가 (Service 결과에 meta 추가 가능하지만 일단 그대로 반환)
        if (result.success && result.data) {
            const totalCount = result.data.total_count;
            result.pagination = {
                total_count: totalCount,
                current_page: page,
                page_size: limit,
                total_pages: Math.ceil(totalCount / limit),
                has_next: (page * limit) < totalCount,
                has_prev: page > 1
            };
        }

        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * POST /api/protocols
 * 새 프로토콜 등록
 */
router.post('/', async (req, res) => {
    try {
        const userId = req.user ? req.user.id : null;
        const result = await ProtocolService.createProtocol(req.body, userId);
        res.status(result.success ? 201 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// ============================================================================
// 동적 라우트 (/:id)
// ============================================================================

/**
 * GET /api/protocols/:id
 * 프로토콜 상세 조회
 */
router.get('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const result = await ProtocolService.getProtocolById(parseInt(id), tenantId);
        res.status(result.success ? 200 : (result.message === 'Protocol not found' ? 404 : 500)).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * PUT /api/protocols/:id
 * 프로토콜 정보 수정
 */
router.put('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const result = await ProtocolService.updateProtocol(parseInt(id), req.body);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * DELETE /api/protocols/:id
 * 프로토콜 삭제
 */
router.delete('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const force = req.query.force === 'true';
        const result = await ProtocolService.deleteProtocol(parseInt(id), force);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// ============================================================================
// 🔄 프로토콜 제어
// ============================================================================

/**
 * POST /api/protocols/:id/enable
 * 프로토콜 활성화
 */
router.post('/:id/enable', async (req, res) => {
    try {
        const { id } = req.params;
        const result = await ProtocolService.setProtocolStatus(parseInt(id), true);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * POST /api/protocols/:id/disable
 * 프로토콜 비활성화
 */
router.post('/:id/disable', async (req, res) => {
    try {
        const { id } = req.params;
        const result = await ProtocolService.setProtocolStatus(parseInt(id), false);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * POST /api/protocols/:id/test
 * 프로토콜 연결 테스트 (시뮬레이션)
 */
router.post('/:id/test', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const result = await ProtocolService.testConnection(parseInt(id), req.body, tenantId);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * GET /api/protocols/:id/devices
 * 특정 프로토콜을 사용하는 디바이스 목록 조회
 */
router.get('/:id/devices', async (req, res) => {
    try {
        const { id } = req.params;
        const limit = parseInt(req.query.limit) || 50;
        const offset = parseInt(req.query.offset) || 0;
        const result = await ProtocolService.getDevicesByProtocol(parseInt(id), limit, offset);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});


// ============================================================================
// Protocol Instances
// ============================================================================

/**
 * GET /api/protocols/:id/instances
 * 특정 프로토콜의 인스턴스 목록 조회
 */
/**
 * GET /api/protocols/:id/instances
 * 특정 프로토콜의 인스턴스 목록 조회
 */
router.get('/:id/instances', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req; // From middleware
        const page = parseInt(req.query.page) || 1;
        const limit = parseInt(req.query.limit) || 20;

        // tenantId가 미들웨어에 의해 설정되면 필터링됨. System Admin의 경우 tenantId가 null일 수 있음(로직에 따라).
        const result = await ProtocolService.getInstancesByProtocolId(parseInt(id), tenantId, page, limit);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * POST /api/protocols/:id/instances
 * 새 인스턴스 생성
 */
router.post('/:id/instances', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId, user } = req;

        let targetTenantId = tenantId;

        // System Admin이라면 body에서 tenant_id 지정 가능
        if (user && user.role === 'system_admin' && req.body.tenant_id) {
            targetTenantId = req.body.tenant_id;
        }

        const result = await ProtocolService.createInstance({
            ...req.body,
            protocol_id: parseInt(id),
            tenant_id: targetTenantId // Service uses this for DB Insert
        });
        res.status(result.success ? 201 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * PUT /api/protocols/instances/:instanceId
 * 인스턴스 정보 수정
 */
router.put('/instances/:instanceId', async (req, res) => {
    try {
        const { instanceId } = req.params;
        const result = await ProtocolService.updateInstance(parseInt(instanceId), req.body);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * DELETE /api/protocols/instances/:instanceId
 * 인스턴스 삭제
 */
router.delete('/instances/:instanceId', async (req, res) => {
    try {
        const { instanceId } = req.params;
        const result = await ProtocolService.deleteInstance(parseInt(instanceId));
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

module.exports = router;
