// =============================================================================
// backend/lib/services/cross-platform/RedisController.js
// Redis 프로세스 시작/중지/재시작
// 원본: crossPlatformManager.js L1320~L1491
// =============================================================================

'use strict';

const { spawn } = require('child_process');
const config = require('../../config/ConfigManager');

class RedisController {
    /**
     * @param {import('./PlatformContext')} ctx
     * @param {import('./ProcessMonitor')} processMonitor
     */
    constructor(ctx, processMonitor) {
        this.ctx = ctx;
        this.processMonitor = processMonitor;
    }

    // =========================================================================
    // ▶️ Redis 시작 (원본 L1324~L1416)
    // win32: detached만 / linux: --daemonize yes 추가
    // =========================================================================

    async startRedis() {
        const { ctx, processMonitor } = this;
        ctx.log('INFO', 'Redis 시작 요청', { platform: ctx.platform });

        try {
            const redisPath = ctx.paths.redis;
            const redisExists = await ctx.fileExists(redisPath);

            if (!redisExists) {
                const errorMsg = `Redis를 찾을 수 없음: ${redisPath}`;
                ctx.log('ERROR', errorMsg);
                return {
                    success: false, error: errorMsg,
                    suggestion: ctx.isWindows
                        ? 'Download Redis for Windows from GitHub'
                        : 'Install Redis using package manager (apt/yum/brew)',
                    platform: ctx.platform
                };
            }

            const processes = await processMonitor.getRunningProcesses();
            if (processes.redis.length > 0) {
                const errorMsg = `Redis가 이미 실행 중입니다 (PID: ${processes.redis[0].pid})`;
                ctx.log('WARN', errorMsg);
                return { success: false, error: errorMsg, platform: ctx.platform, port: 6379 };
            }

            const port = config.getRedisConfig?.()?.port || 6379;
            ctx.log('INFO', 'Redis 프로세스 시작 중...', { port });

            const redisArgs = ctx.isWindows
                ? ['--port', port.toString(), '--maxmemory', '512mb', '--maxmemory-policy', 'allkeys-lru']
                : ['--port', port.toString(), '--daemonize', 'yes', '--maxmemory', '512mb', '--maxmemory-policy', 'allkeys-lru'];

            spawn(redisPath, redisArgs, { detached: true, stdio: 'ignore' });

            await ctx.sleep(2000);

            const newProcesses = await processMonitor.getRunningProcesses();
            const newRedis = newProcesses.redis[0];

            if (newRedis) {
                const successMsg = `Redis가 성공적으로 시작됨 (PID: ${newRedis.pid})`;
                ctx.log('INFO', successMsg, { pid: newRedis.pid, port });
                return { success: true, message: successMsg, pid: newRedis.pid, platform: ctx.platform, port };
            } else {
                ctx.log('ERROR', 'Redis 프로세스 시작 실패');
                return { success: false, error: 'Redis 프로세스 시작 실패', platform: ctx.platform, suggestion: 'Check Redis logs or try manual start' };
            }

        } catch (error) {
            ctx.log('ERROR', 'Redis 시작 중 예외 발생', { error: error.message });
            return { success: false, error: error.message, platform: ctx.platform };
        }
    }

    // =========================================================================
    // ⏹️ Redis 중지 (원본 L1418~L1478)
    // redis-cli shutdown 시도 → 실패 시 kill -9 폴백
    // =========================================================================

    async stopRedis() {
        const { ctx, processMonitor } = this;
        ctx.log('INFO', 'Redis 중지 요청', { platform: ctx.platform });

        try {
            const processes = await processMonitor.getRunningProcesses();
            const runningRedis = processes.redis[0];

            if (!runningRedis) {
                const errorMsg = 'Redis가 실행되고 있지 않습니다';
                ctx.log('WARN', errorMsg);
                return { success: false, error: errorMsg, platform: ctx.platform };
            }

            // redis-cli shutdown 우선 시도
            try {
                ctx.log('DEBUG', 'Redis CLI shutdown 시도');
                await ctx.execCommand(ctx.commands.redisCliShutdown);
                await ctx.sleep(2000);
            } catch (cliError) {
                ctx.log('WARN', 'Redis CLI shutdown 실패, 강제 종료 사용', { error: cliError.message });
                await ctx.execCommand(ctx.commands.processKill(runningRedis.pid));
            }

            await ctx.sleep(1000);
            const newProcesses = await processMonitor.getRunningProcesses();
            const stillRunning = newProcesses.redis.find(p => p.pid === runningRedis.pid);

            if (!stillRunning) {
                ctx.log('INFO', 'Redis가 성공적으로 중지됨', { platform: ctx.platform });
                return { success: true, message: 'Redis가 성공적으로 중지됨', platform: ctx.platform };
            } else {
                // 최종 강제 kill
                ctx.log('DEBUG', 'Redis 강제 종료 시도');
                await ctx.execCommand(ctx.commands.processKill(runningRedis.pid));
                await ctx.sleep(1000);
                ctx.log('INFO', 'Redis가 강제로 중지됨', { platform: ctx.platform });
                return { success: true, message: 'Redis가 강제로 중지됨', platform: ctx.platform };
            }

        } catch (error) {
            ctx.log('ERROR', 'Redis 중지 중 예외 발생', { error: error.message });
            return { success: false, error: error.message, platform: ctx.platform };
        }
    }

    // =========================================================================
    // 🔄 Redis 재시작 (원본 L1480~L1491)
    // stop → 2초 대기 → start
    // =========================================================================

    async restartRedis() {
        const { ctx } = this;
        ctx.log('INFO', 'Redis 재시작 요청', { platform: ctx.platform });

        const stopResult = await this.stopRedis();
        if (!stopResult.success
            && !stopResult.error.includes('not running')
            && !stopResult.error.includes('실행되고 있지 않습니다')) {
            return stopResult;
        }

        await ctx.sleep(2000);
        return await this.startRedis();
    }
}

module.exports = RedisController;
