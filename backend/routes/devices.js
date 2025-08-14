// ============================================================================
// backend/routes/devices.js - 깔끔한 새 버전
// ============================================================================

const express = require('express');
const router = express.Router();

// Repository 사용
const DeviceRepository = require('../lib/database/repositories/DeviceRepository');

// Repository 인스턴스 (lazy initialization)
let deviceRepo = null;

function getDeviceRepo() {
    if (!deviceRepo) {
        deviceRepo = new DeviceRepository();
        console.log("✅ DeviceRepository 초기화 완료");
    }
    return deviceRepo;
}

// 응답 헬퍼 함수
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
// API 엔드포인트들
// ============================================================================

/**
 * GET /api/devices
 * 디바이스 목록 조회
 */
router.get('/', async (req, res) => {
    try {
        console.log('🔍 디바이스 목록 조회 시작...');
        
        const { 
            page = 1, 
            limit = 20, 
            search,
            protocol,
            status
        } = req.query;

        const tenantId = req.tenantId || req.user?.tenant_id || null;

        const options = {
            tenantId,
            search,
            protocol,
            status,
            page: parseInt(page),
            limit: parseInt(limit)
        };

        // DeviceRepository에서 데이터 조회
        const result = await getDeviceRepo().findAll(options);
        
        console.log('🔍 findAll 결과 타입:', typeof result);
        console.log('🔍 result 구조:', Object.keys(result || {}));
        
        // 결과 안전하게 처리
        let devices = [];
        let pagination = { page: 1, limit: 20, total: 0 };
        
        if (result) {
            if (Array.isArray(result)) {
                // result가 직접 배열인 경우
                devices = result;
                pagination.total = result.length;
            } else if (result.items && Array.isArray(result.items)) {
                // result.items가 배열인 경우
                devices = result.items;
                pagination = result.pagination || pagination;
            } else if (result.data && Array.isArray(result.data)) {
                // result.data가 배열인 경우
                devices = result.data;
                pagination = result.pagination || pagination;
            }
        }

        console.log(`✅ 디바이스 ${devices.length}개 조회 완료`);
        
        const responseData = {
            items: devices,
            pagination
        };

        res.json(createResponse(true, responseData, 'Devices retrieved successfully'));

    } catch (error) {
        console.error('❌ 디바이스 목록 조회 실패:', error.message);
        res.status(500).json(createResponse(false, error.message, null, 'DEVICES_FETCH_ERROR'));
    }
});

/**
 * GET /api/devices/:id
 * 특정 디바이스 조회
 */
router.get('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.tenantId || req.user?.tenant_id || null;

        console.log(`🔍 디바이스 ID ${id} 조회...`);

        const device = await getDeviceRepo().findById(parseInt(id), tenantId);

        if (!device) {
            return res.status(404).json(
                createResponse(false, 'Device not found', null, 'DEVICE_NOT_FOUND')
            );
        }

        console.log(`✅ 디바이스 ID ${id} 조회 완료`);
        res.json(createResponse(true, device, 'Device retrieved successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ${req.params.id} 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, error.message, null, 'DEVICE_DETAIL_ERROR'));
    }
});

/**
 * POST /api/devices
 * 새 디바이스 생성
 */
router.post('/', async (req, res) => {
    try {
        const tenantId = req.tenantId || req.user?.tenant_id || 1;
        const deviceData = req.body;

        console.log('🔧 새 디바이스 생성...');

        // 필수 필드 검증
        const requiredFields = ['name', 'protocol'];
        const missingFields = requiredFields.filter(field => !deviceData[field]);
        
        if (missingFields.length > 0) {
            return res.status(400).json(
                createResponse(false, `Missing required fields: ${missingFields.join(', ')}`, null, 'MISSING_REQUIRED_FIELDS')
            );
        }

        const newDevice = await getDeviceRepo().create(deviceData, tenantId);

        console.log(`✅ 새 디바이스 생성 완료: ID ${newDevice.id}`);
        res.status(201).json(createResponse(true, newDevice, 'Device created successfully'));

    } catch (error) {
        console.error('❌ 디바이스 생성 실패:', error.message);
        res.status(500).json(createResponse(false, error.message, null, 'DEVICE_CREATE_ERROR'));
    }
});

/**
 * PUT /api/devices/:id
 * 디바이스 수정
 */
router.put('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.tenantId || req.user?.tenant_id || null;
        const updateData = req.body;

        console.log(`🔧 디바이스 ${id} 수정...`);

        const updatedDevice = await getDeviceRepo().update(parseInt(id), updateData, tenantId);

        if (!updatedDevice) {
            return res.status(404).json(
                createResponse(false, 'Device not found or update failed', null, 'DEVICE_UPDATE_FAILED')
            );
        }

        console.log(`✅ 디바이스 ID ${id} 수정 완료`);
        res.json(createResponse(true, updatedDevice, 'Device updated successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ${req.params.id} 수정 실패:`, error.message);
        res.status(500).json(createResponse(false, error.message, null, 'DEVICE_UPDATE_ERROR'));
    }
});

/**
 * DELETE /api/devices/:id
 * 디바이스 삭제
 */
router.delete('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.tenantId || req.user?.tenant_id || null;

        console.log(`🗑️ 디바이스 ${id} 삭제...`);

        const deleted = await getDeviceRepo().deleteById(parseInt(id), tenantId);

        if (!deleted) {
            return res.status(404).json(
                createResponse(false, 'Device not found or delete failed', null, 'DEVICE_DELETE_FAILED')
            );
        }

        console.log(`✅ 디바이스 ID ${id} 삭제 완료`);
        res.json(createResponse(true, { deleted: true }, 'Device deleted successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ${req.params.id} 삭제 실패:`, error.message);
        res.status(500).json(createResponse(false, error.message, null, 'DEVICE_DELETE_ERROR'));
    }
});

/**
 * GET /api/devices/test
 * 간단한 테스트 엔드포인트
 */
router.get('/test', (req, res) => {
    res.json(createResponse(true, { message: 'Device API is working!' }, 'Test successful'));
});

module.exports = router;