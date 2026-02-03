const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');
const config = require('../lib/config/ConfigManager').getInstance();
const logger = require('../lib/utils/LogManager');

/**
 * GET /api/blobs/:id
 * Serves a blob file by its ID (filename).
 */
router.get('/:id', (req, res) => {
    try {
        const fileId = req.params.id;

        // 1. Sanitize input to prevent directory traversal
        // Only allow alphanumeric, underscores, dots, and hyphens
        if (!/^[a-zA-Z0-9_\-\.]+$/.test(fileId)) {
            logger.api('WARN', 'Invalid blob ID requested', { fileId });
            return res.status(400).json({
                success: false,
                error: 'Invalid file ID'
            });
        }

        // 2. Resolve blob directory
        // In Docker, this is usually /app/data/blobs
        // ConfigManager handle platform specific paths
        const defaultBlobPath = path.resolve(process.cwd(), '../data/blobs');
        const blobDir = config.getSmartPath('BLOB_STORAGE_PATH', defaultBlobPath);
        const filePath = path.join(blobDir, fileId);

        // 3. Check if file exists
        if (!fs.existsSync(filePath)) {
            logger.api('WARN', 'Blob file not found', { filePath });
            return res.status(404).json({
                success: false,
                error: 'File not found'
            });
        }

        // 4. Serve the file
        logger.api('INFO', 'Serving blob file', { fileId });

        // Determine content type based on extension if possible
        const ext = path.extname(fileId).toLowerCase();
        let contentType = 'application/octet-stream';

        if (ext === '.json') contentType = 'application/json';
        else if (ext === '.txt') contentType = 'text/plain';
        else if (ext === '.bin') contentType = 'application/octet-stream';
        else if (ext === '.jpg' || ext === '.jpeg') contentType = 'image/jpeg';
        else if (ext === '.png') contentType = 'image/png';

        res.setHeader('Content-Type', contentType);
        // Hint the browser to download the file
        res.setHeader('Content-Disposition', `attachment; filename="${fileId}"`);

        res.sendFile(filePath);

    } catch (error) {
        logger.api('ERROR', 'Failed to serve blob file', { error: error.message });
        res.status(500).json({
            success: false,
            error: 'Internal server error'
        });
    }
});

module.exports = router;
