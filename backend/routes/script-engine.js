// ============================================================================
// backend/routes/script-engine.js
// ============================================================================

const express = require('express');
const router = express.Router();
const ScriptEngineService = require('../lib/services/ScriptEngineService');
const { createResponse } = require('../lib/utils/responseUtils'); // Assuming this existence or using local if not

/**
 * GET /api/script-engine/functions
 */
router.get('/functions', async (req, res) => {
    try {
        const { category, search } = req.query;
        const result = await ScriptEngineService.getFunctions(category, search);
        res.json(createResponse(true, result, 'Functions retrieved successfully'));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'FUNCTIONS_ERROR'));
    }
});

/**
 * POST /api/script-engine/validate
 */
router.post('/validate', async (req, res) => {
    try {
        const { script, context = {} } = req.body;
        const result = await ScriptEngineService.validateScript(script, context);
        res.json(createResponse(true, result, 'Script validation completed'));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'VALIDATION_ERROR'));
    }
});

/**
 * POST /api/script-engine/test
 */
router.post('/test', async (req, res) => {
    try {
        const { script, context = {} } = req.body;
        const result = await ScriptEngineService.testScript(script, context);
        res.json(createResponse(true, result, 'Script test completed'));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'TEST_ERROR'));
    }
});

/**
 * POST /api/script-engine/parse
 */
router.post('/parse', async (req, res) => {
    try {
        const { script } = req.body;
        const result = await ScriptEngineService.parseScript(script);
        res.json(createResponse(true, result, 'Script parsing completed'));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'PARSE_ERROR'));
    }
});

/**
 * GET /api/script-engine/templates
 */
router.get('/templates', async (req, res) => {
    try {
        const { category } = req.query;
        const result = await ScriptEngineService.getTemplates(category);
        res.json(createResponse(true, result, 'Templates retrieved successfully'));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'TEMPLATES_ERROR'));
    }
});

/**
 * GET /api/script-engine/test
 */
router.get('/test', async (req, res) => {
    try {
        const dbResult = await ScriptEngineService.verifyDatabase();
        res.json(createResponse(true, {
            message: 'Script Engine API is working!',
            database_test: dbResult,
            architecture: 'Service-Repository Pattern'
        }, 'Script Engine API test successful'));
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'TEST_ERROR'));
    }
});

module.exports = router;