const { initializeConnections } = require('./lib/connection/db');
const ConfigManager = require('./lib/config/ConfigManager');
const RepositoryFactory = require('./lib/database/repositories/RepositoryFactory');

async function check() {
    try {
        const config = ConfigManager.getInstance();
        console.log('--- Config Info ---');
        const dbConfig = config.getDatabaseConfig();
        console.log('DB Type:', dbConfig.type);
        console.log('SQLite Path:', dbConfig.sqlite.path);

        console.log('\n--- Initializing Connections ---');
        const dbConnections = await initializeConnections();

        console.log('\n--- Initializing RepositoryFactory ---');
        const rf = RepositoryFactory.getInstance();
        await rf.initialize({
            database: dbConfig,
            cache: { enabled: false }
        });

        const repo = rf.getRepository('DeviceRepository');

        console.log('\n--- Testing Devices ---');
        const devices = await repo.findAll(null, { limit: 10, includeCount: true });
        // findAll usually returns { items, pagination } or Array depending on options
        const deviceItems = devices.items || (Array.isArray(devices) ? devices : []);
        console.log('Devices found:', deviceItems.length);
        if (deviceItems.length > 0) {
            console.log('Sample device ID:', deviceItems[0].id, 'Name:', deviceItems[0].name);
        }

        console.log('\n--- Testing Data Points (Tenant 1) ---');
        const points1 = await repo.searchDataPoints(1, '');
        console.log('Points found for tenant 1:', points1.length);

        console.log('\n--- Testing Devices (Tenant 1) ---');
        const devices1 = await repo.findAll(1, { limit: 10 });
        const deviceItems1 = devices1.items || (Array.isArray(devices1) ? devices1 : []);
        console.log('Devices found for tenant 1:', deviceItems1.length);

        process.exit(0);
    } catch (e) {
        console.error('❌ Error during check:', e);
        process.exit(1);
    }
}

check();
