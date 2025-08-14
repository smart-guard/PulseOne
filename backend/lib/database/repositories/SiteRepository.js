// =============================================================================
// backend/lib/database/repositories/SiteRepository.js
// 🔧 DeviceRepository 패턴과 완전 동일 (쿼리 분리 적용)
// =============================================================================

const BaseRepository = require('./BaseRepository');
const SiteQueries = require('../queries/SiteQueries');

class SiteRepository extends BaseRepository {
    constructor() {
        // DeviceRepository와 동일한 패턴: 매개변수 없는 생성자
        super('sites');
        console.log('🏢 SiteRepository initialized with standard pattern');
    }

    // ==========================================================================
    // 기본 CRUD 연산 (SiteQueries 사용)
    // ==========================================================================

    async findAll() {
        try {
            const query = SiteQueries.findAll();
            const results = await this.executeQuery(query);
            return results;
        } catch (error) {
            console.error(`SiteRepository::findAll failed: ${error.message}`);
            throw error;
        }
    }

    async findById(id) {
        try {
            const query = SiteQueries.findById();
            const results = await this.executeQuery(query, [id]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`SiteRepository::findById failed: ${error.message}`);
            throw error;
        }
    }

    async findByTenant(tenantId) {
        try {
            const query = SiteQueries.findByTenant();
            const results = await this.executeQuery(query, [tenantId]);
            return results;
        } catch (error) {
            console.error(`SiteRepository::findByTenant failed: ${error.message}`);
            throw error;
        }
    }

    async findByCode(code, tenantId) {
        try {
            const query = SiteQueries.findByCode();
            const results = await this.executeQuery(query, [code, tenantId]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`SiteRepository::findByCode failed: ${error.message}`);
            throw error;
        }
    }

    async findRootSites(tenantId) {
        try {
            const query = SiteQueries.findRootSites();
            const results = await this.executeQuery(query, [tenantId]);
            return results;
        } catch (error) {
            console.error(`SiteRepository::findRootSites failed: ${error.message}`);
            throw error;
        }
    }

    async findByParentSite(parentSiteId) {
        try {
            const query = SiteQueries.findByParentSite();
            const results = await this.executeQuery(query, [parentSiteId]);
            return results;
        } catch (error) {
            console.error(`SiteRepository::findByParentSite failed: ${error.message}`);
            throw error;
        }
    }

    async create(siteData) {
        try {
            const query = SiteQueries.create();
            const params = [
                siteData.tenant_id,
                siteData.parent_site_id || null,
                siteData.name,
                siteData.code || null,
                siteData.site_type || 'building',
                siteData.description || null,
                siteData.location || null,
                siteData.timezone || 'UTC',
                siteData.address || null,
                siteData.city || null,
                siteData.country || null,
                siteData.latitude || null,
                siteData.longitude || null,
                siteData.hierarchy_level || 1,
                siteData.hierarchy_path || '/',
                siteData.is_active !== false ? 1 : 0,
                siteData.contact_name || null,
                siteData.contact_email || null,
                siteData.contact_phone || null
            ];
            
            const result = await this.executeNonQuery(query, params);
            return result.insertId || result.lastInsertRowid;
        } catch (error) {
            console.error(`SiteRepository::create failed: ${error.message}`);
            throw error;
        }
    }

    async update(id, siteData) {
        try {
            const query = SiteQueries.update();
            const params = [
                siteData.parent_site_id || null,
                siteData.name,
                siteData.code || null,
                siteData.site_type || 'building',
                siteData.description || null,
                siteData.location || null,
                siteData.timezone || 'UTC',
                siteData.address || null,
                siteData.city || null,
                siteData.country || null,
                siteData.latitude || null,
                siteData.longitude || null,
                siteData.hierarchy_level || 1,
                siteData.hierarchy_path || '/',
                siteData.is_active !== false ? 1 : 0,
                siteData.contact_name || null,
                siteData.contact_email || null,
                siteData.contact_phone || null,
                id
            ];
            
            const result = await this.executeNonQuery(query, params);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`SiteRepository::update failed: ${error.message}`);
            throw error;
        }
    }

    async deleteById(id) {
        try {
            const query = SiteQueries.delete();
            const result = await this.executeNonQuery(query, [id]);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`SiteRepository::deleteById failed: ${error.message}`);
            throw error;
        }
    }

    async exists(id) {
        try {
            const query = SiteQueries.exists();
            const result = await this.executeQuerySingle(query, [id]);
            return result && result.count > 0;
        } catch (error) {
            console.error(`SiteRepository::exists failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 특화 메서드들 (SiteQueries 사용)
    // ==========================================================================

    async findActiveSites(tenantId) {
        try {
            const query = SiteQueries.findActiveSites();
            const results = await this.executeQuery(query, [tenantId]);
            return results;
        } catch (error) {
            console.error(`SiteRepository::findActiveSites failed: ${error.message}`);
            throw error;
        }
    }

    async findBySiteType(siteType, tenantId) {
        try {
            const query = SiteQueries.findBySiteType();
            const results = await this.executeQuery(query, [siteType, tenantId]);
            return results;
        } catch (error) {
            console.error(`SiteRepository::findBySiteType failed: ${error.message}`);
            throw error;
        }
    }

    async searchByName(searchTerm, tenantId) {
        try {
            const query = SiteQueries.searchByName();
            const results = await this.executeQuery(query, [`%${searchTerm}%`, tenantId]);
            return results;
        } catch (error) {
            console.error(`SiteRepository::searchByName failed: ${error.message}`);
            throw error;
        }
    }

    async getHierarchyTree(tenantId) {
        try {
            const query = SiteQueries.getHierarchyTree();
            const results = await this.executeQuery(query, [tenantId, tenantId]);
            return results;
        } catch (error) {
            console.error(`SiteRepository::getHierarchyTree failed: ${error.message}`);
            throw error;
        }
    }

    async getSiteStatistics(tenantId) {
        try {
            const query = SiteQueries.getSiteStatistics();
            const results = await this.executeQuery(query, [tenantId]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`SiteRepository::getSiteStatistics failed: ${error.message}`);
            throw error;
        }
    }

    async updateHierarchy(id, hierarchyLevel, hierarchyPath) {
        try {
            const query = SiteQueries.updateHierarchy();
            const result = await this.executeNonQuery(query, [hierarchyLevel, hierarchyPath, id]);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`SiteRepository::updateHierarchy failed: ${error.message}`);
            throw error;
        }
    }

    async checkCodeExists(code, tenantId, excludeId = null) {
        try {
            let query = SiteQueries.checkCodeExists();
            let params = [code, tenantId];
            
            if (excludeId) {
                query += ' AND id != ?';
                params.push(excludeId);
            }
            
            const result = await this.executeQuerySingle(query, params);
            return result && result.count > 0;
        } catch (error) {
            console.error(`SiteRepository::checkCodeExists failed: ${error.message}`);
            throw error;
        }
    }
}

module.exports = SiteRepository;