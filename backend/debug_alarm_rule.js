const knex = require('knex')({
    client: 'sqlite3',
    connection: { filename: '/app/data/db/pulseone.db' },
    useNullAsDefault: true
});

async function run() {
    try {
        console.log('--- Searching for Alarm Rule: TEST-MODBUS-INSITE ---');
        const rules = await knex('alarm_rules').select('id', 'name', 'is_enabled');
        console.log('All Rules:', rules);

    } catch (err) {
        console.error('Error:', err);
    } finally {
        await knex.destroy();
    }
}

run();
