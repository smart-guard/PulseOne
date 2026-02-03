// =============================================================================
// backend/lib/services/DataService.js
// 데이터 익스플로러 서비스
// =============================================================================

const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

// 헬퍼: JSON 스트링인 경우 파싱 (특히 {"value": 1.0} 형태 대응)
const safeParseValue = (val) => {
    if (typeof val !== 'string' || !val.trim().startsWith('{')) return val;
    try {
        const parsed = JSON.parse(val);
        if (parsed && typeof parsed === 'object' && 'value' in parsed) {
            return parsed.value;
        }
        return parsed;
    } catch (e) {
        return val;
    }
};
const timeSeriesManager = require('../connection/timeseries');
const ConfigManager = require('../config/ConfigManager');
const config = ConfigManager.getInstance();

class DataService extends BaseService {
    constructor() {
        super(null);
    }

    get deviceRepo() {
        return RepositoryFactory.getInstance().getRepository('DeviceRepository');
    }

    get siteRepo() {
        return RepositoryFactory.getInstance().getRepository('SiteRepository');
    }

    /**
     * 데이터포인트 검색
     */
    async searchPoints(params, tenantId) {
        return await this.handleRequest(async () => {
            const {
                page = 1,
                limit = 50,
                search = '',
                device_id,
                site_id,
                data_type,
                enabled_only = false
            } = params;

            let allDataPoints = [];

            if (device_id) {
                // 특정 디바이스의 포인트 조회
                allDataPoints = await this.deviceRepo.getDataPointsByDevice(parseInt(device_id));
            } else {
                // 모든 디바이스의 포인트 검색 (Join 쿼리 사용으로 성능 최적화)
                allDataPoints = await this.deviceRepo.searchDataPoints(tenantId, search);

                // 사이트 필터가 있는 경우 추가 필터링
                if (site_id) {
                    const siteIdInt = parseInt(site_id);
                    // searchDataPoints에 site_id가 없으므로 메모리에서 필터링하거나 쿼리 수정 필요
                    // 일단 메모리에서 처리 (디바이스가 아주 많지 않다는 가정)
                    const devicesInSite = await this.deviceRepo.findAll(tenantId, { siteId: siteIdInt });
                    const validDeviceIds = new Set(devicesInSite.map(d => d.id));
                    allDataPoints = allDataPoints.filter(dp => validDeviceIds.has(dp.device_id));
                }
            }

            // 필터링 적용
            let filteredResults = allDataPoints;

            // 검색어 필터링 (searchDataPoints는 이미 처리하지만, getDataPointsByDevice는 안함)
            if (search && device_id) {
                const searchLower = search.toLowerCase();
                filteredResults = filteredResults.filter(dp =>
                    dp.name?.toLowerCase().includes(searchLower) ||
                    dp.description?.toLowerCase().includes(searchLower) ||
                    dp.tags?.toLowerCase().includes(searchLower)
                );
            }

            if (data_type) {
                filteredResults = filteredResults.filter(dp => dp.data_type === data_type);
            }

            if (enabled_only === 'true' || enabled_only === true) {
                filteredResults = filteredResults.filter(dp => dp.is_enabled);
            }

            const total = filteredResults.length;
            const startIndex = (page - 1) * limit;
            const paginatedResults = filteredResults.slice(startIndex, startIndex + parseInt(limit));

            // 데이터 일관성을 위해 device_name 보장
            const items = paginatedResults.map(dp => ({
                ...dp,
                device_name: dp.device_name || 'Unknown Device'
            }));

            return {
                items,
                pagination: {
                    page: parseInt(page),
                    limit: parseInt(limit),
                    total: total,
                    totalPages: Math.ceil(total / limit),
                    hasNext: startIndex + parseInt(limit) < total,
                    hasPrev: page > 1
                }
            };
        }, 'DataService.searchPoints');
    }

    /**
     * 특정 데이터포인트 상세 조회
     */
    async getDataPointDetail(pointId) {
        return await this.handleRequest(async () => {
            const point = await this.deviceRepo.getDataPointById(parseInt(pointId));
            if (!point) throw new Error('데이터포인트를 찾을 수 없습니다.');
            return point;
        }, 'DataService.getDataPointDetail');
    }

    /**
     * 특정 포인트들의 현재값 일괄 조회
     */
    async getCurrentValuesByPointIds(pointIds, tenantId) {
        return await this.handleRequest(async () => {
            if (!pointIds || pointIds.length === 0) return [];

            const results = [];
            for (const id of pointIds) {
                const point = await this.deviceRepo.getDataPointById(parseInt(id));
                if (point) {
                    results.push({
                        id: point.id,
                        point_id: point.id,
                        name: point.name,
                        value: safeParseValue(point.current_value),
                        unit: point.unit,
                        status: point.status,
                        quality: point.quality || 'good',
                        timestamp: point.last_update || point.updated_at
                    });
                }
            }
            return results;
        }, 'DataService.getCurrentValuesByPointIds');
    }

    /**
     * 디바이스 현재값 조회
     */
    async getDeviceCurrentValues(deviceId, tenantId) {
        return await this.handleRequest(async () => {
            const device = await this.deviceRepo.findById(parseInt(deviceId), tenantId);
            if (!device) throw new Error('디바이스를 찾을 수 없습니다.');

            const currentValues = await this.deviceRepo.getCurrentValuesByDevice(parseInt(deviceId), tenantId);

            return {
                device: {
                    id: device.id,
                    name: device.name,
                    status: device.status,
                    connection_status: device.connection_status,
                    last_communication: device.last_seen || device.last_communication
                },
                current_values: currentValues.map(cv => ({
                    ...cv,
                    value: safeParseValue(cv.current_value),
                    timestamp: cv.value_timestamp,
                    quality: cv.quality || 'good'
                })),
                total_points: currentValues.length
            };
        }, 'DataService.getDeviceCurrentValues');
    }

    /**
     * 여러 디바이스의 상태 및 Redis 데이터 존재 여부 일괄 조회
     */
    async getBulkDeviceStatus(deviceIds, tenantId) {
        return await this.handleRequest(async () => {
            if (!deviceIds) {
                throw new Error('deviceIds is required');
            }

            const ids = Array.isArray(deviceIds) ? deviceIds : String(deviceIds).split(',').map(id => parseInt(id.trim()));
            const results = {};

            for (const id of ids) {
                try {
                    const device = await this.deviceRepo.findById(id, tenantId);

                    if (device) {
                        const hasValues = await this.deviceRepo.hasCurrentValues(id, tenantId);
                        results[id] = {
                            connection_status: device.connection_status,
                            hasRedisData: hasValues,
                            last_seen: device.last_seen || device.last_communication
                        };
                    }
                } catch (error) {
                    this.logger?.warn(`디바이스 ${id} 상태 조회 실패:`, error.message);
                }
            }

            return results;
        }, 'DataService.getBulkDeviceStatus');
    }

    /**
     * 이력 데이터 조회 (InfluxDB)
     */
    async getHistoricalData(params, tenantId) {
        return await this.handleRequest(async () => {
            const { point_ids, start_time, end_time, interval = '1m', aggregation = 'mean' } = params;

            if (!point_ids) throw new Error('point_ids is required');

            const ids = Array.isArray(point_ids) ? point_ids : String(point_ids).split(',').map(id => parseInt(id.trim()));

            // 포인트 메타데이터 조회
            const pointDetails = [];
            for (const id of ids) {
                try {
                    const result = await this.getDataPointDetail(id);
                    if (result.success && result.data) {
                        pointDetails.push(result.data);
                    }
                } catch (error) {
                    this.logger?.warn(`포인트 ${id} 상세 정보 조회 실패:`, error.message);
                }
            }

            const bucket = config.get('INFLUXDB_BUCKET', 'history');

            // Flux 쿼리 생성
            let fluxQuery = `from(bucket: "${bucket}")
                |> range(start: ${start_time}, stop: ${end_time})
                |> filter(fn: (r) => r._measurement == "device_telemetry" or r._measurement == "robot_state")
            `;

            if (ids.length > 0) {
                const pointFilters = ids.map(id => `r.point_id == "${id}"`).join(' or ');
                fluxQuery += `|> filter(fn: (r) => ${pointFilters})\n`;
            }

            // 필드 필터링 (p_ID 형식)
            const fieldFilters = ids.map(id => `r._field == "p_${id}"`).join(' or ');
            if (fieldFilters) {
                fluxQuery += `|> filter(fn: (r) => ${fieldFilters})\n`;
            }

            if (interval && interval !== 'none') {
                fluxQuery += `|> aggregateWindow(every: ${interval}, fn: ${aggregation}, createEmpty: false)\n`;
            }

            fluxQuery += `|> yield(name: "${aggregation}")`;

            const rawData = await timeSeriesManager.queryData(fluxQuery);

            const historical_data = rawData.map(row => ({
                time: row._time,
                point_id: parseInt(row.point_id),
                value: row._value,
                quality: 'good'
            }));

            return {
                data_points: pointDetails.map(p => ({
                    point_id: p.id,
                    point_name: p.name,
                    device_name: p.device_name || 'Unknown',
                    site_name: p.site_name || 'N/A',
                    data_type: p.data_type,
                    unit: p.unit
                })),
                historical_data,
                query_info: {
                    start_time,
                    end_time,
                    interval,
                    aggregation,
                    total_points: historical_data.length
                }
            };
        }, 'DataService.getHistoricalData');
    }

    /**
     * 데이터 통계 조회
     */
    async getStatistics(params, tenantId) {
        return await this.handleRequest(async () => {
            const { site_id } = params;

            const devicesResult = await this.deviceRepo.findAllDevices({
                tenantId,
                siteId: site_id ? parseInt(site_id) : null
            });

            let totalDataPoints = 0;
            let enabledDataPoints = 0;
            const dataTypeDistribution = {};

            for (const device of devicesResult.items) {
                // 삭제된 디바이스는 이미 findAllDevices에서 필터링되었을 것이나 안전하게 확인
                if (device.is_deleted) continue;

                try {
                    const dataPoints = await this.deviceRepo.getDataPointsByDevice(device.id);
                    totalDataPoints += dataPoints.length;

                    // 활성화된 디바이스의 활성화된 포인트만 카운트
                    if (device.is_enabled) {
                        enabledDataPoints += dataPoints.filter(dp => dp.is_enabled).length;
                    }

                    dataPoints.forEach(dp => {
                        dataTypeDistribution[dp.data_type] = (dataTypeDistribution[dp.data_type] || 0) + 1;
                    });
                } catch (error) {
                    this.logger?.warn(`디바이스 ${device.id} 통계 수집 실패:`, error.message);
                }
            }

            return {
                data_points: {
                    total: totalDataPoints,
                    enabled: enabledDataPoints,
                    disabled: totalDataPoints - enabledDataPoints,
                    by_type: dataTypeDistribution
                },
                system_stats: {
                    total_devices: devicesResult.items.length,
                    active_devices: devicesResult.items.filter(d => d.connection_status === 'connected').length,
                    last_updated: new Date().toISOString()
                }
            };
        }, 'DataService.getStatistics');
    }

    /**
     * 현재값 내보내기
     */
    async exportCurrentValues(pointIds, deviceIds, tenantId) {
        return await this.handleRequest(async () => {
            let dataPoints = [];

            if (pointIds && pointIds.length > 0) {
                // Implement if needed
            } else if (deviceIds && deviceIds.length > 0) {
                for (const deviceId of deviceIds) {
                    const values = await this.deviceRepo.getCurrentValuesByDevice(deviceId, tenantId);
                    dataPoints.push(...values);
                }
            }
            return dataPoints;
        }, 'DataService.exportCurrentValues');
    }

    /**
     * 설정 내보내기
     */
    async exportConfiguration(pointIds, deviceIds, tenantId) {
        return await this.handleRequest(async () => {
            // Placeholder
            return [];
        }, 'DataService.exportConfiguration');
    }

    /**
     * 이력 데이터 내보내기
     */
    async exportHistoricalData(params, tenantId) {
        return await this.handleRequest(async () => {
            const result = await this.getHistoricalData(params, tenantId);
            if (result.success && result.data) {
                return result.data.historical_data || [];
            }
            return [];
        }, 'DataService.exportHistoricalData');
    }
}

module.exports = new DataService();
