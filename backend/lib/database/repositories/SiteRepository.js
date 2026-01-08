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
    async findAll(tenantId = null) {
        try {
            let query = this.query().select('*');
            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }
            return await query.orderBy('name', 'asc');
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
                timezone: siteData.timezone || 'UTC',
                address: siteData.address || null,
                city: siteData.city || null,
                country: siteData.country || null,
                latitude: siteData.latitude || null,
                longitude: siteData.longitude || null,
                hierarchy_level: siteData.hierarchy_level || 1,
                hierarchy_path: siteData.hierarchy_path || '/',
                is_active: siteData.is_active !== false ? 1 : 0,
                contact_name: siteData.contact_name || null,
                contact_email: siteData.contact_email || null,
                contact_phone: siteData.contact_phone || null
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

            const result = await query.update({
                parent_site_id: siteData.parent_site_id,
                name: siteData.name,
                code: siteData.code,
                site_type: siteData.site_type,
                description: siteData.description,
                location: siteData.location,
                timezone: siteData.timezone,
                address: siteData.address,
                city: siteData.city,
                country: siteData.country,
                latitude: siteData.latitude,
                longitude: siteData.longitude,
                hierarchy_level: siteData.hierarchy_level,
                hierarchy_path: siteData.hierarchy_path,
                is_active: siteData.is_active !== false ? 1 : 0,
                contact_name: siteData.contact_name,
                contact_email: siteData.contact_email,
                contact_phone: siteData.contact_phone,
                updated_at: this.knex.fn.now()
            });
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

            if (dataToUpdate.is_active !== undefined) {
                dataToUpdate.is_active = dataToUpdate.is_active ? 1 : 0;
            }

            const result = await query.update(dataToUpdate);
            return result > 0;
        } catch (error) {
            this.logger.error('SiteRepository.updatePartial 오류:', error);
            throw error;
        }
    }

    /**
     * Delete a site
     * @param {number} id 
     * @param {number} tenantId 
     */
    async deleteById(id, tenantId = null) {
        try {
            let query = this.query().where('id', id);
            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }
            const result = await query.delete();
            return result > 0;
        } catch (error) {
            this.logger.error('SiteRepository.deleteById 오류:', error);
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