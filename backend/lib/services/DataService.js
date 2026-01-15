// =============================================================================
// backend/lib/services/DataService.js
// 데이터 익스플로러 서비스
// =============================================================================

const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

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
                search,
                device_id,
                site_id,
                data_type,
                enabled_only = false
            } = params;

            let allDataPoints = [];

            if (device_id) {
                allDataPoints = await this.deviceRepo.getDataPointsByDevice(parseInt(device_id));
            } else {
                const devicesResult = await this.deviceRepo.findAllDevices({
                    tenantId,
                    siteId: site_id ? parseInt(site_id) : null
                });

                for (const device of devicesResult.items) {
                    try {
                        const deviceDataPoints = await this.deviceRepo.getDataPointsByDevice(device.id);
                        allDataPoints.push(...deviceDataPoints);
                    } catch (error) {
                        this.logger?.warn(`디바이스 ${device.id} 데이터포인트 조회 실패:`, error.message);
                    }
                }
            }

            // 필터링 적용
            let filteredResults = allDataPoints;
            if (search) {
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

            return {
                items: paginatedResults,
                pagination: {
                    page: parseInt(page),
                    limit: parseInt(limit),
                    total_items: total,
                    totalPages: Math.ceil(total / limit),
                    has_next: startIndex + parseInt(limit) < total,
                    has_prev: page > 1
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
                current_values: currentValues,
                total_points: currentValues.length
            };
        }, 'DataService.getDeviceCurrentValues');
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
}

module.exports = new DataService();
