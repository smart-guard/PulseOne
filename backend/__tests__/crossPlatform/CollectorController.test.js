// =============================================================================
// __tests__/crossPlatform/CollectorController.test.js
// Collector 시작/중지/재시작 로직 테스트
// =============================================================================

'use strict';

jest.mock('../../lib/config/ConfigManager', () => ({
    get: jest.fn((key) => key === 'REDIS_HOST' ? 'localhost' : null)
}));
jest.mock('../../lib/utils/LogManager', () => ({ crossplatform: jest.fn() }));
jest.mock('os', () => ({
    platform: () => 'linux', arch: () => 'x64',
    hostname: () => 'test', release: () => '5.15.0',
    totalmem: () => 8e9, freemem: () => 4e9, uptime: () => 3600,
    cpus: () => [{ model: 'Intel' }], loadavg: () => [0.1, 0.2, 0.3]
}));

const path = require('path');
const PlatformContext = require('../../lib/services/cross-platform/PlatformContext');
const CollectorController = require('../../lib/services/cross-platform/CollectorController');

// ─── 헬퍼: ctx 와 processMonitor 목 생성 ─────────────────────────────────────
function makeContext(overrides = {}) {
    const ctx = new PlatformContext();
    ctx.log = jest.fn();
    ctx.sleep = jest.fn().mockResolvedValue();
    ctx.execCommand = jest.fn().mockResolvedValue({ stdout: '', stderr: '' });
    ctx.fileExists = jest.fn().mockResolvedValue(true);
    ctx.isWindows = false;
    ctx.isDocker = false;
    ctx.paths = {
        collector: '/app/core/collector/bin/pulseone-collector',
        root: '/app'
    };
    return { ...ctx, ...overrides };
}

function makeMonitor(collectorProcesses = []) {
    return {
        getRunningProcesses: jest.fn().mockResolvedValue({
            backend: [], collector: collectorProcesses,
            exportGateway: [], redis: [], modbusSlave: []
        })
    };
}

// ─── startCollector ───────────────────────────────────────────────────────────
describe('CollectorController — startCollector', () => {
    test('실행파일 없으면 success:false + suggestion 반환', async () => {
        const ctx = makeContext();
        ctx.fileExists = jest.fn().mockResolvedValue(false);
        const monitor = makeMonitor();
        const ctrl = new CollectorController(ctx, monitor);

        const result = await ctrl.startCollector();
        expect(result.success).toBe(false);
        expect(result.error).toMatch(/찾을 수 없음/);
        expect(result).toHaveProperty('suggestion');
    });

    test('이미 실행 중이면 success:false + pid 반환', async () => {
        const ctx = makeContext();
        const existingProc = { pid: 999, collectorId: undefined };
        const monitor = makeMonitor([existingProc]);
        ctx.fileExists = jest.fn().mockResolvedValue(true);
        const ctrl = new CollectorController(ctx, monitor);

        const result = await ctrl.startCollector();
        expect(result.success).toBe(false);
        expect(result.pid).toBe(999);
    });

    test('시작 성공: 3초 후 프로세스 감지 → success:true + pid', async () => {
        const ctx = makeContext();
        ctx.fileExists = jest.fn().mockResolvedValue(true);

        // spawnCollector 성공, 3초 후 프로세스 감지
        const newProc = { pid: 1234, collectorId: undefined };
        let callCount = 0;
        const monitor = {
            getRunningProcesses: jest.fn().mockImplementation(() => {
                callCount++;
                // 첫 번째(이미 실행 중 확인) = 없음, 두 번째(시작 후 확인) = 있음
                if (callCount === 1) return Promise.resolve({ collector: [], modbusSlave: [] });
                return Promise.resolve({ collector: [newProc], modbusSlave: [] });
            })
        };

        const ctrl = new CollectorController(ctx, monitor);
        // spawnCollector가 실제 spawn 하지 않도록 모킹
        ctrl.spawnCollector = jest.fn().mockResolvedValue({});

        const result = await ctrl.startCollector();
        expect(result.success).toBe(true);
        expect(result.pid).toBe(1234);
    });

    test('시작 후 프로세스 미감지 → success:false', async () => {
        const ctx = makeContext();
        ctx.fileExists = jest.fn().mockResolvedValue(true);
        // 항상 빈 목록 반환
        const monitor = makeMonitor([]);
        const ctrl = new CollectorController(ctx, monitor);
        ctrl.spawnCollector = jest.fn().mockResolvedValue({});

        const result = await ctrl.startCollector();
        expect(result.success).toBe(false);
    });
});

// ─── stopCollector ────────────────────────────────────────────────────────────
describe('CollectorController — stopCollector', () => {
    test('실행 중인 프로세스 없으면 success:false', async () => {
        const ctx = makeContext();
        const monitor = makeMonitor([]);
        const ctrl = new CollectorController(ctx, monitor);

        const result = await ctrl.stopCollector();
        expect(result.success).toBe(false);
        expect(result.error).toMatch(/실행되고 있지 않/);
    });

    test('kill 후 프로세스 사라짐 → success:true', async () => {
        const ctx = makeContext();
        const pid = 555;
        let callCount = 0;
        const monitor = {
            getRunningProcesses: jest.fn().mockImplementation(() => {
                callCount++;
                if (callCount === 1) return Promise.resolve({ collector: [{ pid }], modbusSlave: [] });
                return Promise.resolve({ collector: [], modbusSlave: [] }); // kill 후 없어짐
            })
        };
        const ctrl = new CollectorController(ctx, monitor);

        const result = await ctrl.stopCollector();
        expect(result.success).toBe(true);
        expect(ctx.execCommand).toHaveBeenCalledWith(expect.stringContaining(String(pid)));
    });

    test('kill 후에도 프로세스 남아 있음 → success:false', async () => {
        const ctx = makeContext();
        const pid = 556;
        const monitor = {
            getRunningProcesses: jest.fn().mockResolvedValue({
                collector: [{ pid }], modbusSlave: []
            })
        };
        const ctrl = new CollectorController(ctx, monitor);

        const result = await ctrl.stopCollector();
        expect(result.success).toBe(false);
    });
});

// ─── restartCollector ─────────────────────────────────────────────────────────
describe('CollectorController — restartCollector', () => {
    test('stop 성공 → start 호출', async () => {
        const ctx = makeContext();
        const ctrl = new CollectorController(ctx, makeMonitor([]));
        ctrl.stopCollector = jest.fn().mockResolvedValue({ success: true });
        ctrl.startCollector = jest.fn().mockResolvedValue({ success: true, pid: 111 });

        const result = await ctrl.restartCollector();
        expect(ctrl.stopCollector).toHaveBeenCalled();
        expect(ctrl.startCollector).toHaveBeenCalled();
        expect(result.success).toBe(true);
    });

    test('"실행되고 있지 않습니다" stop 결과여도 start 호출', async () => {
        const ctx = makeContext();
        const ctrl = new CollectorController(ctx, makeMonitor([]));
        ctrl.stopCollector = jest.fn().mockResolvedValue({
            success: false, error: 'Collector(ID: default)가 실행되고 있지 않습니다'
        });
        ctrl.startCollector = jest.fn().mockResolvedValue({ success: true, pid: 222 });

        const result = await ctrl.restartCollector();
        expect(ctrl.startCollector).toHaveBeenCalled();
    });

    test('stop 실패(예외) → start 미호출', async () => {
        const ctx = makeContext();
        const ctrl = new CollectorController(ctx, makeMonitor([]));
        ctrl.stopCollector = jest.fn().mockResolvedValue({
            success: false, error: '권한 오류'
        });
        ctrl.startCollector = jest.fn();

        await ctrl.restartCollector();
        expect(ctrl.startCollector).not.toHaveBeenCalled();
    });
});
