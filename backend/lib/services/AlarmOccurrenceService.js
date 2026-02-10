// =============================================================================
// backend/lib/services/AlarmOccurrenceService.js
// =============================================================================

const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

/**
 * Service class for Alarm Occurrence operations.
 * Handles business logic and calls AlarmOccurrenceRepository.
 */
class AlarmOccurrenceService extends BaseService {
    constructor() {
        super(null);
    }

    /**
     * Repository lazy loading
     */
    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getRepository('AlarmOccurrenceRepository');
        }
        return this._repository;
    }

    /**
     * Find all alarm occurrences with filtering and pagination.
     */
    async findAll(filters) {
        return this.handleRequest(async () => {
            return await this.repository.findAll(filters);
        });
    }

    /**
     * Find an alarm occurrence by ID.
     */
    async findById(id, tenantId) {
        return this.handleRequest(async () => {
            const occurrence = await this.repository.findById(id, tenantId);
            if (!occurrence) {
                throw new Error(`Alarm occurrence with ID ${id} not found`);
            }
            return occurrence;
        });
    }

    /**
     * Find active alarm occurrences.
     */
    async findActive(tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findActive(tenantId);
        });
    }

    /**
     * Find unacknowledged alarm occurrences.
     */
    async findUnacknowledged(tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findUnacknowledged(tenantId);
        });
    }

    /**
     * Find alarm occurrences by device.
     */
    async findByDevice(deviceId, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findByDevice(deviceId, tenantId);
        });
    }

    /**
     * Find alarm occurrences by category.
     */
    async findByCategory(category, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findByCategory(category, tenantId);
        });
    }

    /**
     * Find alarm occurrences for today.
     */
    async findToday(tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findTodayAlarms(tenantId);
        });
    }

    /**
     * Create a new alarm occurrence.
     */
    async create(occurrenceData) {
        return this.handleRequest(async () => {
            return await this.repository.create(occurrenceData);
        });
    }

    /**
     * Acknowledge an alarm occurrence.
     */
    async acknowledge(id, userId, comment, tenantId) {
        return this.handleRequest(async () => {
            const result = await this.repository.acknowledge(id, userId, comment, tenantId);
            this._notifyStateChange(tenantId);
            return result;
        });
    }

    /**
     * Clear an alarm occurrence.
     */
    async clear(id, userId, clearedValue, comment, tenantId) {
        return this.handleRequest(async () => {
            const result = await this.repository.clear(id, userId, clearedValue, comment, tenantId);
            this._notifyStateChange(tenantId);
            return result;
        });
    }

    /**
     * Acknowledge multiple alarm occurrences.
     */
    async acknowledgeBulk(ids, userId, comment, tenantId) {
        return this.handleRequest(async () => {
            const result = await this.repository.acknowledgeBulk(ids, userId, comment, tenantId);
            this._notifyStateChange(tenantId);
            return result;
        });
    }

    /**
     * Clear multiple alarm occurrences.
     */
    async clearBulk(ids, userId, clearedValue, comment, tenantId) {
        return this.handleRequest(async () => {
            const result = await this.repository.clearBulk(ids, userId, clearedValue, comment, tenantId);
            this._notifyStateChange(tenantId);
            return result;
        });
    }

    /**
     * Acknowledge all active unacknowledged alarm occurrences for a tenant.
     */
    async acknowledgeAll(tenantId, userId, comment) {
        return this.handleRequest(async () => {
            const result = await this.repository.acknowledgeAll(tenantId, userId, comment);
            this._notifyStateChange(tenantId);
            return result;
        });
    }

    /**
     * Clear all active/acknowledged alarm occurrences for a tenant.
     */
    async clearAll(tenantId, userId, clearedValue, comment) {
        return this.handleRequest(async () => {
            const result = await this.repository.clearAll(tenantId, userId, clearedValue, comment);
            this._notifyStateChange(tenantId);
            return result;
        });
    }

    /**
     * Update the state of an alarm occurrence.
     */
    async updateState(id, state, tenantId) {
        return this.handleRequest(async () => {
            const result = await this.repository.updateState(id, state, tenantId);
            this._notifyStateChange(tenantId);
            return result;
        });
    }

    /**
     * WebSocket을 통한 상태 변경 알림 전송
     */
    _notifyStateChange(tenantId) {
        try {
            // Redis Stale Data Cleanup (Phase 4 fix)
            const RepositoryFactory = require('../database/repositories/RepositoryFactory');
            const rf = RepositoryFactory.getInstance();
            const db = rf.database; // Use raw knex or redis instance if available

            // Note: Since we don't have direct Redis service injected here yet,
            // we rely on the fact that the next alarm state check will refresh.
            // However, to be thorough, we should publish a clear command.

            const WebSocketService = require('./WebSocketService');
            const ws = WebSocketService.getInstance();
            if (ws) {
                ws.broadcastToTenant(tenantId, 'alarm:new', {
                    type: 'alarm_state_change',
                    timestamp: new Date().toISOString(),
                    tenant_id: tenantId,
                    data: { tenant_id: tenantId }
                });
            }
        } catch (error) {
            console.warn('❌ [AlarmOccurrenceService] WebSocket 알림 실패:', error.message);
        }
    }

    /**
     * Delete an alarm occurrence.
     */
    async delete(id) {
        return this.handleRequest(async () => {
            return await this.repository.delete(id);
        });
    }

    /**
     * Find alarm occurrences cleared by a specific user.
     */
    async findClearedByUser(userId, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findClearedByUser(userId, tenantId);
        });
    }

    /**
     * Find alarm occurrences acknowledged by a specific user.
     */
    async findAcknowledgedByUser(userId, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findAcknowledgedByUser(userId, tenantId);
        });
    }

    /**
     * Find alarm occurrences by tag.
     */
    async findByTag(tag, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findByTag(tag, tenantId);
        });
    }

    /**
     * Find active alarm occurrences by device.
     */
    async findActiveByDevice(deviceId, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findActiveByDevice(deviceId, tenantId);
        });
    }

    /**
     * Get statistics summary for alarm occurrences.
     */
    async getStatsSummary(tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.getStatsSummary(tenantId);
        });
    }

    /**
     * Get alarm statistics by severity.
     */
    async getStatsBySeverity(tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.getStatsBySeverity(tenantId);
        });
    }

    /**
     * Get alarm statistics by category.
     */
    async getStatsByCategory(tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.getStatsByCategory(tenantId);
        });
    }

    /**
     * Get alarm statistics by device.
     */
    async getStatsByDevice(tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.getStatsByDevice(tenantId);
        });
    }

    /**
     * Get alarm statistics for today.
     */
    async getStatsToday(tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.getStatsToday(tenantId);
        });
    }

    /**
     * Get user action statistics for alarms.
     */
    async getUserActionStats(userId, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.getUserActionStats(userId, tenantId);
        });
    }
}

module.exports = new AlarmOccurrenceService();
