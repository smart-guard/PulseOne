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
}

module.exports = new EdgeServerService();
