// =============================================================================
// backend/__tests__/site.service.test.js
// SiteService unit tests
// =============================================================================

const SiteService = require('../lib/services/SiteService');
const KnexManager = require('../lib/database/KnexManager');
const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');

describe('SiteService Unit Tests', () => {
    let knex;

    beforeAll(async () => {
        process.env.DATABASE_TYPE = 'sqlite';

        // Initialize RepositoryFactory first
        await RepositoryFactory.getInstance().initialize();

        knex = KnexManager.getInstance().getKnex('sqlite');

        // Drop and recreate tables
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
            table.integer('parent_site_id').nullable();
            table.string('name').notNullable();
            table.string('code');
            table.string('site_type').defaultTo('building');
            table.string('description');
            table.string('location');
            table.string('timezone').defaultTo('UTC');
            table.string('address');
            table.string('city');
            table.string('country');
            table.float('latitude');
            table.float('longitude');
            table.integer('hierarchy_level').defaultTo(1);
            table.string('hierarchy_path').defaultTo('/');
            table.integer('is_active').defaultTo(1);
            table.string('contact_name');
            table.string('contact_email');
            table.string('contact_phone');
            table.timestamps(true, true);
        });

        // Insert test tenants
        await knex('tenants').insert([
            { id: 1, company_name: 'Tenant 1' },
            { id: 2, company_name: 'Tenant 2' }
        ]);
    });

    afterAll(async () => {
        await KnexManager.getInstance().destroyAll();
    });

    describe('Site CRUD Operations', () => {
        it('should create a root site', async () => {
            const siteData = {
                name: 'Main HQ',
                code: 'HQ',
                site_type: 'office'
            };

            const response = await SiteService.createSite(siteData, 1);
            expect(response.success).toBe(true);
            expect(response.data.id).toBeDefined();
            expect(response.data.name).toBe('Main HQ');

            const site = await knex('sites').where('id', response.data.id).first();
            expect(site.hierarchy_level).toBe(1);
            expect(site.hierarchy_path).toBe('/');
        });

        it('should create a child site', async () => {
            const root = await knex('sites').where('name', 'Main HQ').first();

            const siteData = {
                name: 'Factory A',
                parent_site_id: root.id,
                site_type: 'factory'
            };

            const response = await SiteService.createSite(siteData, 1);
            expect(response.success).toBe(true);

            const site = await knex('sites').where('id', response.data.id).first();
            expect(site.parent_site_id).toBe(root.id);
            expect(site.hierarchy_level).toBe(2);
            expect(site.hierarchy_path).toBe(`/${root.id}/`);
        });

        it('should get all sites for a tenant', async () => {
            const response = await SiteService.getAllSites(1);
            expect(response.success).toBe(true);
            expect(response.data.length).toBe(2);
        });

        it('should get a site by ID', async () => {
            const root = await knex('sites').where('name', 'Main HQ').first();
            const response = await SiteService.getSiteById(root.id, 1);
            expect(response.success).toBe(true);
            expect(response.data.name).toBe('Main HQ');
        });

        it('should update a site', async () => {
            const root = await knex('sites').where('name', 'Main HQ').first();
            console.log('Root site for update:', root);
            const updateData = { ...root, description: 'Updated Description' };

            const response = await SiteService.updateSite(root.id, updateData, 1);
            if (!response.success) console.error('Update failed:', response.message);
            expect(response.success).toBe(true);

            const updated = await knex('sites').where('id', root.id).first();
            expect(updated.description).toBe('Updated Description');
        });

        it('should patch a site', async () => {
            const root = await knex('sites').where('name', 'Main HQ').first();
            console.log('Root site for patch:', root);
            const response = await SiteService.patchSite(root.id, { city: 'Seoul' }, 1);
            if (!response.success) console.error('Patch failed:', response.message);
            expect(response.success).toBe(true);

            const updated = await knex('sites').where('id', root.id).first();
            expect(updated.city).toBe('Seoul');
        });

        it('should not delete a site with children', async () => {
            const root = await knex('sites').where('name', 'Main HQ').first();
            const response = await SiteService.deleteSite(root.id, 1);
            expect(response.success).toBe(false);
            expect(response.message).toContain('하위 사이트가 존재하여');
        });

        it('should delete a site without children', async () => {
            const child = await knex('sites').where('name', 'Factory A').first();
            const response = await SiteService.deleteSite(child.id, 1);
            expect(response.success).toBe(true);

            const exists = await knex('sites').where('id', child.id).first();
            expect(exists).toBeUndefined();
        });
    });

    describe('Hierarchy and Isolation', () => {
        it('should build a hierarchy tree', async () => {
            // Re-create Factory A
            const root = await knex('sites').where('name', 'Main HQ').first();
            await SiteService.createSite({ name: 'Factory A', parent_site_id: root.id }, 1);

            const response = await SiteService.getHierarchyTree(1);
            expect(response.success).toBe(true);
            expect(response.data.length).toBe(1); // Only root in the top level
            expect(response.data[0].name).toBe('Main HQ');
            expect(response.data[0].children.length).toBe(1);
            expect(response.data[0].children[0].name).toBe('Factory A');
        });

        it('should enforce tenant isolation', async () => {
            const root = await knex('sites').where('name', 'Main HQ').first();

            // Try to access from Tenant 2
            const response = await SiteService.getSiteById(root.id, 2);
            expect(response.success).toBe(false);
            expect(response.message).toContain('사이트를 찾을 수 없습니다');
        });

        it('should allow system admin (tenantId = null) to see all sites', async () => {
            // Create a site for Tenant 2
            await SiteService.createSite({ name: 'T2 Office' }, 2);

            const response = await SiteService.getAllSites(null);
            expect(response.success).toBe(true);
            const names = response.data.map(s => s.name);
            expect(names).toContain('Main HQ');
            expect(names).toContain('T2 Office');
        });
    });
});
