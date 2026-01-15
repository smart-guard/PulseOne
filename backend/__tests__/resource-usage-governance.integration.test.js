const request = require('supertest');
const KnexManager = require('../lib/database/KnexManager');
const knex = KnexManager.getInstance().getKnex();
const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');

describe('Resource Usage Governance Integration Test', () => {
    let tenantId;
    let userId;
    let siteId;
    let protocolId;
    let tenantRepo;
    let deviceRepo;
    let edgeServerRepo;

    beforeAll(async () => {
        const KnexManager = require('../lib/database/KnexManager');
        const knexInstance = KnexManager.getInstance().getKnex();

        // Initialize RepositoryFactory
        const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');
        const factory = RepositoryFactory.getInstance();
        if (!factory.initialized) {
            await factory.initialize({ sqlite: knexInstance });
        }

        tenantRepo = factory.getTenantRepository();
        deviceRepo = factory.getDeviceRepository();
        edgeServerRepo = factory.getEdgeServerRepository();

        // 1. Create Test Tenant
        const tenantRes = await knex('tenants').insert({
            company_name: 'Governance Test Corp ' + Date.now(),
            company_code: 'GOV' + Date.now().toString().slice(-5),
            subscription_plan: 'professional',
            subscription_status: 'active',
            max_edge_servers: 2,
            max_data_points: 10
        });
        tenantId = tenantRes[0];

        // 2. Create Test Site
        const siteRes = await knex('sites').insert({
            tenant_id: tenantId,
            name: 'Test Site',
            code: 'TS' + Date.now().toString().slice(-3),
            site_type: 'factory'
        });
        siteId = siteRes[0];

        // 3. Create a Protocol
        const protoRes = await knex('protocols').insert({
            protocol_type: 'MODBUS_TCP_' + Date.now().toString().slice(-3),
            display_name: 'Modbus TCP Test',
            category: 'industrial'
        });
        protocolId = protoRes[0];
    });

    afterAll(async () => {
        if (!tenantId) return;
        // Clean up - SQLite doesn't support JOIN in DELETE
        await knex('data_points')
            .whereIn('device_id', function () {
                this.select('id').from('devices').where('tenant_id', tenantId);
            })
            .del();

        await knex('devices').where('tenant_id', tenantId).del();
        await knex('edge_servers').where('tenant_id', tenantId).del();
        await knex('sites').where('tenant_id', tenantId).del();
        await knex('tenants').where('id', tenantId).del();
    });

    test('Should count only non-deleted Edge Servers', async () => {
        // Create 2 edge servers
        await knex('edge_servers').insert([
            { tenant_id: tenantId, server_name: 'Edge 1', status: 'active', is_deleted: 0 },
            { tenant_id: tenantId, server_name: 'Edge 2', status: 'pending', is_deleted: 0 }
        ]);

        let stats = await tenantRepo.getTenantStatistics(tenantId);
        expect(Number(stats.edge_servers_count)).toBe(2);

        // Soft-delete one
        await edgeServerRepo.deleteById((await knex('edge_servers').where('server_name', 'Edge 1').first()).id, tenantId);

        stats = await tenantRepo.getTenantStatistics(tenantId);
        expect(Number(stats.edge_servers_count)).toBe(1);
    });

    test('Should count only enabled data points on active devices', async () => {
        // Create a device
        const deviceRes = await knex('devices').insert({
            tenant_id: tenantId,
            site_id: siteId,
            name: 'Device 1',
            protocol_id: protocolId,
            device_type: 'PLC',
            endpoint: '127.0.0.1:502',
            config: '{}',
            is_enabled: 1,
            is_deleted: 0
        });
        const deviceId = deviceRes[0];

        // Create 5 data points
        await knex('data_points').insert([
            { device_id: deviceId, name: 'DP 1', address: 1, data_type: 'INT16', is_enabled: 1 },
            { device_id: deviceId, name: 'DP 2', address: 2, data_type: 'INT16', is_enabled: 1 },
            { device_id: deviceId, name: 'DP 3', address: 3, data_type: 'INT16', is_enabled: 1 },
            { device_id: deviceId, name: 'DP 4', address: 4, data_type: 'INT16', is_enabled: 0 }, // Disabled point
            { device_id: deviceId, name: 'DP 5', address: 5, data_type: 'INT16', is_enabled: 1 }
        ]);

        let stats = await tenantRepo.getTenantStatistics(tenantId);
        expect(Number(stats.data_points_count)).toBe(4); // 4 enabled points

        // Disable device
        await deviceRepo.update(deviceId, { is_enabled: 0 }, tenantId);

        stats = await tenantRepo.getTenantStatistics(tenantId);
        expect(Number(stats.data_points_count)).toBe(0); // All points hidden because device is disabled

        // Re-enable device
        await deviceRepo.update(deviceId, { is_enabled: 1 }, tenantId);
        stats = await tenantRepo.getTenantStatistics(tenantId);
        expect(Number(stats.data_points_count)).toBe(4);

        // Soft-delete device
        await deviceRepo.deleteById(deviceId, tenantId);
        stats = await tenantRepo.getTenantStatistics(tenantId);
        expect(Number(stats.data_points_count)).toBe(0);
    });

    test('Should enforce max_data_points limit in DeviceService', async () => {
        const deviceService = require('../lib/services/DeviceService');

        // Already have 4 points from previous test (deleted device points shouldn't count, but let's be sure)
        // Cleanup from previous tests for fresh start on limits
        await knex('data_points')
            .whereIn('device_id', function () {
                this.select('id').from('devices').where('tenant_id', tenantId);
            })
            .del();
        await knex('devices').where('tenant_id', tenantId).del();

        // Create devices until limit reached
        // Limit is 10.
        // Let's manually insert 9 points first
        const devRes = await knex('devices').insert({
            tenant_id: tenantId,
            site_id: siteId,
            name: 'Limit Device',
            protocol_id: protocolId,
            device_type: 'PLC',
            endpoint: '127.0.0.1:502',
            config: '{}',
            is_enabled: 1,
            is_deleted: 0
        });
        const devId = devRes[0];

        const points = [];
        for (let i = 0; i < 9; i++) {
            points.push({
                device_id: devId,
                name: `Point ${i}`,
                address: 100 + i,
                data_type: 'INT16',
                is_enabled: 1
            });
        }
        await knex('data_points').insert(points);

        // Check stats
        let stats = await tenantRepo.getTenantStatistics(tenantId);
        expect(Number(stats.data_points_count)).toBe(9);

        // Try to create another device (default validateDataPointLimit call)
        // Note: validateDataPointLimit check current usage
        // If limit is 10 and current is 9, next one might pass if it's just a call without additionalPoints.
        // But our logic says: if (maxLimit > 0 && (currentCount + additionalPoints) >= maxLimit) throw error.
        // 9 + 0 >= 10 is false. 10 >= 10 is true.

        // Let's add one more point manually to reach 10
        await knex('data_points').insert({
            device_id: devId,
            name: 'Point 10',
            address: 110,
            data_type: 'INT16',
            is_enabled: 1
        });

        // Now attempt to create a device
        const res = await deviceService.createDevice({
            name: 'Exceed Device',
            device_type: 'PLC',
            protocol_id: protocolId,
            site_id: siteId,
            endpoint: '127.0.0.1:502',
            config: JSON.stringify({ unit_id: 1 })
        }, tenantId);

        expect(res.success).toBe(false);
        expect(res.message).toContain('한도(10개)를 초과했습니다');
    });
});
