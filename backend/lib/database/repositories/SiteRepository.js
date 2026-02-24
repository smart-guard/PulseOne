// =============================================================================
// backend/lib/database/repositories/SiteRepository.js
// SiteRepository refactored to use Knex and extend BaseRepository
// =============================================================================

const BaseRepository = require('./BaseRepository');

class SiteRepository extends BaseRepository {
    constructor() {
        super('sites');
    }

    /**
     * Get all sites for a tenant
     * @param {number} tenantId 
     */
    async findAll(tenantId = null, options = {}) {
        try {
            const buildQuery = (builder) => {
                if (tenantId) builder.where('tenant_id', tenantId);

                // 삭제된 항목 필터링 (기본값: 삭제되지 않은 항목만 표시)
                if (options.onlyDeleted === true) {
                    builder.where('is_deleted', 1);
                } else if (options.includeDeleted !== true) {
                    builder.where('is_deleted', 0);
                }

                if (options.isActive !== undefined) builder.where('is_active', options.isActive ? 1 : 0);
                if (options.search) builder.where('name', 'like', `%${options.search}%`);
                return builder;
            };

            const countBuilder = this.query().count('* as count');
            buildQuery(countBuilder);
            const countResult = await countBuilder.first();
            const total = countResult ? (parseInt(countResult.count) || 0) : 0;

            let query = this.query()
                .select('sites.*')
                .select(this.knex.raw(
                    '(SELECT COUNT(*) FROM edge_servers WHERE edge_servers.site_id = sites.id AND edge_servers.is_deleted = 0) as collector_count'
                ));
            buildQuery(query);

            const limit = parseInt(options.limit) || 1000;
            const page = Math.max(1, parseInt(options.page) || 1);
            const offset = (page - 1) * limit;

            const items = await query.orderBy('name', 'asc').limit(limit).offset(offset);

            return {
                items,
                pagination: {
                    total_count: total,
                    current_page: page,
                    total_pages: Math.ceil(total / limit),
                    limit
                }
            };
        } catch (error) {
            this.logger.error('SiteRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * Find site by ID with tenant isolation
     * @param {number} id 
     * @param {number} tenantId 
     */
    async findById(id, tenantId = null) {
        try {
            let query = this.query().where('id', id);
            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }
            return await query.first();
        } catch (error) {
            this.logger.error('SiteRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * Find sites by parent ID
     * @param {number} parentId 
     * @param {number} tenantId 
     */
    async findByParentId(parentId, tenantId = null) {
        try {
            let query = this.query().where('parent_site_id', parentId);
            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }
            return await query.orderBy('name', 'asc');
        } catch (error) {
            this.logger.error('SiteRepository.findByParentId 오류:', error);
            throw error;
        }
    }

    /**
     * Create a new site
     * @param {Object} siteData 
     */
    async create(siteData) {
        try {
            const [id] = await this.query().insert({
                tenant_id: siteData.tenant_id,
                parent_site_id: siteData.parent_site_id || null,
                name: siteData.name,
                code: siteData.code || null,
                site_type: siteData.site_type || 'building',
                description: siteData.description || null,
                location: siteData.location || null,
                timezone: siteData.timezone || 'Asia/Seoul',
                address: siteData.address || null,
                city: siteData.city || null,
                country: siteData.country || null,
                postal_code: siteData.postal_code || null,
                state_province: siteData.state_province || null,
                coordinates: (siteData.latitude && siteData.longitude) ? `${siteData.latitude},${siteData.longitude}` : (siteData.coordinates || null),
                hierarchy_level: siteData.hierarchy_level || 0,
                hierarchy_path: siteData.hierarchy_path || '/',
                sort_order: siteData.sort_order || 0,
                is_active: siteData.is_active !== false ? 1 : 0,
                is_visible: siteData.is_visible !== false ? 1 : 0,
                monitoring_enabled: siteData.monitoring_enabled !== false ? 1 : 0,
                manager_name: siteData.manager_name || siteData.contact_name || null,
                manager_email: siteData.manager_email || siteData.contact_email || null,
                manager_phone: siteData.manager_phone || siteData.contact_phone || null,
                emergency_contact: siteData.emergency_contact || null,
                operating_hours: siteData.operating_hours || null,
                shift_pattern: siteData.shift_pattern || null,
                working_days: siteData.working_days || null,
                floor_area: siteData.floor_area || null,
                ceiling_height: siteData.ceiling_height || null,
                max_occupancy: siteData.max_occupancy || null,
                edge_server_id: siteData.edge_server_id || null,
                tags: typeof siteData.tags === 'object' ? JSON.stringify(siteData.tags) : (siteData.tags || '[]'),
                metadata: typeof siteData.metadata === 'object' ? JSON.stringify(siteData.metadata) : (siteData.metadata || '{}')
            });
            return id;
        } catch (error) {
            this.logger.error('SiteRepository.create 오류:', error);
            throw error;
        }
    }

    /**
     * Update an existing site
     * @param {number} id 
     * @param {Object} siteData 
     * @param {number} tenantId 
     */
    async update(id, siteData, tenantId = null) {
        try {
            let query = this.query().where('id', id);
            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }

            // [보완] 필드 매핑 및 필터링
            const dataToUpdate = {
                parent_site_id: siteData.parent_site_id || null,
                name: siteData.name,
                code: siteData.code,
                site_type: siteData.site_type,
                description: siteData.description || null,
                location: siteData.location || null,
                timezone: siteData.timezone || 'Asia/Seoul',
                address: siteData.address || null,
                city: siteData.city || null,
                country: siteData.country || null,
                postal_code: siteData.postal_code || null,
                state_province: siteData.state_province || null,

                // 계층 정보
                hierarchy_level: siteData.hierarchy_level || 0,
                hierarchy_path: siteData.hierarchy_path || '/',
                sort_order: siteData.sort_order || 0,

                // 연락처 정보 (프론트엔드 contact_name -> manager_name 매핑)
                manager_name: siteData.manager_name || siteData.contact_name || null,
                manager_email: siteData.manager_email || siteData.contact_email || null,
                manager_phone: siteData.manager_phone || siteData.contact_phone || null,
                emergency_contact: siteData.emergency_contact || null,

                // 운영 및 시설 정보
                operating_hours: siteData.operating_hours || null,
                shift_pattern: siteData.shift_pattern || null,
                working_days: siteData.working_days || null,
                floor_area: siteData.floor_area || null,
                ceiling_height: siteData.ceiling_height || null,
                max_occupancy: siteData.max_occupancy || null,

                // 상태 필드
                is_active: siteData.is_active !== false ? 1 : 0,
                is_visible: siteData.is_visible !== false ? 1 : 0,
                monitoring_enabled: siteData.monitoring_enabled !== false ? 1 : 0,

                // 인프라 연결
                edge_server_id: siteData.edge_server_id || null,

                updated_at: this.knex.fn.now()
            };

            // JSON 필드 처리 (tags, metadata)
            if (siteData.tags !== undefined) {
                dataToUpdate.tags = typeof siteData.tags === 'object' ? JSON.stringify(siteData.tags) : siteData.tags;
            }
            if (siteData.metadata !== undefined) {
                dataToUpdate.metadata = typeof siteData.metadata === 'object' ? JSON.stringify(siteData.metadata) : siteData.metadata;
            }

            // GPS 좌표 처리
            if (siteData.latitude !== undefined && siteData.longitude !== undefined) {
                if (siteData.latitude && siteData.longitude) {
                    dataToUpdate.coordinates = `${siteData.latitude},${siteData.longitude}`;
                } else {
                    dataToUpdate.coordinates = null;
                }
            } else if (siteData.coordinates !== undefined) {
                dataToUpdate.coordinates = siteData.coordinates;
            }

            const result = await query.update(dataToUpdate);
            return result > 0;
        } catch (error) {
            this.logger.error('SiteRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * Partially update a site
     * @param {number} id 
     * @param {Object} siteData 
     * @param {number} tenantId 
     */
    async updatePartial(id, siteData, tenantId = null) {
        try {
            let query = this.query().where('id', id);
            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }

            const dataToUpdate = { ...siteData };
            delete dataToUpdate.id;
            delete dataToUpdate.tenant_id;
            dataToUpdate.updated_at = this.knex.fn.now();

            // Field mapping for contact_name
            if (dataToUpdate.contact_name !== undefined && dataToUpdate.manager_name === undefined) {
                dataToUpdate.manager_name = dataToUpdate.contact_name;
                delete dataToUpdate.contact_name;
            }
            if (dataToUpdate.contact_email !== undefined && dataToUpdate.manager_email === undefined) {
                dataToUpdate.manager_email = dataToUpdate.contact_email;
                delete dataToUpdate.contact_email;
            }
            if (dataToUpdate.contact_phone !== undefined && dataToUpdate.manager_phone === undefined) {
                dataToUpdate.manager_phone = dataToUpdate.contact_phone;
                delete dataToUpdate.contact_phone;
            }

            // Status fields mapping
            const boolFields = ['is_active', 'is_visible', 'monitoring_enabled'];
            boolFields.forEach(f => {
                if (dataToUpdate[f] !== undefined) {
                    dataToUpdate[f] = dataToUpdate[f] ? 1 : 0;
                }
            });

            // JSON handling
            if (dataToUpdate.tags !== undefined && typeof dataToUpdate.tags === 'object') {
                dataToUpdate.tags = JSON.stringify(dataToUpdate.tags);
            }
            if (dataToUpdate.metadata !== undefined && typeof dataToUpdate.metadata === 'object') {
                dataToUpdate.metadata = JSON.stringify(dataToUpdate.metadata);
            }

            // Coordinates handling
            if (dataToUpdate.latitude !== undefined && dataToUpdate.longitude !== undefined) {
                if (dataToUpdate.latitude && dataToUpdate.longitude) {
                    dataToUpdate.coordinates = `${dataToUpdate.latitude},${dataToUpdate.longitude}`;
                } else {
                    dataToUpdate.coordinates = null;
                }
                delete dataToUpdate.latitude;
                delete dataToUpdate.longitude;
            }

            const result = await query.update(dataToUpdate);
            return result > 0;
        } catch (error) {
            this.logger.error('SiteRepository.updatePartial 오류:', error);
            throw error;
        }
    }

    /**
     * Delete a site (Soft Delete)
     * @param {number} id 
     * @param {number} tenantId 
     */
    async deleteById(id, tenantId = null) {
        try {
            let query = this.query().where('id', id);
            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }
            const result = await query.update({
                is_deleted: 1,
                updated_at: this.knex.fn.now()
            });
            return result > 0;
        } catch (error) {
            this.logger.error('SiteRepository.deleteById (soft) 오류:', error);
            throw error;
        }
    }

    /**
     * Restore a deleted site
     * @param {number} id 
     * @param {number} tenantId 
     */
    async restoreById(id, tenantId = null) {
        try {
            let query = this.query().where('id', id);
            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }
            const result = await query.update({
                is_deleted: 0,
                updated_at: this.knex.fn.now()
            });
            return result > 0;
        } catch (error) {
            this.logger.error('SiteRepository.restoreById 오류:', error);
            throw error;
        }
    }

    /**
     * Permanent delete a site
     * @param {number} id 
     * @param {number} tenantId 
     */
    async permanentDeleteById(id, tenantId = null) {
        try {
            let query = this.query().where('id', id);
            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }
            const result = await query.delete();
            return result > 0;
        } catch (error) {
            this.logger.error('SiteRepository.permanentDeleteById 오류:', error);
            throw error;
        }
    }

    /**
     * Get site hierarchy tree for a tenant
     * @param {number} tenantId 
     */
    async getHierarchyTree(tenantId = null) {
        try {
            let query = this.query().select('*');
            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }
            return await query.orderBy('hierarchy_path', 'asc').orderBy('name', 'asc');
        } catch (error) {
            this.logger.error('SiteRepository.getHierarchyTree 오류:', error);
            throw error;
        }
    }
}

module.exports = SiteRepository;