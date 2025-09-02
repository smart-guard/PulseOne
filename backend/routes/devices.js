// ============================================================================
// backend/routes/devices.js (통합 버전)
// 기존 디바이스 관리 API + Collector 프록시 + 설정 동기화
// ============================================================================

const express = require('express');
const router = express.Router();
const sqlite3 = require('sqlite3').verbose();

// Repository imports (기존)
const DeviceRepository = require('../lib/database/repositories/DeviceRepository');
const SiteRepository = require('../lib/database/repositories/SiteRepository');
const ConfigManager = require('../lib/config/ConfigManager');

// 🔥 새로 추가: Collector 프록시 및 동기화 시스템
const { getInstance: getCollectorProxy } = require('../lib/services/CollectorProxyService');
const { getInstance: getConfigSyncHooks } = require('../lib/hooks/ConfigSyncHooks');

const { 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus 
} = require('../middleware/tenantIsolation');

// Repository 인스턴스 생성 (기존)
let deviceRepo = null;
let siteRepo = null;
const configManager = ConfigManager.getInstance();

function getDeviceRepo() {
    if (!deviceRepo) {
        deviceRepo = new DeviceRepository();
        console.log("DeviceRepository 인스턴스 생성 완료");
    }
    return deviceRepo;
}

function getSiteRepo() {
    if (!siteRepo) {
        siteRepo = new SiteRepository();
        console.log("SiteRepository 인스턴스 생성 완료");
    }
    return siteRepo;
}

// ============================================================================
// 미들웨어 및 헬퍼 함수들 (기존 유지)
// ============================================================================

function createResponse(success, data, message, error_code) {
    return {
        success,
        data,
        message: message || (success ? 'Success' : 'Error'),
        error_code: error_code || null,
        timestamp: new Date().toISOString()
    };
}

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

function enhanceDeviceWithRtuInfo(device) {
    if (!device) return device;

    try {
        let config = {};
        
        if (device.config) {
            if (typeof device.config === 'string') {
                if (device.config === '[object Object]' || device.config.startsWith('[object')) {
                    console.warn(`Device ${device.id}: Invalid config string detected, using empty config`);
                    config = {};
                } else {
                    try {
                        config = JSON.parse(device.config);
                    } catch (parseError) {
                        console.warn(`Device ${device.id}: Config JSON parse failed, using empty config:`, parseError.message);
                        config = {};
                    }
                }
            } else if (typeof device.config === 'object') {
                config = device.config;
            } else {
                console.warn(`Device ${device.id}: Unexpected config type ${typeof device.config}, using empty config`);
                config = {};
            }
        }
        
        const enhanced = {
            ...device,
            config: config,
            rtu_info: null
        };

        if (device.protocol_type === 'MODBUS_RTU') {
            enhanced.rtu_info = {
                slave_id: config.slave_id || null,
                master_device_id: config.master_device_id || null,
                baud_rate: config.baud_rate || 9600,
                data_bits: config.data_bits || 8,
                stop_bits: config.stop_bits || 1,
                parity: config.parity || 'N',
                frame_delay_ms: config.frame_delay_ms || null,
                response_timeout_ms: config.response_timeout_ms || 1000,
                is_master: device.device_type === 'GATEWAY',
                is_slave: device.device_type !== 'GATEWAY' && config.master_device_id,
                serial_port: device.endpoint,
                network_info: {
                    protocol: 'Modbus RTU',
                    connection_type: 'Serial',
                    port: device.endpoint
                }
            };
        }

        return enhanced;
    } catch (error) {
        console.warn(`Device ${device.id}: Config processing failed:`, error.message);
        return {
            ...device,
            config: {},
            rtu_info: null
        };
    }
}

function enhanceDevicesWithRtuInfo(devices) {
    if (!Array.isArray(devices)) return devices;
    return devices.map(device => enhanceDeviceWithRtuInfo(device));
}

async function addRtuRelationships(devices, tenantId) {
    if (!Array.isArray(devices)) return devices;
    
    const rtuMasters = devices.filter(d => 
        d.protocol_type === 'MODBUS_RTU' && d.device_type === 'GATEWAY'
    );
    
    const rtuSlaves = devices.filter(d => 
        d.protocol_type === 'MODBUS_RTU' && d.device_type !== 'GATEWAY'
    );
    
    for (const master of rtuMasters) {
        const slaves = rtuSlaves.filter(slave => {
            const slaveConfig = slave.rtu_info;
            return slaveConfig && slaveConfig.master_device_id === master.id;
        });
        
        if (master.rtu_info) {
            master.rtu_info.slave_count = slaves.length;
            master.rtu_info.slaves = slaves.map(slave => ({
                device_id: slave.id,
                device_name: slave.name,
                slave_id: slave.rtu_info ? slave.rtu_info.slave_id : null,
                device_type: slave.device_type,
                connection_status: slave.connection_status,
                manufacturer: slave.manufacturer,
                model: slave.model
            }));
        }
    }
    
    return devices;
}

async function validateProtocolId(protocolId, tenantId) {
    if (!protocolId || typeof protocolId !== 'number') {
        return { valid: false, error: 'protocol_id is required and must be a number' };
    }

    try {
        const protocolQuery = `SELECT id, protocol_type, display_name FROM protocols WHERE id = ? AND is_enabled = 1`;
        const result = await getDeviceRepo().dbFactory.executeQuery(protocolQuery, [protocolId]);
        
        if (!result || result.length === 0) {
            return { valid: false, error: `Invalid or disabled protocol_id: ${protocolId}` };
        }

        return { valid: true, protocol: result[0] };
    } catch (error) {
        console.warn('Protocol validation failed:', error.message);
        return { valid: false, error: 'Protocol validation failed' };
    }
}

// 🔥 새로 추가: Collector 연결 확인 미들웨어
const checkCollectorConnection = async (req, res, next) => {
    try {
        const proxy = getCollectorProxy();
        
        if (!proxy.isCollectorHealthy()) {
            try {
                await proxy.healthCheck();
            } catch (error) {
                return res.status(503).json({
                    success: false,
                    error: 'Collector service is unavailable',
                    details: 'Collector 서비스에 연결할 수 없습니다. 서비스가 실행 중인지 확인하세요.',
                    lastHealthCheck: proxy.getLastHealthCheck(),
                    collectorConfig: proxy.getCollectorConfig()
                });
            }
        }
        
        next();
    } catch (error) {
        res.status(500).json({
            success: false,
            error: 'Failed to check collector connection',
            details: error.message
        });
    }
};

// 🔥 새로 추가: Collector 프록시 에러 핸들러
const handleCollectorProxyError = (error, req, res) => {
    console.error(`❌ Collector Proxy Error [${req.method} ${req.originalUrl}]:`, error);
    
    if (error.name === 'CollectorProxyError') {
        const statusCode = error.status || 500;
        
        res.status(statusCode).json({
            success: false,
            error: `Collector API Error: ${error.operation}`,
            details: error.collectorError || error.message,
            context: error.context,
            collectorResponse: error.collectorResponse
        });
    } else {
        res.status(500).json({
            success: false,
            error: 'Proxy communication failed',
            details: error.message
        });
    }
};

// 인증 미들웨어 (기존)
const devAuthMiddleware = (req, res, next) => {
    req.user = {
        id: 1,
        username: 'admin',
        tenant_id: 1,
        role: 'admin'
    };
    next();
};

const devTenantMiddleware = (req, res, next) => {
    req.tenantId = req.user.tenant_id;
    next();
};

// 글로벌 미들웨어 적용
router.use(devAuthMiddleware);
router.use(devTenantMiddleware);

// ============================================================================
// 우선순위 라우트들 (기존 유지)
// ============================================================================

router.get('/protocols', async (req, res) => {
    try {
        const { tenantId } = req;
        console.log('지원 프로토콜 목록 조회...');

        const protocols = await getDeviceRepo().getAvailableProtocols(tenantId);
        console.log(`${protocols.length}개 프로토콜 조회 완료`);
        
        res.json(createResponse(true, protocols, 'Available protocols retrieved successfully'));
        
    } catch (error) {
        console.error('프로토콜 목록 조회 실패:', error.message);
        console.error('Error stack:', error.stack);
        res.status(500).json(createResponse(false, null, 'Failed to retrieve protocols', error.message));
    }
});

router.get('/statistics', async (req, res) => {
    try {
        const { tenantId } = req;

        console.log('디바이스 통계 조회 (RTU 정보 포함)...');

        try {
            const stats = await getDeviceRepo().getStatsByTenant(tenantId);
            console.log('디바이스 통계 조회 완료');
            res.json(createResponse(true, stats, 'Device statistics retrieved successfully'));
        } catch (repoError) {
            console.warn('Repository 통계 메서드 없음, 기본 통계 생성:', repoError.message);
            
            const devicesResult = await getDeviceRepo().findAllDevices({ tenantId });
            const devices = devicesResult.items || [];
            const enhancedDevices = enhanceDevicesWithRtuInfo(devices);
            
            const rtuDevices = enhancedDevices.filter(d => d.protocol_type === 'MODBUS_RTU');
            const rtuMasters = rtuDevices.filter(d => d.rtu_info && d.rtu_info.is_master);
            const rtuSlaves = rtuDevices.filter(d => d.rtu_info && d.rtu_info.is_slave);
            
            const stats = {
                total_devices: devices.length,
                active_devices: devices.filter(d => d.connection_status === 'connected').length,
                enabled_devices: devices.filter(d => d.is_enabled).length,
                by_protocol: devices.reduce((acc, device) => {
                    const protocol = device.protocol_type || 'unknown';
                    acc[protocol] = (acc[protocol] || 0) + 1;
                    return acc;
                }, {}),
                by_connection: devices.reduce((acc, device) => {
                    const conn = device.connection_status || 'unknown';
                    acc[conn] = (acc[conn] || 0) + 1;
                    return acc;
                }, {}),
                rtu_statistics: {
                    total_rtu_devices: rtuDevices.length,
                    rtu_masters: rtuMasters.length,
                    rtu_slaves: rtuSlaves.length,
                    rtu_networks: rtuMasters.map(master => ({
                        master_id: master.id,
                        master_name: master.name,
                        serial_port: master.endpoint,
                        baud_rate: master.rtu_info.baud_rate,
                        connection_status: master.connection_status
                    }))
                },
                last_updated: new Date().toISOString()
            };
            
            res.json(createResponse(true, stats, 'Device statistics calculated successfully'));
        }

    } catch (error) {
        console.error('디바이스 통계 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_STATS_ERROR'));
    }
});

router.post('/bulk-action', async (req, res) => {
    try {
        const { tenantId } = req;
        const { action, device_ids } = req.body;

        if (!Array.isArray(device_ids) || device_ids.length === 0) {
            return res.status(400).json(
                createResponse(false, null, 'device_ids array is required', 'VALIDATION_ERROR')
            );
        }

        console.log(`일괄 작업 요청: ${action}, 대상: ${device_ids.length}개 디바이스`);

        let successCount = 0;
        let failedCount = 0;
        const errors = [];

        for (const deviceId of device_ids) {
            try {
                switch (action) {
                    case 'enable':
                        const enableResult = await getDeviceRepo().updateDeviceStatus(parseInt(deviceId), true, tenantId);
                        if (enableResult) successCount++;
                        else failedCount++;
                        break;
                    case 'disable':
                        const disableResult = await getDeviceRepo().updateDeviceStatus(parseInt(deviceId), false, tenantId);
                        if (disableResult) successCount++;
                        else failedCount++;
                        break;
                    case 'delete':
                        const deleted = await getDeviceRepo().deleteById(parseInt(deviceId), tenantId);
                        if (deleted) successCount++;
                        else failedCount++;
                        break;
                    default:
                        throw new Error(`Unknown action: ${action}`);
                }
            } catch (error) {
                failedCount++;
                errors.push({
                    device_id: deviceId,
                    error: error.message
                });
            }
        }

        const result = {
            total_processed: device_ids.length,
            successful: successCount,
            failed: failedCount,
            errors: errors.length > 0 ? errors : undefined
        };

        console.log(`일괄 작업 완료: 성공 ${successCount}, 실패 ${failedCount}`);
        res.json(createResponse(true, result, `Bulk ${action} completed`));

    } catch (error) {
        console.error('일괄 작업 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'BULK_ACTION_ERROR'));
    }
});

// 🔥 새로 추가: 배치 워커 제어 API
router.post('/batch/start', async (req, res) => {
    try {
        const { device_ids = [] } = req.body;
        
        if (!Array.isArray(device_ids) || device_ids.length === 0) {
            return res.status(400).json({
                success: false,
                error: 'device_ids must be a non-empty array'
            });
        }
        
        console.log(`🚀 Starting ${device_ids.length} devices: ${device_ids.join(', ')}`);
        
        const proxy = getCollectorProxy();
        
        const promises = device_ids.map(async (deviceId) => {
            try {
                const result = await proxy.startDevice(deviceId.toString());
                return {
                    device_id: deviceId,
                    success: true,
                    data: result.data
                };
            } catch (error) {
                return {
                    device_id: deviceId,
                    success: false,
                    error: error.message
                };
            }
        });
        
        const batchResults = await Promise.all(promises);
        const successful = batchResults.filter(r => r.success);
        const failed = batchResults.filter(r => !r.success);
        
        res.json({
            success: failed.length === 0,
            message: `Batch start completed: ${successful.length} successful, ${failed.length} failed`,
            data: {
                total_processed: batchResults.length,
                successful: successful.length,
                failed: failed.length,
                results: batchResults
            },
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        console.error('❌ Batch start failed:', error);
        
        res.status(500).json({
            success: false,
            error: 'Batch start operation failed',
            details: error.message
        });
    }
});

router.post('/batch/stop', async (req, res) => {
    try {
        const { device_ids = [], graceful = true } = req.body;
        
        if (!Array.isArray(device_ids) || device_ids.length === 0) {
            return res.status(400).json({
                success: false,
                error: 'device_ids must be a non-empty array'
            });
        }
        
        console.log(`🛑 Stopping ${device_ids.length} devices: ${device_ids.join(', ')}`);
        
        const proxy = getCollectorProxy();
        
        const promises = device_ids.map(async (deviceId) => {
            try {
                const result = await proxy.stopDevice(deviceId.toString(), { graceful });
                return {
                    device_id: deviceId,
                    success: true,
                    data: result.data
                };
            } catch (error) {
                return {
                    device_id: deviceId,
                    success: false,
                    error: error.message
                };
            }
        });
        
        const batchResults = await Promise.all(promises);
        const successful = batchResults.filter(r => r.success);
        const failed = batchResults.filter(r => !r.success);
        
        res.json({
            success: failed.length === 0,
            message: `Batch stop completed: ${successful.length} successful, ${failed.length} failed`,
            data: {
                total_processed: batchResults.length,
                successful: successful.length,
                failed: failed.length,
                results: batchResults
            },
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        console.error('❌ Batch stop failed:', error);
        
        res.status(500).json({
            success: false,
            error: 'Batch stop operation failed',
            details: error.message
        });
    }
});

// ============================================================================
// 디바이스 CRUD API (기존 + 설정 동기화 통합)
// ============================================================================

router.get('/', async (req, res) => {
    try {
        const { tenantId } = req;
        const {
            page = 1,
            limit = 25,
            protocol_type,
            protocol_id,
            device_type,
            connection_status,
            status,
            site_id,
            search,
            sort_by = 'id',
            sort_order = 'ASC',
            include_rtu_relations = false,
            include_collector_status = false  // 🔥 새로 추가
        } = req.query;

        console.log('디바이스 목록 조회 요청 (RTU + Collector 상태 포함):', {
            tenantId,
            page: parseInt(page),
            limit: parseInt(limit),
            protocol_id: protocol_id ? parseInt(protocol_id) : undefined,
            include_rtu_relations: include_rtu_relations === 'true',
            include_collector_status: include_collector_status === 'true'
        });

        const options = {
            tenantId,
            protocolType: protocol_type,
            protocolId: protocol_id ? parseInt(protocol_id) : null,
            deviceType: device_type,
            connectionStatus: connection_status,
            status,
            siteId: site_id ? parseInt(site_id) : null,
            search,
            page: parseInt(page),
            limit: parseInt(limit),
            sortBy: sort_by,
            sortOrder: sort_order.toUpperCase()
        };

        let result;
        try {
            result = await getDeviceRepo().findAllDevices(options);
            
            if (!result || !result.items || !Array.isArray(result.items)) {
                result = {
                    items: [],
                    pagination: {
                        page: parseInt(page),
                        limit: parseInt(limit),
                        total_items: 0,
                        has_next: false,
                        has_prev: false
                    }
                };
            }

            console.log('RTU 정보 추가 중...');
            result.items = enhanceDevicesWithRtuInfo(result.items);

            if (include_rtu_relations === 'true') {
                console.log('RTU 마스터-슬래이브 관계 정보 추가 중...');
                result.items = await addRtuRelationships(result.items, tenantId);
            }

            // 🔥 새로 추가: Collector 상태 정보 추가
            if (include_collector_status === 'true') {
                try {
                    console.log('Collector 워커 상태 조회 중...');
                    const proxy = getCollectorProxy();
                    const workerResult = await proxy.getWorkerStatus();
                    const workerStatuses = workerResult.data?.workers || {};
                    
                    result.items.forEach(device => {
                        const workerStatus = workerStatuses[device.id.toString()];
                        device.collector_status = workerStatus || {
                            status: 'unknown',
                            message: 'No status available'
                        };
                    });
                    
                    console.log('Collector 상태 정보 추가 완료');
                } catch (collectorError) {
                    console.warn('⚠️ Collector 상태 조회 실패:', collectorError.message);
                }
            }

        } catch (repoError) {
            console.error('Repository 호출 실패:', repoError.message);
            result = {
                items: [],
                pagination: {
                    page: parseInt(page),
                    limit: parseInt(limit),
                    total_items: 0,
                    has_next: false,
                    has_prev: false
                }
            };
        }

        console.log(`디바이스 ${result.items.length}개 조회 완료 (RTU + Collector 정보 포함)`);
        
        const rtuDevices = result.items.filter(d => d.protocol_type === 'MODBUS_RTU');
        const rtuMasters = rtuDevices.filter(d => d.rtu_info && d.rtu_info.is_master);
        const rtuSlaves = rtuDevices.filter(d => d.rtu_info && d.rtu_info.is_slave);
        
        const responseData = createPaginatedResponse(result.items, result.pagination, 'Devices retrieved successfully');
        
        responseData.data.rtu_summary = {
            total_rtu_devices: rtuDevices.length,
            rtu_masters: rtuMasters.length,
            rtu_slaves: rtuSlaves.length,
            rtu_networks: rtuMasters.map(master => ({
                master_id: master.id,
                master_name: master.name,
                serial_port: master.endpoint,
                baud_rate: master.rtu_info ? master.rtu_info.baud_rate : null,
                slave_count: master.rtu_info ? master.rtu_info.slave_count : 0,
                connection_status: master.connection_status
            }))
        };

        res.json(responseData);

    } catch (error) {
        console.error('디바이스 목록 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICES_LIST_ERROR'));
    }
});

router.get('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const { 
            include_data_points = false,
            include_rtu_network = false,
            include_collector_status = false  // 🔥 새로 추가
        } = req.query;

        console.log(`디바이스 ID ${id} 상세 조회 시작 (RTU + Collector 정보 포함)...`);

        const device = await getDeviceRepo().findById(parseInt(id), tenantId);

        if (!device) {
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        const enhancedDevice = enhanceDeviceWithRtuInfo(device);

        // 🔥 새로 추가: Collector 상태 정보
        if (include_collector_status === 'true') {
            try {
                console.log(`Collector 상태 조회: Device ${id}`);
                const proxy = getCollectorProxy();
                const statusResult = await proxy.getDeviceStatus(id);
                enhancedDevice.collector_status = statusResult.data;
            } catch (collectorError) {
                console.warn(`⚠️ Collector 상태 조회 실패 Device ${id}:`, collectorError.message);
                enhancedDevice.collector_status = {
                    error: 'Unable to fetch real-time status',
                    last_attempt: new Date().toISOString()
                };
            }
        }

        if (include_rtu_network === 'true' && enhancedDevice.protocol_type === 'MODBUS_RTU') {
            console.log('RTU 네트워크 정보 조회 중...');
            
            if (enhancedDevice.rtu_info && enhancedDevice.rtu_info.is_master) {
                try {
                    const allDevices = await getDeviceRepo().findAllDevices({ tenantId });
                    const slaves = (allDevices.items || [])
                        .filter(d => d.protocol_type === 'MODBUS_RTU' && d.device_type !== 'GATEWAY')
                        .filter(d => {
                            const slaveConfig = d.config ? JSON.parse(d.config) : {};
                            return slaveConfig.master_device_id === enhancedDevice.id;
                        })
                        .map(slave => enhanceDeviceWithRtuInfo(slave));

                    enhancedDevice.rtu_network = {
                        role: 'master',
                        slaves: slaves,
                        slave_count: slaves.length,
                        network_status: slaves.length > 0 ? 'active' : 'no_slaves',
                        serial_port: enhancedDevice.endpoint,
                        communication_settings: {
                            baud_rate: enhancedDevice.rtu_info.baud_rate,
                            data_bits: enhancedDevice.rtu_info.data_bits,
                            stop_bits: enhancedDevice.rtu_info.stop_bits,
                            parity: enhancedDevice.rtu_info.parity
                        }
                    };
                } catch (networkError) {
                    console.warn('RTU 네트워크 정보 조회 실패:', networkError.message);
                    enhancedDevice.rtu_network = { role: 'master', error: networkError.message };
                }
                
            } else if (enhancedDevice.rtu_info && enhancedDevice.rtu_info.is_slave) {
                try {
                    const masterId = enhancedDevice.rtu_info.master_device_id;
                    if (masterId) {
                        const master = await getDeviceRepo().findById(masterId, tenantId);
                        enhancedDevice.rtu_network = {
                            role: 'slave',
                            master: master ? enhanceDeviceWithRtuInfo(master) : null,
                            slave_id: enhancedDevice.rtu_info.slave_id,
                            serial_port: enhancedDevice.endpoint
                        };
                    }
                } catch (networkError) {
                    console.warn('RTU 마스터 정보 조회 실패:', networkError.message);
                    enhancedDevice.rtu_network = { role: 'slave', error: networkError.message };
                }
            }
        }

        if (include_data_points === 'true') {
            try {
                const dataPoints = await getDeviceRepo().getDataPointsByDevice(enhancedDevice.id, tenantId);
                enhancedDevice.data_points = dataPoints;
                enhancedDevice.data_points_count = dataPoints.length;
            } catch (dpError) {
                console.warn('데이터포인트 조회 실패:', dpError.message);
                enhancedDevice.data_points = [];
                enhancedDevice.data_points_count = 0;
            }
        }

        console.log(`디바이스 ID ${id} 조회 완료`);
        res.json(createResponse(true, enhancedDevice, 'Device retrieved successfully'));

    } catch (error) {
        console.error(`디바이스 ID ${req.params.id} 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_DETAIL_ERROR'));
    }
});

router.post('/', async (req, res) => {
    try {
        const { tenantId, user } = req;
        const deviceData = {
            ...req.body,
            tenant_id: tenantId,
            created_by: user.id,
            created_at: new Date().toISOString()
        };

        console.log('새 디바이스 등록 요청 (protocol_id 직접 사용 + 동기화):', {
            name: deviceData.name,
            protocol_id: deviceData.protocol_id,
            endpoint: deviceData.endpoint
        });

        if (!deviceData.name || !deviceData.protocol_id || !deviceData.endpoint) {
            return res.status(400).json(
                createResponse(false, null, 'Name, protocol_id, and endpoint are required', 'VALIDATION_ERROR')
            );
        }

        const protocolValidation = await validateProtocolId(deviceData.protocol_id, tenantId);
        if (!protocolValidation.valid) {
            return res.status(400).json(
                createResponse(false, null, protocolValidation.error, 'INVALID_PROTOCOL_ID')
            );
        }

        const existingDevice = await getDeviceRepo().findByName(deviceData.name, tenantId);
        if (existingDevice) {
            return res.status(409).json(
                createResponse(false, null, 'Device with this name already exists', 'DEVICE_NAME_CONFLICT')
            );
        }

        const newDevice = await getDeviceRepo().createDevice(deviceData, tenantId);
        const enhancedDevice = enhanceDeviceWithRtuInfo(newDevice);

        // 🔥 새로 추가: Collector 동기화 후크 실행
        try {
            const hooks = getConfigSyncHooks();
            await hooks.afterDeviceCreate(enhancedDevice);
            console.log(`✅ Device created and synced with Collector: ${newDevice.id}`);
        } catch (syncError) {
            console.warn(`⚠️ Device created but sync failed: ${syncError.message}`);
        }

        console.log(`새 디바이스 등록 완료: ID ${newDevice.id} (protocol_id: ${deviceData.protocol_id})`);
        res.status(201).json(createResponse(true, enhancedDevice, 'Device created successfully'));

    } catch (error) {
        console.error('디바이스 등록 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_CREATE_ERROR'));
    }
});

router.put('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const updateData = {
            ...req.body,
            updated_at: new Date().toISOString()
        };

        console.log(`디바이스 ID ${id} 수정 요청 (protocol_id 직접 사용 + 동기화):`, Object.keys(updateData));

        if (updateData.protocol_id !== undefined) {
            const protocolValidation = await validateProtocolId(updateData.protocol_id, tenantId);
            if (!protocolValidation.valid) {
                return res.status(400).json(
                    createResponse(false, null, protocolValidation.error, 'INVALID_PROTOCOL_ID')
                );
            }
        }

        if (updateData.name) {
            const existingDevice = await getDeviceRepo().findByName(updateData.name, tenantId);
            if (existingDevice && existingDevice.id !== parseInt(id)) {
                return res.status(409).json(
                    createResponse(false, null, 'Device with this name already exists', 'DEVICE_NAME_CONFLICT')
                );
            }
        }

        // 🔥 수정: 업데이트 전 기존 디바이스 조회
        const oldDevice = await getDeviceRepo().findById(parseInt(id), tenantId);
        if (!oldDevice) {
            return res.status(404).json(
                createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND')
            );
        }

        const updatedDevice = await getDeviceRepo().updateDeviceInfo(parseInt(id), updateData, tenantId);

        if (!updatedDevice) {
            return res.status(404).json(
                createResponse(false, null, 'Device not found or update failed', 'DEVICE_UPDATE_FAILED')
            );
        }

        const enhancedDevice = enhanceDeviceWithRtuInfo(updatedDevice);

        // 🔥 새로 추가: Collector 동기화 후크 실행
        try {
            const hooks = getConfigSyncHooks();
            await hooks.afterDeviceUpdate(oldDevice, enhancedDevice);
            console.log(`✅ Device updated and synced with Collector: ${id}`);
        } catch (syncError) {
            console.warn(`⚠️ Device updated but sync failed: ${syncError.message}`);
        }

        console.log(`디바이스 ID ${id} 수정 완료`);
        res.json(createResponse(true, enhancedDevice, 'Device updated successfully'));

    } catch (error) {
        console.error(`디바이스 ID ${req.params.id} 수정 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_UPDATE_ERROR'));
    }
});

router.delete('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;

        console.log(`디바이스 ID ${id} 삭제 요청...`);

        // 🔥 수정: 삭제 전 디바이스 정보 조회 (동기화용)
        const device = await getDeviceRepo().findById(parseInt(id), tenantId);
        if (!device) {
            return res.status(404).json(
                createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND')
            );
        }

        try {
            const dataPoints = await getDeviceRepo().getDataPointsByDevice(parseInt(id), tenantId);
            if (dataPoints.length > 0) {
                console.log(`디바이스에 ${dataPoints.length}개의 데이터포인트가 연결되어 있음`);
                
                if (req.query.force !== 'true') {
                    return res.status(409).json(createResponse(
                        false, 
                        { data_points_count: dataPoints.length }, 
                        'Device has associated data points. Use force=true to delete them.', 
                        'DEVICE_HAS_DEPENDENCIES'
                    ));
                }

                console.log(`force=true로 연관 데이터포인트도 함께 삭제됩니다`);
            }
        } catch (dpError) {
            console.warn('데이터포인트 확인 실패, 계속 진행:', dpError.message);
        }

        // 🔥 새로 추가: Collector 동기화 후크 실행 (삭제 전)
        try {
            const hooks = getConfigSyncHooks();
            await hooks.afterDeviceDelete(device);
            console.log(`✅ Device delete synced with Collector: ${id}`);
        } catch (syncError) {
            console.warn(`⚠️ Device delete sync failed: ${syncError.message}`);
        }

        const deleted = await getDeviceRepo().deleteById(parseInt(id), tenantId);

        if (!deleted) {
            return res.status(404).json(
                createResponse(false, null, 'Device not found or delete failed', 'DEVICE_DELETE_FAILED')
            );
        }

        console.log(`디바이스 ID ${id} 삭제 완료`);
        res.json(createResponse(true, { deleted: true }, 'Device deleted successfully'));

    } catch (error) {
        console.error(`디바이스 ID ${req.params.id} 삭제 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_DELETE_ERROR'));
    }
});

// ============================================================================
// 🔥 새로 추가: Collector 프록시 API들 (디바이스 제어)
// ============================================================================

router.post('/:id/start', checkCollectorConnection, async (req, res) => {
    try {
        const deviceId = req.params.id;
        const { force_restart = false } = req.body;
        
        console.log(`🚀 Starting device worker: ${deviceId}`);
        
        const proxy = getCollectorProxy();
        const result = await proxy.startDevice(deviceId, { forceRestart: force_restart });
        
        res.json({
            success: true,
            message: `Device ${deviceId} started successfully`,
            data: result.data,
            device_id: parseInt(deviceId),
            action: 'start',
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        handleCollectorProxyError(error, req, res);
    }
});

router.post('/:id/stop', checkCollectorConnection, async (req, res) => {
    try {
        const deviceId = req.params.id;
        const { graceful = true } = req.body;
        
        console.log(`🛑 Stopping device worker: ${deviceId}`);
        
        const proxy = getCollectorProxy();
        const result = await proxy.stopDevice(deviceId, { graceful });
        
        res.json({
            success: true,
            message: `Device ${deviceId} stopped successfully`,
            data: result.data,
            device_id: parseInt(deviceId),
            action: 'stop',
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        handleCollectorProxyError(error, req, res);
    }
});

router.post('/:id/restart', checkCollectorConnection, async (req, res) => {
    try {
        const deviceId = req.params.id;
        
        console.log(`🔄 Restarting device worker: ${deviceId}`);
        
        const proxy = getCollectorProxy();
        const result = await proxy.restartDevice(deviceId);
        
        res.json({
            success: true,
            message: `Device ${deviceId} restarted successfully`,
            data: result.data,
            device_id: parseInt(deviceId),
            action: 'restart',
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        handleCollectorProxyError(error, req, res);
    }
});

router.get('/:id/status', checkCollectorConnection, async (req, res) => {
    try {
        const deviceId = req.params.id;
        
        const proxy = getCollectorProxy();
        const result = await proxy.getDeviceStatus(deviceId);
        
        res.json({
            success: true,
            data: result.data,
            device_id: parseInt(deviceId),
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        handleCollectorProxyError(error, req, res);
    }
});

router.get('/:id/data/current', checkCollectorConnection, async (req, res) => {
    try {
        const deviceId = req.params.id;
        const { point_ids } = req.query;
        
        const proxy = getCollectorProxy();
        const pointIds = point_ids ? point_ids.split(',').map(id => id.trim()) : [];
        const result = await proxy.getCurrentData(deviceId, pointIds);
        
        res.json({
            success: true,
            data: result.data,
            device_id: parseInt(deviceId),
            point_count: result.data?.points?.length || 0,
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        handleCollectorProxyError(error, req, res);
    }
});

router.post('/:id/start', checkCollectorConnection, async (req, res) => {
    try {
        const deviceId = req.params.id;
        const { force_restart = false } = req.body;
        
        console.log(`🚀 Starting device worker: ${deviceId}`);
        
        const proxy = getCollectorProxy();
        const result = await proxy.startDevice(deviceId, { forceRestart: force_restart });
        
        res.json({
            success: true,
            message: `Device ${deviceId} started successfully`,
            data: result.data,
            device_id: parseInt(deviceId),
            action: 'start',
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        handleCollectorProxyError(error, req, res);
    }
});

// 🔥 누락된 API 2: 워커 정지  
router.post('/:id/stop', checkCollectorConnection, async (req, res) => {
    try {
        const deviceId = req.params.id;
        const { graceful = true } = req.body;
        
        console.log(`🛑 Stopping device worker: ${deviceId}`);
        
        const proxy = getCollectorProxy();
        const result = await proxy.stopDevice(deviceId, { graceful });
        
        res.json({
            success: true,
            message: `Device ${deviceId} stopped successfully`,
            data: result.data,
            device_id: parseInt(deviceId),
            action: 'stop',
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        handleCollectorProxyError(error, req, res);
    }
});
// ============================================================================
// 🔥 새로 추가: 하드웨어 제어 API
// ============================================================================

router.post('/:id/digital/:outputId/control', checkCollectorConnection, async (req, res) => {
    try {
        const { id: deviceId, outputId } = req.params;
        const { state, duration, force = false } = req.body;
        
        if (state === undefined || state === null) {
            return res.status(400).json({
                success: false,
                error: 'Missing required parameter: state (true/false)'
            });
        }
        
        console.log(`🔌 Digital control: Device ${deviceId}, Output ${outputId}, State: ${state}`);
        
        const proxy = getCollectorProxy();
        const result = await proxy.controlDigitalOutput(deviceId, outputId, state, { duration, force });
        
        res.json({
            success: true,
            message: `Digital output ${outputId} set to ${state}`,
            data: result.data,
            device_id: parseInt(deviceId),
            output_id: outputId,
            state: Boolean(state),
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        handleCollectorProxyError(error, req, res);
    }
});

router.post('/:id/analog/:outputId/control', checkCollectorConnection, async (req, res) => {
    try {
        const { id: deviceId, outputId } = req.params;
        const { value, unit, ramp_time } = req.body;
        
        if (value === undefined || value === null) {
            return res.status(400).json({
                success: false,
                error: 'Missing required parameter: value (number)'
            });
        }
        
        console.log(`📊 Analog control: Device ${deviceId}, Output ${outputId}, Value: ${value}`);
        
        const proxy = getCollectorProxy();
        const result = await proxy.controlAnalogOutput(deviceId, outputId, value, { unit, rampTime: ramp_time });
        
        res.json({
            success: true,
            message: `Analog output ${outputId} set to ${value}${unit ? ' ' + unit : ''}`,
            data: result.data,
            device_id: parseInt(deviceId),
            output_id: outputId,
            value: Number(value),
            unit: unit || null,
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        handleCollectorProxyError(error, req, res);
    }
});

router.post('/:id/config/reload', checkCollectorConnection, async (req, res) => {
    try {
        const deviceId = req.params.id;
        
        console.log(`🔄 Reloading config for device ${deviceId}`);
        
        const proxy = getCollectorProxy();
        const result = await proxy.reloadDeviceConfig(deviceId);
        
        res.json({
            success: true,
            message: `Configuration reloaded for device ${deviceId}`,
            data: result.data,
            device_id: parseInt(deviceId),
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        handleCollectorProxyError(error, req, res);
    }
});

// ============================================================================
// 기존 API들 (그대로 유지)
// ============================================================================

router.post('/:id/enable', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;

        console.log(`디바이스 ID ${id} 활성화 요청...`);

        const updatedDevice = await getDeviceRepo().updateDeviceStatus(parseInt(id), true, tenantId);

        if (!updatedDevice) {
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        const enhancedDevice = enhanceDeviceWithRtuInfo(updatedDevice);

        console.log(`디바이스 ID ${id} 활성화 완료`);
        res.json(createResponse(true, enhancedDevice, 'Device enabled successfully'));

    } catch (error) {
        console.error(`디바이스 ID ${req.params.id} 활성화 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_ENABLE_ERROR'));
    }
});

router.post('/:id/disable', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;

        console.log(`디바이스 ID ${id} 비활성화 요청...`);

        const updatedDevice = await getDeviceRepo().updateDeviceStatus(parseInt(id), false, tenantId);

        if (!updatedDevice) {
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        const enhancedDevice = enhanceDeviceWithRtuInfo(updatedDevice);

        console.log(`디바이스 ID ${id} 비활성화 완료`);
        res.json(createResponse(true, enhancedDevice, 'Device disabled successfully'));

    } catch (error) {
        console.error(`디바이스 ID ${req.params.id} 비활성화 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_DISABLE_ERROR'));
    }
});

router.post('/:id/test-connection', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;

        console.log(`디바이스 ID ${id} 연결 테스트 요청...`);

        const device = await getDeviceRepo().findById(parseInt(id), tenantId);
        if (!device) {
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        const enhancedDevice = enhanceDeviceWithRtuInfo(device);

        const isSuccessful = Math.random() > 0.1;
        const responseTime = Math.floor(Math.random() * 200) + 50;
        
        const testResult = {
            device_id: device.id,
            device_name: device.name,
            endpoint: device.endpoint,
            protocol_type: device.protocol_type,
            test_successful: isSuccessful,
            response_time_ms: responseTime,
            test_timestamp: new Date().toISOString(),
            error_message: isSuccessful ? null : 'Connection timeout or unreachable',
            rtu_info: enhancedDevice.rtu_info
        };

        const newConnectionStatus = isSuccessful ? 'connected' : 'disconnected';
        await getDeviceRepo().updateDeviceConnection(
            device.id,
            newConnectionStatus,
            isSuccessful ? new Date().toISOString() : null,
            tenantId
        );

        console.log(`디바이스 ID ${id} 연결 테스트 완료: ${isSuccessful ? '성공' : '실패'}`);
        res.json(createResponse(true, testResult, `Connection test ${isSuccessful ? 'successful' : 'failed'}`));

    } catch (error) {
        console.error(`디바이스 ID ${req.params.id} 연결 테스트 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'CONNECTION_TEST_ERROR'));
    }
});

router.get('/:id/data-points', async (req, res) => {
    const startTime = Date.now();
    console.log('\n' + '='.repeat(80));
    console.log('API 호출 시작: GET /api/devices/:id/data-points');
    
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const {
            page = 1,
            limit = 100,
            data_type,
            enabled_only = false
        } = req.query;

        console.log('처리 시작: 디바이스 ID', id, '데이터포인트 조회...');

        let device = null;
        try {
            device = await getDeviceRepo().findById(parseInt(id), tenantId);
            console.log('디바이스 조회 결과:', device ? `${device.name} (ID: ${device.id})` : 'null');
        } catch (deviceError) {
            console.error('디바이스 조회 오류:', deviceError.message);
            return res.status(500).json(createResponse(false, null, `디바이스 조회 실패: ${deviceError.message}`, 'DEVICE_QUERY_ERROR'));
        }

        if (!device) {
            console.warn('디바이스를 찾을 수 없음: ID', id);
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        let dataPoints = [];
        try {
            console.log('데이터포인트 조회 중...');
            dataPoints = await getDeviceRepo().getDataPointsByDevice(device.id, tenantId);
            console.log('조회된 데이터포인트 수 =', dataPoints ? dataPoints.length : 0);
            
        } catch (dataPointError) {
            console.error('데이터포인트 조회 오류:', dataPointError.message);
            dataPoints = [];
        }

        if (!Array.isArray(dataPoints)) {
            dataPoints = [];
        }

        let filteredPoints = dataPoints;
        
        if (data_type) {
            const beforeFilter = filteredPoints.length;
            filteredPoints = filteredPoints.filter(dp => dp.data_type === data_type);
            console.log(`데이터 타입 필터 (${data_type}): ${beforeFilter} → ${filteredPoints.length}`);
        }
        
        if (enabled_only === 'true') {
            const beforeFilter = filteredPoints.length;
            filteredPoints = filteredPoints.filter(dp => dp.is_enabled);
            console.log(`활성화 필터: ${beforeFilter} → ${filteredPoints.length}`);
        }

        const pageNum = parseInt(page);
        const limitNum = parseInt(limit);
        const offset = (pageNum - 1) * limitNum;
        const paginatedPoints = filteredPoints.slice(offset, offset + limitNum);

        const pagination = {
            page: pageNum,
            limit: limitNum,
            total_items: filteredPoints.length,
            has_next: offset + limitNum < filteredPoints.length,
            has_prev: pageNum > 1
        };

        const responseData = createPaginatedResponse(
            paginatedPoints, 
            pagination, 
            `Device data points retrieved successfully`
        );

        const processingTime = Date.now() - startTime;
        console.log('API 완료: 총 처리시간', processingTime, 'ms');
        console.log('='.repeat(80) + '\n');

        res.json(responseData);

    } catch (error) {
        const processingTime = Date.now() - startTime;
        console.error('API 전체 실패:', error.message);
        console.error('실패까지 소요시간:', processingTime, 'ms');
        
        res.status(500).json(createResponse(false, null, error.message, 'DATA_POINTS_API_ERROR'));
    }
});

router.get('/rtu/networks', async (req, res) => {
    try {
        const { tenantId } = req;

        console.log('RTU 네트워크 정보 조회...');

        const devicesResult = await getDeviceRepo().findAllDevices({ tenantId });
        const devices = enhanceDevicesWithRtuInfo(devicesResult.items || []);
        const devicesWithRelations = await addRtuRelationships(devices, tenantId);
        
        const rtuMasters = devicesWithRelations.filter(d => 
            d.protocol_type === 'MODBUS_RTU' && d.rtu_info && d.rtu_info.is_master
        );

        const networks = rtuMasters.map(master => ({
            network_id: `rtu_network_${master.id}`,
            master: {
                device_id: master.id,
                device_name: master.name,
                serial_port: master.endpoint,
                connection_status: master.connection_status,
                settings: {
                    baud_rate: master.rtu_info.baud_rate,
                    data_bits: master.rtu_info.data_bits,
                    stop_bits: master.rtu_info.stop_bits,
                    parity: master.rtu_info.parity
                }
            },
            slaves: master.rtu_info.slaves || [],
            slave_count: master.rtu_info.slave_count || 0,
            network_status: master.connection_status === 'connected' ? 'active' : 'inactive'
        }));

        const summary = {
            total_networks: networks.length,
            active_networks: networks.filter(n => n.network_status === 'active').length,
            total_slaves: networks.reduce((sum, n) => sum + n.slave_count, 0),
            networks: networks
        };

        console.log(`RTU 네트워크 ${networks.length}개 조회 완료`);
        res.json(createResponse(true, summary, 'RTU networks retrieved successfully'));

    } catch (error) {
        console.error('RTU 네트워크 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'RTU_NETWORKS_ERROR'));
    }
});

router.get('/debug/direct', async (req, res) => {
    try {
        const dbPath = configManager.get('SQLITE_PATH', './data/db/pulseone.db');
        console.log(`직접 SQLite 조회: ${dbPath}`);

        const devices = await new Promise((resolve, reject) => {
            const db = new sqlite3.Database(dbPath, (err) => {
                if (err) {
                    reject(new Error(`Database connection failed: ${err.message}`));
                    return;
                }
            });

            const sql = `
                SELECT 
                    d.id, d.tenant_id, d.site_id, d.device_group_id, d.edge_server_id,
                    d.name, d.description, d.device_type, d.manufacturer, d.model, 
                    d.serial_number, d.protocol_id, d.endpoint, d.config,
                    d.polling_interval, d.timeout, d.retry_count, d.is_enabled,
                    d.installation_date, d.last_maintenance, d.created_at, d.updated_at,
                    p.protocol_type, p.display_name as protocol_name
                FROM devices d
                LEFT JOIN protocols p ON d.protocol_id = p.id
                WHERE d.tenant_id = 1
                ORDER BY d.id
                LIMIT 20
            `;

            db.all(sql, [], (err, rows) => {
                if (err) {
                    db.close();
                    reject(new Error(`Query failed: ${err.message}`));
                    return;
                }
                
                db.close();
                resolve(rows);
            });
        });

        const enhancedDevices = enhanceDevicesWithRtuInfo(devices.map(device => ({
            ...device,
            is_enabled: !!device.is_enabled
        })));

        console.log(`SQLite 직접 조회 결과: ${devices.length}개 디바이스 (protocol_id 직접 처리)`);
        
        res.json({
            success: true,
            debug: true,
            source: 'direct_sqlite',
            database_path: dbPath,
            data: {
                devices: enhancedDevices,
                count: enhancedDevices.length,
                schema_info: 'protocol_id used directly without conversion'
            },
            message: 'Direct SQLite query successful with direct protocol_id usage'
        });

    } catch (error) {
        console.error('SQLite 직접 조회 실패:', error.message);
        res.status(500).json({
            success: false,
            debug: true,
            error: error.message,
            database_path: configManager.get('SQLITE_PATH', './data/db/pulseone.db')
        });
    }
});

router.get('/debug/repository', async (req, res) => {
    try {
        const repo = getDeviceRepo();
        
        res.json({
            success: true,
            debug: true,
            repository_info: {
                type: typeof repo,
                has_db_factory: !!repo.dbFactory,
                db_factory_type: repo.dbFactory ? typeof repo.dbFactory : null,
                methods: Object.getOwnPropertyNames(Object.getPrototypeOf(repo)),
                config: {
                    database_type: configManager.get('DATABASE_TYPE'),
                    sqlite_path: configManager.get('SQLITE_PATH')
                },
                protocol_support: 'Direct protocol_id usage (no conversion)'
            }
        });

    } catch (error) {
        console.error('Repository 디버깅 실패:', error.message);
        res.status(500).json({
            success: false,
            debug: true,
            error: error.message
        });
    }
});

module.exports = router;