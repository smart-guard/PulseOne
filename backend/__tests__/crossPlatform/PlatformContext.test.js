// =============================================================================
// __tests__/crossPlatform/PlatformContext.test.js
// PlatformContext 유닛 테스트: 환경 감지, 경로 초기화, 명령어, 유틸리티
// jest.doMock() 패턴으로 플랫폼별 os.platform() 주입
// =============================================================================

'use strict';

// jest.mock()을 파일 스코프에서 쓰지 않고, 각 테스트에서 doMock 사용

// ─── 헬퍼 ────────────────────────────────────────────────────────────────────
function loadContextWithPlatform(platform, extraEnv = {}) {
    jest.resetModules();

    // 공통 의존성 mock
    jest.doMock('../../lib/config/ConfigManager', () => ({ get: jest.fn(() => null) }));
    jest.doMock('../../lib/utils/LogManager', () => ({ crossplatform: jest.fn() }));

    // os mock — 플랫폼 파라미터를 직접 반환 (클로저 아닌 리터럴)
    const fakePlatform = platform; // hoisting 문제 없도록 별도 할당
    jest.doMock('os', () => ({
        platform: () => fakePlatform,
        arch: () => 'x64',
        hostname: () => 'test-host',
        release: () => '5.15.0',
        version: undefined,
        totalmem: () => 8 * 1024 * 1024 * 1024,
        freemem: () => 4 * 1024 * 1024 * 1024,
        uptime: () => 3600,
        cpus: () => [{ model: 'Intel Core i7' }],
        loadavg: () => [0.1, 0.2, 0.3]
    }));

    // 환경변수 임시 적용
    const prevEnv = { ...process.env };
    Object.assign(process.env, extraEnv);

    const PlatformContext = require('../../lib/services/cross-platform/PlatformContext');
    const ctx = new PlatformContext();

    // 환경변수 복원
    for (const k of Object.keys(extraEnv)) delete process.env[k];
    Object.assign(process.env, prevEnv);

    return ctx;
}

// ─── 환경 감지 ──────────────────────────────────────────────────────────────
describe('PlatformContext — 환경 감지', () => {
    afterEach(() => jest.resetModules());

    test('linux 플랫폼: isLinux=true, isWindows=false, isMac=false', () => {
        const ctx = loadContextWithPlatform('linux');
        expect(ctx.isLinux).toBe(true);
        expect(ctx.isWindows).toBe(false);
        expect(ctx.isMac).toBe(false);
    });

    test('win32 플랫폼: isWindows=true, isLinux=false', () => {
        const ctx = loadContextWithPlatform('win32');
        expect(ctx.isWindows).toBe(true);
        expect(ctx.isLinux).toBe(false);
    });

    test('darwin 플랫폼: isMac=true', () => {
        const ctx = loadContextWithPlatform('darwin');
        expect(ctx.isMac).toBe(true);
    });

    test('win32: isDevelopment === false (항상)', () => {
        const ctx = loadContextWithPlatform('win32');
        expect(ctx.isDevelopment).toBe(false);
    });

    test('linux + DOCKER_CONTAINER=true: isDevelopment=true', () => {
        const ctx = loadContextWithPlatform('linux', { DOCKER_CONTAINER: 'true' });
        expect(ctx.isDevelopment).toBe(true);
    });
});

// ─── 경로 초기화 ─────────────────────────────────────────────────────────────
describe('PlatformContext — 경로 초기화', () => {
    afterEach(() => jest.resetModules());

    test('linux: paths에 collector, redis, sqlite, root 모두 존재', () => {
        const ctx = loadContextWithPlatform('linux', { DOCKER_CONTAINER: 'true' });
        expect(ctx.paths).toHaveProperty('collector');
        expect(ctx.paths).toHaveProperty('redis');
        expect(ctx.paths).toHaveProperty('sqlite');
        expect(ctx.paths).toHaveProperty('root');
    });

    test('win32: redis 경로가 문자열', () => {
        const ctx = loadContextWithPlatform('win32');
        expect(typeof ctx.paths.redis).toBe('string');
        expect(ctx.paths.redis.length).toBeGreaterThan(0);
    });

    test('darwin: collector 경로가 문자열', () => {
        const ctx = loadContextWithPlatform('darwin');
        expect(typeof ctx.paths.collector).toBe('string');
    });

    test('알 수 없는 플랫폼: linux 기본값으로 폴백 (paths 존재)', () => {
        const ctx = loadContextWithPlatform('freebsd');
        expect(ctx.paths).toHaveProperty('collector');
    });
});

// ─── 명령어 초기화 ────────────────────────────────────────────────────────────
describe('PlatformContext — 명령어 초기화', () => {
    afterEach(() => jest.resetModules());

    test('linux: processKill(1234) = "kill -9 1234"', () => {
        const ctx = loadContextWithPlatform('linux');
        expect(ctx.commands.processKill(1234)).toBe('kill -9 1234');
    });

    test('win32: processKill(5678) = "taskkill /PID 5678 /F"', () => {
        const ctx = loadContextWithPlatform('win32');
        expect(ctx.commands.processKill(5678)).toBe('taskkill /PID 5678 /F');
    });

    test('linux: redisCliShutdown = "redis-cli shutdown"', () => {
        const ctx = loadContextWithPlatform('linux');
        expect(ctx.commands.redisCliShutdown).toBe('redis-cli shutdown');
    });

    test('win32: redisCliShutdown = "redis-cli.exe shutdown"', () => {
        const ctx = loadContextWithPlatform('win32');
        expect(ctx.commands.redisCliShutdown).toBe('redis-cli.exe shutdown');
    });

    test('linux: processFind에 ps aux 포함', () => {
        const ctx = loadContextWithPlatform('linux');
        expect(ctx.commands.processFind).toMatch(/ps aux/);
    });

    test('win32: processFind에 tasklist 포함', () => {
        const ctx = loadContextWithPlatform('win32');
        expect(ctx.commands.processFind).toMatch(/tasklist/);
    });
});

// ─── 유틸리티 ────────────────────────────────────────────────────────────────
describe('PlatformContext — 유틸리티', () => {
    let ctx;
    beforeAll(() => { ctx = loadContextWithPlatform('linux'); });
    afterAll(() => jest.resetModules());

    test('sleep(50): 45ms 이상 대기', async () => {
        const start = Date.now();
        await ctx.sleep(50);
        expect(Date.now() - start).toBeGreaterThanOrEqual(45);
    });

    test('formatBytes(0) = "0 Bytes"', () => {
        expect(ctx.formatBytes(0)).toBe('0 Bytes');
    });

    test('formatBytes(1024) = "1 KB"', () => {
        expect(ctx.formatBytes(1024)).toBe('1 KB');
    });

    test('formatBytes(1024*1024) = "1 MB"', () => {
        expect(ctx.formatBytes(1024 * 1024)).toBe('1 MB');
    });

    test('calculateUptime(null) = "N/A"', () => {
        expect(ctx.calculateUptime(null)).toBe('N/A');
    });

    test('calculateUptime(5초 전) = 초 단위 문자열', () => {
        const result = ctx.calculateUptime(new Date(Date.now() - 5000).toISOString());
        expect(result).toMatch(/\d+s/);
    });

    test('calculateUptime(2분 전) = 분 단위 문자열', () => {
        const result = ctx.calculateUptime(new Date(Date.now() - 130000).toISOString());
        expect(result).toMatch(/m/);
    });

    test('fileExists(존재하지 않는 파일) = false', async () => {
        const result = await ctx.fileExists('/nonexistent/nowhere/file.bin');
        expect(result).toBe(false);
    });

    test('directoryExists(존재하지 않는 경로) = false', async () => {
        const result = await ctx.directoryExists('/nonexistent/dir');
        expect(result).toBe(false);
    });

    test('getDeploymentType(): 문자열 반환', () => {
        const result = ctx.getDeploymentType();
        expect(typeof result).toBe('string');
        expect(result.length).toBeGreaterThan(0);
    });

    test('getPlatformInfo(): detected, architecture, paths 포함', () => {
        const info = ctx.getPlatformInfo();
        expect(info).toHaveProperty('detected');
        expect(info).toHaveProperty('architecture');
        expect(info).toHaveProperty('paths');
    });

    test('getSystemInfo(): platform, memory, runtime, system 포함', async () => {
        const info = await ctx.getSystemInfo();
        expect(info).toHaveProperty('platform');
        expect(info).toHaveProperty('memory');
        expect(info).toHaveProperty('runtime');
        expect(info).toHaveProperty('system');
    });
});
