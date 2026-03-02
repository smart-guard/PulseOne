// backend/lib/services/ControlSequenceService.js
// 멀티포인트 제어 시퀀스 서비스 (Phase 4)

'use strict';

class ControlSequenceService {
    constructor() {
        this._knex = null;
        this._controlLog = null;
    }

    get knex() {
        if (!this._knex) {
            this._knex = require('../database/KnexManager').getInstance().getKnex();
        }
        return this._knex;
    }

    get controlLog() {
        if (!this._controlLog) this._controlLog = require('./ControlLogService');
        return this._controlLog;
    }

    // ─── DB 테이블 보장 ────────────────────────────────────────
    async ensureTable() {
        const exists = await this.knex.schema.hasTable('control_sequences');
        if (exists) return;

        await this.knex.schema.createTable('control_sequences', (t) => {
            t.increments('id').primary();
            t.integer('tenant_id');
            t.text('name').notNullable();
            t.text('description');
            // steps: [{point_id, device_id, value, delay_ms}]
            t.text('steps').notNullable(); // JSON 직렬화
            t.datetime('created_at').defaultTo(this.knex.fn.now());
        });
        console.log('[ControlSequence] ✅ control_sequences table created');
    }

    // ─── CRUD ─────────────────────────────────────────────────
    async list(tenant_id) {
        let q = this.knex('control_sequences').orderBy('id', 'desc');
        if (tenant_id) q = q.where({ tenant_id });
        const rows = await q;
        return rows.map(r => ({ ...r, steps: this._parseSteps(r.steps) }));
    }

    async getById(id) {
        const row = await this.knex('control_sequences').where({ id }).first();
        if (!row) return null;
        return { ...row, steps: this._parseSteps(row.steps) };
    }

    async create(data) {
        if (!data.name || !data.steps?.length) throw new Error('name과 steps 필수');

        const [id] = await this.knex('control_sequences').insert({
            tenant_id: data.tenant_id || null,
            name: data.name,
            description: data.description || null,
            steps: JSON.stringify(data.steps)
        });
        return this.getById(id);
    }

    async update(id, data) {
        const updateData = {};
        if (data.name) updateData.name = data.name;
        if (data.description !== undefined) updateData.description = data.description;
        if (data.steps) updateData.steps = JSON.stringify(data.steps);
        await this.knex('control_sequences').where({ id }).update(updateData);
        return this.getById(id);
    }

    async remove(id) {
        await this.knex('control_sequences').where({ id }).delete();
    }

    // ─── 시퀀스 실행 ─────────────────────────────────────────
    async run(id, executor = 'sequence') {
        const seq = await this.getById(id);
        if (!seq) throw new Error('시퀀스를 찾을 수 없습니다');

        const redisClient = require('../connection/redis');
        const knex = this.knex;
        const results = [];

        for (const step of seq.steps) {
            const { point_id, device_id, value, delay_ms = 0 } = step;

            // delay 대기
            if (delay_ms > 0) await this._sleep(delay_ms);

            const device = await knex('devices').where({ id: device_id }).first();
            const point = await knex('data_points').where({ id: point_id }).first();

            if (!device?.edge_server_id) {
                results.push({ point_id, success: false, error: 'No edge_server_id' });
                continue;
            }

            const request_id = await this.controlLog.createLog({
                tenant_id: seq.tenant_id,
                site_id: device.site_id,
                user_id: null,
                username: executor,
                device_id,
                device_name: device.name,
                protocol_type: device.protocol_type,
                point_id,
                point_name: point?.name || '',
                address: point?.address_string || String(point?.address || ''),
                old_value: null,
                requested_value: String(value)
            });

            const client = await redisClient.getRedisClient();
            if (!client) {
                await this.controlLog.markDelivery(request_id, 0);
                results.push({ point_id, request_id, success: false, error: 'No Redis' });
                continue;
            }

            const channel = `cmd:collector:${device.edge_server_id}`;
            const msg = JSON.stringify({
                command: 'write',
                device_id: String(device_id),
                point_id: String(point_id),
                value: String(value),
                request_id,
                timestamp: new Date().toISOString()
            });

            const subs = await client.publish(channel, msg);
            await this.controlLog.markDelivery(request_id, subs);
            results.push({ point_id, request_id, success: true, subscribers: subs });
        }

        console.log(`[ControlSequence] ✅ 시퀀스 실행 완료: id=${id} steps=${results.length}`);
        return results;
    }

    _parseSteps(stepsJson) {
        try { return JSON.parse(stepsJson); } catch { return []; }
    }

    _sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }
}

module.exports = new ControlSequenceService();
