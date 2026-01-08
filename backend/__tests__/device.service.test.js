// =============================================================================
// backend/__tests__/device.service.test.js
// DeviceService unit tests
// =============================================================================

const DeviceService = require('../lib/services/DeviceService');
const KnexManager = require('../lib/database/KnexManager');
const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');

describe('DeviceService Unit Tests', () => {
    let knex;

    beforeAll(async () => {
        process.env.DATABASE_TYPE = 'sqlite';

        // Initialize RepositoryFactory
        await RepositoryFactory.getInstance().initialize();

        // Disable ConfigSyncHooks for testing
        require('../lib/hooks/ConfigSyncHooks').getInstance().setEnabled(false);

        knex = KnexManager.getInstance().getKnex('sqlite');

        // Drop and recreate tables
        await knex.schema.dropTableIfExists('data_points');
        await knex.schema.dropTableIfExists('device_status');
        await knex.schema.dropTableIfExists('device_settings');
        await knex.schema.dropTableIfExists('devices');
        await knex.schema.dropTableIfExists('protocols');
        await knex.schema.dropTableIfExists('sites');
        await knex.schema.dropTableIfExists('tenants');

        await knex.schema.createTable('tenants', (table) => {
            table.increments('id').primary();
            table.string('company_name');
            table.integer('is_active').defaultTo(1);
        });

        await knex.schema.createTable('sites', (table) => {
            table.increments('id').primary();
            table.integer('tenant_id').references('id').inTable('tenants');
            table.string('name').notNullable();
        });

        await knex.schema.createTable('protocols', (table) => {
            table.increments('id').primary();
            table.string('protocol_type').notNullable();
            table.string('display_name');
            table.integer('is_enabled').defaultTo(1);
        });

        await knex.schema.createTable('devices', (table) => {
            table.increments('id').primary();
            table.integer('tenant_id').references('id').inTable('tenants');
            table.integer('site_id').references('id').inTable('sites');
            table.integer('protocol_id').references('id').inTable('protocols');
            table.integer('device_group_id');
            table.integer('edge_server_id');
            table.string('name').notNullable();
            table.string('description');
            table.string('device_type').defaultTo('PLC');
            table.string('manufacturer');
            table.string('model');
            table.string('serial_number');
            table.string('endpoint');
            table.text('config');
            table.integer('polling_interval').defaultTo(1000);
            table.integer('timeout').defaultTo(3000);
            table.integer('retry_count').defaultTo(3);
            table.integer('is_enabled').defaultTo(1);
            table.datetime('installation_date');
            table.timestamps(true, true);
        });

        await knex.schema.createTable('device_settings', (table) => {
            table.integer('device_id').primary().references('id').inTable('devices');
            table.integer('polling_interval_ms').defaultTo(1000);
            table.timestamps(true, true);
        });

        await knex.schema.createTable('device_status', (table) => {
            table.integer('device_id').primary().references('id').inTable('devices');
            table.string('connection_status').defaultTo('unknown');
            table.integer('error_count').defaultTo(0);
            table.string('last_error');
            table.datetime('last_communication');
            table.timestamps(true, true);
        });

        // Seed data
        await knex('tenants').insert([{ id: 1, company_name: 'Tenant 1' }]);
        await knex('sites').insert([{ id: 1, tenant_id: 1, name: 'Site 1' }]);
        await knex('protocols').insert([{ id: 1, protocol_type: 'MODBUS_TCP', display_name: 'Modbus TCP' }]);
    });

    afterAll(async () => {
        await KnexManager.getInstance().destroyAll();
    });

    describe('Device CRUD', () => {
        it('should create a device', async () => {
            const deviceData = {
                name: 'Test Device',
                site_id: 1,
                protocol_id: 1,
                device_type: 'SENSOR',
                endpoint: '192.168.1.10',
                config: JSON.stringify({ slave_id: 1 })
            };

            const response = await DeviceService.createDevice(deviceData, 1);
            expect(response.success).toBe(true);
            expect(response.data.id).toBeDefined();
            expect(response.data.name).toBe('Test Device');
        });

        it('should get device by ID', async () => {
            const device = await knex('devices').where('name', 'Test Device').first();
            const response = await DeviceService.getDeviceById(device.id, 1);

            expect(response.success).toBe(true);
            expect(response.data.name).toBe('Test Device');
        });

        it('should enforce tenant isolation', async () => {
            const device = await knex('devices').where('name', 'Test Device').first();
            const response = await DeviceService.getDeviceById(device.id, 2);

            expect(response.success).toBe(false);
        });

        it('should delete a device', async () => {
            const device = await knex('devices').where('name', 'Test Device').first();
            const response = await DeviceService.delete(device.id, 1);

            expect(response.success).toBe(true);

            // Check if deleted
            const deletedDevice = await knex('devices').where('id', device.id).first();
            expect(deletedDevice).toBeUndefined();

            // Check if status deleted
            const deletedStatus = await knex('device_status').where('device_id', device.id).first();
            expect(deletedStatus).toBeUndefined();
        });
    });
});
