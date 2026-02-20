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
     * @param {number} [tenantId] 테넌트 ID
     */
    async findAll(tenantId = null) {
        try {
            const query = this.query();
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }
            return await query.orderBy('name', 'ASC');
        } catch (error) {
            this.logger.error('PayloadTemplateRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 템플릿 조회
     * @param {number} id 템플릿 ID
     * @param {number} [tenantId] 테넌트 ID
     */
    async findById(id, tenantId = null) {
        try {
            const query = this.query().where('id', id);
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }
            return await query.first();
        } catch (error) {
            this.logger.error('PayloadTemplateRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 이름으로 템플릿 찾기
     */
    async findByName(name, tenantId = null) {
        const query = this.query().where({ name });
        if (tenantId) {
            query.where('tenant_id', tenantId);
        }
        return await query.first();
    }

    /**
     * 활성화된 템플릿 조회
     */
    async findActive(tenantId = null) {
        const query = this.query().where({ is_active: 1 });
        if (tenantId) {
            query.where('tenant_id', tenantId);
        }
        return await query.orderBy('name', 'ASC');
    }

    /**
     * 템플릿 생성
     */
    async save(data, tenantId) {
        if (!tenantId && !data.tenant_id) {
            throw new Error('tenant_id is required for PayloadTemplate');
        }

        try {
            const dataToInsert = {
                tenant_id: tenantId || data.tenant_id,
                name: data.name,
                system_type: data.system_type || 'custom',
                description: data.description || '',
                template_json: typeof data.template_json === 'object' ? JSON.stringify(data.template_json) : (data.template_json || '{}'),
                is_active: data.is_active !== undefined ? data.is_active : 1,
                created_at: this.knex.fn.now(),
                updated_at: this.knex.fn.now()
            };

            const [id] = await this.knex(this.tableName).insert(dataToInsert);
            return await this.findById(id, tenantId || data.tenant_id);
        } catch (error) {
            this.logger.error('PayloadTemplateRepository.save 오류:', error);
            throw error;
        }
    }

    /**
     * 템플릿 업데이트
     */
    async update(id, data, tenantId = null) {
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

            const query = this.query().where('id', id);
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }

            const affected = await query.update(dataToUpdate);
            return affected > 0 ? await this.findById(id, tenantId) : null;
        } catch (error) {
            this.logger.error('PayloadTemplateRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 템플릿 삭제
     */
    async deleteById(id, tenantId = null) {
        try {
            const query = this.query().where('id', id);
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }
            const affected = await query.delete();
            return affected > 0;
        } catch (error) {
            this.logger.error('PayloadTemplateRepository.deleteById 오류:', error);
            throw error;
        }
    }
}

module.exports = PayloadTemplateRepository;
