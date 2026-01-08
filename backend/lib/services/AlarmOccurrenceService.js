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
            return await this.repository.acknowledge(id, userId, comment, tenantId);
        });
    }

    /**
     * Clear an alarm occurrence.
     */
    async clear(id, userId, clearedValue, comment, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.clear(id, userId, clearedValue, comment, tenantId);
        });
    }

    /**
     * Update the state of an alarm occurrence.
     */
    async updateState(id, state, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.updateState(id, state, tenantId);
        });
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
