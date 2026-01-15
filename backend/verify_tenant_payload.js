
const TenantService = require('./lib/services/TenantService');
const RepositoryFactory = require('./lib/database/repositories/RepositoryFactory');
const config = require('./lib/config/ConfigManager');

async function runVerification() {
    console.log('🚀 Starting Tenant Empty String Verification...');

    // Initialize RepositoryFactory
    await RepositoryFactory.getInstance().initialize({
        database: config.getDatabaseConfig()
    });

    const tenantService = new TenantService();

    try {
        // 1. Create First Tenant with Empty Domain
        console.log('\nTesting Tenant Creation 1 (Empty Domain)...');
        const payload1 = {
            company_name: `Empty Corp 1 ${Date.now()}`,
            company_code: `EC1-${Date.now()}`,
            domain: '', // Empty string
            contact_name: 'Tester 1',
            subscription_plan: 'starter',
            subscription_status: 'trial',
            is_active: true
        };

        const res1 = await tenantService.createTenant(payload1);
        if (res1.success) {
            console.log('✅ Tenant 1 Created. Domain:', res1.data.domain); // Should be null
            if (res1.data.domain !== null && res1.data.domain !== '') {
                console.log('⚠️ Domain is not null/empty:', res1.data.domain);
            }
        } else {
            throw new Error(`Failed to create Tenant 1: ${res1.message}`);
        }

        // 2. Create Second Tenant with Empty Domain (Should NOT fail if converted to null)
        console.log('\nTesting Tenant Creation 2 (Empty Domain)...');
        const payload2 = {
            company_name: `Empty Corp 2 ${Date.now()}`,
            company_code: `EC2-${Date.now()}`,
            domain: '', // Empty string again
            contact_name: 'Tester 2',
            subscription_plan: 'starter',
            subscription_status: 'trial',
            is_active: true
        };

        const res2 = await tenantService.createTenant(payload2);
        if (res2.success) {
            console.log('✅ Tenant 2 Created. Domain:', res2.data.domain);
        } else {
            console.error('❌ Failed to create Tenant 2 (Unique Constraint?):', res2.message);
            throw new Error(`Failed to create Tenant 2: ${res2.message}`);
        }

    } catch (error) {
        console.error('❌ Verification Failed:', error);
        process.exit(1);
    } finally {
        console.log('\n✨ Verification Complete');
        process.exit(0);
    }
}

runVerification();
