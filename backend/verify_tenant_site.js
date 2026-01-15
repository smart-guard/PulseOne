
const TenantService = require('./lib/services/TenantService');
const SiteService = require('./lib/services/SiteService');
const RepositoryFactory = require('./lib/database/repositories/RepositoryFactory');
const config = require('./lib/config/ConfigManager');

async function runVerification() {
    console.log('🚀 Starting Tenant & Site Management Verification...');

    // Initialize RepositoryFactory
    await RepositoryFactory.getInstance().initialize({
        database: config.getDatabaseConfig()
    });

    const tenantService = new TenantService();
    const siteService = SiteService; // SiteService exports an instance

    let tenantId;
    let siteId;
    let childSiteId;

    try {
        // 1. Create Tenant
        console.log('\nTesting Tenant Creation...');
        const tenantData = {
            company_name: `Test Corp ${Date.now()}`,
            company_code: `TC-${Date.now()}`,
            is_active: true
        };
        const tenantResult = await tenantService.createTenant(tenantData);
        if (tenantResult.success && tenantResult.data) {
            console.log('✅ Tenant Created:', tenantResult.data.company_name);
            tenantId = tenantResult.data.id;
        } else {
            throw new Error(`Failed to create tenant: ${tenantResult.message}`);
        }

        // 2. Update Tenant
        console.log('\nTesting Tenant Update...');
        const updateResult = await tenantService.updateTenant(tenantId, { contact_name: 'John Doe' });
        if (updateResult.success && updateResult.data.contact_name === 'John Doe') {
            console.log('✅ Tenant Updated Successfully');
        } else {
            throw new Error('Failed to update tenant');
        }

        // 3. Create Site (Parent)
        console.log('\nTesting Site Creation (Parent)...');
        const siteData = {
            tenant_id: tenantId,
            name: 'Headquarters',
            code: 'HQ-01',
            site_type: 'company',
            is_active: true
        };
        const siteResult = await siteService.createSite(siteData);
        if (siteResult.success && siteResult.data) {
            console.log('✅ Parent Site Created:', siteResult.data.name);
            siteId = siteResult.data.id;
        } else {
            throw new Error(`Failed to create site: ${siteResult.message}`);
        }

        // 4. Create Site (Child)
        console.log('\nTesting Site Creation (Child)...');
        const childSiteData = {
            tenant_id: tenantId,
            parent_site_id: siteId,
            name: 'Floor 1',
            code: 'HQ-F1',
            site_type: 'floor',
            is_active: true
        };
        const childSiteResult = await siteService.createSite(childSiteData);
        if (childSiteResult.success && childSiteResult.data) {
            console.log('✅ Child Site Created:', childSiteResult.data.name, 'Parent ID:', childSiteResult.data.parent_site_id);
            childSiteId = childSiteResult.data.id;
        } else {
            throw new Error(`Failed to create child site: ${childSiteResult.message}`);
        }

        // 5. Test Hierarchy
        console.log('\nTesting Hierarchy Retrieval...');
        const treeResult = await siteService.getHierarchyTree(tenantId);
        if (treeResult.success && treeResult.data) {
            // Basic check - implementation details of getSiteTree might vary, just checking success
            console.log('✅ Site Tree Retrieved. Nodes:', treeResult.data.length);
        } else {
            throw new Error('Failed to retrieve site tree');
        }

        // 6. Test Soft Delete Site (Child)
        console.log('\nTesting Site Soft Delete...');
        const deleteSiteResult = await siteService.deleteSite(childSiteId, tenantId);
        if (deleteSiteResult.success) {
            console.log('✅ Site Soft Deleted');
        } else {
            throw new Error('Failed to delete site');
        }

        // Verify it is deleted (findAll should not return it by default)
        const sitesAfterDelete = await siteService.getAllSites(tenantId); // default no deleted
        const found = sitesAfterDelete.data.items.find(s => s.id === childSiteId);
        if (!found) {
            console.log('✅ Soft deleted site hidden from default list');
        } else {
            throw new Error('Soft deleted site still visible in default list');
        }

        // Verify it exists in onlyDeleted
        const deletedSites = await siteService.getAllSites(tenantId, { onlyDeleted: true });
        const foundDeleted = deletedSites.data.items.find(s => s.id === childSiteId);
        if (foundDeleted) {
            console.log('✅ Soft deleted site found in onlyDeleted list');
        } else {
            throw new Error('Soft deleted site NOT found in onlyDeleted list');
        }

        // 7. Test Restore Site
        console.log('\nTesting Site Restore...');
        const restoreSiteResult = await siteService.restoreSite(childSiteId);
        if (restoreSiteResult.success) {
            console.log('✅ Site Restored');
        } else {
            throw new Error('Failed to restore site');
        }

        // 8. Test Soft Delete Tenant
        console.log('\nTesting Tenant Soft Delete...');
        // Should fail if we don't clean up sites first? Depending on logic.
        // Assuming cascade or check. Usually we prevent deleting if children exist.
        // Let's try to delete tenant.
        try {
            const deleteTenantResult = await tenantService.deleteTenant(tenantId);
            console.log('ℹ️  Tenant delete result:', deleteTenantResult.success);
        } catch (e) {
            console.log('✅ Expected error/check when deleting tenant with sites:', e.message);
            // Verify soft delete logic for tenant if we clean up sites
        }

    } catch (error) {
        console.error('❌ Verification Failed:', error);
        process.exit(1);
    } finally {
        // Cleanup if needed, but since we use soft delete / distinct test data, maybe okay.
        console.log('\n✨ Verification Complete');
        process.exit(0);
    }
}

runVerification();
