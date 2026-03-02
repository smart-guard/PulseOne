// backend/lib/services/ControlSchedulerService.js
// 예약 제어 스케줄러 서비스 (Phase 3)
// node-cron 기반, control_schedules 테이블 사용

'use strict';

let cron;
try { cron = require('node-cron'); } catch { cron = null; }

class ControlSchedulerService {
    constructor() {
        this._knex = null;
        this._controlLog = null;
        this._redisClient = null;
        this._jobs = new Map(); // scheduleId → cron task
        this._initialized = false;
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
        const exists = await this.knex.schema.hasTable('control_schedules');
        if (exists) return;

        await this.knex.schema.createTable('control_schedules', (t) => {
            t.increments('id').primary();
            t.integer('tenant_id');
            t.integer('point_id').notNullable();
            t.integer('device_id').notNullable();
            t.text('point_name');
            t.text('device_name');
            t.text('value').notNullable();
            t.text('cron_expr');               // 반복: "*/5 * * * *" 등
            t.datetime('once_at');             // 일회성: 특정 시각
            t.boolean('enabled').defaultTo(true);
            t.datetime('last_run');
            t.text('description');
            t.datetime('created_at').defaultTo(this.knex.fn.now());
        });
        console.log('[ControlScheduler] ✅ control_schedules table created');
    }

    // ─── 초기화: DB에서 enabled 스케줄 로드 ──────────────────
    async initialize() {
        if (this._initialized) return;
        await this.ensureTable();

        const schedules = await this.knex('control_schedules').where({ enabled: true });
        for (const s of schedules) {
            this._registerJob(s);
        }
        this._initialized = true;
        console.log(`[ControlScheduler] ✅ ${schedules.length}개 스케줄 로드됨`);
    }

    // ─── 스케줄 등록 (내부) ───────────────────────────────────
    _registerJob(schedule) {
        if (schedule.cron_expr) {
            if (!cron) {
                console.warn('[ControlScheduler] node-cron 미설치 — Docker에서 npm install 필요');
                return;
            }
            if (!cron.validate(schedule.cron_expr)) {
                console.warn(`[ControlScheduler] 잘못된 cron 표현식: ${schedule.cron_expr}`);
                return;
            }
            const task = cron.schedule(schedule.cron_expr, () => {
                this._execute(schedule).catch(e =>
                    console.error(`[ControlScheduler] 실행 오류 (id=${schedule.id}):`, e.message)
                );
            }, { timezone: 'Asia/Seoul' });

            this._jobs.set(schedule.id, task);

        } else if (schedule.once_at) {
            // 일회성: once_at까지 남은 ms 계산 후 setTimeout
            const delay = new Date(schedule.once_at).getTime() - Date.now();
            if (delay <= 0) return; // 이미 지난 시각

            const timer = setTimeout(async () => {
                await this._execute(schedule);
                // 실행 후 disabled 처리
                await this.knex('control_schedules').where({ id: schedule.id }).update({ enabled: false });
                this._jobs.delete(schedule.id);
            }, delay);

            this._jobs.set(schedule.id, { stop: () => clearTimeout(timer) });
        }
    }

    // ─── 실제 제어 명령 실행 ──────────────────────────────────
    async _execute(schedule) {
        try {
            const redisClient = require('../connection/redis');
            const knex = this.knex;

            const device = await knex('devices').where({ id: schedule.device_id }).first();
            if (!device?.edge_server_id) {
                console.warn(`[ControlScheduler] edge_server_id 없음 (device=${schedule.device_id})`);
                return;
            }

            // 제어 로그 생성
            const request_id = await this.controlLog.createLog({
                tenant_id: schedule.tenant_id,
                site_id: device.site_id,
                user_id: null,
                username: 'scheduler',
                device_id: schedule.device_id,
                device_name: schedule.device_name || device.name,
                protocol_type: device.protocol_type,
                point_id: schedule.point_id,
                point_name: schedule.point_name || '',
                address: '',
                old_value: null,
                requested_value: String(schedule.value)
            });

            // Redis PUBLISH
            const client = await redisClient.getRedisClient();
            if (!client) {
                await this.controlLog.markDelivery(request_id, 0);
                return;
            }

            const channel = `cmd:collector:${device.edge_server_id}`;
            const message = JSON.stringify({
                command: 'write',
                device_id: String(schedule.device_id),
                point_id: String(schedule.point_id),
                value: String(schedule.value),
                request_id,
                timestamp: new Date().toISOString()
            });

            const subs = await client.publish(channel, message);
            await this.controlLog.markDelivery(request_id, subs);
            await knex('control_schedules').where({ id: schedule.id }).update({ last_run: new Date().toISOString() });

            console.log(`[ControlScheduler] ✅ 실행 완료: schedule=${schedule.id} point=${schedule.point_id} value=${schedule.value}`);
        } catch (err) {
            console.error(`[ControlScheduler] _execute 오류:`, err.message);
        }
    }

    // ─── CRUD ─────────────────────────────────────────────────
    async create(data) {
        const [id] = await this.knex('control_schedules').insert({
            tenant_id: data.tenant_id,
            point_id: data.point_id,
            device_id: data.device_id,
            point_name: data.point_name,
            device_name: data.device_name,
            value: String(data.value),
            cron_expr: data.cron_expr || null,
            once_at: data.once_at || null,
            enabled: data.enabled !== false,
            description: data.description || null
        });

        const schedule = await this.knex('control_schedules').where({ id }).first();
        if (schedule.enabled) this._registerJob(schedule);
        return schedule;
    }

    async list(tenant_id, { page = 1, pageSize = 20 } = {}) {
        let q = this.knex('control_schedules');
        if (tenant_id) q = q.where({ tenant_id });

        const totalRow = await q.clone().count('id as cnt').first();
        const totalCount = parseInt(totalRow?.cnt || 0, 10);

        const rows = await q.clone()
            .orderBy('id', 'desc')
            .limit(pageSize)
            .offset((page - 1) * pageSize);

        return { rows, totalCount, page, pageSize, totalPages: Math.ceil(totalCount / pageSize) };
    }

    async update(id, data) {
        // 기존 job 제거 후 재등록
        const oldJob = this._jobs.get(id);
        if (oldJob) { oldJob.stop(); this._jobs.delete(id); }

        await this.knex('control_schedules').where({ id }).update({
            value: data.value !== undefined ? String(data.value) : undefined,
            cron_expr: data.cron_expr,
            once_at: data.once_at,
            enabled: data.enabled,
            description: data.description
        });

        const schedule = await this.knex('control_schedules').where({ id }).first();
        if (schedule?.enabled) this._registerJob(schedule);
        return schedule;
    }

    async remove(id) {
        const job = this._jobs.get(id);
        if (job) { job.stop(); this._jobs.delete(id); }
        await this.knex('control_schedules').where({ id }).delete();
    }
}

module.exports = new ControlSchedulerService();
