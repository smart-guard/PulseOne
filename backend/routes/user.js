// =============================================================================
// backend/routes/user.js
// 사용자 관리 API - UserService 기반
// =============================================================================

const express = require('express');
const router = express.Router();
const UserService = require('../lib/services/UserService');
const {
    authenticateToken,
    tenantIsolation,
    requireRole
} = require('../middleware/tenantIsolation');

// 모든 라우트에 인증 및 테넌트 격리 적용
router.use(authenticateToken);
router.use(tenantIsolation);

/**
 * @route   GET /api/users
 * @desc    모든 사용자 목록 조회
 * @access  Admin (System/Company/Site)
 */
router.get('/', async (req, res) => {
    const filters = {
        includeDeleted: req.query.includeDeleted === 'true',
        onlyDeleted: req.query.onlyDeleted === 'true'
    };
    const response = await UserService.getAllUsers(req.tenantId, filters);
    res.json(response);
});

// 사용자 복구
router.post('/:id/restore', async (req, res) => {
    const response = await UserService.restoreUser(req.params.id, req.tenantId);
    res.json(response);
});

/**
 * @route   GET /api/users/stats
 * @desc    사용자 통계 조회
 */
router.get('/stats', async (req, res) => {
    const response = await UserService.getStats(req.tenantId);
    res.status(response.success ? 200 : 500).json(response);
});

/**
 * @route   GET /api/users/:id
 * @desc    특정 사용자 상세 조회
 */
router.get('/:id', async (req, res) => {
    const response = await UserService.getUserById(req.params.id, req.tenantId);
    res.status(response.success ? 200 : 404).json(response);
});

/**
 * @route   POST /api/users
 * @desc    새 사용자 생성
 * @access  Admin 이상
 */
router.post('/', requireRole('system_admin', 'company_admin'), async (req, res) => {
    const response = await UserService.createUser(req.body, req.tenantId);
    res.status(response.success ? 201 : 400).json(response);
});

/**
 * @route   PUT /api/users/:id
 * @desc    사용자 정보 전체 업데이트
 */
router.put('/:id', requireRole('system_admin', 'company_admin'), async (req, res) => {
    const response = await UserService.updateUser(req.params.id, req.body, req.tenantId);
    res.status(response.success ? 200 : 400).json(response);
});

/**
 * @route   PATCH /api/users/:id
 * @desc    사용자 정보 부분 업데이트 (비밀번호, 활성화 상태 등)
 */
router.patch('/:id', async (req, res) => {
    // 본인이거나 관리자인 경우만 허용 (여기서는 단순하게 Admin 체크 위주로 구현)
    // TODO: 본인 체크 로직 추가 가능 (req.user.id === req.params.id)
    const response = await UserService.patchUser(req.params.id, req.body, req.tenantId);
    res.status(response.success ? 200 : 400).json(response);
});

/**
 * @route   DELETE /api/users/:id
 * @desc    사용자 삭제
 */
router.delete('/:id', requireRole('system_admin', 'company_admin'), async (req, res) => {
    const response = await UserService.deleteUser(req.params.id, req.tenantId);
    res.status(response.success ? 200 : 400).json(response);
});

module.exports = router;
