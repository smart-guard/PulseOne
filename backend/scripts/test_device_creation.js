const TemplateDeviceService = require('../lib/services/TemplateDeviceService');
const SiteRepository = require('../lib/database/repositories/SiteRepository');
const ConfigManager = require('../lib/config/ConfigManager');
const sqliteConnection = require('../lib/connection/sqlite');

async function testCreation() {
    try {
        const config = ConfigManager.getInstance();
        console.log('DB Path:', config.getDatabaseConfig().sqlite.path);

        await sqliteConnection.connect();

        const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');
        await RepositoryFactory.getInstance().initialize(config.getDatabaseConfig());

        const service = new TemplateDeviceService();
        const siteRepo = new SiteRepository();

        // 1. Get a valid site
        const sites = await siteRepo.findAll();
        let siteId;
        if (sites.length > 0) {
            siteId = sites[0].id;
        } else {
            console.log('No sites found, creating one...');
            const newSite = await siteRepo.create({ name: 'Test Site', tenant_id: 1 });
            siteId = newSite.id;
        }
        console.log('Using Site ID:', siteId);

        // 2. Get a template (e.g., ID 3 - PM8000)
        // 2. Get a template (e.g., ID 3 - PM8000)
        const templateRes = await service.getTemplateDetail(3);
        const template = templateRes.data;
        if (!template) throw new Error('Template not found');
        console.log(`Template: ${template.name}, Points: ${template.data_points.length}`);

        // 3. Create device
        const deviceData = {
            name: 'Test Generated Device ' + Date.now(),
            site_id: siteId,
            endpoint: '192.168.1.100',
            description: 'Generated from test script'
        };

        const createRes = await service.instantiateFromTemplate(3, deviceData, 1);
        console.log('Create Response:', JSON.stringify(createRes, null, 2));

        if (!createRes.success) {
            throw new Error(createRes.message);
        }

        const result = createRes.data;
        console.log('Device Created:', result);

        // 4. Verify points
        const deviceRepo = RepositoryFactory.getInstance().getDeviceRepository();
        const points = await deviceRepo.getDataPointsByDevice(result.device_id);

        console.log(`Created Device Points: ${points.length}`);

        if (points.length === template.data_points.length) {
            console.log('SUCCESS: Point count matches.');
        } else {
            console.error('FAILURE: Point count mismatch.');
        }

    } catch (error) {
        console.error('Error:', error);
    } finally {
        await sqliteConnection.close();
    }
}

testCreation();
