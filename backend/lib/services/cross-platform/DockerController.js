// =============================================================================
// backend/lib/services/cross-platform/DockerController.js
// Docker Unix Socket API: 컨테이너 조회/제어
// 원본: crossPlatformManager.js L926~L1033
// =============================================================================

'use strict';

const http = require('http');

class DockerController {
    /**
     * @param {import('./PlatformContext')} ctx
     */
    constructor(ctx) {
        this.ctx = ctx;
    }

    // =========================================================================
    // 🐳 Docker Socket HTTP 요청 (원본 L930~L969)
    // Unix Socket /var/run/docker.sock 통신
    // =========================================================================

    _dockerRequest(method, socketPath, body = null) {
        return new Promise((resolve, reject) => {
            const options = {
                socketPath: '/var/run/docker.sock',
                path: socketPath,
                method,
                headers: { 'Content-Type': 'application/json' }
            };

            const req = http.request(options, (res) => {
                let data = '';
                res.on('data', chunk => data += chunk);
                res.on('end', () => {
                    if (res.statusCode >= 200 && res.statusCode < 300) {
                        // 204 No Content
                        if (res.statusCode === 204) return resolve(null);
                        try {
                            resolve(data ? JSON.parse(data) : null);
                        } catch (e) {
                            resolve(data);
                        }
                    } else {
                        reject(new Error(`Docker API Error (${res.statusCode}): ${data}`));
                    }
                });
            });

            req.on('error', (err) => reject(new Error(`Docker Socket Error: ${err.message}`)));

            if (body) req.write(JSON.stringify(body));
            req.end();
        });
    }

    // =========================================================================
    // 📋 컨테이너 목록 조회 (원본 L971~L983)
    // 중지된 컨테이너 포함 (all=1)
    // =========================================================================

    async getDockerStatuses() {
        try {
            const containers = await this._dockerRequest('GET', '/containers/json?all=1');
            return containers || [];
        } catch (error) {
            this.ctx.log('DEBUG', 'Docker 상태 조회 실패', { error: error.message });
            throw error;
        }
    }

    // =========================================================================
    // 🎛️ 컨테이너 제어 (원본 L985~L1033)
    // serviceName으로 컨테이너 검색 후 start/stop/restart
    // COMPOSE_PROJECT_NAME 환경변수로 프로젝트 구분
    // =========================================================================

    async controlDockerContainer(serviceName, action, id = null) {
        try {
            const containers = await this._dockerRequest('GET', '/containers/json?all=1');

            const projectName = process.env.COMPOSE_PROJECT_NAME || 'pulseone';
            const targetContainer = containers.find(c => {
                const names = c.Names.map(n => n.replace('/', ''));
                return names.some(n => n.includes(serviceName) && n.includes(projectName));
            });

            if (!targetContainer) {
                throw new Error(`Docker container for service '${serviceName}' not found`);
            }

            const containerId = targetContainer.Id;
            const containerState = targetContainer.State;

            this.ctx.log('INFO', `Docker Container Control: ${action} ${serviceName} (${containerId})`);

            if (action === 'start') {
                if (containerState === 'running') return { success: false, error: 'Container already running' };
                await this._dockerRequest('POST', `/containers/${containerId}/start`);
                return { success: true, message: 'Container started via Docker API' };
            } else if (action === 'stop') {
                if (containerState !== 'running') return { success: false, error: 'Container not running' };
                await this._dockerRequest('POST', `/containers/${containerId}/stop`);
                return { success: true, message: 'Container stopped via Docker API' };
            } else if (action === 'restart') {
                await this._dockerRequest('POST', `/containers/${containerId}/restart`);
                return { success: true, message: 'Container restarted via Docker API' };
            }

        } catch (error) {
            this.ctx.log('ERROR', `Docker Control Failed: ${error.message}`);
            return { success: false, error: error.message, platform: 'docker' };
        }
    }
}

module.exports = DockerController;
