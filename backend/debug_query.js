const knex = require('knex')({
    client: 'sqlite3',
    connection: { filename: '/app/data/db/pulseone.db' },
    useNullAsDefault: true
});

async function run() {
    try {
        console.log('--- Checking DB Content ---');
        const rows = await knex('edge_servers')
            .where('is_deleted', 0)
            .where('server_type', 'gateway')
            .select('*');
        console.log(`Found ${rows.length} rows:`);
        rows.forEach(r => console.log(`ID: ${r.id}, Tenant: ${r.tenant_id}, Name: ${r.server_name}, Type: ${r.server_type}`));

        console.log('\n--- Checking Count Query ---');
        const countResult = await knex('edge_servers')
            .where('is_deleted', 0)
            .where('server_type', 'gateway')
            .count('id as total')
            .first();
        console.log('Count Result:', countResult);

        const total = parseInt(countResult?.total || 0);
        console.log('Parsed Total:', total);

    } catch (err) {
        console.error('Error:', err);
    } finally {
        await knex.destroy();
    }
}

run();
