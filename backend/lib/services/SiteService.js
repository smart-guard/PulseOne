// =============================================================================
// backend/lib/services/SiteService.js
// Site component business logic
// =============================================================================

const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

class SiteService extends BaseService {
    constructor() {
        super(null);
    }

    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getRepository('SiteRepository');
        }
        return this._repository;
    }

    /**
     * Get all sites for a tenant
     */
    async getAllSites(tenantId = null) {
        return await this.handleRequest(async () => {
            return await this.repository.findAll(tenantId);
        }, 'SiteService.getAllSites');
    }

    /**
     * Get a specific site by ID
     */
    async getSiteById(id, tenantId = null) {
        return await this.handleRequest(async () => {
            const site = await this.repository.findById(id, tenantId);
            if (!site) throw new Error('사이트를 찾을 수 없습니다.');
            return site;
        }, 'SiteService.getSiteById');
    }

    /**
     * Find sites by parent ID
     */
    async getSitesByParentId(parentId, tenantId = null) {
        return await this.handleRequest(async () => {
            return await this.repository.findByParentId(parentId, tenantId);
        }, 'SiteService.getSitesByParentId');
    }

    /**
     * Create a new site
     */
    async createSite(siteData, tenantId = null) {
        return await this.handleRequest(async () => {
            // Ensure tenant ID
            if (tenantId) siteData.tenant_id = tenantId;
            if (!siteData.tenant_id) throw new Error('테넌트 ID가 필요합니다.');

            // Check if parent site exists and belongs to same tenant
            if (siteData.parent_site_id) {
                const parent = await this.repository.findById(siteData.parent_site_id, siteData.tenant_id);
                if (!parent) throw new Error('상위 사이트가 존재하지 않거나 권한이 없습니다.');

                siteData.hierarchy_level = (parent.hierarchy_level || 1) + 1;
                siteData.hierarchy_path = `${parent.hierarchy_path || ''}${parent.id}/`;
            } else {
                siteData.hierarchy_level = 1;
                siteData.hierarchy_path = '/';
            }

            const siteId = await this.repository.create(siteData);
            return { id: siteId, name: siteData.name };
        }, 'SiteService.createSite');
    }

    /**
     * Update site information
     */
    async updateSite(id, siteData, tenantId = null) {
        return await this.handleRequest(async () => {
            // Verify existence and tenant
            const existing = await this.repository.findById(id, tenantId);
            if (!existing) throw new Error('사이트를 찾을 수 없거나 권한이 없습니다.');

            // Hierarchy update logic could be complex, keeping it simple for now
            // In a real scenario, moving a site would require updating all its children's paths.

            const success = await this.repository.update(id, siteData, tenantId);
            return { success };
        }, 'SiteService.updateSite');
    }

    /**
     * Partially update site information
     */
    async patchSite(id, siteData, tenantId = null) {
        return await this.handleRequest(async () => {
            const existing = await this.repository.findById(id, tenantId);
            if (!existing) throw new Error('사이트를 찾을 수 없거나 권한이 없습니다.');

            const success = await this.repository.updatePartial(id, siteData, tenantId);
            return { success };
        }, 'SiteService.patchSite');
    }

    /**
     * Delete a site
     */
    async deleteSite(id, tenantId = null) {
        return await this.handleRequest(async () => {
            const existing = await this.repository.findById(id, tenantId);
            if (!existing) throw new Error('사이트를 찾을 수 없거나 권한이 없습니다.');

            // Check for children
            const children = await this.repository.findByParentId(id, tenantId);
            if (children && children.length > 0) {
                throw new Error('하위 사이트가 존재하여 삭제할 수 없습니다.');
            }

            const success = await this.repository.deleteById(id, tenantId);
            return { success };
        }, 'SiteService.deleteSite');
    }

    /**
     * Get hierarchy tree
     */
    async getHierarchyTree(tenantId = null) {
        return await this.handleRequest(async () => {
            const sites = await this.repository.getHierarchyTree(tenantId);

            // Build tree structure
            const siteMap = {};
            const tree = [];

            sites.forEach(site => {
                site.children = [];
                siteMap[site.id] = site;
            });

            sites.forEach(site => {
                if (site.parent_site_id && siteMap[site.parent_site_id]) {
                    siteMap[site.parent_site_id].children.push(site);
                } else {
                    tree.push(site);
                }
            });

            return tree;
        }, 'SiteService.getHierarchyTree');
    }
}

module.exports = new SiteService();
