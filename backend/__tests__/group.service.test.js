
const DeviceGroupService = require('../lib/services/DeviceGroupService');
const KnexManager = require('../lib/database/KnexManager');
const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');

describe('DeviceGroupService Unit Tests', () => {
    let knex;

    beforeAll(async () => {
        // Mock environment for test
        process.env.DATABASE_TYPE = 'sqlite';
        process.env.SQLITE_PATH = ':memory:';

        const ConfigManager = require('../lib/config/ConfigManager');
        ConfigManager.getInstance().set('DATABASE_TYPE', 'sqlite');
        ConfigManager.getInstance().set('SQLITE_PATH', ':memory:');

        // Initialize RepositoryFactory
        await RepositoryFactory.getInstance().initialize();
        knex = KnexManager.getInstance().getKnex('sqlite');

        // Setup Schema
        await knex.schema.dropTableIfExists('device_groups');
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

        await knex.schema.createTable('device_groups', (table) => {
            table.increments('id').primary();
            table.integer('tenant_id').notNullable().references('id').inTable('tenants');
            table.integer('site_id').notNullable().references('id').inTable('sites');
            table.integer('parent_group_id').references('id').inTable('device_groups');
            table.string('name').notNullable();
            table.text('description');
            table.string('group_type').defaultTo('functional');
            table.text('tags');
            table.text('metadata');
            table.integer('is_active').defaultTo(1);
            table.integer('sort_order').defaultTo(0);
            table.integer('created_by');
            table.timestamps(true, true);
        });

        // Seed Data
        await knex('tenants').insert([{ id: 1, company_name: 'Test Tenant' }]);
        await knex('sites').insert([{ id: 1, tenant_id: 1, name: 'Test Site' }]);
    });

    afterAll(async () => {
        await KnexManager.getInstance().destroyAll();
    });

    describe('Group Creation Logic', () => {
        it('should successfully create a root group with valid tenantId', async () => {
            const groupData = { name: 'Root Group' };
            const tenantId = 1;

            const response = await DeviceGroupService.createGroup(groupData, tenantId);
            expect(response.success).toBe(true);
            expect(response.data.name).toBe('Root Group');
            expect(response.data.tenant_id).toBe(1);
            expect(response.data.site_id).toBe(1); // Default site
        });

        it('should successfully create a child group', async () => {
            // Get parent first
            const parents = await knex('device_groups').where({ name: 'Root Group' });
            const parentId = parents[0].id;

            const childData = { name: 'Child Group', parent_group_id: parentId };
            const response = await DeviceGroupService.createGroup(childData, 1);

            expect(response.success).toBe(true);
            expect(response.data.parent_group_id).toBe(parentId);
        });

        it('should fail if tenantId is missing (Constraint Violation)', async () => {
            // Expecting failure because Repository no longer has hardcoded fallback
            const response = await DeviceGroupService.createGroup({ name: 'Fail Group' }, null);

            expect(response.success).toBe(false);
            expect(response.message).toMatch(/SQLITE_CONSTRAINT/);
        });
    });

    describe('Group Tree Retrieval', () => {
        it('should return a tree structure', async () => {
            const response = await DeviceGroupService.getAllGroups(1, { tree: true });

            expect(response.success).toBe(true);
            expect(Array.isArray(response.data)).toBe(true);
            expect(response.data.length).toBeGreaterThan(0);

            const root = response.data.find(g => g.name === 'Root Group');
            expect(root).toBeDefined();
            expect(root.children).toBeDefined();
            expect(root.children.length).toBeGreaterThan(0);
            expect(root.children[0].name).toBe('Child Group');
        });
    });
});
