/**
 * backend/lib/services/ProtocolService.js
 * 프로토콜 관련 비즈니스 로직 처리 서비스
 */

const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

class ProtocolService extends BaseService {
    constructor() {
        super(null);
    }

    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getRepository('ProtocolRepository');
        }
        return this._repository;
    }

    /**
     * 프로토콜 목록 조회 (필터링 및 통계 포함)
     */
    async getProtocols(filters = {}) {
        return await this.handleRequest(async () => {
            const protocols = await this.repository.findAll(filters);
            const totalCount = await this.repository.getTotalCount(filters);

            return {
                items: protocols,
                total_count: totalCount
            };
        }, 'GetProtocols');
    }

    /**
     * 프로토콜 상세 조회
     */
    async getProtocolById(id, tenantId = null) {
        return await this.handleRequest(async () => {
            const protocol = await this.repository.findById(id, tenantId);
            if (!protocol) throw new Error('Protocol not found');
            return protocol;
        }, 'GetProtocolById');
    }

    /**
     * 카테고리별 프로토콜 조회
     */
    async getProtocolsByCategory(category) {
        return await this.handleRequest(async () => {
            return await this.repository.findByCategory(category);
        }, 'GetProtocolsByCategory');
    }

    /**
     * 프로토콜 생성
     */
    async createProtocol(protocolData, userId = null) {
        return await this.handleRequest(async () => {
            // 중복 체크
            const exists = await this.repository.checkProtocolTypeExists(protocolData.protocol_type);
            if (exists) throw new Error('Protocol type already exists');

            return await this.repository.create(protocolData, userId);
        }, 'CreateProtocol');
    }

    /**
     * 프로토콜 업데이트
     */
    async updateProtocol(id, updateData) {
        return await this.handleRequest(async () => {
            return await this.repository.update(id, updateData);
        }, 'UpdateProtocol');
    }

    /**
     * 프로토콜 삭제
     */
    async deleteProtocol(id, force = false) {
        return await this.handleRequest(async () => {
            return await this.repository.delete(id, force);
        }, 'DeleteProtocol');
    }

    /**
     * 프로토콜 활성화/비활성화
     */
    async setProtocolStatus(id, enabled) {
        return await this.handleRequest(async () => {
            if (enabled) {
                return await this.repository.enable(id);
            } else {
                return await this.repository.disable(id);
            }
        }, enabled ? 'EnableProtocol' : 'DisableProtocol');
    }

    /**
     * 프로토콜 통계 조회
     */
    async getProtocolStatistics(tenantId = null) {
        return await this.handleRequest(async () => {
            const [counts, categoryStats, usageStats] = await Promise.all([
                this.repository.getCounts(),
                this.repository.getStatsByCategory(),
                this.repository.getUsageStats(tenantId)
            ]);

            return {
                ...counts,
                categories: categoryStats,
                usage_stats: usageStats
            };
        }, 'GetProtocolStatistics');
    }

    /**
     * 프로토콜별 디바이스 목록 조회
     */
    async getDevicesByProtocol(id, limit = 50, offset = 0) {
        return await this.handleRequest(async () => {
            return await this.repository.getDevicesByProtocol(id, limit, offset);
        }, 'GetDevicesByProtocol');
    }

    /**
     * 프로토콜 연결 테스트 시뮬레이션
     */
    async testConnection(id, testParams, tenantId = null) {
        return await this.handleRequest(async () => {
            const protocol = await this.repository.findById(id, tenantId);
            if (!protocol) throw new Error('Protocol not found');

            const startTime = Date.now();
            let testSuccessful = false;
            let errorMessage = null;

            // 시뮬레이션 로직 (기존 route에서 이동)
            try {
                // 프로토콜별 기본 검증 로직
                if (protocol.protocol_type === 'MODBUS_TCP') {
                    if (!testParams.host || !testParams.port) {
                        throw new Error('Host and port are required for Modbus TCP');
                    }
                    await new Promise(resolve => setTimeout(resolve, Math.random() * 150 + 50));
                    testSuccessful = Math.random() > 0.1;
                } else if (protocol.protocol_type === 'MQTT') {
                    if (!testParams.broker_url) {
                        throw new Error('Broker URL is required for MQTT');
                    }
                    await new Promise(resolve => setTimeout(resolve, Math.random() * 100 + 30));
                    testSuccessful = Math.random() > 0.05;
                } else if (protocol.protocol_type === 'BACNET') {
                    if (!testParams.device_instance) {
                        throw new Error('Device instance is required for BACnet');
                    }
                    await new Promise(resolve => setTimeout(resolve, Math.random() * 300 + 100));
                    testSuccessful = Math.random() > 0.15;
                } else {
                    await new Promise(resolve => setTimeout(resolve, Math.random() * 200 + 50));
                    testSuccessful = Math.random() > 0.2;
                }

                if (!testSuccessful) {
                    errorMessage = 'Connection timeout or device not responding';
                }
            } catch (error) {
                testSuccessful = false;
                errorMessage = error.message;
            }

            return {
                protocol_id: parseInt(id),
                protocol_type: protocol.protocol_type,
                test_successful: testSuccessful,
                response_time_ms: Date.now() - startTime,
                test_timestamp: new Date().toISOString(),
                error_message: errorMessage
            };
        }, 'TestConnection');
    }
}

module.exports = new ProtocolService();
