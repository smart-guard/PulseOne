// =============================================================================
// backend/__tests__/virtual-point.service.test.js
// 가상포인트 서비스 유닛 테스트
// =============================================================================

const VirtualPointService = require('../lib/services/VirtualPointService');
const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');
const knex = require('knex');

describe('VirtualPointService Unit Tests', () => {
    let db;
    const tenantId = 1;

    beforeAll(async () => {
        // 인메모리 SQLite 초기화
        db = knex({
            client: 'sqlite3',
            connection: { filename: ':memory:' },
            useNullAsDefault: true
        });

        // 테이블 생성
        await db.schema.createTable('virtual_points', (table) => {
            table.increments('id').primary();
            table.integer('tenant_id').notNullable();
            table.string('name').notNullable();
            table.text('formula');
            table.string('description');
            table.string('data_type');
            table.string('unit');
            table.string('calculation_trigger');
            table.integer('is_enabled');
            table.string('category');
            table.string('scope_type');
            table.integer('site_id');
            table.integer('device_id');
            table.text('tags');
            table.integer('execution_count').defaultTo(0);
            table.float('avg_execution_time_ms').defaultTo(0);
            table.datetime('last_execution_time');
            table.text('last_error');
            table.timestamps(true, true);
        });

        await db.schema.createTable('virtual_point_inputs', (table) => {
            table.increments('id').primary();
            table.integer('virtual_point_id').notNullable();
            table.string('variable_name');
            table.string('source_type');
            table.integer('source_id');
            table.float('constant_value');
            table.string('source_formula');
            table.integer('is_required');
            table.integer('sort_order');
            table.timestamps(true, true);
        });

        await db.schema.createTable('virtual_point_values', (table) => {
            table.integer('virtual_point_id').primary();
            table.float('value');
            table.string('quality');
            table.datetime('last_calculated');
            table.integer('calculation_duration_ms');
            table.integer('is_stale');
        });

        await db.schema.createTable('virtual_point_execution_history', (table) => {
            table.increments('id').primary();
            table.integer('virtual_point_id');
            table.datetime('execution_time');
            table.integer('execution_duration_ms');
            table.string('result_type');
            table.text('result_value');
            table.string('trigger_source');
            table.integer('success');
        });

        await db.schema.createTable('virtual_point_dependencies', (table) => {
            table.increments('id').primary();
            table.integer('virtual_point_id');
            table.string('depends_on_type');
            table.integer('depends_on_id');
            table.integer('dependency_level');
            table.integer('is_active');
        });

        await db.schema.createTable('alarm_rules', (table) => {
            table.increments('id').primary();
            table.string('target_type');
            table.integer('target_id');
        });

        await db.schema.createTable('alarm_occurrences', (table) => {
            table.increments('id').primary();
            table.integer('rule_id');
            table.integer('point_id');
        });

        // RepositoryFactory에 주입
        const factory = RepositoryFactory.getInstance();
        factory.initialized = true;
        factory.dbManager = { getKnex: () => db };

        // KnexManager에 인메모리 DB 주입
        const KnexManager = require('../lib/database/KnexManager');
        const knexManager = KnexManager.getInstance();
        knexManager.instances.set('sqlite', db);
        knexManager.instances.set('sqlite3', db);
    });

    afterAll(async () => {
        await db.destroy();
    });

    describe('CRUD Operations', () => {
        let testVpId;

        it('should create a virtual point', async () => {
            const vpData = {
                name: 'Test VP 1',
                formula: 'return a + b;',
                description: 'Test Description',
                data_type: 'float',
                category: 'calculation'
            };
            const inputs = [
                { variable_name: 'a', source_type: 'constant', constant_value: 10 },
                { variable_name: 'b', source_type: 'constant', constant_value: 20 }
            ];

            const response = await VirtualPointService.create(vpData, inputs, tenantId);
            if (!response.success) console.error('Create Error:', response.message);
            expect(response.success).toBe(true);
            expect(response.data.name).toBe('Test VP 1');
            testVpId = response.data.id;
        });

        it('should find virtual point by ID', async () => {
            const response = await VirtualPointService.findById(testVpId, tenantId);
            expect(response.success).toBe(true);
            expect(response.data.name).toBe('Test VP 1');
            expect(response.data.inputs.length).toBe(2);
        });

        it('should update virtual point', async () => {
            const updateData = { name: 'Updated VP 1' };
            const newInputs = [{ variable_name: 'c', source_type: 'constant', constant_value: 30 }];

            const response = await VirtualPointService.update(testVpId, updateData, newInputs, tenantId);
            expect(response.success).toBe(true);
            expect(response.data.name).toBe('Updated VP 1');
            expect(response.data.inputs.length).toBe(1);
        });

        it('should toggle enabled status', async () => {
            const response = await VirtualPointService.toggleEnabled(testVpId, false, tenantId);
            expect(response.success).toBe(true);
            expect(response.data.is_enabled).toBe(false);
        });

        it('should delete virtual point', async () => {
            const response = await VirtualPointService.delete(testVpId, tenantId);
            expect(response.success).toBe(true);

            // Verify deletion
            try {
                await VirtualPointService.findById(testVpId, tenantId);
            } catch (error) {
                expect(error.message).toContain('찾을 수 없습니다');
            }
        });
    });

    describe('Statistics', () => {
        it('should get stats by category', async () => {
            await VirtualPointService.create({ name: 'VP A', category: 'cat1' }, [], tenantId);
            await VirtualPointService.create({ name: 'VP B', category: 'cat1' }, [], tenantId);
            await VirtualPointService.create({ name: 'VP C', category: 'cat2' }, [], tenantId);

            const response = await VirtualPointService.getStatsByCategory(tenantId);
            expect(response.success).toBe(true);
            expect(response.data.length).toBeGreaterThanOrEqual(2);
        });

        it('should get performance stats', async () => {
            const response = await VirtualPointService.getPerformanceStats(tenantId);
            expect(response.success).toBe(true);
            expect(response.data.total_points).toBeGreaterThan(0);
        });
    });
});
