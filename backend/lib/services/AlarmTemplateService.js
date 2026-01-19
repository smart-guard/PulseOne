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
            // Check if system template
            const existing = await this.repository.findById(id, tenantId);
            if (existing && existing.is_system_template) {
                throw new Error('System templates cannot be modified.');
            }

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
            // Check if system template
            const existing = await this.repository.findById(id, tenantId);
            if (existing && existing.is_system_template) {
                throw new Error('System templates cannot be deleted.');
            }

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
     * Get statistics for a specific template.
     */
    async getTemplateStats(id, tenantId) {
        return this.handleRequest(async () => {
            return await this.repository.getTemplateStats(id, tenantId);
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
                    template_version: template.version || 1,
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

    /**
     * Get outdated rules for a template (version mismatch).
     */
    async getOutdatedRules(templateId, tenantId) {
        return this.handleRequest(async () => {
            const template = await this.repository.findById(templateId, tenantId);
            if (!template) {
                throw new Error(`Alarm template with ID ${templateId} not found`);
            }

            const appliedRules = await this.repository.findAppliedRules(templateId, tenantId);
            const currentVersion = template.version || 1;

            return appliedRules.filter(rule => {
                const ruleVersion = rule.template_version || 1;
                return ruleVersion < currentVersion;
            });
        });
    }

    /**
     * Propagate template changes to selected rules.
     */
    async propagateTemplate(templateId, { rule_ids }, tenantId, userId) {
        return this.handleRequest(async () => {
            const template = await this.repository.findById(templateId, tenantId);
            if (!template) {
                throw new Error(`Alarm template with ID ${templateId} not found`);
            }

            const ruleRepository = RepositoryFactory.getInstance().getRepository('AlarmRuleRepository');
            const updatedRules = [];
            const errors = [];

            // If no specific rules selected, fetch all outdated ones
            let targetRuleIds = rule_ids;
            if (!targetRuleIds || targetRuleIds.length === 0) {
                const outdated = await this.getOutdatedRules(templateId, tenantId);
                targetRuleIds = outdated.map(r => r.id);
            }

            for (const ruleId of targetRuleIds) {
                try {
                    const rule = await ruleRepository.findById(ruleId, tenantId);
                    if (!rule || rule.template_id !== templateId) {
                        continue;
                    }

                    // Prepare update data based on template
                    const updateData = {
                        name: rule.name, // Keep existing name
                        description: template.description,
                        alarm_type: template.condition_type,
                        severity: template.severity,
                        category: template.category,
                        message_template: template.message_template,

                        // Notifications
                        notification_enabled: template.notification_enabled,
                        auto_acknowledge: template.auto_acknowledge,
                        auto_clear: template.auto_clear,

                        // Tags
                        tags: template.tags,

                        // Version sync
                        template_version: template.version || 1,
                        last_template_update: this.repository.knex.fn.now()
                    };

                    // Re-apply condition logic
                    const config = template.default_config;
                    const parsedConfig = typeof config === 'string' ? JSON.parse(config) : config;

                    if (template.condition_type === 'threshold' || template.condition_type === 'range') {
                        updateData.high_high_limit = parsedConfig.high_high_limit;
                        updateData.high_limit = parsedConfig.high_limit;
                        updateData.low_limit = parsedConfig.low_limit;
                        updateData.low_low_limit = parsedConfig.low_low_limit;
                    } else if (template.condition_type === 'digital') {
                        updateData.trigger_condition = parsedConfig.trigger_condition;
                    } else if (template.condition_type === 'script') {
                        updateData.condition_script = parsedConfig.condition_script;
                    }

                    const updatedRule = await ruleRepository.update(ruleId, updateData, tenantId);
                    updatedRules.push(updatedRule);

                } catch (err) {
                    errors.push({ rule_id: ruleId, error: err.message });
                }
            }

            return {
                updated_count: updatedRules.length,
                total_attempted: targetRuleIds.length,
                updated_rules: updatedRules,
                errors: errors
            };
        });
    }

    /**
     * Propagate template changes to selected rules.
     */
    async propagateTemplate(templateId, { rule_ids }, tenantId, userId) {
        return this.handleRequest(async () => {
            const template = await this.repository.findById(templateId, tenantId);
            if (!template) {
                throw new Error(`Alarm template with ID ${templateId} not found`);
            }

            const ruleRepository = RepositoryFactory.getInstance().getRepository('AlarmRuleRepository');
            const updatedRules = [];
            const errors = [];

            // If no specific rules selected, fetch all outdated ones
            let targetRuleIds = rule_ids;
            if (!targetRuleIds || targetRuleIds.length === 0) {
                const outdated = await this.getOutdatedRules(templateId, tenantId);
                targetRuleIds = outdated.map(r => r.id);
            }

            for (const ruleId of targetRuleIds) {
                try {
                    const rule = await ruleRepository.findById(ruleId, tenantId);
                    if (!rule || rule.template_id !== templateId) {
                        continue;
                    }

                    // Prepare update data based on template
                    const updateData = {
                        name: rule.name, // Keep existing name
                        description: template.description,
                        alarm_type: template.condition_type,
                        severity: template.severity,
                        category: template.category,
                        message_template: template.message_template,

                        // Notifications
                        notification_enabled: template.notification_enabled,
                        auto_acknowledge: template.auto_acknowledge,
                        auto_clear: template.auto_clear,

                        // Tags
                        tags: template.tags,

                        // Version sync
                        template_version: template.version || 1,
                        last_template_update: this.repository.knex.fn.now()
                    };

                    // Re-apply condition logic
                    const config = template.default_config;
                    const parsedConfig = typeof config === 'string' ? JSON.parse(config) : config;

                    if (template.condition_type === 'threshold' || template.condition_type === 'range') {
                        updateData.high_high_limit = parsedConfig.high_high_limit;
                        updateData.high_limit = parsedConfig.high_limit;
                        updateData.low_limit = parsedConfig.low_limit;
                        updateData.low_low_limit = parsedConfig.low_low_limit;
                    } else if (template.condition_type === 'digital') {
                        updateData.trigger_condition = parsedConfig.trigger_condition;
                    } else if (template.condition_type === 'script') {
                        updateData.condition_script = parsedConfig.condition_script;
                    }

                    const updatedRule = await ruleRepository.update(ruleId, updateData, tenantId);
                    updatedRules.push(updatedRule);

                } catch (err) {
                    errors.push({ rule_id: ruleId, error: err.message });
                }
            }

            return {
                updated_count: updatedRules.length,
                total_attempted: targetRuleIds.length,
                updated_rules: updatedRules,
                errors: errors
            };
        });
    }

    /**
     * Simulate template application (Pre-application simulation).
     * Returns estimated alarm frequency based on historical patterns (Mocked for now).
     */
    async simulateTemplate(templateId, { data_point_ids, duration_days }, tenantId) {
        return this.handleRequest(async () => {
            const template = await this.repository.findById(templateId, tenantId);
            if (!template) {
                throw new Error(`Alarm template with ID ${templateId} not found`);
            }

            // In a real system, this would query a TSDB (Time Series Database) 
            // to run the logic against raw historical data.
            // Since we don't have direct access to raw history in this context,
            // we will estimate based on severity and point count.

            const pointCount = data_point_ids ? data_point_ids.length : 1;
            const duration = duration_days || 7;

            let baseRatePerDay = 0.5; // Default: 0.5 alarms per day
            if (template.severity === 'critical') baseRatePerDay = 0.1;
            if (template.severity === 'high') baseRatePerDay = 0.3;
            if (template.severity === 'low') baseRatePerDay = 1.2;

            // Random variation +/- 20%
            const variation = (Math.random() * 0.4) + 0.8;

            const estimatedTotal = Math.round(pointCount * baseRatePerDay * duration * variation);

            return {
                estimated_alarms: estimatedTotal,
                period_days: duration,
                confidence: "medium", // 'high' if real data used
                note: "Simulation based on heuristic model (Historical data integration required for precision)"
            };
        });
    }
}

module.exports = new AlarmTemplateService();
