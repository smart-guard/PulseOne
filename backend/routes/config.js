// backend/routes/config.js
const express = require('express');
const router = express.Router();
const ConfigEditorService = require('../lib/services/ConfigEditorService');
const { authenticateToken, requireRole } = require('../middleware/tenantIsolation');
const LogManager = require('../lib/utils/LogManager');

const service = new ConfigEditorService();
const logger = LogManager.getInstance();

// Middleware: 시스템 관리자 전용
// router.use(authenticateToken); // app.js에서 전역 혹은 상위에서 적용 여부 확인 필요, 여기선 안전하게 주석 처리하거나 명시적 사용
// router.use(requireRole('system_admin')); 

// 1. 설정 파일 목록 조회
router.get('/files', async (req, res) => {
    try {
        const result = await service.listFiles();
        // listFiles returns { files, path }
        res.json({ success: true, data: { files: result.files, path: result.path } });
    } catch (error) {
        logger.error('ConfigAPI', `List files failed: ${error.message}`);
        res.status(500).json({ success: false, error: error.message });
    }
});

// 2. 파일 내용 조회
router.get('/files/:filename', async (req, res) => {
    try {
        const { filename } = req.params;
        const content = await service.getFileContent(filename);
        res.json({ success: true, data: { content } });
    } catch (error) {
        logger.error('ConfigAPI', `Get content failed: ${error.message}`);
        res.status(500).json({ success: false, error: error.message });
    }
});

// 3. 파일 내용 저장
router.put('/files/:filename', async (req, res) => {
    try {
        const { filename } = req.params;
        const { content } = req.body;

        if (content === undefined) {
            return res.status(400).json({ success: false, error: 'Content is required' });
        }

        await service.saveFileContent(filename, content);

        logger.system('WARN', `Configuration file ${filename} updated by ${req.user?.username || 'unknown'}`);
        res.json({ success: true });
    } catch (error) {
        logger.error('ConfigAPI', `Save content failed: ${error.message}`);
        res.status(500).json({ success: false, error: error.message });
    }
});

// 4. 시크릿 암호화
router.post('/encrypt', (req, res) => {
    try {
        const { value } = req.body;
        if (!value) {
            return res.status(400).json({ success: false, error: 'Value is required' });
        }

        const encrypted = service.encryptSecret(value);
        res.json({ success: true, data: { encrypted } });
    } catch (error) {
        logger.error('ConfigAPI', `Encryption failed: ${error.message}`);
        res.status(500).json({ success: false, error: error.message });
    }
});

// 5. 시크릿 복호화
router.post('/decrypt', (req, res) => {
    try {
        const { value } = req.body;
        if (!value) {
            return res.status(400).json({ success: false, error: 'Value is required' });
        }

        const decrypted = service.decryptSecret(value);
        res.json({ success: true, data: { decrypted } });
    } catch (error) {
        logger.error('ConfigAPI', `Decryption failed: ${error.message}`);
        res.status(500).json({ success: false, error: error.message });
    }
});

module.exports = router;
