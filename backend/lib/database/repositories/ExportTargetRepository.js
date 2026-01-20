const BaseRepository = require('./BaseRepository');

/**
 * ExportTargetRepository class
 * Handles database operations for export targets (destinations).
 */
class ExportTargetRepository extends BaseRepository {
    constructor() {
        super('export_targets');
    }

    /**
     * 전체 타겟 조회
     */
    async findAll(tenantId = null) {
        try {
            const query = this.query();
            // if (tenantId) {
            //     query.where('tenant_id', tenantId);
            // }
            const results = await query.orderBy('name', 'ASC');
            return results.map(item => this._parseItem(item));
        } catch (error) {
            this.logger.error('ExportTargetRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 타겟 조회
     */
    async findById(id, tenantId = null) {
        try {
            const query = this.query().where('id', id);
            // if (tenantId) {
            //     query.where('tenant_id', tenantId);
            // }
            const item = await query.first();
            return this._parseItem(item);
        } catch (error) {
            this.logger.error('ExportTargetRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 이름으로 타겟 찾기
     */
    async findByName(name, tenantId) {
        return await this.query()
            .where({ name }) // Removed tenant_id match
            .first();
    }

    /**
     * 활성화된 타겟 조회
     */
    async findActive(tenantId) {
        const results = await this.query()
            .where({ is_enabled: 1 })
            .orderBy('name', 'ASC');
        return results.map(item => this._parseItem(item));
    }

    /**
     * JSON 필드 파싱 헬퍼
     */
    _parseItem(item) {
        if (!item) return null;
        if (typeof item.config === 'string') {
            try {
                item.config = JSON.parse(item.config);
            } catch (e) {
                item.config = {};
            }
        }
        return item;
    }

    /**
     * 타겟 생성
     */
    async save(data, tenantId = null) {
        try {
            const dataToInsert = {
                // tenant_id: tenantId || data.tenant_id, // Removed as column does not exist
                profile_id: data.profile_id,
                name: data.name,
                target_type: data.target_type || data.type,
                config: typeof data.config === 'object' ? JSON.stringify(data.config) : (data.config || '{}'),
                template_id: data.template_id,
                export_mode: data.export_mode || 'on_change',
                export_interval: data.export_interval || 60,
                batch_size: data.batch_size || 100,
                is_enabled: data.is_enabled !== undefined ? data.is_enabled : 1,
                created_at: this.knex.fn.now(),
                updated_at: this.knex.fn.now()
            };

            const [id] = await this.knex(this.tableName).insert(dataToInsert);
            return await this.findById(id, tenantId);
        } catch (error) {
            this.logger.error('ExportTargetRepository.save 오류:', error);
            throw error;
        }
    }

    /**
     * 타겟 업데이트
     */
    async update(id, data, tenantId = null) {
        try {
            const dataToUpdate = {
                updated_at: this.knex.fn.now()
            };

            const allowedFields = ['name', 'target_type', 'type', 'config', 'is_enabled', 'profile_id', 'template_id', 'export_mode', 'export_interval', 'batch_size'];
            allowedFields.forEach(field => {
                if (data[field] !== undefined) {
                    if (field === 'config' && typeof data[field] === 'object') {
                        dataToUpdate[field] = JSON.stringify(data[field]);
                    } else if (field === 'type') {
                        dataToUpdate['target_type'] = data[field];
                    } else {
                        dataToUpdate[field] = data[field];
                    }
                }
            });

            const query = this.query().where('id', id);
            // if (tenantId) query.where('tenant_id', tenantId);

            const affected = await query.update(dataToUpdate);
            return affected > 0 ? await this.findById(id, tenantId) : null;
        } catch (error) {
            this.logger.error('ExportTargetRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 타겟 삭제
     */
    async deleteById(id, tenantId = null) {
        try {
            const query = this.query().where('id', id);
            // if (tenantId) query.where('tenant_id', tenantId);

            const affected = await query.delete();
            return affected > 0;
        } catch (error) {
            this.logger.error('ExportTargetRepository.deleteById 오류:', error);
            throw error;
        }
    }
}

module.exports = ExportTargetRepository;
