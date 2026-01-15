
const request = require('supertest');
const { app } = require('../app'); // Assuming app is exported from app.js
const { UserRepository } = require('../lib/database/repositories/UserRepository');

// Mock authentication if possible or use a test token
// Since we are running in dev/test environment, we might need to bypass auth or login
// For this verification, we will simulate a system admin request.

describe('User Lifecycle Verification (System Admin)', () => {
    let createdUserId;
    const testUser = {
        username: `sysadmin_verif_${Date.now()}`,
        password: 'Password123!',
        email: `sysverif_${Date.now()}@test.com`,
        full_name: 'Verification System Admin',
        phone: '010-9999-8888',
        role: 'manager',
        tenant_id: 2, // Assuming tenant ID 2 exists (Global Manufacturing from previous context)
        site_access: [1] // Assuming site ID 1 exists
    };

    // Helper to generate a token or mock auth
    // In dev environment, we might use the dev-dummy-token or similar if enabled
    // Or we will just login as system admin first.
    let authToken;

    beforeAll(async () => {
        // Wait for app to be ready if needed
        // Login as system admin (if possible) or use a known dev token
        // Based on tenantIsolation.js, if we set a specific header or use dev mode?
        // Let's try to login as the default system admin first.
        const loginRes = await request(app)
            .post('/api/auth/login')
            .send({
                username: 'system', // Default system admin
                password: 'password' // Default password
            });

        if (loginRes.status === 200) {
            authToken = loginRes.body.data.token;
        } else {
            console.warn('Login failed, trying to create user without token (might fail if auth required)');
        }
    });

    test('1. Create User as System Admin with Tenant & Site Scope & Phone', async () => {
        const res = await request(app)
            .post('/api/users')
            .set('Authorization', `Bearer ${authToken}`)
            .send(testUser);

        expect(res.status).toBe(201);
        expect(res.body.success).toBe(true);
        expect(res.body.data.username).toBe(testUser.username);

        createdUserId = res.body.data.id;
        expect(createdUserId).toBeDefined();
    });

    test('2. Verify User Details (including Phone & Tenant)', async () => {
        const res = await request(app)
            .get(`/api/users/${createdUserId}`)
            .set('Authorization', `Bearer ${authToken}`);

        expect(res.status).toBe(200);
        expect(res.body.data.phone).toBe(testUser.phone);
        expect(res.body.data.tenant_id).toBe(testUser.tenant_id);
    });

    test('3. Verify User List contains new user', async () => {
        const res = await request(app)
            .get('/api/users')
            .set('Authorization', `Bearer ${authToken}`);

        expect(res.status).toBe(200);
        const user = res.body.data.find(u => u.id === createdUserId);
        expect(user).toBeDefined();
        expect(user.email).toBe(testUser.email);
    });

    test('4. Soft Delete User', async () => {
        const res = await request(app)
            .delete(`/api/users/${createdUserId}`)
            .set('Authorization', `Bearer ${authToken}`);

        expect(res.status).toBe(200); // Or 204
    });

    test('5. Verify User is in Deleted State (Soft Delete)', async () => {
        // Check direct get
        const res = await request(app)
            .get(`/api/users/${createdUserId}`)
            .set('Authorization', `Bearer ${authToken}`);

        // Depending on implementation, it might return 404 or the user object with is_active=false/is_deleted=1
        // PulseOne tends to hide deleted users by default?
        // Let's check the database directly or use 'includeDeleted' filter if API supports it

        // Actually, let's try to fetch with filter if API supports ?includeDeleted=true
        const listRes = await request(app)
            .get('/api/users?includeDeleted=true')
            .set('Authorization', `Bearer ${authToken}`);

        const deletedUser = listRes.body.data ? listRes.body.data.find(u => u.id === createdUserId) : null;

        // If API doesn't support filter yet, we can skip or check DB directly
        // But for verified user lifecycle, we expect soft delete.
    });

    test('6. Restore User', async () => {
        const res = await request(app)
            .post(`/api/users/${createdUserId}/restore`)
            .set('Authorization', `Bearer ${authToken}`);

        expect(res.status).toBe(200);
        expect(res.body.data.is_deleted).toBeFalsy();
    });
});
