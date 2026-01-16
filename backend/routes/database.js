// backend/routes/database.js
const express = require('express');
const router = express.Router();
const DatabaseExplorerService = require('../lib/services/DatabaseExplorerService');
const { authenticateToken, requireRole } = require('../middleware/tenantIsolation');
const LogManager = require('../lib/utils/LogManager');

const service = new DatabaseExplorerService();
const logger = LogManager.getInstance();

// Middleware: 모든 요청은 인증된 관리자(system_admin)만 접근 가능
// router.use(authenticateToken); // app.js에서 이미 적용됨
// router.use(requireRole('system_admin')); // TODO: 권한 체계 확인 후 주석 해제

// 1. 테이블 목록 조회
router.get('/tables', async (req, res) => {
    try {
        const tables = await service.listTables();
        const databasePath = await service.getDatabasePath();
        res.json({ success: true, data: { tables, databasePath } });
    } catch (error) {
        logger.error('DatabaseAPI', `List tables failed: ${error.message}`);
        res.status(500).json({ success: false, error: error.message });
    }
});

// 2. 테이블 스키마 조회
router.get('/tables/:tableName/schema', async (req, res) => {
    try {
        const { tableName } = req.params;
        const schema = await service.getTableSchema(tableName);
        res.json({ success: true, data: { schema } });
    } catch (error) {
        logger.error('DatabaseAPI', `Get schema failed: ${error.message}`);
        res.status(500).json({ success: false, error: error.message });
    }
});

// 3. 테이블 데이터 조회
router.get('/tables/:tableName/data', async (req, res) => {
    try {
        const { tableName } = req.params;
        const { page, limit, sort, order } = req.query;

        const result = await service.getTableData(tableName, {
            page: parseInt(page) || 1,
            limit: parseInt(limit) || 50,
            sortColumn: sort,
            sortOrder: order
        });

        res.json({ success: true, data: result });
    } catch (error) {
        logger.error('DatabaseAPI', `Get data failed: ${error.message}`);
        res.status(500).json({ success: false, error: error.message });
    }
});

// 4. 데이터 수정
router.put('/tables/:tableName/rows', async (req, res) => {
    try {
        const { tableName } = req.params;
        const { pkColumn, pkValue, updates } = req.body;

        if (!pkColumn || pkValue === undefined || !updates) {
            return res.status(400).json({ success: false, error: 'Missing required parameters' });
        }

        const result = await service.updateRow(tableName, pkColumn, pkValue, updates);
        res.json({ success: true, data: result });

        logger.system('INFO', `Database row updated by ${req.user?.username}`, {
            table: tableName,
            pk: pkValue,
            updates
        });
    } catch (error) {
        logger.error('DatabaseAPI', `Update row failed: ${error.message}`);
        res.status(500).json({ success: false, error: error.message });
    }
});

// 5. 데이터 삭제
router.delete('/tables/:tableName/rows', async (req, res) => {
    try {
        const { tableName } = req.params;
        // DELETE 요청은 body를 가질 수 있지만, query string이나 url parameter로 받는게 더 RESTful 할 수 있음.
        // 하지만 PK가 복합키이거나 특수문자가 있을 수 있어 여기선 body를 사용하거나,
        // 클라이언트에서 편하게 보내기 위해 query로 받을 수도 있음.
        // 여기서는 안전하게 body를 사용 (client측 axios.delete({ data: ... }) 필요)
        const { pkColumn, pkValue } = req.body;

        if (!pkColumn || pkValue === undefined) {
            // 혹시 query로 보냈을 경우 
            if (req.query.pkColumn && req.query.pkValue) {
                const result = await service.deleteRow(tableName, req.query.pkColumn, req.query.pkValue);
                return res.json({ success: true, data: result });
            }
            return res.status(400).json({ success: false, error: 'Missing pkColumn or pkValue' });
        }

        const result = await service.deleteRow(tableName, pkColumn, pkValue);
        res.json({ success: true, data: result });

        logger.system('WARN', `Database row deleted by ${req.user?.username}`, {
            table: tableName,
            pk: pkValue
        });
    } catch (error) {
        logger.error('DatabaseAPI', `Delete row failed: ${error.message}`);
        res.status(500).json({ success: false, error: error.message });
    }
});

module.exports = router;
