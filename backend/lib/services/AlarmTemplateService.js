// =============================================================================
// backend/lib/services/AlarmTemplateService.js
// =============================================================================

const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

/**
 * Service class for Alarm Template operations.
 * Handles business logic and calls AlarmTemplateRepository.
 */
class AlarmTemplateService extends BaseService {
    constructor() {
        super(null);
    }

    /**
     * Repository lazy loading
     */
    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getRepository('AlarmTemplateRepository');
        }
        return this._repository;
    }

    /**
     * Find all alarm templates with filtering and pagination.
     */
    async findAll(filters) {
        return this.handleRequest(async () => {
            return await this.repository.findAll(filters);
        });
    }

    /**
     * Find an alarm template by ID.
     */
    async findById(id, tenantId) {
        return this.handleRequest(async () => {
            const template = await this.repository.findById(id, tenantId);
            if (!template) {
                throw new Error(`Alarm template with ID ${id} not found`);
            }
            return template;
        });
    }

    /**
     * Find alarm templates by category.
     */
    async findByCategory(category, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findByCategory(category, tenantId);
        });
    }

    /**
     * Find system templates.
     */
    async findSystemTemplates() {
        return this.handleRequest(async () => {
            return await this.repository.findSystemTemplates();
        });
    }

    /**
     * Find user-defined templates.
     */
    async findUserTemplates(tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findUserTemplates(tenantId);
        });
    }

    /**
     * Find templates by data type.
     */
    async findByDataType(dataType, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findByDataType(dataType, tenantId);
        });
    }

    /**
     * Create a new alarm template.
     */
    async create(templateData, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.create(templateData, tenantId);
        });
    }

    /**
     * Update an existing alarm template.
     */
    async update(id, updateData, tenantId) {
        return this.handleRequest(async () => {
            const updated = await this.repository.update(id, updateData, tenantId);
            if (!updated) {
                throw new Error(`Alarm template with ID ${id} not found or update failed`);
            }
            return updated;
        });
    }

    /**
     * Delete an alarm template (soft delete).
     */
    async delete(id, tenantId) {
        return this.handleRequest(async () => {
            const deleted = await this.repository.delete(id, tenantId);
            if (!deleted) {
                throw new Error(`Alarm template with ID ${id} not found or delete failed`);
            }
            return true;
        });
    }

    /**
     * Increment usage count for a template.
     */
    async incrementUsage(id, incrementBy) {
        return this.handleRequest(async () => {
            return await this.repository.incrementUsage(id, incrementBy);
        });
    }

    /**
     * Get most used templates.
     */
    async findMostUsed(tenantId, limit) {
        return this.handleRequest(async () => {
            return await this.repository.findMostUsed(tenantId, limit);
        });
    }

    /**
     * Get statistics summary for alarm templates.
     */
    async getStatistics(tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.getStatistics(tenantId);
        });
    }
}

module.exports = new AlarmTemplateService();
