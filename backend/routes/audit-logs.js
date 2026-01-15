const express = require('express');
const router = express.Router();
const AuditLogService = require('../lib/services/AuditLogService');

const auditLogService = new AuditLogService();

/**
 * @route GET /api/audit-logs
 * @desc 감사 로그 목록 조회
 */
router.get('/', async (req, res) => {
    const options = {
        entityType: req.query.entityType,
        entityId: req.query.entityId,
        action: req.query.action,
        userId: req.query.userId,
        startDate: req.query.startDate,
        endDate: req.query.endDate,
        page: req.query.page,
        limit: req.query.limit
    };

    const result = await auditLogService.getLogs(req.tenantId, options);
    res.status(result.success ? 200 : 500).json(result);
});

module.exports = router;
