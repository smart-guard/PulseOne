// ============================================================================
// backend/routes/realtime.js
// 실시간 데이터 관리 API - WebSocket + Redis 기반
// ============================================================================

const express = require('express');
const router = express.Router();
const WebSocket = require('ws');
const Redis = require('redis');
const DeviceRepository = require('../lib/database/repositories/DeviceRepository');
const { 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus
} = require('../middleware/tenantIsolation');

// Repository 및 Redis 클라이언트
const deviceRepo = new DeviceRepository();
let redisClient;
let wsServer;

// Redis 연결 초기화
async function initializeRedis() {
    try {
        redisClient = Redis.createClient({
            host: process.env.REDIS_HOST || 'localhost',
            port: process.env.REDIS_PORT || 6379,
            password: process.env.REDIS_PASSWORD || undefined,
            db: process.env.REDIS_REALTIME_DB || 1
        });
        
        await redisClient.connect();
        console.log('✅ Redis connected for realtime data');
    } catch (error) {
        console.error('❌ Redis connection failed:', error);
        // Fallback to memory cache
        redisClient = new Map(); // 메모리 기반 폴백
    }
}

// WebSocket 서버 초기화
function initializeWebSocket(server) {
    wsServer = new WebSocket.Server({ 
        server,
        path: '/ws/realtime'
    });

    wsServer.on('connection', handleWebSocketConnection);
    console.log('✅ WebSocket server initialized for realtime data');
}

// ============================================================================
// 🎯 유틸리티 함수들
// ============================================================================

function createResponse(success, data, message, error_code) {
    const response = {
        success,
        timestamp: new Date().toISOString()
    };
    
    if (success) {
        response.data = data;
        response.message = message || 'Success';
    } else {
        response.error = data;
        response.error_code = error_code || 'INTERNAL_ERROR';
    }
    
    return response;
}

/**
 * Redis 키 생성
 */
function getRedisKey(type, tenantId, deviceId = null, dataPointId = null) {
    const parts = ['rt', tenantId, type]; // rt = realtime
    if (deviceId) parts.push(deviceId);
    if (dataPointId) parts.push(dataPointId);
    return parts.join(':');
}

/**
 * 실시간 데이터 Redis에 저장
 */
async function setRealtimeData(key, data, expireSeconds = 300) {
    try {
        if (redisClient.set) {
            // Redis 클라이언트
            await redisClient.setEx(key, expireSeconds, JSON.stringify(data));
        } else {
            // 메모리 폴백
            redisClient.set(key, data);
            setTimeout(() => redisClient.delete(key), expireSeconds * 1000);
        }
    } catch (error) {
        console.error('Redis set error:', error);
    }
}

/**
 * 실시간 데이터 Redis에서 조회
 */
async function getRealtimeData(key) {
    try {
        if (redisClient.get) {
            // Redis 클라이언트
            const data = await redisClient.get(key);
            return data ? JSON.parse(data) : null;
        } else {
            // 메모리 폴백
            return redisClient.get(key) || null;
        }
    } catch (error) {
        console.error('Redis get error:', error);
        return null;
    }
}

// ============================================================================
// 🌐 실시간 데이터 API 엔드포인트들
// ============================================================================

/**
 * GET /api/realtime/current-values
 * 현재 값 일괄 조회
 */
router.get('/current-values', 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus,
    async (req, res) => {
        try {
            const { tenantId, isSystemAdmin, siteAccess, deviceAccess } = req;
            const { 
                device_ids, 
                site_id,
                data_type, // number, boolean, string
                quality_filter = 'good', // good, bad, uncertain
                limit = 100 
            } = req.query;

            // 디바이스 목록 조회
            let devices;
            if (device_ids) {
                const ids = device_ids.split(',').map(id => parseInt(id));
                devices = await Promise.all(
                    ids.map(id => deviceRepo.findById(id, isSystemAdmin ? null : tenantId))
                );
                devices = devices.filter(device => device !== null);
            } else if (site_id) {
                devices = await deviceRepo.findBySite(parseInt(site_id), false, tenantId);
            } else {
                const result = await deviceRepo.findWithPagination(
                    {}, 
                    isSystemAdmin ? null : tenantId, 
                    1, 
                    parseInt(limit)
                );
                devices = result.devices;
            }

            // 접근 권한 필터링
            if (deviceAccess && deviceAccess.length > 0) {
                devices = devices.filter(device => deviceAccess.includes(device.id));
            }

            if (siteAccess && siteAccess.length > 0) {
                devices = devices.filter(device => siteAccess.includes(device.site_id));
            }

            // 각 디바이스의 실시간 데이터 조회
            const currentValues = [];
            
            for (const device of devices) {
                const deviceKey = getRedisKey('device', tenantId, device.id);
                const deviceData = await getRealtimeData(deviceKey);
                
                if (deviceData && deviceData.data_points) {
                    for (const dataPoint of deviceData.data_points) {
                        // 필터 적용
                        if (data_type && dataPoint.data_type !== data_type) continue;
                        if (quality_filter !== 'all' && dataPoint.quality !== quality_filter) continue;
                        
                        currentValues.push({
                            id: `${device.id}_${dataPoint.id}`,
                            device_id: device.id,
                            device_name: device.name,
                            site_name: device.site_name,
                            data_point_id: dataPoint.id,
                            name: dataPoint.name,
                            value: dataPoint.value,
                            data_type: dataPoint.data_type,
                            unit: dataPoint.unit,
                            quality: dataPoint.quality,
                            timestamp: dataPoint.timestamp,
                            alarm: dataPoint.alarm || false,
                            trend: dataPoint.trend || 'stable' // up, down, stable
                        });
                    }
                }
            }

            // 최신 순으로 정렬
            currentValues.sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp));

            res.json(createResponse(true, {
                current_values: currentValues,
                total_count: currentValues.length,
                connected_devices: devices.filter(d => d.status === 'connected').length,
                total_devices: devices.length,
                last_updated: new Date().toISOString()
            }, 'Current values retrieved successfully'));

        } catch (error) {
            console.error('Get current values error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'CURRENT_VALUES_FETCH_ERROR'));
        }
    }
);

/**
 * GET /api/realtime/device/:id/values
 * 특정 디바이스의 실시간 데이터 조회
 */
router.get('/device/:id/values', 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus,
    async (req, res) => {
        try {
            const { id } = req.params;
            const { tenantId, isSystemAdmin } = req;
            const { include_history = false, history_minutes = 60 } = req.query;

            // 디바이스 존재 확인
            const device = await deviceRepo.findById(parseInt(id), isSystemAdmin ? null : tenantId);
            if (!device) {
                return res.status(404).json(
                    createResponse(false, 'Device not found', null, 'DEVICE_NOT_FOUND')
                );
            }

            // 실시간 데이터 조회
            const deviceKey = getRedisKey('device', tenantId, device.id);
            const realtimeData = await getRealtimeData(deviceKey);

            let response = {
                device_id: device.id,
                device_name: device.name,
                device_status: device.status,
                site_name: device.site_name,
                last_seen: device.last_seen,
                data_points: []
            };

            if (realtimeData) {
                response.data_points = realtimeData.data_points || [];
                response.last_updated = realtimeData.last_updated;
            }

            // 히스토리 데이터 포함 (선택적)
            if (include_history === 'true') {
                const historyKey = getRedisKey('history', tenantId, device.id);
                const historyData = await getRealtimeData(historyKey);
                
                if (historyData) {
                    const cutoffTime = new Date(Date.now() - parseInt(history_minutes) * 60 * 1000);
                    response.history = historyData.values.filter(
                        point => new Date(point.timestamp) > cutoffTime
                    );
                }
            }

            res.json(createResponse(true, response, 'Device realtime data retrieved successfully'));

        } catch (error) {
            console.error('Get device realtime data error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'DEVICE_REALTIME_FETCH_ERROR'));
        }
    }
);

/**
 * POST /api/realtime/subscribe
 * 실시간 데이터 구독 설정
 */
router.post('/subscribe', 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus,
    async (req, res) => {
        try {
            const { tenantId } = req;
            const { device_ids, data_point_ids, update_interval = 1000, filters = {} } = req.body;

            // 구독 설정 생성
            const subscriptionId = `sub_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
            const subscription = {
                id: subscriptionId,
                tenant_id: tenantId,
                device_ids: device_ids || [],
                data_point_ids: data_point_ids || [],
                update_interval,
                filters,
                created_at: new Date().toISOString(),
                active: true
            };

            // Redis에 구독 정보 저장
            const subscriptionKey = getRedisKey('subscription', tenantId, subscriptionId);
            await setRealtimeData(subscriptionKey, subscription, 3600); // 1시간 유효

            res.status(201).json(createResponse(true, {
                subscription_id: subscriptionId,
                websocket_url: `/ws/realtime?subscription_id=${subscriptionId}`,
                subscription: subscription
            }, 'Subscription created successfully'));

        } catch (error) {
            console.error('Create subscription error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'SUBSCRIPTION_CREATE_ERROR'));
        }
    }
);

/**
 * DELETE /api/realtime/subscribe/:id
 * 실시간 데이터 구독 해제
 */
router.delete('/subscribe/:id', 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus,
    async (req, res) => {
        try {
            const { id } = req.params;
            const { tenantId } = req;

            // 구독 정보 조회
            const subscriptionKey = getRedisKey('subscription', tenantId, id);
            const subscription = await getRealtimeData(subscriptionKey);

            if (!subscription) {
                return res.status(404).json(
                    createResponse(false, 'Subscription not found', null, 'SUBSCRIPTION_NOT_FOUND')
                );
            }

            // 구독 비활성화
            subscription.active = false;
            subscription.cancelled_at = new Date().toISOString();
            await setRealtimeData(subscriptionKey, subscription, 60); // 1분 후 삭제

            res.json(createResponse(true, { cancelled_subscription_id: id }, 'Subscription cancelled successfully'));

        } catch (error) {
            console.error('Cancel subscription error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'SUBSCRIPTION_CANCEL_ERROR'));
        }
    }
);

/**
 * POST /api/realtime/simulate-data
 * 개발용 데이터 시뮬레이션 (개발 환경에서만)
 */
router.post('/simulate-data', 
    authenticateToken, 
    tenantIsolation, 
    async (req, res) => {
        try {
            if (process.env.NODE_ENV === 'production') {
                return res.status(403).json(
                    createResponse(false, 'Data simulation not allowed in production', null, 'SIMULATION_FORBIDDEN')
                );
            }

            const { tenantId } = req;
            const { device_id, duration_seconds = 60, interval_ms = 1000 } = req.body;

            // 디바이스 확인
            const device = await deviceRepo.findById(parseInt(device_id), tenantId);
            if (!device) {
                return res.status(404).json(
                    createResponse(false, 'Device not found', null, 'DEVICE_NOT_FOUND')
                );
            }

            // 시뮬레이션 시작
            startDataSimulation(tenantId, device, duration_seconds * 1000, interval_ms);

            res.json(createResponse(true, {
                simulation_started: true,
                device_id: parseInt(device_id),
                duration_seconds,
                interval_ms
            }, 'Data simulation started'));

        } catch (error) {
            console.error('Start simulation error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'SIMULATION_START_ERROR'));
        }
    }
);

// ============================================================================
// 🔄 WebSocket 연결 관리
// ============================================================================

/**
 * WebSocket 연결 처리
 */
function handleWebSocketConnection(ws, req) {
    console.log('🔌 New WebSocket connection established');
    
    const url = new URL(req.url, `http://${req.headers.host}`);
    const subscriptionId = url.searchParams.get('subscription_id');
    
    if (!subscriptionId) {
        ws.close(4001, 'Missing subscription_id');
        return;
    }

    ws.subscriptionId = subscriptionId;
    ws.isAlive = true;
    
    // 심박 확인
    ws.on('pong', () => {
        ws.isAlive = true;
    });

    // 메시지 처리
    ws.on('message', async (data) => {
        try {
            const message = JSON.parse(data);
            await handleWebSocketMessage(ws, message);
        } catch (error) {
            console.error('WebSocket message error:', error);
            ws.send(JSON.stringify({
                type: 'error',
                error: 'Invalid message format'
            }));
        }
    });

    // 연결 종료 처리
    ws.on('close', () => {
        console.log(`🔌 WebSocket connection closed: ${subscriptionId}`);
    });

    // 초기 연결 확인 메시지
    ws.send(JSON.stringify({
        type: 'connection_established',
        subscription_id: subscriptionId,
        timestamp: new Date().toISOString()
    }));

    // 주기적 데이터 전송 시작
    startRealtimeDataStream(ws);
}

/**
 * WebSocket 메시지 처리
 */
async function handleWebSocketMessage(ws, message) {
    switch (message.type) {
        case 'ping':
            ws.send(JSON.stringify({ type: 'pong', timestamp: new Date().toISOString() }));
            break;
            
        case 'update_subscription':
            // 구독 설정 업데이트
            const subscriptionKey = getRedisKey('subscription', message.tenant_id, ws.subscriptionId);
            const subscription = await getRealtimeData(subscriptionKey);
            
            if (subscription) {
                Object.assign(subscription, message.updates);
                await setRealtimeData(subscriptionKey, subscription, 3600);
                
                ws.send(JSON.stringify({
                    type: 'subscription_updated',
                    subscription_id: ws.subscriptionId
                }));
            }
            break;
            
        default:
            ws.send(JSON.stringify({
                type: 'error',
                error: `Unknown message type: ${message.type}`
            }));
    }
}

/**
 * 실시간 데이터 스트림 시작
 */
async function startRealtimeDataStream(ws) {
    const sendData = async () => {
        if (ws.readyState !== WebSocket.OPEN) return;
        
        try {
            // 구독 정보 조회
            const subscriptionKey = getRedisKey('subscription', 'unknown', ws.subscriptionId);
            const subscription = await getRealtimeData(subscriptionKey);
            
            if (!subscription || !subscription.active) {
                ws.close(4002, 'Subscription not found or inactive');
                return;
            }

            // 구독된 디바이스들의 데이터 수집
            const realtimeData = [];
            
            for (const deviceId of subscription.device_ids) {
                const deviceKey = getRedisKey('device', subscription.tenant_id, deviceId);
                const deviceData = await getRealtimeData(deviceKey);
                
                if (deviceData) {
                    realtimeData.push({
                        device_id: deviceId,
                        data_points: deviceData.data_points || [],
                        last_updated: deviceData.last_updated
                    });
                }
            }

            // 데이터 전송
            ws.send(JSON.stringify({
                type: 'realtime_data',
                subscription_id: ws.subscriptionId,
                data: realtimeData,
                timestamp: new Date().toISOString()
            }));

        } catch (error) {
            console.error('WebSocket data stream error:', error);
        }
    };

    // 초기 데이터 전송
    await sendData();
    
    // 주기적 데이터 전송
    const interval = setInterval(sendData, 1000); // 1초마다
    
    ws.on('close', () => {
        clearInterval(interval);
    });
}

// ============================================================================
// 🎲 데이터 시뮬레이션 (개발용)
// ============================================================================

/**
 * 데이터 시뮬레이션 시작
 */
function startDataSimulation(tenantId, device, duration, interval) {
    console.log(`🎲 Starting data simulation for device ${device.id}`);
    
    const startTime = Date.now();
    const dataPoints = [
        { id: 1, name: 'Temperature', data_type: 'number', unit: '°C', base_value: 25 },
        { id: 2, name: 'Pressure', data_type: 'number', unit: 'Bar', base_value: 1.2 },
        { id: 3, name: 'Running', data_type: 'boolean', unit: '', base_value: true },
        { id: 4, name: 'Status', data_type: 'string', unit: '', base_value: 'Normal' }
    ];

    const simulationInterval = setInterval(async () => {
        const elapsed = Date.now() - startTime;
        
        if (elapsed >= duration) {
            clearInterval(simulationInterval);
            console.log(`🎲 Data simulation completed for device ${device.id}`);
            return;
        }

        // 시뮬레이션 데이터 생성
        const simulatedData = {
            device_id: device.id,
            data_points: dataPoints.map(dp => ({
                id: dp.id,
                name: dp.name,
                value: generateSimulatedValue(dp),
                data_type: dp.data_type,
                unit: dp.unit,
                quality: Math.random() > 0.95 ? 'uncertain' : 'good',
                timestamp: new Date().toISOString(),
                alarm: Math.random() > 0.9
            })),
            last_updated: new Date().toISOString()
        };

        // Redis에 저장
        const deviceKey = getRedisKey('device', tenantId, device.id);
        await setRealtimeData(deviceKey, simulatedData, 300);

        // WebSocket으로 브로드캐스트
        broadcastToSubscribers(tenantId, device.id, simulatedData);

    }, interval);
}

/**
 * 시뮬레이션 값 생성
 */
function generateSimulatedValue(dataPoint) {
    switch (dataPoint.data_type) {
        case 'number':
            const variation = dataPoint.base_value * 0.1;
            return +(dataPoint.base_value + (Math.random() - 0.5) * variation).toFixed(2);
            
        case 'boolean':
            return Math.random() > 0.8 ? !dataPoint.base_value : dataPoint.base_value;
            
        case 'string':
            const statuses = ['Normal', 'Warning', 'Alarm', 'Maintenance'];
            return Math.random() > 0.9 ? statuses[Math.floor(Math.random() * statuses.length)] : dataPoint.base_value;
            
        default:
            return dataPoint.base_value;
    }
}

/**
 * 구독자들에게 브로드캐스트
 */
function broadcastToSubscribers(tenantId, deviceId, data) {
    if (!wsServer) return;

    wsServer.clients.forEach((client) => {
        if (client.readyState === WebSocket.OPEN) {
            // TODO: 구독 필터링 로직
            client.send(JSON.stringify({
                type: 'device_data_update',
                device_id: deviceId,
                data: data,
                timestamp: new Date().toISOString()
            }));
        }
    });
}

// ============================================================================
// 🧹 정리 작업
// ============================================================================

// 주기적 정리 작업 (죽은 연결 제거)
setInterval(() => {
    if (!wsServer) return;
    
    wsServer.clients.forEach((ws) => {
        if (ws.isAlive === false) {
            ws.terminate();
            return;
        }
        
        ws.isAlive = false;
        ws.ping();
    });
}, 30000); // 30초마다

// 모듈 초기화
initializeRedis();

module.exports = {
    router,
    initializeWebSocket,
    initializeRedis
};