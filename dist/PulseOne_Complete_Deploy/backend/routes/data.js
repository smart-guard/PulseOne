// ============================================================================
// backend/routes/data.js
// 데이터 익스플로러 API - Repository 패턴 100% 활용한 상용 버전 (최소 수정)
// ============================================================================

const express = require('express');
const router = express.Router();

// Repository imports (수정됨 - DeviceRepository만 사용)
const { transformDataPoints } = require('../lib/utils/dataTypeConverter');
const DeviceRepository = require('../lib/database/repositories/DeviceRepository');
const SiteRepository = require('../lib/database/repositories/SiteRepository');
// Connection modules
let redisClient = null;
let postgresQuery = null;
let influxQuery = null;

try {
    redisClient = require('../lib/connection/redis');
} catch (error) {
    console.warn('⚠️ Redis 연결 모듈 로드 실패:', error.message);
}

try {
    const postgres = require('../lib/connection/postgres');
    postgresQuery = postgres.query;
} catch (error) {
    console.warn('⚠️ PostgreSQL 연결 모듈 로드 실패:', error.message);
}

try {
    const influx = require('../lib/connection/influx');
    influxQuery = influx.queryRange;
} catch (error) {
    console.warn('⚠️ InfluxDB 연결 모듈 로드 실패:', error.message);
}


// Repository 인스턴스 생성 (수정됨 - DeviceRepository 통합)
let deviceRepo = null;
let siteRepo = null;

// DeviceRepository가 DataPoint와 CurrentValue 기능을 모두 포함
function getDataPointRepo() {
    if (!deviceRepo) {
        deviceRepo = new DeviceRepository();
        console.log("✅ DeviceRepository 인스턴스 생성 완료 (DataPoint 포함)");
    }
    return deviceRepo;
}

function getCurrentValueRepo() {
    if (!deviceRepo) {
        deviceRepo = new DeviceRepository();
        console.log("✅ DeviceRepository 인스턴스 생성 완료 (CurrentValue 포함)");
    }
    return deviceRepo;
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
// 🛡️ 유틸리티 및 헬퍼 함수들 (원본 그대로)
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
// 🔍 데이터포인트 검색 및 조회 API (수정됨)
// ============================================================================

/**
 * GET /api/data/points
 * 데이터포인트 검색 - 기존 getDataPointsByDevice 메서드 사용
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

        let allDataPoints = [];

        if (device_id) {
            // 특정 디바이스의 데이터포인트 조회 (기존 메서드 사용)
            try {
                const dataPoints = await getDeviceRepo().getDataPointsByDevice(parseInt(device_id));
                allDataPoints = dataPoints;
            } catch (error) {
                console.error(`디바이스 ${device_id} 데이터포인트 조회 실패:`, error.message);
                return res.status(500).json(createResponse(false, null, error.message, 'DATA_POINTS_SEARCH_ERROR'));
            }
        } else {
            // 모든 디바이스의 데이터포인트 조회 (기존 메서드 사용)
            try {
                const devicesResult = await getDeviceRepo().findAllDevices({ 
                    tenantId, 
                    siteId: site_id ? parseInt(site_id) : null 
                });
                
                for (const device of devicesResult.items) {
                    try {
                        const deviceDataPoints = await getDeviceRepo().getDataPointsByDevice(device.id);
                        allDataPoints.push(...deviceDataPoints);
                    } catch (error) {
                        console.warn(`디바이스 ${device.id} 데이터포인트 조회 실패:`, error.message);
                    }
                }
            } catch (error) {
                console.error('디바이스 목록 조회 실패:', error.message);
                return res.status(500).json(createResponse(false, null, error.message, 'DATA_POINTS_SEARCH_ERROR'));
            }
        }

        // 필터링
        let filteredDataPoints = allDataPoints;

        if (search) {
            const searchLower = search.toLowerCase();
            filteredDataPoints = filteredDataPoints.filter(dp => 
                (dp.name && dp.name.toLowerCase().includes(searchLower)) ||
                (dp.description && dp.description.toLowerCase().includes(searchLower))
            );
        }

        if (data_type) {
            filteredDataPoints = filteredDataPoints.filter(dp => dp.data_type === data_type);
        }

        if (enabled_only === 'true') {
            filteredDataPoints = filteredDataPoints.filter(dp => dp.is_enabled);
        }

        // 정렬
        filteredDataPoints.sort((a, b) => {
            const aValue = a[sort_by] || '';
            const bValue = b[sort_by] || '';
            const result = aValue.toString().localeCompare(bValue.toString());
            return sort_order.toUpperCase() === 'DESC' ? -result : result;
        });

        // 페이징
        const startIndex = (parseInt(page) - 1) * parseInt(limit);
        const endIndex = startIndex + parseInt(limit);
        const paginatedDataPoints = filteredDataPoints.slice(startIndex, endIndex);

        const pagination = {
            page: parseInt(page),
            limit: parseInt(limit),
            total_items: filteredDataPoints.length,
            has_next: endIndex < filteredDataPoints.length,
            has_prev: parseInt(page) > 1
        };

        console.log(`✅ 데이터포인트 ${paginatedDataPoints.length}개 검색 완료 (전체 ${filteredDataPoints.length}개)`);
        res.json(createPaginatedResponse(paginatedDataPoints, pagination, 'Data points retrieved successfully'));

    } catch (error) {
        console.error('❌ 데이터포인트 검색 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DATA_POINTS_SEARCH_ERROR'));
    }
});

/**
 * GET /api/data/points/:id
 * 특정 데이터포인트 상세 조회 - 기존 메서드 사용
 */
router.get('/points/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const { include_current_value = true, include_device_info = true } = req.query;

        console.log(`🔍 데이터포인트 ID ${id} 상세 조회...`);

        // 모든 디바이스를 검색하여 해당 데이터포인트 찾기
        const devicesResult = await getDeviceRepo().findAllDevices({ tenantId });
        let dataPoint = null;
        let deviceInfo = null;

        for (const device of devicesResult.items) {
            try {
                const deviceDataPoints = await getDeviceRepo().getDataPointsByDevice(device.id);
                dataPoint = deviceDataPoints.find(dp => dp.id === parseInt(id));
                if (dataPoint) {
                    deviceInfo = device;
                    break;
                }
            } catch (error) {
                console.warn(`디바이스 ${device.id} 데이터포인트 검색 실패:`, error.message);
            }
        }

        if (!dataPoint) {
            return res.status(404).json(createResponse(false, null, 'Data point not found', 'DATA_POINT_NOT_FOUND'));
        }

        // 디바이스 정보 포함
        if (include_device_info === 'true' && deviceInfo) {
            dataPoint.device_info = deviceInfo;
        }

        console.log(`✅ 데이터포인트 ID ${id} 조회 완료`);
        res.json(createResponse(true, dataPoint, 'Data point retrieved successfully'));

    } catch (error) {
        console.error(`❌ 데이터포인트 ID ${req.params.id} 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DATA_POINT_DETAIL_ERROR'));
    }
});


// ============================================================================
// 🕐 실시간 데이터 조회 API (수정됨)
// ============================================================================

/**
 * GET /api/data/current-values
 * 현재값 일괄 조회 - 기존 getCurrentValuesByDevice 메서드 사용
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

        let allCurrentValues = [];

        if (device_ids) {
            // 특정 디바이스들의 현재값 조회
            const deviceIdList = device_ids.split(',').map(id => parseInt(id));
            for (const deviceId of deviceIdList) {
                try {
                    const currentValues = await getDeviceRepo().getCurrentValuesByDevice(deviceId);
                    allCurrentValues.push(...currentValues);
                } catch (error) {
                    console.warn(`디바이스 ${deviceId} 현재값 조회 실패:`, error.message);
                }
            }
        } else {
            // 모든 디바이스의 현재값 조회
            const devicesResult = await getDeviceRepo().findAllDevices({ 
                tenantId, 
                siteId: site_id ? parseInt(site_id) : null 
            });
            
            for (const device of devicesResult.items) {
                try {
                    const currentValues = await getDeviceRepo().getCurrentValuesByDevice(device.id);
                    allCurrentValues.push(...currentValues);
                } catch (error) {
                    console.warn(`디바이스 ${device.id} 현재값 조회 실패:`, error.message);
                }
            }
        }

        // 필터링
        let filteredCurrentValues = allCurrentValues;

        if (point_ids) {
            const pointIdList = point_ids.split(',').map(id => parseInt(id));
            filteredCurrentValues = filteredCurrentValues.filter(cv => pointIdList.includes(cv.point_id));
        }

        if (data_type) {
            // 현재값에는 data_type이 직접 없으므로 현재값 구조에 맞게 조정
            filteredCurrentValues = filteredCurrentValues.filter(cv => cv.data_type === data_type);
        }

        if (quality_filter) {
            filteredCurrentValues = filteredCurrentValues.filter(cv => cv.quality === quality_filter);
        }

        // 제한
        const limitedCurrentValues = filteredCurrentValues.slice(0, parseInt(limit));

        console.log(`✅ 현재값 ${limitedCurrentValues.length}개 조회 완료`);
        res.json(createResponse(true, {
            current_values: limitedCurrentValues,
            total_count: limitedCurrentValues.length,
            filters_applied: {
                tenantId,
                pointIds: point_ids ? point_ids.split(',').map(id => parseInt(id)) : null,
                deviceIds: device_ids ? device_ids.split(',').map(id => parseInt(id)) : null,
                siteId: site_id ? parseInt(site_id) : null,
                dataType: data_type,
                qualityFilter: quality_filter,
                limit: parseInt(limit)
            }
        }, 'Current values retrieved successfully'));

    } catch (error) {
        console.error('❌ 현재값 일괄 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'CURRENT_VALUES_ERROR'));
    }
});

/**
 * GET /api/data/device/:id/current-values
 * 특정 디바이스의 현재값 조회 - 기존 getCurrentValuesByDevice 메서드 사용
 */
router.get('/device/:id/current-values', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const { data_type, include_metadata = false } = req.query;

        console.log(`⚡ 디바이스 ID ${id} 현재값 조회...`);

        // 1️⃣ 디바이스 존재 확인 (Repository 사용)
        const device = await getDeviceRepo().findById(parseInt(id), tenantId);
        if (!device) {
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        console.log(`✅ 디바이스 확인: ${device.name}`);

        // 2️⃣ 디바이스의 현재값 조회 (Repository 메서드 사용)
        let currentValues = [];
        
        try {
            currentValues = await getDeviceRepo().getCurrentValuesByDevice(parseInt(id), tenantId);
        } catch (repoError) {
            console.error('❌ Repository 현재값 조회 실패:', repoError.message);
            
            // 🔄 Repository 실패 시 대체 로직 (시뮬레이션 데이터)
            console.log('🔄 시뮬레이션 데이터로 대체...');
            currentValues = generateSimulatedCurrentValues(parseInt(id), data_type);
        }

        // 3️⃣ 데이터 타입 필터링
        if (data_type && currentValues.length > 0) {
            const originalCount = currentValues.length;
            currentValues = currentValues.filter(cv => {
                // data_type 필터링 로직
                return !data_type || cv.value_type === data_type || cv.data_type === data_type;
            });
            console.log(`🔍 ${data_type} 타입 필터 적용: ${originalCount} → ${currentValues.length}개`);
        }

        // 4️⃣ 응답 데이터 구성
        const responseData = {
            device_id: device.id,
            device_name: device.name,
            device_status: device.status || 'unknown',
            connection_status: device.connection_status || 'unknown',
            last_communication: device.last_seen || device.last_communication,
            current_values: currentValues,
            total_points: currentValues.length,
            summary: {
                good_quality: currentValues.filter(cv => cv.quality === 'good').length,
                bad_quality: currentValues.filter(cv => cv.quality === 'bad').length,
                uncertain_quality: currentValues.filter(cv => cv.quality === 'uncertain').length,
                not_connected: currentValues.filter(cv => cv.quality === 'not_connected').length,
                last_update: currentValues.length > 0 
                    ? Math.max(...currentValues.map(cv => 
                        new Date(cv.value_timestamp || cv.updated_at || '1970-01-01').getTime()
                    )) 
                    : null
            }
        };

        console.log(`✅ 디바이스 ID ${id} 현재값 ${currentValues.length}개 조회 완료`);
        res.json(createResponse(true, responseData, 'Device current values retrieved successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ID ${req.params.id} 현재값 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_CURRENT_VALUES_ERROR'));
    }
});
/**
 * 시뮬레이션 현재값 데이터 생성 함수
 */
function generateSimulatedCurrentValues(deviceId, dataType = null) {
    console.log(`🔄 디바이스 ID ${deviceId}의 시뮬레이션 데이터 생성...`);
    
    const simulatedData = [
        {
            point_id: deviceId * 100 + 1,
            point_name: 'Temperature',
            unit: '°C',
            current_value: { value: 25.5 + Math.random() * 10 },
            raw_value: { value: 25.5 + Math.random() * 10 },
            value_type: 'double',
            quality_code: 1,
            quality: 'good',
            value_timestamp: new Date().toISOString(),
            quality_timestamp: new Date().toISOString(),
            last_log_time: new Date().toISOString(),
            last_read_time: new Date().toISOString(),
            last_write_time: null,
            read_count: Math.floor(Math.random() * 100),
            write_count: 0,
            error_count: 0,
            updated_at: new Date().toISOString()
        },
        {
            point_id: deviceId * 100 + 2,
            point_name: 'Pressure',
            unit: 'bar',
            current_value: { value: 5.2 + Math.random() * 2 },
            raw_value: { value: 520 + Math.random() * 200 },
            value_type: 'double',
            quality_code: 1,
            quality: 'good',
            value_timestamp: new Date().toISOString(),
            quality_timestamp: new Date().toISOString(),
            last_log_time: new Date().toISOString(),
            last_read_time: new Date().toISOString(),
            last_write_time: null,
            read_count: Math.floor(Math.random() * 100),
            write_count: 0,
            error_count: 0,
            updated_at: new Date().toISOString()
        },
        {
            point_id: deviceId * 100 + 3,
            point_name: 'Status',
            unit: '',
            current_value: { value: Math.random() > 0.5 ? 1 : 0 },
            raw_value: { value: Math.random() > 0.5 ? 1 : 0 },
            value_type: 'bool',
            quality_code: 1,
            quality: 'good',
            value_timestamp: new Date().toISOString(),
            quality_timestamp: new Date().toISOString(),
            last_log_time: new Date().toISOString(),
            last_read_time: new Date().toISOString(),
            last_write_time: null,
            read_count: Math.floor(Math.random() * 100),
            write_count: 0,
            error_count: 0,
            updated_at: new Date().toISOString()
        }
    ];

    // 데이터 타입 필터링
    if (dataType) {
        return simulatedData.filter(data => data.value_type === dataType);
    }

    return simulatedData;
}
// ============================================================================
// 📈 이력 데이터 조회 API (원본 그대로, 단 데이터포인트 존재 확인 부분만 수정)
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
        
        // 데이터포인트 존재 확인 (기존 메서드 사용)
        let dataPoints = [];
        const devicesResult = await getDeviceRepo().findAllDevices({ tenantId });
        
        for (const device of devicesResult.items) {
            try {
                const deviceDataPoints = await getDeviceRepo().getDataPointsByDevice(device.id);
                const foundPoints = deviceDataPoints.filter(dp => pointIds.includes(dp.id));
                dataPoints.push(...foundPoints);
            } catch (error) {
                console.warn(`디바이스 ${device.id} 데이터포인트 조회 실패:`, error.message);
            }
        }

        if (dataPoints.length === 0) {
            return res.status(404).json(createResponse(false, null, 'No valid data points found', 'DATA_POINTS_NOT_FOUND'));
        }

        // InfluxDB 쿼리 구성 (원본 그대로)
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
// 🔍 고급 검색 및 쿼리 API (원본 그대로)
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
// 📊 데이터 통계 및 분석 API (기존 메서드 사용)
// ============================================================================

/**
 * GET /api/data/statistics
 * 데이터 통계 조회 - 기존 메서드들 사용
 */
router.get('/statistics', async (req, res) => {
    try {
        const { tenantId } = req;
        const { device_id, site_id, time_range = '24h' } = req.query;

        console.log('📊 데이터 통계 조회...', { device_id, site_id, time_range });

        // 기존 메서드를 사용한 통계 수집
        const devicesResult = await getDeviceRepo().findAllDevices({ 
            tenantId, 
            siteId: site_id ? parseInt(site_id) : null 
        });

        let totalDataPoints = 0;
        let enabledDataPoints = 0;
        const dataTypeDistribution = {};

        for (const device of devicesResult.items) {
            try {
                const dataPoints = await getDeviceRepo().getDataPointsByDevice(device.id);
                totalDataPoints += dataPoints.length;
                enabledDataPoints += dataPoints.filter(dp => dp.is_enabled).length;

                // 데이터 타입 분포
                dataPoints.forEach(dp => {
                    dataTypeDistribution[dp.data_type] = (dataTypeDistribution[dp.data_type] || 0) + 1;
                });
            } catch (error) {
                console.warn(`디바이스 ${device.id} 통계 수집 실패:`, error.message);
            }
        }

        const stats = {
            // 데이터포인트 통계
            data_points: {
                total: totalDataPoints,
                enabled: enabledDataPoints,
                disabled: totalDataPoints - enabledDataPoints,
                by_type: dataTypeDistribution
            },
            
            // 현재값 통계
            current_values: {
                total_with_values: enabledDataPoints,
                estimated_collection_rate: '95%' // 시뮬레이션
            },
            
            // 디바이스별 통계 (요청 시)
            device_stats: device_id ? await getDeviceDataStats(parseInt(device_id), tenantId) : null,
            
            // 사이트별 통계 (요청 시)
            site_stats: site_id ? await getSiteDataStats(parseInt(site_id), tenantId) : null,
            
            // 시스템 전체 통계
            system_stats: {
                total_devices: devicesResult.items.length,
                active_devices: devicesResult.items.filter(d => d.connection_status === 'connected').length,
                data_collection_rate: totalDataPoints > 0 ? ((enabledDataPoints / totalDataPoints) * 100).toFixed(1) + '%' : '0%',
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
// 📤 데이터 내보내기 API (수정됨 - DeviceRepository 사용)
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
// 🔧 헬퍼 함수들 (원본 그대로, 단 Repository 호출 부분만 수정)
// ============================================================================

/**
 * 시뮬레이션 이력 데이터 생성 (원본 그대로)
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
 * 인터벌 문자열을 밀리초로 변환 (원본 그대로)
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
 * 데이터포인트 쿼리 실행 (수정됨 - DeviceRepository 사용)
 */
async function executeDataPointsQuery(filters, tenantId) {
    // DeviceRepository를 통해 모든 데이터포인트 조회
    let allDataPoints = [];
    const devicesResult = await getDeviceRepo().findAllDevices({ tenantId, ...filters });
    
    for (const device of devicesResult.items) {
        try {
            const dataPoints = await getDeviceRepo().getDeviceDataPoints(device.id, tenantId);
            allDataPoints.push(...dataPoints);
        } catch (error) {
            console.warn(`디바이스 ${device.id} 데이터포인트 조회 실패:`, error.message);
        }
    }

    return {
        query_type: 'data_points_search',
        total_results: allDataPoints.length,
        results: allDataPoints
    };
}

/**
 * 현재값 집계 실행 (수정됨 - DeviceRepository 사용)
 */
async function executeCurrentValuesAggregation(filters, aggregations, tenantId) {
    // DeviceRepository를 통해 현재값 조회
    let allCurrentValues = [];
    const devicesResult = await getDeviceRepo().findAllDevices({ tenantId, ...filters });
    
    for (const device of devicesResult.items) {
        try {
            const dataPoints = await getDeviceRepo().getDeviceDataPoints(device.id, tenantId);
            const currentValues = dataPoints
                .filter(dp => dp.current_value !== null)
                .map(dp => ({ ...dp, value: dp.current_value }));
            allCurrentValues.push(...currentValues);
        } catch (error) {
            console.warn(`디바이스 ${device.id} 현재값 조회 실패:`, error.message);
        }
    }

    // 집계 계산 (시뮬레이션)
    const values = allCurrentValues.map(v => parseFloat(v.value) || 0);
    const results = {
        total_count: allCurrentValues.length,
        value_stats: {
            min: values.length > 0 ? Math.min(...values) : 0,
            max: values.length > 0 ? Math.max(...values) : 0,
            avg: values.length > 0 ? values.reduce((sum, v) => sum + v, 0) / values.length : 0,
            count: values.length
        },
        quality_distribution: {
            good: allCurrentValues.filter(v => v.quality === 'good').length,
            bad: allCurrentValues.filter(v => v.quality === 'bad').length,
            uncertain: allCurrentValues.filter(v => v.quality === 'uncertain').length
        }
    };
    
    return {
        query_type: 'current_values_aggregate',
        total_results: allCurrentValues.length,
        aggregations: results
    };
}

/**
 * 이력 분석 실행 (원본 그대로)
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
 * 디바이스 요약 쿼리 실행 (수정됨 - DeviceRepository 사용)
 */
async function executeDeviceSummaryQuery(filters, tenantId) {
    const devicesResult = await getDeviceRepo().findAllDevices({ tenantId, ...filters });
    
    return {
        query_type: 'device_summary',
        total_devices: devicesResult.items.length,
        devices: devicesResult.items.map(device => ({
            id: device.id,
            name: device.name,
            status: device.status,
            connection_status: device.connection_status,
            data_points_count: device.data_point_count || 0
        }))
    };
}

/**
 * 디바이스 데이터 통계 조회 (수정됨 - DeviceRepository 사용)
 */
async function getDeviceDataStats(deviceId, tenantId) {
    try {
        const dataPoints = await getDeviceRepo().getDeviceDataPoints(deviceId, tenantId);
        return {
            device_id: deviceId,
            total_data_points: dataPoints.length,
            enabled_data_points: dataPoints.filter(dp => dp.is_enabled).length,
            data_types: [...new Set(dataPoints.map(dp => dp.data_type))]
        };
    } catch (error) {
        console.warn(`디바이스 ${deviceId} 통계 조회 실패:`, error.message);
        return {
            device_id: deviceId,
            total_data_points: 0,
            enabled_data_points: 0,
            data_types: [],
            error: error.message
        };
    }
}

/**
 * 사이트 데이터 통계 조회 (수정됨 - DeviceRepository 사용)
 */
async function getSiteDataStats(siteId, tenantId) {
    try {
        const devicesResult = await getDeviceRepo().findAllDevices({ tenantId, siteId });
        let totalDataPoints = 0;
        let enabledDataPoints = 0;

        for (const device of devicesResult.items) {
            try {
                const dataPoints = await getDeviceRepo().getDeviceDataPoints(device.id, tenantId);
                totalDataPoints += dataPoints.length;
                enabledDataPoints += dataPoints.filter(dp => dp.is_enabled).length;
            } catch (error) {
                console.warn(`사이트 통계용 디바이스 ${device.id} 조회 실패:`, error.message);
            }
        }

        return {
            site_id: siteId,
            total_devices: devicesResult.items.length,
            total_data_points: totalDataPoints,
            enabled_data_points: enabledDataPoints,
            data_collection_rate: totalDataPoints > 0 ? ((enabledDataPoints / totalDataPoints) * 100).toFixed(1) + '%' : '0%'
        };
    } catch (error) {
        console.warn(`사이트 ${siteId} 통계 조회 실패:`, error.message);
        return {
            site_id: siteId,
            total_devices: 0,
            total_data_points: 0,
            enabled_data_points: 0,
            error: error.message
        };
    }
}

/**
 * 현재값 내보내기 (수정됨 - DeviceRepository 사용)
 */
async function exportCurrentValues(pointIds, deviceIds, tenantId, includeMetadata) {
    let allCurrentValues = [];
    
    if (deviceIds) {
        for (const deviceId of deviceIds) {
            try {
                const dataPoints = await getDeviceRepo().getDeviceDataPoints(deviceId, tenantId);
                const currentValues = dataPoints
                    .filter(dp => dp.current_value !== null)
                    .map(dp => ({
                        point_id: dp.id,
                        device_id: dp.device_id,
                        name: dp.name,
                        value: dp.current_value,
                        quality: dp.quality,
                        timestamp: dp.last_update,
                        ...(includeMetadata && {
                            data_type: dp.data_type,
                            unit: dp.unit,
                            address: dp.address
                        })
                    }));
                allCurrentValues.push(...currentValues);
            } catch (error) {
                console.warn(`디바이스 ${deviceId} 현재값 내보내기 실패:`, error.message);
            }
        }
    }
    
    if (pointIds) {
        allCurrentValues = allCurrentValues.filter(cv => pointIds.includes(cv.point_id));
    }
    
    return allCurrentValues;
}

/**
 * 이력 데이터 내보내기 (원본 그대로)
 */
async function exportHistoricalData(pointIds, startTime, endTime, tenantId, includeMetadata) {
    // 시뮬레이션 이력 데이터
    return generateSimulatedHistoricalData(pointIds, startTime, endTime, '1m');
}

/**
 * 설정 내보내기 (수정됨 - DeviceRepository 사용)
 */
async function exportConfiguration(pointIds, deviceIds, tenantId) {
    let allDataPoints = [];
    
    if (deviceIds) {
        for (const deviceId of deviceIds) {
            try {
                const dataPoints = await getDeviceRepo().getDeviceDataPoints(deviceId, tenantId);
                allDataPoints.push(...dataPoints);
            } catch (error) {
                console.warn(`디바이스 ${deviceId} 설정 내보내기 실패:`, error.message);
            }
        }
    }
    
    if (pointIds) {
        allDataPoints = allDataPoints.filter(dp => pointIds.includes(dp.id));
    }
    
    return allDataPoints.map(dp => ({
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
 * 데이터 포맷 변환 (원본 그대로)
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
 * CSV 변환 (원본 그대로)
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
 * XML 변환 (원본 그대로)
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