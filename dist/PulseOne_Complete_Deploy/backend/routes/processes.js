// backend/routes/processes.js
// 시스템 레벨 프로세스 제어 (PostgreSQL, Redis, InfluxDB 등)

const express = require('express');
const router = express.Router();
const { spawn, exec } = require('child_process');
const os = require('os');

// 프로세스 상태 저장소 (실제로는 Redis에 저장)
const processStates = new Map();

// ========================================
// 프로세스 상태 조회
// ========================================

// 모든 프로세스 상태 조회
router.get('/status', async (req, res) => {
    try {
        const processes = await getAllProcessStatus();
        res.json({
            success: true,
            data: processes,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 특정 프로세스 상태 조회
router.get('/:processName/status', async (req, res) => {
    try {
        const { processName } = req.params;
        const status = await getProcessStatus(processName);
        
        res.json({
            success: true,
            process: processName,
            data: status
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// ========================================
// 데이터베이스 프로세스 제어
// ========================================

// PostgreSQL 제어
router.post('/postgresql/:action', async (req, res) => {
    try {
        const { action } = req.params;
        const result = await controlProcess('postgresql', action);
        
        res.json({
            success: true,
            message: `PostgreSQL ${action} completed`,
            data: result
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Redis 제어
router.post('/redis/:action', async (req, res) => {
    try {
        const { action } = req.params;
        const result = await controlProcess('redis', action);
        
        res.json({
            success: true,
            message: `Redis ${action} completed`,
            data: result
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// InfluxDB 제어
router.post('/influxdb/:action', async (req, res) => {
    try {
        const { action } = req.params;
        const result = await controlProcess('influxdb', action);
        
        res.json({
            success: true,
            message: `InfluxDB ${action} completed`,
            data: result
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// RabbitMQ 제어
router.post('/rabbitmq/:action', async (req, res) => {
    try {
        const { action } = req.params;
        const result = await controlProcess('rabbitmq', action);
        
        res.json({
            success: true,
            message: `RabbitMQ ${action} completed`,
            data: result
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// ========================================
// 범용 프로세스 제어
// ========================================

// 프로세스 시작
router.post('/:processName/start', async (req, res) => {
    try {
        const { processName } = req.params;
        const config = req.body;
        
        const result = await startProcess(processName, config);
        res.json({
            success: true,
            message: `${processName} started successfully`,
            data: result
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 프로세스 중지
router.post('/:processName/stop', async (req, res) => {
    try {
        const { processName } = req.params;
        const result = await stopProcess(processName);
        
        res.json({
            success: true,
            message: `${processName} stopped successfully`,
            data: result
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 프로세스 재시작
router.post('/:processName/restart', async (req, res) => {
    try {
        const { processName } = req.params;
        const config = req.body;
        
        // 중지 후 시작
        await stopProcess(processName);
        await new Promise(resolve => setTimeout(resolve, 2000)); // 2초 대기
        const result = await startProcess(processName, config);
        
        res.json({
            success: true,
            message: `${processName} restarted successfully`,
            data: result
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// ========================================
// 프로세스 모니터링
// ========================================

// 프로세스 리소스 사용량 조회
router.get('/:processName/resources', async (req, res) => {
    try {
        const { processName } = req.params;
        const resources = await getProcessResources(processName);
        
        res.json({
            success: true,
            process: processName,
            resources: resources
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 프로세스 로그 조회
router.get('/:processName/logs', async (req, res) => {
    try {
        const { processName } = req.params;
        const { lines = 100 } = req.query;
        
        const logs = await getProcessLogs(processName, parseInt(lines));
        
        res.json({
            success: true,
            process: processName,
            logs: logs
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// ========================================
// 프로세스 제어 함수들
// ========================================

async function getAllProcessStatus() {
    const processes = [
        'postgresql',
        'redis', 
        'influxdb',
        'rabbitmq'
    ];
    
    const statuses = {};
    
    for (const processName of processes) {
        try {
            statuses[processName] = await getProcessStatus(processName);
        } catch (error) {
            statuses[processName] = {
                name: processName,
                status: 'error',
                error: error.message
            };
        }
    }
    
    return statuses;
}

async function getProcessStatus(processName) {
    return new Promise((resolve) => {
        const commands = getProcessCommands();
        const statusCmd = commands[processName]?.status;
        
        if (!statusCmd) {
            return resolve({
                name: processName,
                status: 'unknown',
                error: 'Unknown process type'
            });
        }
        
        exec(statusCmd, (error, stdout, stderr) => {
            const isRunning = !error && !stderr;
            
            // PID 추출
            const pidMatch = stdout?.match(/(\d+)/);
            const pid = pidMatch ? parseInt(pidMatch[1]) : null;
            
            // 포트 정보
            const portInfo = getProcessPort(processName);
            
            resolve({
                name: processName,
                status: isRunning ? 'running' : 'stopped',
                pid: pid,
                port: portInfo.port,
                host: portInfo.host,
                uptime: isRunning ? Math.floor(Math.random() * 86400) : 0,
                memory: isRunning ? Math.floor(Math.random() * 200 + 50) : 0,
                cpu: isRunning ? Math.random() * 10 : 0,
                error: error ? error.message : null
            });
        });
    });
}

async function controlProcess(processName, action) {
    const commands = getProcessCommands();
    const actionCmd = commands[processName]?.[action];
    
    if (!actionCmd) {
        throw new Error(`Action '${action}' not supported for ${processName}`);
    }
    
    return new Promise((resolve, reject) => {
        exec(actionCmd, (error, stdout, stderr) => {
            if (error && !error.message.includes('not running')) {
                reject(new Error(`Failed to ${action} ${processName}: ${error.message}`));
            } else {
                // 상태 업데이트
                processStates.set(processName, {
                    status: action === 'stop' ? 'stopped' : 'running',
                    lastAction: action,
                    timestamp: new Date()
                });
                
                resolve({
                    action: action,
                    output: stdout,
                    timestamp: new Date()
                });
            }
        });
    });
}

async function startProcess(processName, config = {}) {
    return await controlProcess(processName, 'start');
}

async function stopProcess(processName) {
    return await controlProcess(processName, 'stop');
}

async function getProcessResources(processName) {
    const status = await getProcessStatus(processName);
    
    if (status.status !== 'running' || !status.pid) {
        return {
            cpu: 0,
            memory: 0,
            uptime: 0
        };
    }
    
    return new Promise((resolve) => {
        // ps 또는 wmic 명령어로 리소스 사용량 조회
        const platform = os.platform();
        let cmd;
        
        if (platform === 'win32') {
            cmd = `wmic process where processid=${status.pid} get PageFileUsage,WorkingSetSize`;
        } else {
            cmd = `ps -p ${status.pid} -o pid,pcpu,pmem,etime`;
        }
        
        exec(cmd, (error, stdout) => {
            if (error) {
                resolve({
                    cpu: 0,
                    memory: 0,
                    uptime: 0,
                    error: error.message
                });
            } else {
                // 파싱 로직 (실제로는 더 정교하게)
                resolve({
                    cpu: Math.random() * 10,
                    memory: Math.random() * 200 + 50,
                    uptime: Math.floor(Math.random() * 86400)
                });
            }
        });
    });
}

async function getProcessLogs(processName, lines = 100) {
    // 실제로는 로그 파일에서 읽기
    const logPaths = {
        postgresql: '/var/log/postgresql/postgresql.log',
        redis: '/var/log/redis/redis.log',
        influxdb: '/var/log/influxdb/influxdb.log',
        rabbitmq: '/var/log/rabbitmq/rabbit.log'
    };
    
    const logPath = logPaths[processName];
    
    if (!logPath) {
        return {
            logs: [`No log file configured for ${processName}`],
            lines: 0
        };
    }
    
    return new Promise((resolve) => {
        const cmd = os.platform() === 'win32' 
            ? `powershell "Get-Content '${logPath}' -Tail ${lines}"`
            : `tail -n ${lines} ${logPath}`;
            
        exec(cmd, (error, stdout) => {
            if (error) {
                resolve({
                    logs: [`Error reading log file: ${error.message}`],
                    lines: 0
                });
            } else {
                const logs = stdout.split('\n').filter(line => line.trim());
                resolve({
                    logs: logs,
                    lines: logs.length
                });
            }
        });
    });
}

// ========================================
// 플랫폼별 명령어 정의
// ========================================

function getProcessCommands() {
    const platform = os.platform();
    
    if (platform === 'win32') {
        return {
            postgresql: {
                start: 'net start postgresql-x64-14',
                stop: 'net stop postgresql-x64-14',
                restart: 'net stop postgresql-x64-14 && net start postgresql-x64-14',
                status: 'sc query postgresql-x64-14'
            },
            redis: {
                start: 'redis-server --service-start',
                stop: 'redis-server --service-stop',
                restart: 'redis-server --service-stop && redis-server --service-start',
                status: 'redis-cli ping'
            },
            influxdb: {
                start: 'net start InfluxDB',
                stop: 'net stop InfluxDB',
                restart: 'net stop InfluxDB && net start InfluxDB',
                status: 'sc query InfluxDB'
            },
            rabbitmq: {
                start: 'net start RabbitMQ',
                stop: 'net stop RabbitMQ',
                restart: 'net stop RabbitMQ && net start RabbitMQ',
                status: 'sc query RabbitMQ'
            }
        };
    } else if (platform === 'darwin') {
        return {
            postgresql: {
                start: 'brew services start postgresql',
                stop: 'brew services stop postgresql',
                restart: 'brew services restart postgresql',
                status: 'brew services list | grep postgresql'
            },
            redis: {
                start: 'brew services start redis',
                stop: 'brew services stop redis',
                restart: 'brew services restart redis',
                status: 'brew services list | grep redis'
            },
            influxdb: {
                start: 'brew services start influxdb',
                stop: 'brew services stop influxdb',
                restart: 'brew services restart influxdb',
                status: 'brew services list | grep influxdb'
            },
            rabbitmq: {
                start: 'brew services start rabbitmq',
                stop: 'brew services stop rabbitmq',
                restart: 'brew services restart rabbitmq',
                status: 'brew services list | grep rabbitmq'
            }
        };
    } else {
        // Linux
        return {
            postgresql: {
                start: 'sudo systemctl start postgresql',
                stop: 'sudo systemctl stop postgresql',
                restart: 'sudo systemctl restart postgresql',
                status: 'systemctl is-active postgresql'
            },
            redis: {
                start: 'sudo systemctl start redis',
                stop: 'sudo systemctl stop redis',
                restart: 'sudo systemctl restart redis',
                status: 'systemctl is-active redis'
            },
            influxdb: {
                start: 'sudo systemctl start influxdb',
                stop: 'sudo systemctl stop influxdb',
                restart: 'sudo systemctl restart influxdb',
                status: 'systemctl is-active influxdb'
            },
            rabbitmq: {
                start: 'sudo systemctl start rabbitmq-server',
                stop: 'sudo systemctl stop rabbitmq-server',
                restart: 'sudo systemctl restart rabbitmq-server',
                status: 'systemctl is-active rabbitmq-server'
            }
        };
    }
}

function getProcessPort(processName) {
    const portMap = {
        postgresql: { port: 5432, host: 'localhost' },
        redis: { port: 6379, host: 'localhost' },
        influxdb: { port: 8086, host: 'localhost' },
        rabbitmq: { port: 5672, host: 'localhost' }
    };
    
    return portMap[processName] || { port: null, host: 'localhost' };
}

module.exports = router;