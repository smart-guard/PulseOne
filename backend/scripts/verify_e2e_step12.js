// backend/scripts/verify_e2e_step12.js
const Redis = require('ioredis');
const sqlite3 = require('sqlite3').verbose();
const axios = require('axios');
const path = require('path');

// --- Configuration ---
const ARGS = process.argv.slice(2);
const DEVICE_NAME = ARGS.find(arg => arg.startsWith('--deviceName='))?.split('=')[1] || 'E2E_Test_Device_01';

const DB_PATH = process.env.SQLITE_DB_PATH || path.join(__dirname, '../../data/db/pulseone.db');
const REDIS_HOST = process.env.REDIS_HOST || 'localhost';
const REDIS_PORT = process.env.REDIS_PORT || 6379;
const INFLUX_URL = process.env.INFLUX_URL || 'http://localhost:8086';
const INFLUX_TOKEN = process.env.INFLUX_TOKEN || 'my-super-secret-auth-token';
const INFLUX_ORG = process.env.INFLUX_ORG || 'pulseone';
const INFLUX_BUCKET = process.env.INFLUX_BUCKET || 'timeseries';

// --- Utilities ---
const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms));
const log = (msg, type = 'INFO') => {
    const icon = type === 'PASS' ? '✅' : type === 'FAIL' ? '❌' : type === 'SUCCESS' ? '✅' : 'ℹ️';
    console.log(`${icon} [${type}] ${msg}`);
};

// --- Main Verification Flow ---
async function verify() {
    log(`Starting E2E Verification for Device: ${DEVICE_NAME}`, 'INFO');

    // 1. Discovery: Connect to SQLite and find Device ID
    log(`[Step 1] Resolving Device ID from SQLite...`, 'INFO');
    const deviceId = await getDeviceIdByName(DEVICE_NAME);
    if (!deviceId) {
        log(`Device '${DEVICE_NAME}' not found in SQLite. Have you created it in the UI?`, 'FAIL');
        process.exit(1);
    }
    log(`Device Found: ID = ${deviceId}`, 'PASS');

    // 2. Redis: Check Realtime Data
    log(`[Step 2] Checking Redis Realtime Data (Polling 30s)...`, 'INFO');
    const redisClient = new Redis({ host: REDIS_HOST, port: REDIS_PORT });
    const redisSuccess = await checkRedisData(redisClient, deviceId);
    redisClient.disconnect();

    if (!redisSuccess) {
        log(`Redis data scan timed out. Collector might not be running or device not connected.`, 'FAIL');
        // Don't exit yet, check other stores
    }

    // 3. RDB: Check Device Status
    log(`[Step 3] Checking Device Connection Status in RDB...`, 'INFO');
    const status = await getDeviceStatus(deviceId);
    if (status === 'connected') {
        log(`Device Status is 'connected'`, 'PASS');
    } else {
        log(`Device Status is '${status}' (Expected: 'connected')`, 'FAIL');
    }

    // 4. InfluxDB: Check Time Series Data (Generic & Robot)
    log(`[Step 4] Checking InfluxDB History...`, 'INFO');

    // Check Generic (should be 0 for ROBOT now, but let's just check existence first)
    // Actually, we want to verify ROBOT data specifically
    const influxSuccess = await checkInfluxRobotData(deviceId, "Temperature");

    if (influxSuccess) {
        log('✅ InfluxDB Data (Robot Mode) Verified', 'SUCCESS');
    } else {
        log('❌ InfluxDB Data Verification Failed', 'ERROR');
        process.exit(1);
    }

    // [Step 5] Checking Alarms and Virtual Points (Advanced Verification)
    log('[Step 5] Checking Alarms and Virtual Points (Advanced Verification)...', 'INFO');

    // Need a fresh Redis client
    const redisCheckClient = new Redis({ host: REDIS_HOST, port: REDIS_PORT });
    let alarmSuccess = false;
    let vpSuccess = false;

    // 5.1 Check Alarm (Poll for up to 10s)
    const alarmKey = 'alarm:active:1001';
    let alarmStatus = null;
    for (let k = 0; k < 5; k++) {
        alarmStatus = await redisCheckClient.get(alarmKey);
        if (alarmStatus) break;
        await sleep(2000);
    }

    if (alarmStatus) {
        log(`Alarm Verified: Key ${alarmKey} exists.`, 'PASS');
        alarmSuccess = true;
    } else {
        log(`Alarm Not Found: Key ${alarmKey} does not exist after polling.`, 'FAIL');
    }

    // 5.2 Check Virtual Point (Poll for up to 10s)
    const vpKey = 'virtualpoint:1001';
    let vpData = null;
    for (let k = 0; k < 5; k++) {
        vpData = await redisCheckClient.get(vpKey);
        if (vpData) break;
        await sleep(2000);
    }

    if (vpData) {
        log(`Virtual Point Verified: Key ${vpKey} exists.`, 'PASS');
        try {
            const vpJson = JSON.parse(vpData);
            if (vpJson.value > 0) {
                log(`Virtual Point Calculation Valid: ${vpJson.value}`, 'PASS');
                vpSuccess = true;
            } else {
                log(`Virtual Point Value Warning: ${vpJson.value}`, 'FAIL');
            }
        } catch (e) {
            log('Virtual Point JSON Parse Error', 'FAIL');
        }
    } else {
        log(`Virtual Point Not Found: Key ${vpKey} does not exist after polling.`, 'FAIL');
    }

    redisCheckClient.disconnect();

    // Final Summary
    console.log('\n--- E2E Verification Summary ---');
    // Using loose conditions for Alarm/VP to avoid blocking success if not created yet (user might want pipeline verified mainly)
    // But user asked for "Alarm/VP" verification. So failing them is appropriate if they don't exist.
    if (redisSuccess && status === 'connected' && influxSuccess) {
        if (alarmSuccess && vpSuccess) {
            log(`ALL CHECKS PASSED (Pipeline + Alarms + VPs)`, 'PASS');
            process.exit(0);
        } else {
            log(`PIPELINE PASSED, but Advanced Checks (Alarm/VP) Failed. Did you create them?`, 'FAIL');
            process.exit(1);
        }
    } else {
        log(`SOME CHECKS FAILED`, 'FAIL');
        process.exit(1);
    }
}

// --- Helpers ---

function getDeviceIdByName(name) {
    return new Promise((resolve, reject) => {
        const db = new sqlite3.Database(DB_PATH, sqlite3.OPEN_READONLY, (err) => {
            if (err) return reject(err);
        });
        db.get("SELECT id FROM devices WHERE name = ?", [name], (err, row) => {
            db.close();
            if (err) reject(err);
            resolve(row ? row.id : null);
        });
    });
}

function getDeviceStatus(id) {
    return new Promise((resolve, reject) => {
        const db = new sqlite3.Database(DB_PATH, sqlite3.OPEN_READONLY, (err) => {
            if (err) return reject(err);
        });
        db.get("SELECT connection_status FROM device_status WHERE device_id = ?", [id], (err, row) => {
            db.close();
            if (err) reject(err);
            resolve(row ? row.connection_status : 'unknown');
        });
    });
}

async function checkRedisData(redis, deviceId) {
    const key = `current_values:${deviceId}`;
    for (let i = 0; i < 15; i++) { // 15 * 2s = 30s
        const data = await redis.get(key);
        if (data) {
            try {
                const parsed = JSON.parse(data);
                log(`Redis Data Received: ${JSON.stringify(parsed).substring(0, 100)}...`, 'PASS');
                return true;
            } catch (e) {
                log(`Invalid JSON in Redis: ${data}`, 'FAIL');
                return false;
            }
        }
        await sleep(2000);
        process.stdout.write('.');
    }
    console.log(''); // Newline
    return false;
}

async function checkInfluxRobotData(deviceId, fieldName) {
    const query = `
    from(bucket: "${INFLUX_BUCKET}")
    |> range(start: -10m)
    |> filter(fn: (r) => r["_measurement"] == "robot_state")
    |> filter(fn: (r) => r["device_id"] == "${deviceId}")
    |> filter(fn: (r) => r["_field"] == "${fieldName}")
    |> count()
`;

    log(`Querying InfluxDB (Robot Mode): device=${deviceId}, field=${fieldName}`, 'DEBUG');

    try {
        const response = await axios.post(`${INFLUX_URL}/api/v2/query?org=${INFLUX_ORG}`, query, {
            headers: {
                'Authorization': `Token ${INFLUX_TOKEN}`,
                'Content-Type': 'application/vnd.flux',
                'Accept': 'application/csv'
            }
        });

        const lines = response.data.trim().split('\n');
        if (lines.length > 1) {
            const count = lines[1].split(',')[5]; // Value column usually
            log(`Found ${lines.length - 1} records. Count value: ${count}`, 'PASS');
            return true;
        }
        log(`No robot data found. Response: ${response.data}`, 'FAIL');
        return false;
    } catch (error) {
        log(`InfluxDB Query Error: ${error.message}`, 'FAIL');
        if (error.response) log(error.response.data, 'FAIL');
        return false;
    }
}

verify().catch(err => {
    console.error('Unexpected Error:', err);
    process.exit(1);
});
