const express = require('express');
const router = express.Router();
const DashboardService = require('../lib/services/DashboardService');
const CrossPlatformManager = require('../lib/services/crossPlatformManager');

// ============================================================================
// 미들웨어
// ============================================================================

const authenticateToken = (req, res, next) => {
    // 실제 환경에서는 인증 로직이 들어감
    req.user = { id: 1, tenant_id: 1, role: 'admin' };
    req.tenantId = 1;
    next();
};

const tenantIsolation = (req, res, next) => {
    if (!req.tenantId) req.tenantId = 1;
    next();
};

const validateTenantStatus = (req, res, next) => {
    next();
};

// 응답 헬퍼
function createResponse(success, data, message, error_code) {
    return {
        success,
        data,
        message: message || (success ? 'Success' : 'Error'),
        error_code: error_code,
        timestamp: new Date().toISOString()
    };
}

// =============================================================================
// 서비스 제어 API
// =============================================================================

/**
 * POST /api/dashboard/service/:serviceName/control
 * 서비스 시작/중지/재시작 제어
 */
router.post('/service/:serviceName/control', authenticateToken, async (req, res) => {
    try {
        const { serviceName } = req.params;
        const { action } = req.body;

        if (!['start', 'stop', 'restart'].includes(action)) {
            return res.status(400).json(createResponse(false, null,
                `Invalid action: ${action}. Must be start, stop, or restart`, 'INVALID_ACTION'));
        }

        const result = await DashboardService.controlService(serviceName, action);
        if (result.success) {
            res.json(result);
        } else {
            res.status(500).json(result);
        }
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'SERVICE_CONTROL_ERROR'));
    }
});

/**
 * GET /api/dashboard/services/status
 * 모든 서비스 상태 조회
 */
router.get('/services/status', authenticateToken, async (req, res) => {
    try {
        const result = await CrossPlatformManager.getServicesForAPI();
        if (result.success) {
            res.json(createResponse(true, {
                services: result.data,
                summary: result.summary
            }, 'Services status retrieved successfully'));
        } else {
            res.status(500).json(createResponse(false, null, 'Failed to get services status', 'STATUS_CHECK_ERROR'));
        }
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'STATUS_CHECK_ERROR'));
    }
});

// ============================================================================
// 대시보드 데이터 API
// ============================================================================

/**
 * GET /api/dashboard/overview
 * 전체 시스템 개요 데이터
 */
router.get('/overview',
    authenticateToken,
    tenantIsolation,
    validateTenantStatus,
    async (req, res) => {
        try {
            const result = await DashboardService.getOverviewData(req.tenantId);
            res.json(result);
        } catch (error) {
            res.status(500).json(createResponse(false, null, error.message, 'DASHBOARD_OVERVIEW_ERROR'));
        }
    });

/**
 * GET /api/dashboard/tenant-stats
 * 테넌트별 상세 통계
 */
router.get('/tenant-stats',
    authenticateToken,
    tenantIsolation,
    validateTenantStatus,
    async (req, res) => {
        try {
            const result = await DashboardService.getTenantStats(req.tenantId);
            res.json(result);
        } catch (error) {
            res.status(500).json(createResponse(false, null, error.message, 'TENANT_STATS_ERROR'));
        }
    });

/**
 * GET /api/dashboard/recent-devices
 * 최근 연결된 디바이스 목록
 */
router.get('/recent-devices',
    authenticateToken,
    tenantIsolation,
    async (req, res) => {
        try {
            const limit = parseInt(req.query.limit) || 10;
            const result = await DashboardService.handleRequest(async () => {
                return await DashboardService.deviceRepo.getRecentActiveDevices(req.tenantId, limit);
            }, 'DashboardRoute.getRecentDevices');
            res.json(result);
        } catch (error) {
            res.status(500).json(createResponse(false, null, error.message, 'RECENT_DEVICES_ERROR'));
        }
    });

/**
 * GET /api/dashboard/system-health
 * 시스템 헬스 상태
 */
router.get('/system-health',
    authenticateToken,
    async (req, res) => {
        try {
            const result = await DashboardService.getSystemHealth();
            res.json(result);
        } catch (error) {
            res.status(500).json(createResponse(false, null, error.message, 'SYSTEM_HEALTH_ERROR'));
        }
    });

/**
 * GET /api/dashboard/test
 * 대시보드 API 테스트 엔드포인트
 */
router.get('/test', (req, res) => {
    res.json(createResponse(true, {
        message: 'Complete Dashboard API is working!',
        data_source: 'DashboardService + CrossPlatformManager',
        endpoints: [
            'GET /api/dashboard/overview',
            'GET /api/dashboard/tenant-stats',
            'GET /api/dashboard/recent-devices',
            'GET /api/dashboard/system-health',
            'GET /api/dashboard/services/status',
            'POST /api/dashboard/service/{name}/control'
        ],
        timestamp: new Date().toISOString()
    }, 'Dashboard API test successful'));
});

module.exports = router;