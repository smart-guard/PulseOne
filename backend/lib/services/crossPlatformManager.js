// backend/lib/services/crossPlatformManager.js - 통합 로그 매니저 연동 완성본
// Windows, Linux, macOS, AWS 모두 지원 + Redis 제어 기능 + 통합 로깅

const os = require('os');
const path = require('path');
const { spawn, exec } = require('child_process');
const fs = require('fs').promises;
const fsSync = require('fs');

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
    // ConfigManager에서 커스텀 경로 확인
        const customCollectorPath = config.get('COLLECTOR_EXECUTABLE_PATH');
        const customRedisPath = config.get('REDIS_EXECUTABLE_PATH');
    
        const basePaths = {
            development: {
                win32: {
                    root: process.cwd(),
                    collector: customCollectorPath || path.resolve(process.cwd(), '..', 'collector.exe'),
                    redis: customRedisPath || path.resolve(process.cwd(), '..', 'redis-server.exe'),
                    config: path.join(process.cwd(), 'config'),
                    data: path.join(process.cwd(), 'data'),
                    logs: path.join(process.cwd(), 'logs'),
                    sqlite: path.join(process.cwd(), 'data', 'pulseone.db'),
                    separator: '\\'
                },
                linux: {
                    root: process.cwd(),
                    collector: customCollectorPath || path.join(process.cwd(), 'collector', 'bin', 'collector'),
                    redis: customRedisPath || '/usr/bin/redis-server',
                    config: path.join(process.cwd(), 'config'),
                    data: path.join(process.cwd(), 'data'),
                    logs: path.join(process.cwd(), 'logs'),
                    sqlite: path.join(process.cwd(), 'data', 'pulseone.db'),
                    separator: '/'
                },
                darwin: {
                    root: process.cwd(),
                    collector: customCollectorPath || path.join(process.cwd(), 'collector', 'bin', 'collector'),
                    redis: customRedisPath || '/usr/local/bin/redis-server',
                    config: path.join(process.cwd(), 'config'),
                    data: path.join(process.cwd(), 'data'),
                    logs: path.join(process.cwd(), 'logs'),
                    sqlite: path.join(process.cwd(), 'data', 'pulseone.db'),
                    separator: '/'
                }
            },
            production: {
                win32: {
                    root: process.cwd(),
                    // Windows 프로덕션에서 backend 폴더에서 실행되므로 상위 폴더 참조
                    collector: customCollectorPath || path.resolve(process.cwd(), '..', 'collector.exe'),
                    redis: customRedisPath || path.resolve(process.cwd(), '..', 'redis-server.exe'),
                    config: path.resolve(process.cwd(), '..', 'config'),
                    data: path.resolve(process.cwd(), '..', 'data'),
                    logs: path.resolve(process.cwd(), '..', 'logs'),
                    sqlite: path.resolve(process.cwd(), '..', 'data', 'pulseone.db'),
                    separator: '\\'
                },
                linux: {
                    root: process.cwd(),
                    collector: customCollectorPath || path.join(process.cwd(), 'collector'),
                    redis: customRedisPath || '/usr/bin/redis-server',
                    config: path.join(process.cwd(), 'config'),
                    data: path.join(process.cwd(), 'data'),
                    logs: path.join(process.cwd(), 'logs'),
                    sqlite: path.join(process.cwd(), 'data', 'pulseone.db'),
                    separator: '/'
                },
                darwin: {
                    root: process.cwd(),
                    collector: customCollectorPath || path.join(process.cwd(), 'collector'),
                    redis: customRedisPath || '/usr/local/bin/redis-server',
                    config: path.join(process.cwd(), 'config'),
                    data: path.join(process.cwd(), 'data'),
                    logs: path.join(process.cwd(), 'logs'),
                    sqlite: path.join(process.cwd(), 'data', 'pulseone.db'),
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
                processFind: 'tasklist /fo csv | findstr /i "collector.exe redis-server.exe node.exe"',
                processKill: (pid) => `taskkill /PID ${pid} /F`,
                serviceList: 'sc query type=service state=all | findstr "PulseOne"',
                systemInfo: 'wmic OS get TotalVisibleMemorySize,FreePhysicalMemory /value',
                pathExists: (path) => `if exist "${path}" echo "EXISTS" else echo "NOT_EXISTS"`,
                redisCliShutdown: 'redis-cli.exe shutdown'
            };
        } else {
            return {
                processFind: 'ps aux | grep -E "(collector|redis-server|node.*app\.js)" | grep -v grep',
                processKill: (pid) => `kill -TERM ${pid}`,
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
            if (this.isWindows) {
                return await this.getWindowsProcesses();
            } else {
                return await this.getUnixProcesses();
            }
        } catch (error) {
            this.log('ERROR', 'getRunningProcesses 실패', { error: error.message });
            return { backend: [], collector: [], redis: [] };
        }
    }

    async getWindowsProcesses() {
        try {
            this.log('DEBUG', 'Windows 프로세스 감지 시작');
      
            const { stdout } = await this.execCommand('tasklist /fo csv');
            this.log('DEBUG', `tasklist 출력 길이: ${stdout.length} 문자`);
      
            const lines = stdout.split('\n').filter(line => line.trim());
            const processes = { backend: [], collector: [], redis: [] };
      
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

                // 프로세스 분류
                const lowerImageName = imageName.toLowerCase();
        
                if (lowerImageName.includes('node.exe')) {
                    processes.backend.push(processInfo);
                } else if (lowerImageName.includes('collector.exe')) {
                    this.log('INFO', `Collector 프로세스 발견: PID ${pid}`);
                    processes.collector.push(processInfo);
                } else if (lowerImageName.includes('redis-server.exe')) {
                    this.log('INFO', `Redis 프로세스 발견: PID ${pid}`);
                    processes.redis.push(processInfo);
                }
            });

            this.log('INFO', '프로세스 감지 완료', {
                backend: processes.backend.length,
                collector: processes.collector.length,
                redis: processes.redis.length
            });
      
            return processes;
      
        } catch (error) {
            this.log('ERROR', 'Windows 프로세스 감지 실패', { error: error.message });
      
            // 폴백 방식
            try {
                const collectorCheck = await this.execCommand('tasklist | findstr collector.exe');
                const redisCheck = await this.execCommand('tasklist | findstr redis-server.exe');
        
                const processes = { backend: [], collector: [], redis: [] };
        
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
      
            const processes = { backend: [], collector: [], redis: [] };
      
            lines.forEach(line => {
                const parts = line.trim().split(/\s+/);
                if (parts.length >= 11) {
                    const pid = parseInt(parts[1]);
                    const command = parts.slice(10).join(' ');
          
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
                        processes.collector.push(processInfo);
                    } else if (command.includes('redis-server')) {
                        processes.redis.push(processInfo);
                    }
                }
            });

            return processes;
        } catch (error) {
            this.log('ERROR', 'Unix 프로세스 감지 실패', { error: error.message });
            return { backend: [], collector: [], redis: [] };
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
            {
                name: 'collector',
                displayName: 'Data Collector',
                icon: 'fas fa-download',
                description: `C++ 데이터 수집 서비스 (${this.platform})`,
                controllable: true,
                status: processes.collector.length > 0 ? 'running' : 'stopped',
                pid: processes.collector[0]?.pid || null,
                platform: this.platform,
                executable: path.basename(this.paths.collector),
                uptime: processes.collector[0] ? this.calculateUptime(processes.collector[0].startTime) : 'N/A',
                memoryUsage: processes.collector[0]?.memory || 'N/A',
                cpuUsage: processes.collector[0]?.cpu || 'N/A',
                executablePath: this.paths.collector,
                exists: collectorExists
            },
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

    async startCollector() {
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
            if (processes.collector.length > 0) {
                const errorMsg = `Collector가 이미 실행 중입니다 (PID: ${processes.collector[0].pid})`;
                this.log('WARN', errorMsg);
                return {
                    success: false,
                    error: errorMsg,
                    platform: this.platform
                };
            }

            // 3. Collector 시작
            this.log('INFO', 'Collector 프로세스 시작 중...');
            const startResult = await this.spawnCollector();
      
            // 4. 시작 확인 (3초 대기)
            this.log('DEBUG', '프로세스 시작 대기 중... (3초)');
            await this.sleep(3000);
      
            const newProcesses = await this.getRunningProcesses();
            const newCollector = newProcesses.collector[0];

            if (newCollector) {
                const successMsg = `Collector가 성공적으로 시작됨 (PID: ${newCollector.pid})`;
                this.log('INFO', successMsg, { pid: newCollector.pid, platform: this.platform });
                return {
                    success: true,
                    message: successMsg,
                    pid: newCollector.pid,
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

    async spawnCollector() {
    // 상대 경로를 절대 경로로 변환
        const absoluteCollectorPath = path.resolve(this.paths.collector);
    
        this.log('DEBUG', 'Collector 프로세스 스폰 시도', {
            originalPath: this.paths.collector,
            absolutePath: absoluteCollectorPath,
            exists: fsSync.existsSync(absoluteCollectorPath),
            workingDir: path.dirname(absoluteCollectorPath)
        });
    
        if (!fsSync.existsSync(absoluteCollectorPath)) {
            throw new Error(`Collector 실행파일이 존재하지 않음: ${absoluteCollectorPath}`);
        }
    
        if (this.isWindows) {
            return spawn(absoluteCollectorPath, [], {
                cwd: path.dirname(absoluteCollectorPath),
                detached: true,
                stdio: 'ignore'
            });
        } else {
            return spawn(absoluteCollectorPath, [], {
                cwd: process.cwd(),
                detached: true,
                stdio: 'ignore',
                env: {
                    ...process.env,
                    LD_LIBRARY_PATH: '/usr/local/lib:/usr/lib',
                    PATH: process.env.PATH + ':/usr/local/bin'
                }
            });
        }
    }

    async stopCollector() {
        this.log('INFO', 'Collector 중지 요청', { platform: this.platform });
    
        try {
            const processes = await this.getRunningProcesses();
            const runningCollector = processes.collector[0];

            if (!runningCollector) {
                const errorMsg = 'Collector가 실행되고 있지 않습니다';
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

    async restartCollector() {
        this.log('INFO', 'Collector 재시작 요청', { platform: this.platform });
    
        const stopResult = await this.stopCollector();
        if (!stopResult.success && !stopResult.error.includes('not running') && !stopResult.error.includes('실행되고 있지 않습니다')) {
            return stopResult;
        }

        await this.sleep(2000);
    
        return await this.startCollector();
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