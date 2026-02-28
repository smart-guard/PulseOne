// backend/lib/services/ControlLogService.js
// 제어 감사 로그 서비스 - 3단계 상태 추적 + 알람 매칭

'use strict';

const { v4: uuidv4 } = require('uuid');

class ControlLogService {
    constructor() {
        this._knex = null;
        this._redisClient = null;
        this._pendingVerifications = new Map(); // requestId → timer
        this._pendingAlarmMatches = new Map();
        this._initialized = false;
    }

    // ─── lazy getters ──────────────────────────────────────────
    get knex() {
        if (!this._knex) {
            const KnexManager = require('../database/KnexManager');
            this._knex = KnexManager.getInstance().getKnex();
        }
        return this._knex;
    }

    get redisClient() {
        if (!this._redisClient) this._redisClient = require('../connection/redis');
        return this._redisClient;
    }

    // ─── 초기화 (Redis control:result 구독) ────────────────────
    async initialize() {
        if (this._initialized) return;
        try {
            const ConfigManager = require('../config/ConfigManager');
            const Redis = require('ioredis');
            const cfg = ConfigManager.getInstance().getRedisConfig();

            const subClient = new Redis({
                host: cfg.host,
                port: cfg.port,
                password: cfg.password || undefined,
                db: cfg.db || 0,
                lazyConnect: true,
                connectTimeout: cfg.connectTimeout || 5000
            });

            subClient.on('connect', () => console.log('[ControlLog] ✅ Redis 구독 연결됨'));
            subClient.on('error', (e) => console.warn('[ControlLog] Redis 구독 오류:', e.message));
            subClient.on('message', (channel, message) => {
                if (channel === 'control:result') this._onControlResult(message);
            });

            await subClient.connect();
            await subClient.subscribe('control:result');
            console.log('[ControlLog] ✅ Subscribed to control:result channel');

            this._initialized = true;
        } catch (err) {
            console.warn('[ControlLog] Redis subscription failed (non-fatal):', err.message);
        }
    }

    // ─── 제어 로그 생성 ────────────────────────────────────────
    async createLog({
        tenant_id, site_id, user_id, username,
        device_id, device_name, protocol_type,
        point_id, point_name, address,
        old_value, requested_value
    }) {
        const request_id = uuidv4();

        try {
            await this.knex('control_logs').insert({
                request_id,
                tenant_id, site_id, user_id, username,
                device_id, device_name, protocol_type,
                point_id, point_name, address,
                old_value: old_value != null ? String(old_value) : null,
                requested_value: String(requested_value),
                delivery_status: 'pending',
                execution_result: 'pending',
                verification_result: 'pending',
                final_status: 'pending',
                requested_at: new Date().toISOString()
            });
        } catch (err) {
            console.error('[ControlLog] INSERT failed:', err.message);
        }

        return request_id;
    }

    // ─── 전달 상태 업데이트 (Redis publish 후 subscriber_count 확인) ─
    async markDelivery(request_id, subscriber_count) {
        try {
            if (subscriber_count === 0) {
                await this.knex('control_logs').where({ request_id }).update({
                    delivery_status: 'no_collector',
                    subscriber_count: 0,
                    final_status: 'failure',
                    execution_result: 'failure',
                    execution_error: 'No collector connected (subscriber_count=0)'
                });
            } else {
                await this.knex('control_logs').where({ request_id }).update({
                    delivery_status: 'delivered',
                    subscriber_count,
                    delivered_at: new Date().toISOString()
                });

                // 30초 timeout 설정 (Collector write 실행 + Redis 왕복 여유)
                const timer = setTimeout(() => this._markTimeout(request_id), 30000);
                this._pendingVerifications.set(request_id, timer);
            }
        } catch (err) {
            console.error('[ControlLog] markDelivery error:', err.message);
        }
    }

    // ─── Collector 결과 수신 ────────────────────────────────────
    async _onControlResult(message) {
        let data;
        try { data = JSON.parse(message); } catch { return; }

        const { request_id, success, is_async, error_message, duration_ms } = data;
        if (!request_id) return;

        // timeout 타이머 취소
        const timer = this._pendingVerifications.get(request_id);
        if (timer) { clearTimeout(timer); this._pendingVerifications.delete(request_id); }

        const execution_result = is_async ? 'protocol_async'
            : success ? 'protocol_success' : 'protocol_failure';

        const now = new Date().toISOString();

        try {
            await this.knex('control_logs').where({ request_id }).update({
                execution_result,
                execution_error: error_message || null,
                executed_at: now,
                duration_ms: duration_ms || null,
                // 비동기 프로토콜은 partial, 실패는 failure, 성공은 일단 pending(검증 대기)
                final_status: is_async ? 'partial' : success ? 'pending' : 'failure',
                verification_result: (is_async || !success) ? 'skipped' : 'pending'
            });

            if (success && !is_async) {
                // 동기 프로토콜 성공: 20초 후 값 검증
                const row = await this.knex('control_logs').where({ request_id }).first();
                if (row) {
                    setTimeout(() => this._verifyValue(request_id, row.point_id, row.requested_value), 20000);
                    // 80초 후 알람 매칭
                    setTimeout(() => this._matchAlarm(request_id, row.point_id, now), 80000);
                }
            } else if (is_async) {
                // 비동기: 알람 매칭만
                const row = await this.knex('control_logs').where({ request_id }).first();
                if (row) setTimeout(() => this._matchAlarm(request_id, row.point_id, now), 80000);
            }
        } catch (err) {
            console.error('[ControlLog] _onControlResult error:', err.message);
        }
    }

    // ─── 타임아웃 처리 ─────────────────────────────────────────
    async _markTimeout(request_id) {
        try {
            await this.knex('control_logs').where({ request_id }).update({
                execution_result: 'timeout',
                final_status: 'timeout',
                verification_result: 'skipped'
            });
            console.warn(`[ControlLog] ⏱ Timeout: ${request_id}`);
        } catch (err) {
            console.error('[ControlLog] _markTimeout error:', err.message);
        }
    }

    // ─── 값 반영 검증 (다음 폴링 후) ───────────────────────────
    async _verifyValue(request_id, point_id, requested_value) {
        try {
            const cv = await this.knex('current_values').where({ point_id }).first();
            if (!cv) return;

            // current_value는 JSON {"value": x} 형태
            let actualVal = null;
            try {
                const parsed = JSON.parse(cv.current_value);
                actualVal = parsed?.value != null ? String(parsed.value) : String(cv.current_value);
            } catch {
                actualVal = String(cv.current_value);
            }

            const verified = actualVal == String(requested_value);
            const now = new Date().toISOString();

            await this.knex('control_logs').where({ request_id }).update({
                verification_result: verified ? 'verified' : 'unverified',
                verified_value: actualVal,
                verified_at: now,
                final_status: verified ? 'success' : 'partial'
            });

            console.log(`[ControlLog] ${verified ? '✅ Verified' : '⚠️ Unverified'}: ${request_id} (got=${actualVal}, expected=${requested_value})`);
        } catch (err) {
            console.error('[ControlLog] _verifyValue error:', err.message);
        }
    }

    // ─── 알람 매칭 ─────────────────────────────────────────────
    async _matchAlarm(request_id, point_id, base_time) {
        try {
            const alarm = await this.knex('alarm_occurrences')
                .where({ point_id })
                .where('occurrence_time', '>=', base_time)
                .orderBy('occurrence_time', 'asc')
                .first();

            if (alarm) {
                await this.knex('control_logs').where({ request_id }).update({
                    linked_alarm_id: alarm.id,
                    alarm_matched_at: new Date().toISOString()
                });
                console.log(`[ControlLog] 🔔 Alarm matched: request=${request_id}, alarm=${alarm.id}`);
            }
        } catch (err) {
            console.error('[ControlLog] _matchAlarm error:', err.message);
        }
    }

    // ─── 조회 API용 ────────────────────────────────────────────
    async getLogs({
        device_id, point_id, user_id, tenant_id, final_status,
        protocol_type, from, to, page = 1, limit = 50
    }) {
        let q = this.knex('control_logs').select('*').orderBy('requested_at', 'desc');
        if (device_id) q = q.where({ device_id: parseInt(device_id) });
        if (point_id) q = q.where({ point_id: parseInt(point_id) });
        if (user_id) q = q.where({ user_id: parseInt(user_id) });
        if (tenant_id) q = q.where({ tenant_id: parseInt(tenant_id) });
        if (final_status) q = q.where({ final_status });
        if (protocol_type) q = q.where({ protocol_type });
        if (from) q = q.where('requested_at', '>=', from);
        if (to) q = q.where('requested_at', '<=', to);

        const total = await q.clone().count('id as count').first();
        const rows = await q.offset((page - 1) * limit).limit(limit);

        return {
            data: rows,
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: total?.count || 0
            }
        };
    }

    async getLogById(id) {
        return this.knex('control_logs').where({ id }).first();
    }

    // ─── DB 테이블 보장 (앱 시작 시) ───────────────────────────
    async ensureTable() {
        const exists = await this.knex.schema.hasTable('control_logs');
        if (exists) return;
        await this.knex.schema.createTable('control_logs', (t) => {
            t.increments('id').primary();
            t.text('request_id').notNullable().unique();
            t.integer('tenant_id'); t.integer('site_id');
            t.integer('user_id'); t.text('username');
            t.integer('device_id').notNullable(); t.text('device_name');
            t.text('protocol_type');
            t.integer('point_id').notNullable(); t.text('point_name'); t.text('address');
            t.text('old_value'); t.text('requested_value').notNullable();
            t.text('delivery_status').defaultTo('pending');
            t.integer('subscriber_count').defaultTo(0);
            t.datetime('delivered_at');
            t.text('execution_result').defaultTo('pending');
            t.text('execution_error');
            t.datetime('executed_at');
            t.integer('duration_ms');
            t.text('verification_result').defaultTo('pending');
            t.text('verified_value'); t.datetime('verified_at');
            t.integer('linked_alarm_id'); t.datetime('alarm_matched_at');
            t.text('final_status').defaultTo('pending');
            t.datetime('requested_at').defaultTo(this.knex.fn.now());
        });
        await this.knex.schema.table('control_logs', (t) => {
            t.index('device_id'); t.index('point_id');
            t.index('user_id'); t.index('tenant_id');
            t.index('final_status'); t.index('linked_alarm_id');
        });
        console.log('[ControlLog] ✅ control_logs table created');
    }
}

module.exports = new ControlLogService();
