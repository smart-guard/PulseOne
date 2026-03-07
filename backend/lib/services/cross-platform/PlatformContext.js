// =============================================================================
// backend/lib/services/cross-platform/PlatformContext.js
// 공유 플랫폼 컨텍스트: 환경 감지, 경로 설정, 명령어 설정, 유틸리티
// 원본: crossPlatformManager.js L1~L192, L1493~L1645
// =============================================================================

'use strict';

const os = require('os');
const path = require('path');
const { exec } = require('child_process');
const fs = require('fs').promises;
const fsSync = require('fs');

const config = require('../../config/ConfigManager');
const logger = require('../../utils/LogManager');

class PlatformContext {
    constructor() {
        this.platform = os.platform();
        this.architecture = os.arch();
        this.isWindows = this.platform === 'win32';
        this.isLinux = this.platform === 'linux';
        this.isMac = this.platform === 'darwin';
        this.isDocker = process.env.DOCKER_CONTAINER === 'true';
        this.isDevelopment = this.detectDevelopmentEnvironment();

        // 플랫폼별 설정 초기화
        this.paths = this.initializePaths();
        this.commands = this.initializeCommands();

        // 통합 로그 매니저 사용
        this.log = (level, message, metadata) => {
            logger.crossplatform(level, message, metadata);
        };

        this.log('INFO', 'PlatformContext 초기화 완료', {
            platform: this.platform,
            architecture: this.architecture,
            development: this.isDevelopment,
            collectorPath: this.paths.collector,
            redisPath: this.paths.redis
        });
    }

    // =========================================================================
    // 🔍 환경 감지 (원본 L47~L68)
    // Windows는 항상 프로덕션으로 처리
    // =========================================================================

    detectDevelopmentEnvironment() {
        // Windows는 항상 프로덕션으로 처리
        if (this.platform === 'win32') {
            return false;
        }

        const indicators = [
            process.env.NODE_ENV === 'development',
            process.env.ENV_STAGE === 'dev',
            process.cwd().includes('/app'),
            process.env.DOCKER_CONTAINER === 'true'
        ];

        try {
            fsSync.accessSync('/.dockerenv');
            indicators.push(true);
        } catch (e) {
            // Docker 환경 아님
        }

        return indicators.some(Boolean);
    }

    // =========================================================================
    // 📁 플랫폼별 경로 설정 (원본 L74~L166)
    // ConfigManager 통합: COLLECTOR_EXECUTABLE_PATH, REDIS_EXECUTABLE_PATH 우선
    // =========================================================================

    initializePaths() {
        const cwd = process.cwd();
        // Project Root 감지: backend 폴더 내부라면 부모 폴더가 루트
        const projectRoot = (cwd.endsWith('backend') || cwd.endsWith('backend/'))
            ? path.resolve(cwd, '..')
            : cwd;

        // ConfigManager에서 커스텀 경로 확인
        const customCollectorPath = config.get('COLLECTOR_EXECUTABLE_PATH');
        const customRedisPath = config.get('REDIS_EXECUTABLE_PATH');

        const basePaths = {
            development: {
                win32: {
                    root: projectRoot,
                    collector: customCollectorPath || path.resolve(projectRoot, 'pulseone-collector.exe'),
                    exportGateway: path.resolve(projectRoot, 'pulseone-export-gateway.exe'),
                    modbusSlave: path.resolve(projectRoot, 'pulseone-modbus-slave.exe'),
                    redis: customRedisPath || path.resolve(projectRoot, 'redis-server.exe'),
                    config: path.join(projectRoot, 'config'),
                    data: path.join(projectRoot, 'data'),
                    logs: path.join(projectRoot, 'logs'),
                    sqlite: path.join(projectRoot, 'data', 'db', 'pulseone.db'),
                    separator: '\\'
                },
                linux: {
                    root: projectRoot,
                    collector: customCollectorPath || path.resolve(projectRoot, 'core', 'collector', 'bin', 'pulseone-collector'),
                    exportGateway: path.resolve(projectRoot, 'core', 'export-gateway', 'bin', 'export-gateway'),
                    modbusSlave: path.resolve(projectRoot, 'core', 'modbus-slave', 'bin', 'pulseone-modbus-slave'),
                    redis: customRedisPath || '/usr/bin/redis-server',
                    config: path.join(projectRoot, 'config'),
                    data: path.join(projectRoot, 'data'),
                    logs: path.join(projectRoot, 'data', 'logs'),
                    sqlite: path.join(projectRoot, 'data', 'db', 'pulseone.db'),
                    separator: '/'
                },
                darwin: {
                    root: projectRoot,
                    collector: customCollectorPath || path.join(projectRoot, 'core', 'collector', 'bin', 'pulseone-collector'),
                    exportGateway: path.join(projectRoot, 'core', 'export-gateway', 'bin', 'export-gateway'),
                    modbusSlave: path.join(projectRoot, 'core', 'modbus-slave', 'bin', 'pulseone-modbus-slave'),
                    redis: customRedisPath || '/usr/local/bin/redis-server',
                    config: path.join(projectRoot, 'config'),
                    data: path.join(projectRoot, 'data'),
                    logs: path.join(projectRoot, 'logs'),
                    sqlite: path.join(projectRoot, 'data', 'db', 'pulseone.db'),
                    separator: '/'
                }
            },
            production: {
                win32: {
                    root: projectRoot,
                    collector: customCollectorPath || path.resolve(projectRoot, 'pulseone-collector.exe'),
                    exportGateway: path.resolve(projectRoot, 'pulseone-export-gateway.exe'),
                    modbusSlave: path.resolve(projectRoot, 'pulseone-modbus-slave.exe'),
                    redis: customRedisPath || path.resolve(projectRoot, 'redis-server.exe'),
                    config: path.resolve(projectRoot, 'config'),
                    data: path.resolve(projectRoot, 'data'),
                    logs: path.resolve(projectRoot, 'logs'),
                    sqlite: path.resolve(projectRoot, 'data', 'db', 'pulseone.db'),
                    separator: '\\'
                },
                linux: {
                    root: projectRoot,
                    collector: customCollectorPath || path.resolve(projectRoot, 'core', 'collector', 'bin', 'pulseone-collector'),
                    exportGateway: path.resolve(projectRoot, 'core', 'export-gateway', 'bin', 'export-gateway'),
                    modbusSlave: path.resolve(projectRoot, 'core', 'modbus-slave', 'bin', 'pulseone-modbus-slave'),
                    redis: customRedisPath || '/usr/bin/redis-server',
                    config: path.join(projectRoot, 'config'),
                    data: path.join(projectRoot, 'data'),
                    logs: path.join(projectRoot, 'data', 'logs'),
                    sqlite: path.join(projectRoot, 'data', 'db', 'pulseone.db'),
                    separator: '/'
                },
                darwin: {
                    root: projectRoot,
                    collector: customCollectorPath || path.join(projectRoot, 'collector'),
                    exportGateway: path.join(projectRoot, 'export-gateway'),
                    modbusSlave: path.join(projectRoot, 'modbus-slave'),
                    redis: customRedisPath || '/usr/local/bin/redis-server',
                    config: path.join(projectRoot, 'config'),
                    data: path.join(projectRoot, 'data'),
                    logs: path.join(projectRoot, 'data', 'logs'),
                    sqlite: path.join(projectRoot, 'data', 'db', 'pulseone.db'),
                    separator: '/'
                }
            }
        };

        const environment = this.isDevelopment ? 'development' : 'production';
        return basePaths[environment][this.platform] || basePaths.development.linux;
    }

    // =========================================================================
    // 🔧 플랫폼별 명령어 설정 (원본 L172~L192)
    // win32 / unix 두 갈래
    // =========================================================================

    initializeCommands() {
        if (this.isWindows) {
            return {
                processFind: 'tasklist /fo csv | findstr /i "collector redis-server node pulseone"',
                processKill: (pid) => `taskkill /PID ${pid} /F`,
                serviceList: 'sc query type=service state=all | findstr "PulseOne"',
                systemInfo: 'wmic OS get TotalVisibleMemorySize,FreePhysicalMemory /value',
                pathExists: (p) => `if exist "${p}" echo "EXISTS" else echo "NOT_EXISTS"`,
                redisCliShutdown: 'redis-cli.exe shutdown'
            };
        } else {
            return {
                processFind: 'ps aux | grep -E "(collector|export-gateway|redis-server|node.*app\\.js|pulseone)" | grep -v grep',
                processKill: (pid) => `kill -9 ${pid}`,
                serviceList: 'systemctl list-units --type=service | grep pulseone',
                systemInfo: 'free -h && df -h',
                pathExists: (p) => `[ -f "${p}" ] && echo "EXISTS" || echo "NOT_EXISTS"`,
                redisCliShutdown: 'redis-cli shutdown'
            };
        }
    }

    // =========================================================================
    // 🖥️ 시스템 정보 (원본 L1497~L1537)
    // =========================================================================

    async getSystemInfo() {
        return {
            platform: {
                type: this.platform,
                architecture: this.architecture,
                hostname: os.hostname(),
                release: os.release(),
                version: os.version?.() || 'Unknown',
                deployment: this.getDeploymentType(),
                development: this.isDevelopment
            },
            runtime: {
                nodeVersion: process.version,
                pid: process.pid,
                uptime: `${Math.round(process.uptime() / 60)}분`,
                workingDirectory: process.cwd(),
                environment: process.env.NODE_ENV || 'development'
            },
            memory: {
                total: `${Math.round(os.totalmem() / 1024 / 1024 / 1024)}GB`,
                free: `${Math.round(os.freemem() / 1024 / 1024 / 1024)}GB`,
                used: `${Math.round((os.totalmem() - os.freemem()) / 1024 / 1024 / 1024)}GB`,
                processMemory: this.formatBytes(process.memoryUsage().rss)
            },
            system: {
                cpuCores: os.cpus().length,
                cpuModel: os.cpus()[0]?.model || 'Unknown',
                systemUptime: `${Math.round(os.uptime() / 3600)}시간`,
                loadAverage: os.loadavg()
            },
            pulseone: {
                installationPath: this.paths.root,
                configPath: this.paths.config,
                dataPath: this.paths.data,
                logsPath: this.paths.logs,
                sqlitePath: this.paths.sqlite,
                collectorPath: this.paths.collector,
                redisPath: this.paths.redis
            }
        };
    }

    // =========================================================================
    // 🛠️ 유틸리티 (원본 L1543~L1639)
    // =========================================================================

    /**
     * shell 명령어 실행 (Promise 래퍼)
     * 타임아웃 10초 고정
     */
    execCommand(command) {
        return new Promise((resolve, reject) => {
            exec(command, { encoding: 'utf8', timeout: 10000 }, (error, stdout, stderr) => {
                if (error) {
                    reject(error);
                } else {
                    resolve({ stdout, stderr });
                }
            });
        });
    }

    /** 파일 존재 여부 (비동기) */
    async fileExists(filePath) {
        try {
            const stats = await fs.stat(filePath);
            return stats.isFile();
        } catch {
            return false;
        }
    }

    /** 디렉토리 존재 여부 (비동기) */
    async directoryExists(dirPath) {
        try {
            const stats = await fs.stat(dirPath);
            return stats.isDirectory();
        } catch {
            return false;
        }
    }

    /** ms 단위 sleep */
    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    /** 바이트를 사람이 읽기 좋은 문자열로 변환 */
    formatBytes(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    /** 업타임 계산 (startTime → 사람이 읽는 문자열) */
    calculateUptime(startTime) {
        if (!startTime) return 'N/A';
        const uptimeMs = Date.now() - new Date(startTime).getTime();
        const seconds = Math.floor(uptimeMs / 1000);
        const minutes = Math.floor(seconds / 60);
        const hours = Math.floor(minutes / 60);
        const days = Math.floor(hours / 24);

        if (days > 0) return `${days}d ${hours % 24}h ${minutes % 60}m`;
        if (hours > 0) return `${hours}h ${minutes % 60}m`;
        if (minutes > 0) return `${minutes}m ${seconds % 60}s`;
        return `${seconds}s`;
    }

    /** 배포 유형 문자열 (UI/로그 표시용) */
    getDeploymentType() {
        if (this.isDevelopment) {
            return this.isWindows ? 'Windows Development' :
                this.isLinux ? 'Linux Development / Docker' :
                    this.isMac ? 'macOS Development' : 'Development';
        } else {
            return this.isWindows ? 'Windows Native Package' :
                this.isLinux ? 'Linux Package / Docker / AWS' :
                    this.isMac ? 'macOS Application' : 'Cross Platform';
        }
    }

    /** 플랫폼 상세 정보 (System 탭 표시용) */
    getPlatformInfo() {
        return {
            detected: this.platform,
            architecture: this.architecture,
            development: this.isDevelopment,
            features: {
                serviceManager: this.isWindows ? 'Windows Services' : 'systemd',
                processManager: this.isWindows ? 'Task Manager' : 'ps/htop',
                packageManager: this.isWindows ? 'MSI/NSIS' :
                    this.isLinux ? 'apt/yum/dnf' : 'Homebrew',
                autoStart: this.isWindows ? 'Windows Services' :
                    this.isLinux ? 'systemd units' : 'launchd'
            },
            paths: this.paths,
            deployment: {
                current: this.getDeploymentType(),
                recommended: this.isWindows ? 'Native Windows Package' :
                    this.isLinux ? 'Docker or Native Package' :
                        'Native Application Bundle',
                alternatives: ['Docker', 'Manual Installation', 'Cloud Deployment']
            }
        };
    }
}

module.exports = PlatformContext;
