// =============================================================================
// backend/__tests__/alarm-rule.service.test.js
// AlarmRuleService 단위 테스트
// =============================================================================

const AlarmRuleService = require('../lib/services/AlarmRuleService');
const KnexManager = require('../lib/database/KnexManager');
const RepositoryFactory = require('../lib/database/repositories/RepositoryFactory');

describe('AlarmRuleService Tests', () => {
    let knex;
    let tenantId = 1;
    let userId = 1;

    beforeAll(async () => {
        process.env.DATABASE_TYPE = 'sqlite';
        process.env.SQLITE_PATH = ':memory:';

        await RepositoryFactory.getInstance().initialize();
        knex = KnexManager.getInstance().getKnex('sqlite');

        // 기존 테이블 제거 (충돌 방지)
        await knex.schema.dropTableIfExists('alarm_rules');
        await knex.schema.dropTableIfExists('virtual_points');
        await knex.schema.dropTableIfExists('data_points');
        await knex.schema.dropTableIfExists('devices');
        await knex.schema.dropTableIfExists('protocols');
        await knex.schema.dropTableIfExists('sites');
        await knex.schema.dropTableIfExists('tenants');

        // 테이블 생성
        await knex.schema.createTable('tenants', table => {
            table.increments('id').primary();
            table.string('company_name');
        });

        await knex.schema.createTable('sites', table => {
            table.increments('id').primary();
            table.integer('tenant_id');
            table.string('name');
            table.string('location');
        });

        await knex.schema.createTable('protocols', table => {
            table.increments('id').primary();
            table.string('protocol_type');
            table.string('display_name');
        });

        await knex.schema.createTable('devices', table => {
            table.increments('id').primary();
            table.integer('tenant_id');
            table.integer('site_id');
            table.integer('protocol_id');
            table.string('name');
            table.string('device_type');
        });

        await knex.schema.createTable('data_points', table => {
            table.increments('id').primary();
            table.integer('device_id');
            table.string('name');
            table.string('unit');
        });

        await knex.schema.createTable('virtual_points', table => {
            table.increments('id').primary();
            table.integer('tenant_id');
            table.string('name');
        });

        await knex.schema.createTable('alarm_rules', table => {
            table.increments('id').primary();
            table.integer('tenant_id').notNullable();
            table.string('name').notNullable();
            table.string('description');
            table.string('target_type').notNullable();
            table.integer('target_id').notNullable();
            table.string('target_group');
            table.string('alarm_type').notNullable();
            table.float('high_high_limit');
            table.float('high_limit');
            table.float('low_limit');
            table.float('low_low_limit');
            table.float('deadband').defaultTo(0);
            table.float('rate_of_change').defaultTo(0);
            table.string('trigger_condition');
            table.text('condition_script');
            table.text('message_script');
            table.text('message_config');
            table.string('message_template');
            table.string('severity').defaultTo('medium');
            table.integer('priority').defaultTo(100);
            table.integer('auto_acknowledge').defaultTo(0);
            table.integer('acknowledge_timeout_min').defaultTo(0);
            table.integer('auto_clear').defaultTo(1);
            table.text('suppression_rules');
            table.integer('notification_enabled').defaultTo(1);
            table.integer('notification_delay_sec').defaultTo(0);
            table.integer('notification_repeat_interval_min').defaultTo(0);
            table.text('notification_channels');
            table.text('notification_recipients');
            table.integer('is_enabled').defaultTo(1);
            table.integer('is_latched').defaultTo(0);
            table.integer('created_by');
            table.integer('template_id');
            table.string('rule_group');
            table.integer('created_by_template').defaultTo(0);
            table.timestamp('last_template_update');
            table.integer('escalation_enabled').defaultTo(0);
            table.integer('escalation_max_level').defaultTo(3);
            table.text('escalation_rules');
            table.string('category');
            table.text('tags');
            table.timestamps(true, true);
        });

        // 초기 데이터 세팅
        await knex('tenants').insert({ id: 1, company_name: 'Test Tenant' });
        await knex('sites').insert({ id: 1, tenant_id: 1, name: 'Main Site' });
        await knex('protocols').insert({ id: 1, protocol_type: 'modbus', display_name: 'Modbus' });
        await knex('devices').insert({ id: 1, tenant_id: 1, site_id: 1, protocol_id: 1, name: 'Test Device', device_type: 'analog' });
    });

    afterAll(async () => {
        await KnexManager.getInstance().destroyAll();
    });

    describe('CRUD Operations', () => {
        let testRuleId;

        it('should create a new alarm rule', async () => {
            const ruleData = {
                tenant_id: tenantId,
                name: 'Test Analog Alarm',
                target_type: 'device',
                target_id: 1,
                alarm_type: 'analog',
                high_limit: 80.5,
                severity: 'high',
                category: 'environmental'
            };

            const response = await AlarmRuleService.createAlarmRule(ruleData, userId);

            if (!response.success) console.error('Creation failed:', response.message);
            expect(response.success).toBe(true);
            expect(response.data.name).toBe('Test Analog Alarm');
            testRuleId = response.data.id;
        });

        it('should get an alarm rule by ID', async () => {
            const response = await AlarmRuleService.getAlarmRuleById(testRuleId, tenantId);

            expect(response.success).toBe(true);
            expect(response.data.id).toBe(testRuleId);
        });

        it('should update an alarm rule', async () => {
            const response = await AlarmRuleService.updateAlarmRule(testRuleId, {
                severity: 'critical'
            }, tenantId);

            expect(response.success).toBe(true);
            expect(response.data.severity).toBe('critical');
        });

        it('should list alarm rules with filters', async () => {
            const response = await AlarmRuleService.getAlarmRules({
                tenantId: tenantId,
                severity: 'critical'
            });

            expect(response.success).toBe(true);
            expect(response.data.items.length).toBeGreaterThan(0);
        });

        it('should delete an alarm rule', async () => {
            const response = await AlarmRuleService.deleteAlarmRule(testRuleId, tenantId);
            expect(response.success).toBe(true);

            const check = await AlarmRuleService.getAlarmRuleById(testRuleId, tenantId);
            expect(check.success).toBe(false);
        });
    });

    describe('Statistics & Isolation', () => {
        it('should get rule statistics', async () => {
            await AlarmRuleService.createAlarmRule({
                tenant_id: tenantId,
                name: 'Rule 1',
                target_type: 'device',
                target_id: 1,
                alarm_type: 'analog',
                severity: 'medium'
            }, userId);

            const response = await AlarmRuleService.getRuleStats(tenantId);
            expect(response.success).toBe(true);
            expect(response.data.summary.total_rules).toBeGreaterThan(0);
        });

        it('should respect tenant isolation', async () => {
            const res1 = await AlarmRuleService.createAlarmRule({
                tenant_id: 1,
                name: 'Tenant 1 Rule',
                target_type: 'device',
                target_id: 1,
                alarm_type: 'analog'
            }, userId);

            const res2 = await AlarmRuleService.getAlarmRuleById(res1.data.id, 2);
            expect(res2.success).toBe(false);
        });
    });
});
