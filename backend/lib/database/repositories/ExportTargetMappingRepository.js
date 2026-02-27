const BaseRepository = require('./BaseRepository');

/**
 * ExportTargetMappingRepository class
 * Handles database operations for export target mappings.
 */
class ExportTargetMappingRepository extends BaseRepository {
    constructor() {
        super('export_target_mappings');
    }

    /**
     * 특정 타겟의 모든 매핑 조회
     * @param {number} targetId 타겟 ID
     * @param {number} [tenantId] 테넌트 ID
     */
    async findByTargetId(targetId, tenantId = null) {
        try {
            const query = this.query().where('target_id', targetId);
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }
            const results = await query.orderBy('id', 'ASC');
            return results.map(item => this._parseItem(item));
        } catch (error) {
            this.logger.error('ExportTargetMappingRepository.findByTargetId 오류:', error);
            throw error;
        }
    }

    /**
     * 특정 포인트의 매핑 조회
     */
    async findByPointId(pointId, tenantId = null) {
        try {
            const query = this.query().where('point_id', pointId);
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }
            const results = await query;
            return results.map(item => this._parseItem(item));
        } catch (error) {
            this.logger.error('ExportTargetMappingRepository.findByPointId 오류:', error);
            throw error;
        }
    }

    /**
     * 매핑 생성
     */
    async save(data, tenantId, siteId = null) {
        if (!tenantId && !data.tenant_id) {
            throw new Error('tenant_id is required for ExportTargetMapping');
        }

        try {
            const dataToInsert = {
                tenant_id: tenantId || data.tenant_id,
                target_id: data.target_id,
                point_id: data.point_id,
                site_id: siteId || data.site_id || data.building_id,
                target_field_name: data.target_field_name,
                target_description: data.target_description || '',
                conversion_config: typeof data.conversion_config === 'object' ? JSON.stringify(data.conversion_config) : (data.conversion_config || '{}'),
                is_enabled: data.is_enabled !== undefined ? data.is_enabled : 1,
                created_at: this.knex.raw("datetime('now', 'localtime')")
            };

            const [id] = await this.query().insert(dataToInsert);
            const item = await this.query().where('id', id).first();
            return this._parseItem(item);
        } catch (error) {
            this.logger.error('ExportTargetMappingRepository.save 오류:', error);
            throw error;
        }
    }

    /**
     * 매핑 수정
     */
    async update(id, data, tenantId = null) {
        try {
            const dataToUpdate = {
                updated_at: this.knex.raw("datetime('now', 'localtime')")
            };

            const allowedFields = ['target_field_name', 'target_description', 'conversion_config', 'is_enabled', 'site_id'];
            allowedFields.forEach(field => {
                if (data[field] !== undefined) {
                    if (field === 'conversion_config' && typeof data[field] === 'object') {
                        dataToUpdate[field] = JSON.stringify(data[field]);
                    } else {
                        dataToUpdate[field] = data[field];
                    }
                }
            });

            if (data.building_id !== undefined && data.site_id === undefined) {
                dataToUpdate.site_id = data.building_id;
            }

            const query = this.query().where('id', id);
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }

            const affected = await query.update(dataToUpdate);
            if (affected > 0) {
                const item = await this.findById(id, tenantId);
                return this._parseItem(item);
            }
            return null;
        } catch (error) {
            this.logger.error('ExportTargetMappingRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 조회 (Helper)
     */
    async findById(id, tenantId = null) {
        const query = this.query().where('id', id);
        if (tenantId) {
            query.where('tenant_id', tenantId);
        }
        return await query.first();
    }

    /**
     * JSON 필드 파싱 헬퍼
     */
    _parseItem(item) {
        if (!item) return null;
        if (typeof item.conversion_config === 'string') {
            try {
                item.conversion_config = JSON.parse(item.conversion_config);
            } catch (e) {
                item.conversion_config = {};
            }
        }
        return item;
    }

    /**
     * 특정 타겟의 모든 매핑 삭제
     */
    async deleteByTargetId(targetId, tenantId = null, siteId = null) {
        try {
            const query = this.query().where('target_id', targetId);
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }
            if (siteId) {
                query.where('site_id', siteId);
            }
            const affected = await query.delete();
            return affected > 0;
        } catch (error) {
            this.logger.error('ExportTargetMappingRepository.deleteByTargetId 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 매핑 삭제
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
            this.logger.error('ExportTargetMappingRepository.deleteById 오류:', error);
            throw error;
        }
    }
}

module.exports = ExportTargetMappingRepository;
