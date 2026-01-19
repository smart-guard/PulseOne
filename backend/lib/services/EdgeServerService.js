const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');
const redisClient = require('../connection/redis');
const LogManager = require('../utils/LogManager'); // Assuming a LogManager exists, or use console

/**
 * EdgeServerService class
 * Handles business logic for collector instances (edge_servers).
 */
class EdgeServerService extends BaseService {
    constructor() {
        super(null);
        this.redis = redisClient;
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
            // DB 목록 가져오기
            const servers = await this.repository.findAll(tenantId);

            // Redis에서 실시간 상태 병합 (Optional)
            // 성능을 위해 필요 시 별도 메서드로 분리 전권장
            return servers;
        }, 'GetAllEdgeServers');
    }

    /**
     * 에지 서버 상세 조회 (기존 getServerDetail)
     */
    async getEdgeServerById(id, tenantId) {
        return await this.handleRequest(async () => {
            const server = await this.repository.findById(id, tenantId);
            if (!server) throw new Error('Server not found');

            // 실시간 상태 조회 병합
            try {
                const liveStatus = await this.getLiveStatus(id);
                if (liveStatus) {
                    server.live_status = liveStatus;
                }
            } catch (ignored) { }

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

    // =========================================================================
    // 📡 Gateway Command & Control (C2) Methods
    // =========================================================================

    /**
     * 게이트웨이로 명령 전송 (Redis Pub/Sub)
     * @param {number} serverId 
     * @param {string} commandType 'config:reload', 'service:restart', etc
     * @param {object} payload 
     */
    async sendCommand(serverId, commandType, payload = {}) {
        return await this.handleRequest(async () => {
            const channel = `cmd:gateway:${serverId}`; // 특정 게이트웨이 지정
            // 또는 광역 채널 사용 시: `config:reload` (모든 게이트웨이가 구독 중인 채널)

            // 현재 C++ 구현은 'config:reload' 채널을 구독하므로, 
            // 개별 제어보다는 Broadcast 방식으로 구현되어 있음.
            // 개별 제어를 위해서는 C++이 `cmd:gateway:{ID}`를 구독해야 함.
            // 우선 계획된 'config:reload' 채널로 발행.

            const targetChannel = commandType === 'config:reload' ? 'config:reload' : `cmd:gateway:${serverId}`;

            const message = JSON.stringify({
                command: commandType,
                payload: payload,
                timestamp: Date.now()
            });

            // RedisManager proxy handles async connection internally if using the direct proxy methods,
            // but let's be explicit to ensure it works.
            const client = await this.redis.getRedisClient();
            if (!client) throw new Error('Redis client not available');

            await client.publish(targetChannel, message);

            return { success: true, channel: targetChannel, command: commandType };
        }, 'SendCommand');
    }

    /**
     * 게이트웨이 실시간 상태 조회 (Redis)
     * @param {number} serverId 
     */
    async getLiveStatus(serverId) {
        try {
            const key = `gateway:status:${serverId}`;
            const client = await this.redis.getRedisClient();
            if (!client) return null;

            const data = await client.get(key);
            return data ? JSON.parse(data) : null;
        } catch (error) {
            console.error(`Failed to get live status for server ${serverId}:`, error);
            return null;
        }
    }
}

module.exports = new EdgeServerService();
