
const KnexManager = require('./lib/database/KnexManager');

// Initialize Knex
const knexManager = KnexManager.getInstance();
const knex = knexManager.getKnex('sqlite');

async function runMigration() {
    try {
        console.log('Using database:', knex.client.connectionSettings.filename);

        const hasColumn = await knex.schema.hasColumn('alarm_rule_templates', 'template_type');

        if (hasColumn) {
            console.log('✅ Column `template_type` already exists.');
        } else {
            console.log('🚧 Adding column `template_type`...');
            await knex.schema.table('alarm_rule_templates', table => {
                table.string('template_type', 20).defaultTo('simple');
            });
            console.log('✅ Column `template_type` added successfully.');
        }
    } catch (error) {
        console.error('❌ Migration failed:', error);
    } finally {
        await knex.destroy();
    }
}

runMigration();
