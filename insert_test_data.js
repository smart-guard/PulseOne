const DeviceRepository = require('./backend/lib/database/repositories/DeviceRepository');
const knex = require('knex');
const path = require('path');

const logger = {
    log: (...args) => console.log('LOG:', ...args),
    error: (...args) => console.error('ERROR:', ...args),
    info: (...args) => console.log('INFO:', ...args)
};

async function insertTestData() {
    const dbPath = '/app/data/db/pulseone.db';
    const db = knex({
        client: 'sqlite3',
        connection: { filename: dbPath },
        useNullAsDefault: true
    });

    const repo = new DeviceRepository(db, logger);
    const TARGET_DEVICE_ID = 1; // PLC-001

    try {
        console.log(`\n--- Inserting NIKE_FIRE Test Data (Device ID: ${TARGET_DEVICE_ID}) ---`);

        const testPoints = [
            {
                name: 'NIKE_FIRE_01_PV',
                address: 100,
                address_string: 'CO:100',
                data_type: 'BOOL',
                description: '1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 현재값 이상',
                access_mode: 'read'
            },
            {
                name: 'NIKE_FIRE_01_SRS',
                address: 101,
                address_string: 'CO:101',
                data_type: 'BOOL',
                description: '1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 센서 인식상태 이상',
                access_mode: 'read'
            },
            {
                name: 'NIKE_FIRE_01_SCS',
                address: 102,
                address_string: 'CO:102',
                data_type: 'BOOL',
                description: '1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 센서 통신상태 이상',
                access_mode: 'read'
            },
            {
                name: 'NIKE_FIRE_01_SSS',
                address: 200,
                address_string: 'HR:200',
                data_type: 'INT16',
                description: '1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 센서 신호강도 이상',
                access_mode: 'read'
            },
            {
                name: 'NIKE_FIRE_01_SBV',
                address: 201,
                address_string: 'HR:201',
                data_type: 'INT16',
                description: '1F 방재실 소방수신기 1번 접점 감지센서(VDC) 1번 무선 센서 배터리전압 이상',
                access_mode: 'read'
            }
        ];

        // We use repository.update logic to handle potential duplicates (Upsert)
        // Note: Repository.update expects { data_points: [...] }
        const updateData = {
            data_points: testPoints
        };

        console.log('Inserting/Updating via Repository...');
        await repo.update(TARGET_DEVICE_ID, updateData, 1);

        console.log('\n✅ Test data inserted successfully.');

        // Verify output
        const results = await db('data_points').where('device_id', TARGET_DEVICE_ID).where('name', 'like', 'NIKE_FIRE%');
        console.table(results.map(p => ({ Name: p.name, Addr: p.address_string, Type: p.data_type })));

    } catch (err) {
        console.error('Failed to insert test data:', err);
    } finally {
        await db.destroy();
    }
}

insertTestData();
