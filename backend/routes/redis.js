const express = require('express');
const router = express.Router();
const { getRedisClient, getRedisManager } = require('../lib/connection/redis');
const logger = require('../lib/utils/LogManager');

// Helper to handle async errors
const asyncHandler = (fn) => (req, res, next) => {
    Promise.resolve(fn(req, res, next)).catch(next);
};

// Check if Redis is available
const checkRedis = async (req, res, next) => {
    // 1. Check for dynamic connection request
    const customConfigStr = req.headers['x-redis-config'];
    if (customConfigStr) {
        try {
            const config = JSON.parse(customConfigStr);
            const redis = require('redis'); // Import here to avoid global dependency issues if not needed elsewhere

            // Create ad-hoc client
            const client = redis.createClient({
                socket: {
                    host: config.host || 'localhost',
                    port: config.port || 6379,
                    connectTimeout: 3000
                },
                password: config.password || undefined,
                database: config.db || 0
            });

            await client.connect();

            req.redis = client;
            req.isCustomClient = true;
            req.redisConnectionConfig = {
                host: config.host || 'localhost',
                port: config.port || 6379,
                db: config.db || 0
            };

            // Ensure disconnection on response finish
            res.on('finish', async () => {
                if (client.isOpen) {
                    try {
                        await client.disconnect();
                    } catch (e) {
                        logger.api('WARN', 'Failed to disconnect custom Redis client', { error: e.message });
                    }
                }
            });

            next();
            return;
        } catch (error) {
            logger.api('ERROR', 'Failed to connect to custom Redis', { error: error.message });
            return res.status(400).json({ success: false, error: `Invalid Redis Config or Connection Failed: ${error.message}` });
        }
    }

    // 2. Fallback to Default System Redis
    try {
        const client = await getRedisClient();
        if (!client || !client.isOpen) {
            return res.status(503).json({
                success: false,
                error: 'Redis connection is not available'
            });
        }
        req.redis = client;
        next();
    } catch (error) {
        logger.api('ERROR', 'Redis access failed', { error: error.message });
        res.status(503).json({ success: false, error: 'Redis service unavailable' });
    }
};

router.use(checkRedis);

// GET /api/redis/info - Server Info
router.get('/info', asyncHandler(async (req, res) => {
    const info = await req.redis.info();

    // Parse INFO output into a more usable JSON format
    const sections = {};
    let currentSection = 'default';

    info.split('\r\n').forEach(line => {
        if (line.startsWith('#')) {
            currentSection = line.substring(2).toLowerCase();
            sections[currentSection] = {};
        } else if (line.indexOf(':') > -1) {
            const [key, value] = line.split(':');
            if (sections[currentSection]) {
                sections[currentSection][key] = value;
            }
        }
    });

    const connectionInfo = req.isCustomClient ? req.redisConnectionConfig : require('../lib/connection/redis').getRedisManager().getStatus().config;

    res.json({
        success: true,
        data: {
            info: sections,
            connection: {
                host: connectionInfo.host,
                port: connectionInfo.port,
                db: connectionInfo.db
            }
        }
    });
}));

// GET /api/redis/scan - Scan keys
router.get('/scan', asyncHandler(async (req, res) => {
    const { cursor = 0, match = '*', count = 100 } = req.query;

    const result = await req.redis.scan(cursor, {
        MATCH: match,
        COUNT: parseInt(count)
    });

    const keys = result.keys;
    const nextCursor = result.cursor;

    // Enrich keys with simplified Type/TTL info (optional, might be slow for large sets)
    // For performance, we might just return keys. Let's return keys first.
    // To make it useful, let's fetch types pipeline

    const pipeline = req.redis.multi();
    keys.forEach(key => {
        pipeline.type(key);
        pipeline.ttl(key);
    });

    const metadata = await pipeline.exec(); // [[null, 'string'], [null, -1], ...]

    const enrichedKeys = keys.map((key, index) => {
        // exec returns array of responses. Each response is just the value in v4? 
        // node-redis v4 multi.exec returns array of results.
        // Actually it depends on legacy mode. Let's assume standard v4.
        const type = metadata[index * 2];   // type result
        const ttl = metadata[index * 2 + 1]; // ttl result

        return {
            key,
            type: type,
            ttl: ttl
        };
    });

    res.json({
        success: true,
        data: {
            cursor: nextCursor,
            keys: enrichedKeys
        }
    });
}));

// GET /api/redis/:key - Get Key Details
router.get('/key/:key(*)', asyncHandler(async (req, res) => {
    // using (* ) to allow slashes in key name
    const key = req.params.key;
    const type = await req.redis.type(key);
    const ttl = await req.redis.ttl(key);
    let value = null;

    switch (type) {
        case 'string':
            value = await req.redis.get(key);
            break;
        case 'list':
            value = await req.redis.lRange(key, 0, -1);
            break;
        case 'set':
            value = await req.redis.sMembers(key);
            break;
        case 'zset':
            value = await req.redis.zRangeWithScores(key, 0, -1);
            break;
        case 'hash':
            value = await req.redis.hGetAll(key);
            break;
        case 'stream':
            // Streams are complex to view fully, maybe just last few entries
            value = "(Stream viewing not fully supported yet)";
            break;
        case 'none':
            return res.status(404).json({ success: false, error: 'Key not found' });
        default:
            value = `(Unsupported type: ${type})`;
    }

    // Try to parse JSON if string
    let isJson = false;
    if (type === 'string') {
        try {
            JSON.parse(value);
            isJson = true;
        } catch (e) {
            isJson = false;
        }
    }

    res.json({
        success: true,
        data: {
            key,
            type,
            ttl,
            value,
            isJson
        }
    });
}));

// POST /api/redis - Set Key
router.post('/', asyncHandler(async (req, res) => {
    const { key, value, type = 'string', ttl } = req.body;

    if (!key) return res.status(400).json({ success: false, error: 'Key is required' });

    // Handle complex types
    if (type === 'string') {
        let valToSet = value;
        if (typeof value === 'object') {
            valToSet = JSON.stringify(value);
        }
        await req.redis.set(key, valToSet);
    }
    else if (type === 'list') {
        if (!Array.isArray(value)) {
            return res.status(400).json({ success: false, error: 'Value must be an array for List type' });
        }

        // Transaction: Delete + RPUSH
        const multi = req.redis.multi();
        multi.del(key);
        if (value.length > 0) {
            // Serialize items if they are objects
            const items = value.map(v => typeof v === 'object' ? JSON.stringify(v) : String(v));
            multi.rPush(key, items);
        }
        await multi.exec();
    }
    else if (type === 'set') {
        if (!Array.isArray(value)) {
            return res.status(400).json({ success: false, error: 'Value must be an array for Set type' });
        }

        const multi = req.redis.multi();
        multi.del(key);
        if (value.length > 0) {
            const items = value.map(v => typeof v === 'object' ? JSON.stringify(v) : String(v));
            multi.sAdd(key, items);
        }
        await multi.exec();
    }
    else if (type === 'hash') {
        if (typeof value !== 'object' || Array.isArray(value)) {
            return res.status(400).json({ success: false, error: 'Value must be an object for Hash type' });
        }

        const multi = req.redis.multi();
        multi.del(key);
        // Serialize values
        const entries = {};
        for (const [k, v] of Object.entries(value)) {
            entries[k] = typeof v === 'object' ? JSON.stringify(v) : String(v);
        }
        if (Object.keys(entries).length > 0) {
            multi.hSet(key, entries);
        }
        await multi.exec();
    }
    else {
        return res.status(501).json({ success: false, error: `Type ${type} is not currently supported for updates` });
    }

    if (ttl && ttl > 0) {
        await req.redis.expire(key, parseInt(ttl));
    }

    res.json({
        success: true,
        message: `Key ${key} updated`
    });
}));

// DELETE /api/redis/:key - Delete Key
router.delete('/:key(*)', asyncHandler(async (req, res) => {
    const key = req.params.key;
    const result = await req.redis.del(key);

    if (result === 1) {
        res.json({ success: true, message: 'Key deleted' });
    } else {
        res.status(404).json({ success: false, error: 'Key not found' });
    }
}));

module.exports = router;
