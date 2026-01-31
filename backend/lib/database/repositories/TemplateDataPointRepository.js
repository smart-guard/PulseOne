const BaseRepository = require('./BaseRepository');

/**
 * TemplateDataPointRepository class
 * Handles database operations for template data points.
 */
class TemplateDataPointRepository extends BaseRepository {
    constructor() {
        super('template_data_points');
    }

    /**
     * 템플릿 ID로 데이터 포인트들을 조회합니다.
     */
    async findByTemplateId(templateId) {
        try {
            return await this.query()
                .where('template_device_id', templateId)
                .orderBy('sort_order', 'ASC')
                .orderBy('address', 'ASC');
        } catch (error) {
            this.logger.error('TemplateDataPointRepository.findByTemplateId 오류:', error);
            throw error;
        }
    }

    /**
     * 새로운 템플릿 데이터 포인트를 생성합니다.
     */
    async create(data) {
        try {
            const [id] = await this.query().insert({
                template_device_id: data.template_device_id,
                name: data.name,
                description: data.description || null,
                address: data.address,
                address_string: data.address_string || null,
                data_type: data.data_type || 'FLOAT32',
                access_mode: data.access_mode || 'read',
                unit: data.unit || null,
                scaling_factor: data.scaling_factor || 1.0,
                scaling_offset: data.scaling_offset || 0.0,
                is_writable: data.is_writable ? 1 : 0,
                is_active: data.is_active !== false ? 1 : 0,
                sort_order: data.sort_order || 0,
                metadata: typeof data.metadata === 'object' ? JSON.stringify(data.metadata) : (data.metadata || null)
            });
            return await this.query().where('id', id).first();
        } catch (error) {
            this.logger.error('TemplateDataPointRepository.create 오류:', error);
            throw error;
        }
    }

    /**
     * 템플릿 데이터 포인트를 업데이트합니다.
     */
    async update(id, data) {
        try {
            const updateData = {
                updated_at: this.knex.fn.now()
            };

            const fields = [
                'name', 'description', 'address', 'address_string',
                'data_type', 'access_mode', 'unit', 'scaling_factor', 'scaling_offset',
                'is_writable', 'is_active', 'sort_order', 'metadata'
            ];

            fields.forEach(field => {
                if (data[field] !== undefined) {
                    if (field === 'metadata' && typeof data[field] === 'object') {
                        updateData[field] = JSON.stringify(data[field]);
                    } else if (field === 'is_writable' || field === 'is_active') {
                        updateData[field] = data[field] ? 1 : 0;
                    } else {
                        updateData[field] = data[field];
                    }
                }
            });

            const affected = await this.query().where('id', id).update(updateData);
            return affected > 0 ? await this.query().where('id', id).first() : null;
        } catch (error) {
            this.logger.error('TemplateDataPointRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 템플릿 데이터 포인트를 삭제합니다.
     */
    async deleteById(id) {
        try {
            const affected = await this.query().where('id', id).del();
            return affected > 0;
        } catch (error) {
            this.logger.error('TemplateDataPointRepository.deleteById 오류:', error);
            throw error;
        }
    }

    /**
     * 특정 템플릿의 모든 데이터 포인트를 삭제합니다.
     */
    async deleteByTemplateId(templateId, trx = null) {
        try {
            const affected = await this.query(trx).where('template_device_id', templateId).del();
            return affected > 0;
        } catch (error) {
            this.logger.error('TemplateDataPointRepository.deleteByTemplateId 오류:', error);
            throw error;
        }
    }
}

module.exports = TemplateDataPointRepository;
