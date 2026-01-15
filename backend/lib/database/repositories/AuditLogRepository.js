const BaseRepository = require('./BaseRepository');

/**
 * AuditLogRepository class
 * Handles database operations for audit logs.
 */
class AuditLogRepository extends BaseRepository {
    constructor() {
        super('audit_logs');
    }

    /**
     * 감사 로그를 조회합니다.
     */
    async findAll(tenantId = null, options = {}) {
        try {
            const query = this.query().select('audit_logs.*', 'u.username');
            query.leftJoin('users as u', 'u.id', 'audit_logs.user_id');

            if (tenantId) {
                query.where('audit_logs.tenant_id', tenantId);
            }

            if (options.entityType) {
                query.where('entity_type', options.entityType);
            }

            if (options.entityId) {
                query.where('entity_id', options.entityId);
            }

            if (options.action) {
                query.where('action', options.action);
            }

            if (options.userId) {
                query.where('user_id', options.userId);
            }

            if (options.startDate) {
                query.where('audit_logs.created_at', '>=', options.startDate);
            }

            if (options.endDate) {
                query.where('audit_logs.created_at', '<=', options.endDate);
            }

            // Pagination
            const page = parseInt(options.page) || 1;
            const limit = parseInt(options.limit) || 20;
            const offset = (page - 1) * limit;

            query.orderBy('audit_logs.created_at', 'DESC');

            const items = await query.limit(limit).offset(offset);

            // Total count
            const countQuery = this.query();
            if (tenantId) countQuery.where('tenant_id', tenantId);
            if (options.entityType) countQuery.where('entity_type', options.entityType);
            if (options.entityId) countQuery.where('entity_id', options.entityId);
            const countResult = await countQuery.count('id as total').first();
            const total = countResult ? (countResult.total || 0) : 0;

            return { items, total, page, limit };
        } catch (error) {
            this.logger.error('AuditLogRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * 새로운 감사 로그를 생성합니다.
     */
    async create(data) {
        try {
            const [id] = await this.query().insert({
                tenant_id: data.tenant_id,
                user_id: data.user_id,
                action: data.action,
                entity_type: data.entity_type,
                entity_id: data.entity_id,
                entity_name: data.entity_name || null,
                old_value: typeof data.old_value === 'object' ? JSON.stringify(data.old_value) : (data.old_value || null),
                new_value: typeof data.new_value === 'object' ? JSON.stringify(data.new_value) : (data.new_value || null),
                change_summary: data.change_summary || null,
                ip_address: data.ip_address || null,
                user_agent: data.user_agent || null,
                request_id: data.request_id || null,
                severity: data.severity || 'info',
                details: typeof data.details === 'object' ? JSON.stringify(data.details) : (data.details || null)
            });
            return id;
        } catch (error) {
            this.logger.error('AuditLogRepository.create 오류:', error);
            throw error;
        }
    }
}

module.exports = AuditLogRepository;
