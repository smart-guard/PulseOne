const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

/**
 * EdgeServerService class
 * Handles business logic for collector instances (edge_servers).
 */
class EdgeServerService extends BaseService {
    constructor() {
        super(null);
    }

    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getEdgeServerRepository();
        }
        return this._repository;
    }

    /**
     * 모든 에지 서버 조회 (기존 getAllServers)
     */
    async getAllEdgeServers(tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.findAll(tenantId);
        }, 'GetAllEdgeServers');
    }

    /**
     * 에지 서버 상세 조회 (기존 getServerDetail)
     */
    async getEdgeServerById(id, tenantId) {
        return await this.handleRequest(async () => {
            const server = await this.repository.findById(id, tenantId);
            if (!server) throw new Error('Server not found');
            return server;
        }, 'GetEdgeServerById');
    }

    /**
     * 활성 에지 서버 목록 조회
     */
    async getActiveEdgeServers(tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.findActive(tenantId);
        }, 'GetActiveEdgeServers');
    }

    /**
     * 서버 상태 및 메트릭 업데이트 (Edge 서버로부터의 하트비트)
     */
    async updateEdgeServerStatus(id, status, remarks) {
        return await this.handleRequest(async () => {
            const updateData = {
                status: status || 'active',
                last_seen: new Date(),
                remarks: remarks
            };
            return await this.repository.update(id, updateData);
        }, 'UpdateEdgeServerStatus');
    }

    /**
     * 신규 서버 등록 (기존 registerServer)
     */
    async registerEdgeServer(serverData, tenantId) {
        return await this.handleRequest(async () => {
            // 1. 테넌트의 한도 정보 조회
            const TenantService = require('./TenantService');
            const tenantService = new TenantService();
            const tenant = await tenantService.getTenantById(tenantId);

            if (!tenant) {
                throw new Error('고객사 정보를 찾을 수 없습니다.');
            }

            // 2. 현재 등록된 서버 수 조회
            const currentServers = await this.repository.findAll(tenantId);
            const activeCount = currentServers.length;

            // 3. 한도 체크
            const maxLimit = tenant.max_edge_servers || 1;
            if (activeCount >= maxLimit) {
                throw new Error(`EDGE 서버 등록 한도(${maxLimit}대)를 초과했습니다. 더 이상 등록할 수 없습니다.`);
            }

            if (!serverData.registration_token) {
                serverData.registration_token = Buffer.from(`${tenantId}-${serverData.server_name}-${Date.now()}`).toString('base64');
            }
            return await this.repository.create(serverData, tenantId);
        }, 'RegisterEdgeServer');
    }

    /**
     * 서버 등록 해제 (기존 deleteServer)
     */
    async unregisterEdgeServer(id, tenantId) {
        return await this.handleRequest(async () => {
            const success = await this.repository.deleteById(id, tenantId);
            if (!success) throw new Error('Server not found or delete failed');
            return { id, success: true };
        }, 'UnregisterEdgeServer');
    }
}

module.exports = new EdgeServerService();
