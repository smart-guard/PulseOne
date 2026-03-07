// =============================================================================
// backend/lib/services/cross-platform/index.js
// CrossPlatformManager 파사드 — 기존 API 100% 호환
// 모든 기능을 개별 컨트롤러에 위임, 외부 코드 변경 없음
// =============================================================================

'use strict';

const PlatformContext = require('./PlatformContext');
const ProcessMonitor = require('./ProcessMonitor');
const ServiceStateManager = require('./ServiceStateManager');
const DockerController = require('./DockerController');
const CollectorController = require('./CollectorController');
const GatewayController = require('./GatewayController');
const ModbusSlaveController = require('./ModbusSlaveController');
const RedisController = require('./RedisController');

class CrossPlatformManager extends PlatformContext {
    constructor() {
        super(); // 환경감지, 경로, 명령어, 유틸리티 초기화

        // 컨트롤러 조합 (의존성 주입)
        this._processMonitor = new ProcessMonitor(this);
        this._docker = new DockerController(this);
        this._serviceState = new ServiceStateManager(this, this._processMonitor);
        this._collector = new CollectorController(this, this._processMonitor);
        this._gateway = new GatewayController(this, this._processMonitor, this._docker);
        this._modbusSlave = new ModbusSlaveController(this, this._processMonitor, this._docker);
        this._redis = new RedisController(this, this._processMonitor);
    }

    // ─── 프로세스 조회 ──────────────────────────────────────────────────────
    getRunningProcesses() { return this._processMonitor.getRunningProcesses(); }

    // ─── 서비스 상태 집계 / 헬스체크 ────────────────────────────────────────
    getServicesForAPI() { return this._serviceState.getServicesForAPI(); }
    performHealthCheck() { return this._serviceState.performHealthCheck(); }

    // ─── Docker 제어 ─────────────────────────────────────────────────────────
    controlDockerContainer(serviceName, action, id = null) {
        return this._docker.controlDockerContainer(serviceName, action, id);
    }

    // ─── Collector 제어 ──────────────────────────────────────────────────────
    startCollector(collectorId = null) { return this._collector.startCollector(collectorId); }
    spawnCollector(collectorId = null) { return this._collector.spawnCollector(collectorId); }
    stopCollector(collectorId = null) { return this._collector.stopCollector(collectorId); }
    restartCollector(collectorId = null) { return this._collector.restartCollector(collectorId); }

    // ─── Export Gateway 제어 ─────────────────────────────────────────────────
    startExportGateway(gatewayId = null) { return this._gateway.startExportGateway(gatewayId); }
    spawnExportGateway(gatewayId = null) { return this._gateway.spawnExportGateway(gatewayId); }
    stopExportGateway(gatewayId = null) { return this._gateway.stopExportGateway(gatewayId); }
    restartExportGateway(gatewayId = null) { return this._gateway.restartExportGateway(gatewayId); }

    // ─── Modbus Slave 제어 ───────────────────────────────────────────────────
    startModbusSlave(deviceId = null) { return this._modbusSlave.startModbusSlave(deviceId); }
    spawnModbusSlave(deviceId = null) { return this._modbusSlave.spawnModbusSlave(deviceId); }
    stopModbusSlave(deviceId = null) { return this._modbusSlave.stopModbusSlave(deviceId); }
    restartModbusSlave(deviceId = null) { return this._modbusSlave.restartModbusSlave(deviceId); }

    // ─── Redis 제어 ──────────────────────────────────────────────────────────
    startRedis() { return this._redis.startRedis(); }
    stopRedis() { return this._redis.stopRedis(); }
    restartRedis() { return this._redis.restartRedis(); }
}

// 싱글톤 — 기존 require('./crossPlatformManager') 호환
module.exports = new CrossPlatformManager();
