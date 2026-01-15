const axios = require('axios');

const BASE_URL = 'http://localhost:3000/api/system';
const AUTH_HEADER = { Authorization: 'Bearer dev-dummy-token' }; // System Admin

async function runTest() {
    console.log('🚀 Starting RBAC Lifecycle Verification...');
    let createdRoleId;

    try {
        // 1. Get Permissions
        console.log('\nStep 1: Fetching System Permissions...');
        const permRes = await axios.get(`${BASE_URL}/permissions`, { headers: AUTH_HEADER });
        const permissions = permRes.data.data;
        console.log(`✅ Fetched ${permissions.length} permissions.`);
        if (permissions.length === 0) throw new Error('No permissions found!');

        const firstPermId = permissions[0].id;
        const secondPermId = permissions[1].id;

        // 2. Create Role
        console.log('\nStep 2: Creating New Role...');
        const newRole = {
            name: 'Verification Role',
            description: 'Role created by verification script',
            permissions: [firstPermId]
        };
        const createRes = await axios.post(`${BASE_URL}/roles`, newRole, { headers: AUTH_HEADER });
        const createdRole = createRes.data.data;
        createdRoleId = createdRole.id;
        console.log(`✅ Role Created: ${createdRole.name} (ID: ${createdRoleId})`);

        // 3. Verify Role Details
        console.log('\nStep 3: Verifying Role Details...');
        const getRes = await axios.get(`${BASE_URL}/roles/${createdRoleId}`, { headers: AUTH_HEADER });
        const fetchedRole = getRes.data.data;

        if (fetchedRole.name !== newRole.name) throw new Error('Role name mismatch');
        if (!fetchedRole.permissions.includes(firstPermId)) throw new Error('Permission assignment failed');
        console.log('✅ Role Details Verified.');

        // 4. Update Role
        console.log('\nStep 4: Updating Role...');
        const updateData = {
            name: 'Verification Role Updated',
            description: 'Updated description',
            permissions: [firstPermId, secondPermId]
        };
        const updateRes = await axios.put(`${BASE_URL}/roles/${createdRoleId}`, updateData, { headers: AUTH_HEADER });
        const updatedRole = updateRes.data.data;

        if (updatedRole.name !== updateData.name) throw new Error('Role update name mismatch');

        // Verify permissions again
        const getUpdatedRes = await axios.get(`${BASE_URL}/roles/${createdRoleId}`, { headers: AUTH_HEADER });
        const finalRole = getUpdatedRes.data.data;
        if (finalRole.permissions.length !== 2) throw new Error('Permission update failed count');
        console.log('✅ Role Update Verified.');

        // 5. Delete Role
        console.log('\nStep 5: Deleting Role...');
        await axios.delete(`${BASE_URL}/roles/${createdRoleId}`, { headers: AUTH_HEADER });

        try {
            await axios.get(`${BASE_URL}/roles/${createdRoleId}`, { headers: AUTH_HEADER });
            throw new Error('Role still exists after delete');
        } catch (e) {
            if (e.response && e.response.status === 404) {
                console.log('✅ Role Deleted Successfully (404 on fetch).');
            } else {
                throw e;
            }
        }

        console.log('\n🎉 ALL RBAC VERIFICATION STEPS PASSED!');
    } catch (error) {
        console.error('\n❌ Test Failed:', error.message);
        if (error.response) {
            console.error('Response Status:', error.response.status);
            console.error('Response Data:', error.response.data);
        }
        process.exit(1);
    }
}

runTest();
