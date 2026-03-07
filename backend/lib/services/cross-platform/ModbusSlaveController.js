// =============================================================================
// backend/lib/services/cross-platform/ModbusSlaveController.js
// Modbus Slave 프로세스 시작/스폰/중지/재시작
// 원본: crossPlatformManager.js L1192~L1318
// =============================================================================

'use strict';

const { spawn } = require('child_process');
const config = require('../../config/ConfigManager');

class ModbusSlaveController {
    /**
     * @param {import('./PlatformContext')} ctx
     * @param {import('./ProcessMonitor')} processMonitor
     * @param {import('./DockerController')} dockerController
     */
    constructor(ctx, processMonitor, dockerController) {
        this.ctx = ctx;
        this.processMonitor = processMonitor;
        this.dockerController = dockerController;
    }

    // =========================================================================
    // ▶️ Modbus Slave 시작 (원본 L1196~L1253)
    // Docker 환경: Supervisor가 DB 변경 감지하여 자동 spawn (최대 60초 이내)
    // =========================================================================

    async startModbusSlave(deviceId = null) {
        const { ctx, processMonitor } = this;
        ctx.log('INFO', 'Modbus Slave 시작 요청', { platform: ctx.platform, deviceId });

        if (ctx.isDocker) {
            return {
                success: true,
                message: 'Docker 환경: Modbus Slave Supervisor가 자동으로 디바이스를 시작합니다 (최대 60초 내).',
                platform: 'docker'
            };
        }

        try {
            const slaveExists = await ctx.fileExists(ctx.paths.modbusSlave || '');
            if (!slaveExists) {
                return {
                    success: false,
                    error: `Modbus Slave 실행파일을 찾을 수 없음: ${ctx.paths.modbusSlave}`,
                    platform: ctx.platform
                };
            }

            const processes = await processMonitor.getRunningProcesses();
            const existing = deviceId !== null
                ? (processes.modbusSlave || []).find(p => p.deviceId === deviceId)
                : (processes.modbusSlave || [])[0];

            if (existing) {
                return {
                    success: false,
                    error: `Modbus Slave(device-id: ${deviceId || 'supervisor'})가 이미 실행 중입니다 (PID: ${existing.pid})`,
                    pid: existing.pid
                };
            }

            await this.spawnModbusSlave(deviceId);
            await ctx.sleep(3000);

            const newProcesses = await processMonitor.getRunningProcesses();
            const newSlave = deviceId !== null
                ? (newProcesses.modbusSlave || []).find(p => p.deviceId === deviceId)
                : (newProcesses.modbusSlave || [])[0];

            if (newSlave) {
                return {
                    success: true,
                    message: `Modbus Slave(device-id: ${deviceId || 'supervisor'}) 시작됨`,
                    pid: newSlave.pid, deviceId, platform: ctx.platform
                };
            } else {
                return { success: false, error: 'Modbus Slave 시작 실패', platform: ctx.platform };
            }
        } catch (error) {
            return { success: false, error: error.message, platform: ctx.platform };
        }
    }

    // =========================================================================
    // 🚀 Modbus Slave 스폰 (원본 L1255~L1273)
    // --device-id 인자로 특정 디바이스 지정 가능
    // =========================================================================

    async spawnModbusSlave(deviceId = null) {
        const { ctx } = this;
        const absolutePath = require('path').resolve(ctx.paths.modbusSlave || '');
        const args = deviceId !== null ? ['--device-id', deviceId.toString()] : [];

        ctx.log('DEBUG', 'Modbus Slave spawn', { absolutePath, args });

        return spawn(absolutePath, args, {
            cwd: ctx.paths.root,
            detached: true,
            stdio: 'ignore',
            env: {
                ...process.env,
                TZ: 'Asia/Seoul',
                REDIS_PRIMARY_HOST: config.get('REDIS_PRIMARY_HOST') || config.get('REDIS_HOST') || 'localhost',
                SQLITE_PATH: ctx.paths.sqlite,
                LD_LIBRARY_PATH: '/usr/local/lib:/usr/lib'
            }
        });
    }

    // =========================================================================
    // ⏹️ Modbus Slave 중지 (원본 L1275~L1303)
    // Docker 환경: enabled=0으로 변경하면 Supervisor가 자동 kill
    // =========================================================================

    async stopModbusSlave(deviceId = null) {
        const { ctx, processMonitor } = this;
        ctx.log('INFO', 'Modbus Slave 중지 요청', { deviceId });

        if (ctx.isDocker) {
            return {
                success: true,
                message: 'Docker 환경: modbus_slave_devices.enabled=0 으로 설정하면 Supervisor가 자동 중지합니다.',
                platform: 'docker'
            };
        }

        try {
            const processes = await processMonitor.getRunningProcesses();
            const running = deviceId !== null
                ? (processes.modbusSlave || []).find(p => p.deviceId === deviceId)
                : (processes.modbusSlave || [])[0];

            if (!running) {
                return { success: false, error: '실행 중인 Modbus Slave가 없습니다' };
            }

            await ctx.execCommand(ctx.commands.processKill(running.pid));
            await ctx.sleep(2000);
            return { success: true, message: `Modbus Slave(PID: ${running.pid}) 중지됨` };
        } catch (error) {
            return { success: false, error: error.message };
        }
    }

    // =========================================================================
    // 🔄 Modbus Slave 재시작 (원본 L1305~L1318)
    // Docker: controlDockerContainer 위임
    // =========================================================================

    async restartModbusSlave(deviceId = null) {
        const { ctx, dockerController } = this;
        ctx.log('INFO', 'Modbus Slave 재시작 요청', { deviceId });

        if (ctx.isDocker) {
            return await dockerController.controlDockerContainer('modbus-slave', 'restart', deviceId);
        }

        const stopResult = await this.stopModbusSlave(deviceId);
        if (!stopResult.success && stopResult.error !== '실행 중인 Modbus Slave가 없습니다') {
            return stopResult;
        }
        await ctx.sleep(1000);
        return await this.startModbusSlave(deviceId);
    }
}

module.exports = ModbusSlaveController;
