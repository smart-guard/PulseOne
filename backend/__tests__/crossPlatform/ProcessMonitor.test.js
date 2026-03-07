// =============================================================================
// __tests__/crossPlatform/ProcessMonitor.test.js
// ProcessMonitor 유닛 테스트: tasklist CSV 파싱, ps aux 파싱, 에러 폴백
// =============================================================================

'use strict';

jest.mock('../../lib/config/ConfigManager', () => ({ get: jest.fn(() => null) }));
jest.mock('../../lib/utils/LogManager', () => ({ crossplatform: jest.fn() }));
jest.mock('os', () => ({
    platform: () => 'linux',
    arch: () => 'x64',
    hostname: () => 'test-host',
    release: () => '5.15.0',
    totalmem: () => 8 * 1024 * 1024 * 1024,
    freemem: () => 4 * 1024 * 1024 * 1024,
    uptime: () => 3600,
    cpus: () => [{ model: 'Intel Core i7' }],
    loadavg: () => [0.1, 0.2, 0.3]
}));

const PlatformContext = require('../../lib/services/cross-platform/PlatformContext');
const ProcessMonitor = require('../../lib/services/cross-platform/ProcessMonitor');

// ─── 샘플 데이터 ─────────────────────────────────────────────────────────────

// Windows tasklist /fo csv 샘플 출력
const TASKLIST_SAMPLE = `"Image Name","PID","Session Name","Session#","Mem Usage"
"pulseone-collector.exe","1234","Console","1","25,000 K"
"redis-server.exe","5678","Console","1","15,000 K"
"pulseone-export-gateway.exe","9012","Console","1","18,000 K"
"pulseone-modbus-slave.exe","3456","Console","1","12,000 K"
"node.exe","7890","Console","1","50,000 K"
"notepad.exe","9999","Console","1","5,000 K"
`;

// Unix ps aux 샘플 출력 (USER PID %CPU %MEM VSZ RSS TTY STAT START TIME COMMAND)
const PS_AUX_SAMPLE = `root         1  0.0  0.1   1000  500 ?   Ss   10:00   0:00 /sbin/init
nobody    1001  0.5  2.0  55000 20000 ?  Sl   10:01   0:05 node /app/app.js
nobody    1002  1.0  3.0  60000 30000 ?  Sl   10:02   0:10 /app/core/collector/bin/pulseone-collector --id 1
nobody    1003  0.8  2.5  50000 25000 ?  Sl   10:03   0:08 /app/core/export-gateway/bin/export-gateway --id 6
nobody    1004  0.3  1.5  40000 15000 ?  Sl   10:04   0:03 /app/core/modbus-slave/bin/pulseone-modbus-slave --device-id 42
nobody    1005  0.2  1.0  30000 10000 ?  Sl   10:05   0:02 redis-server *:6379
`;

// ─── 테스트: Windows 프로세스 파싱 ──────────────────────────────────────────
describe('ProcessMonitor — Windows tasklist 파싱', () => {
    let ctx;
    let monitor;

    beforeAll(() => {
        ctx = new PlatformContext();
        ctx.isWindows = true;
        // execCommand 모킹 — TASKLIST_SAMPLE 반환
        ctx.execCommand = jest.fn().mockResolvedValue({
            stdout: TASKLIST_SAMPLE,
            stderr: ''
        });
        ctx.log = jest.fn();
        monitor = new ProcessMonitor(ctx);
    });

    test('collector 프로세스 감지 (PID 1234)', async () => {
        const procs = await monitor.getWindowsProcesses();
        expect(procs.collector).toHaveLength(1);
        expect(procs.collector[0].pid).toBe(1234);
        expect(procs.collector[0].platform).toBe('win32');
    });

    test('redis 프로세스 감지 (PID 5678)', async () => {
        const procs = await monitor.getWindowsProcesses();
        expect(procs.redis).toHaveLength(1);
        expect(procs.redis[0].pid).toBe(5678);
    });

    test('export-gateway 프로세스 감지 (PID 9012)', async () => {
        const procs = await monitor.getWindowsProcesses();
        expect(procs.exportGateway).toHaveLength(1);
        expect(procs.exportGateway[0].pid).toBe(9012);
    });

    test('modbus-slave 프로세스 감지 (PID 3456)', async () => {
        const procs = await monitor.getWindowsProcesses();
        expect(procs.modbusSlave).toHaveLength(1);
        expect(procs.modbusSlave[0].pid).toBe(3456);
    });

    test('관계없는 프로세스(notepad.exe) 미분류', async () => {
        const procs = await monitor.getWindowsProcesses();
        const allPids = [
            ...procs.collector,
            ...procs.redis,
            ...procs.exportGateway,
            ...(procs.modbusSlave || [])
        ].map(p => p.pid);
        expect(allPids).not.toContain(9999);
    });
});

// ─── 테스트: Windows 폴백 ─────────────────────────────────────────────────
describe('ProcessMonitor — Windows tasklist 파싱 실패 시 폴백', () => {
    let ctx;
    let monitor;

    beforeAll(() => {
        ctx = new PlatformContext();
        ctx.isWindows = true;
        ctx.log = jest.fn();
        let callCount = 0;
        // 첫 번째 호출(tasklist /fo csv) 실패, 이후(findstr) 성공
        ctx.execCommand = jest.fn().mockImplementation((cmd) => {
            if (cmd.includes('/fo csv')) {
                return Promise.reject(new Error('tasklist 실패'));
            }
            if (cmd.includes('collector')) {
                return Promise.resolve({ stdout: 'pulseone-collector.exe', stderr: '' });
            }
            if (cmd.includes('redis-server')) {
                return Promise.resolve({ stdout: 'redis-server.exe', stderr: '' });
            }
            return Promise.resolve({ stdout: '', stderr: '' });
        });
        monitor = new ProcessMonitor(ctx);
    });

    test('폴백: collector 발견 시 pid=1 더미 반환', async () => {
        const procs = await monitor.getWindowsProcesses();
        expect(procs.collector.length).toBeGreaterThan(0);
    });

    test('폴백: redis 발견 시 pid=2 더미 반환', async () => {
        const procs = await monitor.getWindowsProcesses();
        expect(procs.redis.length).toBeGreaterThan(0);
    });

    test('폴백: modbusSlave 키 존재 (빈 배열이라도)', async () => {
        const procs = await monitor.getWindowsProcesses();
        expect(procs).toHaveProperty('modbusSlave');
    });
});

// ─── 테스트: Unix 프로세스 파싱 ──────────────────────────────────────────────
describe('ProcessMonitor — Unix ps aux 파싱', () => {
    let ctx;
    let monitor;

    beforeAll(() => {
        ctx = new PlatformContext();
        ctx.isWindows = false;
        ctx.platform = 'linux';
        ctx.log = jest.fn();
        ctx.commands = { processFind: 'ps aux | grep ...' };
        ctx.execCommand = jest.fn().mockResolvedValue({
            stdout: PS_AUX_SAMPLE,
            stderr: ''
        });
        monitor = new ProcessMonitor(ctx);
    });

    test('backend (node app.js) 프로세스 감지 (PID 1001)', async () => {
        const procs = await monitor.getUnixProcesses();
        expect(procs.backend).toHaveLength(1);
        expect(procs.backend[0].pid).toBe(1001);
    });

    test('collector 프로세스 감지 (PID 1002)', async () => {
        const procs = await monitor.getUnixProcesses();
        expect(procs.collector).toHaveLength(1);
        expect(procs.collector[0].pid).toBe(1002);
    });

    test('collector --id 1 인자 파싱', async () => {
        const procs = await monitor.getUnixProcesses();
        expect(procs.collector[0].collectorId).toBe(1);
    });

    test('export-gateway 프로세스 감지 (PID 1003)', async () => {
        const procs = await monitor.getUnixProcesses();
        expect(procs.exportGateway[0].pid).toBe(1003);
    });

    test('export-gateway --id 6 인자 파싱', async () => {
        const procs = await monitor.getUnixProcesses();
        expect(procs.exportGateway[0].gatewayId).toBe(6);
    });

    test('modbus-slave 프로세스 감지 (PID 1004)', async () => {
        const procs = await monitor.getUnixProcesses();
        expect(procs.modbusSlave[0].pid).toBe(1004);
    });

    test('modbus-slave --device-id 42 인자 파싱', async () => {
        const procs = await monitor.getUnixProcesses();
        expect(procs.modbusSlave[0].deviceId).toBe(42);
    });

    test('redis-server 프로세스 감지 (PID 1005)', async () => {
        const procs = await monitor.getUnixProcesses();
        expect(procs.redis[0].pid).toBe(1005);
    });
});

// ─── 테스트: Unix 파싱 실패 시 빈 배열 반환 ─────────────────────────────────
describe('ProcessMonitor — Unix 파싱 오류 처리', () => {
    test('execCommand 실패 시 빈 객체 반환 (exception 미전파)', async () => {
        const ctx = new PlatformContext();
        ctx.isWindows = false;
        ctx.platform = 'linux';
        ctx.log = jest.fn();
        ctx.commands = { processFind: 'ps aux' };
        ctx.execCommand = jest.fn().mockRejectedValue(new Error('command failed'));
        const monitor = new ProcessMonitor(ctx);

        const procs = await monitor.getUnixProcesses();
        expect(procs.backend).toEqual([]);
        expect(procs.collector).toEqual([]);
        expect(procs.redis).toEqual([]);
    });
});

// ─── 테스트: getRunningProcesses 분기 ────────────────────────────────────────
describe('ProcessMonitor — getRunningProcesses 분기', () => {
    test('isWindows=true 시 getWindowsProcesses 호출', async () => {
        const ctx = new PlatformContext();
        ctx.isWindows = true;
        ctx.isDocker = false;
        ctx.isDevelopment = false;
        ctx.log = jest.fn();
        ctx.execCommand = jest.fn().mockResolvedValue({ stdout: TASKLIST_SAMPLE, stderr: '' });

        const monitor = new ProcessMonitor(ctx);
        const spy = jest.spyOn(monitor, 'getWindowsProcesses');
        await monitor.getRunningProcesses();
        expect(spy).toHaveBeenCalled();
    });

    test('isWindows=false 시 getUnixProcesses 호출', async () => {
        const ctx = new PlatformContext();
        ctx.isWindows = false;
        ctx.isDocker = false;
        ctx.isDevelopment = false;
        ctx.platform = 'linux';
        ctx.log = jest.fn();
        ctx.commands = { processFind: 'ps aux' };
        ctx.execCommand = jest.fn().mockResolvedValue({ stdout: PS_AUX_SAMPLE, stderr: '' });

        const monitor = new ProcessMonitor(ctx);
        const spy = jest.spyOn(monitor, 'getUnixProcesses');
        await monitor.getRunningProcesses();
        expect(spy).toHaveBeenCalled();
    });
});
