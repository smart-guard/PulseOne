// backend/lib/services/crossPlatformManager.js - 통합 로그 매니저 연동 완성본
// Windows, Linux, macOS, AWS 모두 지원 + Redis 제어 기능 + 통합 로깅

const os = require('os');
const path = require('path');
const { spawn, exec } = require('child_process');
const fs = require('fs').promises;
const fsSync = require('fs');
const http = require('http'); // For Docker Socket Communication

// ConfigManager와 LogManager 연동
const config = require('../config/ConfigManager');
const logger = require('../utils/LogManager');

class CrossPlatformManager {
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

        this.log('INFO', 'CrossPlatformManager 초기화 완료', {
            platform: this.platform,
            architecture: this.architecture,
            development: this.isDevelopment,
            collectorPath: this.paths.collector,
            redisPath: this.paths.redis
        });
    }

    // ========================================
    // 🔍 환경 감지 (Windows는 항상 프로덕션)
    // ========================================

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

    // ========================================
    // 📁 플랫폼별 경로 설정 (ConfigManager 통합)
    // ========================================

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

    // ========================================
    // 🔧 플랫폼별 명령어 설정
    // ========================================

    initializeCommands() {
        if (this.isWindows) {
            return {
                processFind: 'tasklist /fo csv | findstr /i "collector redis-server node pulseone"',
                processKill: (pid) => `taskkill /PID ${pid} /F`,
                serviceList: 'sc query type=service state=all | findstr "PulseOne"',
                systemInfo: 'wmic OS get TotalVisibleMemorySize,FreePhysicalMemory /value',
                pathExists: (path) => `if exist "${path}" echo "EXISTS" else echo "NOT_EXISTS"`,
                redisCliShutdown: 'redis-cli.exe shutdown'
            };
        } else {
            return {
                processFind: 'ps aux | grep -E "(collector|export-gateway|redis-server|node.*app\\.js|pulseone)" | grep -v grep',
                processKill: (pid) => `kill -9 ${pid}`,
                serviceList: 'systemctl list-units --type=service | grep pulseone',
                systemInfo: 'free -h && df -h',
                pathExists: (path) => `[ -f "${path}" ] && echo "EXISTS" || echo "NOT_EXISTS"`,
                redisCliShutdown: 'redis-cli shutdown'
            };
        }
    }

    // ========================================
    // 🔍 프로세스 조회 (강력한 감지)
    // ========================================

    async getRunningProcesses() {
        try {
            const results = this.isWindows ?
                await this.getWindowsProcesses() :
                await this.getUnixProcesses();

            // Docker 환경이라면 추가적인 컨테이너 정보 수집
            if (this.isDocker || this.isDevelopment) {
                try {
                    const dockerInfo = await this._getDockerStatuses();
                    results.dockerContainers = dockerInfo;
                } catch (e) {
                    this.log('DEBUG', 'Docker API를 통한 상태 조회 실패 (무시 가능)', { error: e.message });
                }
            }

            return results;
        } catch (error) {
            this.log('ERROR', 'getRunningProcesses 실패', { error: error.message });
            return { backend: [], collector: [], exportGateway: [], redis: [], dockerContainers: [] };
        }
    }

    async getWindowsProcesses() {
        try {
            this.log('DEBUG', 'Windows 프로세스 감지 시작');

            const { stdout } = await this.execCommand('tasklist /fo csv');
            this.log('DEBUG', `tasklist 출력 길이: ${stdout.length} 문자`);

            const lines = stdout.split('\n').filter(line => line.trim());
            const processes = { backend: [], collector: [], exportGateway: [], redis: [] };

            lines.forEach((line, index) => {
                if (index === 0) return; // 헤더 스킵

                // CSV 파싱
                const csvParts = line.match(/(".*?"|[^",\s]+)(?=\s*,|\s*$)/g);
                if (!csvParts || csvParts.length < 5) return;

                const imageName = csvParts[0].replace(/"/g, '').trim();
                const pid = parseInt(csvParts[1].replace(/"/g, '').trim());
                const memUsage = csvParts[4].replace(/"/g, '').trim();

                if (isNaN(pid)) return;

                const processInfo = {
                    pid: pid,
                    name: imageName,
                    commandLine: imageName,
                    startTime: new Date(),
                    platform: 'windows',
                    memory: memUsage
                };

                // 프로세스 분류 - pkg로 컴파일된 실제 exe명 기준
                const lowerImageName = imageName.toLowerCase();

                if (lowerImageName.includes('pulseone-backend.exe') || lowerImageName.includes('node.exe')) {
                    processes.backend.push(processInfo);
                } else if (lowerImageName.includes('pulseone-collector.exe') || lowerImageName.includes('collector.exe')) {
                    this.log('INFO', `Collector 프로세스 발견: PID ${pid}`);
                    processes.collector.push(processInfo);
                } else if (lowerImageName.includes('redis-server.exe')) {
                    this.log('INFO', `Redis 프로세스 발견: PID ${pid}`);
                    processes.redis.push(processInfo);
                } else if (lowerImageName.includes('pulseone-export-gateway.exe') || lowerImageName.includes('export-gateway.exe')) {
                    this.log('INFO', `Export Gateway 프로세스 발견: PID ${pid}`);
                    processes.exportGateway.push(processInfo);
                }
            });

            this.log('INFO', '프로세스 감지 완료', {
                backend: processes.backend.length,
                collector: processes.collector.length,
                exportGateway: processes.exportGateway.length,
                redis: processes.redis.length
            });

            return processes;

        } catch (error) {
            this.log('ERROR', 'Windows 프로세스 감지 실패', { error: error.message });

            // 폴백 방식
            try {
                const collectorCheck = await this.execCommand('tasklist | findstr /i "collector pulseone-collector"');
                const redisCheck = await this.execCommand('tasklist | findstr /i "redis-server"');

                const processes = { backend: [], collector: [], redis: [], exportGateway: [] };

                if (collectorCheck.stdout.trim()) {
                    this.log('INFO', 'Collector 프로세스 발견 (폴백)');
                    processes.collector.push({
                        pid: 1,
                        name: 'collector.exe',
                        commandLine: 'collector.exe',
                        startTime: new Date(),
                        platform: 'windows'
                    });
                }

                if (redisCheck.stdout.trim()) {
                    this.log('INFO', 'Redis 프로세스 발견 (폴백)');
                    processes.redis.push({
                        pid: 2,
                        name: 'redis-server.exe',
                        commandLine: 'redis-server.exe',
                        startTime: new Date(),
                        platform: 'windows'
                    });
                }

                // Backend는 항상 실행중으로 가정
                processes.backend.push({
                    pid: process.pid,
                    name: 'node.exe',
                    commandLine: 'node.exe app.js',
                    startTime: new Date(),
                    platform: 'windows'
                });

                return processes;

            } catch (fallbackError) {
                this.log('ERROR', 'Windows 폴백 프로세스 감지도 실패', { error: fallbackError.message });
                return { backend: [], collector: [], redis: [] };
            }
        }
    }

    async getUnixProcesses() {
        try {
            const { stdout } = await this.execCommand(this.commands.processFind);
            const lines = stdout.split('\n').filter(line => line.trim());

            const processes = { backend: [], collector: [], exportGateway: [], redis: [] };

            lines.forEach(line => {
                const parts = line.trim().split(/\s+/);
                if (parts.length >= 11) {
                    const pid = parseInt(parts[1]);
                    const command = parts.slice(10).join(' ');

                    if (command.includes('<defunct>') || command.includes('[defunct]')) {
                        return;
                    }

                    const processInfo = {
                        pid: pid,
                        name: parts[10],
                        commandLine: command,
                        user: parts[0],
                        cpu: parts[2],
                        memory: parts[3],
                        platform: this.platform
                    };

                    if (command.includes('node') && command.includes('app.js')) {
                        processes.backend.push(processInfo);
                    } else if (command.includes('collector')) {
                        // ID 추출 시도 (--id <id> 또는 -i <id>)
                        const idMatch = command.match(/(?:--id|-i)\s+(\d+)/);
                        if (idMatch) {
                            processInfo.collectorId = parseInt(idMatch[1]);
                            this.log('DEBUG', `Collector ID 감지: ${processInfo.collectorId}`, { pid });
                        }
                        processes.collector.push(processInfo);
                    } else if (command.includes('export-gateway')) {
                        // ID 추출 시도 (--id <id> 또는 -i <id>)
                        const idMatch = command.match(/(?:--id|-i)\s+(\d+)/);
                        if (idMatch) {
                            processInfo.gatewayId = parseInt(idMatch[1]);
                            this.log('DEBUG', `Export Gateway ID 감지: ${processInfo.gatewayId}`, { pid });
                        }
                        processes.exportGateway.push(processInfo);
                    } else if (command.includes('modbus-slave') || command.includes('pulseone-modbus-slave')) {
                        const idMatch = command.match(/--device-id[=\s](\d+)/);
                        if (idMatch) {
                            processInfo.deviceId = parseInt(idMatch[1]);
                            this.log('DEBUG', `Modbus Slave ID 감지: ${processInfo.deviceId}`, { pid });
                        }
                        if (!processes.modbusSlave) processes.modbusSlave = [];
                        processes.modbusSlave.push(processInfo);
                    } else if (command.includes('redis-server')) {
                        processes.redis.push(processInfo);
                    }
                }
            });

            return processes;
        } catch (error) {
            this.log('ERROR', 'Unix 프로세스 감지 실패', { error: error.message });
            return { backend: [], collector: [], exportGateway: [], redis: [] };
        }
    }

    // ========================================
    // 📊 서비스 상태 조회
    // ========================================

    async getServicesForAPI() {
        this.log('INFO', '서비스 상태 조회 시작');

        const processes = await this.getRunningProcesses();

        // 파일 존재 확인
        const [collectorExists, redisExists] = await Promise.all([
            this.fileExists(this.paths.collector),
            this.fileExists(this.paths.redis)
        ]);

        this.log('DEBUG', '파일 존재 확인 완료', {
            collectorPath: this.paths.collector,
            collectorExists,
            redisPath: this.paths.redis,
            redisExists
        });

        const collectorServices = processes.collector.map((proc, index) => ({
            name: proc.collectorId !== undefined ? `collector-${proc.collectorId}` : `collector-${index}`,
            displayName: proc.collectorId !== undefined ? `Collector (ID: ${proc.collectorId})` : 'Data Collector',
            icon: 'fas fa-download',
            description: `C++ 데이터 수집 서비스 (${this.platform})`,
            controllable: true,
            status: 'running',
            pid: proc.pid,
            collectorId: proc.collectorId || null,
            platform: this.platform,
            executable: path.basename(this.paths.collector),
            uptime: this.calculateUptime(proc.startTime),
            memoryUsage: proc.memory || 'N/A',
            cpuUsage: proc.cpu || 'N/A',
            executablePath: this.paths.collector,
            exists: collectorExists
        }));

        // If no collectors are running locally, add a stopped one so Docker API logic can update its status if applicable
        if (collectorServices.length === 0) {
            collectorServices.push({
                name: 'collector',
                displayName: 'Data Collector',
                icon: 'fas fa-download',
                description: `C++ 데이터 수집 서비스 (${this.platform})`,
                controllable: true,
                status: 'stopped',
                pid: null,
                platform: this.platform,
                executable: path.basename(this.paths.collector),
                uptime: 'N/A',
                memoryUsage: 'N/A',
                cpuUsage: 'N/A',
                executablePath: this.paths.collector,
                exists: collectorExists
            });
        }

        const services = [
            {
                name: 'backend',
                displayName: 'Backend API',
                icon: 'fas fa-server',
                description: `REST API 서버 (${this.platform})`,
                controllable: false,
                status: processes.backend.length > 0 ? 'running' : 'stopped',
                pid: processes.backend[0]?.pid || null,
                platform: this.platform,
                executable: this.isWindows ? 'node.exe' : 'node',
                uptime: processes.backend[0] ? this.calculateUptime(processes.backend[0].startTime) : 'N/A',
                memoryUsage: processes.backend[0]?.memory || 'N/A',
                cpuUsage: processes.backend[0]?.cpu || 'N/A'
            },
            ...collectorServices,
            {
                name: 'redis',
                displayName: 'Redis Cache',
                icon: 'fas fa-database',
                description: `인메모리 캐시 서버 (${this.platform})`,
                controllable: true,
                status: processes.redis.length > 0 ? 'running' : 'stopped',
                pid: processes.redis[0]?.pid || null,
                platform: this.platform,
                executable: path.basename(this.paths.redis),
                uptime: processes.redis[0] ? this.calculateUptime(processes.redis[0].startTime) : 'N/A',
                memoryUsage: processes.redis[0]?.memory || 'N/A',
                cpuUsage: processes.redis[0]?.cpu || 'N/A',
                executablePath: this.paths.redis,
                exists: redisExists,
                port: config.getRedisConfig?.()?.port || 6379
            }
        ];

        // Export Gateway Services
        const exportGatewayExists = await this.fileExists(this.paths.exportGateway);
        const exportGatewayServices = processes.exportGateway.map((proc, index) => ({
            name: proc.gatewayId !== undefined ? `export-gateway-${proc.gatewayId}` : `export-gateway-${index}`,
            displayName: proc.gatewayId !== undefined ? `Export Gateway (ID: ${proc.gatewayId})` : 'Export Gateway',
            icon: 'fas fa-satellite-dish',
            description: `데이터 내보내기 서비스 (${this.platform})`,
            controllable: true,
            status: 'running',
            pid: proc.pid,
            gatewayId: proc.gatewayId || null,
            platform: this.platform,
            executable: path.basename(this.paths.exportGateway),
            uptime: this.calculateUptime(proc.startTime),
            memoryUsage: proc.memory || 'N/A',
            cpuUsage: proc.cpu || 'N/A',
            executablePath: this.paths.exportGateway,
            exists: exportGatewayExists
        }));

        if (exportGatewayServices.length === 0) {
            exportGatewayServices.push({
                name: 'export-gateway',
                displayName: 'Export Gateway',
                icon: 'fas fa-satellite-dish',
                description: `데이터 내보내기 서비스 (${this.platform})`,
                controllable: true,
                status: 'stopped',
                pid: null,
                platform: this.platform,
                executable: path.basename(this.paths.exportGateway),
                uptime: 'N/A',
                memoryUsage: 'N/A',
                cpuUsage: 'N/A',
                executablePath: this.paths.exportGateway,
                exists: exportGatewayExists
            });
        }

        services.push(...exportGatewayServices);

        // Modbus Slave Services
        const modbusSlaveExists = await this.fileExists(this.paths.modbusSlave || '');
        const modbusSlaveProcesses = processes.modbusSlave || [];
        const modbusSlaveServices = modbusSlaveProcesses.map((proc, index) => ({
            name: proc.deviceId !== undefined ? `modbus-slave-${proc.deviceId}` : `modbus-slave-${index}`,
            displayName: proc.deviceId !== undefined ? `Modbus Slave (ID: ${proc.deviceId})` : 'Modbus Slave',
            icon: 'fas fa-network-wired',
            description: `Modbus TCP Slave 서비스 (${this.platform})`,
            controllable: true,
            status: 'running',
            pid: proc.pid,
            deviceId: proc.deviceId || null,
            platform: this.platform,
            executable: path.basename(this.paths.modbusSlave || 'pulseone-modbus-slave'),
            uptime: this.calculateUptime(proc.startTime),
            memoryUsage: proc.memory || 'N/A',
            cpuUsage: proc.cpu || 'N/A',
            executablePath: this.paths.modbusSlave,
            exists: modbusSlaveExists
        }));

        if (modbusSlaveServices.length === 0) {
            modbusSlaveServices.push({
                name: 'modbus-slave',
                displayName: 'Modbus Slave',
                icon: 'fas fa-network-wired',
                description: `Modbus TCP Slave 서비스 (${this.platform})`,
                controllable: true,
                status: 'stopped',
                pid: null,
                platform: this.platform,
                executable: path.basename(this.paths.modbusSlave || 'pulseone-modbus-slave'),
                uptime: 'N/A',
                memoryUsage: 'N/A',
                cpuUsage: 'N/A',
                executablePath: this.paths.modbusSlave,
                exists: modbusSlaveExists
            });
        }
        services.push(...modbusSlaveServices);

        // Redis 및 가상 서비스 네트워크 헬스체크 (Docker 환경 대응)
        if (this.isDocker || this.isDevelopment) {
            const dockerInfo = processes.dockerContainers || [];

            for (const service of services) {
                if (service.status === 'stopped') {
                    const baseName = service.name.split('-')[0].toLowerCase();

                    // Docker 컨테이너 정보에서 해당 서비스 찾기
                    // 서비스 이름 매칭 (예: collector, redis, backend, export-gateway)
                    const container = dockerInfo.find(c => {
                        const names = c.Names.map(n => n.replace(/^\//, '').toLowerCase());
                        return names.some(name => {
                            if (baseName === 'export') {
                                return name.includes('export-gateway') || name.includes('win-gateway') || name.includes('gateway');
                            }
                            return name.includes(baseName);
                        });
                    });

                    if (container) {
                        const isRunning = container.State === 'running' || container.Status.toLowerCase().includes('up');
                        if (isRunning) {
                            service.status = 'running';
                            service.description += ' (Docker Managed)';

                            // Docker API에서 받은 추가 정보 매핑
                            if (!service.uptime || service.uptime === 'N/A') {
                                service.uptime = container.Status;
                            }

                            // 컨테이너 ID 저장
                            service.containerId = container.Id;
                        }
                    }
                }
            }
        }

        const summary = services.reduce((acc, service) => {
            acc.total++;
            if (service.status === 'running') acc.running++;
            else if (service.status === 'stopped') acc.stopped++;
            return acc;
        }, { total: 0, running: 0, stopped: 0, unknown: 0 });

        this.log('INFO', '서비스 상태 조회 완료', { summary });

        return {
            success: true,
            data: services,
            summary,
            platform: {
                type: this.platform,
                architecture: this.architecture,
                deployment: this.getDeploymentType(),
                development: this.isDevelopment
            },
            paths: this.paths,
            timestamp: new Date().toISOString()
        };
    }

    // ========================================
    // 🏥 헬스 체크
    // ========================================

    async performHealthCheck() {
        this.log('INFO', '헬스체크 시작');

        const health = {
            overall: 'healthy',
            platform: this.platform,
            architecture: this.architecture,
            development: this.isDevelopment,
            services: {},
            system: await this.getSystemInfo(),
            checks: {
                collector_executable: await this.fileExists(this.paths.collector),
                redis_executable: await this.fileExists(this.paths.redis),
                database_file: await this.fileExists(this.paths.sqlite),
                config_directory: await this.directoryExists(this.paths.config),
                logs_directory: await this.directoryExists(this.paths.logs),
                data_directory: await this.directoryExists(this.paths.data)
            },
            timestamp: new Date().toISOString()
        };

        try {
            const processes = await this.getRunningProcesses();

            health.services.backend = {
                running: processes.backend.length > 0,
                pid: processes.backend[0]?.pid || null,
                platform: this.platform,
                essential: true
            };

            health.services.collector = {
                running: processes.collector.length > 0,
                pid: processes.collector[0]?.pid || null,
                platform: this.platform,
                essential: false,
                available: health.checks.collector_executable
            };

            health.services.redis = {
                running: processes.redis.length > 0,
                pid: processes.redis[0]?.pid || null,
                platform: this.platform,
                essential: false,
                available: health.checks.redis_executable
            };

            // 전체 상태 결정
            if (!health.services.backend.running) {
                health.overall = 'critical';
            } else if (!health.services.collector.running && health.checks.collector_executable) {
                health.overall = 'degraded';
            } else if (!health.services.redis.running && health.checks.redis_executable) {
                health.overall = 'degraded';
            }

            this.log('INFO', '헬스체크 완료', {
                overall: health.overall,
                services: {
                    backend: health.services.backend.running,
                    collector: health.services.collector.running,
                    redis: health.services.redis.running
                }
            });

        } catch (error) {
            health.overall = 'error';
            health.error = error.message;
            this.log('ERROR', '헬스체크 실패', { error: error.message });
        }

        return health;
    }

    // ========================================
    // 🎮 Collector 서비스 제어
    // ========================================

    async startCollector(collectorId = null) {
        this.log('INFO', 'Collector 시작 요청', {
            platform: this.platform,
            path: this.paths.collector,
            workingDir: process.cwd()
        });

        try {
            // 1. 실행파일 존재 확인
            const collectorExists = await this.fileExists(this.paths.collector);
            this.log('DEBUG', `실행파일 존재 확인: ${collectorExists}`);

            if (!collectorExists) {
                const errorMsg = `Collector 실행파일을 찾을 수 없음: ${this.paths.collector}`;
                this.log('ERROR', errorMsg);

                return {
                    success: false,
                    error: errorMsg,
                    suggestion: this.isDevelopment ? 'Build the Collector project first' : 'Install PulseOne Collector package',
                    buildCommand: this.isWindows ? 'cd collector && make' : 'cd collector && make debug',
                    platform: this.platform,
                    development: this.isDevelopment,
                    timestamp: new Date().toISOString()
                };
            }

            // 2. 이미 실행 중인지 확인
            const processes = await this.getRunningProcesses();
            const existingCollector = collectorId !== null
                ? processes.collector.find(p => p.collectorId === collectorId)
                : processes.collector[0];

            if (existingCollector) {
                const errorMsg = `Collector(ID: ${collectorId || 'default'})가 이미 실행 중입니다 (PID: ${existingCollector.pid})`;
                this.log('WARN', errorMsg);
                return {
                    success: false,
                    error: errorMsg,
                    pid: existingCollector.pid,
                    platform: this.platform
                };
            }

            // 3. Collector 시작
            this.log('INFO', `Collector 프로세스 시작 중... (ID: ${collectorId || 'default'})`);
            const startResult = await this.spawnCollector(collectorId);

            // 4. 시작 확인 (3초 대기)
            this.log('DEBUG', '프로세스 시작 대기 중... (3초)');
            await this.sleep(3000);

            const newProcesses = await this.getRunningProcesses();
            const newCollector = collectorId !== null
                ? newProcesses.collector.find(p => p.collectorId === collectorId)
                : newProcesses.collector[0];

            if (newCollector) {
                const successMsg = `Collector(ID: ${collectorId || 'default'})가 성공적으로 시작됨 (PID: ${newCollector.pid})`;
                this.log('INFO', successMsg, { pid: newCollector.pid, platform: this.platform });
                return {
                    success: true,
                    message: successMsg,
                    pid: newCollector.pid,
                    collectorId: collectorId,
                    platform: this.platform,
                    executable: this.paths.collector
                };
            } else {
                const errorMsg = 'Collector 프로세스 시작 실패';
                this.log('ERROR', errorMsg);
                return {
                    success: false,
                    error: errorMsg,
                    platform: this.platform,
                    suggestion: 'Check collector logs for startup errors'
                };
            }

        } catch (error) {
            this.log('ERROR', 'Collector 시작 중 예외 발생', { error: error.message });
            return {
                success: false,
                error: error.message,
                platform: this.platform
            };
        }
    }

    async spawnCollector(collectorId = null) {
        // 상대 경로를 절대 경로로 변환
        const absoluteCollectorPath = path.resolve(this.paths.collector);
        const args = [];
        if (collectorId !== null) {
            args.push('--id', collectorId.toString());
        }

        this.log('DEBUG', 'Collector 프로세스 스폰 시도', {
            originalPath: this.paths.collector,
            absolutePath: absoluteCollectorPath,
            args: args,
            exists: fsSync.existsSync(absoluteCollectorPath),
            workingDir: path.dirname(absoluteCollectorPath)
        });

        if (!fsSync.existsSync(absoluteCollectorPath)) {
            throw new Error(`Collector 실행파일이 존재하지 않음: ${absoluteCollectorPath}`);
        }

        if (this.isWindows) {
            return spawn(absoluteCollectorPath, args, {
                cwd: this.paths.root,
                detached: true,
                stdio: 'ignore',
                env: {
                    ...process.env,
                    TZ: 'Asia/Seoul', // Force KST
                    REDIS_HOST: config.get('REDIS_HOST') || 'localhost',
                    DATA_DIR: this.paths.root,
                    PULSEONE_DATA_DIR: this.paths.root
                }
            });
        } else {
            return spawn(absoluteCollectorPath, args, {
                cwd: this.paths.root,
                detached: true,
                stdio: 'ignore',
                env: {
                    ...process.env,
                    TZ: 'Asia/Seoul', // Force KST
                    REDIS_HOST: config.get('REDIS_HOST') || 'localhost',
                    LD_LIBRARY_PATH: '/usr/local/lib:/usr/lib',
                    PATH: process.env.PATH + ':/usr/local/bin',
                    DATA_DIR: this.paths.root,
                    PULSEONE_DATA_DIR: this.paths.root
                }
            });
        }
    }

    async stopCollector(collectorId = null) {
        this.log('INFO', 'Collector 중지 요청', { platform: this.platform, collectorId });

        try {
            const processes = await this.getRunningProcesses();
            const runningCollector = collectorId !== null
                ? processes.collector.find(p => p.collectorId === collectorId)
                : processes.collector[0];

            if (!runningCollector) {
                const errorMsg = `Collector(ID: ${collectorId || 'default'})가 실행되고 있지 않습니다`;
                this.log('WARN', errorMsg);
                return {
                    success: false,
                    error: errorMsg,
                    platform: this.platform
                };
            }

            this.log('INFO', `프로세스 종료 중... PID: ${runningCollector.pid}`);
            await this.execCommand(this.commands.processKill(runningCollector.pid));

            // 종료 확인 (3초 대기)
            await this.sleep(3000);

            const newProcesses = await this.getRunningProcesses();
            const stillRunning = newProcesses.collector.find(p => p.pid === runningCollector.pid);

            if (!stillRunning) {
                const successMsg = 'Collector가 성공적으로 중지됨';
                this.log('INFO', successMsg, { platform: this.platform });
                return {
                    success: true,
                    message: successMsg,
                    platform: this.platform
                };
            } else {
                const errorMsg = 'Collector 프로세스 중지 실패';
                this.log('ERROR', errorMsg);
                return {
                    success: false,
                    error: errorMsg,
                    platform: this.platform
                };
            }

        } catch (error) {
            this.log('ERROR', 'Collector 중지 중 예외 발생', { error: error.message });
            return {
                success: false,
                error: error.message,
                platform: this.platform
            };
        }
    }

    async restartCollector(collectorId = null) {
        this.log('INFO', 'Collector 재시작 요청', { platform: this.platform, collectorId });

        const stopResult = await this.stopCollector(collectorId);
        if (!stopResult.success && !stopResult.error.includes('not running') && !stopResult.error.includes('실행되고 있지 않습니다')) {
            return stopResult;
        }

        await this.sleep(2000);

        return await this.startCollector(collectorId);
    }

    // ========================================
    // 🐳 Docker Control (Socket)
    // ========================================

    async _dockerRequest(method, path, body = null) {
        return new Promise((resolve, reject) => {
            const options = {
                socketPath: '/var/run/docker.sock',
                path: path,
                method: method,
                headers: {
                    'Content-Type': 'application/json'
                }
            };

            const req = http.request(options, (res) => {
                let data = '';
                res.on('data', chunk => data += chunk);
                res.on('end', () => {
                    if (res.statusCode >= 200 && res.statusCode < 300) {
                        // 204 No Content handling
                        if (res.statusCode === 204) return resolve(null);
                        try {
                            const parsed = data ? JSON.parse(data) : null;
                            resolve(parsed);
                        } catch (e) {
                            resolve(data);
                        }
                    } else {
                        reject(new Error(`Docker API Error (${res.statusCode}): ${data}`));
                    }
                });
            });

            req.on('error', (err) => {
                reject(new Error(`Docker Socket Error: ${err.message}`));
            });

            if (body) {
                req.write(JSON.stringify(body));
            }
            req.end();
        });
    }

    /**
     * Docker API를 통해 컨테이너 상태 정보를 조회합니다.
     */
    async _getDockerStatuses() {
        try {
            // 모든 컨테이너 목록 조회 (중지된 것 포함)
            const containers = await this._dockerRequest('GET', '/containers/json?all=1');
            return containers || [];
        } catch (error) {
            this.log('DEBUG', 'Docker 상태 조회 실패', { error: error.message });
            throw error;
        }
    }

    /**
     * Controls a Docker container by name pattern
     */
    async controlDockerContainer(serviceName, action, id = null) {
        try {
            // Find container by name
            // Name usually: {project}_{service}_{index}
            // Start simple: match "export-gateway" in name
            const containers = await this._dockerRequest('GET', '/containers/json?all=1');

            // Filter Logic:
            // 1. Must contain serviceName (e.g., 'export-gateway')
            // 2. If ID provided, maybe match specific number? (Currently mapped to single service)
            // Compromise: Find thefirst container matching 'export-gateway' that belongs to our project
            const projectName = process.env.COMPOSE_PROJECT_NAME || 'pulseone';
            const targetContainer = containers.find(c => {
                const names = c.Names.map(n => n.replace('/', '')); // Remove leading slash
                return names.some(n => n.includes(serviceName) && n.includes(projectName));
            });

            if (!targetContainer) {
                throw new Error(`Docker container for service '${serviceName}' not found`);
            }

            const containerId = targetContainer.Id;
            const containerState = targetContainer.State;

            this.log('INFO', `Docker Container Control: ${action} ${serviceName} (${containerId})`);

            if (action === 'start') {
                if (containerState === 'running') return { success: false, error: 'Container already running' };
                await this._dockerRequest('POST', `/containers/${containerId}/start`);
                return { success: true, message: 'Container started via Docker API' };
            }
            else if (action === 'stop') {
                if (containerState !== 'running') return { success: false, error: 'Container not running' };
                await this._dockerRequest('POST', `/containers/${containerId}/stop`);
                return { success: true, message: 'Container stopped via Docker API' };
            }
            else if (action === 'restart') {
                await this._dockerRequest('POST', `/containers/${containerId}/restart`);
                return { success: true, message: 'Container restarted via Docker API' };
            }

        } catch (error) {
            this.log('ERROR', `Docker Control Failed: ${error.message}`);
            return { success: false, error: error.message, platform: 'docker' };
        }
    }

    // ========================================
    // 📤 Export Gateway 서비스 제어
    // ========================================

    async startExportGateway(gatewayId = null) {
        this.log('INFO', 'Export Gateway 시작 요청', {
            platform: this.platform,
            path: this.paths.exportGateway,
            gatewayId
        });

        // [RESTORED] Docker Environment Handling
        // We MUST NOT spawn local processes in the backend container if a dedicated
        // export-gateway container is available.
        if (this.isDocker) {
            this.log('INFO', 'Docker 환경 감지: 전용 컨테이너에 제어 위임');
            // Note: In Docker, the dedicated container's launcher will auto-detect the new gateway.
            // We return success here to indicate the command was 'accepted' by the system.
            return {
                success: true,
                message: 'Docker 환경: 전용 컨테이너에서 게이트웨이가 자동 시작됩니다.',
                platform: 'docker'
            };
        }

        try {
            const gatewayExists = await this.fileExists(this.paths.exportGateway);
            if (!gatewayExists) {
                return {
                    success: false,
                    error: `Export Gateway 실행파일을 찾을 수 없음: ${this.paths.exportGateway}`,
                    platform: this.platform
                };
            }

            const processes = await this.getRunningProcesses();
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
            await this.sleep(3000);

            const newProcesses = await this.getRunningProcesses();
            const newGateway = gatewayId !== null
                ? newProcesses.exportGateway.find(p => p.gatewayId === gatewayId)
                : newProcesses.exportGateway[0];

            if (newGateway) {
                return {
                    success: true,
                    message: `Export Gateway(ID: ${gatewayId || 'default'}) 시작됨`,
                    pid: newGateway.pid,
                    gatewayId: gatewayId,
                    platform: this.platform
                };
            } else {
                return { success: false, error: 'Export Gateway 시작 실패', platform: this.platform };
            }
        } catch (error) {
            this.log('ERROR', 'Export Gateway 시작 예외', { error: error.message });
            return { success: false, error: error.message, platform: this.platform };
        }
    }

    async spawnExportGateway(gatewayId = null) {
        const absolutePath = path.resolve(this.paths.exportGateway);
        const args = [];
        if (gatewayId !== null) {
            args.push('--id', gatewayId.toString());
        }

        this.log('DEBUG', 'Export Gateway spawn', { absolutePath, args });

        if (this.isWindows) {
            return spawn(absolutePath, args, {
                cwd: this.paths.root,
                detached: true,
                stdio: 'ignore',
                env: {
                    ...process.env,
                    TZ: 'Asia/Seoul', // Force KST
                    REDIS_HOST: config.get('REDIS_HOST') || 'localhost',
                    DATA_DIR: this.paths.root,
                    PULSEONE_DATA_DIR: this.paths.root
                }
            });
        } else {
            return spawn(absolutePath, args, {
                cwd: this.paths.root,
                detached: true,
                stdio: 'ignore',
                env: {
                    ...process.env,
                    TZ: 'Asia/Seoul', // Force KST
                    REDIS_HOST: config.get('REDIS_HOST') || 'localhost',
                    LD_LIBRARY_PATH: '/usr/local/lib:/usr/lib',
                    DATA_DIR: this.paths.root,
                    PULSEONE_DATA_DIR: this.paths.root
                }
            });
        }
    }

    async stopExportGateway(gatewayId = null) {
        this.log('INFO', 'Export Gateway 중지 요청', { gatewayId });

        // [MODIFIED] Docker Environment Handling Removed
        // if (this.isDocker) {
        //    this.log('INFO', 'Docker 환경 감지: Export Gateway 컨테이너 중지 시도');
        //    return await this.controlDockerContainer('export-gateway', 'stop', gatewayId);
        // }

        try {
            const processes = await this.getRunningProcesses();
            const runningGateway = gatewayId !== null
                ? processes.exportGateway.find(p => p.gatewayId === gatewayId)
                : processes.exportGateway[0];

            if (!runningGateway) {
                return { success: false, error: '실행 중인 게이트웨이가 없습니다' };
            }

            await this.execCommand(this.commands.processKill(runningGateway.pid));
            await this.sleep(2000);

            return { success: true, message: 'Export Gateway 중지됨' };
        } catch (error) {
            return { success: false, error: error.message };
        }
    }

    async restartExportGateway(gatewayId = null) {
        this.log('INFO', 'Export Gateway 재시작 요청', { gatewayId });

        // [MODIFIED] Docker Environment Handling Removed
        // if (this.isDocker) {
        //    return await this.controlDockerContainer('export-gateway', 'restart', gatewayId);
        // }

        const stopResult = await this.stopExportGateway(gatewayId);
        if (!stopResult.success && stopResult.error !== '실행 중인 게이트웨이가 없습니다') {
            return stopResult;
        }

        await this.sleep(1000);
        return await this.startExportGateway(gatewayId);
    }

    // ========================================
    // 🔌 Modbus Slave 서비스 제어
    // ========================================

    async startModbusSlave(deviceId = null) {
        this.log('INFO', 'Modbus Slave 시작 요청', { platform: this.platform, deviceId });

        // Docker: Supervisor가 DB 변경 감지하여 자동 spawn (최대 60초 대기)
        if (this.isDocker) {
            return {
                success: true,
                message: 'Docker 환경: Modbus Slave Supervisor가 자동으로 디바이스를 시작합니다 (최대 60초 내).',
                platform: 'docker'
            };
        }

        try {
            const slaveExists = await this.fileExists(this.paths.modbusSlave || '');
            if (!slaveExists) {
                return {
                    success: false,
                    error: `Modbus Slave 실행파일을 찾을 수 없음: ${this.paths.modbusSlave}`,
                    platform: this.platform
                };
            }

            const processes = await this.getRunningProcesses();
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
            await this.sleep(3000);

            const newProcesses = await this.getRunningProcesses();
            const newSlave = deviceId !== null
                ? (newProcesses.modbusSlave || []).find(p => p.deviceId === deviceId)
                : (newProcesses.modbusSlave || [])[0];

            if (newSlave) {
                return {
                    success: true,
                    message: `Modbus Slave(device-id: ${deviceId || 'supervisor'}) 시작됨`,
                    pid: newSlave.pid,
                    deviceId,
                    platform: this.platform
                };
            } else {
                return { success: false, error: 'Modbus Slave 시작 실패', platform: this.platform };
            }
        } catch (error) {
            return { success: false, error: error.message, platform: this.platform };
        }
    }

    async spawnModbusSlave(deviceId = null) {
        const absolutePath = path.resolve(this.paths.modbusSlave || '');
        const args = deviceId !== null ? ['--device-id', deviceId.toString()] : [];

        this.log('DEBUG', 'Modbus Slave spawn', { absolutePath, args });

        return spawn(absolutePath, args, {
            cwd: this.paths.root,
            detached: true,
            stdio: 'ignore',
            env: {
                ...process.env,
                TZ: 'Asia/Seoul',
                REDIS_PRIMARY_HOST: config.get('REDIS_PRIMARY_HOST') || config.get('REDIS_HOST') || 'localhost',
                SQLITE_PATH: this.paths.sqlite,
                LD_LIBRARY_PATH: '/usr/local/lib:/usr/lib'
            }
        });
    }

    async stopModbusSlave(deviceId = null) {
        this.log('INFO', 'Modbus Slave 중지 요청', { deviceId });

        // Docker: enabled=0으로 변경하면 Supervisor가 자동 kill
        if (this.isDocker) {
            return {
                success: true,
                message: 'Docker 환경: modbus_slave_devices.enabled=0 으로 설정하면 Supervisor가 자동 중지합니다.',
                platform: 'docker'
            };
        }

        try {
            const processes = await this.getRunningProcesses();
            const running = deviceId !== null
                ? (processes.modbusSlave || []).find(p => p.deviceId === deviceId)
                : (processes.modbusSlave || [])[0];

            if (!running) {
                return { success: false, error: '실행 중인 Modbus Slave가 없습니다' };
            }

            await this.execCommand(this.commands.processKill(running.pid));
            await this.sleep(2000);
            return { success: true, message: `Modbus Slave(PID: ${running.pid}) 중지됨` };
        } catch (error) {
            return { success: false, error: error.message };
        }
    }

    async restartModbusSlave(deviceId = null) {
        this.log('INFO', 'Modbus Slave 재시작 요청', { deviceId });

        if (this.isDocker) {
            return await this.controlDockerContainer('modbus-slave', 'restart', deviceId);
        }

        const stopResult = await this.stopModbusSlave(deviceId);
        if (!stopResult.success && stopResult.error !== '실행 중인 Modbus Slave가 없습니다') {
            return stopResult;
        }
        await this.sleep(1000);
        return await this.startModbusSlave(deviceId);
    }

    // ========================================
    // 🔴 Redis 서비스 제어
    // ========================================

    async startRedis() {
        this.log('INFO', 'Redis 시작 요청', { platform: this.platform });

        try {
            const redisPath = this.paths.redis;
            const redisExists = await this.fileExists(redisPath);

            if (!redisExists) {
                const errorMsg = `Redis를 찾을 수 없음: ${redisPath}`;
                this.log('ERROR', errorMsg);
                return {
                    success: false,
                    error: errorMsg,
                    suggestion: this.isWindows ?
                        'Download Redis for Windows from GitHub' :
                        'Install Redis using package manager (apt/yum/brew)',
                    platform: this.platform
                };
            }

            const processes = await this.getRunningProcesses();
            if (processes.redis.length > 0) {
                const errorMsg = `Redis가 이미 실행 중입니다 (PID: ${processes.redis[0].pid})`;
                this.log('WARN', errorMsg);
                return {
                    success: false,
                    error: errorMsg,
                    platform: this.platform,
                    port: 6379
                };
            }

            let redisProcess;
            const port = config.getRedisConfig?.()?.port || 6379;

            this.log('INFO', 'Redis 프로세스 시작 중...', { port });

            if (this.isWindows) {
                redisProcess = spawn(redisPath, [
                    '--port', port.toString(),
                    '--maxmemory', '512mb',
                    '--maxmemory-policy', 'allkeys-lru'
                ], {
                    detached: true,
                    stdio: 'ignore'
                });
            } else {
                redisProcess = spawn(redisPath, [
                    '--port', port.toString(),
                    '--daemonize', 'yes',
                    '--maxmemory', '512mb',
                    '--maxmemory-policy', 'allkeys-lru'
                ], {
                    detached: true,
                    stdio: 'ignore'
                });
            }

            await this.sleep(2000);

            const newProcesses = await this.getRunningProcesses();
            const newRedis = newProcesses.redis[0];

            if (newRedis) {
                const successMsg = `Redis가 성공적으로 시작됨 (PID: ${newRedis.pid})`;
                this.log('INFO', successMsg, { pid: newRedis.pid, port });
                return {
                    success: true,
                    message: successMsg,
                    pid: newRedis.pid,
                    platform: this.platform,
                    port: port
                };
            } else {
                const errorMsg = 'Redis 프로세스 시작 실패';
                this.log('ERROR', errorMsg);
                return {
                    success: false,
                    error: errorMsg,
                    platform: this.platform,
                    suggestion: 'Check Redis logs or try manual start'
                };
            }

        } catch (error) {
            this.log('ERROR', 'Redis 시작 중 예외 발생', { error: error.message });
            return {
                success: false,
                error: error.message,
                platform: this.platform
            };
        }
    }

    async stopRedis() {
        this.log('INFO', 'Redis 중지 요청', { platform: this.platform });

        try {
            const processes = await this.getRunningProcesses();
            const runningRedis = processes.redis[0];

            if (!runningRedis) {
                const errorMsg = 'Redis가 실행되고 있지 않습니다';
                this.log('WARN', errorMsg);
                return {
                    success: false,
                    error: errorMsg,
                    platform: this.platform
                };
            }

            try {
                this.log('DEBUG', 'Redis CLI shutdown 시도');
                await this.execCommand(this.commands.redisCliShutdown);
                await this.sleep(2000);
            } catch (cliError) {
                this.log('WARN', 'Redis CLI shutdown 실패, 강제 종료 사용', { error: cliError.message });
                await this.execCommand(this.commands.processKill(runningRedis.pid));
            }

            await this.sleep(1000);
            const newProcesses = await this.getRunningProcesses();
            const stillRunning = newProcesses.redis.find(p => p.pid === runningRedis.pid);

            if (!stillRunning) {
                const successMsg = 'Redis가 성공적으로 중지됨';
                this.log('INFO', successMsg, { platform: this.platform });
                return {
                    success: true,
                    message: successMsg,
                    platform: this.platform
                };
            } else {
                this.log('DEBUG', 'Redis 강제 종료 시도');
                await this.execCommand(this.commands.processKill(runningRedis.pid));
                await this.sleep(1000);

                const successMsg = 'Redis가 강제로 중지됨';
                this.log('INFO', successMsg, { platform: this.platform });
                return {
                    success: true,
                    message: successMsg,
                    platform: this.platform
                };
            }

        } catch (error) {
            this.log('ERROR', 'Redis 중지 중 예외 발생', { error: error.message });
            return {
                success: false,
                error: error.message,
                platform: this.platform
            };
        }
    }

    async restartRedis() {
        this.log('INFO', 'Redis 재시작 요청', { platform: this.platform });

        const stopResult = await this.stopRedis();
        if (!stopResult.success && !stopResult.error.includes('not running') && !stopResult.error.includes('실행되고 있지 않습니다')) {
            return stopResult;
        }

        await this.sleep(2000);

        return await this.startRedis();
    }

    // ========================================
    // 🖥️ 시스템 정보
    // ========================================

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

    // ========================================
    // 🛠️ 유틸리티 메소드들
    // ========================================

    async execCommand(command) {
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

    async fileExists(filePath) {
        try {
            const stats = await fs.stat(filePath);
            return stats.isFile();
        } catch {
            return false;
        }
    }

    async directoryExists(dirPath) {
        try {
            const stats = await fs.stat(dirPath);
            return stats.isDirectory();
        } catch {
            return false;
        }
    }

    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    formatBytes(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    calculateUptime(startTime) {
        if (!startTime) return 'N/A';

        const uptimeMs = Date.now() - new Date(startTime).getTime();
        const seconds = Math.floor(uptimeMs / 1000);
        const minutes = Math.floor(seconds / 60);
        const hours = Math.floor(minutes / 60);
        const days = Math.floor(hours / 24);

        if (days > 0) {
            return `${days}d ${hours % 24}h ${minutes % 60}m`;
        } else if (hours > 0) {
            return `${hours}h ${minutes % 60}m`;
        } else if (minutes > 0) {
            return `${minutes}m ${seconds % 60}s`;
        } else {
            return `${seconds}s`;
        }
    }

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

    getPlatformInfo() {
        return {
            detected: this.platform,
            architecture: this.architecture,
            development: this.isDevelopment,
            features: {
                serviceManager: this.platform === 'win32' ? 'Windows Services' : 'systemd',
                processManager: this.platform === 'win32' ? 'Task Manager' : 'ps/htop',
                packageManager: this.platform === 'win32' ? 'MSI/NSIS' :
                    this.platform === 'linux' ? 'apt/yum/dnf' : 'Homebrew',
                autoStart: this.platform === 'win32' ? 'Windows Services' :
                    this.platform === 'linux' ? 'systemd units' : 'launchd'
            },
            paths: this.paths,
            deployment: {
                current: this.getDeploymentType(),
                recommended: this.platform === 'win32' ? 'Native Windows Package' :
                    this.platform === 'linux' ? 'Docker or Native Package' :
                        'Native Application Bundle',
                alternatives: ['Docker', 'Manual Installation', 'Cloud Deployment']
            }
        };
    }
}

// 싱글톤 인스턴스 생성
const crossPlatformManager = new CrossPlatformManager();

module.exports = crossPlatformManager;