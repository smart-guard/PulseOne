// ===========================================================================
// backend/routes/system.js
// ===========================================================================

const express = require('express');
const router = express.Router();
const SystemService = require('../lib/services/SystemService');
const { getInstance: getRepositoryFactory } = require('../lib/database/repositories/RepositoryFactory');

/**
 * GET /api/system/databases
 */
router.get('/databases', async (req, res) => {
    try {
        const result = await SystemService.getDatabaseStatus();
        res.json(result);
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

/**
 * GET /api/system/config
 */
router.get('/config', async (req, res) => {
    try {
        const result = await SystemService.getSystemConfig();
        res.json({ success: true, data: result });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

/**
 * GET /api/system/info
 */
router.get('/info', async (req, res) => {
    try {
        const result = await SystemService.getSystemInfo();
        res.json({ success: true, data: result });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

/**
 * GET /api/system/health
 */
router.get('/health', (req, res) => {
    res.json({
        status: 'healthy',
        timestamp: new Date().toISOString(),
        uptime: process.uptime()
    });
});

/**
 * GET /api/system/log-level
 * Collector 시작 시 호출 - DB에 저장된 로그레벨 반환
 */
router.get('/log-level', async (req, res) => {
    try {
        const repo = getRepositoryFactory().getSystemSettingsRepository();
        const level = await repo.getValue('log_level', 'INFO');
        res.json({ success: true, level: level.toUpperCase() });
    } catch (error) {
        res.json({ success: true, level: 'INFO' });
    }
});

/**
 * GET /api/system/logging-settings
 * Frontend에서 현재 로깅 설정 조회
 */
router.get('/logging-settings', async (req, res) => {
    try {
        const repo = getRepositoryFactory().getSystemSettingsRepository();
        const settings = {
            log_level: await repo.getValue('log_level', 'INFO'),
            enable_debug_logging: (await repo.getValue('enable_debug_logging', 'false')) === 'true',
            log_to_file: (await repo.getValue('log_to_file', 'true')) === 'true',
            log_to_db: (await repo.getValue('log_to_db', 'true')) === 'true',
            max_log_file_size_mb: parseInt(await repo.getValue('max_log_file_size_mb', '100'), 10),
            log_retention_days: parseInt(await repo.getValue('log_retention_days', '30'), 10),
            packet_logging_enabled: (await repo.getValue('packet_logging_enabled', 'false')) === 'true',
            compress_old_logs: (await repo.getValue('compress_old_logs', 'true')) === 'true',
        };
        res.json({ success: true, data: settings });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

/**
 * GET /api/system/general-settings
 * Frontend에서 현재 일반 설정 조회
 */
router.get('/general-settings', async (req, res) => {
    try {
        const repo = getRepositoryFactory().getSystemSettingsRepository();
        const settings = {
            system_name: await repo.getValue('general_system_name', 'PulseOne IoT Platform'),
            timezone: await repo.getValue('general_timezone', 'Asia/Seoul'),
            language: await repo.getValue('general_language', 'ko-KR'),
            date_format: await repo.getValue('general_date_format', 'YYYY-MM-DD'),
            time_format: await repo.getValue('general_time_format', '24h'),
            session_timeout: parseInt(await repo.getValue('general_session_timeout', '480'), 10),
            max_login_attempts: parseInt(await repo.getValue('general_max_login_attempts', '5'), 10),
            account_lockout_duration: parseInt(await repo.getValue('general_account_lockout_duration', '30'), 10),
        };
        res.json({ success: true, data: settings });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

/**
 * GET /api/system/data-collection-settings
 * Frontend에서 현재 데이터 수집 설정 조회
 */
router.get('/data-collection-settings', async (req, res) => {
    try {
        const repo = getRepositoryFactory().getSystemSettingsRepository();
        const settings = {
            default_polling_interval: parseInt(await repo.getValue('default_polling_interval', '5000'), 10),
            max_concurrent_connections: parseInt(await repo.getValue('max_concurrent_connections', '20'), 10),
            device_response_timeout: parseInt(await repo.getValue('device_response_timeout', '3000'), 10),
            influxdb_storage_interval: parseInt(await repo.getValue('influxdb_storage_interval', '0'), 10),
        };
        res.json({ success: true, data: settings });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// ============================================================
// DB 저장 헬퍼
// ============================================================
async function saveToDb(repo, key, value) {
    try {
        await repo.updateValue(key, String(value));
    } catch (e) {
        console.warn(`[system.js] Failed to save ${key} to DB:`, e.message);
    }
}

/**
 * PUT /api/system/settings
 * 로깅 설정 + 일반 설정 저장 + Collector 실시간 전파
 */
router.put('/settings', async (req, res) => {
    try {
        const { logging, data_collection, general } = req.body || {};
        const results = { saved: [], errors: [] };
        const repo = getRepositoryFactory().getSystemSettingsRepository();

        // ── 일반 설정 ──────────────────────────────────────────
        if (general) {
            const generalKeys = [
                'system_name', 'timezone', 'language',
                'date_format', 'time_format',
                'session_timeout', 'max_login_attempts', 'account_lockout_duration'
            ];
            for (const key of generalKeys) {
                if (general[key] !== undefined) {
                    await saveToDb(repo, `general_${key}`, general[key]);
                    results.saved.push({ key: `general_${key}`, value: general[key] });
                }
            }
        }

        // ── 로깅 설정 ──────────────────────────────────────────
        if (logging) {
            // 1. log_level: DB 저장 + Collector 실시간 전파
            if (logging.log_level !== undefined) {
                const level = String(logging.log_level).toUpperCase();
                await saveToDb(repo, 'log_level', level);

                try {
                    const { getInstance: getCollectorProxy } = require('../lib/services/CollectorProxyService');
                    const result = await getCollectorProxy().setLogLevel(level);
                    results.saved.push({ key: 'log_level', value: level, ...result });
                    console.log(`[system] log_level → ${level} (propagated: ${result.propagated}/${result.total})`);
                } catch (err) {
                    console.warn('[system] log_level propagation failed:', err.message);
                    results.saved.push({ key: 'log_level', value: level, propagated: 0 });
                }
            }

            // 2. Collector에 전파할 로깅 설정 묶음
            const collectorLoggingConfig = {};

            if (logging.enable_debug_logging !== undefined) {
                const val = Boolean(logging.enable_debug_logging);
                await saveToDb(repo, 'enable_debug_logging', val);
                collectorLoggingConfig.enable_debug_logging = val;
                results.saved.push({ key: 'enable_debug_logging', value: val });
            }

            if (logging.log_to_file !== undefined) {
                const val = Boolean(logging.log_to_file);
                await saveToDb(repo, 'log_to_file', val);
                collectorLoggingConfig.log_to_file = val;
                results.saved.push({ key: 'log_to_file', value: val });
            }

            if (logging.max_log_file_size_mb !== undefined) {
                const val = parseInt(logging.max_log_file_size_mb, 10) || 100;
                await saveToDb(repo, 'max_log_file_size_mb', val);
                collectorLoggingConfig.max_log_file_size_mb = val;
                results.saved.push({ key: 'max_log_file_size_mb', value: val });
            }

            // 3. Backend 전용 설정 (DB 저장만)
            if (logging.log_to_db !== undefined) {
                const val = Boolean(logging.log_to_db);
                await saveToDb(repo, 'log_to_db', val);
                results.saved.push({ key: 'log_to_db', value: val });
            }

            if (logging.log_retention_days !== undefined) {
                const val = parseInt(logging.log_retention_days, 10) || 30;
                await saveToDb(repo, 'log_retention_days', val);
                results.saved.push({ key: 'log_retention_days', value: val });
            }

            if (logging.compress_old_logs !== undefined) {
                const val = Boolean(logging.compress_old_logs);
                await saveToDb(repo, 'compress_old_logs', val);
                results.saved.push({ key: 'compress_old_logs', value: val });
            }

            // 4. 패킷 로깅 (data_collection 카테고리지만 DB+Collector 전파)
            if (logging.packet_logging_enabled !== undefined) {
                const val = Boolean(logging.packet_logging_enabled);
                await saveToDb(repo, 'packet_logging_enabled', val);
                collectorLoggingConfig.packet_logging_enabled = val;
                results.saved.push({ key: 'packet_logging_enabled', value: val });
            }

            // 5. Collector에 로깅 설정 일괄 전파
            if (Object.keys(collectorLoggingConfig).length > 0) {
                try {
                    const { getInstance: getCollectorProxy } = require('../lib/services/CollectorProxyService');
                    const result = await getCollectorProxy().setLoggingConfig(collectorLoggingConfig);
                    console.log(`[system] logging-config propagated (${result.propagated}/${result.total})`);
                } catch (err) {
                    console.warn('[system] logging-config propagation failed:', err.message);
                }
            }
        }

        // ── 데이터 수집 설정 (패킷 로깅 포함) ────────────────────
        if (data_collection) {
            if (data_collection.packet_logging_enabled !== undefined) {
                const val = Boolean(data_collection.packet_logging_enabled);
                await saveToDb(repo, 'packet_logging_enabled', val);

                try {
                    const { getInstance: getCollectorProxy } = require('../lib/services/CollectorProxyService');
                    await getCollectorProxy().setLoggingConfig({ packet_logging_enabled: val });
                    results.saved.push({ key: 'packet_logging_enabled', value: val });
                    console.log(`[system] packet_logging_enabled → ${val}`);
                } catch (err) {
                    console.warn('[system] packet_logging propagation failed:', err.message);
                }
            }

            // 기타 data_collection 설정은 DB에만 저장
            const dcKeys = ['default_polling_interval', 'max_concurrent_connections', 'device_response_timeout', 'influxdb_storage_interval'];
            for (const key of dcKeys) {
                if (data_collection[key] !== undefined) {
                    await saveToDb(repo, key, data_collection[key]);
                    results.saved.push({ key, value: data_collection[key] });
                }
            }
        }

        res.json({ success: true, results });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

module.exports = router;