const { Client } = require('pg');

const client = new Client({
    user: 'pulseone',
    host: 'localhost',
    database: 'pulseone',
    password: 'yourpassword',
    port: 5432,
});

async function migrate() {
    try {
        await client.connect();
        console.log('Connected to PostgreSQL');

        // Check if table exists
        const res = await client.query("SELECT to_regclass('public.template_devices');");
        if (!res.rows[0].to_regclass) {
            console.error('Table template_devices does not exist!');
            // Optional: Create table if missing?? No, that might be dangerous.
            // But purely for soft delete, we assume it should exist.
            return;
        }

        // Check if column exists
        const colRes = await client.query(`
      SELECT column_name 
      FROM information_schema.columns 
      WHERE table_name='template_devices' AND column_name='is_deleted';
    `);

        if (colRes.rows.length > 0) {
            console.log('Column is_deleted already exists.');
        } else {
            console.log('Adding is_deleted column...');
            await client.query("ALTER TABLE template_devices ADD COLUMN is_deleted SMALLINT DEFAULT 0;");
            console.log('Column added successfully.');
        }

    } catch (err) {
        console.error('Migration error:', err);
    } finally {
        await client.end();
    }
}

migrate();
