// =============================================================================
// backend/routes/modbus-slave.js
// Modbus Slave 디바이스 관리 API
// export-gateway 라우트와 동일한 패턴
// =============================================================================
const express = require('express');
const router = express.Router();
const ModbusSlaveService = require('../lib/services/ModbusSlaveService');
const LogManager = require('../lib/utils/LogManager');

// ─────────────────────────────────────────────────────────────────────────────
// 헬퍼: System Admin이면 null(전체), 일반 사용자면 자신의 tenant_id 반환
// ─────────────────────────────────────────────────────────────────────────────
function getEffectiveTenantId(req) {
    const role = req.user?.role;
    if (role === 'system_admin' || role === 'admin') return null; // 전체 테넌트 조회
    return req.user?.tenant_id || 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// 디바이스 CRUD
// ─────────────────────────────────────────────────────────────────────────────

/**
 * GET /api/modbus-slave
 * 디바이스 목록 (tenant 격리, 선택적 site 필터)
 */
router.get('/', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const siteId = req.query.site_id ? parseInt(req.query.site_id) : null;
        const result = await ModbusSlaveService.getAllDevices(tenantId, siteId);
        // handleRequest 이중 중첩 해소: result가 {success, data:[]} 형태면 data만 꺼냄
        const devices = Array.isArray(result) ? result : (result?.data ?? result);
        res.json({ success: true, data: devices });
    } catch (error) {
        LogManager.api('ERROR', 'Modbus Slave 목록 조회 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

// ─────────────────────────────────────────────────────────────────────────────
// 시스템 예약 포트 조회 (환경변수 기반, Docker/Native 공통)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * GET /api/modbus-slave/reserved-ports
 * 시스템에서 이미 사용 중인 포트 목록 반환 (환경변수 기반)
 * Docker/Native 둘 다 정확한 포트를 반환
 */
router.get('/reserved-ports', async (req, res) => {
    try {
        const ports = new Set([
            // 시스템 기본 포트
            22, 80, 443,
            // 각 서비스 — 환경변수 우선, fallback은 기본값
            parseInt(process.env.PORT || process.env.BACKEND_PORT || '3000'),
            parseInt(process.env.COLLECTOR_API_PORT || '8501'),
            parseInt(process.env.REDIS_PRIMARY_PORT || process.env.REDIS_PORT || '6379'),
            parseInt(process.env.RABBITMQ_PORT || '5672'),
            parseInt(process.env.RABBITMQ_MANAGEMENT_PORT || '15672'),
            parseInt(process.env.INFLUXDB_PORT || '8086'),
            1883,   // MQTT (고정)
            5173,   // Frontend Vite dev server
            9229,   // Node.js debugger
            50502,  // Modbus TCP Simulator
        ].filter(p => !isNaN(p) && p > 0));

        res.json({
            success: true,
            data: {
                ports: Array.from(ports),
                labels: {
                    22: 'SSH',
                    80: 'HTTP',
                    443: 'HTTPS',
                    [parseInt(process.env.PORT || '3000')]: 'Backend API',
                    [parseInt(process.env.COLLECTOR_API_PORT || '8501')]: 'Collector API',
                    [parseInt(process.env.REDIS_PRIMARY_PORT || '6379')]: 'Redis',
                    [parseInt(process.env.RABBITMQ_PORT || '5672')]: 'RabbitMQ',
                    [parseInt(process.env.RABBITMQ_MANAGEMENT_PORT || '15672')]: 'RabbitMQ Management',
                    [parseInt(process.env.INFLUXDB_PORT || '8086')]: 'InfluxDB',
                    1883: 'MQTT',
                    5173: 'Frontend Dev',
                    9229: 'Node Debugger',
                    50502: 'Modbus Simulator',
                }
            }
        });
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// ─────────────────────────────────────────────────────────────────────────────
// 통신 이력 (Access Logs) — /:id 보다 앞에 위치해야 함 (라우터 순서 중요)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * GET /api/modbus-slave/access-logs/stats
 */
router.get('/access-logs/stats', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const { device_id, date_from, date_to } = req.query;
        const result = await ModbusSlaveService.getAccessLogStats(tenantId, {
            deviceId: device_id, dateFrom: date_from, dateTo: date_to,
        });
        res.json({ success: true, data: result?.data ?? result });
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * GET /api/modbus-slave/access-logs
 */
router.get('/access-logs', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const { device_id, client_ip, date_from, date_to, page = 1, limit = 50 } = req.query;
        const result = await ModbusSlaveService.getAccessLogs(tenantId, {
            deviceId: device_id, clientIp: client_ip, dateFrom: date_from, dateTo: date_to,
            page: parseInt(page), limit: parseInt(limit),
        });
        res.json({ success: true, data: result?.data ?? result });
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * GET /api/modbus-slave/packet-logs
 * 패킷 로그 파일 목록
 */
router.get('/packet-logs', async (req, res) => {
    try {
        const { device_id } = req.query;
        const result = await ModbusSlaveService.getPacketLogFiles(
            device_id ? parseInt(device_id) : null
        );
        res.json({ success: true, data: result?.data ?? result });
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * GET /api/modbus-slave/packet-logs/read
 * 패킷 로그 파일 내용 (tail)
 */
router.get('/packet-logs/read', async (req, res) => {
    try {
        const { path: filePath, tail = 500 } = req.query;
        if (!filePath) return res.status(400).json({ success: false, message: 'path 필수' });
        const result = await ModbusSlaveService.readPacketLogFile(filePath, parseInt(tail));
        res.json({ success: true, data: result?.data ?? result });
    } catch (error) {
        const status = error.statusCode || 500;
        res.status(status).json({ success: false, message: error.message });
    }
});

/**
 * GET /api/modbus-slave/deleted
 * 소프트 삭제된 디바이스 목록 조회 (/:id 보다 앞에 등록해야 함)
 */
router.get('/deleted', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const siteId = req.query.site_id || null;
        const result = await ModbusSlaveService.getDeletedDevices(tenantId, siteId);
        res.json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * GET /api/modbus-slave/:id
 * 단일 디바이스 조회
 */
router.get('/:id', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const result = await ModbusSlaveService.getDeviceById(req.params.id, tenantId);
        const device = result?.id ? result : (result?.data ?? result);
        res.json({ success: true, data: device });
    } catch (error) {
        const status = error.statusCode || 500;
        res.status(status).json({ success: false, message: error.message });
    }
});

/**
 * POST /api/modbus-slave
 * 디바이스 등록
 * body: { site_id, name, tcp_port, unit_id, max_clients, description }
 */
router.post('/', async (req, res) => {
    try {
        const tenantId = req.body.tenant_id || getEffectiveTenantId(req) || 1;
        const result = await ModbusSlaveService.createDevice(req.body, tenantId);
        const device = result?.id ? result : (result?.data ?? result);
        LogManager.api('INFO', `Modbus Slave 디바이스 등록: ${req.body.name}`);
        res.status(201).json({ success: true, data: device });
    } catch (error) {
        const status = error.statusCode || 500;
        LogManager.api('ERROR', 'Modbus Slave 디바이스 등록 실패', { error: error.message });
        res.status(status).json({ success: false, message: error.message });
    }
});

/**
 * PUT /api/modbus-slave/:id
 * 디바이스 수정
 */
router.put('/:id', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const result = await ModbusSlaveService.updateDevice(req.params.id, req.body, tenantId);
        const device = result?.id ? result : (result?.data ?? result);
        res.json({ success: true, data: device });
    } catch (error) {
        const status = error.statusCode || 500;
        res.status(status).json({ success: false, message: error.message });
    }
});

/**
 * DELETE /api/modbus-slave/:id
 * 디바이스 삭제
 */
router.delete('/:id', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const result = await ModbusSlaveService.deleteDevice(req.params.id, tenantId);
        res.json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * POST /api/modbus-slave/:id/restore
 * 소프트 삭제된 디바이스 복구
 */
router.post('/:id/restore', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const result = await ModbusSlaveService.restoreDevice(req.params.id, tenantId);
        res.json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// ─────────────────────────────────────────────────────────────────────────────
// 레지스터 매핑
// ─────────────────────────────────────────────────────────────────────────────

/**
 * GET /api/modbus-slave/:id/mappings
 * 디바이스 레지스터 매핑 목록
 */
router.get('/:id/mappings', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const result = await ModbusSlaveService.getMappings(req.params.id, tenantId);
        const mappings = Array.isArray(result) ? result : (result?.data ?? result);
        res.json({ success: true, data: mappings });
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * PUT /api/modbus-slave/:id/mappings
 * 레지스터 매핑 전체 교체
 * body: [ { point_id, register_type, register_address, data_type, ... }, ... ]
 */
router.put('/:id/mappings', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const mappings = req.body.mappings || req.body;
        const result = await ModbusSlaveService.saveMappings(req.params.id, mappings, tenantId);
        const saved = Array.isArray(result) ? result : (result?.data ?? result);
        res.json({ success: true, data: saved });
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * GET /api/modbus-slave/:id/register-values
 * 디바이스의 매핑별 현재 실시간 값 조회 (current_values JOIN)
 */
router.get('/:id/register-values', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const result = await ModbusSlaveService.getRegisterValues(req.params.id, tenantId);
        const values = Array.isArray(result) ? result : (result?.data ?? result);
        res.json({ success: true, data: values });
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// ─────────────────────────────────────────────────────────────────────────────
// 프로세스 제어 (start / stop / restart)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * POST /api/modbus-slave/:id/start
 * 디바이스 Worker 프로세스 시작
 */
router.post('/:id/start', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const result = await ModbusSlaveService.startDevice(req.params.id, tenantId);
        LogManager.api('INFO', `Modbus Slave 시작: ID ${req.params.id}`);
        res.json(result);
    } catch (error) {
        LogManager.api('ERROR', `Modbus Slave 시작 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * POST /api/modbus-slave/:id/stop
 * 디바이스 Worker 프로세스 중지
 */
router.post('/:id/stop', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const result = await ModbusSlaveService.stopDevice(req.params.id, tenantId);
        LogManager.api('INFO', `Modbus Slave 중지: ID ${req.params.id}`);
        res.json(result);
    } catch (error) {
        LogManager.api('ERROR', `Modbus Slave 중지 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * POST /api/modbus-slave/:id/restart
 * 디바이스 Worker 프로세스 재시작
 */
router.post('/:id/restart', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const result = await ModbusSlaveService.restartDevice(req.params.id, tenantId);
        LogManager.api('INFO', `Modbus Slave 재시작: ID ${req.params.id}`);
        res.json(result);
    } catch (error) {
        LogManager.api('ERROR', `Modbus Slave 재시작 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

// ─────────────────────────────────────────────────────────────────────────────
// 통계 / 상태
// ─────────────────────────────────────────────────────────────────────────────

/**
 * GET /api/modbus-slave/:id/stats
 * 디바이스 실시간 통계. 동시에 Redis→DB 스냅샷 기록(비동기, non-blocking)
 */
router.get('/:id/stats', async (req, res) => {
    try {
        const tenantId = getEffectiveTenantId(req);
        const stats = await ModbusSlaveService.getDeviceStats(req.params.id, tenantId);

        // 스냅샷을 비동기로 저장 (응답 지연 없음)
        ModbusSlaveService.snapshotAccessLogs(parseInt(req.params.id), tenantId)
            .catch(e => LogManager.api('WARN', 'Modbus Slave 스냅샷 오류', { error: e.message }));

        // handleRequest가 이미 { success, data: {...} } 형태로 반환하므로 그대로 전달
        res.json(stats);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

module.exports = router;

