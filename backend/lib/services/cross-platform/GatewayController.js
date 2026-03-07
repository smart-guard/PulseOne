// =============================================================================
// backend/lib/services/cross-platform/GatewayController.js
// Export Gateway 프로세스 시작/스폰/중지/재시작
// 원본: crossPlatformManager.js L1035~L1190
// =============================================================================

'use strict';

const path = require('path');
const { spawn } = require('child_process');
const config = require('../../config/ConfigManager');

class GatewayController {
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
    // ▶️ Export Gateway 시작 (원본 L1039~L1106)
    // Docker 환경에서는 컨테이너가 자동 처리 → 즉시 success 반환
    // =========================================================================

    async startExportGateway(gatewayId = null) {
        const { ctx, processMonitor } = this;
        ctx.log('INFO', 'Export Gateway 시작 요청', { platform: ctx.platform, path: ctx.paths.exportGateway, gatewayId });

        // Docker 환경: 전용 컨테이너가 DB 변경 감지하여 자동 시작
        if (ctx.isDocker) {
            ctx.log('INFO', 'Docker 환경 감지: 전용 컨테이너에 제어 위임');
            return {
                success: true,
                message: 'Docker 환경: 전용 컨테이너에서 게이트웨이가 자동 시작됩니다.',
                platform: 'docker'
            };
        }

        try {
            const gatewayExists = await ctx.fileExists(ctx.paths.exportGateway);
            if (!gatewayExists) {
                return {
                    success: false,
                    error: `Export Gateway 실행파일을 찾을 수 없음: ${ctx.paths.exportGateway}`,
                    platform: ctx.platform
                };
            }

            const processes = await processMonitor.getRunningProcesses();
            const existingGateway = gatewayId !== null
                ? processes.exportGateway.find(p => p.gatewayId === gatewayId)
                : processes.exportGateway[0];

            if (existingGateway) {
                return {
                    success: false,
                    error: `Export Gateway(ID: ${gatewayId || 'default'})가 이미 실행 중입니다`,
                    pid: existingGateway.pid
                };
            }

            await this.spawnExportGateway(gatewayId);
            await ctx.sleep(3000);

            const newProcesses = await processMonitor.getRunningProcesses();
            const newGateway = gatewayId !== null
                ? newProcesses.exportGateway.find(p => p.gatewayId === gatewayId)
                : newProcesses.exportGateway[0];

            if (newGateway) {
                return {
                    success: true,
                    message: `Export Gateway(ID: ${gatewayId || 'default'}) 시작됨`,
                    pid: newGateway.pid, gatewayId, platform: ctx.platform
                };
            } else {
                return { success: false, error: 'Export Gateway 시작 실패', platform: ctx.platform };
            }
        } catch (error) {
            ctx.log('ERROR', 'Export Gateway 시작 예외', { error: error.message });
            return { success: false, error: error.message, platform: ctx.platform };
        }
    }

    // =========================================================================
    // 🚀 Export Gateway 스폰 (원본 L1108~L1145)
    // win32: TZ + REDIS_HOST / linux: + LD_LIBRARY_PATH
    // =========================================================================

    async spawnExportGateway(gatewayId = null) {
        const { ctx } = this;
        const absolutePath = path.resolve(ctx.paths.exportGateway);
        const args = gatewayId !== null ? ['--id', gatewayId.toString()] : [];

        ctx.log('DEBUG', 'Export Gateway spawn', { absolutePath, args });

        const env = ctx.isWindows
            ? { ...process.env, TZ: 'Asia/Seoul', REDIS_HOST: config.get('REDIS_HOST') || 'localhost', DATA_DIR: ctx.paths.root, PULSEONE_DATA_DIR: ctx.paths.root }
            : { ...process.env, TZ: 'Asia/Seoul', REDIS_HOST: config.get('REDIS_HOST') || 'localhost', LD_LIBRARY_PATH: '/usr/local/lib:/usr/lib', DATA_DIR: ctx.paths.root, PULSEONE_DATA_DIR: ctx.paths.root };

        return spawn(absolutePath, args, {
            cwd: ctx.paths.root, detached: true, stdio: 'ignore', env
        });
    }

    // =========================================================================
    // ⏹️ Export Gateway 중지 (원본 L1147~L1173)
    // =========================================================================

    async stopExportGateway(gatewayId = null) {
        const { ctx, processMonitor } = this;
        ctx.log('INFO', 'Export Gateway 중지 요청', { gatewayId });

        try {
            const processes = await processMonitor.getRunningProcesses();
            const runningGateway = gatewayId !== null
                ? processes.exportGateway.find(p => p.gatewayId === gatewayId)
                : processes.exportGateway[0];

            if (!runningGateway) {
                return { success: false, error: '실행 중인 게이트웨이가 없습니다' };
            }

            await ctx.execCommand(ctx.commands.processKill(runningGateway.pid));
            await ctx.sleep(2000);

            return { success: true, message: 'Export Gateway 중지됨' };
        } catch (error) {
            return { success: false, error: error.message };
        }
    }

    // =========================================================================
    // 🔄 Export Gateway 재시작 (원본 L1175~L1190)
    // stop → 1초 대기 → start
    // =========================================================================

    async restartExportGateway(gatewayId = null) {
        const { ctx } = this;
        ctx.log('INFO', 'Export Gateway 재시작 요청', { gatewayId });

        const stopResult = await this.stopExportGateway(gatewayId);
        if (!stopResult.success && stopResult.error !== '실행 중인 게이트웨이가 없습니다') {
            return stopResult;
        }

        await ctx.sleep(1000);
        return await this.startExportGateway(gatewayId);
    }
}

module.exports = GatewayController;
