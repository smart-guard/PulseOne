// ============================================================================
// backend/routes/protocols.js - 페이징 정보 포함한 개선된 라우트
// 프로토콜 전용 CRUD API 라우트 - Repository 패턴 완벽 적용
// ============================================================================

const express = require('express');
const router = express.Router();

// Repository 및 미들웨어 설정
const ProtocolRepository = require('../lib/database/repositories/ProtocolRepository');
const { 
    authenticateToken, 
    tenantIsolation, 
    requirePermission 
} = require('../middleware/tenantIsolation');

// Repository 인스턴스 생성
let protocolRepo = null;

function getProtocolRepo() {
    if (!protocolRepo) {
        protocolRepo = new ProtocolRepository();
        console.log("✅ ProtocolRepository 인스턴스 생성 완료");
    }
    return protocolRepo;
}

// 응답 생성 함수 - 페이징 정보 지원
function createResponse(success, data, message, error_code, meta = null) {
    const response = {
        success,
        data,
        message: message || (success ? 'Operation successful' : 'Operation failed'),
        error_code: error_code || null,
        timestamp: new Date().toISOString()
    };
    
    if (meta) {
        response.meta = meta;
        if (meta.pagination) {
            response.pagination = meta.pagination;
        }
    }
    
    return response;
}

// 개발용 미들웨어
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
// 정적 라우트를 동적 라우트보다 먼저 배치
// ============================================================================

/**
 * GET /api/protocols/statistics
 * 프로토콜 사용 통계 조회
 */
router.get('/statistics', async (req, res) => {
    try {
        const { tenantId } = req;

        console.log('📊 프로토콜 통계 조회...');

        // Repository에서 통계 데이터 조회
        const [counts, categoryStats, usageStats] = await Promise.all([
            getProtocolRepo().getCounts(),
            getProtocolRepo().getStatsByCategory(),
            getProtocolRepo().getUsageStats(tenantId)
        ]);

        const stats = {
            ...counts,
            categories: categoryStats,
            usage_stats: usageStats
        };

        console.log('✅ 프로토콜 통계 조회 완료:', stats);

        res.json(createResponse(true, stats, 'Protocol statistics retrieved successfully'));

    } catch (error) {
        console.error('❌ 프로토콜 통계 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'PROTOCOL_STATS_ERROR'));
    }
});

/**
 * GET /api/protocols/category/:category
 * 카테고리별 프로토콜 목록 조회
 */
router.get('/category/:category', async (req, res) => {
    try {
        const { category } = req.params;

        console.log(`🏷️ 카테고리 ${category} 프로토콜 조회...`);

        const protocols = await getProtocolRepo().findByCategory(category);

        console.log(`✅ 카테고리 ${category} 프로토콜 ${protocols.length}개 조회 완료`);

        res.json(createResponse(true, protocols, 'Category protocols retrieved successfully'));

    } catch (error) {
        console.error(`❌ 카테고리 ${req.params.category} 프로토콜 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'CATEGORY_PROTOCOLS_ERROR'));
    }
});

// ============================================================================
// 📋 프로토콜 목록 조회 API - 페이징 정보 포함
// ============================================================================

/**
 * GET /api/protocols
 * 프로토콜 목록 조회 (필터링 및 페이징 지원)
 */
router.get('/', async (req, res) => {
    try {
        const { tenantId } = req;
        const filters = {
            tenantId,
            category: req.query.category,
            enabled: req.query.enabled,
            deprecated: req.query.deprecated,
            uses_serial: req.query.uses_serial,
            requires_broker: req.query.requires_broker,
            search: req.query.search,
            sortBy: req.query.sortBy || 'display_name',
            sortOrder: req.query.sortOrder || 'ASC',
            limit: req.query.limit ? parseInt(req.query.limit) : null,
            offset: req.query.offset ? parseInt(req.query.offset) : null
        };

        console.log('📋 프로토콜 목록 조회 요청:', filters);

        // 프로토콜 데이터와 총 개수를 병렬로 조회
        const [protocols, totalCount] = await Promise.all([
            getProtocolRepo().findAll(filters),
            getProtocolRepo().getTotalCount(filters)
        ]);

        console.log(`✅ 프로토콜 ${protocols.length}개 조회 완료 (전체: ${totalCount}개)`);

        // 페이징 정보 계산
        const pagination = {
            total_count: totalCount,
            current_page: filters.offset && filters.limit ? Math.floor(filters.offset / filters.limit) + 1 : 1,
            page_size: filters.limit || totalCount,
            total_pages: filters.limit ? Math.ceil(totalCount / filters.limit) : 1,
            has_next: filters.offset && filters.limit ? (filters.offset + filters.limit) < totalCount : false,
            has_prev: filters.offset && filters.limit ? filters.offset > 0 : false,
            offset: filters.offset || 0,
            limit: filters.limit || totalCount
        };

        res.json(createResponse(
            true, 
            protocols, 
            'Protocols retrieved successfully', 
            null, 
            { pagination }
        ));

    } catch (error) {
        console.error('❌ 프로토콜 목록 조회 실패:', error.message);
        console.error('❌ 에러 스택:', error.stack);
        res.status(500).json(createResponse(false, null, error.message, 'PROTOCOL_LIST_ERROR'));
    }
});

    } catch (error) {
        console.error('❌ 프로토콜 목록 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'PROTOCOL_LIST_ERROR'));
    }
});

// ============================================================================
// ➕ 프로토콜 생성 API - Repository 사용
// ============================================================================

/**
 * POST /api/protocols
 * 새 프로토콜 등록
 */
router.post('/', async (req, res) => {
    try {
        const { user } = req;
        const protocolData = req.body;

        console.log('➕ 새 프로토콜 등록 요청:', protocolData.protocol_type);

        // 필수 필드 검증
        if (!protocolData.protocol_type || !protocolData.display_name) {
            return res.status(400).json(
                createResponse(false, null, 'Protocol type and display name are required', 'VALIDATION_ERROR')
            );
        }

        // 중복 프로토콜 타입 검사
        const exists = await getProtocolRepo().checkProtocolTypeExists(protocolData.protocol_type);
        if (exists) {
            return res.status(409).json(
                createResponse(false, null, 'Protocol type already exists', 'PROTOCOL_EXISTS')
            );
        }

        // 프로토콜 생성
        const newProtocol = await getProtocolRepo().create(protocolData, user.id);

        console.log(`✅ 프로토콜 ${protocolData.protocol_type} 등록 완료 (ID: ${newProtocol.id})`);

        res.status(201).json(createResponse(true, newProtocol, 'Protocol created successfully'));

    } catch (error) {
        console.error('❌ 프로토콜 생성 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'PROTOCOL_CREATE_ERROR'));
    }
});

// ============================================================================
// 동적 라우트들 (/:id 포함)을 정적 라우트 뒤에 배치
// ============================================================================

/**
 * GET /api/protocols/:id
 * 프로토콜 상세 조회
 */
router.get('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;

        console.log(`🔍 프로토콜 ID ${id} 상세 조회...`);

        const protocol = await getProtocolRepo().findById(id, tenantId);

        if (!protocol) {
            return res.status(404).json(
                createResponse(false, null, 'Protocol not found', 'PROTOCOL_NOT_FOUND')
            );
        }

        console.log(`✅ 프로토콜 ID ${id} 조회 완료: ${protocol.display_name}`);

        res.json(createResponse(true, protocol, 'Protocol retrieved successfully'));

    } catch (error) {
        console.error(`❌ 프로토콜 ID ${req.params.id} 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'PROTOCOL_DETAIL_ERROR'));
    }
});

/**
 * PUT /api/protocols/:id
 * 프로토콜 정보 수정
 */
router.put('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const updateData = req.body;

        console.log(`📝 프로토콜 ID ${id} 수정 요청:`, updateData);

        // 프로토콜 수정
        const updatedProtocol = await getProtocolRepo().update(id, updateData);

        console.log(`✅ 프로토콜 ID ${id} 수정 완료`);

        res.json(createResponse(true, updatedProtocol, 'Protocol updated successfully'));

    } catch (error) {
        console.error(`❌ 프로토콜 ID ${req.params.id} 수정 실패:`, error.message);
        
        if (error.message.includes('not found')) {
            res.status(404).json(createResponse(false, null, error.message, 'PROTOCOL_NOT_FOUND'));
        } else {
            res.status(500).json(createResponse(false, null, error.message, 'PROTOCOL_UPDATE_ERROR'));
        }
    }
});

/**
 * DELETE /api/protocols/:id
 * 프로토콜 삭제 (연결된 디바이스가 없는 경우만)
 */
router.delete('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { force } = req.query;

        console.log(`🗑️ 프로토콜 ID ${id} 삭제 요청 (강제: ${!!force})`);

        await getProtocolRepo().delete(id, force === 'true');

        console.log(`✅ 프로토콜 ID ${id} 삭제 완료`);

        res.json(createResponse(true, { deleted: true }, 'Protocol deleted successfully'));

    } catch (error) {
        console.error(`❌ 프로토콜 ID ${req.params.id} 삭제 실패:`, error.message);

        if (error.message.includes('not found')) {
            res.status(404).json(createResponse(false, null, error.message, 'PROTOCOL_NOT_FOUND'));
        } else if (error.message.includes('devices are using this protocol')) {
            res.status(409).json(createResponse(false, null, error.message, 'PROTOCOL_IN_USE'));
        } else {
            res.status(500).json(createResponse(false, null, error.message, 'PROTOCOL_DELETE_ERROR'));
        }
    }
});

// ============================================================================
// 🔄 프로토콜 제어 API들
// ============================================================================

/**
 * POST /api/protocols/:id/enable
 * 프로토콜 활성화
 */
router.post('/:id/enable', async (req, res) => {
    try {
        const { id } = req.params;

        console.log(`🟢 프로토콜 ID ${id} 활성화...`);

        const updatedProtocol = await getProtocolRepo().enable(id);

        console.log(`✅ 프로토콜 ID ${id} 활성화 완료`);

        res.json(createResponse(true, updatedProtocol, 'Protocol enabled successfully'));

    } catch (error) {
        console.error(`❌ 프로토콜 ID ${req.params.id} 활성화 실패:`, error.message);

        if (error.message.includes('not found')) {
            res.status(404).json(createResponse(false, null, 'Protocol not found', 'PROTOCOL_NOT_FOUND'));
        } else {
            res.status(500).json(createResponse(false, null, error.message, 'PROTOCOL_ENABLE_ERROR'));
        }
    }
});

/**
 * POST /api/protocols/:id/disable
 * 프로토콜 비활성화
 */
router.post('/:id/disable', async (req, res) => {
    try {
        const { id } = req.params;

        console.log(`🔴 프로토콜 ID ${id} 비활성화...`);

        const updatedProtocol = await getProtocolRepo().disable(id);

        console.log(`✅ 프로토콜 ID ${id} 비활성화 완료`);

        res.json(createResponse(true, updatedProtocol, 'Protocol disabled successfully'));

    } catch (error) {
        console.error(`❌ 프로토콜 ID ${req.params.id} 비활성화 실패:`, error.message);

        if (error.message.includes('not found')) {
            res.status(404).json(createResponse(false, null, 'Protocol not found', 'PROTOCOL_NOT_FOUND'));
        } else {
            res.status(500).json(createResponse(false, null, error.message, 'PROTOCOL_DISABLE_ERROR'));
        }
    }
});

/**
 * POST /api/protocols/:id/test
 * 프로토콜 연결 테스트 (시뮬레이션)
 */
router.post('/:id/test', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const testParams = req.body;

        console.log(`🔗 프로토콜 ID ${id} 연결 테스트...`);

        // 프로토콜 정보 조회
        const protocol = await getProtocolRepo().findById(id, tenantId);
        
        if (!protocol) {
            return res.status(404).json(
                createResponse(false, null, 'Protocol not found', 'PROTOCOL_NOT_FOUND')
            );
        }

        // 실제 연결 테스트 시뮬레이션
        const startTime = Date.now();
        
        let testResult = {
            protocol_id: parseInt(id),
            protocol_type: protocol.protocol_type,
            test_successful: false,
            response_time_ms: 0,
            test_timestamp: new Date().toISOString(),
            error_message: null
        };

        try {
            // 프로토콜별 기본 검증 로직
            if (protocol.protocol_type === 'MODBUS_TCP') {
                if (!testParams.host || !testParams.port) {
                    throw new Error('Host and port are required for Modbus TCP');
                }
                await new Promise(resolve => setTimeout(resolve, Math.random() * 150 + 50));
                testResult.test_successful = Math.random() > 0.1; // 90% 성공률
            } else if (protocol.protocol_type === 'MQTT') {
                if (!testParams.broker_url) {
                    throw new Error('Broker URL is required for MQTT');
                }
                await new Promise(resolve => setTimeout(resolve, Math.random() * 100 + 30));
                testResult.test_successful = Math.random() > 0.05; // 95% 성공률
            } else if (protocol.protocol_type === 'BACNET') {
                if (!testParams.device_instance) {
                    throw new Error('Device instance is required for BACnet');
                }
                await new Promise(resolve => setTimeout(resolve, Math.random() * 300 + 100));
                testResult.test_successful = Math.random() > 0.15; // 85% 성공률
            } else {
                await new Promise(resolve => setTimeout(resolve, Math.random() * 200 + 50));
                testResult.test_successful = Math.random() > 0.2; // 80% 성공률
            }

            if (!testResult.test_successful) {
                testResult.error_message = 'Connection timeout or device not responding';
            }

        } catch (error) {
            testResult.test_successful = false;
            testResult.error_message = error.message;
        }

        testResult.response_time_ms = Date.now() - startTime;

        console.log(`${testResult.test_successful ? '✅' : '❌'} 프로토콜 ID ${id} 테스트 완료 (${testResult.response_time_ms}ms)`);

        res.json(createResponse(true, testResult, 'Protocol test completed'));

    } catch (error) {
        console.error(`❌ 프로토콜 ID ${req.params.id} 테스트 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'PROTOCOL_TEST_ERROR'));
    }
});

/**
 * GET /api/protocols/:id/devices
 * 특정 프로토콜을 사용하는 디바이스 목록 조회
 */
router.get('/:id/devices', async (req, res) => {
    try {
        const { id } = req.params;
        const limit = req.query.limit ? parseInt(req.query.limit) : 50;
        const offset = req.query.offset ? parseInt(req.query.offset) : 0;

        console.log(`📱 프로토콜 ID ${id}의 디바이스 목록 조회 (limit: ${limit}, offset: ${offset})`);

        const devices = await getProtocolRepo().getDevicesByProtocol(id, limit, offset);

        console.log(`✅ 프로토콜 ID ${id}의 디바이스 ${devices.length}개 조회 완료`);

        res.json(createResponse(true, devices, 'Protocol devices retrieved successfully'));

    } catch (error) {
        console.error(`❌ 프로토콜 ID ${req.params.id} 디바이스 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'PROTOCOL_DEVICES_ERROR'));
    }
});

module.exports = router;