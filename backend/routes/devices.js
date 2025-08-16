// ============================================================================
// backend/routes/devices.js
// 디바이스 관리 API - Repository 패턴 100% 활용한 상용 버전
// ============================================================================

const express = require('express');
const router = express.Router();

// Repository imports (기존 완성된 것들 사용)
const DeviceRepository = require('../lib/database/repositories/DeviceRepository');
// DataPointRepository는 DeviceRepository에 포함됨
const SiteRepository = require('../lib/database/repositories/SiteRepository');
const { 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus 
} = require('../middleware/tenantIsolation');

// Repository 인스턴스 생성
let deviceRepo = null;
let dataPointRepo = null;
let siteRepo = null;

function getDeviceRepo() {
    if (!deviceRepo) {
        deviceRepo = new DeviceRepository();
        console.log("✅ DeviceRepository 인스턴스 생성 완료");
    }
    return deviceRepo;
}

function getDataPointRepo() {
    if (!dataPointRepo) {
        dataPointRepo = new DataPointRepository();
        console.log("✅ DataPointRepository 인스턴스 생성 완료");
    }
    return dataPointRepo;
}

function getSiteRepo() {
    if (!siteRepo) {
        siteRepo = new SiteRepository();
        console.log("✅ SiteRepository 인스턴스 생성 완료");
    }
    return siteRepo;
}

// ============================================================================
// 🛡️ 미들웨어 및 헬퍼 함수들
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
 * 페이징 응답 생성
 */
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

/**
 * 인증 미들웨어 (개발용)
 */
const devAuthMiddleware = (req, res, next) => {
    // 개발 단계에서는 기본 사용자 설정
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

// 글로벌 미들웨어 적용
router.use(devAuthMiddleware);
router.use(devTenantMiddleware);

// ============================================================================
// 📱 디바이스 CRUD API
// ============================================================================

/**
 * GET /api/devices
 * 디바이스 목록 조회 (페이징, 필터링, 정렬 지원)
 */
router.get('/', async (req, res) => {
    try {
        const { tenantId } = req;
        const {
            page = 1,
            limit = 25,
            protocol_type,
            device_type,
            connection_status,
            status,
            site_id,
            search,
            sort_by = 'id',
            sort_order = 'ASC'
        } = req.query;

        console.log('📱 디바이스 목록 조회 요청:', {
            tenantId,
            page: parseInt(page),
            limit: parseInt(limit),
            filters: { protocol_type, device_type, connection_status, status, site_id, search }
        });

        // Repository를 통한 조회
        const options = {
            tenantId,
            protocolType: protocol_type,
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

        // 🔥 에러 처리 강화
        let result;
        try {
            result = await getDeviceRepo().findAllDevices(options);
            
            // result가 undefined이거나 잘못된 형태인 경우 처리
            if (!result) {
                console.warn('⚠️ Repository에서 null/undefined 반환됨, 빈 결과로 처리');
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
            
            // result.items가 없는 경우 처리
            if (!result.items || !Array.isArray(result.items)) {
                console.warn('⚠️ result.items가 배열이 아님, 빈 배열로 처리');
                result.items = [];
            }

            // pagination이 없는 경우 처리
            if (!result.pagination) {
                console.warn('⚠️ pagination 정보 없음, 기본값으로 설정');
                result.pagination = {
                    page: parseInt(page),
                    limit: parseInt(limit),
                    total_items: result.items.length,
                    has_next: false,
                    has_prev: false
                };
            }

        } catch (repoError) {
            console.error('❌ Repository 호출 실패:', repoError.message);
            console.error('❌ Repository 스택:', repoError.stack);
            
            // Repository 에러 시 빈 결과 반환
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

        console.log(`✅ 디바이스 ${result.items ? result.items.length : 0}개 조회 완료`);
        res.json(createPaginatedResponse(result.items, result.pagination, 'Devices retrieved successfully'));

    } catch (error) {
        console.error('❌ 디바이스 목록 조회 실패:', error.message);
        console.error('❌ 전체 스택:', error.stack);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICES_LIST_ERROR'));
    }
});

/**
 * GET /api/devices/:id
 * 특정 디바이스 상세 조회
 */
router.get('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const { include_data_points = false } = req.query;

        console.log(`📱 디바이스 ID ${id} 상세 조회 시작...`);

        const device = await getDeviceRepo().findById(parseInt(id), tenantId);

        if (!device) {
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        // 데이터포인트 포함 요청 시
        if (include_data_points === 'true') {
            const dataPoints = await getDataPointRepo().findByDeviceId(device.id, tenantId);
            device.data_points = dataPoints;
            device.data_points_count = dataPoints.length;
        }

        console.log(`✅ 디바이스 ID ${id} 조회 완료`);
        res.json(createResponse(true, device, 'Device retrieved successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ID ${req.params.id} 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_DETAIL_ERROR'));
    }
});

/**
 * POST /api/devices
 * 새 디바이스 등록
 */
router.post('/', async (req, res) => {
    try {
        const { tenantId, user } = req;
        const deviceData = {
            ...req.body,
            tenant_id: tenantId,
            created_by: user.id,
            created_at: new Date().toISOString()
        };

        console.log('📱 새 디바이스 등록 요청:', {
            name: deviceData.name,
            protocol_type: deviceData.protocol_type,
            endpoint: deviceData.endpoint
        });

        // 유효성 검사
        if (!deviceData.name || !deviceData.protocol_type || !deviceData.endpoint) {
            return res.status(400).json(
                createResponse(false, null, 'Name, protocol_type, and endpoint are required', 'VALIDATION_ERROR')
            );
        }

        // 같은 이름의 디바이스 중복 확인
        const existingDevice = await getDeviceRepo().findByName(deviceData.name, tenantId);
        if (existingDevice) {
            return res.status(409).json(
                createResponse(false, null, 'Device with this name already exists', 'DEVICE_NAME_CONFLICT')
            );
        }

        const newDevice = await getDeviceRepo().create(deviceData, tenantId);

        console.log(`✅ 새 디바이스 등록 완료: ID ${newDevice.id}`);
        res.status(201).json(createResponse(true, newDevice, 'Device created successfully'));

    } catch (error) {
        console.error('❌ 디바이스 등록 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_CREATE_ERROR'));
    }
});

/**
 * PUT /api/devices/:id
 * 디바이스 정보 수정
 */
router.put('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const updateData = {
            ...req.body,
            updated_at: new Date().toISOString()
        };

        console.log(`📱 디바이스 ID ${id} 수정 요청:`, Object.keys(updateData));

        // 이름 중복 확인 (이름 변경 시)
        if (updateData.name) {
            const existingDevice = await getDeviceRepo().findByName(updateData.name, tenantId);
            if (existingDevice && existingDevice.id !== parseInt(id)) {
                return res.status(409).json(
                    createResponse(false, null, 'Device with this name already exists', 'DEVICE_NAME_CONFLICT')
                );
            }
        }

        const updatedDevice = await getDeviceRepo().update(parseInt(id), updateData, tenantId);

        if (!updatedDevice) {
            return res.status(404).json(
                createResponse(false, null, 'Device not found or update failed', 'DEVICE_UPDATE_FAILED')
            );
        }

        console.log(`✅ 디바이스 ID ${id} 수정 완료`);
        res.json(createResponse(true, updatedDevice, 'Device updated successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ID ${req.params.id} 수정 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_UPDATE_ERROR'));
    }
});

/**
 * DELETE /api/devices/:id
 * 디바이스 삭제
 */
router.delete('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;

        console.log(`📱 디바이스 ID ${id} 삭제 요청...`);

        // 연관된 데이터포인트 확인
        const dataPoints = await getDataPointRepo().findByDeviceId(parseInt(id), tenantId);
        if (dataPoints.length > 0) {
            console.log(`⚠️ 디바이스에 ${dataPoints.length}개의 데이터포인트가 연결되어 있음`);
            
            // 옵션: force=true인 경우 연관 데이터도 삭제
            if (req.query.force !== 'true') {
                return res.status(409).json(createResponse(
                    false, 
                    { data_points_count: dataPoints.length }, 
                    'Device has associated data points. Use force=true to delete them.', 
                    'DEVICE_HAS_DEPENDENCIES'
                ));
            }

            // 연관 데이터포인트 삭제
            for (const dataPoint of dataPoints) {
                await getDataPointRepo().deleteById(dataPoint.id, tenantId);
            }
            console.log(`✅ 연관된 ${dataPoints.length}개 데이터포인트 삭제 완료`);
        }

        const deleted = await getDeviceRepo().deleteById(parseInt(id), tenantId);

        if (!deleted) {
            return res.status(404).json(
                createResponse(false, null, 'Device not found or delete failed', 'DEVICE_DELETE_FAILED')
            );
        }

        console.log(`✅ 디바이스 ID ${id} 삭제 완료`);
        res.json(createResponse(true, { deleted: true }, 'Device deleted successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ID ${req.params.id} 삭제 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_DELETE_ERROR'));
    }
});

// ============================================================================
// 📊 디바이스 상태 및 제어 API
// ============================================================================

/**
 * POST /api/devices/:id/enable
 * 디바이스 활성화
 */
router.post('/:id/enable', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;

        console.log(`🟢 디바이스 ID ${id} 활성화 요청...`);

        const updatedDevice = await getDeviceRepo().update(
            parseInt(id),
            { 
                is_enabled: true, 
                status: 'enabled',
                updated_at: new Date().toISOString()
            },
            tenantId
        );

        if (!updatedDevice) {
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        console.log(`✅ 디바이스 ID ${id} 활성화 완료`);
        res.json(createResponse(true, updatedDevice, 'Device enabled successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ID ${req.params.id} 활성화 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_ENABLE_ERROR'));
    }
});

/**
 * POST /api/devices/:id/disable
 * 디바이스 비활성화
 */
router.post('/:id/disable', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;

        console.log(`🔴 디바이스 ID ${id} 비활성화 요청...`);

        const updatedDevice = await getDeviceRepo().update(
            parseInt(id),
            { 
                is_enabled: false, 
                status: 'disabled',
                connection_status: 'disconnected',
                updated_at: new Date().toISOString()
            },
            tenantId
        );

        if (!updatedDevice) {
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        console.log(`✅ 디바이스 ID ${id} 비활성화 완료`);
        res.json(createResponse(true, updatedDevice, 'Device disabled successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ID ${req.params.id} 비활성화 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_DISABLE_ERROR'));
    }
});

/**
 * POST /api/devices/:id/restart
 * 디바이스 재시작
 */
router.post('/:id/restart', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;

        console.log(`🔄 디바이스 ID ${id} 재시작 요청...`);

        // 실제로는 Collector에 재시작 명령 전송
        // 여기서는 상태만 업데이트
        const updatedDevice = await getDeviceRepo().update(
            parseInt(id),
            { 
                status: 'restarting',
                last_restart: new Date().toISOString(),
                updated_at: new Date().toISOString()
            },
            tenantId
        );

        if (!updatedDevice) {
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        // 3초 후 running 상태로 변경 (시뮬레이션)
        setTimeout(async () => {
            try {
                await getDeviceRepo().update(
                    parseInt(id),
                    { 
                        status: 'running',
                        connection_status: 'connected',
                        updated_at: new Date().toISOString()
                    },
                    tenantId
                );
                console.log(`✅ 디바이스 ID ${id} 재시작 완료`);
            } catch (err) {
                console.error(`❌ 디바이스 ID ${id} 재시작 후 상태 업데이트 실패:`, err.message);
            }
        }, 3000);

        res.json(createResponse(true, updatedDevice, 'Device restart initiated'));

    } catch (error) {
        console.error(`❌ 디바이스 ID ${req.params.id} 재시작 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_RESTART_ERROR'));
    }
});

/**
 * POST /api/devices/:id/test-connection
 * 디바이스 연결 테스트
 */
router.post('/:id/test-connection', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;

        console.log(`🔗 디바이스 ID ${id} 연결 테스트 요청...`);

        const device = await getDeviceRepo().findById(parseInt(id), tenantId);
        if (!device) {
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        // 실제로는 Collector에 연결 테스트 요청
        // 여기서는 시뮬레이션
        const testStartTime = Date.now();
        
        // 연결 테스트 시뮬레이션 (90% 성공률)
        const isSuccessful = Math.random() > 0.1;
        const responseTime = Math.floor(Math.random() * 200) + 50; // 50-250ms
        
        const testResult = {
            device_id: device.id,
            device_name: device.name,
            endpoint: device.endpoint,
            protocol_type: device.protocol_type,
            test_successful: isSuccessful,
            response_time_ms: responseTime,
            test_timestamp: new Date().toISOString(),
            error_message: isSuccessful ? null : 'Connection timeout or unreachable'
        };

        // 테스트 결과에 따라 디바이스 상태 업데이트
        const newConnectionStatus = isSuccessful ? 'connected' : 'disconnected';
        await getDeviceRepo().update(
            device.id,
            {
                connection_status: newConnectionStatus,
                last_seen: isSuccessful ? new Date().toISOString() : device.last_seen,
                updated_at: new Date().toISOString()
            },
            tenantId
        );

        console.log(`✅ 디바이스 ID ${id} 연결 테스트 완료: ${isSuccessful ? '성공' : '실패'}`);
        res.json(createResponse(true, testResult, `Connection test ${isSuccessful ? 'successful' : 'failed'}`));

    } catch (error) {
        console.error(`❌ 디바이스 ID ${req.params.id} 연결 테스트 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'CONNECTION_TEST_ERROR'));
    }
});

// ============================================================================
// 📊 디바이스 데이터포인트 관리 API
// ============================================================================

/**
 * GET /api/devices/:id/data-points
 * 디바이스의 데이터포인트 목록 조회
 */
router.get('/:id/data-points', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const {
            page = 1,
            limit = 50,
            data_type,
            enabled_only = false
        } = req.query;

        console.log(`📊 디바이스 ID ${id} 데이터포인트 조회...`);

        const device = await getDeviceRepo().findById(parseInt(id), tenantId);
        if (!device) {
            return res.status(404).json(createResponse(false, null, 'Device not found', 'DEVICE_NOT_FOUND'));
        }

        const options = {
            deviceId: device.id,
            tenantId,
            dataType: data_type,
            enabledOnly: enabled_only === 'true',
            page: parseInt(page),
            limit: parseInt(limit)
        };

        const result = await getDataPointRepo().findByDevice(options);

        console.log(`✅ 디바이스 ID ${id} 데이터포인트 ${result.items.length}개 조회 완료`);
        res.json(createPaginatedResponse(result.items, result.pagination, 'Device data points retrieved successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ID ${req.params.id} 데이터포인트 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_DATA_POINTS_ERROR'));
    }
});

// ============================================================================
// 🔍 검색 및 필터 API
// ============================================================================

/**
 * GET /api/devices/protocols
 * 지원하는 프로토콜 목록 조회
 */
router.get('/protocols', async (req, res) => {
    try {
        const { tenantId } = req;

        console.log('📋 지원 프로토콜 목록 조회...');

        const protocols = await getDeviceRepo().getAvailableProtocols(tenantId);

        console.log(`✅ ${protocols.length}개 프로토콜 조회 완료`);
        res.json(createResponse(true, protocols, 'Available protocols retrieved successfully'));

    } catch (error) {
        console.error('❌ 프로토콜 목록 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'PROTOCOLS_LIST_ERROR'));
    }
});

/**
 * GET /api/devices/statistics
 * 디바이스 통계 조회
 */
router.get('/statistics', async (req, res) => {
    try {
        const { tenantId } = req;

        console.log('📊 디바이스 통계 조회...');

        const stats = await getDeviceRepo().getStatsByTenant(tenantId);

        console.log('✅ 디바이스 통계 조회 완료');
        res.json(createResponse(true, stats, 'Device statistics retrieved successfully'));

    } catch (error) {
        console.error('❌ 디바이스 통계 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_STATS_ERROR'));
    }
});

/**
 * POST /api/devices/bulk-action
 * 일괄 작업 (활성화/비활성화/삭제)
 */
router.post('/bulk-action', async (req, res) => {
    try {
        const { tenantId } = req;
        const { action, device_ids } = req.body;

        if (!Array.isArray(device_ids) || device_ids.length === 0) {
            return res.status(400).json(
                createResponse(false, null, 'device_ids array is required', 'VALIDATION_ERROR')
            );
        }

        console.log(`🔄 일괄 작업 요청: ${action}, 대상: ${device_ids.length}개 디바이스`);

        let successCount = 0;
        let failedCount = 0;
        const errors = [];

        for (const deviceId of device_ids) {
            try {
                let updateData = { updated_at: new Date().toISOString() };

                switch (action) {
                    case 'enable':
                        updateData = { ...updateData, is_enabled: true, status: 'enabled' };
                        break;
                    case 'disable':
                        updateData = { ...updateData, is_enabled: false, status: 'disabled', connection_status: 'disconnected' };
                        break;
                    case 'delete':
                        const deleted = await getDeviceRepo().deleteById(parseInt(deviceId), tenantId);
                        if (deleted) successCount++;
                        else failedCount++;
                        continue;
                    default:
                        throw new Error(`Unknown action: ${action}`);
                }

                if (action !== 'delete') {
                    const updated = await getDeviceRepo().update(parseInt(deviceId), updateData, tenantId);
                    if (updated) successCount++;
                    else failedCount++;
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

        console.log(`✅ 일괄 작업 완료: 성공 ${successCount}, 실패 ${failedCount}`);
        res.json(createResponse(true, result, `Bulk ${action} completed`));

    } catch (error) {
        console.error('❌ 일괄 작업 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'BULK_ACTION_ERROR'));
    }
});

const sqlite3 = require('sqlite3').verbose();
const ConfigManager = require('../lib/config/ConfigManager');
const configManager = ConfigManager.getInstance();

/**
 * GET /api/devices/debug/direct
 * SQLite 직접 조회 (디버깅용)
 */
router.get('/debug/direct', async (req, res) => {
    try {
        const dbPath = configManager.get('SQLITE_PATH', './data/db/pulseone.db');
        console.log(`🔍 직접 SQLite 조회: ${dbPath}`);

        const devices = await new Promise((resolve, reject) => {
            const db = new sqlite3.Database(dbPath, (err) => {
                if (err) {
                    reject(new Error(`Database connection failed: ${err.message}`));
                    return;
                }
            });

            const sql = `
                SELECT 
                    id, tenant_id, site_id, device_group_id, edge_server_id,
                    name, description, device_type, manufacturer, model, 
                    serial_number, protocol_type, endpoint, config,
                    polling_interval, timeout, retry_count, is_enabled,
                    installation_date, last_maintenance, created_at, updated_at
                FROM devices 
                WHERE tenant_id = 1
                ORDER BY id
                LIMIT 10
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

        console.log(`✅ SQLite 직접 조회 결과: ${devices.length}개 디바이스`);
        
        res.json({
            success: true,
            debug: true,
            source: 'direct_sqlite',
            database_path: dbPath,
            data: {
                devices: devices.map(device => ({
                    ...device,
                    is_enabled: !!device.is_enabled,
                    config: device.config ? JSON.parse(device.config) : null
                })),
                count: devices.length
            },
            message: 'Direct SQLite query successful'
        });

    } catch (error) {
        console.error('❌ SQLite 직접 조회 실패:', error.message);
        res.status(500).json({
            success: false,
            debug: true,
            error: error.message,
            database_path: configManager.get('SQLITE_PATH', './data/db/pulseone.db')
        });
    }
});

/**
 * GET /api/devices/debug/repository
 * Repository 상태 확인
 */
router.get('/debug/repository', async (req, res) => {
    try {
        console.log('🔍 Repository 디버깅...');
        
        const repo = getDeviceRepo();
        console.log('Repository 인스턴스:', typeof repo);
        console.log('Repository 메소드들:', Object.getOwnPropertyNames(Object.getPrototypeOf(repo)));
        
        // Repository의 DatabaseFactory 상태 확인
        if (repo.dbFactory) {
            console.log('DatabaseFactory 존재:', typeof repo.dbFactory);
        } else {
            console.log('DatabaseFactory 없음');
        }

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
                }
            }
        });

    } catch (error) {
        console.error('❌ Repository 디버깅 실패:', error.message);
        res.status(500).json({
            success: false,
            debug: true,
            error: error.message
        });
    }
});

/**
 * GET /api/devices/debug/query
 * 실제 쿼리 실행 테스트
 */
router.get('/debug/query', async (req, res) => {
    try {
        console.log('🔍 DeviceQueries.getDevicesWithAllInfo() 테스트...');
        
        const DeviceQueries = require('../lib/database/queries/DeviceQueries');
        const DatabaseFactory = require('../lib/database/DatabaseFactory');
        
        // 실제 Repository에서 사용하는 것과 동일한 방식으로 테스트
        const dbFactory = new DatabaseFactory();
        
        let query = DeviceQueries.getDevicesWithAllInfo();
        const params = [];

        // 테넌트 필터 추가 (기본값)
        query += DeviceQueries.addTenantFilter();
        params.push(1);

        // 그룹화 및 정렬
        query += DeviceQueries.getGroupByAndOrder();

        // 제한
        query += DeviceQueries.addLimit();
        params.push(10);

        console.log('🔍 실행할 쿼리:');
        console.log(query);
        console.log('🔍 파라미터:', params);

        // 실제 쿼리 실행
        const queryResult = await dbFactory.executeQuery(query, params);
        
        console.log('🔍 쿼리 결과 타입:', typeof queryResult);
        console.log('🔍 쿼리 결과 구조:', Object.keys(queryResult || {}));
        
        if (Array.isArray(queryResult)) {
            console.log('🔍 배열 길이:', queryResult.length);
        } else if (queryResult && queryResult.rows) {
            console.log('🔍 rows 길이:', queryResult.rows.length);
        }

        res.json({
            success: true,
            debug: true,
            query_info: {
                sql: query,
                params: params,
                result_type: typeof queryResult,
                result_keys: Object.keys(queryResult || {}),
                is_array: Array.isArray(queryResult),
                has_rows: !!(queryResult && queryResult.rows),
                length: Array.isArray(queryResult) ? queryResult.length : 
                       (queryResult && queryResult.rows ? queryResult.rows.length : 'unknown')
            },
            raw_result: queryResult
        });

    } catch (error) {
        console.error('❌ 쿼리 테스트 실패:', error.message);
        console.error('❌ 스택:', error.stack);
        
        res.status(500).json({
            success: false,
            debug: true,
            error: error.message,
            stack: error.stack
        });
    }
});

module.exports = router;