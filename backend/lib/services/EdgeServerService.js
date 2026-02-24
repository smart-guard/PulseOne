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

            // Redis에서 실시간 상태 병합
            try {
                const client = await this.redis.getRedisClient();
                if (client) {
                    for (const server of servers) {
                        const prefix = (server.server_type || 'collector').toLowerCase() === 'gateway'
                            ? 'gateway:status'
                            : 'collector:status';

                        const key = `${prefix}:${server.id}`;
                        const data = await client.get(key);
                        if (data) {
                            server.live_status = JSON.parse(data);
                        }
                    }
                }
            } catch (err) {
                // Ignore Redis errors for list view
            }

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
                // server.server_type determines the key prefix
                const liveStatus = await this.getLiveStatus(id, server.server_type);
                if (liveStatus) {
                    server.live_status = liveStatus;
                }
            } catch (ignored) { }

            return server;
        }, 'GetEdgeServerById');
    }

    /**
     * 에지 서버(게이트웨이/콜렉터)에 명령 전달 (Redis Pub/Sub)
     * @param {number} serverId 
     * @param {string} command 'manual_export', 'config:reload', 'target:reload' 등
     * @param {any} payload 명령 데이터
     */
    async sendCommand(serverId, command, payload = {}) {
        return await this.handleRequest(async () => {
            const server = await this.repository.findById(serverId);
            if (!server) throw new Error(`Server with ID ${serverId} not found`);

            const serverType = (server.server_type || 'collector').toLowerCase();
            let channel;

            // 명령 종류에 따른 채널 매핑 (C2 프로토콜 규격)
            if (command === 'config:reload' || command === 'target:reload') {
                // 브로드캐스트 명령
                channel = command;
            } else if (command === 'manual_export') {
                // 특정 대상 지정 명령
                channel = `cmd:${serverType}:${serverId}`;
            } else {
                // 기본 명령 패턴
                channel = `cmd:${serverType}:${serverId}:${command}`;
            }

            const message = {
                command,
                payload,
                serverId,
                serverType,
                timestamp: new Date().toISOString()
            };

            const client = await this.redis.getRedisClient();
            if (!client) throw new Error('Redis connection not available');

            const result = await client.publish(channel, JSON.stringify(message));

            console.log(`📡 [C2] Command sent to ${channel}:`, command);

            return {
                channel,
                command,
                recipient_count: result,
                timestamp: message.timestamp
            };
        }, 'SendCommand');
    }

    // ... (rest of the file until getLiveStatus)

    /**
     * 게이트웨이/콜렉터 실시간 상태 조회 (Redis)
     * @param {number} serverId 
     * @param {string} serverType 'collector' (default) or 'gateway'
     */
    /**
     * 게이트웨이/콜렉터 실시간 상태 조회 (Redis)
     * @param {number} serverId 
     * @param {string} serverType 'collector' (default) or 'gateway'
     */
    async getLiveStatus(serverId, serverType = 'collector') {
        try {
            const prefix = (serverType || 'collector').toLowerCase() === 'gateway'
                ? 'gateway:status'
                : 'collector:status';

            const key = `${prefix}:${serverId}`;
            const client = await this.redis.getRedisClient();
            if (!client) return null;

            const data = await client.get(key);
            return data ? JSON.parse(data) : null;
        } catch (error) {
            console.error(`Failed to get live status for server ${serverId}:`, error);
            return null;
        }
    }

    /**
     * 사이트별 Collector 목록 + 장치 수 조회 (단일 쿼리)
     */
    async getCollectorsBySite(siteId) {
        return await this.handleRequest(async () => {
            return await this.repository.findBySiteIdWithDeviceCount(siteId);
        }, 'GetCollectorsBySite');
    }

    /**
     * 테넌트 Collector 쿼터 현황 조회
     */
    async getQuotaStatus(tenantId) {
        return await this.handleRequest(async () => {
            const tenantRepo = require('../database/repositories/RepositoryFactory').getInstance().getTenantRepository();
            const tenant = await tenantRepo.findById(tenantId);
            if (!tenant) throw new Error('테넌트를 찾을 수 없습니다.');

            const used = await this.repository.countByTenant(tenantId);
            const max = tenant.max_edge_servers || 3;

            // 온라인/오프라인 집계
            const allCollectors = await this.repository.findAll(tenantId);
            const online = allCollectors.filter(c => c.status === 'active' || c.status === 'online').length;
            const offline = used - online;

            return {
                used,
                max,
                available: max - used,
                is_exceeded: used >= max,
                online,
                offline
            };
        }, 'GetQuotaStatus');
    }

    /**
     * 미배정 Collector 목록 (site_id IS NULL, 서버사이드 필터)
     */
    async getUnassignedCollectors(tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.findUnassigned(tenantId);
        }, 'GetUnassignedCollectors');
    }

    /**
     * Collector를 다른 사이트로 재배정
     * 조건: 연결된 device 수 = 0
     */
    async reassignToSite(collectorId, newSiteId, tenantId) {
        return await this.handleRequest(async () => {
            const collector = await this.repository.findById(collectorId, tenantId);
            if (!collector) throw new Error('Collector를 찾을 수 없습니다.');

            // 장치 연결 체크
            const deviceCount = await this.repository.countDevicesByCollector(collectorId);
            if (deviceCount > 0) {
                throw new Error(
                    `Collector에 장치 ${deviceCount}개가 연결되어 있어 이동할 수 없습니다.\n` +
                    `먼저 연결된 장치를 다른 Collector로 재배정하세요.`
                );
            }

            // site_id=0이면 미배정(NULL) 처리
            const effectiveSiteId = newSiteId === 0 ? null : newSiteId;
            const success = await this.repository.updateSiteId(collectorId, effectiveSiteId, tenantId);
            if (!success) throw new Error('Collector 재배정에 실패했습니다.');

            return await this.repository.findById(collectorId, tenantId);
        }, 'ReassignToSite');
    }

    /**
     * Collector 등록 (수동)
     */
    async registerEdgeServer(data, tenantId, user) {
        return await this.handleRequest(async () => {
            const effectiveTenantId = tenantId || data.tenant_id;
            if (!effectiveTenantId) throw new Error('테넌트 ID가 필요합니다.');

            const server = await this.repository.create({
                ...data,
                tenant_id: effectiveTenantId,
                server_type: data.server_type || 'collector',
                status: 'pending'
            });
            return server;
        }, 'RegisterEdgeServer');
    }

    /**
     * Collector 삭제
     * 조건: 연결된 device 수 = 0
     */
    async unregisterEdgeServer(collectorId, tenantId, user) {
        return await this.handleRequest(async () => {
            const collector = await this.repository.findById(collectorId, tenantId);
            if (!collector) throw new Error('Collector를 찾을 수 없습니다.');

            // 장치 연결 체크
            const deviceCount = await this.repository.countDevicesByCollector(collectorId);
            if (deviceCount > 0) {
                throw new Error(
                    `Collector에 장치 ${deviceCount}개가 연결되어 있어 삭제할 수 없습니다.\n` +
                    `먼저 연결된 장치를 다른 Collector로 재배정하세요.`
                );
            }

            const success = await this.repository.deleteById(collectorId, tenantId);
            return { success, id: collectorId };
        }, 'UnregisterEdgeServer');
    }

    /**
     * Edge Server 상태 업데이트
     */
    async updateEdgeServerStatus(id, status, remarks) {
        return await this.handleRequest(async () => {
            const updated = await this.repository.update(id, { status });
            return updated;
        }, 'UpdateEdgeServerStatus');
    }

    /**
     * 활성 Edge Server 목록
     */
    async getActiveEdgeServers(tenantId) {
        return await this.handleRequest(async () => {
            const servers = await this.repository.findAll(tenantId);
            return servers.filter(s => s.status === 'active');
        }, 'GetActiveEdgeServers');
    }
}

module.exports = new EdgeServerService();

