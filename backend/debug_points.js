const knex = require('knex')({
    client: 'sqlite3',
    connection: { filename: '/app/data/db/pulseone.db' },
    useNullAsDefault: true
});

async function run() {
    try {
        console.log('--- Searching for Data Points: TEST-MODBUS-INSITE ---');
        const points = await knex('data_points')
            .where('name', 'like', '%TEST-MODBUS%')
            .orWhere('name', 'like', '%WLS%') // Based on user image WLS.PV etc
            .select('id', 'name', 'address', 'device_id', 'description');

        console.log('Found Points:', points);

        if (points.length > 0) {
            const pointIds = points.map(p => p.id);
            console.log('Checking Alarm Rules for these Point IDs:', pointIds);

            const rules = await knex('alarm_rules')
                .whereIn('target_id', pointIds)
                .andWhere('target_type', 'point')
                .select('*');

            console.log('Related Rules (by target_id):', rules);
        }

    } catch (err) {
        console.error('Error:', err);
    } finally {
        await knex.destroy();
    }
}

run();
