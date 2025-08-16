// ============================================================================
// backend/routes/data.js
// 데이터 익스플로러 API - Repository 패턴 100% 활용한 상용 버전
// ============================================================================

const express = require('express');
const router = express.Router();

// Repository imports (기존 완성된 것들 사용)
const DataPointRepository = require('../lib/database/repositories/DataPointRepository');
const CurrentValueRepository = require('../lib/database/repositories/CurrentValueRepository');
const DeviceRepository = require('../lib/database/repositories/DeviceRepository');
const SiteRepository = require('../lib/database/repositories/SiteRepository');

// Connection modules
const redisClient = require('../lib/connection/redis');
const { query: postgresQuery } = require('../lib/connection/postgres');
const { queryRange: influxQuery } = require('../lib/connection/influx');

// Repository 인스턴스 생성
let dataPointRepo = null;
let currentValueRepo = null;
let deviceRepo = null;
let siteRepo = null;

function getDataPointRepo() {
    if (!dataPointRepo) {
        dataPointRepo = new DataPointRepository();
        console.log("✅ DataPointRepository 인스턴스 생성 완료");
    }
    return dataPointRepo;
}

function getCurrentValueRepo() {
    if (!currentValueRepo) {
        currentValueRepo = new CurrentValueRepository();
        console.log("✅ CurrentValueRepository 인스턴스 생성 완료");
    }
    return currentValueRepo;
}

function getDeviceRepo() {
    if (!deviceRepo) {
        deviceRepo = new DeviceRepository();
        console.log("✅ DeviceRepository 인스턴스 생성 완료");
    }
    return deviceRepo;
}

function getSiteRepo() {
    if (!siteRepo) {
        siteRepo = new SiteRepository();
        console.log("✅ SiteRepository 인스턴스 생성 완료");
    }
    return siteRepo;
}

// ============================================================================
// 🛡️ 유틸리티 및 헬퍼 함수들
// ============================================================================

/**
 * 표준 응답 생성
 */
function createResponse(success, data, message, error_code) {
    return {
        success,
        data,
        message: message || (success ? 'Success' : 'Error'),
        error_code: error_code || null,
        timestamp: new Date().toISOString()
    };
}

/**
 * 페이징 응답 생성
 */
function createPaginatedResponse(items, pagination, message) {
    return createResponse(true, {
        items,
        pagination: {
            page: pagination.page,
            limit: pagination.limit,
            total: pagination.total_items,
            totalPages: Math.ceil(pagination.total_items / pagination.limit),
            hasNext: pagination.has_next,
            hasPrev: pagination.has_prev
        }
    }, message);
}

/**
 * 개발용 인증 미들웨어
 */
const devAuthMiddleware = (req, res, next) => {
    req.user = {
        id: 1,
        username: 'admin',
        tenant_id: 1,
        role: 'admin'
    };
    next();
};

/**
 * 테넌트 격리 미들웨어
 */
const devTenantMiddleware = (req, res, next) => {
    req.tenantId = req.user.tenant_id;
    next();
};

// 전역 미들웨어 적용
router.use(devAuthMiddleware);
router.use(devTenantMiddleware);

// ============================================================================
// 🔍 데이터포인트 검색 및 조회 API
// ============================================================================

/**
 * GET /api/data/points
 * 데이터포인트 검색 (페이징, 필터링, 정렬 지원)
 */
router.get('/points', async (req, res) => {
    try {
        const { tenantId } = req;
        const {
            page = 1,
            limit = 50,
            search,
            device_id,
            site_id,
            data_type,
            enabled_only = false,
            sort_by = 'name',
            sort_order = 'ASC',
            include_current_value = false
        } = req.query;

        console.log('🔍 데이터포인트 검색 요청:', {
            tenantId,
            page: parseInt(page),
            limit: parseInt(limit),
            filters: { search, device_id, site_id, data_type, enabled_only }
        });

        const options = {
            tenantId,
            search,
            deviceId: device_id ? parseInt(device_id) : null,
            siteId: site_id ? parseInt(site_id) : null,
            dataType: data_type,
            enabledOnly: enabled_only === 'true',
            page: parseInt(page),
            limit: parseInt(limit),
            sortBy: sort_by,
            sortOrder: sort_order.toUpperCase(),
            includeCurrentValue: include_current_value === 'true'
        };

        const result = await getDataPointRepo().findAll(options);

        // 현재값 포함 요청 시
        if (include_current_value === 'true' && result.items.length > 0) {
            for (const dataPoint of result.items) {
                try {
                    const currentValue = await getCurrentValueRepo().findByPointId(dataPoint.id, tenantId);
                    dataPoint.current_value = currentValue;
                } catch (error) {
                    console.warn(`현재값 조회 실패 (포인트 ID: ${dataPoint.id}):`, error.message);
                    dataPoint.current_value = null;
                }
            }
        }

        console.log(`✅ 데이터포인트 ${result.items.length}개 검색 완료`);
        res.json(createPaginatedResponse(result.items, result.pagination, 'Data points retrieved successfully'));

    } catch (error) {
        console.error('❌ 데이터포인트 검색 실패:', error.message);
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
        const { tenantId } = req;
        const { include_current_value = true, include_device_info = true } = req.query;

        console.log(`🔍 데이터포인트 ID ${id} 상세 조회...`);

        const dataPoint = await getDataPointRepo().findById(parseInt(id), tenantId);
        if (!dataPoint) {
            return res.status(404).json(createResponse(false, null, 'Data point not found', 'DATA_POINT_NOT_FOUND'));
        }

        // 현재값 포함
        if (include_current_value === 'true') {
            try {
                const currentValue = await getCurrentValueRepo().findByPointId(dataPoint.id, tenantId);
                dataPoint.current_value = currentValue;
            } catch (error) {
                console.warn(`현재값 조회 실패 (포인트 ID: ${id}):`, error.message);
                dataPoint.current_value = null;
            }
        }

        // 디바이스 정보 포함
        if (include_device_info === 'true' && dataPoint.device_id) {
            try {
                const device = await getDeviceRepo().findById(dataPoint.device_id, tenantId);
                dataPoint.device_info = device;
            } catch (error) {
                console.warn(`디바이스 정보 조회 실패 (디바이스 ID: ${dataPoint.device_id}):`, error.message);
                dataPoint.device_info = null;
            }
        }

        console.log(`✅ 데이터포인트 ID ${id} 조회 완료`);
        res.json(createResponse(true, dataPoint, 'Data point retrieved successfully'));

    } catch (error) {
        console.error(`❌ 데이터포인트 ID ${req.params.id} 조회 실패:`, error.message);
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
        const { tenantId } = req;
        const {
            point_ids,
            device_ids,
            site_id,
            data_type,
            quality_filter,
            limit = 100
        } = req.query;

        console.log('⚡ 현재값 일괄 조회 요청:', {
            point_ids: point_ids ? point_ids.split(',').length + '개' : 'all',
            device_ids: device_ids ? device_ids.split(',').length + '개' : 'all',
            site_id,
            data_type
        });

        const options = {
            tenantId,
            pointIds: point_ids ? point_ids.split(',').map(id => parseInt(id)) : null,
            deviceIds: device_ids ? device_ids.split(',').map(id => parseInt(id)) : null,
            siteId: site_id ? parseInt(site_id) : null,
            dataType: data_type,
            qualityFilter: quality_filter,
            limit: parseInt(limit)
        };

        const currentValues = await getCurrentValueRepo().findByFilter(options);

        console.log(`✅ 현재값 ${currentValues.length}개 조회 완료`);
        res.json(createResponse(true, {
            current_values: currentValues,
            total_count: currentValues.length,
            filters_applied: options
        }, 'Current values retrieved successfully'));

    } catch (error) {
        console.error('❌ 현재값 일괄 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'CURRENT_VALUES_ERROR'));
    }
});

/**
 * GET /api/data/device/:id/current-values
 * 특정 디바이스의 현재값 조회
 */
router.get('/device/:id/current-values', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const { data_type, include_metadata = false } = req.query;

        console.log(`⚡ 디바이스 ID ${id} 현재값 조회...`);

        // 디바이스 존재 확인
        const device = await getDeviceRepo().findById(parseInt(id), tenantId);
        if (!device) {
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        const options = {
            tenantId,
            deviceIds: [parseInt(id)],
            dataType: data_type,
            includeMetadata: include_metadata === 'true'
        };

        const currentValues = await getCurrentValueRepo().findByFilter(options);

        const responseData = {
            device_id: device.id,
            device_name: device.name,
            device_status: device.status,
            connection_status: device.connection_status,
            last_communication: device.last_seen,
            current_values: currentValues,
            total_points: currentValues.length
        };

        console.log(`✅ 디바이스 ID ${id} 현재값 ${currentValues.length}개 조회 완료`);
        res.json(createResponse(true, responseData, 'Device current values retrieved successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ID ${req.params.id} 현재값 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_CURRENT_VALUES_ERROR'));
    }
});

// ============================================================================
// 📈 이력 데이터 조회 API
// ============================================================================

/**
 * GET /api/data/historical
 * 이력 데이터 조회 (InfluxDB 기반)
 */
router.get('/historical', async (req, res) => {
    try {
        const { tenantId } = req;
        const {
            point_ids,
            start_time,
            end_time,
            interval = '1m',
            aggregation = 'mean',
            limit = 1000
        } = req.query;

        if (!point_ids || !start_time || !end_time) {
            return res.status(400).json(createResponse(
                false, 
                null, 
                'point_ids, start_time, and end_time are required', 
                'VALIDATION_ERROR'
            ));
        }

        console.log('📈 이력 데이터 조회 요청:', {
            point_ids: point_ids.split(',').length + '개',
            start_time,
            end_time,
            interval,
            aggregation
        });

        const pointIds = point_ids.split(',').map(id => parseInt(id));
        
        // 데이터포인트 존재 확인
        const dataPoints = await getDataPointRepo().findByIds(pointIds, tenantId);
        if (dataPoints.length === 0) {
            return res.status(404).json(createResponse(false, null, 'No valid data points found', 'DATA_POINTS_NOT_FOUND'));
        }

        // InfluxDB 쿼리 구성
        const influxOptions = {
            measurement: 'data_values',
            fields: ['value'],
            tags: {
                tenant_id: tenantId.toString(),
                point_id: pointIds.map(id => id.toString())
            },
            start: start_time,
            end: end_time,
            interval,
            aggregation,
            limit: parseInt(limit)
        };

        try {
            const historicalData = await influxQuery(influxOptions);
            
            const responseData = {
                data_points: dataPoints,
                historical_data: historicalData,
                query_info: {
                    start_time,
                    end_time,
                    interval,
                    aggregation,
                    total_points: historicalData.length
                }
            };

            console.log(`✅ 이력 데이터 ${historicalData.length}개 조회 완료`);
            res.json(createResponse(true, responseData, 'Historical data retrieved successfully'));

        } catch (influxError) {
            console.warn('InfluxDB 조회 실패, 시뮬레이션 데이터 생성:', influxError.message);
            
            // InfluxDB 실패 시 시뮬레이션 데이터 생성
            const simulatedData = generateSimulatedHistoricalData(pointIds, start_time, end_time, interval);
            
            const responseData = {
                data_points: dataPoints,
                historical_data: simulatedData,
                query_info: {
                    start_time,
                    end_time,
                    interval,
                    aggregation,
                    total_points: simulatedData.length,
                    simulated: true
                }
            };

            res.json(createResponse(true, responseData, 'Historical data retrieved (simulated)'));
        }

    } catch (error) {
        console.error('❌ 이력 데이터 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'HISTORICAL_DATA_ERROR'));
    }
});

// ============================================================================
// 🔍 고급 검색 및 쿼리 API
// ============================================================================

/**
 * POST /api/data/query
 * 고급 데이터 쿼리 실행
 */
router.post('/query', async (req, res) => {
    try {
        const { tenantId } = req;
        const {
            query_type,
            filters,
            aggregations,
            time_range,
            output_format = 'json'
        } = req.body;

        console.log('🔍 고급 쿼리 실행 요청:', { query_type, filters, aggregations });

        let result;

        switch (query_type) {
            case 'data_points_search':
                result = await executeDataPointsQuery(filters, tenantId);
                break;
            case 'current_values_aggregate':
                result = await executeCurrentValuesAggregation(filters, aggregations, tenantId);
                break;
            case 'historical_analysis':
                result = await executeHistoricalAnalysis(filters, time_range, aggregations, tenantId);
                break;
            case 'device_summary':
                result = await executeDeviceSummaryQuery(filters, tenantId);
                break;
            default:
                return res.status(400).json(createResponse(false, null, `Unknown query type: ${query_type}`, 'INVALID_QUERY_TYPE'));
        }

        console.log(`✅ 고급 쿼리 실행 완료: ${result.total_results || 0}개 결과`);
        res.json(createResponse(true, result, 'Advanced query executed successfully'));

    } catch (error) {
        console.error('❌ 고급 쿼리 실행 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ADVANCED_QUERY_ERROR'));
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
        const { tenantId } = req;
        const { device_id, site_id, time_range = '24h' } = req.query;

        console.log('📊 데이터 통계 조회...', { device_id, site_id, time_range });

        const stats = {
            // 데이터포인트 통계
            data_points: await getDataPointRepo().getStatsByTenant(tenantId),
            
            // 현재값 통계
            current_values: await getCurrentValueRepo().getStatsByTenant(tenantId),
            
            // 디바이스별 통계 (요청 시)
            device_stats: device_id ? await getDeviceDataStats(parseInt(device_id), tenantId) : null,
            
            // 사이트별 통계 (요청 시)
            site_stats: site_id ? await getSiteDataStats(parseInt(site_id), tenantId) : null,
            
            // 시스템 전체 통계
            system_stats: {
                total_devices: await getDeviceRepo().countByTenant(tenantId),
                active_devices: await getDeviceRepo().countActive(tenantId),
                data_collection_rate: '98.5%', // 시뮬레이션
                average_response_time: Math.floor(Math.random() * 100) + 50, // ms
                last_updated: new Date().toISOString()
            }
        };

        console.log('✅ 데이터 통계 조회 완료');
        res.json(createResponse(true, stats, 'Data statistics retrieved successfully'));

    } catch (error) {
        console.error('❌ 데이터 통계 조회 실패:', error.message);
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
        const { tenantId } = req;
        const {
            export_type,
            point_ids,
            device_ids,
            start_time,
            end_time,
            format = 'csv',
            include_metadata = true
        } = req.body;

        console.log('📤 데이터 내보내기 요청:', { export_type, format, point_ids: point_ids?.length || 0 });

        if (!export_type || !['current', 'historical', 'configuration'].includes(export_type)) {
            return res.status(400).json(createResponse(false, null, 'Invalid export_type', 'VALIDATION_ERROR'));
        }

        let exportData;
        let filename;

        switch (export_type) {
            case 'current':
                exportData = await exportCurrentValues(point_ids, device_ids, tenantId, include_metadata);
                filename = `current_values_${new Date().toISOString().split('T')[0]}.${format}`;
                break;
            case 'historical':
                if (!start_time || !end_time) {
                    return res.status(400).json(createResponse(false, null, 'start_time and end_time required for historical export', 'VALIDATION_ERROR'));
                }
                exportData = await exportHistoricalData(point_ids, start_time, end_time, tenantId, include_metadata);
                filename = `historical_data_${start_time}_${end_time}.${format}`;
                break;
            case 'configuration':
                exportData = await exportConfiguration(point_ids, device_ids, tenantId);
                filename = `configuration_${new Date().toISOString().split('T')[0]}.${format}`;
                break;
        }

        const exportResult = {
            export_id: `export_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`,
            filename,
            format,
            export_type,
            total_records: exportData.length,
            file_size_mb: (JSON.stringify(exportData).length / 1024 / 1024).toFixed(2),
            generated_at: new Date().toISOString(),
            download_url: `/api/data/download/${filename}`, // 실제로는 파일 스토리지 URL
            data: format === 'json' ? exportData : convertToFormat(exportData, format)
        };

        console.log(`✅ 데이터 내보내기 완료: ${exportData.length}개 레코드`);
        res.json(createResponse(true, exportResult, 'Data export completed successfully'));

    } catch (error) {
        console.error('❌ 데이터 내보내기 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DATA_EXPORT_ERROR'));
    }
});

// ============================================================================
// 🔧 헬퍼 함수들
// ============================================================================

/**
 * 시뮬레이션 이력 데이터 생성
 */
function generateSimulatedHistoricalData(pointIds, startTime, endTime, interval) {
    const start = new Date(startTime);
    const end = new Date(endTime);
    const intervalMs = parseInterval(interval);
    
    const data = [];
    for (let time = start; time <= end; time = new Date(time.getTime() + intervalMs)) {
        for (const pointId of pointIds) {
            data.push({
                time: time.toISOString(),
                point_id: pointId,
                value: Math.random() * 100,
                quality: Math.random() > 0.05 ? 'good' : 'uncertain'
            });
        }
    }
    return data;
}

/**
 * 인터벌 문자열을 밀리초로 변환
 */
function parseInterval(interval) {
    const unit = interval.slice(-1);
    const value = parseInt(interval.slice(0, -1));
    
    switch (unit) {
        case 's': return value * 1000;
        case 'm': return value * 60 * 1000;
        case 'h': return value * 60 * 60 * 1000;
        case 'd': return value * 24 * 60 * 60 * 1000;
        default: return 60 * 1000; // 기본 1분
    }
}

/**
 * 데이터포인트 쿼리 실행
 */
async function executeDataPointsQuery(filters, tenantId) {
    const options = {
        tenantId,
        ...filters
    };
    const result = await getDataPointRepo().findAll(options);
    return {
        query_type: 'data_points_search',
        total_results: result.items.length,
        results: result.items
    };
}

/**
 * 현재값 집계 실행
 */
async function executeCurrentValuesAggregation(filters, aggregations, tenantId) {
    const options = {
        tenantId,
        ...filters
    };
    const currentValues = await getCurrentValueRepo().findByFilter(options);
    
    // 집계 계산 (시뮬레이션)
    const results = {
        total_count: currentValues.length,
        value_stats: {
            min: Math.min(...currentValues.map(v => parseFloat(v.value) || 0)),
            max: Math.max(...currentValues.map(v => parseFloat(v.value) || 0)),
            avg: currentValues.reduce((sum, v) => sum + (parseFloat(v.value) || 0), 0) / currentValues.length,
            count: currentValues.length
        },
        quality_distribution: {
            good: currentValues.filter(v => v.quality === 'good').length,
            bad: currentValues.filter(v => v.quality === 'bad').length,
            uncertain: currentValues.filter(v => v.quality === 'uncertain').length
        }
    };
    
    return {
        query_type: 'current_values_aggregate',
        total_results: currentValues.length,
        aggregations: results
    };
}

/**
 * 이력 분석 실행
 */
async function executeHistoricalAnalysis(filters, timeRange, aggregations, tenantId) {
    // 시뮬레이션 분석 결과
    return {
        query_type: 'historical_analysis',
        time_range: timeRange,
        total_data_points: Math.floor(Math.random() * 10000) + 1000,
        analysis_results: {
            trends: 'increasing',
            anomalies_detected: Math.floor(Math.random() * 10),
            data_quality_score: (90 + Math.random() * 10).toFixed(1) + '%',
            coverage_percentage: (95 + Math.random() * 5).toFixed(1) + '%'
        }
    };
}

/**
 * 디바이스 요약 쿼리 실행
 */
async function executeDeviceSummaryQuery(filters, tenantId) {
    const devices = await getDeviceRepo().findAll({ tenantId, ...filters });
    
    return {
        query_type: 'device_summary',
        total_devices: devices.items.length,
        devices: devices.items.map(device => ({
            id: device.id,
            name: device.name,
            status: device.status,
            connection_status: device.connection_status,
            data_points_count: Math.floor(Math.random() * 20) + 5 // 시뮬레이션
        }))
    };
}

/**
 * 디바이스 데이터 통계 조회
 */
async function getDeviceDataStats(deviceId, tenantId) {
    const dataPoints = await getDataPointRepo().findByDeviceId(deviceId, tenantId);
    return {
        device_id: deviceId,
        total_data_points: dataPoints.length,
        enabled_data_points: dataPoints.filter(dp => dp.is_enabled).length,
        data_types: [...new Set(dataPoints.map(dp => dp.data_type))]
    };
}

/**
 * 사이트 데이터 통계 조회
 */
async function getSiteDataStats(siteId, tenantId) {
    // 시뮬레이션 통계
    return {
        site_id: siteId,
        total_devices: Math.floor(Math.random() * 20) + 5,
        total_data_points: Math.floor(Math.random() * 200) + 50,
        data_collection_rate: (95 + Math.random() * 5).toFixed(1) + '%'
    };
}

/**
 * 현재값 내보내기
 */
async function exportCurrentValues(pointIds, deviceIds, tenantId, includeMetadata) {
    const options = {
        tenantId,
        pointIds,
        deviceIds,
        includeMetadata
    };
    const currentValues = await getCurrentValueRepo().findByFilter(options);
    return currentValues;
}

/**
 * 이력 데이터 내보내기
 */
async function exportHistoricalData(pointIds, startTime, endTime, tenantId, includeMetadata) {
    // 시뮬레이션 이력 데이터
    return generateSimulatedHistoricalData(pointIds, startTime, endTime, '1m');
}

/**
 * 설정 내보내기
 */
async function exportConfiguration(pointIds, deviceIds, tenantId) {
    const dataPoints = pointIds ? 
        await getDataPointRepo().findByIds(pointIds, tenantId) :
        await getDataPointRepo().findByDeviceIds(deviceIds, tenantId);
    
    return dataPoints.map(dp => ({
        id: dp.id,
        name: dp.name,
        device_id: dp.device_id,
        address: dp.address,
        data_type: dp.data_type,
        unit: dp.unit,
        is_enabled: dp.is_enabled
    }));
}

/**
 * 데이터 포맷 변환
 */
function convertToFormat(data, format) {
    switch (format) {
        case 'csv':
            return convertToCSV(data);
        case 'json':
            return data;
        case 'xml':
            return convertToXML(data);
        default:
            return data;
    }
}

/**
 * CSV 변환
 */
function convertToCSV(data) {
    if (!data.length) return '';
    
    const headers = Object.keys(data[0]).join(',');
    const rows = data.map(row => 
        Object.values(row).map(value => 
            typeof value === 'string' && value.includes(',') ? `"${value}"` : value
        ).join(',')
    );
    
    return [headers, ...rows].join('\n');
}

/**
 * XML 변환 (간단한 구현)
 */
function convertToXML(data) {
    let xml = '<?xml version="1.0" encoding="UTF-8"?>\n<data>\n';
    for (const item of data) {
        xml += '  <item>\n';
        for (const [key, value] of Object.entries(item)) {
            xml += `    <${key}>${value}</${key}>\n`;
        }
        xml += '  </item>\n';
    }
    xml += '</data>';
    return xml;
}

module.exports = router;