// =============================================================================
// backend/lib/database/repositories/SystemSettingsRepository.js
// =============================================================================

const BaseRepository = require('./BaseRepository');

/**
 * @class SystemSettingsRepository
 * @description Repository for managing system_settings table.
 */
class SystemSettingsRepository extends BaseRepository {
    constructor() {
        super('system_settings');
    }

    /**
     * Find all settings by category.
     * @param {string} category Category name.
     * @returns {Promise<Array>} Array of setting objects.
     */
    async findByCategory(category) {
        return await this.query()
            .where('category', category)
            .orderBy('key_name', 'ASC');
    }

    /**
     * Find a setting by key.
     * @param {string} key Setting key.
     * @returns {Promise<Object>} Setting object.
     */
    async findByKey(key) {
        return await this.query()
            .where('key_name', key)
            .first();
    }

    /**
     * Update or create a setting value.
     * @param {string} key Setting key.
     * @param {string} value New value.
     * @param {string} userId User ID who makes the change.
     * @returns {Promise<number>} Number of affected rows.
     */
    async updateValue(key, value, userId = null) {
        const existing = await this.findByKey(key);

        if (existing) {
            return await this.query()
                .where('key_name', key)
                .update({
                    value: String(value),
                    updated_by: userId,
                    updated_at: new Date()
                });
        } else {
            return await this.query()
                .insert({
                    key_name: key,
                    value: String(value),
                    updated_by: userId,
                    updated_at: new Date()
                });
        }
    }

    /**
     * Get value only by key.
     * @param {string} key Setting key.
     * @param {any} defaultValue Default value if not found.
     * @returns {Promise<string>} Setting value.
     */
    async getValue(key, defaultValue = null) {
        const setting = await this.findByKey(key);
        return setting ? setting.value : defaultValue;
    }
}

module.exports = SystemSettingsRepository;
