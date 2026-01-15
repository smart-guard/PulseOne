const BaseRepository = require('./BaseRepository');

/**
 * DeviceGroupRepository class
 * Handles database operations for logical device groups (device_groups).
 */
class DeviceGroupRepository extends BaseRepository {
    constructor() {
        super('device_groups');
    }

    /**
     * 모든 그룹을 조회합니다. (계층 구조 지원)
     */
    async findAll(tenantId = null, siteId = null) {
        try {
            const query = this.query();
            if (tenantId) query.where('tenant_id', tenantId);
            if (siteId) query.where('site_id', siteId);

            return await query.orderBy('sort_order', 'ASC').orderBy('name', 'ASC');
        } catch (error) {
            this.logger.error('DeviceGroupRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 그룹을 조회합니다.
     */
    async findById(id, tenantId = null) {
        try {
            const query = this.query().where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);
            return await query.first();
        } catch (error) {
            this.logger.error('DeviceGroupRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 자식 그룹들을 조회합니다.
     */
    async findChildren(parentId, tenantId = null) {
        try {
            const query = this.query().where('parent_group_id', parentId);
            if (tenantId) query.where('tenant_id', tenantId);
            return await query.orderBy('sort_order', 'ASC');
        } catch (error) {
            this.logger.error('DeviceGroupRepository.findChildren 오류:', error);
            throw error;
        }
    }

    /**
     * 최상위 그룹들을 조회합니다.
     */
    async findRootGroups(tenantId = null, siteId = null) {
        try {
            const query = this.query().whereNull('parent_group_id');
            if (tenantId) query.where('tenant_id', tenantId);
            if (siteId) query.where('site_id', siteId);
            return await query.orderBy('sort_order', 'ASC');
        } catch (error) {
            this.logger.error('DeviceGroupRepository.findRootGroups 오류:', error);
            throw error;
        }
    }

    async create(groupData, tenantId = null) {
        try {
            const dataToInsert = {
                tenant_id: tenantId || groupData.tenant_id,
                site_id: groupData.site_id || 1,
                parent_group_id: groupData.parent_group_id || null,
                name: groupData.name,
                description: groupData.description || null,
                group_type: groupData.group_type || 'functional',
                tags: typeof groupData.tags === 'object' ? JSON.stringify(groupData.tags) : (groupData.tags || '[]'),
                metadata: typeof groupData.metadata === 'object' ? JSON.stringify(groupData.metadata) : (groupData.metadata || '{}'),
                is_active: groupData.is_active !== false ? 1 : 0,
                sort_order: groupData.sort_order || 0,
                created_by: groupData.created_by || null
            };

            const [id] = await this.knex('device_groups').insert(dataToInsert);
            return await this.findById(id, dataToInsert.tenant_id);
        } catch (error) {
            this.logger.error('DeviceGroupRepository.create 오류:', error);
            throw error;
        }
    }

    /**
     * 그룹 정보를 업데이트합니다.
     */
    async update(id, updateData, tenantId = null) {
        try {
            const dataToUpdate = {
                updated_at: this.knex.fn.now()
            };

            const allowedFields = [
                'name', 'description', 'parent_group_id', 'group_type',
                'tags', 'metadata', 'is_active', 'sort_order'
            ];

            allowedFields.forEach(field => {
                if (updateData[field] !== undefined) {
                    if ((field === 'tags' || field === 'metadata') && typeof updateData[field] === 'object') {
                        dataToUpdate[field] = JSON.stringify(updateData[field]);
                    } else {
                        dataToUpdate[field] = updateData[field];
                    }
                }
            });

            const query = this.query().where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);

            const affected = await query.update(dataToUpdate);
            return affected > 0 ? await this.findById(id, tenantId) : null;
        } catch (error) {
            this.logger.error('DeviceGroupRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 그룹을 삭제합니다.
     */
    async deleteById(id, tenantId = null) {
        try {
            return await this.knex.transaction(async (trx) => {
                // 1. 자식 그룹들의 parent_group_id를 NULL로 설정 (또는 정책에 따라 삭제)
                await trx('device_groups').where('parent_group_id', id).update({ parent_group_id: null });

                // 2. 이 그룹에 소속된 디바이스들의 device_group_id를 NULL로 설정
                await trx('devices').where('device_group_id', id).update({ device_group_id: null });

                // 3. 그룹 삭제
                let query = trx('device_groups').where('id', id);
                if (tenantId) query = query.where('tenant_id', tenantId);

                const affected = await query.del();
                return affected > 0;
            });
        } catch (error) {
            this.logger.error('DeviceGroupRepository.deleteById 오류:', error);
            throw error;
        }
    }
}

module.exports = DeviceGroupRepository;
