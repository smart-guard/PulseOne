// ============================================================================
// backend/routes/realtime.js에 추가할 API 확장 코드 
// 기존 realtime.js 파일의 마지막 부분에 추가하세요
// ============================================================================

// =============================================================================
// 📊 DataExplorer용 실시간 데이터 API 확장 (기존 패턴 100% 준수)
// =============================================================================

/**
 * GET /api/realtime/current-values
 * 현재 값 일괄 조회 (인증 없이도 테스트 가능하도록 수정)
 */
router.get('/current-values', async (req, res) => {
    try {
        const { 
            device_ids, 
            site_id,
            data_type,
            quality_filter = 'good',
            limit = 100 
        } = req.query;

        // 시뮬레이션 현재값 데이터 생성 (기존 패턴 준수)
        const currentValues = [];
        const deviceCount = device_ids ? device_ids.split(',').length : Math.floor(Math.random() * 5) + 3;
        
        for (let i = 0; i < Math.min(deviceCount, parseInt(limit)); i++) {
            const deviceId = device_ids ? device_ids.split(',')[i] : i + 1;
            const pointsPerDevice = Math.floor(Math.random() * 8) + 2;
            
            for (let j = 0; j < pointsPerDevice; j++) {
                const dataPoint = generateRealtimeDataPoint(deviceId, j);
                if (!quality_filter || quality_filter === 'all' || dataPoint.quality === quality_filter) {
                    currentValues.push(dataPoint);
                }
            }
        }
        
        res.json({
            status: 'success',
            message: `${currentValues.length}개의 현재값 조회 완료`,
            data: {
                current_values: currentValues,
                total_count: currentValues.length,
                filters_applied: {
                    device_ids: device_ids || 'all',
                    site_id: site_id || 'all',
                    data_type: data_type || 'all',
                    quality_filter
                }
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('현재값 조회 실패:', error.message);
        res.status(500).json({
            status: 'error',
            message: '현재값 조회 실패',
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

/**
 * GET /api/realtime/device/:id/values
 * 특정 디바이스의 현재값 조회
 */
router.get('/device/:id/values', async (req, res) => {
    try {
        const { id } = req.params;
        const { data_type, limit = 50, include_metadata = false } = req.query;
        
        // 시뮬레이션 디바이스 데이터포인트들 (기존 패턴 준수)
        const deviceValues = [];
        const pointCount = Math.min(parseInt(limit), 20);
        
        for (let i = 0; i < pointCount; i++) {
            const dataPoint = generateRealtimeDataPoint(id, i);
            if (!data_type || dataPoint.dataType === data_type) {
                if (include_metadata === 'true') {
                    dataPoint.metadata = {
                        ...dataPoint.metadata,
                        collection_method: 'polling',
                        update_frequency: '1000ms',
                        last_error: null,
                        config_hash: `cfg_${Math.random().toString(36).substr(2, 8)}`
                    };
                }
                deviceValues.push(dataPoint);
            }
        }
        
        res.json({
            status: 'success',
            message: `디바이스 ${id}의 ${deviceValues.length}개 데이터포인트 조회 완료`,
            data: {
                device_id: parseInt(id),
                device_name: `Device_${id}`,
                device_status: Math.random() > 0.1 ? 'connected' : 'disconnected',
                last_communication: new Date(Date.now() - Math.random() * 30000).toISOString(),
                data_points: deviceValues,
                total_points: deviceValues.length
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('디바이스 값 조회 실패:', error.message);
        res.status(500).json({
            status: 'error',
            message: '디바이스 값 조회 실패',
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

/**
 * POST /api/realtime/subscribe
 * 실시간 구독 설정
 */
router.post('/subscribe', async (req, res) => {
    try {
        const { keys, update_interval = 1000, filters = {} } = req.body;
        
        if (!Array.isArray(keys) || keys.length === 0) {
            return res.status(400).json({
                status: 'error',
                message: 'keys array is required and must not be empty',
                timestamp: new Date().toISOString()
            });
        }
        
        // 시뮬레이션 구독 ID 생성 (기존 패턴 준수)
        const subscriptionId = `sub_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
        
        // 메모리에 구독 정보 저장 (실제로는 Redis에 저장)
        const subscriptionData = {
            id: subscriptionId,
            keys,
            update_interval,
            filters,
            created_at: new Date().toISOString(),
            status: 'active',
            client_info: {
                ip: req.ip || 'unknown',
                user_agent: req.get('User-Agent') || 'unknown'
            }
        };
        
        res.status(201).json({
            status: 'success',
            message: `${keys.length}개 키에 대한 구독 설정 완료`,
            data: {
                subscription_id: subscriptionId,
                websocket_url: `/ws/realtime?subscription_id=${subscriptionId}`,
                subscription: subscriptionData,
                estimated_updates_per_minute: Math.ceil(60000 / update_interval) * keys.length
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('구독 설정 실패:', error.message);
        res.status(500).json({
            status: 'error',
            message: '구독 설정 실패',
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

/**
 * DELETE /api/realtime/subscribe/:id
 * 실시간 구독 해제
 */
router.delete('/subscribe/:id', async (req, res) => {
    try {
        const { id } = req.params;
        
        // 시뮬레이션 구독 해제 (기존 패턴 준수)
        const unsubscribeResult = {
            subscription_id: id,
            unsubscribed_at: new Date().toISOString(),
            was_active: Math.random() > 0.1, // 90% 확률로 활성 구독이었음
            cleanup_completed: true
        };
        
        res.json({
            status: 'success',
            message: `구독 ${id} 해제 완료`,
            data: unsubscribeResult,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('구독 해제 실패:', error.message);
        res.status(500).json({
            status: 'error',
            message: '구독 해제 실패',
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

/**
 * GET /api/realtime/subscriptions
 * 활성 구독 목록 조회
 */
router.get('/subscriptions', async (req, res) => {
    try {
        const { status = 'all', limit = 50 } = req.query;
        
        // 시뮬레이션 구독 목록 생성 (기존 패턴 준수)
        const subscriptions = [];
        const subCount = Math.min(parseInt(limit), Math.floor(Math.random() * 10) + 3);
        
        for (let i = 0; i < subCount; i++) {
            const subStatus = Math.random() > 0.2 ? 'active' : 'inactive';
            if (status === 'all' || status === subStatus) {
                subscriptions.push({
                    id: `sub_${Date.now() - i * 1000}_${Math.random().toString(36).substr(2, 9)}`,
                    keys_count: Math.floor(Math.random() * 50) + 5,
                    status: subStatus,
                    created_at: new Date(Date.now() - Math.random() * 3600000).toISOString(),
                    last_update: new Date(Date.now() - Math.random() * 60000).toISOString(),
                    update_interval: [1000, 2000, 5000][Math.floor(Math.random() * 3)],
                    client_ip: `192.168.1.${Math.floor(Math.random() * 200) + 50}`
                });
            }
        }
        
        res.json({
            status: 'success',
            message: `${subscriptions.length}개의 구독 조회 완료`,
            data: {
                subscriptions,
                total_count: subscriptions.length,
                active_count: subscriptions.filter(s => s.status === 'active').length,
                inactive_count: subscriptions.filter(s => s.status === 'inactive').length
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('구독 목록 조회 실패:', error.message);
        res.status(500).json({
            status: 'error',
            message: '구독 목록 조회 실패',
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

/**
 * GET /api/realtime/stats
 * 실시간 데이터 통계
 */
router.get('/stats', async (req, res) => {
    try {
        // 시뮬레이션 통계 데이터 (기존 패턴 준수)
        const stats = {
            realtime_connections: Math.floor(Math.random() * 20) + 5,
            total_subscriptions: Math.floor(Math.random() * 50) + 15,
            active_subscriptions: Math.floor(Math.random() * 40) + 10,
            messages_per_second: Math.floor(Math.random() * 500) + 100,
            data_points_monitored: Math.floor(Math.random() * 2000) + 500,
            average_latency_ms: Math.floor(Math.random() * 50) + 10,
            uptime_seconds: Math.floor(Math.random() * 100000) + 86400,
            last_restart: new Date(Date.now() - Math.random() * 86400000).toISOString(),
            memory_usage: {
                used_mb: Math.floor(Math.random() * 512) + 128,
                total_mb: 1024,
                percentage: Math.floor(Math.random() * 50) + 25
            },
            error_rate: (Math.random() * 2).toFixed(3) + '%'
        };
        
        res.json({
            status: 'success',
            message: '실시간 데이터 통계 조회 완료',
            data: stats,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('통계 조회 실패:', error.message);
        res.status(500).json({
            status: 'error',
            message: '통계 조회 실패',
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// =============================================================================
// 헬퍼 함수들 (기존 패턴 준수)
// =============================================================================

/**
 * 실시간 데이터포인트 시뮬레이션 생성
 */
function generateRealtimeDataPoint(deviceId, pointIndex) {
    const pointNames = [
        'temperature', 'pressure', 'flow_rate', 'level', 'vibration',
        'motor_speed', 'current', 'voltage', 'power', 'efficiency',
        'humidity', 'ph_value', 'conductivity', 'turbidity', 'alarm_status'
    ];
    
    const dataTypes = ['number', 'boolean', 'string'];
    const qualities = ['good', 'bad', 'uncertain'];
    const units = ['°C', 'bar', 'L/min', 'mm', 'Hz', 'rpm', 'A', 'V', 'W', '%', 'pH'];
    
    const pointName = pointNames[pointIndex % pointNames.length];
    
    // 포인트 이름에 따라 적절한 데이터 타입 결정
    let dataType = 'number';
    if (pointName.includes('alarm') || pointName.includes('status')) {
        dataType = Math.random() > 0.5 ? 'boolean' : 'string';
    }
    
    const quality = Math.random() > 0.05 ? 'good' : qualities[Math.floor(Math.random() * qualities.length)];
    
    let value;
    let unit;
    
    switch (dataType) {
        case 'number':
            if (pointName.includes('temperature')) {
                value = (Math.random() * 50 + 20).toFixed(2);
                unit = '°C';
            } else if (pointName.includes('pressure')) {
                value = (Math.random() * 10 + 1).toFixed(2);
                unit = 'bar';
            } else if (pointName.includes('speed')) {
                value = (Math.random() * 3000 + 500).toFixed(0);
                unit = 'rpm';
            } else if (pointName.includes('current')) {
                value = (Math.random() * 50 + 5).toFixed(2);
                unit = 'A';
            } else if (pointName.includes('voltage')) {
                value = (Math.random() * 50 + 200).toFixed(1);
                unit = 'V';
            } else if (pointName.includes('flow')) {
                value = (Math.random() * 200 + 10).toFixed(2);
                unit = 'L/min';
            } else if (pointName.includes('level')) {
                value = (Math.random() * 100).toFixed(1);
                unit = '%';
            } else if (pointName.includes('ph')) {
                value = (Math.random() * 8 + 4).toFixed(2);
                unit = 'pH';
            } else {
                value = (Math.random() * 100).toFixed(2);
                unit = units[Math.floor(Math.random() * units.length)];
            }
            break;
        case 'boolean':
            value = Math.random() > (pointName.includes('alarm') ? 0.9 : 0.3);
            break;
        case 'string':
            const statusValues = ['Normal', 'Warning', 'Alarm', 'Maintenance', 'Auto', 'Manual'];
            value = statusValues[Math.floor(Math.random() * statusValues.length)];
            break;
    }
    
    return {
        id: `device_${deviceId}_${pointName}_${pointIndex}`,
        key: `pulseone:device:${deviceId}:${pointName}`,
        name: pointName.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase()),
        value,
        dataType,
        unit: dataType === 'number' ? unit : undefined,
        timestamp: new Date().toISOString(),
        quality,
        size: Math.floor(Math.random() * 512) + 32,
        ttl: Math.random() > 0.8 ? Math.floor(Math.random() * 1800) + 300 : undefined,
        metadata: {
            device_id: parseInt(deviceId),
            data_point_id: pointIndex + 1,
            protocol: ['MODBUS_TCP', 'MQTT', 'BACNET'][Math.floor(Math.random() * 3)],
            last_communication: new Date(Date.now() - Math.floor(Math.random() * 30000)).toISOString(),
            update_count: Math.floor(Math.random() * 1000) + 100,
            error_count: Math.floor(Math.random() * 10)
        }
    };
}

// 초기화 함수들 export (기존 파일의 마지막에 추가)
module.exports = {
    router,
    initializeRedis,
    initializeWebSocket
};