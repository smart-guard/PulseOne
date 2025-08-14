// backend/routes/services.js (크로스 플랫폼용)
// Docker 없이 네이티브 프로세스 직접 관리 (Windows/Linux/macOS 모두 지원)

const express = require('express');
const router = express.Router();
const CrossPlatformManager = require('../lib/services/crossPlatformManager');

// ========================================
// 📊 서비스 상태 조회 (Windows 프로세스 기반)
// ========================================

// 모든 서비스 상태 조회
router.get('/', async (req, res) => {
  try {
    const result = await CrossPlatformManager.getServicesForAPI();
    res.json(result);
  } catch (error) {
    console.error('Error fetching services:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to fetch service status',
      details: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// 특정 서비스 상세 상태 조회
router.get('/:serviceName', async (req, res) => {
  try {
    const { serviceName } = req.params;
    const allServices = await CrossPlatformManager.getServicesForAPI();
    const service = allServices.data.find(s => s.name === serviceName);
    
    if (!service) {
      return res.status(404).json({
        success: false,
        error: `Service '${serviceName}' not found`,
        availableServices: allServices.data.map(s => s.name),
        timestamp: new Date().toISOString()
      });
    }

    res.json({
      success: true,
      data: {
        ...service,
        // 📈 추가 상세 정보
        platform: 'Windows',
        management: 'Native Process',
        executablePath: service.executable,
        lastUpdated: new Date().toISOString()
      }
    });
  } catch (error) {
    console.error(`Error fetching service ${req.params.serviceName}:`, error);
    res.status(500).json({
      success: false,
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// ========================================
// 🎮 Collector 서비스 제어
// ========================================

// Collector 시작
router.post('/collector/start', async (req, res) => {
  try {
    console.log('🚀 Starting Collector service via Cross-Platform API...');
    const result = await CrossPlatformManager.startCollector();
    
    if (result.success) {
      res.json({
        success: true,
        message: `Collector service started successfully on ${result.platform}`,
        pid: result.pid,
        platform: result.platform,
        timestamp: new Date().toISOString()
      });
    } else {
      res.status(400).json({
        success: false,
        error: result.error,
        platform: result.platform,
        timestamp: new Date().toISOString()
      });
    }
  } catch (error) {
    console.error('Error starting Collector:', error);
    res.status(500).json({
      success: false,
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// Collector 중지
router.post('/collector/stop', async (req, res) => {
  try {
    console.log('🛑 Stopping Collector service via Cross-Platform API...');
    const result = await CrossPlatformManager.stopCollector();
    
    if (result.success) {
      res.json({
        success: true,
        message: `Collector service stopped successfully on ${result.platform}`,
        platform: result.platform,
        timestamp: new Date().toISOString()
      });
    } else {
      res.status(400).json({
        success: false,
        error: result.error,
        platform: result.platform,
        timestamp: new Date().toISOString()
      });
    }
  } catch (error) {
    console.error('Error stopping Collector:', error);
    res.status(500).json({
      success: false,
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// Collector 재시작
router.post('/collector/restart', async (req, res) => {
  try {
    console.log('🔄 Restarting Collector service via Cross-Platform API...');
    const result = await CrossPlatformManager.restartCollector();
    
    if (result.success) {
      res.json({
        success: true,
        message: `Collector service restarted successfully on ${result.platform}`,
        pid: result.pid,
        platform: result.platform,
        timestamp: new Date().toISOString()
      });
    } else {
      res.status(400).json({
        success: false,
        error: result.error,
        platform: result.platform,
        timestamp: new Date().toISOString()
      });
    }
  } catch (error) {
    console.error('Error restarting Collector:', error);
    res.status(500).json({
      success: false,
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// ========================================
// 🚫 필수 서비스 제어 차단
// ========================================

// Backend 제어 시도 시 에러 반환
router.post('/backend/:action', (req, res) => {
  res.status(403).json({
    success: false,
    error: 'Backend service is essential and cannot be controlled',
    message: 'Backend service runs as the main application process',
    suggestion: 'Use system tray icon or Windows Services manager instead',
    timestamp: new Date().toISOString()
  });
});

// Frontend 제어 시도 시 에러 반환
router.post('/frontend/:action', (req, res) => {
  res.status(403).json({
    success: false,
    error: 'Frontend is served by Backend and cannot be controlled separately',
    message: 'Frontend is integrated with the Backend service',
    suggestion: 'Access via web browser at http://localhost:3000',
    timestamp: new Date().toISOString()
  });
});

// ========================================
// 🔧 시스템 헬스 체크
// ========================================

// 전체 시스템 헬스 체크
router.get('/health/check', async (req, res) => {
  try {
    const health = await CrossPlatformManager.performHealthCheck();
    
    const statusCode = health.overall === 'healthy' ? 200 :
                      health.overall === 'degraded' ? 206 : 503;

    res.status(statusCode).json({
      success: true,
      health: health,
      deployment: {
        type: health.platform === 'win32' ? 'Windows Native' : 
              health.platform === 'linux' ? 'Linux Native' : 
              health.platform === 'darwin' ? 'macOS Native' : 'Cross Platform',
        containerized: false,
        platform: health.platform,
        architecture: process.arch,
        nodeVersion: process.version
      }
    });
  } catch (error) {
    console.error('Error performing health check:', error);
    res.status(500).json({
      success: false,
      error: error.message,
      health: {
        overall: 'error',
        timestamp: new Date().toISOString()
      }
    });
  }
});

// 시스템 정보 조회
router.get('/system/info', async (req, res) => {
  try {
    const os = require('os');
    const path = require('path');
    
    const systemInfo = {
      platform: {
        type: 'Windows Native Installation',
        os: os.platform(),
        release: os.release(),
        architecture: os.arch(),
        hostname: os.hostname()
      },
      runtime: {
        nodeVersion: process.version,
        pid: process.pid,
        uptime: `${Math.round(process.uptime() / 60)}분`,
        memoryUsage: {
          rss: `${Math.round(process.memoryUsage().rss / 1024 / 1024)}MB`,
          heapTotal: `${Math.round(process.memoryUsage().heapTotal / 1024 / 1024)}MB`,
          heapUsed: `${Math.round(process.memoryUsage().heapUsed / 1024 / 1024)}MB`,
          external: `${Math.round(process.memoryUsage().external / 1024 / 1024)}MB`
        }
      },
      system: {
        totalMemory: `${Math.round(os.totalmem() / 1024 / 1024 / 1024)}GB`,
        freeMemory: `${Math.round(os.freemem() / 1024 / 1024 / 1024)}GB`,
        cpuCores: os.cpus().length,
        loadAverage: os.loadavg(),
        systemUptime: `${Math.round(os.uptime() / 3600)}시간`
      },
      pulseone: {
        version: '2.1.0',
        installationPath: CrossPlatformManager.paths.root,
        configPath: CrossPlatformManager.paths.config,
        dataPath: CrossPlatformManager.paths.data,
        logsPath: CrossPlatformManager.paths.logs
      },
      deployment: {
        type: 'Cross Platform Native',
        containerized: false,
        distributed: false,
        scalable: false,
        supportedPlatforms: ['Windows', 'Linux', 'macOS', 'AWS', 'Docker'],
        notes: `Ideal for industrial edge computing and SCADA applications on ${os.platform()}`
      },
      timestamp: new Date().toISOString()
    };

    res.json({
      success: true,
      data: systemInfo
    });
  } catch (error) {
    console.error('Error getting system info:', error);
    res.status(500).json({
      success: false,
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// ========================================
// 🔍 프로세스 모니터링
// ========================================

// 실행 중인 PulseOne 프로세스 목록
router.get('/processes', async (req, res) => {
  try {
    const processMap = await CrossPlatformManager.getRunningProcesses();
    
    const processes = [
      ...processMap.backend.map(p => ({
        ...p,
        service: 'backend',
        type: 'Node.js Application'
      })),
      ...processMap.collector.map(p => ({
        ...p,
        service: 'collector',
        type: 'C++ Application'
      }))
    ];

    res.json({
      success: true,
      data: {
        processes: processes,
        summary: {
          total: processes.length,
          backend: processMap.backend.length,
          collector: processMap.collector.length
        },
        platform: CrossPlatformManager.platform,
        architecture: CrossPlatformManager.architecture,
        timestamp: new Date().toISOString()
      }
    });
  } catch (error) {
    console.error('Error getting processes:', error);
    res.status(500).json({
      success: false,
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// ========================================
// 🎯 플랫폼별 특화 기능들
// ========================================

// 플랫폼 감지 및 특화 기능 정보
router.get('/platform/info', (req, res) => {
  const platform = CrossPlatformManager.platform;
  
  const platformInfo = {
    detected: platform,
    architecture: CrossPlatformManager.architecture,
    features: {
      serviceManager: platform === 'win32' ? 'Windows Services' : 'systemd',
      processManager: platform === 'win32' ? 'Task Manager' : 'ps/htop',
      packageManager: platform === 'win32' ? 'MSI/NSIS' : 
                     platform === 'linux' ? 'apt/yum/dnf' : 'Homebrew',
      autoStart: platform === 'win32' ? 'Windows Services' : 
                platform === 'linux' ? 'systemd units' : 'launchd'
    },
    paths: CrossPlatformManager.paths,
    deployment: {
      recommended: platform === 'win32' ? 'Native Windows Package' :
                  platform === 'linux' ? 'Docker or Native Package' :
                  'Native Application Bundle',
      alternatives: ['Docker', 'Manual Installation', 'Cloud Deployment']
    }
  };

  res.json({
    success: true,
    data: platformInfo
  });
});

// 플랫폼별 서비스 상태 (더 상세한 정보)
router.get('/platform/services', async (req, res) => {
  try {
    const platform = CrossPlatformManager.platform;
    
    if (platform === 'win32') {
      // Windows 서비스 상태 확인
      const command = 'sc query type=service state=all | findstr "PulseOne"';
      
      try {
        const { stdout } = await CrossPlatformManager.execCommand(command);
        const services = stdout.split('\n').filter(line => line.trim());
        
        res.json({
          success: true,
          data: {
            platform: 'Windows',
            serviceManager: 'Windows Services',
            services: services,
            registered: services.length > 0,
            message: services.length > 0 ? 
              'PulseOne is registered as Windows Service' : 
              'PulseOne is running as standalone application'
          }
        });
      } catch (error) {
        res.json({
          success: true,
          data: {
            platform: 'Windows',
            serviceManager: 'Windows Services',
            services: [],
            registered: false,
            message: 'PulseOne is running as standalone application',
            note: 'This is normal for development or portable installations'
          }
        });
      }
    } else if (platform === 'linux') {
      // Linux systemd 서비스 확인
      try {
        const { stdout } = await CrossPlatformManager.execCommand('systemctl list-units | grep pulseone');
        const services = stdout.split('\n').filter(line => line.trim());
        
        res.json({
          success: true,
          data: {
            platform: 'Linux',
            serviceManager: 'systemd',
            services: services,
            registered: services.length > 0,
            message: services.length > 0 ? 
              'PulseOne is registered as systemd service' : 
              'PulseOne is running as standalone application'
          }
        });
      } catch (error) {
        res.json({
          success: true,
          data: {
            platform: 'Linux',
            serviceManager: 'systemd',
            services: [],
            registered: false,
            message: 'PulseOne is running as standalone application'
          }
        });
      }
    } else {
      // macOS or other platforms
      res.json({
        success: true,
        data: {
          platform: platform,
          serviceManager: platform === 'darwin' ? 'launchd' : 'manual',
          services: [],
          registered: false,
          message: 'Platform-specific service management not implemented'
        }
      });
    }
  } catch (error) {
    console.error('Error checking platform services:', error);
    res.status(500).json({
      success: false,
      error: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// 시스템 트레이/알림 상태 (플랫폼별)
router.get('/ui/status', (req, res) => {
  const platform = CrossPlatformManager.platform;
  
  const uiInfo = {
    platform: platform,
    available: true,
    type: platform === 'win32' ? 'System Tray' : 
          platform === 'linux' ? 'System Indicator' : 
          platform === 'darwin' ? 'Menu Bar' : 'Unknown',
    features: []
  };

  if (platform === 'win32') {
    uiInfo.features = [
      'Quick service control',
      'Status monitoring', 
      'Log viewer access',
      'Settings panel',
      'Windows notification integration'
    ];
  } else if (platform === 'linux') {
    uiInfo.features = [
      'System indicator icon',
      'Quick service control',
      'Status monitoring',
      'Desktop notifications'
    ];
  } else if (platform === 'darwin') {
    uiInfo.features = [
      'Menu bar icon',
      'Quick access menu',
      'macOS notification center integration'
    ];
  }

  res.json({
    success: true,
    data: {
      ...uiInfo,
      message: `Cross-platform UI integration for ${platform}`,
      webAccess: 'http://localhost:3000'
    }
  });
});

module.exports = router;