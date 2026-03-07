// =============================================================================
// backend/lib/services/cross-platform/CollectorController.js
// Collector 프로세스 시작/스폰/중지/재시작
// 원본: crossPlatformManager.js L713~L924
// =============================================================================

'use strict';

const path = require('path');
const fsSync = require('fs');
const { spawn } = require('child_process');
const config = require('../../config/ConfigManager');

class CollectorController {
    /**
     * @param {import('./PlatformContext')} ctx
     * @param {import('./ProcessMonitor')} processMonitor
     */
    constructor(ctx, processMonitor) {
        this.ctx = ctx;
        this.processMonitor = processMonitor;
    }

    // =========================================================================
    // ▶️ Collector 시작 (원본 L717~L804)
    // 1) 실행파일 존재 확인
    // 2) 이미 실행 중 여부 확인
    // 3) spawn → 3초 대기 → 프로세스 재확인
    // =========================================================================

    async startCollector(collectorId = null) {
        const { ctx, processMonitor } = this;
        ctx.log('INFO', 'Collector 시작 요청', {
            platform: ctx.platform, path: ctx.paths.collector, workingDir: process.cwd()
        });

        try {
            const collectorExists = await ctx.fileExists(ctx.paths.collector);
            ctx.log('DEBUG', `실행파일 존재 확인: ${collectorExists}`);

            if (!collectorExists) {
                const errorMsg = `Collector 실행파일을 찾을 수 없음: ${ctx.paths.collector}`;
                ctx.log('ERROR', errorMsg);
                return {
                    success: false, error: errorMsg,
                    suggestion: ctx.isDevelopment ? 'Build the Collector project first' : 'Install PulseOne Collector package',
                    buildCommand: ctx.isWindows ? 'cd collector && make' : 'cd collector && make debug',
                    platform: ctx.platform, development: ctx.isDevelopment,
                    timestamp: new Date().toISOString()
                };
            }

            const processes = await processMonitor.getRunningProcesses();
            const existingCollector = collectorId !== null
                ? processes.collector.find(p => p.collectorId === collectorId)
                : processes.collector[0];

            if (existingCollector) {
                const errorMsg = `Collector(ID: ${collectorId || 'default'})가 이미 실행 중입니다 (PID: ${existingCollector.pid})`;
                ctx.log('WARN', errorMsg);
                return { success: false, error: errorMsg, pid: existingCollector.pid, platform: ctx.platform };
            }

            ctx.log('INFO', `Collector 프로세스 시작 중... (ID: ${collectorId || 'default'})`);
            await this.spawnCollector(collectorId);

            ctx.log('DEBUG', '프로세스 시작 대기 중... (3초)');
            await ctx.sleep(3000);

            const newProcesses = await processMonitor.getRunningProcesses();
            const newCollector = collectorId !== null
                ? newProcesses.collector.find(p => p.collectorId === collectorId)
                : newProcesses.collector[0];

            if (newCollector) {
                const successMsg = `Collector(ID: ${collectorId || 'default'})가 성공적으로 시작됨 (PID: ${newCollector.pid})`;
                ctx.log('INFO', successMsg, { pid: newCollector.pid, platform: ctx.platform });
                return {
                    success: true, message: successMsg,
                    pid: newCollector.pid, collectorId, platform: ctx.platform,
                    executable: ctx.paths.collector
                };
            } else {
                const errorMsg = 'Collector 프로세스 시작 실패';
                ctx.log('ERROR', errorMsg);
                return { success: false, error: errorMsg, platform: ctx.platform, suggestion: 'Check collector logs for startup errors' };
            }

        } catch (error) {
            ctx.log('ERROR', 'Collector 시작 중 예외 발생', { error: error.message });
            return { success: false, error: error.message, platform: ctx.platform };
        }
    }

    // =========================================================================
    // 🚀 Collector 프로세스 스폰 (원본 L806~L855)
    // detached + stdio ignore 로 백그라운드 실행
    // =========================================================================

    async spawnCollector(collectorId = null) {
        const { ctx } = this;
        const absoluteCollectorPath = path.resolve(ctx.paths.collector);
        const args = collectorId !== null ? ['--id', collectorId.toString()] : [];

        ctx.log('DEBUG', 'Collector 프로세스 스폰 시도', {
            originalPath: ctx.paths.collector, absolutePath: absoluteCollectorPath,
            args, exists: fsSync.existsSync(absoluteCollectorPath),
            workingDir: path.dirname(absoluteCollectorPath)
        });

        if (!fsSync.existsSync(absoluteCollectorPath)) {
            throw new Error(`Collector 실행파일이 존재하지 않음: ${absoluteCollectorPath}`);
        }

        const env = ctx.isWindows
            ? { ...process.env, TZ: 'Asia/Seoul', REDIS_HOST: config.get('REDIS_HOST') || 'localhost', DATA_DIR: ctx.paths.root, PULSEONE_DATA_DIR: ctx.paths.root }
            : { ...process.env, TZ: 'Asia/Seoul', REDIS_HOST: config.get('REDIS_HOST') || 'localhost', LD_LIBRARY_PATH: '/usr/local/lib:/usr/lib', PATH: process.env.PATH + ':/usr/local/bin', DATA_DIR: ctx.paths.root, PULSEONE_DATA_DIR: ctx.paths.root };

        return spawn(absoluteCollectorPath, args, {
            cwd: ctx.paths.root, detached: true, stdio: 'ignore', env
        });
    }

    // =========================================================================
    // ⏹️ Collector 중지 (원본 L857~L911)
    // kill -9 / taskkill 후 3초 대기 → 프로세스 재확인
    // =========================================================================

    async stopCollector(collectorId = null) {
        const { ctx, processMonitor } = this;
        ctx.log('INFO', 'Collector 중지 요청', { platform: ctx.platform, collectorId });

        try {
            const processes = await processMonitor.getRunningProcesses();
            const runningCollector = collectorId !== null
                ? processes.collector.find(p => p.collectorId === collectorId)
                : processes.collector[0];

            if (!runningCollector) {
                const errorMsg = `Collector(ID: ${collectorId || 'default'})가 실행되고 있지 않습니다`;
                ctx.log('WARN', errorMsg);
                return { success: false, error: errorMsg, platform: ctx.platform };
            }

            ctx.log('INFO', `프로세스 종료 중... PID: ${runningCollector.pid}`);
            await ctx.execCommand(ctx.commands.processKill(runningCollector.pid));
            await ctx.sleep(3000);

            const newProcesses = await processMonitor.getRunningProcesses();
            const stillRunning = newProcesses.collector.find(p => p.pid === runningCollector.pid);

            if (!stillRunning) {
                const successMsg = 'Collector가 성공적으로 중지됨';
                ctx.log('INFO', successMsg, { platform: ctx.platform });
                return { success: true, message: successMsg, platform: ctx.platform };
            } else {
                ctx.log('ERROR', 'Collector 프로세스 중지 실패');
                return { success: false, error: 'Collector 프로세스 중지 실패', platform: ctx.platform };
            }

        } catch (error) {
            ctx.log('ERROR', 'Collector 중지 중 예외 발생', { error: error.message });
            return { success: false, error: error.message, platform: ctx.platform };
        }
    }

    // =========================================================================
    // 🔄 Collector 재시작 (원본 L913~L924)
    // stop → 2초 대기 → start
    // =========================================================================

    async restartCollector(collectorId = null) {
        const { ctx } = this;
        ctx.log('INFO', 'Collector 재시작 요청', { platform: ctx.platform, collectorId });

        const stopResult = await this.stopCollector(collectorId);
        if (!stopResult.success
            && !stopResult.error.includes('not running')
            && !stopResult.error.includes('실행되고 있지 않습니다')) {
            return stopResult;
        }

        await ctx.sleep(2000);
        return await this.startCollector(collectorId);
    }
}

module.exports = CollectorController;
