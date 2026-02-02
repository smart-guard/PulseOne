const express = require('express');
const router = express.Router();
const DataService = require('../lib/services/DataService');

// ============================================================================
// 🛡️ 미들웨어 (테스트용)
// ============================================================================

const {
    authenticateToken,
    tenantIsolation
} = require('../middleware/tenantIsolation');

// 글로벌 미들웨어는 app.js에서 적용되지만, 개별 라우터에서도 명시적으로 사용 가능
router.use(authenticateToken);
router.use(tenantIsolation);

// 응답 헬퍼
function createResponse(success, data, message, errorCode) {
    return {
        success,
        data,
        message: message || (success ? 'Success' : 'Error'),
        error: errorCode,
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
        res.json(result);
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
        res.json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'DATA_POINT_DETAIL_ERROR'));
    }
});

// ============================================================================
// 🕐 실시간 데이터 조회 API
// ============================================================================

/**
 * GET /api/data/current-values
 * 현재값 일괄 조회 (device_ids 또는 point_ids 지원)
 */
router.get('/current-values', async (req, res) => {
    try {
        const { device_ids, point_ids, limit = 100 } = req.query;
        let resultData = [];

        if (point_ids) {
            // 특정 포인트들 조회 (Batch)
            const ids = point_ids.split(',').map(id => parseInt(id.trim()));
            const result = await DataService.getCurrentValuesByPointIds(ids, req.tenantId);
            if (result.success) {
                resultData = result.data;
            }
        } else if (device_ids) {
            // 특정 디바이스들의 모든 포인트 조회
            const ids = device_ids.split(',').map(id => parseInt(id.trim()));
            for (const id of ids) {
                const vals = await DataService.deviceRepo.getCurrentValuesByDevice(id, req.tenantId);
                resultData.push(...vals);
            }
        } else {
            // 모든 활성 데이터포인트 검색
            const searchResult = await DataService.searchPoints({ limit: 1000, enabled_only: true }, req.tenantId);
            if (searchResult.success && searchResult.data) {
                resultData = searchResult.data.items.map(dp => ({
                    point_id: dp.id,
                    name: dp.name,
                    value: dp.current_value
                }));
            }
        }

        res.json(createResponse(true, {
            current_values: resultData.slice(0, parseInt(limit)),
            total_count: resultData.length
        }));
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
        res.json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_CURRENT_VALUES_ERROR'));
    }
});

/**
 * POST /api/data/devices/status
 * 여러 디바이스의 상태 일괄 조회 (Bulk Status)
 */
router.post('/devices/status', async (req, res) => {
    try {
        const { device_ids } = req.body;
        if (!device_ids) {
            return res.status(400).json(createResponse(false, null, 'device_ids is required', 'VALIDATION_ERROR'));
        }

        const result = await DataService.getBulkDeviceStatus(device_ids, req.tenantId);
        res.json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'BULK_STATUS_ERROR'));
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
        const { point_ids, start_time, end_time, interval, aggregation } = req.query;
        if (!point_ids || !start_time || !end_time) {
            return res.status(400).json(createResponse(false, null, 'point_ids, start_time, and end_time are required', 'VALIDATION_ERROR'));
        }

        const result = await DataService.getHistoricalData(req.query, req.tenantId);
        res.json(result);
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
        res.json(result);
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
        let exportResult;

        if (export_type === 'current') {
            exportResult = await DataService.exportCurrentValues(point_ids, device_ids, req.tenantId);
        } else if (export_type === 'configuration') {
            exportResult = await DataService.exportConfiguration(point_ids, device_ids, req.tenantId);
        } else if (export_type === 'historical') {
            exportResult = await DataService.exportHistoricalData(req.body, req.tenantId);
        }

        if (exportResult && exportResult.success) {
            const exportData = exportResult.data || [];
            res.json(createResponse(true, {
                filename: `${export_type}_data_${Date.now()}.${format}`,
                total_records: exportData.length,
                data: exportData
            }));
        } else {
            res.status(500).json(exportResult || createResponse(false, null, 'Export failed', 'EXPORT_ERROR'));
        }
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'DATA_EXPORT_ERROR'));
    }
});

module.exports = router;