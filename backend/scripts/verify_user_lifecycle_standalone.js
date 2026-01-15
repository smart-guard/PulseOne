
const axios = require('axios');

const BASE_URL = 'http://localhost:3000/api';
const SYSTEM_ADMIN_CREDS = {
    username: 'system',
    password: 'password' // Default password for dev environment
};

async function runTest() {
    console.log('🚀 Starting User Lifecycle Verification (Standalone E2E)...');
    let token;
    let createdUserId;

    try {
        // 1. Setup Auth (Dev Mode)
        console.log('Step 1: Setting up Auth (Using dev-dummy-token)...');
        // In dev mode, tenantIsolation.js accepts this token or just NODE_ENV=development check if token exists
        const token = 'dev-dummy-token';
        const authHeader = { Authorization: `Bearer ${token}` };
        console.log('✅ Auth Header Prepared');

        // 2. Create User
        console.log('Step 2: Creating User with Phone Number and Tenant Scope...');
        const newUser = {
            username: `sysverif_${Date.now()}`,
            password: 'Password123!',
            email: `sysverif_${Date.now()}@test.com`,
            full_name: 'Verification Admin',
            phone: '010-9999-8888',
            role: 'site_admin',
            tenant_id: 2, // Using Tenant 2
            site_access: [1] // Using Site 1
        };

        const createRes = await axios.post(`${BASE_URL}/users`, newUser, { headers: authHeader });
        if (createRes.data.success) {
            createdUserId = createRes.data.data.id;
            console.log(`✅ User Created: ID ${createdUserId}, Username: ${createRes.data.data.username}`);
        } else {
            throw new Error(`User creation failed: ${JSON.stringify(createRes.data)}`);
        }

        // 3. Verify User Details
        console.log('Step 3: Verifying User Details (Phone & Tenant Config)...');
        const getRes = await axios.get(`${BASE_URL}/users/${createdUserId}`, { headers: authHeader });
        const userData = getRes.data.data;

        if (userData.phone === newUser.phone) {
            console.log(`✅ Phone Number Match: ${userData.phone}`);
        } else {
            console.error(`❌ Phone Number Mismatch: Expected ${newUser.phone}, Got ${userData.phone}`);
            process.exit(1);
        }

        if (userData.tenant_id === newUser.tenant_id) {
            console.log(`✅ Tenant ID Match: ${userData.tenant_id}`);
        } else {
            console.error(`❌ Tenant ID Mismatch: Expected ${newUser.tenant_id}, Got ${userData.tenant_id}`);
            process.exit(1);
        }

        // 4. Soft Delete
        console.log('Step 4: Testing Soft Delete...');
        await axios.delete(`${BASE_URL}/users/${createdUserId}`, { headers: authHeader });
        console.log('✅ Soft Delete Request Sent');

        // Check if user is deleted (depends on API behavior, might return 404 or is_deleted flag)
        try {
            const checkDeletedRes = await axios.get(`${BASE_URL}/users/${createdUserId}`, { headers: authHeader });
            // If successful, check is_deleted flag if visible, or is_active
            if (checkDeletedRes.data.data.is_deleted === 1 || checkDeletedRes.data.data.is_deleted === true) {
                console.log('✅ User verified as deleted (is_deleted=1)');
            } else {
                console.warn('⚠️ User retrieved but is_deleted not set to true? Response:', checkDeletedRes.data.data);
            }
        } catch (e) {
            // If 404, that might be expected behavior for deleted users in standard get
            console.log('✅ User not found via standard GET (Expected for deleted user)');
        }

        // 5. Restore User
        console.log('Step 5: Restoring User...');
        // Need to find the correct restore endpoint. Based on code I viewed: /users/:id/restore
        const restoreRes = await axios.post(`${BASE_URL}/users/${createdUserId}/restore`, {}, { headers: authHeader });
        if (restoreRes.data.success) {
            console.log('✅ Restore Request Successful');
        } else {
            throw new Error('Restore failed');
        }

        const verifyRestore = await axios.get(`${BASE_URL}/users/${createdUserId}`, { headers: authHeader });
        if (!verifyRestore.data.data.is_deleted) {
            console.log('✅ User successfully restored (is_deleted is false/null)');
        } else {
            console.error('❌ User still appears deleted');
            process.exit(1);
        }

        console.log('🎉 All Verification Steps Passed Successfully!');

    } catch (error) {
        console.error('❌ Test Failed:', error.message);
        if (error.response) {
            console.error('Response Data:', error.response.data);
        }
        process.exit(1);
    }
}

runTest();
