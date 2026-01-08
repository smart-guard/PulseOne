// =============================================================================
// backend/lib/services/AlarmRuleService.js
// AlarmRule 비즈니스 로직을 처리하는 서비스 계층
// =============================================================================

const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

class AlarmRuleService extends BaseService {
    constructor() {
        super(null);
    }

    /**
     * Repository lazy loading
     */
    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getRepository('AlarmRuleRepository');
        }
        return this._repository;
    }

    /**
     * 모든 알람 규칙 조회 (필터 및 페이징 포함)
     */
    async getAlarmRules(filters = {}) {
        return await this.handleRequest(async () => {
            return await this.repository.findAll(filters);
        }, 'GetAlarmRules');
    }

    /**
     * ID로 알람 규칙 상세 조회
     */
    async getAlarmRuleById(id, tenantId) {
        return await this.handleRequest(async () => {
            const rule = await this.repository.findById(id, tenantId);
            if (!rule) {
                throw new Error(`Alarm rule with ID ${id} not found`);
            }
            return rule;
        }, 'GetAlarmRuleById');
    }

    /**
     * 알람 규칙 생성
     */
    async createAlarmRule(ruleData, userId) {
        return await this.handleRequest(async () => {
            return await this.repository.create(ruleData, userId);
        }, 'CreateAlarmRule');
    }

    /**
     * 알람 규칙 수정
     */
    async updateAlarmRule(id, updateData, tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.update(id, updateData, tenantId);
        }, 'UpdateAlarmRule');
    }

    /**
     * 알람 규칙 삭제
     */
    async deleteAlarmRule(id, tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.delete(id, tenantId);
        }, 'DeleteAlarmRule');
    }

    /**
     * 알람 규칙 활성화/비활성화 토글
     */
    async setRuleStatus(id, isEnabled, tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.updateEnabledStatus(id, isEnabled, tenantId);
        }, 'SetRuleStatus');
    }

    /**
     * 알람 규칙 통계 요약 조회
     */
    async getRuleStats(tenantId) {
        return await this.handleRequest(async () => {
            const [summary, bySeverity, byType, byCategory] = await Promise.all([
                this.repository.getStatsSummary(tenantId),
                this.repository.getStatsBySeverity(tenantId),
                this.repository.getStatsByType(tenantId),
                this.repository.getStatsByCategory(tenantId)
            ]);

            return {
                summary,
                by_severity: bySeverity,
                by_type: byType,
                by_category: byCategory
            };
        }, 'GetRuleStats');
    }

    /**
     * 특정 타겟(디바이스, 포인트 등)에 대한 알람 규칙 조회
     */
    async getRulesByTarget(targetType, targetId, tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.findByTarget(targetType, targetId, tenantId);
        }, 'GetRulesByTarget');
    }
}

module.exports = new AlarmRuleService();
