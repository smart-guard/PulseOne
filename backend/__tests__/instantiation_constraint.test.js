
const knex = require('knex');
const KnexManager = require('../lib/database/KnexManager');
const TemplateDeviceService = require('../lib/services/TemplateDeviceService');
const ConfigManager = require('../lib/config/ConfigManager');

// Mock dependencies
class MockAuditLogService {
    async logAction() { }
    async logChange() { }
}

class MockProtocolRepository {
    async findById(id) {
        return {
            id: id,
            protocol_type: 'MODBUS_TCP',
            is_enabled: 1
        };
    }
}

describe('Device Instantiation Test', () => {
    let knexInstance;
    let service;
    let createdDeviceId;
    let templateId;

    beforeAll(async () => {
        // Initialize real database connection
        const configManager = ConfigManager.getInstance();
        // Force SQLite for testing
        process.env.DATABASE_TYPE = 'SQLITE';
        process.env.SQLITE_DB_PATH = '/app/data/db/pulseone.db'; // Assuming docker path from previous context

        knexInstance = KnexManager.getInstance().getKnex('sqlite');

        service = new TemplateDeviceService();
        // Inject mocks
        service.auditLogService = new MockAuditLogService();
        service.protocolRepo = new MockProtocolRepository();
    });

    afterAll(async () => {
        if (createdDeviceId) {
            await knexInstance('devices').where('id', createdDeviceId).del();
            await knexInstance('device_settings').where('device_id', createdDeviceId).del();
            await knexInstance('data_points').where('device_id', createdDeviceId).del();
        }
        if (templateId) {
            await knexInstance('template_devices').where('id', templateId).del();
            await knexInstance('template_data_points').where('template_device_id', templateId).del();
        }
        await KnexManager.getInstance().destroyAll();
    });

    test('should instantiate device and normalize BOOLEAN to BOOL', async () => {
        // 1. Create a dummy template with BOOLEAN type
        const [tid] = await knexInstance('template_devices').insert({
            name: 'Test Constraint Template',
            protocol_id: 1, // Modbus
            config: JSON.stringify({ slave_id: 1 }),
            created_by: 1,
            tenant_id: 1 // Assuming tenant 1 exists
        });
        templateId = tid;

        await knexInstance('template_data_points').insert([
            {
                template_device_id: templateId,
                name: 'Test Boolean Point',
                address: 100,
                // THIS IS THE OFFENDER: 'BOOLEAN'
                data_type: 'BOOLEAN',
                access_mode: 'read',
                is_writable: 0
            },
            {
                template_device_id: templateId,
                name: 'Test Valid Point',
                address: 101,
                data_type: 'UINT16',
                access_mode: 'read',
                is_writable: 0
            }
        ]);

        // 2. Instantiate data
        const instantiateData = {
            name: 'Test Instantiated Device',
            site_id: 1, // Assuming site 1 exists
            endpoint: '127.0.0.1:502',
            config: { slave_id: 99 }
        };

        // 3. Perform Instantiation
        console.log("Attempting instantiation...");
        const result = await service.instantiateFromTemplate(templateId, instantiateData, 1);
        createdDeviceId = result.device_id;

        expect(result).toBeDefined();
        expect(result.device_id).toBeGreaterThan(0);
        console.log(`Device created with ID: ${createdDeviceId}`);

        // 4. Verify data_points
        const points = await knexInstance('data_points').where('device_id', createdDeviceId);
        expect(points.length).toBe(2);

        const boolPoint = points.find(p => p.address === 100);
        const uintPoint = points.find(p => p.address === 101);

        // CHECK: Boolean point should be normalized to 'BOOL'
        console.log(`Point 100 type: ${boolPoint.data_type}`);
        expect(boolPoint.data_type).toBe('BOOL');

        // CHECK: UINT16 point should remain 'UINT16'
        console.log(`Point 101 type: ${uintPoint.data_type}`);
        expect(uintPoint.data_type).toBe('UINT16');

    });
});
