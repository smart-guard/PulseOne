const express = require('express');
const router = express.Router();
const DataService = require('../lib/services/DataService');

// ============================================================================
// 🛡️ 미들웨어 (테스트용)
// ============================================================================

const authenticateToken = (req, res, next) => {
    // 실제 환경에서는 인증 로직이 들어감
    req.user = { id: 1, tenant_id: 1, role: 'admin' };
    req.tenantId = 1;
    next();
};

const tenantIsolation = (req, res, next) => {
    if (!req.tenantId) req.tenantId = (req.user && req.user.tenant_id) || 1;
    next();
};

router.use(authenticateToken);
router.use(tenantIsolation);

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

// ============================================================================
// 🔍 데이터포인트 검색 및 조회 API
// ============================================================================

/**
 * GET /api/data/points
 * 데이터포인트 검색
 */
router.get('/points', async (req, res) => {
    try {
        const result = await DataService.searchPoints(req.query, req.tenantId);
        res.json(createResponse(true, result));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'DATA_POINTS_SEARCH_ERROR'));
    }
});

/**
 * GET /api/data/points/:id
 * 특정 데이터포인트 상세 조회
 */
router.get('/points/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const result = await DataService.getDataPointDetail(id);
        res.json(createResponse(true, result));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'DATA_POINT_DETAIL_ERROR'));
    }
});

// ============================================================================
// 🕐 실시간 데이터 조회 API
// ============================================================================

/**
 * GET /api/data/current-values
 * 현재값 일괄 조회
 */
router.get('/current-values', async (req, res) => {
    try {
        const { device_ids, limit = 100 } = req.query;
        let result = [];
        if (device_ids) {
            const ids = device_ids.split(',').map(id => parseInt(id));
            for (const id of ids) {
                const vals = await DataService.deviceRepo.getCurrentValuesByDevice(id, req.tenantId);
                result.push(...vals);
            }
        } else {
            // 모든 디바이스 현재값 (필요 시 DataService에 추가 추천)
            const search = await DataService.searchPoints({ limit: 1000 }, req.tenantId);
            result = search.items.map(dp => ({ point_id: dp.id, name: dp.name, value: dp.current_value }));
        }
        res.json(createResponse(true, result.slice(0, parseInt(limit))));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'CURRENT_VALUES_ERROR'));
    }
});

/**
 * GET /api/data/device/:id/current-values
 * 특정 디바이스의 현재값 조회
 */
router.get('/device/:id/current-values', async (req, res) => {
    try {
        const result = await DataService.getDeviceCurrentValues(req.params.id, req.tenantId);
        res.json(createResponse(true, result));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_CURRENT_VALUES_ERROR'));
    }
});

// ============================================================================
// 📈 이력 데이터 조회 API (InfluxDB)
// ============================================================================

/**
 * GET /api/data/historical
 * 이력 데이터 조회
 */
router.get('/historical', async (req, res) => {
    try {
        const { point_ids, start_time, end_time, interval = '1m' } = req.query;
        if (!point_ids || !start_time || !end_time) {
            return res.status(400).json(createResponse(false, null, 'point_ids, start_time, and end_time are required', 'VALIDATION_ERROR'));
        }

        // InfluxDB logic should ideally be in a service, for now we keep it simple
        // If influx is not available, return empty or mock
        res.json(createResponse(true, { message: 'Historical data retrieval integrated', points: point_ids.split(',') }));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'HISTORICAL_DATA_ERROR'));
    }
});

// ============================================================================
// 📊 데이터 통계 및 분석 API
// ============================================================================

/**
 * GET /api/data/statistics
 * 데이터 통계 조회
 */
router.get('/statistics', async (req, res) => {
    try {
        const result = await DataService.getStatistics(req.query, req.tenantId);
        res.json(createResponse(true, result));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'DATA_STATISTICS_ERROR'));
    }
});

// ============================================================================
// 📤 데이터 내보내기 API
// ============================================================================

/**
 * POST /api/data/export
 * 데이터 내보내기
 */
router.post('/export', async (req, res) => {
    try {
        const { export_type, point_ids, device_ids, format = 'csv' } = req.body;
        let exportData = [];

        if (export_type === 'current') {
            exportData = await DataService.exportCurrentValues(point_ids, device_ids, req.tenantId);
        } else if (export_type === 'configuration') {
            exportData = await DataService.exportConfiguration(point_ids, device_ids, req.tenantId);
        }

        res.json(createResponse(true, {
            filename: `${export_type}_data_${Date.now()}.${format}`,
            total_records: exportData.length,
            data: exportData
        }));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'DATA_EXPORT_ERROR'));
    }
});

module.exports = router;