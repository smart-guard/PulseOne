const axios = require('axios');

const BASE_URL = 'http://localhost:3000/api/system';
const AUTH_HEADER = { Authorization: 'Bearer dev-dummy-token' };

async function cleanup() {
    console.log('🧹 Starting Verification Role Cleanup...');
    try {
        // 1. Fetch all roles
        const res = await axios.get(`${BASE_URL}/roles`, { headers: AUTH_HEADER });
        const roles = res.data.data;

        // 2. Filter for test roles
        const targets = roles.filter(r =>
            r.name === 'Verification Role' ||
            r.name === 'Verification Role Updated' ||
            r.description === 'Role created by verification script'
        );

        if (targets.length === 0) {
            console.log('✅ No verification roles found to delete.');
            return;
        }

        console.log(`🔍 Found ${targets.length} test roles. Deleting...`);

        // 3. Delete each target
        for (const role of targets) {
            try {
                process.stdout.write(`Deleting [${role.name}] (${role.id})... `);
                await axios.delete(`${BASE_URL}/roles/${role.id}`, { headers: AUTH_HEADER });
                console.log('OK');
            } catch (delErr) {
                console.log('FAILED');
                console.error(`  Error: ${delErr.message}`);
            }
        }

        console.log('\n✨ Cleanup complete.');

    } catch (err) {
        console.error('\n❌ Cleanup Failed:', err.message);
        if (err.response) {
            console.error('Response Status:', err.response.status);
        }
        process.exit(1);
    }
}

cleanup();
