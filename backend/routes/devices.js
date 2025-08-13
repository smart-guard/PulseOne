// ============================================================================
// backend/routes/devices.js
// ConfigManager를 활용한 확장 가능한 디바이스 관리 API
// ============================================================================

const express = require('express');
const router = express.Router();
const DeviceRepository = require('../lib/database/repositories/DeviceRepository');
const SiteRepository = require('../lib/database/repositories/SiteRepository');
const ConfigManager = require('../lib/config/ConfigManager');
const { 
    authenticateToken, 
    tenantIsolation, 
    checkSiteAccess,
    checkDeviceAccess,
    requireRole,
    requirePermission,
    validateTenantStatus
} = require('../middleware/tenantIsolation');

// Repository 및 ConfigManager 인스턴스
const deviceRepo = new DeviceRepository();
const siteRepo = new SiteRepository();
const configManager = ConfigManager.getInstance();

// ConfigManager 초기화
configManager.initialize().catch(console.error);

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

// ============================================================================
// 🔧 설정 관련 API 엔드포인트들
// ============================================================================

/**
 * GET /api/devices/config/protocols
 * 지원하는 프로토콜 목록 조회
 */
router.get('/config/protocols', 
    authenticateToken, 
    async (req, res) => {
        try {
            const protocols = await configManager.getSupportedProtocols();
            res.json(createResponse(true, protocols, 'Supported protocols retrieved successfully'));
        } catch (error) {
            console.error('Get protocols error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'PROTOCOLS_FETCH_ERROR'));
        }
    }
);

/**
 * GET /api/devices/config/device-types
 * 디바이스 타입 목록 조회
 */
router.get('/config/device-types', 
    authenticateToken, 
    async (req, res) => {
        try {
            const deviceTypes = await configManager.getDeviceTypes();
            res.json(createResponse(true, deviceTypes, 'Device types retrieved successfully'));
        } catch (error) {
            console.error('Get device types error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'DEVICE_TYPES_FETCH_ERROR'));
        }
    }
);

/**
 * GET /api/devices/config/status-types
 * 디바이스 상태 목록 조회
 */
router.get('/config/status-types', 
    authenticateToken, 
    async (req, res) => {
        try {
            const statusTypes = await configManager.getDeviceStatus();
            res.json(createResponse(true, statusTypes, 'Device status types retrieved successfully'));
        } catch (error) {
            console.error('Get status types error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'STATUS_TYPES_FETCH_ERROR'));
        }
    }
);

/**
 * POST /api/devices/config/protocols
 * 새 프로토콜 추가 (시스템 관리자만)
 */
router.post('/config/protocols', 
    authenticateToken, 
    tenantIsolation,
    requireRole('system_admin'),
    async (req, res) => {
        try {
            const protocolData = req.body;
            
            // 필수 필드 검증
            const requiredFields = ['value', 'name', 'description'];
            const missingFields = requiredFields.filter(field => !protocolData[field]);
            
            if (missingFields.length > 0) {
                return res.status(400).json(
                    createResponse(false, `Missing required fields: ${missingFields.join(', ')}`, null, 'MISSING_REQUIRED_FIELDS')
                );
            }

            await configManager.addProtocol(protocolData);
            res.status(201).json(createResponse(true, protocolData, 'Protocol added successfully'));
        } catch (error) {
            console.error('Add protocol error:', error);
            
            if (error.message.includes('already exists')) {
                res.status(409).json(createResponse(false, error.message, null, 'PROTOCOL_ALREADY_EXISTS'));
            } else {
                res.status(500).json(createResponse(false, error.message, null, 'PROTOCOL_ADD_ERROR'));
            }
        }
    }
);

/**
 * POST /api/devices/config/device-types
 * 새 디바이스 타입 추가 (시스템 관리자만)
 */
router.post('/config/device-types', 
    authenticateToken, 
    tenantIsolation,
    requireRole('system_admin'),
    async (req, res) => {
        try {
            const typeData = req.body;
            
            // 필수 필드 검증
            const requiredFields = ['value', 'name', 'description'];
            const missingFields = requiredFields.filter(field => !typeData[field]);
            
            if (missingFields.length > 0) {
                return res.status(400).json(
                    createResponse(false, `Missing required fields: ${missingFields.join(', ')}`, null, 'MISSING_REQUIRED_FIELDS')
                );
            }

            await configManager.addDeviceType(typeData);
            res.status(201).json(createResponse(true, typeData, 'Device type added successfully'));
        } catch (error) {
            console.error('Add device type error:', error);
            
            if (error.message.includes('already exists')) {
                res.status(409).json(createResponse(false, error.message, null, 'DEVICE_TYPE_ALREADY_EXISTS'));
            } else {
                res.status(500).json(createResponse(false, error.message, null, 'DEVICE_TYPE_ADD_ERROR'));
            }
        }
    }
);

// ============================================================================
// 🌐 메인 디바이스 API 엔드포인트들
// ============================================================================

/**
 * GET /api/devices
 * 디바이스 목록 조회 - 페이징, 필터링, 검색 지원
 */
router.get('/', 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus,
    async (req, res) => {
        try {
            const { tenantId, isSystemAdmin, siteAccess } = req;
            const {
                page = 1,
                limit = 20,
                site_id,
                protocol,
                device_type,
                status,
                search,
                include_child_sites = false,
                manufacturer,
                sort_by = 'name',
                sort_order = 'asc'
            } = req.query;

            // 동적 필터 유효성 검증
            if (protocol) {
                const isValidProtocol = await configManager.validateValue('protocols', protocol);
                if (!isValidProtocol) {
                    return res.status(400).json(
                        createResponse(false, `Invalid protocol: ${protocol}`, null, 'INVALID_PROTOCOL')
                    );
                }
            }

            if (device_type) {
                const isValidDeviceType = await configManager.validateValue('deviceTypes', device_type);
                if (!isValidDeviceType) {
                    return res.status(400).json(
                        createResponse(false, `Invalid device type: ${device_type}`, null, 'INVALID_DEVICE_TYPE')
                    );
                }
            }

            if (status) {
                const isValidStatus = await configManager.validateValue('deviceStatus', status);
                if (!isValidStatus) {
                    return res.status(400).json(
                        createResponse(false, `Invalid status: ${status}`, null, 'INVALID_STATUS')
                    );
                }
            }

            // 필터 객체 생성
            const filters = {};
            if (site_id) filters.site_id = site_id;
            if (protocol) filters.protocol = protocol;
            if (device_type) filters.device_type = device_type;
            if (status) filters.status = status;
            if (manufacturer) filters.manufacturer = manufacturer;
            if (search) filters.search = search;

            // Repository를 통한 데이터 조회
            const result = await deviceRepo.findWithPagination(
                filters,
                isSystemAdmin ? null : tenantId,
                parseInt(page),
                parseInt(limit),
                sort_by,
                sort_order.toUpperCase()
            );

            // 사이트 접근 권한 필터링
            if (siteAccess && siteAccess.length > 0) {
                result.devices = result.devices.filter(device => 
                    siteAccess.includes(device.site_id)
                );
                
                // 필터링 후 총 개수 재계산
                const totalFiltered = result.devices.length;
                result.pagination.total_items = totalFiltered;
                result.pagination.total_pages = Math.ceil(totalFiltered / limit);
            }

            res.json(createResponse(true, result, 'Devices retrieved successfully'));

        } catch (error) {
            console.error('Get devices error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'DEVICES_FETCH_ERROR'));
        }
    }
);

/**
 * GET /api/devices/:id
 * 특정 디바이스 상세 조회
 */
router.get('/:id', 
    authenticateToken, 
    tenantIsolation, 
    checkDeviceAccess,
    validateTenantStatus,
    async (req, res) => {
        try {
            const { id } = req.params;
            const { tenantId, isSystemAdmin } = req;

            const device = await deviceRepo.findById(
                parseInt(id), 
                isSystemAdmin ? null : tenantId
            );

            if (!device) {
                return res.status(404).json(
                    createResponse(false, 'Device not found', null, 'DEVICE_NOT_FOUND')
                );
            }

            // 연결 설정 파싱
            if (device.connection_config) {
                try {
                    device.connection_config = JSON.parse(device.connection_config);
                } catch (e) {
                    console.warn(`Failed to parse connection_config for device ${id}`);
                }
            }

            res.json(createResponse(true, device, 'Device details retrieved successfully'));

        } catch (error) {
            console.error('Get device details error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'DEVICE_DETAIL_ERROR'));
        }
    }
);

/**
 * POST /api/devices
 * 새 디바이스 생성
 */
router.post('/', 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus,
    requireRole('system_admin', 'company_admin', 'site_admin'),
    requirePermission('manage_devices'),
    async (req, res) => {
        try {
            const { tenantId, isSystemAdmin } = req;
            const deviceData = req.body;

            // 필수 필드 검증
            const requiredFields = ['name', 'site_id', 'device_type', 'protocol'];
            const missingFields = requiredFields.filter(field => !deviceData[field]);
            
            if (missingFields.length > 0) {
                return res.status(400).json(
                    createResponse(false, `Missing required fields: ${missingFields.join(', ')}`, null, 'MISSING_REQUIRED_FIELDS')
                );
            }

            // 동적 유효성 검증
            const isValidProtocol = await configManager.validateValue('protocols', deviceData.protocol);
            if (!isValidProtocol) {
                return res.status(400).json(
                    createResponse(false, `Unsupported protocol: ${deviceData.protocol}`, null, 'INVALID_PROTOCOL')
                );
            }

            const isValidDeviceType = await configManager.validateValue('deviceTypes', deviceData.device_type);
            if (!isValidDeviceType) {
                return res.status(400).json(
                    createResponse(false, `Invalid device type: ${deviceData.device_type}`, null, 'INVALID_DEVICE_TYPE')
                );
            }

            // 테넌트 설정
            const targetTenantId = isSystemAdmin && deviceData.tenant_id ? deviceData.tenant_id : tenantId;

            // 사이트 존재 확인
            const site = await siteRepo.findById(deviceData.site_id, targetTenantId);
            if (!site) {
                return res.status(400).json(
                    createResponse(false, 'Site not found', null, 'SITE_NOT_FOUND')
                );
            }

            // IP 주소 중복 확인 (선택적)
            if (deviceData.ip_address) {
                const duplicateDevices = await deviceRepo.findByIpAddress(deviceData.ip_address, targetTenantId);
                if (duplicateDevices.length > 0) {
                    return res.status(409).json(
                        createResponse(false, `Device with IP ${deviceData.ip_address} already exists`, null, 'DUPLICATE_IP_ADDRESS')
                    );
                }
            }

            // 시리얼 번호 중복 확인 (선택적)
            if (deviceData.serial_number) {
                const isDuplicate = await deviceRepo.checkSerialDuplicate(deviceData.serial_number, targetTenantId);
                if (isDuplicate) {
                    return res.status(409).json(
                        createResponse(false, `Device with serial number ${deviceData.serial_number} already exists`, null, 'DUPLICATE_SERIAL_NUMBER')
                    );
                }
            }

            // 디바이스 생성
            const deviceId = await deviceRepo.save(deviceData, targetTenantId);
            
            // 생성된 디바이스 조회
            const newDevice = await deviceRepo.findById(deviceId, targetTenantId);

            res.status(201).json(createResponse(true, newDevice, 'Device created successfully'));

        } catch (error) {
            console.error('Create device error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'DEVICE_CREATE_ERROR'));
        }
    }
);

/**
 * PUT /api/devices/:id
 * 디바이스 정보 수정
 */
router.put('/:id', 
    authenticateToken, 
    tenantIsolation, 
    checkDeviceAccess,
    validateTenantStatus,
    requireRole('system_admin', 'company_admin', 'site_admin'),
    requirePermission('manage_devices'),
    async (req, res) => {
        try {
            const { id } = req.params;
            const { tenantId, isSystemAdmin } = req;
            const updateData = req.body;

            // 기존 디바이스 존재 확인
            const existingDevice = await deviceRepo.findById(
                parseInt(id), 
                isSystemAdmin ? null : tenantId
            );

            if (!existingDevice) {
                return res.status(404).json(
                    createResponse(false, 'Device not found', null, 'DEVICE_NOT_FOUND')
                );
            }

            // 동적 유효성 검증
            if (updateData.protocol) {
                const isValidProtocol = await configManager.validateValue('protocols', updateData.protocol);
                if (!isValidProtocol) {
                    return res.status(400).json(
                        createResponse(false, `Unsupported protocol: ${updateData.protocol}`, null, 'INVALID_PROTOCOL')
                    );
                }
            }

            if (updateData.device_type) {
                const isValidDeviceType = await configManager.validateValue('deviceTypes', updateData.device_type);
                if (!isValidDeviceType) {
                    return res.status(400).json(
                        createResponse(false, `Invalid device type: ${updateData.device_type}`, null, 'INVALID_DEVICE_TYPE')
                    );
                }
            }

            // IP 주소 중복 확인 (변경하는 경우만)
            if (updateData.ip_address && updateData.ip_address !== existingDevice.ip_address) {
                const duplicateDevices = await deviceRepo.findByIpAddress(
                    updateData.ip_address, 
                    isSystemAdmin ? null : tenantId
                );
                
                // 자기 자신 제외하고 중복 확인
                const otherDevices = duplicateDevices.filter(device => device.id !== parseInt(id));
                if (otherDevices.length > 0) {
                    return res.status(409).json(
                        createResponse(false, `Device with IP ${updateData.ip_address} already exists`, null, 'DUPLICATE_IP_ADDRESS')
                    );
                }
            }

            // 디바이스 수정
            const success = await deviceRepo.update(
                parseInt(id), 
                updateData, 
                isSystemAdmin ? null : tenantId
            );

            if (!success) {
                return res.status(400).json(
                    createResponse(false, 'Failed to update device', null, 'UPDATE_FAILED')
                );
            }

            // 수정된 디바이스 조회
            const updatedDevice = await deviceRepo.findById(
                parseInt(id), 
                isSystemAdmin ? null : tenantId
            );

            res.json(createResponse(true, updatedDevice, 'Device updated successfully'));

        } catch (error) {
            console.error('Update device error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'DEVICE_UPDATE_ERROR'));
        }
    }
);

/**
 * DELETE /api/devices/:id
 * 디바이스 삭제 (소프트 삭제)
 */
router.delete('/:id', 
    authenticateToken, 
    tenantIsolation, 
    checkDeviceAccess,
    validateTenantStatus,
    requireRole('system_admin', 'company_admin', 'site_admin'),
    requirePermission('manage_devices'),
    async (req, res) => {
        try {
            const { id } = req.params;
            const { tenantId, isSystemAdmin } = req;

            // 디바이스 존재 확인
            const existingDevice = await deviceRepo.findById(
                parseInt(id), 
                isSystemAdmin ? null : tenantId
            );

            if (!existingDevice) {
                return res.status(404).json(
                    createResponse(false, 'Device not found', null, 'DEVICE_NOT_FOUND')
                );
            }

            // TODO: 연결된 데이터 포인트나 알람 확인 후 결정
            // const hasDataPoints = await dataPointRepo.countByDevice(id);
            // if (hasDataPoints > 0) {
            //     return res.status(400).json(
            //         createResponse(false, 'Cannot delete device with data points', null, 'HAS_DATA_POINTS')
            //     );
            // }

            // 디바이스 삭제
            const success = await deviceRepo.deleteById(
                parseInt(id), 
                isSystemAdmin ? null : tenantId
            );

            if (!success) {
                return res.status(400).json(
                    createResponse(false, 'Failed to delete device', null, 'DELETE_FAILED')
                );
            }

            res.json(createResponse(true, { deleted_device_id: parseInt(id) }, 'Device deleted successfully'));

        } catch (error) {
            console.error('Delete device error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'DEVICE_DELETE_ERROR'));
        }
    }
);

// ============================================================================
// 🔧 디바이스 제어 및 테스트 API
// ============================================================================

/**
 * POST /api/devices/:id/test-connection
 * 디바이스 연결 테스트
 */
router.post('/:id/test-connection', 
    authenticateToken, 
    tenantIsolation, 
    checkDeviceAccess,
    validateTenantStatus,
    requirePermission('test_devices'),
    async (req, res) => {
        try {
            const { id } = req.params;
            const { tenantId, isSystemAdmin } = req;

            // 연결 설정 조회
            const connectionConfig = await deviceRepo.getConnectionConfig(parseInt(id));
            
            if (!connectionConfig) {
                return res.status(404).json(
                    createResponse(false, 'Device not found', null, 'DEVICE_NOT_FOUND')
                );
            }

            // TODO: 실제 연결 테스트 로직 구현
            // 지금은 시뮬레이션
            const testResult = {
                device_id: parseInt(id),
                protocol: connectionConfig.protocol,
                connection_status: 'success', // success, timeout, error
                response_time: Math.random() * 100 + 50, // ms
                test_timestamp: new Date().toISOString(),
                details: {
                    ip_address: connectionConfig.ip_address,
                    port: connectionConfig.port,
                    protocol_specific: {}
                }
            };

            // 연결 테스트 결과에 따라 디바이스 상태 업데이트
            const newStatus = testResult.connection_status === 'success' ? 'connected' : 'error';
            await deviceRepo.updateStatus(parseInt(id), newStatus);

            res.json(createResponse(true, testResult, 'Connection test completed'));

        } catch (error) {
            console.error('Test connection error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'CONNECTION_TEST_ERROR'));
        }
    }
);

/**
 * GET /api/devices/statistics
 * 디바이스 통계 조회
 */
router.get('/statistics', 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus,
    async (req, res) => {
        try {
            const { tenantId, isSystemAdmin } = req;

            const targetTenantId = isSystemAdmin ? null : tenantId;

            // Repository에서 통계 조회
            const [
                tenantStats,
                protocolStats,
                siteStats
            ] = await Promise.all([
                deviceRepo.getStatsByTenant(targetTenantId || 1), // 시스템 관리자용 처리 필요
                deviceRepo.getStatsByProtocol(targetTenantId || 1),
                deviceRepo.getStatsBySite(targetTenantId || 1)
            ]);

            const statistics = {
                tenant_stats: tenantStats,
                protocol_stats: protocolStats,
                site_stats: siteStats,
                generated_at: new Date().toISOString()
            };

            res.json(createResponse(true, statistics, 'Device statistics retrieved successfully'));

        } catch (error) {
            console.error('Get device statistics error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'STATISTICS_FETCH_ERROR'));
        }
    }
);

module.exports = router;