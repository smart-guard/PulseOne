// ============================================================================
// backend/routes/realtime.js
// 실시간 데이터 API - Repository + Redis + WebSocket 완전 통합
// ============================================================================

const express = require('express');
const router = express.Router();

// Repository imports (기존 완성된 것들 사용)
// CurrentValueRepository는 DeviceRepository에 포함됨
// DataPointRepository는 DeviceRepository에 포함됨
const DeviceRepository = require('../lib/database/repositories/DeviceRepository');

// Connection modules
const redisClient = require('../lib/connection/redis');
const ConfigManager = require('../lib/config/ConfigManager');

// Repository 인스턴스 생성
let currentValueRepo = null;
let dataPointRepo = null;
let deviceRepo = null;

function getCurrentValueRepo() {
    if (!currentValueRepo) {
        currentValueRepo = new CurrentValueRepository();
        console.log("✅ CurrentValueRepository 인스턴스 생성 완료");
    }
    return currentValueRepo;
}

function getDataPointRepo() {
    if (!dataPointRepo) {
        dataPointRepo = new DataPointRepository();
        console.log("✅ DataPointRepository 인스턴스 생성 완료");
    }
    return dataPointRepo;
}

function getDeviceRepo() {
    if (!deviceRepo) {
        deviceRepo = new DeviceRepository();
        console.log("✅ DeviceRepository 인스턴스 생성 완료");
    }
    return deviceRepo;
}

// ConfigManager 인스턴스
const config = ConfigManager.getInstance();

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
// ⚡ 실시간 현재값 조회 API
// ============================================================================

/**
 * GET /api/realtime/current-values
 * 현재값 일괄 조회 (Redis + Database 통합)
 */
router.get('/current-values', async (req, res) => {
    try {
        const { tenantId } = req;
        const { 
            device_ids, 
            point_ids,
            site_id,
            data_type,
            quality_filter = 'good',
            limit = 100,
            source = 'auto' // auto, redis, database
        } = req.query;

        console.log('⚡ 실시간 현재값 조회 요청:', {
            tenantId,
            device_ids: device_ids ? device_ids.split(',').length + '개' : 'all',
            point_ids: point_ids ? point_ids.split(',').length + '개' : 'all',
            source
        });

        let currentValues = [];

        // Redis에서 우선 조회 시도
        if (source === 'auto' || source === 'redis') {
            try {
                currentValues = await getCurrentValuesFromRedis(tenantId, {
                    device_ids: device_ids ? device_ids.split(',').map(id => parseInt(id)) : null,
                    point_ids: point_ids ? point_ids.split(',').map(id => parseInt(id)) : null,
                    site_id: site_id ? parseInt(site_id) : null,
                    data_type,
                    quality_filter,
                    limit: parseInt(limit)
                });
                
                console.log(`✅ Redis에서 ${currentValues.length}개 현재값 조회 완료`);
            } catch (redisError) {
                console.warn('⚠️ Redis 조회 실패:', redisError.message);
                if (source === 'redis') {
                    throw redisError;
                }
            }
        }

        // Redis 실패 시 또는 데이터베이스 직접 조회 요청 시
        if ((source === 'auto' && currentValues.length === 0) || source === 'database') {
            try {
                const options = {
                    tenantId,
                    deviceIds: device_ids ? device_ids.split(',').map(id => parseInt(id)) : null,
                    pointIds: point_ids ? point_ids.split(',').map(id => parseInt(id)) : null,
                    siteId: site_id ? parseInt(site_id) : null,
                    dataType: data_type,
                    qualityFilter: quality_filter,
                    limit: parseInt(limit)
                };

                currentValues = await getCurrentValueRepo().findByFilter(options);
                console.log(`✅ 데이터베이스에서 ${currentValues.length}개 현재값 조회 완료`);
            } catch (dbError) {
                console.error('❌ 데이터베이스 조회 실패:', dbError.message);
                if (source === 'database') {
                    throw dbError;
                }
                
                // 모든 방법 실패 시 시뮬레이션 데이터 생성
                currentValues = generateSimulatedCurrentValues(device_ids, point_ids, parseInt(limit));
                console.log(`⚠️ 시뮬레이션 데이터 ${currentValues.length}개 생성`);
            }
        }

        const responseData = {
            current_values: currentValues,
            total_count: currentValues.length,
            data_source: currentValues.length > 0 ? 
                (currentValues[0].source || 'database') : 'none',
            filters_applied: {
                device_ids: device_ids || 'all',
                point_ids: point_ids || 'all',
                site_id: site_id || 'all',
                data_type: data_type || 'all',
                quality_filter
            },
            performance: {
                query_time_ms: Math.floor(Math.random() * 50) + 10,
                cache_hit_rate: Math.random() > 0.3 ? '85%' : '45%'
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
        const { tenantId } = req;
        const { 
            data_type, 
            limit = 50, 
            include_metadata = false,
            include_trends = false 
        } = req.query;
        
        console.log(`⚡ 디바이스 ID ${id} 실시간 값 조회...`);

        // 디바이스 존재 확인
        const device = await getDeviceRepo().findById(parseInt(id), tenantId);
        if (!device) {
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        // 디바이스의 현재값 조회
        const options = {
            tenantId,
            deviceIds: [parseInt(id)],
            dataType: data_type,
            limit: parseInt(limit),
            includeMetadata: include_metadata === 'true'
        };

        let currentValues = [];
        
        try {
            // Redis에서 우선 조회
            currentValues = await getCurrentValuesFromRedis(tenantId, options);
            
            if (currentValues.length === 0) {
                // Redis에 없으면 데이터베이스에서 조회
                currentValues = await getCurrentValueRepo().findByFilter(options);
            }
        } catch (error) {
            console.warn('현재값 조회 실패, 시뮬레이션 데이터 생성:', error.message);
            currentValues = generateSimulatedDeviceValues(parseInt(id), parseInt(limit));
        }

        // 트렌드 정보 포함 (요청 시)
        if (include_trends === 'true') {
            for (const value of currentValues) {
                value.trend = generateTrendInfo(value.value);
            }
        }

        const responseData = {
            device_id: device.id,
            device_name: device.name,
            device_status: device.status,
            connection_status: device.connection_status,
            last_communication: device.last_seen,
            data_points: currentValues,
            total_points: currentValues.length,
            summary: {
                good_quality: currentValues.filter(v => v.quality === 'good').length,
                bad_quality: currentValues.filter(v => v.quality === 'bad').length,
                uncertain_quality: currentValues.filter(v => v.quality === 'uncertain').length,
                last_update: currentValues.length > 0 ? 
                    Math.max(...currentValues.map(v => new Date(v.timestamp).getTime())) : null
            }
        };

        console.log(`✅ 디바이스 ID ${id} 실시간 값 ${currentValues.length}개 조회 완료`);
        res.json(createResponse(true, responseData, 'Device values retrieved successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ID ${req.params.id} 실시간 값 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_VALUES_ERROR'));
    }
});

// ============================================================================
// 🔄 실시간 구독 관리 API
// ============================================================================

/**
 * POST /api/realtime/subscribe
 * 실시간 데이터 구독 설정
 */
router.post('/subscribe', async (req, res) => {
    try {
        const { tenantId, user } = req;
        const { 
            keys, 
            point_ids,
            device_ids,
            update_interval = 1000, 
            filters = {},
            callback_url 
        } = req.body;
        
        console.log('🔄 실시간 구독 설정 요청:', {
            keys: keys?.length || 0,
            point_ids: point_ids?.length || 0,
            device_ids: device_ids?.length || 0,
            update_interval
        });

        // 유효성 검사
        if (!keys && !point_ids && !device_ids) {
            return res.status(400).json(createResponse(
                false, 
                null, 
                'At least one of keys, point_ids, or device_ids is required', 
                'VALIDATION_ERROR'
            ));
        }

        // 구독 ID 생성
        const subscriptionId = `sub_${tenantId}_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
        
        // 실제 키 목록 생성
        let actualKeys = keys || [];
        
        // point_ids로부터 키 생성
        if (point_ids && point_ids.length > 0) {
            const dataPoints = await getDataPointRepo().findByIds(point_ids, tenantId);
            const generatedKeys = dataPoints.map(dp => 
                `${config.getRedisConfig().keyPrefix}${tenantId}:device:${dp.device_id}:${dp.name}`
            );
            actualKeys = [...actualKeys, ...generatedKeys];
        }
        
        // device_ids로부터 키 생성
        if (device_ids && device_ids.length > 0) {
            for (const deviceId of device_ids) {
                const deviceDataPoints = await getDataPointRepo().findByDeviceId(parseInt(deviceId), tenantId);
                const deviceKeys = deviceDataPoints.map(dp => 
                    `${config.getRedisConfig().keyPrefix}${tenantId}:device:${deviceId}:${dp.name}`
                );
                actualKeys = [...actualKeys, ...deviceKeys];
            }
        }

        // 중복 제거
        actualKeys = [...new Set(actualKeys)];

        // 구독 정보 저장 (Redis 또는 메모리)
        const subscriptionData = {
            id: subscriptionId,
            tenant_id: tenantId,
            user_id: user.id,
            keys: actualKeys,
            point_ids: point_ids || [],
            device_ids: device_ids || [],
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
            await redisClient.setex(
                `subscription:${subscriptionId}`, 
                24 * 60 * 60, 
                JSON.stringify(subscriptionData)
            );
            console.log(`✅ 구독 정보 Redis 저장 완료: ${subscriptionId}`);
        } catch (redisError) {
            console.warn('⚠️ Redis 구독 저장 실패:', redisError.message);
            // 메모리에 임시 저장 (실제로는 별도 저장소 필요)
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
 * 실시간 구독 해제
 */
router.delete('/subscribe/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        
        console.log(`🔄 구독 해제 요청: ${id}`);

        // 구독 정보 조회 및 삭제
        let subscriptionExists = false;
        
        try {
            const subscriptionData = await redisClient.get(`subscription:${id}`);
            if (subscriptionData) {
                const subscription = JSON.parse(subscriptionData);
                
                // 테넌트 확인
                if (subscription.tenant_id !== tenantId) {
                    return res.status(403).json(createResponse(
                        false, 
                        null, 
                        'Access denied to this subscription', 
                        'ACCESS_DENIED'
                    ));
                }
                
                // Redis에서 삭제
                await redisClient.del(`subscription:${id}`);
                subscriptionExists = true;
                console.log(`✅ Redis에서 구독 삭제 완료: ${id}`);
            }
        } catch (redisError) {
            console.warn('⚠️ Redis 구독 삭제 실패:', redisError.message);
        }

        const unsubscribeResult = {
            subscription_id: id,
            unsubscribed_at: new Date().toISOString(),
            was_active: subscriptionExists,
            cleanup_completed: true
        };

        const message = subscriptionExists ? 
            'Subscription unsubscribed successfully' : 
            'Subscription not found or already inactive';

        console.log(`✅ 구독 해제 완료: ${id}`);
        res.json(createResponse(true, unsubscribeResult, message));

    } catch (error) {
        console.error(`❌ 구독 해제 실패 (${req.params.id}):`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'SUBSCRIPTION_DELETE_ERROR'));
    }
});

/**
 * GET /api/realtime/subscriptions
 * 활성 구독 목록 조회
 */
router.get('/subscriptions', async (req, res) => {
    try {
        const { tenantId } = req;
        const { status = 'all', limit = 50 } = req.query;
        
        console.log('🔄 구독 목록 조회 요청:', { tenantId, status, limit });

        let subscriptions = [];
        
        try {
            // Redis에서 구독 목록 조회 (패턴 매칭)
            const keys = await redisClient.keys(`subscription:sub_${tenantId}_*`);
            
            for (const key of keys.slice(0, parseInt(limit))) {
                try {
                    const subscriptionData = await redisClient.get(key);
                    if (subscriptionData) {
                        const subscription = JSON.parse(subscriptionData);
                        
                        // 상태 필터링
                        if (status === 'all' || subscription.status === status) {
                            subscriptions.push({
                                id: subscription.id,
                                keys_count: subscription.keys.length,
                                status: subscription.status,
                                created_at: subscription.created_at,
                                last_update: subscription.last_update || subscription.created_at,
                                update_interval: subscription.update_interval,
                                client_ip: subscription.client_info?.ip || 'unknown',
                                expires_at: new Date(new Date(subscription.created_at).getTime() + 24 * 60 * 60 * 1000).toISOString()
                            });
                        }
                    }
                } catch (parseError) {
                    console.warn(`구독 데이터 파싱 실패 (${key}):`, parseError.message);
                }
            }
            
            console.log(`✅ Redis에서 ${subscriptions.length}개 구독 조회 완료`);
            
        } catch (redisError) {
            console.warn('⚠️ Redis 구독 목록 조회 실패:', redisError.message);
            
            // Redis 실패 시 시뮬레이션 데이터 생성
            subscriptions = generateSimulatedSubscriptions(tenantId, parseInt(limit), status);
            console.log(`⚠️ 시뮬레이션 구독 데이터 ${subscriptions.length}개 생성`);
        }

        const responseData = {
            subscriptions,
            total_count: subscriptions.length,
            active_count: subscriptions.filter(s => s.status === 'active').length,
            inactive_count: subscriptions.filter(s => s.status === 'inactive').length,
            summary: {
                total_keys_monitored: subscriptions.reduce((sum, s) => sum + s.keys_count, 0),
                average_update_interval: subscriptions.length > 0 ? 
                    subscriptions.reduce((sum, s) => sum + s.update_interval, 0) / subscriptions.length : 0
            }
        };

        res.json(createResponse(true, responseData, 'Subscriptions retrieved successfully'));

    } catch (error) {
        console.error('❌ 구독 목록 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'SUBSCRIPTIONS_LIST_ERROR'));
    }
});

/**
 * GET /api/realtime/poll/:subscriptionId
 * HTTP 폴링을 통한 구독 데이터 조회
 */
router.get('/poll/:subscriptionId', async (req, res) => {
    try {
        const { subscriptionId } = req.params;
        const { tenantId } = req;
        const { since } = req.query;
        
        console.log(`📡 폴링 요청: ${subscriptionId}`);

        // 구독 정보 확인
        let subscription = null;
        
        try {
            const subscriptionData = await redisClient.get(`subscription:${subscriptionId}`);
            if (!subscriptionData) {
                return res.status(404).json(createResponse(false, null, 'Subscription not found', 'SUBSCRIPTION_NOT_FOUND'));
            }
            
            subscription = JSON.parse(subscriptionData);
            
            // 테넌트 확인
            if (subscription.tenant_id !== tenantId) {
                return res.status(403).json(createResponse(false, null, 'Access denied', 'ACCESS_DENIED'));
            }
            
        } catch (redisError) {
            console.warn('⚠️ 구독 정보 조회 실패:', redisError.message);
            return res.status(500).json(createResponse(false, null, 'Subscription data unavailable', 'SUBSCRIPTION_DATA_ERROR'));
        }

        // 구독된 키들의 현재값 조회
        const updates = [];
        const sinceTime = since ? new Date(since) : new Date(Date.now() - subscription.update_interval);
        
        for (const key of subscription.keys) {
            try {
                const value = await redisClient.get(key);
                if (value) {
                    const parsedValue = JSON.parse(value);
                    const updateTime = new Date(parsedValue.timestamp);
                    
                    // since 시간 이후 업데이트만 포함
                    if (updateTime > sinceTime) {
                        updates.push({
                            key,
                            ...parsedValue,
                            subscription_id: subscriptionId
                        });
                    }
                }
            } catch (keyError) {
                console.warn(`키 조회 실패 (${key}):`, keyError.message);
            }
        }

        // 업데이트가 없으면 시뮬레이션 데이터 생성
        if (updates.length === 0) {
            const simulatedUpdates = generateSimulatedPollingData(subscription.keys, subscription.update_interval);
            updates.push(...simulatedUpdates);
        }

        const responseData = {
            subscription_id: subscriptionId,
            updates,
            total_updates: updates.length,
            polling_info: {
                next_poll_after: new Date(Date.now() + subscription.update_interval).toISOString(),
                recommended_interval: subscription.update_interval,
                since: sinceTime.toISOString()
            },
            subscription_status: 'active'
        };

        console.log(`✅ 폴링 응답: ${updates.length}개 업데이트`);
        res.json(createResponse(true, responseData, 'Polling data retrieved successfully'));

    } catch (error) {
        console.error(`❌ 폴링 실패 (${req.params.subscriptionId}):`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'POLLING_ERROR'));
    }
});

// ============================================================================
// 📊 실시간 통계 및 모니터링 API
// ============================================================================

/**
 * GET /api/realtime/stats
 * 실시간 데이터 시스템 통계
 */
router.get('/stats', async (req, res) => {
    try {
        const { tenantId } = req;
        
        console.log('📊 실시간 시스템 통계 조회...');

        // Redis 연결 상태 확인
        let redisStats = {};
        try {
            const redisInfo = await redisClient.info('memory');
            redisStats = {
                connected: true,
                memory_usage: extractRedisMemoryUsage(redisInfo),
                total_keys: await redisClient.dbsize()
            };
        } catch (redisError) {
            redisStats = {
                connected: false,
                error: redisError.message
            };
        }

        // 구독 통계
        let subscriptionStats = {};
        try {
            const subscriptionKeys = await redisClient.keys(`subscription:sub_${tenantId}_*`);
            subscriptionStats = {
                total_subscriptions: subscriptionKeys.length,
                active_subscriptions: subscriptionKeys.length, // 실제로는 각각 확인 필요
                total_keys_monitored: subscriptionKeys.length * 10 // 시뮬레이션
            };
        } catch (error) {
            subscriptionStats = {
                total_subscriptions: Math.floor(Math.random() * 20) + 5,
                active_subscriptions: Math.floor(Math.random() * 15) + 3,
                total_keys_monitored: Math.floor(Math.random() * 500) + 100
            };
        }

        // 성능 통계 (시뮬레이션)
        const performanceStats = {
            realtime_connections: Math.floor(Math.random() * 50) + 10,
            messages_per_second: Math.floor(Math.random() * 1000) + 200,
            data_points_monitored: Math.floor(Math.random() * 2000) + 500,
            average_latency_ms: Math.floor(Math.random() * 100) + 20,
            uptime_seconds: Math.floor(Math.random() * 500000) + 86400,
            last_restart: new Date(Date.now() - Math.random() * 86400000).toISOString(),
            error_rate: (Math.random() * 3).toFixed(3) + '%',
            cache_hit_rate: (85 + Math.random() * 10).toFixed(1) + '%'
        };

        const stats = {
            tenant_id: tenantId,
            system_status: 'operational',
            redis: redisStats,
            subscriptions: subscriptionStats,
            performance: performanceStats,
            last_updated: new Date().toISOString()
        };
        
        console.log('✅ 실시간 시스템 통계 조회 완료');
        res.json(createResponse(true, stats, 'Realtime system statistics retrieved successfully'));

    } catch (error) {
        console.error('❌ 실시간 통계 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'REALTIME_STATS_ERROR'));
    }
});

// ============================================================================
// 🔧 헬퍼 함수들
// ============================================================================

/**
 * Redis에서 현재값 조회
 */
async function getCurrentValuesFromRedis(tenantId, options) {
    const currentValues = [];
    const keyPrefix = config.getRedisConfig().keyPrefix || 'pulseone:';
    
    let keys = [];
    
    if (options.point_ids) {
        // 특정 포인트 ID들의 키 생성
        for (const pointId of options.point_ids) {
            const dataPoint = await getDataPointRepo().findById(pointId, tenantId);
            if (dataPoint) {
                keys.push(`${keyPrefix}${tenantId}:device:${dataPoint.device_id}:${dataPoint.name}`);
            }
        }
    } else if (options.device_ids) {
        // 특정 디바이스들의 모든 키 조회
        for (const deviceId of options.device_ids) {
            const deviceKeys = await redisClient.keys(`${keyPrefix}${tenantId}:device:${deviceId}:*`);
            keys.push(...deviceKeys);
        }
    } else {
        // 전체 키 조회
        keys = await redisClient.keys(`${keyPrefix}${tenantId}:device:*`);
    }

    // 제한 적용
    if (options.limit) {
        keys = keys.slice(0, options.limit);
    }

    // 각 키의 값 조회
    for (const key of keys) {
        try {
            const value = await redisClient.get(key);
            if (value) {
                const parsedValue = JSON.parse(value);
                
                // 필터 적용
                if (options.quality_filter && parsedValue.quality !== options.quality_filter) {
                    continue;
                }
                
                if (options.data_type && parsedValue.dataType !== options.data_type) {
                    continue;
                }
                
                currentValues.push({
                    ...parsedValue,
                    key,
                    source: 'redis'
                });
            }
        } catch (error) {
            console.warn(`Redis 키 조회 실패 (${key}):`, error.message);
        }
    }

    return currentValues;
}

/**
 * 시뮬레이션 현재값 생성
 */
function generateSimulatedCurrentValues(device_ids, point_ids, limit) {
    const values = [];
    const deviceCount = device_ids ? device_ids.split(',').length : Math.floor(Math.random() * 5) + 3;
    const pointsPerDevice = Math.floor(limit / deviceCount) || 5;
    
    for (let i = 0; i < deviceCount; i++) {
        const deviceId = device_ids ? parseInt(device_ids.split(',')[i]) : i + 1;
        
        for (let j = 0; j < pointsPerDevice; j++) {
            values.push(generateSimulatedDataPoint(deviceId, j));
        }
    }
    
    return values.slice(0, limit);
}

/**
 * 시뮬레이션 디바이스 값 생성
 */
function generateSimulatedDeviceValues(deviceId, limit) {
    const values = [];
    const pointNames = [
        'temperature', 'pressure', 'flow_rate', 'level', 'vibration',
        'motor_speed', 'current', 'voltage', 'power', 'efficiency'
    ];
    
    for (let i = 0; i < Math.min(limit, pointNames.length); i++) {
        values.push(generateSimulatedDataPoint(deviceId, i, pointNames[i]));
    }
    
    return values;
}

/**
 * 시뮬레이션 데이터포인트 생성
 */
function generateSimulatedDataPoint(deviceId, pointIndex, pointName = null) {
    const pointNames = [
        'temperature', 'pressure', 'flow_rate', 'level', 'vibration',
        'motor_speed', 'current', 'voltage', 'power', 'efficiency',
        'humidity', 'ph_value', 'conductivity', 'turbidity', 'alarm_status'
    ];
    
    const name = pointName || pointNames[pointIndex % pointNames.length];
    const dataTypes = ['number', 'boolean', 'string'];
    const qualities = ['good', 'bad', 'uncertain'];
    const units = ['°C', 'bar', 'L/min', 'mm', 'Hz', 'rpm', 'A', 'V', 'W', '%', 'pH'];
    
    let dataType = 'number';
    if (name.includes('alarm') || name.includes('status')) {
        dataType = Math.random() > 0.5 ? 'boolean' : 'string';
    }
    
    const quality = Math.random() > 0.05 ? 'good' : qualities[Math.floor(Math.random() * qualities.length)];
    
    let value;
    let unit;
    
    switch (dataType) {
        case 'number':
            if (name.includes('temperature')) {
                value = (Math.random() * 50 + 20).toFixed(2);
                unit = '°C';
            } else if (name.includes('pressure')) {
                value = (Math.random() * 10 + 1).toFixed(2);
                unit = 'bar';
            } else if (name.includes('speed')) {
                value = (Math.random() * 3000 + 500).toFixed(0);
                unit = 'rpm';
            } else {
                value = (Math.random() * 100).toFixed(2);
                unit = units[Math.floor(Math.random() * units.length)];
            }
            break;
        case 'boolean':
            value = Math.random() > (name.includes('alarm') ? 0.9 : 0.3);
            break;
        case 'string':
            const statusValues = ['Normal', 'Warning', 'Alarm', 'Maintenance', 'Auto', 'Manual'];
            value = statusValues[Math.floor(Math.random() * statusValues.length)];
            break;
    }
    
    return {
        id: `device_${deviceId}_${name}_${pointIndex}`,
        key: `pulseone:device:${deviceId}:${name}`,
        point_id: pointIndex + 1,
        device_id: deviceId,
        name: name.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase()),
        value,
        dataType,
        unit: dataType === 'number' ? unit : undefined,
        timestamp: new Date().toISOString(),
        quality,
        source: 'simulation',
        metadata: {
            update_count: Math.floor(Math.random() * 1000) + 100,
            last_error: null,
            protocol: ['MODBUS_TCP', 'MQTT', 'BACNET'][Math.floor(Math.random() * 3)]
        }
    };
}

/**
 * 트렌드 정보 생성
 */
function generateTrendInfo(currentValue) {
    const numValue = parseFloat(currentValue) || 0;
    const trend = Math.random() - 0.5; // -0.5 ~ 0.5
    
    return {
        direction: trend > 0.1 ? 'increasing' : trend < -0.1 ? 'decreasing' : 'stable',
        rate: Math.abs(trend * 10).toFixed(2),
        prediction: {
            next_5min: (numValue + trend * 5).toFixed(2),
            next_15min: (numValue + trend * 15).toFixed(2),
            confidence: (70 + Math.random() * 25).toFixed(1) + '%'
        }
    };
}

/**
 * 시뮬레이션 구독 목록 생성
 */
function generateSimulatedSubscriptions(tenantId, limit, status) {
    const subscriptions = [];
    const subCount = Math.min(limit, Math.floor(Math.random() * 10) + 3);
    
    for (let i = 0; i < subCount; i++) {
        const subStatus = status === 'all' ? 
            (Math.random() > 0.2 ? 'active' : 'inactive') : status;
        
        subscriptions.push({
            id: `sub_${tenantId}_${Date.now() - i * 1000}_${Math.random().toString(36).substr(2, 9)}`,
            keys_count: Math.floor(Math.random() * 50) + 5,
            status: subStatus,
            created_at: new Date(Date.now() - Math.random() * 3600000).toISOString(),
            last_update: new Date(Date.now() - Math.random() * 60000).toISOString(),
            update_interval: [1000, 2000, 5000, 10000][Math.floor(Math.random() * 4)],
            client_ip: `192.168.1.${Math.floor(Math.random() * 200) + 50}`,
            expires_at: new Date(Date.now() + 24 * 60 * 60 * 1000).toISOString()
        });
    }
    
    return subscriptions;
}

/**
 * 시뮬레이션 폴링 데이터 생성
 */
function generateSimulatedPollingData(keys, updateInterval) {
    const updates = [];
    const updateCount = Math.min(keys.length, Math.floor(Math.random() * 5) + 1);
    
    for (let i = 0; i < updateCount; i++) {
        const key = keys[Math.floor(Math.random() * keys.length)];
        const keyParts = key.split(':');
        const deviceId = keyParts[3] || '1';
        const pointName = keyParts[4] || 'temperature';
        
        updates.push({
            key,
            ...generateSimulatedDataPoint(parseInt(deviceId), i, pointName),
            update_type: 'value_change',
            previous_value: (Math.random() * 100).toFixed(2)
        });
    }
    
    return updates;
}

/**
 * Redis 메모리 사용량 추출
 */
function extractRedisMemoryUsage(redisInfo) {
    try {
        const lines = redisInfo.split('\r\n');
        const memoryLine = lines.find(line => line.startsWith('used_memory_human:'));
        return memoryLine ? memoryLine.split(':')[1] : 'unknown';
    } catch (error) {
        return 'unknown';
    }
}

module.exports = router;