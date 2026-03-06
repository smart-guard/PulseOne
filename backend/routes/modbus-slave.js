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
// 디바이스 CRUD
// ─────────────────────────────────────────────────────────────────────────────

/**
 * GET /api/modbus-slave
 * 디바이스 목록 (tenant 격리, 선택적 site 필터)
 */
router.get('/', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
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

/**
 * GET /api/modbus-slave/:id
 * 단일 디바이스 조회
 */
router.get('/:id', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
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
        const tenantId = req.body.tenant_id || req.user?.tenant_id || 1; // 어드민이 폼에서 tenant_id 선택 가능
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
        const tenantId = req.user?.tenant_id || 1;
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
        const tenantId = req.user?.tenant_id || 1;
        const result = await ModbusSlaveService.deleteDevice(req.params.id, tenantId);
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
        const tenantId = req.user?.tenant_id || 1;
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
        const tenantId = req.user?.tenant_id || 1;
        const mappings = req.body.mappings || req.body;
        const result = await ModbusSlaveService.saveMappings(req.params.id, mappings, tenantId);
        const saved = Array.isArray(result) ? result : (result?.data ?? result);
        res.json({ success: true, data: saved });
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
        const tenantId = req.user?.tenant_id || 1;
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
        const tenantId = req.user?.tenant_id || 1;
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
        const tenantId = req.user?.tenant_id || 1;
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
 * 디바이스 실시간 통계 (프로세스 상태, 향후 Redis 통계)
 */
router.get('/:id/stats', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const stats = await ModbusSlaveService.getDeviceStats(req.params.id, tenantId);
        res.json({ success: true, data: stats });
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

module.exports = router;
