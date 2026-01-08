// =============================================================================
// backend/__tests__/user.service.test.js
// UserService unit tests
// =============================================================================

const UserService = require('../lib/services/UserService');
const KnexManager = require('../lib/database/KnexManager');
const bcrypt = require('bcryptjs');

describe('UserService Unit Tests', () => {
    let knex;

    beforeAll(async () => {
        // Ensure we are using SQLite in-memory for testing
        process.env.DATABASE_TYPE = 'sqlite';

        knex = KnexManager.getInstance().getKnex('sqlite');

        // Drop tables if they exist to ensure a clean state
        await knex.schema.dropTableIfExists('users');
        await knex.schema.dropTableIfExists('tenants');

        // Create necessary tables for testing
        await knex.schema.createTable('tenants', (table) => {
            table.increments('id').primary();
            table.string('company_name');
            table.string('company_code');
            table.string('domain');
            table.string('contact_email');
            table.string('subscription_plan');
            table.string('subscription_status');
            table.integer('is_active');
            table.timestamps(true, true);
        });

        await knex.schema.createTable('users', (table) => {
            table.increments('id').primary();
            table.integer('tenant_id').references('id').inTable('tenants');
            table.string('username').unique().notNullable();
            table.string('email').notNullable();
            table.string('password_hash').notNullable();
            table.string('full_name');
            table.string('department');
            table.string('role').defaultTo('viewer');
            table.text('permissions'); // SQLite text for JSON
            table.text('site_access');
            table.text('device_access');
            table.integer('is_active').defaultTo(1);
            table.datetime('last_login');
            table.string('password_reset_token');
            table.datetime('password_reset_expires');
            table.timestamps(true, true);
        });

        // Insert dummy tenant
        await knex('tenants').insert({
            id: 1,
            company_name: 'Test Tenant',
            company_code: 'TEST',
            domain: 'test.com',
            contact_email: 'test@test.com',
            subscription_plan: 'pro',
            subscription_status: 'active',
            is_active: 1
        });
    });

    afterAll(async () => {
        await KnexManager.getInstance().destroyAll();
    });

    describe('User CRUD Operations', () => {
        it('should create a new user with hashed password', async () => {
            const userData = {
                username: 'admin_test',
                password: 'password123',
                email: 'admin@test.com',
                role: 'admin',
                full_name: 'Admin User'
            };

            const response = await UserService.createUser(userData, 1);

            expect(response.success).toBe(true);
            expect(response.data.id).toBeDefined();
            expect(response.data.username).toBe('admin_test');

            // Verify password hashing
            const userInDb = await knex('users').where('id', response.data.id).first();
            const isPasswordMatch = await bcrypt.compare('password123', userInDb.password_hash);
            expect(isPasswordMatch).toBe(true);
        });

        it('should not create a user with duplicate username', async () => {
            const userData = {
                username: 'admin_test',
                password: 'newpassword',
                email: 'other@test.com'
            };

            const response = await UserService.createUser(userData, 1);
            expect(response.success).toBe(false);
            expect(response.message).toContain('이미 존재하는 사용자명입니다');
        });

        it('should get all users for a tenant and sanitize sensitive data', async () => {
            const response = await UserService.getAllUsers(1);

            expect(response.success).toBe(true);
            expect(response.data.length).toBeGreaterThan(0);

            const user = response.data.find(u => u.username === 'admin_test');
            expect(user).toBeDefined();
            expect(user.password_hash).toBeUndefined();
            expect(user.password_reset_token).toBeUndefined();
        });

        it('should get a user by ID', async () => {
            const users = await knex('users').select('id').where('username', 'admin_test');
            const userId = users[0].id;

            const response = await UserService.getUserById(userId, 1);

            expect(response.success).toBe(true);
            expect(response.data.username).toBe('admin_test');
        });

        it('should update user information', async () => {
            const users = await knex('users').select('id').where('username', 'admin_test');
            const userId = users[0].id;

            const updateData = {
                username: 'admin_updated',
                email: 'updated@test.com',
                role: 'admin',
                is_active: true
            };

            const response = await UserService.updateUser(userId, updateData, 1);
            expect(response.success).toBe(true);

            const updatedUser = await UserService.getUserById(userId, 1);
            expect(updatedUser.data.username).toBe('admin_updated');
            expect(updatedUser.data.email).toBe('updated@test.com');
        });

        it('should patch user password', async () => {
            const users = await knex('users').select('id').where('username', 'admin_updated');
            const userId = users[0].id;

            const patchData = {
                password: 'new_secret_password'
            };

            const response = await UserService.patchUser(userId, patchData, 1);
            expect(response.success).toBe(true);

            const userInDb = await knex('users').where('id', userId).first();
            const isPasswordMatch = await bcrypt.compare('new_secret_password', userInDb.password_hash);
            expect(isPasswordMatch).toBe(true);
        });

        it('should delete a user', async () => {
            const users = await knex('users').select('id').where('username', 'admin_updated');
            const userId = users[0].id;

            const response = await UserService.deleteUser(userId, 1);
            expect(response.success).toBe(true);

            const checkResponse = await UserService.getUserById(userId, 1);
            expect(checkResponse.success).toBe(false);
        });
    });

    describe('Tenant Isolation', () => {
        it('should not allow access to users from another tenant', async () => {
            // Create user in tenant 1
            const user1Data = {
                username: 'tenant1_user',
                password: 'password',
                email: 't1@test.com'
            };
            const createRes = await UserService.createUser(user1Data, 1);
            const userId = createRes.data.id;

            // Try to access from tenant 2
            const response = await UserService.getUserById(userId, 2);
            expect(response.success).toBe(false);
            expect(response.message).toContain('사용자를 찾을 수 없습니다');
        });

        it('should allow cross-tenant access when tenantId is null (System Admin case)', async () => {
            // Create user in tenant 3
            await knex('tenants').insert({
                id: 3,
                company_name: 'Tenant 3',
                is_active: 1
            });

            const user3Data = {
                username: 'tenant3_user',
                password: 'password',
                email: 't3@test.com'
            };
            await UserService.createUser(user3Data, 3);

            // Fetch with tenantId = null
            const response = await UserService.getAllUsers(null);
            expect(response.success).toBe(true);

            const usernames = response.data.map(u => u.username);
            expect(usernames).toContain('tenant3_user');
            expect(usernames).toContain('tenant1_user');
        });
    });
});
