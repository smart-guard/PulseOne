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

    get edgeServerRepo() {
        if (!this._edgeServerRepo) {
            this._edgeServerRepo = RepositoryFactory.getInstance().getEdgeServerRepository();
        }
        return this._edgeServerRepo;
    }

    get tenantRepo() {
        if (!this._tenantRepo) {
            this._tenantRepo = RepositoryFactory.getInstance().getTenantRepository();
        }
        return this._tenantRepo;
    }

    /**
     * Get all sites for a tenant
     */
    async getAllSites(tenantId = null, options = {}) {
        return await this.handleRequest(async () => {
            return await this.repository.findAll(tenantId, options);
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
     * - 테넌트 max_edge_servers 쿼터 체크
     * - 사이트 생성 후 Collector 1개 자동 생성
     */
    async createSite(siteData, tenantId = null) {
        return await this.handleRequest(async () => {
            // Ensure tenant ID
            if (tenantId) siteData.tenant_id = tenantId;
            if (!siteData.tenant_id) throw new Error('테넌트 ID가 필요합니다.');

            // ── 쿼터 체크 ──────────────────────────────────────
            const tenant = await this.tenantRepo.findById(siteData.tenant_id);
            if (!tenant) throw new Error('테넌트를 찾을 수 없습니다.');

            const maxAllowed = tenant.max_edge_servers || 3;
            const usedCount = await this.edgeServerRepo.countByTenant(siteData.tenant_id);

            if (usedCount >= maxAllowed) {
                throw new Error(
                    `Collector 할당 한도 초과 (사용 ${usedCount}/${maxAllowed}).\n` +
                    `시스템 관리자에게 한도 증설을 요청하세요.`
                );
            }
            // ─────────────────────────────────────────────────────

            // 계층 정보 처리
            if (siteData.parent_site_id) {
                const parent = await this.repository.findById(siteData.parent_site_id, siteData.tenant_id);
                if (!parent) throw new Error('상위 사이트가 존재하지 않거나 권한이 없습니다.');
                siteData.hierarchy_level = (parent.hierarchy_level || 1) + 1;
                siteData.hierarchy_path = `${parent.hierarchy_path || ''}${parent.id}/`;
            } else {
                siteData.hierarchy_level = 1;
                siteData.hierarchy_path = '/';
            }

            // 사이트 생성
            const siteId = await this.repository.create(siteData);

            // ── Collector 자동 생성 ────────────────────────────
            const collectorName = siteData.collector_name ||
                `${siteData.site_name || siteData.name || 'Site'}-Collector`;

            await this.edgeServerRepo.create({
                tenant_id: siteData.tenant_id,
                site_id: siteId,
                server_name: collectorName,
                server_type: 'collector',
                description: siteData.collector_description || `${collectorName} (자동 생성)`,
                status: 'pending',
            });
            // ─────────────────────────────────────────────────────

            return await this.repository.findById(siteId, siteData.tenant_id);
        }, 'SiteService.createSite');
    }

    /**
     * Update site information
     */
    async updateSite(id, siteData, tenantId = null) {
        return await this.handleRequest(async () => {
            const existing = await this.repository.findById(id, tenantId);
            if (!existing) throw new Error('사이트를 찾을 수 없거나 권한이 없습니다.');

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
     * - 하위 사이트 존재 시 차단
     * - 연결된 Collector 존재 시 차단
     */
    async deleteSite(id, tenantId = null) {
        return await this.handleRequest(async () => {
            const existing = await this.repository.findById(id, tenantId);
            if (!existing) throw new Error('사이트를 찾을 수 없거나 권한이 없습니다.');

            // 하위 사이트 체크
            const children = await this.repository.findByParentId(id, tenantId);
            if (children && children.length > 0) {
                throw new Error('하위 사이트가 존재하여 삭제할 수 없습니다.');
            }

            // ── 연결된 Collector 체크 ──────────────────────────
            const collectors = await this.edgeServerRepo.findBySiteId(id);
            if (collectors && collectors.length > 0) {
                throw new Error(
                    `이 사이트에 Collector ${collectors.length}개가 연결되어 있습니다.\n` +
                    `Collector를 다른 사이트로 이동하거나 삭제한 후 시도하세요.`
                );
            }
            // ─────────────────────────────────────────────────────

            const success = await this.repository.deleteById(id, tenantId);
            return { success };
        }, 'SiteService.deleteSite');
    }

    /**
     * Restore a deleted site
     */
    async restoreSite(id, tenantId = null) {
        return await this.handleRequest(async () => {
            const success = await this.repository.restoreById(id, tenantId);
            if (!success) throw new Error('사이트 복구에 실패했습니다.');
            return { success };
        }, 'SiteService.restoreSite');
    }

    /**
     * Get hierarchy tree
     */
    async getHierarchyTree(tenantId = null) {
        return await this.handleRequest(async () => {
            const sites = await this.repository.getHierarchyTree(tenantId);

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
