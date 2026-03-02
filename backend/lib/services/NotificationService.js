// backend/lib/services/NotificationService.js
// 제어 실패 알림 서비스 (Phase 7)
// 채널: WebSocket push (즉시)

'use strict';

class NotificationService {
    constructor() {
        this._io = null;
        this._knex = null;
    }

    setIo(io) { this._io = io; }

    get knex() {
        if (!this._knex) {
            this._knex = require('../database/KnexManager').getInstance().getKnex();
        }
        return this._knex;
    }

    /**
     * 알림 전송
     * @param {number|null} tenant_id
     * @param {string} title
     * @param {string} message
     * @param {object} meta - 추가 데이터 (request_id, point_name 등)
     * @param {'error'|'warning'|'info'} level
     */
    send(tenant_id, title, message, meta = {}, level = 'error') {
        const payload = {
            type: 'control_notification',
            level,
            title,
            message,
            meta,
            timestamp: new Date().toISOString()
        };

        if (this._io) {
            if (tenant_id) {
                this._io.to(`tenant:${tenant_id}`).emit('notification', payload);
            }
            // broadcast (개발/단일 테넌트 환경용)
            this._io.emit('notification', payload);
        }

        console.log(`[Notification] [${level.toUpperCase()}] ${title}: ${message}`);
    }
}

module.exports = new NotificationService();
