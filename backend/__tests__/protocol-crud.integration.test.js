// =============================================================================
// backend/__tests__/protocol-crud.integration.test.js
// 🎯 Protocol CRUD Integration Test
// =============================================================================

// 환경 변수 설정 (최상단)
process.env.DATABASE_TYPE = 'sqlite';
process.env.SQLITE_PATH = ':memory:';

const request = require('supertest');
const express = require('express');
const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');
const KnexManager = require('../lib/database/KnexManager');
const ConfigManager = require('../lib/config/ConfigManager');

// Mock middlewares
jest.mock('../middleware/tenantIsolation', () => ({
    authenticateToken: (req, res, next) => {
        req.user = { id: 1, tenant_id: 1 };
        next();
    },
    tenantIsolation: (req, res, next) => {
        req.tenantId = 1;
        next();
    },
    validateTenantStatus: (req, res, next) => next()
}));

// Route require must happen after environment is set
const protocolRoutes = require('../routes/protocols');

describe('Protocol CRUD Integration Test', () => {
    let app;
    let factory;
    let knex;

    beforeAll(async () => {
        // Config 강제 리로드
        ConfigManager.getInstance().set('DATABASE_TYPE', 'sqlite');
        ConfigManager.getInstance().set('SQLITE_PATH', ':memory:');

        // Express 앱 생성
        app = express();
        app.use(express.json());

        // RepositoryFactory 초기화 (In-memory SQLite)
        factory = RepositoryFactory.getInstance();
        await factory.initialize({
            database: { type: 'sqlite', filename: ':memory:' },
            logger: { enabled: false }
        });

        // Initialize schema for 'protocols' table
        const km = KnexManager.getInstance();
        knex = km.getKnex('sqlite');

        if (!knex) {
            throw new Error('Knex instance failed to initialize');
        }

        await knex.schema.dropTableIfExists('device_status');
        await knex.schema.dropTableIfExists('devices');
        await knex.schema.dropTableIfExists('protocols');

        await knex.schema.createTable('protocols', (table) => {
            table.increments('id').primary();
            table.string('protocol_type').notNullable();
            table.string('display_name');
            table.text('description');
            table.integer('default_port');
            table.integer('uses_serial').defaultTo(0);
            table.integer('requires_broker').defaultTo(0);
            table.text('supported_operations');
            table.text('supported_data_types');
            table.text('connection_params');
            table.integer('default_polling_interval').defaultTo(1000);
            table.integer('default_timeout').defaultTo(5000);
            table.integer('max_concurrent_connections').defaultTo(1);
            table.string('category').defaultTo('custom');
            table.string('vendor');
            table.string('standard_reference');
            table.integer('is_enabled').defaultTo(1);
            table.integer('is_deprecated').defaultTo(0);
            table.string('min_firmware_version');
            table.timestamps(true, true);
        });

        await knex.schema.createTable('devices', (table) => {
            table.increments('id').primary();
            table.integer('protocol_id').notNullable();
            table.integer('tenant_id').notNullable();
            table.integer('is_enabled').defaultTo(1);
        });

        await knex.schema.createTable('device_status', (table) => {
            table.integer('device_id').primary();
            table.string('connection_status').defaultTo('unknown');
        });

        // Seed some data
        await knex('protocols').insert([
            { id: 1, protocol_type: 'MODBUS_TCP', display_name: 'Modbus TCP', category: 'industrial' },
            { id: 2, protocol_type: 'MQTT', display_name: 'MQTT', category: 'iot' }
        ]);

        app.use('/api/protocols', protocolRoutes);
    });

    afterAll(async () => {
        if (factory) {
            await factory.cleanup();
        }
    });

    test('✅ POST /api/protocols - 새로운 프로토콜 생성 검증 (모든 필드 포함)', async () => {
        const fullProtocol = {
            protocol_type: 'FULL_TEST_PROTOCOL',
            display_name: 'Full Test Protocol',
            description: 'Testing all available fields',
            category: 'industrial',
            default_port: 1234,
            uses_serial: true,
            requires_broker: true,
            supported_operations: ['read', 'write', 'execute'],
            supported_data_types: ['int16', 'float32'],
            connection_params: { timeout: 3000, retry: 3 },
            default_polling_interval: 2000,
            default_timeout: 10000,
            max_concurrent_connections: 5,
            vendor: 'Test Vendor',
            standard_reference: 'IEEE 123.4',
            is_enabled: true,
            min_firmware_version: '1.2.3'
        };

        const response = await request(app)
            .post('/api/protocols')
            .send(fullProtocol)
            .expect(201);

        expect(response.body.success).toBe(true);
        const data = response.body.data;
        expect(data.protocol_type).toBe('FULL_TEST_PROTOCOL');
        expect(data.uses_serial).toBe(true);
        expect(data.requires_broker).toBe(true);
        expect(data.supported_operations).toContain('read');
        expect(data.connection_params.timeout).toBe(3000);

        // DB 검증
        const inserted = await knex('protocols').where('protocol_type', 'FULL_TEST_PROTOCOL').first();
        expect(inserted.default_port).toBe(1234);
        expect(inserted.uses_serial).toBe(1); // SQLite stores boolean as integer
        expect(inserted.requires_broker).toBe(1);

        // JSON 및 Array 필드는 문자열로 저장됨 (Repository에서 stringify)
        const ops = JSON.parse(inserted.supported_operations);
        expect(ops).toContain('write');

        const params = JSON.parse(inserted.connection_params);
        expect(params.retry).toBe(3);
    });

    test('✅ POST /api/protocols - 새로운 프로토콜 생성 검증', async () => {
        const newProtocol = {
            protocol_type: 'BACNET_IP',
            display_name: 'BACnet IP',
            description: 'Building Automation and Control networks',
            category: 'building_automation',
            default_port: 47808,
            is_enabled: true
        };

        const response = await request(app)
            .post('/api/protocols')
            .send(newProtocol)
            .expect(201);

        expect(response.body.success).toBe(true);
        expect(response.body.data.protocol_type).toBe('BACNET_IP');
        expect(response.body.data.display_name).toBe('BACnet IP');

        // DB에 잘 저장되었는지 확인
        const inserted = await knex('protocols').where('protocol_type', 'BACNET_IP').first();
        expect(inserted).toBeDefined();
        expect(inserted.category).toBe('building_automation');
    });

    test('✅ PUT /api/protocols/:id - 프로토콜 수정 및 저장 검증 (모든 필드 포함)', async () => {
        const updateAll = {
            display_name: 'Modbus TCP Full Update',
            description: 'Updating every single field',
            category: 'industrial',
            default_port: 502,
            uses_serial: false,
            requires_broker: false,
            supported_operations: ['read_coils', 'write_single_register'],
            supported_data_types: ['int32', 'float64'],
            connection_params: { slave_id: 2, baudrate: 9600 },
            default_polling_interval: 3000,
            default_timeout: 8000,
            max_concurrent_connections: 10,
            vendor: 'Updated Vendor',
            is_enabled: false
        };

        const response = await request(app)
            .put('/api/protocols/1') // MODBUS_TCP
            .send(updateAll)
            .expect(200);

        expect(response.body.success).toBe(true);
        expect(response.body.data.display_name).toBe('Modbus TCP Full Update');
        expect(response.body.data.supported_operations).toContain('read_coils');

        // DB 검증
        const updated = await knex('protocols').where('id', 1).first();
        expect(updated.display_name).toBe('Modbus TCP Full Update');
        expect(updated.is_enabled).toBe(0);

        const ops = JSON.parse(updated.supported_operations);
        expect(ops).toContain('write_single_register');

        const params = JSON.parse(updated.connection_params);
        expect(params.slave_id).toBe(2);
    });

    test('✅ PUT /api/protocols/:id - 프로토콜 수정 및 저장 검증', async () => {
        const updateData = {
            display_name: 'Modbus TCP Updated',
            description: 'Updated description for Modbus',
            default_port: 503
        };

        const response = await request(app)
            .put('/api/protocols/1')
            .send(updateData)
            .expect(200);

        expect(response.body.success).toBe(true);
        expect(response.body.data.display_name).toBe('Modbus TCP Updated');
        expect(response.body.data.default_port).toBe(503);

        // DB에 반영되었는지 확인
        const updated = await knex('protocols').where('id', 1).first();
        expect(updated.display_name).toBe('Modbus TCP Updated');
        expect(updated.default_port).toBe(503);
    });

    test('✅ POST /api/protocols - 중복 프로토콜 타입 생성 방지 검증', async () => {
        const duplicateProtocol = {
            protocol_type: 'MODBUS_TCP', // 이미 id 1번으로 존재
            display_name: 'Another Modbus'
        };

        const response = await request(app)
            .post('/api/protocols')
            .send(duplicateProtocol)
            .expect(500); // Service에서 Error 던지면 500 반환하도록 짜여있음

        expect(response.body.success).toBe(false);
        expect(response.body.message).toBe('Protocol type already exists');
    });
});
