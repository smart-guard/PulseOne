const request = require('supertest');
const express = require('express');
const KnexManager = require('../lib/database/KnexManager');
const ConfigManager = require('../lib/config/ConfigManager');
const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');
const manufacturerRoutes = require('../routes/manufacturers');

describe('Manufacturer CRUD Integration Test', () => {
    let app;
    let factory;
    let knex;

    beforeAll(async () => {
        // Config forced to in-memory sqlite
        ConfigManager.set('DATABASE_TYPE', 'sqlite');
        ConfigManager.set('SQLITE_PATH', ':memory:');

        app = express();
        app.use(express.json());

        // Get singletons
        factory = RepositoryFactory.getInstance();

        await factory.initialize({
            database: { type: 'sqlite', filename: ':memory:' },
            logger: { enabled: false }
        });

        const km = KnexManager.getInstance();
        knex = km.getKnex('sqlite');

        if (!knex) {
            throw new Error('Knex instance failed to initialize');
        }

        // Setup schema
        await knex.schema.dropTableIfExists('device_models');
        await knex.schema.dropTableIfExists('manufacturers');

        await knex.schema.createTable('manufacturers', (table) => {
            table.increments('id').primary();
            table.string('name').notNullable();
            table.text('description');
            table.string('country');
            table.string('website');
            table.integer('is_active').defaultTo(1);
            table.timestamps(true, true);
        });

        await knex.schema.createTable('device_models', (table) => {
            table.increments('id').primary();
            table.integer('manufacturer_id').notNullable();
            table.string('name');
            table.integer('is_enabled').defaultTo(1);
        });

        // Seed initial data
        await knex('manufacturers').insert([
            { id: 1, name: 'Mitsubishi Electric', description: 'Industrial automation leader', country: 'Japan', is_active: 1 },
            { id: 2, name: 'Siemens AG', description: 'Global conglomerate', country: 'Germany', is_active: 1 }
        ]);

        app.use('/api/manufacturers', manufacturerRoutes);
    });

    afterAll(async () => {
        if (factory) {
            await factory.cleanup();
        }
    });

    test('✅ GET /api/manufacturers - 목록 조회 검증', async () => {
        const response = await request(app)
            .get('/api/manufacturers')
            .expect(200);

        expect(response.body.success).toBe(true);
        expect(response.body.data.items.length).toBeGreaterThanOrEqual(2);
        expect(response.body.data.items[0].name).toBe('Mitsubishi Electric');
    });

    test('✅ POST /api/manufacturers - 신규 제조사 생성 검증', async () => {
        const uniqueName = `LS Electric ${Date.now()}`;
        const newManufacturer = {
            name: uniqueName,
            description: 'South Korean electric industrial company',
            country: 'South Korea'
        };

        const response = await request(app)
            .post('/api/manufacturers')
            .send(newManufacturer)
            .expect(201);

        expect(response.body.success).toBe(true);
        expect(response.body.data.name).toBe(uniqueName);

        const inserted = await knex('manufacturers').where('name', uniqueName).first();
        expect(inserted).toBeDefined();
        expect(inserted.country).toBe('South Korea');
    });

    test('✅ PUT /api/manufacturers/:id - 제조사 정보 수정 검증', async () => {
        const updateData = {
            description: 'Updated Mitsubishi description',
            is_active: false
        };

        const response = await request(app)
            .put('/api/manufacturers/1')
            .send(updateData)
            .expect(200);

        expect(response.body.success).toBe(true);
        expect(response.body.data.description).toBe('Updated Mitsubishi description');
        expect(response.body.data.is_active).toBe(false);

        const updated = await knex('manufacturers').where('id', 1).first();
        expect(updated.description).toBe('Updated Mitsubishi description');
        expect(updated.is_active).toBe(0);
    });

    test('✅ DELETE /api/manufacturers/:id - 제조사 삭제 검증', async () => {
        // 2번(Siemens) 삭제
        const response = await request(app)
            .delete('/api/manufacturers/2')
            .expect(200);

        expect(response.body.success).toBe(true);

        const deleted = await knex('manufacturers').where('id', 2).first();
        expect(deleted).toBeUndefined();
    });

    test('✅ DELETE /api/manufacturers/:id - 모델이 있는 제조사 삭제 방지 검증', async () => {
        // 이미 1번(Mitsubishi)은 존재함. 여기에 모델 하나 추가
        await knex('device_models').insert({ manufacturer_id: 1, name: 'FX5U' });

        const response = await request(app)
            .delete('/api/manufacturers/1')
            .expect(400);

        expect(response.body.success).toBe(false);
        expect(response.body.message).toContain('연결된');
    });
});
