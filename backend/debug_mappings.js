const knex = require('knex')({
    client: 'sqlite3',
    connection: { filename: '/app/data/db/pulseone.db' },
    useNullAsDefault: true
});

async function run() {
    try {
        console.log('--- Checking Export Target Mappings ---');
        // Find the "Verification Target" ID first
        const target = await knex('export_targets')
            .where('name', 'like', '%Verification Target%')
            .orWhere('name', 'like', '%Insite%')
            .first();

        if (target) {
            console.log('Target Found:', { id: target.id, name: target.name });

            const mappings = await knex('export_target_mappings')
                .where('target_id', target.id)
                .select('*');

            console.log(`Found ${mappings.length} mappings for this target:`);
            if (mappings.length > 0) {
                console.log(mappings);
            } else {
                console.log('NO MAPPINGS FOUND.');
            }
        } else {
            console.log('Target NOT Found. Listing all targets:');
            const targets = await knex('export_targets').select('id', 'name');
            console.log(targets);
        }

    } catch (err) {
        console.error('Error:', err);
    } finally {
        await knex.destroy();
    }
}

run();
