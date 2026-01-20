const RepositoryFactory = require('./backend/lib/database/repositories/RepositoryFactory');
const ConfigManager = require('./backend/lib/config/ConfigManager');

async function check() {
    try {
        const repo = RepositoryFactory.getInstance().getRepository('DeviceRepository');
        console.log('Testing searchDataPoints(null, "")...');
        const points = await repo.searchDataPoints(null, '');
        console.log('Found points:', points.length);
        if (points.length > 0) {
            console.log('Sample point:', points[0]);
        }

        console.log('\nTesting searchDataPoints(1, "")...');
        const points1 = await repo.searchDataPoints(1, '');
        console.log('Found points for tenant 1:', points1.length);

        console.log('\nTesting devices findAll(null)...');
        const devices = await repo.findAll(null, { limit: 10 });
        const deviceItems = Array.isArray(devices) ? devices : devices.items;
        console.log('Found devices:', deviceItems.length);
        if (deviceItems.length > 0) {
            console.log('Sample device:', deviceItems[0]);
        }

        process.exit(0);
    } catch (e) {
        console.error(e);
        process.exit(1);
    }
}

check();
