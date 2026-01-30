const express = require('express');
const router = express.Router();
const ExportGatewayService = require('../lib/services/ExportGatewayService');
const LogManager = require('../lib/utils/LogManager');

// =============================================================================
// Export Configuration API (Profiles & Targets)
// =============================================================================

// =============================================================================
// Export Profiles
// =============================================================================

/**
 * @route GET /api/export/profiles
 * @desc Export Profile 목록 조회
 */
router.get('/profiles', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.getAllProfiles(tenantId);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '프로파일 목록 조회 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route GET /api/export/profiles/:id
 * @desc Export Profile 상세 조회
 */
router.get('/profiles/:id', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.getProfileById(req.params.id, tenantId);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `프로파일 조회 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/profiles
 * @desc 신규 Export Profile 생성
 */
router.post('/profiles', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.createProfile(req.body, tenantId);

        LogManager.api('INFO', `프로파일 생성됨: ${req.body.name}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '프로파일 생성 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route PUT /api/export/profiles/:id
 * @desc Export Profile 수정
 */
router.put('/profiles/:id', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.updateProfile(req.params.id, req.body, tenantId);

        LogManager.api('INFO', `프로파일 수정됨: ID ${req.params.id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `프로파일 수정 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route DELETE /api/export/profiles/:id
 * @desc Export Profile 삭제
 */
router.delete('/profiles/:id', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.deleteProfile(req.params.id, tenantId);

        LogManager.api('INFO', `프로파일 삭제됨: ID ${req.params.id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `프로파일 삭제 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

// =============================================================================
// Export Targets
// =============================================================================

/**
 * @route GET /api/export/targets
 * @desc Export Target 목록 조회
 */
router.get('/targets', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.getAllTargets(tenantId);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '타겟 목록 조회 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/targets
 * @desc 신규 Export Target 생성
 */
router.post('/targets', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.createTarget(req.body, tenantId);

        LogManager.api('INFO', `타겟 생성됨: ${req.body.name}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '타겟 생성 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route PUT /api/export/targets/:id
 * @desc Export Target 수정
 */
router.put('/targets/:id', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.updateTarget(req.params.id, req.body, tenantId);

        LogManager.api('INFO', `타겟 수정됨: ID ${req.params.id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `타겟 수정 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route DELETE /api/export/targets/:id
 * @desc Export Target 삭제
 */
router.delete('/targets/:id', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.deleteTarget(req.params.id, tenantId);

        LogManager.api('INFO', `타겟 삭제됨: ID ${req.params.id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `타겟 삭제 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/targets/test
 * @desc Export Target 연결 테스트
 */
router.post('/targets/test', async (req, res) => {
    try {
        const response = await ExportGatewayService.testTargetConnection(req.body);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '타겟 연결 테스트 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

// =============================================================================
// Payload Templates
// =============================================================================

/**
 * @route GET /api/export/templates
 * @desc Payload Template 목록 조회
 */
router.get('/templates', async (req, res) => {
    try {
        const response = await ExportGatewayService.getAllPayloadTemplates();
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '템플릿 목록 조회 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/templates
 * @desc 신규 Payload Template 생성
 */
router.post('/templates', async (req, res) => {
    try {
        const response = await ExportGatewayService.createPayloadTemplate(req.body);
        LogManager.api('INFO', `템플릿 생성됨: ${req.body.name}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '템플릿 생성 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route PUT /api/export/templates/:id
 * @desc Payload Template 수정
 */
router.put('/templates/:id', async (req, res) => {
    try {
        const response = await ExportGatewayService.updatePayloadTemplate(req.params.id, req.body);
        LogManager.api('INFO', `템플릿 수정됨: ID ${req.params.id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `템플릿 수정 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route DELETE /api/export/templates/:id
 * @desc Payload Template 삭제
 */
router.delete('/templates/:id', async (req, res) => {
    try {
        const response = await ExportGatewayService.deletePayloadTemplate(req.params.id);
        LogManager.api('INFO', `템플릿 삭제됨: ID ${req.params.id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `템플릿 삭제 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

// =============================================================================
// Export Schedules
// =============================================================================

/**
 * @route GET /api/export/schedules
 * @desc Export Schedule 목록 조회
 */
router.get('/schedules', async (req, res) => {
    try {
        const response = await ExportGatewayService.getAllSchedules();
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '스케줄 목록 조회 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/schedules
 * @desc 신규 Export Schedule 생성
 */
router.post('/schedules', async (req, res) => {
    try {
        const response = await ExportGatewayService.createSchedule(req.body);
        LogManager.api('INFO', `스케줄 생성됨: ${req.body.schedule_name}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '스케줄 생성 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route PUT /api/export/schedules/:id
 * @desc Export Schedule 수정
 */
router.put('/schedules/:id', async (req, res) => {
    try {
        const response = await ExportGatewayService.updateSchedule(req.params.id, req.body);
        LogManager.api('INFO', `스케줄 수정됨: ID ${req.params.id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `스케줄 수정 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route DELETE /api/export/schedules/:id
 * @desc Export Schedule 삭제
 */
router.delete('/schedules/:id', async (req, res) => {
    try {
        const response = await ExportGatewayService.deleteSchedule(req.params.id);
        LogManager.api('INFO', `스케줄 삭제됨: ID ${req.params.id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `스케줄 삭제 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

// =============================================================================
// Target Mappings
// =============================================================================

/**
 * @route GET /api/export/targets/:targetId/mappings
 * @desc 타겟별 매핑 목록 조회
 */
router.get('/targets/:targetId/mappings', async (req, res) => {
    try {
        const response = await ExportGatewayService.getTargetMappings(req.params.targetId);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `매핑 조회 실패 (${req.params.targetId})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/targets/:targetId/mappings
 * @desc 타겟별 매핑 일괄 저장
 */
router.post('/targets/:targetId/mappings', async (req, res) => {
    try {
        const response = await ExportGatewayService.saveTargetMappings(req.params.targetId, req.body.mappings);
        LogManager.api('INFO', `매핑 저장됨: Target ${req.params.targetId}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `매핑 저장 실패 (${req.params.targetId})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

// =============================================================================
// Export Gateway Management (Dedicated)
// =============================================================================

router.get('/gateways', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const page = parseInt(req.query.page) || 1;
        const limit = parseInt(req.query.limit) || 4; // Adjusted to 4 for easier testing vs Grid
        const response = await ExportGatewayService.getAllGateways(tenantId, page, limit);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '게이트웨이 목록 조회 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/gateways
 * @desc 신규 Export Gateway 등록
 */
router.post('/gateways', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.registerGateway(req.body, tenantId);

        LogManager.api('INFO', `게이트웨이 등록됨: ${req.body.name}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '게이트웨이 등록 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route PUT /api/export/gateways/:id
 * @desc Export Gateway 수정
 */
router.put('/gateways/:id', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.updateGateway(req.params.id, req.body, tenantId);

        LogManager.api('INFO', `게이트웨이 수정됨: ID ${req.params.id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `게이트웨이 수정 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route DELETE /api/export/gateways/:id
 * @desc Export Gateway 삭제
 */
router.delete('/gateways/:id', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.deleteGateway(req.params.id, tenantId);

        LogManager.api('INFO', `게이트웨이 삭제됨: ID ${req.params.id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `게이트웨이 삭제 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

// =============================================================================
// Assignments & Deployment
// =============================================================================

/**
 * @route GET /api/export/gateways/:gatewayId/assignments
 * @desc 특정 게이트웨이의 프로파일 할당 현황 조회
 */
router.get('/gateways/:gatewayId/assignments', async (req, res) => {
    try {
        const response = await ExportGatewayService.getAssignmentsByGateway(req.params.gatewayId);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `할당 조회 실패 (${req.params.gatewayId})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/gateways/:gatewayId/assign
 * @desc 프로파일을 게이트웨이에 할당
 */
router.post('/gateways/:gatewayId/assign', async (req, res) => {
    try {
        const { profileId } = req.body;
        const response = await ExportGatewayService.assignProfileToGateway(profileId, req.params.gatewayId);

        LogManager.api('INFO', `프로파일 할당: P${profileId} -> G${req.params.gatewayId}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '할당 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/gateways/:gatewayId/unassign
 * @desc 프로파일 할당 해제
 */
router.post('/gateways/:gatewayId/unassign', async (req, res) => {
    try {
        const { profileId } = req.body;
        const response = await ExportGatewayService.unassignProfile(profileId, req.params.gatewayId);

        LogManager.api('INFO', `프로파일 할당 해제: P${profileId} -> G${req.params.gatewayId}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '할당 해제 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/gateways/:gatewayId/deploy
 * @desc 변경된 설정을 게이트웨이에 배포 (C2 Reload Command)
 */
router.post('/gateways/:gatewayId/deploy', async (req, res) => {
    try {
        const response = await ExportGatewayService.deployConfig(req.params.gatewayId);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `설정 배포 실패 (${req.params.gatewayId})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/gateways/:gatewayId/manual-export
 * @desc 수동 데이터 전송 트리거
 */
router.post('/gateways/:gatewayId/manual-export', async (req, res) => {
    try {
        // payload requires { target_name, point_id, command_id }
        const response = await ExportGatewayService.manualExport(req.params.gatewayId, req.body);

        LogManager.api('INFO', `수동 전송 명령 전송: G${req.params.gatewayId}, Point ${req.body.point_id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `수동 전송 전송 실패 (${req.params.gatewayId})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/gateways/:gatewayId/start
 * @desc 게이트웨이 프로세스 시작
 */
router.post('/gateways/:gatewayId/start', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.startGateway(req.params.gatewayId, tenantId);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `게이트웨이 시작 실패 (${req.params.gatewayId})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/gateways/:gatewayId/stop
 * @desc 게이트웨이 프로세스 중지
 */
router.post('/gateways/:gatewayId/stop', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.stopGateway(req.params.gatewayId, tenantId);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `게이트웨이 중지 실패 (${req.params.gatewayId})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export/gateways/:gatewayId/restart
 * @desc 게이트웨이 프로세스 재시작
 */
router.post('/gateways/:gatewayId/restart', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.restartGateway(req.params.gatewayId, tenantId);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `게이트웨이 재시작 실패 (${req.params.gatewayId})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

// =============================================================================
// Export Logs
// =============================================================================

/**
 * @route GET /api/export/logs/statistics
 * @desc Export Log 통계 조회
 */
router.get('/logs/statistics', async (req, res) => {
    try {
        const filters = {
            target_id: req.query.target_id,
            status: req.query.status,
            log_type: req.query.log_type,
            date_from: req.query.date_from,
            date_to: req.query.date_to,
            target_type: req.query.target_type,
            search_term: req.query.search_term
        };
        const response = await ExportGatewayService.getExportLogStatistics(filters);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '내보내기 로그 통계 조회 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route GET /api/export/logs
 * @desc Export Log 목록 조회
 */
router.get('/logs', async (req, res) => {
    try {
        const filters = {
            target_id: req.query.target_id,
            status: req.query.status,
            log_type: req.query.log_type,
            date_from: req.query.date_from,
            date_to: req.query.date_to,
            target_type: req.query.target_type,
            search_term: req.query.search_term
        };
        const page = parseInt(req.query.page) || 1;
        const limit = parseInt(req.query.limit) || 50;

        const response = await ExportGatewayService.getExportLogs(filters, page, limit);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '내보내기 로그 조회 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

module.exports = router;
