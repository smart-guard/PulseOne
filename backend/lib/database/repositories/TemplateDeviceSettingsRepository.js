const BaseRepository = require('./BaseRepository');

/**
 * TemplateDeviceSettingsRepository class
 * Handles database operations for template device settings.
 */
class TemplateDeviceSettingsRepository extends BaseRepository {
    constructor() {
        super('template_device_settings');
    }

    /**
     * 템플릿 ID로 설정을 조회합니다.
     */
    async findByTemplateId(templateId) {
        try {
            return await this.query().where('template_device_id', templateId).first();
        } catch (error) {
            this.logger.error('TemplateDeviceSettingsRepository.findByTemplateId 오류:', error);
            throw error;
        }
    }

    /**
     * 새로운 템플릿 설정을 생성합니다.
     */
    async create(data) {
        try {
            await this.query().insert({
                template_device_id: data.template_device_id,
                polling_interval_ms: data.polling_interval_ms || 1000,
                connection_timeout_ms: data.connection_timeout_ms || 10000,
                read_timeout_ms: data.read_timeout_ms || 5000,
                write_timeout_ms: data.write_timeout_ms || 5000,
                max_retry_count: data.max_retry_count || 3
            });
            return await this.findByTemplateId(data.template_device_id);
        } catch (error) {
            this.logger.error('TemplateDeviceSettingsRepository.create 오류:', error);
            throw error;
        }
    }

    /**
     * 템플릿 설정을 업데이트합니다.
     */
    async update(templateId, data) {
        try {
            const updateData = {};
            const fields = [
                'polling_interval_ms', 'connection_timeout_ms',
                'read_timeout_ms', 'write_timeout_ms', 'max_retry_count'
            ];

            fields.forEach(field => {
                if (data[field] !== undefined) {
                    updateData[field] = data[field];
                }
            });

            const affected = await this.query().where('template_device_id', templateId).update(updateData);
            return affected > 0 ? await this.findByTemplateId(templateId) : null;
        } catch (error) {
            this.logger.error('TemplateDeviceSettingsRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 설정을 삭제합니다.
     */
    async deleteByTemplateId(templateId) {
        try {
            const affected = await this.query().where('template_device_id', templateId).del();
            return affected > 0;
        } catch (error) {
            this.logger.error('TemplateDeviceSettingsRepository.deleteByTemplateId 오류:', error);
            throw error;
        }
    }
}

module.exports = TemplateDeviceSettingsRepository;
