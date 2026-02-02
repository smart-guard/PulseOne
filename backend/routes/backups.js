const express = require('express');
const router = express.Router();
const BackupService = require('../lib/services/BackupService');

/**
 * GET /api/system/backups
 * 백업 목록 조회
 */
router.get('/backups', async (req, res) => {
    const options = {
        page: req.query.page,
        limit: req.query.limit,
        status: req.query.status,
        type: req.query.type
    };
    const result = await BackupService.getBackups(options);
    res.json(result);
});

/**
 * POST /api/system/backups
 * 즉시 백업 실행
 */
router.post('/backups', async (req, res) => {
    const result = await BackupService.createBackup(req.body, req.user);
    res.json(result);
});

/**
 * DELETE /api/system/backups/:id
 * 백업 삭제
 */
router.delete('/backups/:id', async (req, res) => {
    const result = await BackupService.deleteBackup(req.params.id);
    res.json(result);
});

/**
 * POST /api/system/backups/:id/restore
 * 백업에서 복원
 */
router.post('/backups/:id/restore', async (req, res) => {
    const result = await BackupService.restoreBackup(req.params.id);
    res.json(result);
});

/**
 * GET /api/system/backups/settings
 * 백업 설정 조회
 */
router.get('/settings', async (req, res) => {
    const result = await BackupService.getSettings();
    res.json(result);
});

/**
 * POST /api/system/backups/settings
 * 백업 설정 업데이트
 */
router.post('/settings', async (req, res) => {
    const result = await BackupService.updateSettings(req.body, req.user);
    res.json(result);
});

module.exports = router;
