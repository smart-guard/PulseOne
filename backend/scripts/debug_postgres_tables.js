const { Client } = require('pg');

const client = new Client({
    user: 'pulseone',
    host: 'localhost',
    database: 'pulseone',
    password: 'yourpassword',
    port: 5432,
});

async function listTables() {
    try {
        await client.connect();
        console.log('Connected to PostgreSQL');

        const res = await client.query(`
      SELECT table_name 
      FROM information_schema.tables 
      WHERE table_schema = 'public'
      ORDER BY table_name;
    `);

        console.log('Tables found:', res.rows.map(r => r.table_name));

    } catch (err) {
        console.error('Error:', err);
    } finally {
        await client.end();
    }
}

listTables();
