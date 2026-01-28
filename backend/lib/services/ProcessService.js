// =============================================================================
// backend/lib/services/ProcessService.js
// =============================================================================

const BaseService = require('./BaseService');
const crossPlatformManager = require('./crossPlatformManager');
const os = require('os');
const { exec } = require('child_process');

/**
 * Service class for managing system processes and databases.
 */
class ProcessService extends BaseService {
    constructor() {
        super(null);
    }

    /**
     * Get status of all managed processes.
     */
    async getAllStatus() {
        return await this.handleRequest(async () => {
            const apiResult = await crossPlatformManager.getServicesForAPI();

            // Add other databases if not present
            const services = apiResult.data;
            const extraProcesses = ['postgresql', 'influxdb', 'rabbitmq'];

            for (const name of extraProcesses) {
                if (!services.find(s => s.name === name)) {
                    const status = await this.getSpecificStatus(name);
                    services.push(status);
                }
            }

            return {
                processes: services,
                platform: apiResult.platform,
                summary: apiResult.summary
            };
        }, 'ProcessService.getAllStatus');
    }

    /**
     * Get status of a specific process.
     */
    async getSpecificStatus(processName) {
        return await this.handleRequest(async () => {
            const isDocker = process.env.DOCKER_CONTAINER === 'true';

            // Docker 환경에서 외부 서비스(DB 등)는 포트 체크로 확인
            if (isDocker) {
                const ports = {
                    'postgresql': 5432,
                    'influxdb': 8086,
                    'rabbitmq': 5672,
                    'redis': 6379,
                    'collector': 50510
                };

                const port = ports[processName.toLowerCase()];
                if (port) {
                    const host = processName.toLowerCase() === 'collector' ? 'collector' :
                        processName.toLowerCase() === 'redis' ? 'redis' :
                            processName.toLowerCase() === 'rabbitmq' ? 'rabbitmq' :
                                processName.toLowerCase() === 'influxdb' ? 'influxdb' :
                                    processName.toLowerCase() === 'postgresql' ? 'postgres' : 'localhost';

                    const isPortOpen = await this._checkTcpPort(host, port);
                    return {
                        name: processName,
                        status: isPortOpen ? 'running' : 'stopped',
                        timestamp: new Date().toISOString(),
                        checkMethod: 'tcp-port'
                    };
                }
            }

            const commands = this._getPlatformCommands();
            const statusCmd = commands[processName]?.status;

            if (!statusCmd) {
                return { name: processName, status: 'unknown' };
            }

            return new Promise((resolve) => {
                exec(statusCmd, (error, stdout, stderr) => {
                    const isRunning = !error && !stderr;
                    resolve({
                        name: processName,
                        status: isRunning ? 'running' : 'stopped',
                        timestamp: new Date().toISOString(),
                        checkMethod: 'platform-cmd'
                    });
                });
            });
        }, `ProcessService.getSpecificStatus(${processName})`);
    }

    /**
     * TCP 포트 오픈 여부 확인 (헬스체크용)
     */
    _checkTcpPort(host, port, timeout = 1000) {
        const net = require('net');
        return new Promise((resolve) => {
            const socket = new net.Socket();
            let resolved = false;

            const timer = setTimeout(() => {
                if (!resolved) {
                    resolved = true;
                    socket.destroy();
                    resolve(false);
                }
            }, timeout);

            socket.connect(port, host, () => {
                if (!resolved) {
                    resolved = true;
                    socket.end();
                    clearTimeout(timer);
                    resolve(true);
                }
            });

            socket.on('error', () => {
                if (!resolved) {
                    resolved = true;
                    socket.destroy();
                    clearTimeout(timer);
                    resolve(false);
                }
            });
        });
    }

    /**
     * Control a process (start, stop, restart).
     */
    async controlProcess(processName, action) {
        return await this.handleRequest(async () => {
            // Priority 1: Built-in crossPlatformManager methods
            const lowerName = processName.toLowerCase();
            if (lowerName === 'collector' || lowerName.startsWith('collector-')) {
                let collectorId = null;
                if (lowerName.startsWith('collector-')) {
                    const idPart = lowerName.split('-')[1];
                    collectorId = idPart === 'default' ? null : parseInt(idPart);
                }
                if (action === 'start') return await crossPlatformManager.startCollector(collectorId);
                if (action === 'stop') return await crossPlatformManager.stopCollector(collectorId);
                if (action === 'restart') return await crossPlatformManager.restartCollector(collectorId);
            } else if (lowerName === 'export-gateway' || lowerName.startsWith('export-gateway-')) {
                let gatewayId = null;
                if (lowerName.startsWith('export-gateway-')) {
                    const idPart = lowerName.split('-')[2]; // export-gateway-id
                    gatewayId = idPart === 'default' ? null : parseInt(idPart);
                }
                if (action === 'start') return await crossPlatformManager.startExportGateway(gatewayId);
                if (action === 'stop') return await crossPlatformManager.stopExportGateway(gatewayId);
                if (action === 'restart') return await crossPlatformManager.restartExportGateway(gatewayId);
            } else if (lowerName === 'redis') {
                if (action === 'start') return await crossPlatformManager.startRedis();
                if (action === 'stop') return await crossPlatformManager.stopRedis();
                if (action === 'restart') return await crossPlatformManager.restartRedis();
            }

            // Priority 2: Generic platform commands
            const commands = this._getPlatformCommands();
            const actionCmd = commands[processName]?.[action];

            if (!actionCmd) {
                throw new Error(`Action '${action}' not supported for ${processName}`);
            }

            const { stdout, stderr } = await crossPlatformManager.execCommand(actionCmd);
            return {
                success: true,
                action,
                process: processName,
                output: stdout.trim() || 'Command executed successfully'
            };
        }, `ProcessService.controlProcess(${processName}, ${action})`);
    }

    /**
     * Get platform information.
     */
    async getPlatformInfo() {
        return await this.handleRequest(async () => {
            return crossPlatformManager.getPlatformInfo();
        }, 'ProcessService.getPlatformInfo');
    }

    /**
     * Internal: Get platform-specific commands (copied logic from original route)
     */
    _getPlatformCommands() {
        const platform = os.platform();
        if (platform === 'win32') {
            return {
                postgresql: { start: 'net start postgresql-x64-14', stop: 'net stop postgresql-x64-14', status: 'sc query postgresql-x64-14' },
                influxdb: { start: 'net start InfluxDB', stop: 'net stop InfluxDB', status: 'sc query InfluxDB' },
                rabbitmq: { start: 'net start RabbitMQ', stop: 'net stop RabbitMQ', status: 'sc query RabbitMQ' }
            };
        } else if (platform === 'darwin') {
            return {
                postgresql: { start: 'brew services start postgresql', stop: 'brew services stop postgresql', status: 'brew services list | grep postgresql' },
                influxdb: { start: 'brew services start influxdb', stop: 'brew services stop influxdb', status: 'brew services list | grep influxdb' },
                rabbitmq: { start: 'brew services start rabbitmq', stop: 'brew services stop rabbitmq', status: 'brew services list | grep rabbitmq' }
            };
        } else {
            return {
                postgresql: { start: 'sudo systemctl start postgresql', stop: 'sudo systemctl stop postgresql', status: 'systemctl is-active postgresql' },
                influxdb: { start: 'sudo systemctl start influxdb', stop: 'sudo systemctl stop influxdb', status: 'systemctl is-active influxdb' },
                rabbitmq: { start: 'sudo systemctl start rabbitmq-server', stop: 'sudo systemctl stop rabbitmq-server', status: 'systemctl is-active rabbitmq-server' }
            };
        }
    }
}

module.exports = new ProcessService();
