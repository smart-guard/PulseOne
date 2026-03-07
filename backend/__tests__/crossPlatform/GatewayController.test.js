// =============================================================================
// __tests__/crossPlatform/GatewayController.test.js
// Export Gateway 시작/중지/재시작 로직 + Docker 분기 테스트
// =============================================================================

'use strict';

jest.mock('../../lib/config/ConfigManager', () => ({
    get: jest.fn(() => 'localhost')
}));
jest.mock('../../lib/utils/LogManager', () => ({ crossplatform: jest.fn() }));
jest.mock('os', () => ({
    platform: () => 'linux', arch: () => 'x64',
    hostname: () => 'test', release: () => '5.15.0',
    totalmem: () => 8e9, freemem: () => 4e9, uptime: () => 3600,
    cpus: () => [{ model: 'Intel' }], loadavg: () => [0.1, 0.2, 0.3]
}));

const PlatformContext = require('../../lib/services/cross-platform/PlatformContext');
const GatewayController = require('../../lib/services/cross-platform/GatewayController');

function makeContext(isDocker = false) {
    const ctx = new PlatformContext();
    ctx.log = jest.fn();
    ctx.sleep = jest.fn().mockResolvedValue();
    ctx.execCommand = jest.fn().mockResolvedValue({ stdout: '', stderr: '' });
    ctx.fileExists = jest.fn().mockResolvedValue(true);
    ctx.isWindows = false;
    ctx.isDocker = isDocker;
    ctx.paths = {
        exportGateway: '/app/core/export-gateway/bin/export-gateway',
        root: '/app'
    };
    return ctx;
}

function makeMonitor(gatewayProcesses = []) {
    return {
        getRunningProcesses: jest.fn().mockResolvedValue({
            backend: [], collector: [],
            exportGateway: gatewayProcesses,
            redis: [], modbusSlave: []
        })
    };
}

function makeDocker() {
    return { controlDockerContainer: jest.fn().mockResolvedValue({ success: true }) };
}

// ─── Docker 환경 분기 ────────────────────────────────────────────────────────
describe('GatewayController — Docker 환경', () => {
    test('startExportGateway: Docker 환경에서는 즉시 success:true 반환', async () => {
        const ctx = makeContext(true); // isDocker=true
        const ctrl = new GatewayController(ctx, makeMonitor(), makeDocker());

        const result = await ctrl.startExportGateway();
        expect(result.success).toBe(true);
        expect(result.platform).toBe('docker');
        expect(result.message).toMatch(/Docker/);
    });
});

// ─── 실행파일 없음 처리 ────────────────────────────────────────────────────────
describe('GatewayController — 실행파일 없음', () => {
    test('fileExists=false → success:false', async () => {
        const ctx = makeContext(false);
        ctx.fileExists = jest.fn().mockResolvedValue(false);
        const ctrl = new GatewayController(ctx, makeMonitor(), makeDocker());

        const result = await ctrl.startExportGateway();
        expect(result.success).toBe(false);
        expect(result.error).toMatch(/찾을 수 없음/);
    });
});

// ─── 이미 실행 중 ─────────────────────────────────────────────────────────────
describe('GatewayController — 이미 실행 중', () => {
    test('프로세스 존재 시 success:false + 이미 실행 중 메시지', async () => {
        const ctx = makeContext(false);
        const existingProc = { pid: 777, gatewayId: undefined };
        const ctrl = new GatewayController(ctx, makeMonitor([existingProc]), makeDocker());

        const result = await ctrl.startExportGateway();
        expect(result.success).toBe(false);
        expect(result.pid).toBe(777);
    });
});

// ─── 시작 성공 ────────────────────────────────────────────────────────────────
describe('GatewayController — 시작 성공', () => {
    test('spawn 후 프로세스 감지 → success:true', async () => {
        const ctx = makeContext(false);
        const newProc = { pid: 888, gatewayId: undefined };
        let callCount = 0;
        const monitor = {
            getRunningProcesses: jest.fn().mockImplementation(() => {
                callCount++;
                if (callCount === 1) return Promise.resolve({ exportGateway: [], modbusSlave: [] });
                return Promise.resolve({ exportGateway: [newProc], modbusSlave: [] });
            })
        };
        const ctrl = new GatewayController(ctx, monitor, makeDocker());
        ctrl.spawnExportGateway = jest.fn().mockResolvedValue({});

        const result = await ctrl.startExportGateway();
        expect(result.success).toBe(true);
        expect(result.pid).toBe(888);
    });
});

// ─── 중지 ─────────────────────────────────────────────────────────────────────
describe('GatewayController — stopExportGateway', () => {
    test('실행 중인 게이트웨이 없으면 success:false', async () => {
        const ctx = makeContext(false);
        const ctrl = new GatewayController(ctx, makeMonitor([]), makeDocker());

        const result = await ctrl.stopExportGateway();
        expect(result.success).toBe(false);
    });

    test('프로세스 있으면 kill 명령 실행 후 success:true', async () => {
        const ctx = makeContext(false);
        const ctrl = new GatewayController(ctx, makeMonitor([{ pid: 321, gatewayId: undefined }]), makeDocker());

        const result = await ctrl.stopExportGateway();
        expect(result.success).toBe(true);
        expect(ctx.execCommand).toHaveBeenCalled();
    });
});

// ─── 재시작 ────────────────────────────────────────────────────────────────────
describe('GatewayController — restartExportGateway', () => {
    test('stop 성공 후 start 호출', async () => {
        const ctx = makeContext(false);
        const ctrl = new GatewayController(ctx, makeMonitor([]), makeDocker());
        ctrl.stopExportGateway = jest.fn().mockResolvedValue({ success: true });
        ctrl.startExportGateway = jest.fn().mockResolvedValue({ success: true, pid: 444 });

        const result = await ctrl.restartExportGateway();
        expect(ctrl.startExportGateway).toHaveBeenCalled();
        expect(result.success).toBe(true);
    });

    test('"실행 중인 게이트웨이가 없습니다" 오류여도 start 호출', async () => {
        const ctx = makeContext(false);
        const ctrl = new GatewayController(ctx, makeMonitor([]), makeDocker());
        ctrl.stopExportGateway = jest.fn().mockResolvedValue({
            success: false, error: '실행 중인 게이트웨이가 없습니다'
        });
        ctrl.startExportGateway = jest.fn().mockResolvedValue({ success: true, pid: 555 });

        await ctrl.restartExportGateway();
        expect(ctrl.startExportGateway).toHaveBeenCalled();
    });
});
