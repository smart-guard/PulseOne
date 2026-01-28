const DatabaseFactory = require('./lib/database/DatabaseFactory');
const path = require('path');

// Inside container, SQLite is at /app/data/db/pulseone.db
const dbPath = '/app/data/db/pulseone.db';
console.log('Checking DB at:', dbPath);

const dbConfig = {
    database: { type: 'sqlite' },
    sqlite: { path: dbPath }
};

async function check() {
    try {
        const db = DatabaseFactory.getInstance(dbConfig);
        const conn = await db.getMainConnection();

        console.log('--- Schema: export_profile_assignments ---');
        const schema = await conn.all("PRAGMA table_info(export_profile_assignments)");
        console.table(schema);

        console.log('--- Indexes: export_profile_assignments ---');
        const indexes = await conn.all("PRAGMA index_list(export_profile_assignments)");
        console.table(indexes);

        console.log('--- Data: export_profile_assignments ---');
        const rows = await conn.all("SELECT * FROM export_profile_assignments");
        console.log(JSON.stringify(rows, null, 2));

        console.log('--- Test Query directly from Repo logic ---');
        const profileId = 1; // Example profile ID from Default Profile
        const gatewayId = 2; // Example gateway ID from Local Export Gateway
        const query = `SELECT id, is_active FROM export_profile_assignments WHERE profile_id = ? AND gateway_id = ?`;
        const result = await conn.all(query, [profileId, gatewayId]);
        console.log('Result for P1, G2:', result);
        console.log('Result type:', typeof result);
        console.log('Is Array?', Array.isArray(result));

    } catch (e) {
        console.error(e);
    }
}

check();
