// =============================================================================
// backend/lib/services/cross-platform/ProcessMonitor.js
// 프로세스 조회: Windows(tasklist) / Unix(ps aux) / Docker 컨테이너 상태
// 원본: crossPlatformManager.js L194~L393
// =============================================================================

'use strict';

class ProcessMonitor {
    /**
     * @param {import('./PlatformContext')} ctx — PlatformContext 인스턴스
     */
    constructor(ctx) {
        this.ctx = ctx;
    }

    // =========================================================================
    // 🔍 프로세스 조회 진입점 (원본 L198~L219)
    // Windows / Unix 분기 후 Docker 컨테이너 정보 추가
    // =========================================================================

    async getRunningProcesses() {
        const { ctx } = this;
        try {
            const results = ctx.isWindows
                ? await this.getWindowsProcesses()
                : await this.getUnixProcesses();

            // Docker 환경이라면 컨테이너 정보 추가 수집
            if (ctx.isDocker || ctx.isDevelopment) {
                try {
                    const dockerInfo = await this._getDockerStatuses();
                    results.dockerContainers = dockerInfo;
                } catch (e) {
                    ctx.log('DEBUG', 'Docker API를 통한 상태 조회 실패 (무시 가능)', { error: e.message });
                }
            }

            return results;
        } catch (error) {
            ctx.log('ERROR', 'getRunningProcesses 실패', { error: error.message });
            return { backend: [], collector: [], exportGateway: [], redis: [], dockerContainers: [] };
        }
    }

    // =========================================================================
    // 🪟 Windows 프로세스 조회 (원본 L221~L327)
    // tasklist /fo csv 파싱 → 이미지명 기준 분류
    // =========================================================================

    async getWindowsProcesses() {
        const { ctx } = this;
        try {
            ctx.log('DEBUG', 'Windows 프로세스 감지 시작');

            const { stdout } = await ctx.execCommand('tasklist /fo csv');
            ctx.log('DEBUG', `tasklist 출력 길이: ${stdout.length} 문자`);

            const lines = stdout.split('\n').filter(line => line.trim());
            const processes = { backend: [], collector: [], exportGateway: [], redis: [], modbusSlave: [] };

            lines.forEach((line, index) => {
                if (index === 0) return; // 헤더 스킵

                // CSV 파싱 — 따옴표 포함 필드 처리
                const csvParts = line.match(/(\".*?\"|[^\",\s]+)(?=\s*,|\s*$)/g);
                if (!csvParts || csvParts.length < 5) return;

                const imageName = csvParts[0].replace(/"/g, '').trim();
                const pid = parseInt(csvParts[1].replace(/"/g, '').trim());
                const memUsage = csvParts[4].replace(/"/g, '').trim();

                if (isNaN(pid)) return;

                const processInfo = {
                    pid,
                    name: imageName,
                    commandLine: imageName,
                    startTime: new Date(),
                    platform: 'win32',
                    memory: memUsage
                };

                // pkg 컴파일된 실제 exe명 기준 분류
                const lower = imageName.toLowerCase();

                if (lower.includes('pulseone-backend.exe') || lower.includes('node.exe')) {
                    processes.backend.push(processInfo);
                } else if (lower.includes('pulseone-collector.exe') || lower.includes('collector.exe')) {
                    ctx.log('INFO', `Collector 프로세스 발견: PID ${pid}`);
                    processes.collector.push(processInfo);
                } else if (lower.includes('redis-server.exe')) {
                    ctx.log('INFO', `Redis 프로세스 발견: PID ${pid}`);
                    processes.redis.push(processInfo);
                } else if (lower.includes('pulseone-export-gateway.exe') || lower.includes('export-gateway.exe')) {
                    ctx.log('INFO', `Export Gateway 프로세스 발견: PID ${pid}`);
                    processes.exportGateway.push(processInfo);
                } else if (lower.includes('pulseone-modbus-slave.exe') || lower.includes('modbus-slave.exe')) {
                    ctx.log('INFO', `Modbus Slave 프로세스 발견: PID ${pid}`);
                    processes.modbusSlave.push(processInfo);
                }
            });

            ctx.log('INFO', '프로세스 감지 완료', {
                backend: processes.backend.length,
                collector: processes.collector.length,
                exportGateway: processes.exportGateway.length,
                redis: processes.redis.length
            });

            return processes;

        } catch (error) {
            ctx.log('ERROR', 'Windows 프로세스 감지 실패', { error: error.message });

            // 폴백: findstr 방식
            try {
                const collectorCheck = await ctx.execCommand('tasklist | findstr /i "collector pulseone-collector"');
                const redisCheck = await ctx.execCommand('tasklist | findstr /i "redis-server"');

                const processes = { backend: [], collector: [], redis: [], exportGateway: [], modbusSlave: [] };

                if (collectorCheck.stdout.trim()) {
                    ctx.log('INFO', 'Collector 프로세스 발견 (폴백)');
                    processes.collector.push({
                        pid: 1, name: 'collector.exe', commandLine: 'collector.exe',
                        startTime: new Date(), platform: 'win32'
                    });
                }

                if (redisCheck.stdout.trim()) {
                    ctx.log('INFO', 'Redis 프로세스 발견 (폴백)');
                    processes.redis.push({
                        pid: 2, name: 'redis-server.exe', commandLine: 'redis-server.exe',
                        startTime: new Date(), platform: 'win32'
                    });
                }

                // Backend는 현재 Node.js 프로세스 자체
                processes.backend.push({
                    pid: process.pid, name: 'node.exe',
                    commandLine: 'node.exe app.js', startTime: new Date(), platform: 'win32'
                });

                return processes;

            } catch (fallbackError) {
                ctx.log('ERROR', 'Windows 폴백 프로세스 감지도 실패', { error: fallbackError.message });
                return { backend: [], collector: [], redis: [] };
            }
        }
    }

    // =========================================================================
    // 🐧 Unix 프로세스 조회 (원본 L329~L393)
    // ps aux 파싱 → command 문자열 기준 분류
    // =========================================================================

    async getUnixProcesses() {
        const { ctx } = this;
        try {
            const { stdout } = await ctx.execCommand(ctx.commands.processFind);
            const lines = stdout.split('\n').filter(line => line.trim());

            const processes = { backend: [], collector: [], exportGateway: [], redis: [] };

            lines.forEach(line => {
                const parts = line.trim().split(/\s+/);
                if (parts.length < 11) return;

                const pid = parseInt(parts[1]);
                const command = parts.slice(10).join(' ');

                // defunct 프로세스 제외
                if (command.includes('<defunct>') || command.includes('[defunct]')) return;

                const processInfo = {
                    pid,
                    name: parts[10],
                    commandLine: command,
                    user: parts[0],
                    cpu: parts[2],
                    memory: parts[3],
                    platform: ctx.platform
                };

                if (command.includes('node') && command.includes('app.js')) {
                    processes.backend.push(processInfo);
                } else if (command.includes('collector')) {
                    // --id <id> 또는 -i <id> 인자 추출
                    const idMatch = command.match(/(?:--id|-i)\s+(\d+)/);
                    if (idMatch) {
                        processInfo.collectorId = parseInt(idMatch[1]);
                        ctx.log('DEBUG', `Collector ID 감지: ${processInfo.collectorId}`, { pid });
                    }
                    processes.collector.push(processInfo);
                } else if (command.includes('export-gateway')) {
                    const idMatch = command.match(/(?:--id|-i)\s+(\d+)/);
                    if (idMatch) {
                        processInfo.gatewayId = parseInt(idMatch[1]);
                        ctx.log('DEBUG', `Export Gateway ID 감지: ${processInfo.gatewayId}`, { pid });
                    }
                    processes.exportGateway.push(processInfo);
                } else if (command.includes('modbus-slave') || command.includes('pulseone-modbus-slave')) {
                    const idMatch = command.match(/--device-id[=\s](\d+)/);
                    if (idMatch) {
                        processInfo.deviceId = parseInt(idMatch[1]);
                        ctx.log('DEBUG', `Modbus Slave ID 감지: ${processInfo.deviceId}`, { pid });
                    }
                    if (!processes.modbusSlave) processes.modbusSlave = [];
                    processes.modbusSlave.push(processInfo);
                } else if (command.includes('redis-server')) {
                    processes.redis.push(processInfo);
                }
            });

            return processes;
        } catch (error) {
            ctx.log('ERROR', 'Unix 프로세스 감지 실패', { error: error.message });
            return { backend: [], collector: [], exportGateway: [], redis: [] };
        }
    }

    // =========================================================================
    // 🐳 Docker 컨테이너 상태 조회 (내부용)
    // Docker Socket을 통해 컨테이너 목록 반환
    // DockerController와 별개로 ProcessMonitor 내에서 경량 사용
    // =========================================================================

    async _getDockerStatuses() {
        const http = require('http');
        return new Promise((resolve, reject) => {
            const options = {
                socketPath: '/var/run/docker.sock',
                path: '/containers/json?all=1',
                method: 'GET',
                headers: { 'Content-Type': 'application/json' }
            };
            const req = http.request(options, (res) => {
                let data = '';
                res.on('data', chunk => data += chunk);
                res.on('end', () => {
                    try { resolve(data ? JSON.parse(data) : []); }
                    catch (e) { resolve([]); }
                });
            });
            req.on('error', (err) => reject(new Error(`Docker Socket Error: ${err.message}`)));
            req.end();
        });
    }
}

module.exports = ProcessMonitor;
