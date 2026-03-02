// backend/lib/services/ControlTemplateService.js
// 제어 템플릿 서비스 (Phase 8)
// 자주 쓰는 제어값을 저장/불러오기

'use strict';

class ControlTemplateService {
    constructor() {
        this._knex = null;
    }

    get knex() {
        if (!this._knex) {
            this._knex = require('../database/KnexManager').getInstance().getKnex();
        }
        return this._knex;
    }

    async ensureTable() {
        const exists = await this.knex.schema.hasTable('control_templates');
        if (exists) return;

        await this.knex.schema.createTable('control_templates', (t) => {
            t.increments('id').primary();
            t.integer('tenant_id');
            t.text('name').notNullable();
            t.integer('point_id');
            t.text('value').notNullable();
            t.text('description');
            t.datetime('created_at').defaultTo(this.knex.fn.now());
        });
        console.log('[ControlTemplate] ✅ control_templates table created');
    }

    async list(tenant_id, point_id) {
        let q = this.knex('control_templates').orderBy('name');
        if (tenant_id) q = q.where({ tenant_id });
        if (point_id) q = q.where({ point_id: parseInt(point_id) });
        return q;
    }

    async create(data) {
        if (!data.name || data.value === undefined) throw new Error('name과 value 필수');
        const [id] = await this.knex('control_templates').insert({
            tenant_id: data.tenant_id || null,
            name: data.name,
            point_id: data.point_id || null,
            value: String(data.value),
            description: data.description || null
        });
        return this.knex('control_templates').where({ id }).first();
    }

    async update(id, data) {
        const upd = {};
        if (data.name) upd.name = data.name;
        if (data.value !== undefined) upd.value = String(data.value);
        if (data.description !== undefined) upd.description = data.description;
        await this.knex('control_templates').where({ id }).update(upd);
        return this.knex('control_templates').where({ id }).first();
    }

    async remove(id) {
        await this.knex('control_templates').where({ id }).delete();
    }
}

module.exports = new ControlTemplateService();
