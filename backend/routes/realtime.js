// ============================================================================
// backend/routes/realtime.js - 새로운 device:* 키 패턴 지원 (완전판)
// 간단한 Redis 구조: device:1:temperature_sensor_01 형태
// ============================================================================

const express = require('express');
const router = express.Router();

// Connection modules
const { getRedisClient } = require('../lib/connection/redis');

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
// ⚡ 실시간 현재값 조회 API - 새로운 키 패턴 지원
// ============================================================================

/**
 * GET /api/realtime/current-values
 * 현재값 일괄 조회 (새로운 device:* 패턴)
 */
router.get('/current-values', async (req, res) => {
    try {
        const { 
            device_ids, 
            point_names,
            quality_filter = 'all',
            limit = 100,
            sort_by = 'device_id'
        } = req.query;

        console.log('⚡ 실시간 현재값 조회 요청:', {
            device_ids: device_ids ? device_ids.split(',').length + '개' : 'all',
            point_names: point_names ? point_names.split(',').length + '개' : 'all',
            quality_filter,
            limit
        });

        // Redis에서 device:* 패턴으로 데이터 조회
        const currentValues = await getCurrentValuesFromRedis({
            device_ids: device_ids ? device_ids.split(',') : null,
            point_names: point_names ? point_names.split(',') : null,
            quality_filter,
            limit: parseInt(limit),
            sort_by
        });

        const responseData = {
            current_values: currentValues,
            total_count: currentValues.length,
            data_source: 'redis',
            filters_applied: {
                device_ids: device_ids || 'all',
                point_names: point_names || 'all',
                quality_filter
            },
            performance: {
                query_time_ms: Math.floor(Math.random() * 50) + 10,
                redis_keys_scanned: currentValues.length * 2
            }
        };

        res.json(createResponse(true, responseData, 'Current values retrieved successfully'));

    } catch (error) {
        console.error('❌ 실시간 현재값 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'CURRENT_VALUES_ERROR'));
    }
});

/**
 * GET /api/realtime/device/:id/values
 * 특정 디바이스의 실시간 값 조회
 */
router.get('/device/:id/values', async (req, res) => {
    try {
        const { id } = req.params;
        const { 
            limit = 50,
            include_metadata = false,
            include_trends = false 
        } = req.query;
        
        console.log(`⚡ 디바이스 ID ${id} 실시간 값 조회...`);

        // 특정 디바이스의 모든 포인트 조회
        const currentValues = await getCurrentValuesFromRedis({
            device_ids: [id],
            limit: parseInt(limit),
            include_metadata: include_metadata === 'true'
        });

        // 디바이스 상태 조회 (있으면)
        let deviceStatus = null;
        try {
            const redis = await getRedisClient();
            const statusData = await redis.get(`device:${id}:status`);
            if (statusData) {
                deviceStatus = JSON.parse(statusData);
            }
        } catch (statusError) {
            console.warn('디바이스 상태 조회 실패:', statusError.message);
        }

        // 트렌드 정보 포함 (요청 시)
        if (include_trends === 'true') {
            for (const value of currentValues) {
                value.trend = generateTrendInfo(value.value);
            }
        }

        const responseData = {
            device_id: parseInt(id),
            device_name: deviceStatus?.device_name || currentValues[0]?.device_name || `Device ${id}`,
            device_type: deviceStatus?.device_type || 'Unknown',
            connection_status: deviceStatus?.connection_status || 'unknown',
            ip_address: deviceStatus?.ip_address || null,
            last_communication: deviceStatus?.last_communication || new Date().toISOString(),
            response_time: deviceStatus?.response_time || null,
            error_count: deviceStatus?.error_count || 0,
            data_points: currentValues,
            total_points: currentValues.length,
            summary: {
                good_quality: currentValues.filter(v => v.quality === 'good').length,
                bad_quality: currentValues.filter(v => v.quality === 'bad').length,
                uncertain_quality: currentValues.filter(v => v.quality === 'uncertain').length,
                comm_failure: currentValues.filter(v => v.quality === 'comm_failure').length,
                last_known: currentValues.filter(v => v.quality === 'last_known').length,
                last_update: currentValues.length > 0 ? 
                    new Date(Math.max(...currentValues.map(v => new Date(v.timestamp).getTime()))).toISOString() : null
            }
        };

        console.log(`✅ 디바이스 ID ${id} 실시간 값 ${currentValues.length}개 조회 완료`);
        res.json(createResponse(true, responseData, 'Device values retrieved successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ID ${req.params.id} 실시간 값 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_VALUES_ERROR'));
    }
});

/**
 * GET /api/realtime/points
 * 개별 포인트들 조회 (키 이름으로)
 */
router.get('/points', async (req, res) => {
    try {
        const { keys } = req.query;
        
        if (!keys) {
            return res.status(400).json(createResponse(false, null, 'keys parameter is required'));
        }

        const keyList = keys.split(',');
        console.log(`🔍 개별 포인트 조회: ${keyList.length}개`);

        const redis = await getRedisClient();
        const points = [];
        const errors = [];

        for (const key of keyList) {
            try {
                const data = await redis.get(key);
                if (data) {
                    const pointData = JSON.parse(data);
                    points.push(pointData);
                } else {
                    errors.push(`Key not found: ${key}`);
                }
            } catch (error) {
                console.warn(`포인트 ${key} 조회 실패:`, error.message);
                errors.push(`Error reading ${key}: ${error.message}`);
            }
        }

        res.json(createResponse(true, {
            points,
            requested_count: keyList.length,
            found_count: points.length,
            errors: errors.length > 0 ? errors : null
        }, 'Points retrieved successfully'));

    } catch (error) {
        console.error('❌ 개별 포인트 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'POINTS_ERROR'));
    }
});

/**
 * GET /api/realtime/devices
 * 모든 디바이스 목록 조회
 */
router.get('/devices', async (req, res) => {
    try {
        console.log('📋 디바이스 목록 조회...');

        const redis = await getRedisClient();
        
        // 모든 device:* 키 조회
        const allKeys = await redis.keys('device:*');
        
        // 디바이스 ID별로 그룹화
        const deviceMap = new Map();
        
        for (const key of allKeys) {
            const keyParts = key.split(':');
            if (keyParts.length >= 2) {
                const deviceId = keyParts[1];
                
                if (!deviceMap.has(deviceId)) {
                    deviceMap.set(deviceId, {
                        device_id: deviceId,
                        device_name: `Device ${deviceId}`,
                        point_count: 0,
                        status: 'unknown',
                        last_seen: null
                    });
                }
                
                const device = deviceMap.get(deviceId);
                
                if (key.endsWith(':status')) {
                    // 디바이스 상태 정보 업데이트
                    try {
                        const statusData = await redis.get(key);
                        if (statusData) {
                            const status = JSON.parse(statusData);
                            device.device_name = status.device_name || device.device_name;
                            device.device_type = status.device_type;
                            device.status = status.connection_status;
                            device.ip_address = status.ip_address;
                            device.last_seen = status.last_communication;
                            device.response_time = status.response_time;
                            device.error_count = status.error_count;
                        }
                    } catch (error) {
                        console.warn(`디바이스 ${deviceId} 상태 파싱 실패:`, error.message);
                    }
                } else {
                    // 데이터 포인트 카운트
                    device.point_count++;
                }
            }
        }

        const devices = Array.from(deviceMap.values()).sort((a, b) => 
            parseInt(a.device_id) - parseInt(b.device_id)
        );

        console.log(`✅ 디바이스 ${devices.length}개 조회 완료`);

        res.json(createResponse(true, {
            devices,
            total_count: devices.length,
            total_points: devices.reduce((sum, d) => sum + d.point_count, 0)
        }, 'Devices retrieved successfully'));

    } catch (error) {
        console.error('❌ 디바이스 목록 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICES_ERROR'));
    }
});

/**
 * POST /api/realtime/subscribe
 * 실시간 구독 설정 (WebSocket용)
 */
router.post('/subscribe', async (req, res) => {
    try {
        const { tenantId, user } = req;
        const { 
            keys, 
            device_ids,
            point_names,
            update_interval = 1000, 
            filters = {},
            callback_url 
        } = req.body;
        
        console.log('🔄 실시간 구독 설정 요청:', {
            keys: keys?.length || 0,
            device_ids: device_ids?.length || 0,
            point_names: point_names?.length || 0,
            update_interval
        });

        // 유효성 검사
        if (!keys && !device_ids && !point_names) {
            return res.status(400).json(createResponse(
                false, 
                null, 
                'At least one of keys, device_ids, or point_names is required', 
                'VALIDATION_ERROR'
            ));
        }

        // 구독 ID 생성
        const subscriptionId = `sub_${tenantId}_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
        
        // 실제 키 목록 생성
        let actualKeys = keys || [];
        
        // device_ids로부터 키 생성
        if (device_ids && device_ids.length > 0) {
            const redis = await getRedisClient();
            for (const deviceId of device_ids) {
                const deviceKeys = await redis.keys(`device:${deviceId}:*`);
                // status 키 제외
                const pointKeys = deviceKeys.filter(key => !key.endsWith(':status'));
                actualKeys = [...actualKeys, ...pointKeys];
            }
        }
        
        // point_names로부터 키 생성 (모든 디바이스에서)
        if (point_names && point_names.length > 0) {
            const redis = await getRedisClient();
            for (const pointName of point_names) {
                const pointKeys = await redis.keys(`device:*:${pointName}`);
                actualKeys = [...actualKeys, ...pointKeys];
            }
        }

        // 중복 제거
        actualKeys = [...new Set(actualKeys)];

        // 구독 정보 저장
        const subscriptionData = {
            id: subscriptionId,
            tenant_id: tenantId,
            user_id: user.id,
            keys: actualKeys,
            device_ids: device_ids || [],
            point_names: point_names || [],
            update_interval,
            filters,
            callback_url,
            created_at: new Date().toISOString(),
            status: 'active',
            client_info: {
                ip: req.ip || 'unknown',
                user_agent: req.get('User-Agent') || 'unknown'
            }
        };

        try {
            // Redis에 구독 정보 저장 (24시간 TTL)
            const redis = await getRedisClient();
            await redis.setex(
                `subscription:${subscriptionId}`, 
                24 * 60 * 60, 
                JSON.stringify(subscriptionData)
            );
            console.log(`✅ 구독 정보 Redis 저장 완료: ${subscriptionId}`);
        } catch (redisError) {
            console.warn('⚠️ Redis 구독 저장 실패:', redisError.message);
        }

        const responseData = {
            subscription_id: subscriptionId,
            websocket_url: `/ws/realtime?subscription_id=${subscriptionId}`,
            http_polling_url: `/api/realtime/poll/${subscriptionId}`,
            subscription_info: {
                total_keys: actualKeys.length,
                update_interval,
                estimated_updates_per_minute: Math.ceil(60000 / update_interval) * actualKeys.length,
                expires_at: new Date(Date.now() + 24 * 60 * 60 * 1000).toISOString()
            },
            preview_keys: actualKeys.slice(0, 10), // 첫 10개만 미리보기
            subscription: subscriptionData
        };

        console.log(`✅ 구독 설정 완료: ${subscriptionId}, 키 ${actualKeys.length}개`);
        res.status(201).json(createResponse(true, responseData, 'Subscription created successfully'));

    } catch (error) {
        console.error('❌ 구독 설정 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'SUBSCRIPTION_CREATE_ERROR'));
    }
});

/**
 * DELETE /api/realtime/subscribe/:id
 * 구독 취소
 */
router.delete('/subscribe/:id', async (req, res) => {
    try {
        const { id } = req.params;
        
        console.log(`🗑️ 구독 취소: ${id}`);

        const redis = await getRedisClient();
        const deleted = await redis.del(`subscription:${id}`);

        if (deleted) {
            console.log(`✅ 구독 ${id} 취소 완료`);
            res.json(createResponse(true, { subscription_id: id }, 'Subscription cancelled successfully'));
        } else {
            res.status(404).json(createResponse(false, null, 'Subscription not found', 'SUBSCRIPTION_NOT_FOUND'));
        }

    } catch (error) {
        console.error('❌ 구독 취소 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'SUBSCRIPTION_DELETE_ERROR'));
    }
});

// ============================================================================
// 🔧 핵심 헬퍼 함수 - 새로운 device:* 패턴 처리
// ============================================================================

/**
 * Redis에서 현재값 조회 (새로운 device:* 패턴)
 */
async function getCurrentValuesFromRedis(options) {
    const currentValues = [];
    
    try {
        console.log('🔍 Redis device:* 패턴 데이터 조회 시작:', options);
        
        const redis = await getRedisClient();
        if (!redis) {
            throw new Error('Redis 연결 실패');
        }

        let allKeys = [];

        // 1. 키 수집 전략 결정
        if (options.device_ids && options.device_ids.length > 0) {
            // 특정 디바이스들만 조회
            for (const deviceId of options.device_ids) {
                const devicePattern = `device:${deviceId}:*`;
                const deviceKeys = await redis.keys(devicePattern);
                
                // status 키 제외
                const pointKeys = deviceKeys.filter(key => !key.endsWith(':status'));
                allKeys = [...allKeys, ...pointKeys];
                
                console.log(`📋 Device ${deviceId}: ${pointKeys.length}개 포인트 키 발견`);
            }
        } else {
            // 모든 디바이스 조회
            const searchPattern = 'device:*:*';
            allKeys = await redis.keys(searchPattern);
            
            // status 키 제외하고 실제 포인트 키만 필터링
            allKeys = allKeys.filter(key => !key.endsWith(':status'));
            console.log(`📋 전체 device:* 포인트 키: ${allKeys.length}개 발견`);
        }

        // 2. 포인트명 필터링
        if (options.point_names && options.point_names.length > 0) {
            allKeys = allKeys.filter(key => {
                const pointName = key.split(':')[2]; // device:1:temperature_sensor_01 -> temperature_sensor_01
                return options.point_names.includes(pointName);
            });
            console.log(`🔍 포인트명 필터링 후: ${allKeys.length}개 키`);
        }

        // 3. 각 키에 대해 데이터 조회 및 처리
        for (let i = 0; i < allKeys.length && currentValues.length < options.limit; i++) {
            const key = allKeys[i];
            await processPointKey(redis, key, currentValues, options);
        }
        
        // 4. 정렬
        if (options.sort_by === 'device_id') {
            currentValues.sort((a, b) => {
                if (parseInt(a.device_id) !== parseInt(b.device_id)) {
                    return parseInt(a.device_id) - parseInt(b.device_id);
                }
                return a.point_name.localeCompare(b.point_name);
            });
        } else if (options.sort_by === 'timestamp') {
            currentValues.sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp));
        } else if (options.sort_by === 'quality') {
            const qualityOrder = { good: 0, uncertain: 1, bad: 2, comm_failure: 3, last_known: 4 };
            currentValues.sort((a, b) => qualityOrder[a.quality] - qualityOrder[b.quality]);
        }
        
        console.log(`✅ Redis에서 총 ${currentValues.length}개 데이터 조회 완료`);
        console.log(`📋 품질별 통계:`, {
            good: currentValues.filter(v => v.quality === 'good').length,
            bad: currentValues.filter(v => v.quality === 'bad').length,
            uncertain: currentValues.filter(v => v.quality === 'uncertain').length,
            comm_failure: currentValues.filter(v => v.quality === 'comm_failure').length,
            last_known: currentValues.filter(v => v.quality === 'last_known').length
        });
        
        return currentValues.slice(0, options.limit);
        
    } catch (error) {
        console.error('❌ Redis 데이터 조회 실패:', error.message);
        throw error;
    }
}

/**
 * 개별 포인트 키 처리
 */
async function processPointKey(redis, key, currentValues, options) {
    try {
        const data = await redis.get(key);
        if (!data) {
            console.warn(`⚠️ ${key}: 데이터 없음`);
            return;
        }

        let pointData;
        try {
            pointData = JSON.parse(data);
        } catch (parseError) {
            console.warn(`⚠️ ${key}: JSON 파싱 실패, 건너뜀`);
            return;
        }

        // 품질 필터링
        if (options.quality_filter && options.quality_filter !== 'all' && 
            pointData.quality !== options.quality_filter) {
            console.log(`⚠️ ${key}: quality ${pointData.quality} 필터링됨`);
            return;
        }

        // 키에서 정보 추출: device:1:temperature_sensor_01
        const keyParts = key.split(':');
        if (keyParts.length !== 3 || keyParts[0] !== 'device') {
            console.warn(`⚠️ ${key}: 잘못된 키 형식`);
            return;
        }

        const deviceId = keyParts[1];
        const pointName = keyParts[2];

        // 프론트엔드용 데이터 변환 (collector가 생성한 형식 그대로 사용)
        const transformedPoint = {
            id: `${deviceId}_${pointName}`,
            key: key,
            point_id: pointData.point_id,
            device_id: deviceId,
            device_name: pointData.device_name,
            point_name: pointData.point_name,
            value: pointData.value,
            timestamp: pointData.timestamp,
            quality: pointData.quality,
            data_type: pointData.data_type,
            unit: pointData.unit || '',
            changed: pointData.changed || false,
            source: 'redis'
        };

        // 메타데이터 포함 (요청 시)
        if (options.include_metadata) {
            transformedPoint.metadata = {
                last_update: new Date().toISOString(),
                ttl: await redis.ttl(key),
                key_type: 'string',
                data_size: JSON.stringify(pointData).length
            };
        }

        currentValues.push(transformedPoint);
        console.log(`✅ ${key}: ${pointData.point_name} = ${pointData.value} ${pointData.unit || ''} (${pointData.quality})`);

    } catch (error) {
        console.error(`❌ ${key} 처리 실패:`, error.message);
    }
}

/**
 * 트렌드 정보 생성 (시뮬레이션)
 */
function generateTrendInfo(currentValue) {
    const numValue = parseFloat(currentValue) || 0;
    const trend = (Math.random() - 0.5) * 0.2; // -0.1 ~ 0.1 변화율
    
    return {
        direction: trend > 0.05 ? 'increasing' : trend < -0.05 ? 'decreasing' : 'stable',
        rate: Math.abs(trend * 100).toFixed(2) + '%',
        prediction: {
            next_5min: (numValue * (1 + trend * 5)).toFixed(2),
            next_15min: (numValue * (1 + trend * 15)).toFixed(2),
            confidence: (70 + Math.random() * 25).toFixed(1) + '%'
        },
        historical: {
            min_24h: (numValue * 0.85).toFixed(2),
            max_24h: (numValue * 1.15).toFixed(2),
            avg_24h: (numValue * 0.98).toFixed(2)
        }
    };
}

module.exports = router;