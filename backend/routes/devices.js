// ============================================================================
// backend/routes/devices.js
// 실제 DeviceRepository 연결 - 기존 구조 유지하면서 Repository 사용
// ============================================================================

const express = require('express');
const router = express.Router();

// 🔥 실제 Repository 사용
const DeviceRepository = require('../lib/database/repositories/DeviceRepository');
const JsonConfigManager = require('../lib/config/JsonConfigManager');

// 🔥 기존 미들웨어 유지 (있는 경우에만)
const { 
    authenticateToken, 
    tenantIsolation, 
    checkSiteAccess,
    checkDeviceAccess,
    requireRole,
    requirePermission,
    validateTenantStatus
} = require('../middleware/tenantIsolation');

// Repository 및 JsonConfigManager 인스턴스
const deviceRepo = new DeviceRepository();
let configManager = null;

// JsonConfigManager 초기화 (있는 경우에만)
try {
    configManager = JsonConfigManager.getInstance();
    configManager.initialize().catch(console.error);
} catch (error) {
    console.warn('⚠️ JsonConfigManager 없음, 기본 설정 사용');
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

// 시뮬레이션 알람 데이터 생성 함수
function generateDeviceAlarms(deviceId, tenantId) {
    const severities = ['critical', 'high', 'medium', 'low'];
    const messages = [
        '온도 임계값 초과',
        '압력 센서 이상', 
        '통신 연결 실패',
        '전력 공급 불안정',
        '진동 수치 높음',
        '습도 센서 오류'
    ];

    const alarmCount = Math.floor(Math.random() * 6) + 1; // 1-6개
    const alarms = [];

    for (let i = 0; i < alarmCount; i++) {
        const severity = severities[Math.floor(Math.random() * severities.length)];
        
        alarms.push({
            id: `alarm_${deviceId}_${Date.now()}_${i}`,
            device_id: deviceId,
            severity,
            message: messages[Math.floor(Math.random() * messages.length)],
            occurred_at: new Date(Date.now() - Math.random() * 86400000).toISOString(),
            acknowledged: Math.random() > 0.6,
            tenant_id: tenantId
        });
    }

    return alarms.sort((a, b) => new Date(b.occurred_at) - new Date(a.occurred_at));
}

// ============================================================================
// 🔧 설정 관련 API 엔드포인트들 (JsonConfigManager 있는 경우에만)
// ============================================================================

/**
 * GET /api/devices/config/protocols
 * 지원하는 프로토콜 목록 조회
 */
router.get('/config/protocols', async (req, res) => {
    try {
        // JsonConfigManager가 있으면 사용, 없으면 기본값
        let protocols;
        if (configManager) {
            protocols = await configManager.getSupportedProtocols();
        } else {
            // 기본 프로토콜 목록
            protocols = [
                { value: 'MODBUS_TCP', name: 'Modbus TCP', description: 'Modbus over TCP/IP' },
                { value: 'MODBUS_RTU', name: 'Modbus RTU', description: 'Modbus RTU Serial' },
                { value: 'MQTT', name: 'MQTT', description: 'MQTT Protocol' },
                { value: 'BACNET', name: 'BACnet', description: 'Building Automation Protocol' }
            ];
        }
        
        res.json(createResponse(true, protocols, 'Supported protocols retrieved successfully'));
    } catch (error) {
        console.error('Get protocols error:', error);
        res.status(500).json(createResponse(false, error.message, null, 'PROTOCOLS_FETCH_ERROR'));
    }
});

/**
 * GET /api/devices/config/device-types
 * 디바이스 타입 목록 조회
 */
router.get('/config/device-types', async (req, res) => {
    try {
        let deviceTypes;
        if (configManager) {
            deviceTypes = await configManager.getDeviceTypes();
        } else {
            // 기본 디바이스 타입 목록
            deviceTypes = [
                { value: 'PLC', name: 'PLC', description: 'Programmable Logic Controller' },
                { value: 'HMI', name: 'HMI', description: 'Human Machine Interface' },
                { value: 'SENSOR', name: 'Sensor', description: 'IoT Sensor Device' },
                { value: 'METER', name: 'Meter', description: 'Energy/Flow Meter' },
                { value: 'CONTROLLER', name: 'Controller', description: 'Process Controller' }
            ];
        }
        
        res.json(createResponse(true, deviceTypes, 'Device types retrieved successfully'));
    } catch (error) {
        console.error('Get device types error:', error);
        res.status(500).json(createResponse(false, error.message, null, 'DEVICE_TYPES_FETCH_ERROR'));
    }
});

// ============================================================================
// 🌐 메인 디바이스 API 엔드포인트들 (실제 Repository 사용)
// ============================================================================

/**
 * GET /api/devices
 * 디바이스 목록 조회 - 실제 DeviceRepository 사용
 */
router.get('/', async (req, res) => {
    try {
        console.log('🔍 DeviceRepository를 사용한 디바이스 목록 조회...');
        
        const { 
            page = 1, 
            limit = 20, 
            include_alarms = 'false',
            site_id,
            device_type,
            protocol,
            status,
            search,
            sort_by = 'name',
            sort_order = 'asc'
        } = req.query;

        // 🔥 미들웨어에서 tenantId 추출 (있는 경우)
        const tenantId = req.tenantId || req.user?.tenant_id || null;

        // 🔥 실제 DeviceRepository 사용
        const options = {
            tenantId,
            siteId: site_id ? parseInt(site_id) : null,
            deviceType: device_type,
            protocol,
            status,
            search,
            page: parseInt(page),
            limit: parseInt(limit),
            sortBy: sort_by,
            sortOrder: sort_order
        };

        const result = await deviceRepo.findAll(options);

        // 🔥 알람 정보 추가 (시뮬레이션)
        if (include_alarms === 'true') {
            result.items = result.items.map(device => {
                const deviceAlarms = generateDeviceAlarms(device.id, tenantId);
                return {
                    ...device,
                    alarm_summary: {
                        total: deviceAlarms.length,
                        critical: deviceAlarms.filter(a => a.severity === 'critical').length,
                        high: deviceAlarms.filter(a => a.severity === 'high').length,
                        medium: deviceAlarms.filter(a => a.severity === 'medium').length,
                        low: deviceAlarms.filter(a => a.severity === 'low').length,
                        unacknowledged: deviceAlarms.filter(a => !a.acknowledged).length,
                        latest_alarm: deviceAlarms.length > 0 ? deviceAlarms[0] : null
                    }
                };
            });
        }

        console.log(`✅ ${result.items.length}개 디바이스 조회 완료 (Repository 사용)`);
        res.json(createResponse(true, result, 'Devices retrieved successfully from repository'));

    } catch (error) {
        console.error('❌ 디바이스 목록 조회 실패:', error);
        res.status(500).json(createResponse(false, error.message, null, 'DEVICES_FETCH_ERROR'));
    }
});

/**
 * GET /api/devices/:id
 * 특정 디바이스 상세 조회 - 실제 DeviceRepository 사용
 */
router.get('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.tenantId || req.user?.tenant_id || null;

        console.log(`🔍 DeviceRepository를 사용한 디바이스 ${id} 상세 조회...`);

        // 🔥 실제 DeviceRepository 사용
        const device = await deviceRepo.findById(parseInt(id), tenantId);

        if (!device) {
            return res.status(404).json(
                createResponse(false, 'Device not found', null, 'DEVICE_NOT_FOUND')
            );
        }

        console.log(`✅ 디바이스 ID ${id} 상세 조회 완료 (Repository 사용)`);
        res.json(createResponse(true, device, 'Device details retrieved successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ${req.params.id} 상세 조회 실패:`, error);
        res.status(500).json(createResponse(false, error.message, null, 'DEVICE_DETAIL_ERROR'));
    }
});

/**
 * POST /api/devices
 * 새 디바이스 생성 - 실제 DeviceRepository 사용
 */
router.post('/', async (req, res) => {
    try {
        const tenantId = req.tenantId || req.user?.tenant_id || 1; // 기본값 1
        const deviceData = req.body;

        console.log('🔧 DeviceRepository를 사용한 새 디바이스 생성...');

        // 필수 필드 검증
        const requiredFields = ['name', 'protocol'];
        const missingFields = requiredFields.filter(field => !deviceData[field]);
        
        if (missingFields.length > 0) {
            return res.status(400).json(
                createResponse(false, `Missing required fields: ${missingFields.join(', ')}`, null, 'MISSING_REQUIRED_FIELDS')
            );
        }

        // 🔥 실제 DeviceRepository 사용
        const newDevice = await deviceRepo.create(deviceData, tenantId);

        console.log(`✅ 새 디바이스 생성 완료: ID ${newDevice.id} (Repository 사용)`);
        res.status(201).json(createResponse(true, newDevice, 'Device created successfully'));

    } catch (error) {
        console.error('❌ 디바이스 생성 실패:', error);
        res.status(500).json(createResponse(false, error.message, null, 'DEVICE_CREATE_ERROR'));
    }
});

/**
 * PUT /api/devices/:id
 * 디바이스 수정 - 실제 DeviceRepository 사용
 */
router.put('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.tenantId || req.user?.tenant_id || null;
        const updateData = req.body;

        console.log(`🔧 DeviceRepository를 사용한 디바이스 ${id} 수정...`);

        // 🔥 실제 DeviceRepository 사용
        const updatedDevice = await deviceRepo.update(parseInt(id), updateData, tenantId);

        if (!updatedDevice) {
            return res.status(404).json(
                createResponse(false, 'Device not found or update failed', null, 'DEVICE_UPDATE_FAILED')
            );
        }

        console.log(`✅ 디바이스 ID ${id} 수정 완료 (Repository 사용)`);
        res.json(createResponse(true, updatedDevice, 'Device updated successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ${req.params.id} 수정 실패:`, error);
        res.status(500).json(createResponse(false, error.message, null, 'DEVICE_UPDATE_ERROR'));
    }
});

/**
 * DELETE /api/devices/:id
 * 디바이스 삭제 - 실제 DeviceRepository 사용
 */
router.delete('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.tenantId || req.user?.tenant_id || null;

        console.log(`🗑️ DeviceRepository를 사용한 디바이스 ${id} 삭제...`);

        // 🔥 실제 DeviceRepository 사용
        const deleted = await deviceRepo.deleteById(parseInt(id), tenantId);

        if (!deleted) {
            return res.status(404).json(
                createResponse(false, 'Device not found or delete failed', null, 'DEVICE_DELETE_FAILED')
            );
        }

        console.log(`✅ 디바이스 ID ${id} 삭제 완료 (Repository 사용)`);
        res.json(createResponse(true, { deleted: true }, 'Device deleted successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ${req.params.id} 삭제 실패:`, error);
        res.status(500).json(createResponse(false, error.message, null, 'DEVICE_DELETE_ERROR'));
    }
});

// ============================================================================
// 🌐 디바이스 특화 API 엔드포인트들
// ============================================================================

/**
 * GET /api/devices/:id/alarms
 * 특정 디바이스의 활성 알람 조회
 */
router.get('/:id/alarms', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.tenantId || req.user?.tenant_id || null;

        console.log(`🚨 디바이스 ${id}의 알람 조회...`);

        // 디바이스 존재 확인
        const deviceExists = await deviceRepo.exists(parseInt(id), tenantId);
        if (!deviceExists) {
            return res.status(404).json(
                createResponse(false, 'Device not found', null, 'DEVICE_NOT_FOUND')
            );
        }

        // 시뮬레이션 알람 데이터
        const deviceAlarms = generateDeviceAlarms(parseInt(id), tenantId);

        const result = {
            device_id: parseInt(id),
            active_alarms: deviceAlarms,
            summary: {
                total: deviceAlarms.length,
                critical: deviceAlarms.filter(a => a.severity === 'critical').length,
                high: deviceAlarms.filter(a => a.severity === 'high').length,
                medium: deviceAlarms.filter(a => a.severity === 'medium').length,
                low: deviceAlarms.filter(a => a.severity === 'low').length,
                unacknowledged: deviceAlarms.filter(a => !a.acknowledged).length
            }
        };

        console.log(`✅ 디바이스 ${id} 알람 조회 완료: ${deviceAlarms.length}건`);
        res.json(createResponse(true, result, 'Device alarms retrieved successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ${req.params.id} 알람 조회 실패:`, error);
        res.status(500).json(createResponse(false, error.message, null, 'DEVICE_ALARMS_ERROR'));
    }
});

/**
 * POST /api/devices/:id/test-connection
 * 디바이스 연결 테스트 - 실제 DeviceRepository 사용
 */
router.post('/:id/test-connection', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.tenantId || req.user?.tenant_id || null;

        console.log(`🧪 DeviceRepository를 사용한 디바이스 ${id} 연결 테스트...`);

        // 🔥 실제 DeviceRepository 사용
        const testResult = await deviceRepo.testConnection(parseInt(id), tenantId);

        console.log(`✅ 디바이스 ${id} 연결 테스트 완료: ${testResult.success ? '성공' : '실패'}`);
        res.json(createResponse(true, testResult, 'Connection test completed'));

    } catch (error) {
        console.error(`❌ 디바이스 ${req.params.id} 연결 테스트 실패:`, error);
        res.status(500).json(createResponse(false, error.message, null, 'CONNECTION_TEST_ERROR'));
    }
});

/**
 * GET /api/devices/protocol/:protocol
 * 프로토콜별 디바이스 조회 - 실제 DeviceRepository 사용
 */
router.get('/protocol/:protocol', async (req, res) => {
    try {
        const { protocol } = req.params;
        const tenantId = req.tenantId || req.user?.tenant_id || null;

        console.log(`🔍 프로토콜 ${protocol} 디바이스 조회 (Repository 사용)...`);

        // 🔥 실제 DeviceRepository 사용
        const devices = await deviceRepo.findByProtocol(protocol, tenantId);

        console.log(`✅ 프로토콜 ${protocol} 디바이스 조회 완료: ${devices.length}개`);
        res.json(createResponse(true, devices, `Devices with protocol ${protocol} retrieved successfully`));

    } catch (error) {
        console.error(`❌ 프로토콜 ${req.params.protocol} 디바이스 조회 실패:`, error);
        res.status(500).json(createResponse(false, error.message, null, 'PROTOCOL_DEVICES_ERROR'));
    }
});

// ============================================================================
// 🔥 데이터베이스 연결 테스트 엔드포인트
// ============================================================================

/**
 * GET /api/devices/test/database
 * 데이터베이스 연결 및 Repository 테스트
 */
router.get('/test/database', async (req, res) => {
    try {
        console.log('🧪 DeviceRepository 및 DB 연결 테스트...');

        // Repository 헬스체크
        const healthCheck = await deviceRepo.healthCheck();

        // 간단한 데이터 조회 테스트
        const testDevices = await deviceRepo.findAll({ limit: 5 });

        const result = {
            repository_health: healthCheck,
            test_query: {
                success: true,
                devices_found: testDevices.items ? testDevices.items.length : 0,
                total_devices: testDevices.pagination ? testDevices.pagination.total_items : 0
            },
            cache_stats: deviceRepo.getCacheStats(),
            test_time: new Date().toISOString()
        };

        console.log('✅ DeviceRepository 테스트 완료:', result);
        res.json(createResponse(true, result, 'DeviceRepository test successful'));

    } catch (error) {
        console.error('❌ DeviceRepository 테스트 실패:', error);
        res.status(500).json(createResponse(false, error.message, null, 'REPOSITORY_TEST_ERROR'));
    }
});

// ============================================================================
// 🔥 통계 API 엔드포인트들
// ============================================================================

/**
 * GET /api/devices/stats/protocol-distribution
 * 프로토콜별 분포 통계
 */
router.get('/stats/protocol-distribution', async (req, res) => {
    try {
        const tenantId = req.tenantId || req.user?.tenant_id || null;

        console.log('📊 프로토콜별 분포 통계 조회 (Repository 사용)...');

        // 🔥 실제 DeviceRepository 사용
        const distribution = await deviceRepo.getProtocolDistribution(tenantId);

        console.log(`✅ 프로토콜별 분포 통계 조회 완료: ${distribution.length}개 프로토콜`);
        res.json(createResponse(true, distribution, 'Protocol distribution retrieved successfully'));

    } catch (error) {
        console.error('❌ 프로토콜별 분포 통계 조회 실패:', error);
        res.status(500).json(createResponse(false, error.message, null, 'PROTOCOL_STATS_ERROR'));
    }
});

/**
 * GET /api/devices/stats/status-distribution
 * 상태별 분포 통계
 */
router.get('/stats/status-distribution', async (req, res) => {
    try {
        const tenantId = req.tenantId || req.user?.tenant_id || null;

        console.log('📊 상태별 분포 통계 조회 (Repository 사용)...');

        // 🔥 실제 DeviceRepository 사용
        const distribution = await deviceRepo.getStatusDistribution(tenantId);

        console.log(`✅ 상태별 분포 통계 조회 완료: ${distribution.length}개 상태`);
        res.json(createResponse(true, distribution, 'Status distribution retrieved successfully'));

    } catch (error) {
        console.error('❌ 상태별 분포 통계 조회 실패:', error);
        res.status(500).json(createResponse(false, error.message, null, 'STATUS_STATS_ERROR'));
    }
});

module.exports = router;