// =============================================================================
// backend/lib/services/RealtimeService.js
// 실시간 데이터 및 구독 관리 서비스
// =============================================================================

const BaseService = require('./BaseService');
const { getRedisClient } = require('../connection/redis');

class RealtimeService extends BaseService {
    constructor() {
        super(null);
    }

    /**
     * Redis에서 현재값 조회 (device:* 패턴)
     */
    async getCurrentValues(params) {
        return await this.handleRequest(async () => {
            const {
                device_ids,
                point_names,
                quality_filter = 'all',
                limit = 100
            } = params;

            const redis = await getRedisClient();
            let allKeys = [];

            if (device_ids && device_ids.length > 0) {
                for (const deviceId of device_ids) {
                    const keys = await redis.keys(`device:${deviceId}:*`);
                    allKeys.push(...keys.filter(k => !k.endsWith(':status')));
                }
            } else {
                const keys = await redis.keys('device:*:*');
                allKeys = keys.filter(k => !k.endsWith(':status'));
            }

            if (point_names && point_names.length > 0) {
                allKeys = allKeys.filter(key => {
                    const parts = key.split(':');
                    return parts.length === 3 && point_names.includes(parts[2]);
                });
            }

            const values = [];
            for (let i = 0; i < allKeys.length && values.length < limit; i++) {
                const data = await redis.get(allKeys[i]);
                if (data) {
                    const pointData = JSON.parse(data);
                    if (quality_filter === 'all' || pointData.quality === quality_filter) {
                        values.push({
                            ...pointData,
                            key: allKeys[i],
                            device_id: allKeys[i].split(':')[1],
                            point_name: allKeys[i].split(':')[2]
                        });
                    }
                }
            }

            return values;
        }, 'RealtimeService.getCurrentValues');
    }

    /**
     * 실시간 구독 생성
     */
    async createSubscription(params, tenantId, userId) {
        return await this.handleRequest(async () => {
            const { keys, device_ids, point_names, update_interval = 1000 } = params;
            const redis = await getRedisClient();

            let actualKeys = keys || [];

            if (device_ids && device_ids.length > 0) {
                for (const id of device_ids) {
                    const dk = await redis.keys(`device:${id}:*`);
                    actualKeys.push(...dk.filter(k => !k.endsWith(':status')));
                }
            }

            actualKeys = [...new Set(actualKeys)];
            const subscriptionId = `sub_${tenantId}_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;

            const subData = {
                id: subscriptionId,
                tenant_id: tenantId,
                user_id: userId,
                keys: actualKeys,
                update_interval,
                created_at: new Date().toISOString()
            };

            await redis.setex(`subscription:${subscriptionId}`, 24 * 60 * 60, JSON.stringify(subData));

            return {
                subscription_id: subscriptionId,
                total_keys: actualKeys.length,
                update_interval
            };
        }, 'RealtimeService.createSubscription');
    }

    /**
     * 특정 디바이스의 실시간 값 조회 (상태 포함)
     */
    async getDeviceRealtimeValues(deviceId) {
        return await this.handleRequest(async () => {
            const redis = await getRedisClient();
            const keys = await redis.keys(`device:${deviceId}:*`);

            const statusKey = `device:${deviceId}:status`;
            const statusData = await redis.get(statusKey);
            const status = statusData ? JSON.parse(statusData) : null;

            const pointKeys = keys.filter(k => !k.endsWith(':status'));
            const points = [];
            for (const key of pointKeys) {
                const data = await redis.get(key);
                if (data) points.push(JSON.parse(data));
            }

            return {
                device_id: deviceId,
                status: status,
                connection_status: status?.connection_status || (points.length > 0 ? 'connected' : 'disconnected'),
                last_communication: status?.last_communication || (points.length > 0 ? points[0].timestamp : null),
                error_count: status?.error_count || 0,
                data_points: points,
                total_points: points.length,
                summary: {
                    good_quality: points.filter(p => p.quality === 'good').length,
                    bad_quality: points.filter(p => p.quality === 'bad').length,
                    uncertain_quality: points.filter(p => p.quality === 'uncertain').length,
                    comm_failure: points.filter(p => p.quality === 'comm_failure').length,
                    last_known: points.filter(p => p.quality === 'last_known').length,
                    last_update: points.length > 0 ? new Date(Math.max(...points.map(p => p.timestamp))).toISOString() : new Date().toISOString()
                }
            };
        }, 'RealtimeService.getDeviceRealtimeValues');
    }

    /**
     * 초기 가동 시 DB 데이터를 Redis로 동기화 (Seeding)
     * 수집기가 데이터를 쓰기 전에도 UI에서 확인 가능하도록 함
     */
    async initializeRedisData() {
        return await this.handleRequest(async () => {
            const redis = await getRedisClient();
            const RepositoryFactory = require('../database/repositories/RepositoryFactory').getInstance();
            const deviceRepo = RepositoryFactory.getDeviceRepository();
            const knex = require('../database/KnexManager').getInstance().getKnex();

            // 1. 모든 활성화된 디바이스 조회
            const devices = await deviceRepo.query()
                .where('is_enabled', 1)
                .andWhere(builder => {
                    builder.where('is_deleted', 0).orWhereNull('is_deleted');
                });

            let pointCount = 0;
            let deviceCount = 0;

            for (const device of devices) {
                // 2. 디바이스별 데이터 포인트 및 DB의 현재값 조회
                const points = await deviceRepo.getDataPointsByDevice(device.id);

                for (const point of points) {
                    const redisKey = `device:${device.id}:${point.name}`;

                    let displayValue = point.current_value;
                    if (displayValue && typeof displayValue === 'string' && displayValue.startsWith('{')) {
                        try {
                            const parsed = JSON.parse(displayValue);
                            if (parsed && parsed.value !== undefined) {
                                displayValue = String(parsed.value);
                            }
                        } catch (e) {
                            // 파싱 실패 시 원본 사용
                        }
                    } else if (displayValue === null || displayValue === undefined) {
                        displayValue = '0';
                    } else {
                        displayValue = String(displayValue);
                    }

                    const redisValue = {
                        id: `p-${point.id}`,
                        key: redisKey,
                        changed: false,
                        data_type: point.data_type || 'number',
                        device_id: String(device.id),
                        device_name: device.name,
                        point_id: point.id,
                        point_name: point.name,
                        quality: point.quality || 'good',
                        source: 'backend_init',
                        timestamp: Date.now(),
                        unit: point.unit || '',
                        value: displayValue,
                        access_mode: point.access_mode || 'read',
                        is_writable: point.is_writable || 0
                    };

                    await redis.set(redisKey, JSON.stringify(redisValue));
                    pointCount++;
                }

                // 3. 디바이스 상태 캐시 초기화
                const statusKey = `device:${device.id}:status`;
                const statusValue = {
                    connection_status: device.connection_status || 'disconnected',
                    last_communication: device.last_communication,
                    error_count: device.error_count || 0,
                    updated_at: new Date().toISOString()
                };
                await redis.set(statusKey, JSON.stringify(statusValue));
                deviceCount++;
            }

            // 4. 가상 포인트 초기화 (current_value가 있는 것만)
            let vpCount = 0;
            try {
                const virtualPoints = await knex('virtual_points')
                    .leftJoin('current_values', 'virtual_points.id', 'current_values.point_id')
                    .where('virtual_points.is_enabled', 1)
                    .andWhere(builder => {
                        builder.where('virtual_points.is_deleted', 0).orWhereNull('virtual_points.is_deleted');
                    })
                    .whereNotNull('current_values.current_value')
                    .select(
                        'virtual_points.id',
                        'virtual_points.name',
                        'virtual_points.device_id',
                        'virtual_points.unit',
                        'virtual_points.data_type',
                        'current_values.current_value',
                        'current_values.quality'
                    );

                for (const vp of virtualPoints) {
                    const deviceId = vp.device_id || 'virtual';
                    const redisKey = `device:${deviceId}:${vp.name}`;

                    let displayValue = vp.current_value;
                    if (displayValue && typeof displayValue === 'string' && displayValue.startsWith('{')) {
                        try {
                            const parsed = JSON.parse(displayValue);
                            if (parsed && parsed.value !== undefined) {
                                displayValue = String(parsed.value);
                            }
                        } catch (e) { }
                    } else {
                        displayValue = String(displayValue);
                    }

                    const redisValue = {
                        id: `vp-${vp.id}`,
                        key: redisKey,
                        changed: false,
                        data_type: vp.data_type || 'float',
                        device_id: String(deviceId),
                        device_name: 'Virtual',
                        point_id: vp.id,
                        point_name: vp.name,
                        quality: vp.quality || 'good',
                        source: 'backend_init_virtual',
                        timestamp: Date.now(),
                        unit: vp.unit || '',
                        value: displayValue,
                        access_mode: 'read',
                        is_writable: 0,
                        is_virtual: true
                    };

                    await redis.set(redisKey, JSON.stringify(redisValue));
                    vpCount++;
                }
            } catch (vpErr) {
                console.warn('[RealtimeService] 가상 포인트 초기화 실패:', vpErr.message);
            }

            return {
                initialized_devices: deviceCount,
                initialized_points: pointCount,
                initialized_virtual_points: vpCount,
                timestamp: new Date().toISOString()
            };
        }, 'RealtimeService.initializeRedisData');
    }

}

module.exports = new RealtimeService();
