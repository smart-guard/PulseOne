const RepositoryFactory = require('./lib/database/repositories/RepositoryFactory');

async function testCRUD() {
    try {
        console.log('--- Master Model CRUD Backend Test ---');

        const factory = RepositoryFactory.getInstance();
        await factory.initialize();

        const repo = factory.getTemplateDeviceRepository();
        const pointRepo = factory.getTemplateDataPointRepository();

        // 1. Create
        console.log('\n[1] Creating Test Template...');
        const newTemplate = await repo.create({
            name: 'CRUD_TEST_TEMPLATE',
            manufacturer_id: 1, // assumes exists
            model_id: 1,        // assumes exists
            device_type: 'PLC',
            protocol_id: 1,
            polling_interval: 1000,
            timeout: 3000,
            version: '1.0.0',
            is_public: 1
        });
        console.log('Created Template ID:', newTemplate.id);

        // 2. Add Point
        console.log('\n[2] Adding Data Point...');
        const newPoint = await pointRepo.create({
            template_device_id: newTemplate.id,
            name: 'TEST_POINT',
            address: 100,
            data_type: 'UINT16',
            access_mode: 'read'
        });
        console.log('Created Point ID:', newPoint.id);

        // 3. Read Detail
        console.log('\n[3] Reading Detail...');
        const detail = await repo.findById(newTemplate.id);
        console.log('Template Name:', detail.name);
        const points = await pointRepo.findByTemplateId(newTemplate.id);
        console.log('Point Count:', points.length);

        // 4. Update
        console.log('\n[4] Updating Template...');
        await repo.update(newTemplate.id, { description: 'Updated Description' });
        const updated = await repo.findById(newTemplate.id);
        console.log('Updated Description:', updated.description);

        // 5. Delete
        console.log('\n[5] Deleting Template & Points...');
        await pointRepo.deleteByTemplateId(newTemplate.id);
        const delSuccess = await repo.deleteById(newTemplate.id);
        console.log('Delete Success:', delSuccess);

        const finalCheck = await repo.findById(newTemplate.id);
        console.log('Final Check (should be null):', finalCheck === null);

        console.log('\n--- Backend CRUD Test Passed ---');
        process.exit(0);
    } catch (err) {
        console.error('\n--- Backend CRUD Test Failed ---');
        console.error(err);
        process.exit(1);
    }
}

testCRUD();
