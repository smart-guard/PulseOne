const KnexManager = require('../lib/database/KnexManager');
const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');
const DeviceService = require('../lib/services/DeviceService');
const DeviceGroupService = require('../lib/services/DeviceGroupService');
const ConfigManager = require('../lib/config/ConfigManager');

describe('Device Bulk Operations & Hierarchy Tests', () => {
    let knex;
    let deviceRepo;
    let groupRepo;

    beforeAll(async () => {
        // Force SQLite in-memory for tests
        process.env.DATABASE_TYPE = 'sqlite';
        process.env.SQLITE_PATH = ':memory:';

        ConfigManager.getInstance().set('DATABASE_TYPE', 'sqlite');
        ConfigManager.getInstance().set('SQLITE_PATH', ':memory:');

        // Initialize RepositoryFactory
        await RepositoryFactory.getInstance().initialize();

        knex = KnexManager.getInstance().getKnex('sqlite');

        // Cleanup existing tables
        const tables = ['data_points', 'device_status', 'device_group_assignments', 'devices', 'protocols', 'device_groups', 'sites', 'tenants', 'audit_logs'];
        for (const table of tables) {
            await knex.schema.dropTableIfExists(table);
        }

        // Setup minimal schema correctly
        await knex.schema.createTable('tenants', (table) => {
            table.increments('id').primary();
            table.string('company_name');
            table.integer('is_active').defaultTo(1);
            table.timestamps(true, true);
        });

        await knex.schema.createTable('sites', (table) => {
            table.increments('id').primary();
            table.integer('tenant_id');
            table.string('name').notNullable();
            table.timestamps(true, true);
        });

        await knex.schema.createTable('device_groups', (table) => {
            table.increments('id').primary();
            table.string('name').notNullable();
            table.integer('parent_group_id');
            table.integer('tenant_id');
            table.integer('site_id');
            table.string('group_type').defaultTo('logical');
            table.integer('sort_order').defaultTo(0);
            table.timestamps(true, true);
        });

        await knex.schema.createTable('protocols', (table) => {
            table.increments('id').primary();
            table.string('protocol_type').notNullable();
            table.string('display_name');
            table.timestamps(true, true);
        });

        await knex.schema.createTable('devices', (table) => {
            table.increments('id').primary();
            table.string('name').notNullable();
            table.integer('device_group_id');
            table.integer('protocol_id');
            table.integer('tenant_id');
            table.integer('site_id');
            table.string('protocol_type');
            table.string('endpoint');
            table.json('config');
            table.json('tags');
            table.timestamps(true, true);
        });

        await knex.schema.createTable('device_status', (table) => {
            table.integer('device_id').primary();
            table.string('connection_status').defaultTo('unknown');
            table.timestamp('last_communication');
            table.integer('error_count').defaultTo(0);
            table.timestamps(true, true);
        });

        await knex.schema.createTable('device_group_assignments', (table) => {
            table.integer('device_id').notNullable();
            table.integer('group_id').notNullable();
            table.integer('is_primary').defaultTo(0);
            table.timestamp('created_at').defaultTo(knex.raw("datetime('now', 'localtime')"));
            table.primary(['device_id', 'group_id']);
        });

        await knex.schema.createTable('data_points', (table) => {
            table.increments('id').primary();
            table.integer('device_id');
            table.integer('is_enabled').defaultTo(1);
            table.timestamps(true, true);
        });

        await knex.schema.createTable('audit_logs', (table) => {
            table.increments('id').primary();
            table.integer('user_id');
            table.integer('tenant_id');
            table.string('action');
            table.string('target_type');
            table.integer('target_id');
            table.string('entity_type');
            table.integer('entity_id');
            table.string('entity_name');
            table.string('change_summary');
            table.json('details');
            table.json('old_value');
            table.json('new_value');
            table.string('severity');
            table.string('ip_address');
            table.string('user_agent');
            table.string('request_id');
            table.timestamps(true, true);
        });

        deviceRepo = RepositoryFactory.getInstance().getDeviceRepository();
        groupRepo = RepositoryFactory.getInstance().getDeviceGroupRepository();
    });

    beforeEach(async () => {
        // Clean up
        await knex('devices').del();
        await knex('device_groups').del();
        await knex('device_group_assignments').del();

        // Seed Groups: Root (1) -> Child (2) -> Grandchild (3)
        await knex('device_groups').insert([
            { id: 1, name: 'Root Group', tenant_id: 1, site_id: 1, sort_order: 1 },
            { id: 2, name: 'Child Group', parent_group_id: 1, tenant_id: 1, site_id: 1, sort_order: 2 },
            { id: 3, name: 'Grandchild Group', parent_group_id: 2, tenant_id: 1, site_id: 1, sort_order: 3 },
            { id: 4, name: 'Other Group', tenant_id: 1, site_id: 1, sort_order: 4 }
        ]);

        // Seed Devices
        await knex('devices').insert([
            { id: 1, name: 'Device 1', device_group_id: 1, tenant_id: 1, site_id: 1, protocol_type: 'MODBUS_TCP', endpoint: '127.0.0.1:502', protocol_id: 1 },
            { id: 2, name: 'Device 2', device_group_id: 2, tenant_id: 1, site_id: 1, protocol_type: 'MODBUS_TCP', endpoint: '127.0.0.1:503', protocol_id: 1 },
            { id: 3, name: 'Device 3', device_group_id: 3, tenant_id: 1, site_id: 1, protocol_type: 'MODBUS_TCP', endpoint: '127.0.0.1:504', protocol_id: 1 },
            { id: 4, name: 'Device 4', device_group_id: null, tenant_id: 1, site_id: 1, protocol_type: 'MODBUS_TCP', endpoint: '127.0.0.1:505', protocol_id: 1 }
        ]);

        // Device 4 belongs to both Group 2 and Group 4 via assignments
        await knex('device_group_assignments').insert([
            { device_id: 4, group_id: 2 },
            { device_id: 4, group_id: 4 }
        ]);
    });

    afterAll(async () => {
        await RepositoryFactory.getInstance().cleanup();
    });

    describe('Hierarchy & Recursive Filtering', () => {
        test('getDescendantIds should return all child IDs recursively', async () => {
            const result = await DeviceGroupService.getDescendantIds(1, 1);
            expect(result.success).toBe(true);
            expect(result.data).toContain(1);
            expect(result.data).toContain(2);
            expect(result.data).toContain(3);
            expect(result.data).not.toContain(4);
        });

        test('DeviceRepository should filter by multiple group IDs (Recursive Logic)', async () => {
            const descendantIds = [1, 2, 3];
            const result = await deviceRepo.findAll(1, { groupId: descendantIds });

            // Should be exactly items array if includeCount is false
            const items = Array.isArray(result) ? result : result.items;
            expect(items.length).toBe(4); // Device 1, 2, 3 (via direct id) + Device 4 (via assignment to group 2)
            const ids = items.map(d => d.id);
            expect(ids).toContain(1);
            expect(ids).toContain(2);
            expect(ids).toContain(3);
            expect(ids).toContain(4);
        });

        test('DeviceRepository should filter by assignment only', async () => {
            const result = await deviceRepo.findAll(1, { groupId: 4 });
            const items = Array.isArray(result) ? result : result.items;
            expect(items.length).toBe(1);
            expect(items[0].id).toBe(4);
        });
    });

    describe('Bulk Update', () => {
        test('bulkUpdateDevices should change group for multiple devices', async () => {
            const deviceIds = [1, 4];
            const targetGroupId = 3;

            const result = await DeviceService.bulkUpdateDevices(deviceIds, { device_group_id: targetGroupId }, 1, { id: 99, tenant_id: 1 });
            expect(result.success).toBe(true);
            expect(result.data).toBe(2); // 2 rows affected

            // Verify
            const movedDevices = await knex('devices').whereIn('id', deviceIds);
            movedDevices.forEach(d => {
                expect(d.device_group_id).toBe(targetGroupId);
            });
        });
    });

    describe('Bulk Delete', () => {
        test('bulkDeleteDevices should remove multiple devices', async () => {
            const deviceIds = [1, 2];

            const result = await DeviceService.bulkDeleteDevices(deviceIds, 1, { id: 99, tenant_id: 1 });
            expect(result.success).toBe(true);
            expect(result.data).toBe(2); // 2 rows affected

            // Verify
            const remaining = await knex('devices').whereIn('id', deviceIds);
            expect(remaining.length).toBe(0);
        });
    });

    describe('Pagination with Filters', () => {
        test('findAll includeCount should respect group filters', async () => {
            const descendantIds = [2, 3]; // Child + Grandchild
            const result = await deviceRepo.findAll(1, {
                groupId: descendantIds,
                includeCount: true
            });

            expect(result.total).toBe(3); // Device 2, 3 + Device 4 (assigned to 2)
            expect(result.items.length).toBe(3);
        });
    });
});
