// =============================================================================
// __tests__/crossPlatform/ModbusSlaveController.test.js
// ModbusSlave 시작/중지/재시작 + Docker 분기 테스트
// =============================================================================

'use strict';

jest.mock('../../lib/config/ConfigManager', () => ({
    get: jest.fn((key) => {
        if (key === 'REDIS_PRIMARY_HOST' || key === 'REDIS_HOST') return 'localhost';
        return null;
    })
}));
jest.mock('../../lib/utils/LogManager', () => ({ crossplatform: jest.fn() }));
jest.mock('os', () => ({
    platform: () => 'linux', arch: () => 'x64',
    hostname: () => 'test', release: () => '5.15.0',
    totalmem: () => 8e9, freemem: () => 4e9, uptime: () => 3600,
    cpus: () => [{ model: 'Intel' }], loadavg: () => [0.1, 0.2, 0.3]
}));

const PlatformContext = require('../../lib/services/cross-platform/PlatformContext');
const ModbusSlaveController = require('../../lib/services/cross-platform/ModbusSlaveController');

function makeContext(isDocker = false) {
    const ctx = new PlatformContext();
    ctx.log = jest.fn();
    ctx.sleep = jest.fn().mockResolvedValue();
    ctx.execCommand = jest.fn().mockResolvedValue({ stdout: '', stderr: '' });
    ctx.fileExists = jest.fn().mockResolvedValue(true);
    ctx.isWindows = false;
    ctx.isDocker = isDocker;
    ctx.paths = {
        modbusSlave: '/app/core/modbus-slave/bin/pulseone-modbus-slave',
        sqlite: '/app/data/db/pulseone.db',
        root: '/app'
    };
    return ctx;
}

function makeMonitor(modbusProcesses = []) {
    return {
        getRunningProcesses: jest.fn().mockResolvedValue({
            backend: [], collector: [],
            exportGateway: [], redis: [],
            modbusSlave: modbusProcesses
        })
    };
}

function makeDocker() {
    return {
        controlDockerContainer: jest.fn().mockResolvedValue({
            success: true, message: 'Container restarted'
        })
    };
}

// ─── Docker 환경 ─────────────────────────────────────────────────────────────
describe('ModbusSlaveController — Docker 환경', () => {
    test('startModbusSlave: Docker 환경에서 즉시 success:true', async () => {
        const ctx = makeContext(true);
        const ctrl = new ModbusSlaveController(ctx, makeMonitor(), makeDocker());

        const result = await ctrl.startModbusSlave();
        expect(result.success).toBe(true);
        expect(result.platform).toBe('docker');
        expect(result.message).toMatch(/Supervisor/);
    });

    test('stopModbusSlave: Docker 환경에서 enabled=0 안내 메시지', async () => {
        const ctx = makeContext(true);
        const ctrl = new ModbusSlaveController(ctx, makeMonitor(), makeDocker());

        const result = await ctrl.stopModbusSlave();
        expect(result.success).toBe(true);
        expect(result.message).toMatch(/enabled=0/);
    });

    test('restartModbusSlave: Docker 환경에서 controlDockerContainer 호출', async () => {
        const ctx = makeContext(true);
        const docker = makeDocker();
        const ctrl = new ModbusSlaveController(ctx, makeMonitor(), docker);

        const result = await ctrl.restartModbusSlave(42);
        expect(docker.controlDockerContainer).toHaveBeenCalledWith('modbus-slave', 'restart', 42);
        expect(result.success).toBe(true);
    });
});

// ─── 실행파일 없음 ────────────────────────────────────────────────────────────
describe('ModbusSlaveController — 실행파일 없음', () => {
    test('fileExists=false → success:false', async () => {
        const ctx = makeContext(false);
        ctx.fileExists = jest.fn().mockResolvedValue(false);
        const ctrl = new ModbusSlaveController(ctx, makeMonitor(), makeDocker());

        const result = await ctrl.startModbusSlave();
        expect(result.success).toBe(false);
        expect(result.error).toMatch(/찾을 수 없음/);
    });
});

// ─── 이미 실행 중 ─────────────────────────────────────────────────────────────
describe('ModbusSlaveController — 이미 실행 중', () => {
    test('deviceId 일치하는 프로세스 있으면 success:false', async () => {
        const ctx = makeContext(false);
        const existingProc = { pid: 111, deviceId: 42 };
        const monitor = makeMonitor([existingProc]);
        const ctrl = new ModbusSlaveController(ctx, monitor, makeDocker());

        const result = await ctrl.startModbusSlave(42);
        expect(result.success).toBe(false);
        expect(result.pid).toBe(111);
    });
});

// ─── 시작 성공 ────────────────────────────────────────────────────────────────
describe('ModbusSlaveController — 시작 성공', () => {
    test('spawn 후 프로세스 감지 → success:true + pid', async () => {
        const ctx = makeContext(false);
        const newProc = { pid: 222, deviceId: 1 };
        let callCount = 0;
        const monitor = {
            getRunningProcesses: jest.fn().mockImplementation(() => {
                callCount++;
                if (callCount === 1) return Promise.resolve({ modbusSlave: [] });
                return Promise.resolve({ modbusSlave: [newProc] });
            })
        };
        const ctrl = new ModbusSlaveController(ctx, monitor, makeDocker());
        ctrl.spawnModbusSlave = jest.fn().mockResolvedValue({});

        const result = await ctrl.startModbusSlave(1);
        expect(result.success).toBe(true);
        expect(result.pid).toBe(222);
    });
});

// ─── 중지 ─────────────────────────────────────────────────────────────────────
describe('ModbusSlaveController — stopModbusSlave (비 Docker)', () => {
    test('실행 중인 slave 없으면 success:false', async () => {
        const ctx = makeContext(false);
        const ctrl = new ModbusSlaveController(ctx, makeMonitor([]), makeDocker());

        const result = await ctrl.stopModbusSlave();
        expect(result.success).toBe(false);
        expect(result.error).toMatch(/없습니다/);
    });

    test('running slave 있으면 kill 호출 후 success:true', async () => {
        const ctx = makeContext(false);
        const ctrl = new ModbusSlaveController(ctx, makeMonitor([{ pid: 333, deviceId: 5 }]), makeDocker());

        const result = await ctrl.stopModbusSlave(5);
        expect(result.success).toBe(true);
        expect(ctx.execCommand).toHaveBeenCalledWith(expect.stringContaining('333'));
    });
});

// ─── 재시작 (비 Docker) ───────────────────────────────────────────────────────
describe('ModbusSlaveController — restartModbusSlave (비 Docker)', () => {
    test('stop → start 순서 호출', async () => {
        const ctx = makeContext(false);
        const ctrl = new ModbusSlaveController(ctx, makeMonitor([]), makeDocker());
        ctrl.stopModbusSlave = jest.fn().mockResolvedValue({
            success: false, error: '실행 중인 Modbus Slave가 없습니다'
        });
        ctrl.startModbusSlave = jest.fn().mockResolvedValue({ success: true, pid: 999 });

        const result = await ctrl.restartModbusSlave();
        expect(ctrl.startModbusSlave).toHaveBeenCalled();
        expect(result.success).toBe(true);
    });
});
