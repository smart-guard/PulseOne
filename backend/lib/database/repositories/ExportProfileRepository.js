const BaseRepository = require('./BaseRepository');

/**
 * ExportProfileRepository class
 * Handles database operations for export profiles (data mapping).
 */
class ExportProfileRepository extends BaseRepository {
    constructor() {
        super('export_profiles');
    }

    /**
     * 전체 프로파일 조회
     * @param {number} [tenantId] 테넌트 ID (null인 경우 전체 조회 - Admin용)
     */
    async findAll(tenantId = null) {
        try {
            const query = this.query();
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }
            const results = await query.orderBy('name', 'ASC');
            return results.map(item => this._parseItem(item));
        } catch (error) {
            this.logger.error('ExportProfileRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 프로파일 조회
     * @param {number} id 프로파일 ID
     * @param {number} [tenantId] 테넌트 ID
     */
    async findById(id, tenantId = null) {
        try {
            const query = this.query().where('id', id);
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }
            const item = await query.first();
            return this._parseItem(item);
        } catch (error) {
            this.logger.error('ExportProfileRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 이름으로 프로파일 찾기
     */
    async findByName(name, tenantId = null) {
        const query = this.query().where({ name });
        if (tenantId) {
            query.where('tenant_id', tenantId);
        }
        return await query.first();
    }

    /**
     * 활성화된 프로파일 조회
     */
    async findActive(tenantId = null) {
        const query = this.query().where({ is_enabled: 1 });
        if (tenantId) {
            query.where('tenant_id', tenantId);
        }
        const results = await query.orderBy('name', 'ASC');
        return results.map(item => this._parseItem(item));
    }

    /**
     * JSON 필드 파싱 헬퍼
     */
    _parseItem(item) {
        if (!item) return null;
        if (typeof item.data_points === 'string') {
            try {
                item.data_points = JSON.parse(item.data_points);
            } catch (e) {
                item.data_points = [];
            }
        }
        return item;
    }

    /**
     * 프로파일 생성
     */
    async save(data, tenantId) {
        if (!tenantId && !data.tenant_id) {
            throw new Error('tenant_id is required for ExportProfile');
        }

        try {
            const dataToInsert = {
                tenant_id: tenantId || data.tenant_id,
                name: data.name,
                description: data.description || '',
                data_points: typeof data.data_points === 'object' ? JSON.stringify(data.data_points) : (data.data_points || '[]'),
                is_enabled: data.is_enabled !== undefined ? data.is_enabled : 1,
                created_at: this.knex.raw("datetime('now', 'localtime')"),
                updated_at: this.knex.raw("datetime('now', 'localtime')")
            };

            const [id] = await this.knex(this.tableName).insert(dataToInsert);
            return await this.findById(id, tenantId || data.tenant_id);
        } catch (error) {
            this.logger.error('ExportProfileRepository.save 오류:', error);
            throw error;
        }
    }

    /**
     * 프로파일 업데이트
     */
    async update(id, data, tenantId = null) {
        try {
            const dataToUpdate = {
                updated_at: this.knex.raw("datetime('now', 'localtime')")
            };

            const allowedFields = ['name', 'description', 'data_points', 'is_enabled'];
            allowedFields.forEach(field => {
                if (data[field] !== undefined) {
                    if (field === 'data_points' && typeof data[field] === 'object') {
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
            this.logger.error('ExportProfileRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 프로파일 삭제
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
            this.logger.error('ExportProfileRepository.deleteById 오류:', error);
            throw error;
        }
    }
}

module.exports = ExportProfileRepository;
