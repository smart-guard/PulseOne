const BaseRepository = require('./BaseRepository');

/**
 * PayloadTemplateRepository class
 * Handles database operations for payload templates.
 */
class PayloadTemplateRepository extends BaseRepository {
    constructor() {
        super('payload_templates');
    }

    /**
     * 전체 템플릿 조회
     */
    async findAll() {
        try {
            return await this.query().orderBy('name', 'ASC');
        } catch (error) {
            this.logger.error('PayloadTemplateRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 템플릿 조회
     */
    async findById(id) {
        try {
            return await this.query().where('id', id).first();
        } catch (error) {
            this.logger.error('PayloadTemplateRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 이름으로 템플릿 찾기
     */
    async findByName(name) {
        return await this.query()
            .where({ name })
            .first();
    }

    /**
     * 활성화된 템플릿 조회
     */
    async findActive() {
        return await this.query()
            .where({ is_active: 1 })
            .orderBy('name', 'ASC');
    }

    /**
     * 템플릿 생성
     */
    async save(data) {
        try {
            const dataToInsert = {
                name: data.name,
                system_type: data.system_type || 'custom',
                description: data.description || '',
                template_json: typeof data.template_json === 'object' ? JSON.stringify(data.template_json) : (data.template_json || '{}'),
                is_active: data.is_active !== undefined ? data.is_active : 1,
                created_at: this.knex.fn.now(),
                updated_at: this.knex.fn.now()
            };

            const [id] = await this.knex(this.tableName).insert(dataToInsert);
            return await this.findById(id);
        } catch (error) {
            this.logger.error('PayloadTemplateRepository.save 오류:', error);
            throw error;
        }
    }

    /**
     * 템플릿 업데이트
     */
    async update(id, data) {
        try {
            const dataToUpdate = {
                updated_at: this.knex.fn.now()
            };

            const allowedFields = ['name', 'system_type', 'description', 'template_json', 'is_active'];
            allowedFields.forEach(field => {
                if (data[field] !== undefined) {
                    if (field === 'template_json' && typeof data[field] === 'object') {
                        dataToUpdate[field] = JSON.stringify(data[field]);
                    } else {
                        dataToUpdate[field] = data[field];
                    }
                }
            });

            const affected = await this.query().where('id', id).update(dataToUpdate);
            return affected > 0 ? await this.findById(id) : null;
        } catch (error) {
            this.logger.error('PayloadTemplateRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 템플릿 삭제
     */
    async deleteById(id) {
        try {
            const affected = await this.query().where('id', id).delete();
            return affected > 0;
        } catch (error) {
            this.logger.error('PayloadTemplateRepository.deleteById 오류:', error);
            throw error;
        }
    }
}

module.exports = PayloadTemplateRepository;
