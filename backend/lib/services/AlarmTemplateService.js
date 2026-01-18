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

    /**
     * Find alarm templates by tag.
     */
    async findByTag(tag, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findByTag(tag, tenantId);
        });
    }

    /**
     * Find rules created from a specific template.
     */
    async findAppliedRules(templateId, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.findAppliedRules(templateId, tenantId);
        });
    }

    /**
     * Search alarm templates by keyword.
     */
    async search(keyword, tenantId, limit) {
        return this.handleRequest(async () => {
            return await this.repository.search(keyword, tenantId, limit);
        });
    }

    /**
     * Apply a template to multiple data points.
     */
    async applyTemplate(templateId, { data_point_ids, custom_configs, rule_group_name }, tenantId, userId) {
        return this.handleRequest(async () => {
            const template = await this.repository.findById(templateId, tenantId);
            if (!template) {
                throw new Error(`Alarm template with ID ${templateId} not found`);
            }

            const ruleRepository = RepositoryFactory.getInstance().getRepository('AlarmRuleRepository');
            const createdRules = [];

            for (const dpId of data_point_ids) {
                const config = (custom_configs && custom_configs[dpId]) || template.default_config;

                // Base rule data derived from template
                const ruleData = {
                    tenant_id: tenantId,
                    name: `${template.name} (${dpId})`,
                    description: template.description,
                    target_type: 'data_point',
                    target_id: dpId,
                    alarm_type: template.condition_type,
                    severity: template.severity,
                    category: template.category,
                    message_template: template.message_template,
                    is_enabled: true,
                    created_by: userId,
                    template_id: templateId,
                    created_by_template: true,
                    rule_group: rule_group_name || null,

                    // Standard notification settings from template
                    notification_enabled: template.notification_enabled,
                    auto_acknowledge: template.auto_acknowledge,
                    auto_clear: template.auto_clear,

                    // Tags from template
                    tags: template.tags
                };

                // Map template configuration to rule fields
                const parsedConfig = typeof config === 'string' ? JSON.parse(config) : config;

                if (template.condition_type === 'threshold' || template.condition_type === 'range') {
                    ruleData.high_high_limit = parsedConfig.high_high_limit;
                    ruleData.high_limit = parsedConfig.high_limit;
                    ruleData.low_limit = parsedConfig.low_limit;
                    ruleData.low_low_limit = parsedConfig.low_low_limit;
                } else if (template.condition_type === 'digital') {
                    ruleData.trigger_condition = parsedConfig.trigger_condition;
                } else if (template.condition_type === 'script') {
                    ruleData.condition_script = parsedConfig.condition_script;
                }

                const newRule = await ruleRepository.create(ruleData, userId);
                createdRules.push(newRule);
            }

            // Update usage count for the template
            await this.repository.incrementUsage(templateId, data_point_ids.length);

            return {
                rules_created: createdRules.length,
                created_rules: createdRules
            };
        });
    }
}

module.exports = new AlarmTemplateService();
