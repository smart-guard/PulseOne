const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

/**
 * AuditLogService class
 * Handles business logic for audit logs.
 */
class AuditLogService extends BaseService {
    constructor() {
        super(null);
    }

    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getAuditLogRepository();
        }
        return this._repository;
    }

    /**
     * 감사 로그를 조회합다.
     */
    async getLogs(tenantId, options = {}) {
        return await this.handleRequest(async () => {
            return await this.repository.findAll(tenantId, options);
        }, 'GetAuditLogs');
    }

    /**
     * 새로운 감사 로그를 저장합니다.
     */
    async logAction(data) {
        return await this.handleRequest(async () => {
            return await this.repository.create(data);
        }, 'LogAuditAction');
    }

    /**
     * 변경 사항을 자동으로 감 로그로 기록하는 헬퍼 메서드
     */
    async logChange(user, action, entity, oldValue, newValue, summary) {
        return await this.logAction({
            tenant_id: user.tenant_id,
            user_id: user.id,
            action: action,
            entity_type: entity.type,
            entity_id: entity.id,
            entity_name: entity.name,
            old_value: oldValue,
            new_value: newValue,
            change_summary: summary,
            severity: 'info'
        });
    }
}

module.exports = AuditLogService;
