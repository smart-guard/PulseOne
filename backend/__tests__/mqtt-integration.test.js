const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');
const KnexManager = require('../lib/database/KnexManager');

describe('MQTT Repository Integration Tests', () => {
    let deviceRepo;
    let templateRepo;
    let knex;

    beforeAll(async () => {
        // Force live DB for verification
        const config = require('../lib/config/ConfigManager').getInstance();
        config.set('DATABASE_TYPE', 'sqlite');
        config.set('SQLITE_PATH', '/app/data/db/pulseone.db');
        config.set('SQLITE_PATH', '/app/data/db/pulseone.db');
        config.set('DB_PATH', '/app/data/db/pulseone.db');

        const factory = RepositoryFactory.getInstance();
        await factory.initialize();
        deviceRepo = factory.getDeviceRepository();
        templateRepo = factory.getTemplateDataPointRepository();
        knex = KnexManager.getInstance().getKnex();
    });

    describe('TemplateDataPointRepository', () => {
        it('should correctly save and retrieve MQTT-specific fields', async () => {
            const testTag = 'test-mqtt-' + Date.now();
            const data = {
                template_device_id: 1, // Assumes template 1 exists or is just a dummy
                name: 'MQTT Test Point',
                address: 0,
                address_string: 'sensors/temp',
                mapping_key: 'val',
                data_type: 'FLOAT32',
                protocol_params: { qos: 1, retained: true },
                metadata: { unit: 'C' }
            };

            const created = await templateRepo.create(data);
            expect(created).toBeDefined();
            expect(created.address_string).toBe('sensors/temp');
            expect(created.mapping_key).toBe('val');

            // protocol_params should be parsed if the repo handles hydration, 
            // or we check the raw if it doesn't.
            const fetched = await knex('template_data_points').where('id', created.id).first();
            const params = typeof fetched.protocol_params === 'string'
                ? JSON.parse(fetched.protocol_params)
                : fetched.protocol_params;

            expect(fetched.address_string).toBe('sensors/temp');
            expect(fetched.mapping_key).toBe('val');
            expect(params.qos).toBe(1);
            expect(params.retained).toBe(true);

            // Cleanup
            if (created && created.id) {
                await knex('template_data_points').where('id', created.id).del();
            }
        });
    });

    describe('DeviceRepository', () => {
        it('should support mapping_key in data points during device update', async () => {
            // 1. Create a dummy device if needed or use existing
            const device = await deviceRepo.findById(1, 1);
            if (!device) return; // Skip if no seed data

            const updateData = {
                data_points: [
                    {
                        name: 'MQTT Point 1',
                        address: 0,
                        address_string: 'topic/1',
                        mapping_key: 'key1',
                        data_type: 'FLOAT32',
                        protocol_params: { test: 'data' }
                    }
                ]
            };

            const updated = await deviceRepo.update(device.id, updateData, 1);
            const point = updated.data_points.find(p => p.address_string === 'topic/1');

            expect(point).toBeDefined();
            expect(point.mapping_key).toBe('key1');

            const params = typeof point.protocol_params === 'string'
                ? JSON.parse(point.protocol_params)
                : point.protocol_params;
            expect(params.test).toBe('data');
        });
    });
});
