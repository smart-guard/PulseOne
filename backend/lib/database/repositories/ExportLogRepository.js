const BaseRepository = require('./BaseRepository');

/**
 * @class ExportLogRepository
 * @description Repository for managing export_logs table
 */
class ExportLogRepository extends BaseRepository {
    constructor() {
        super('export_logs');
    }

    /**
     * Export Log 목록 조회 (페이징 및 필터링)
     * @param {Object} filters 필터 조건 (target_id, status, log_type)
     * @param {number} page 페이지 번호
     * @param {number} limit 페이지당 항목 수
     * @returns {Promise<Object>} 로그 목록 및 페이징 정보
     */
    async findAll(filters = {}, page = 1, limit = 50) {
        try {
            // Use Knex Query Builder via this.knex
            const query = this.knex(`${this.tableName} as l`)
                .leftJoin('export_targets as t', 'l.target_id', 't.id')
                .leftJoin('export_profiles as p', 't.profile_id', 'p.id')
                .leftJoin('edge_servers as g', 'l.gateway_id', 'g.id')
                .select(
                    'l.*',
                    't.name as target_name',
                    't.target_type',
                    'p.name as profile_name',
                    'g.server_name as gateway_name'
                );

            if (filters.target_id) {
                query.where('l.target_id', filters.target_id);
            }
            if (filters.status) {
                query.where('l.status', filters.status);
            }
            if (filters.log_type) {
                query.where('l.log_type', filters.log_type);
            }
            if (filters.date_from) {
                query.where('l.timestamp', '>=', filters.date_from);
            }
            if (filters.date_to) {
                query.where('l.timestamp', '<=', filters.date_to);
            }
            if (filters.target_type) {
                query.whereRaw('LOWER(t.target_type) = ?', [filters.target_type.toLowerCase()]);
            }

            if (filters.search_term) {
                query.where(function () {
                    this.where('t.name', 'like', `%${filters.search_term}%`)
                        .orWhere('p.name', 'like', `%${filters.search_term}%`)
                        .orWhere('g.server_name', 'like', `%${filters.search_term}%`);
                });
            }

            if (filters.gateway_id) {
                query.where('l.gateway_id', filters.gateway_id);
            }

            // Get total count
            const countResult = await query.clone().clearSelect().count('l.id as total').first();
            const total = parseInt(countResult?.total || 0);

            // Pagination
            const offset = (page - 1) * limit;
            const items = await query.orderBy('l.timestamp', 'DESC').offset(offset).limit(limit);

            return {
                items,
                pagination: {
                    current_page: parseInt(page),
                    total_pages: Math.ceil(total / limit),
                    total_count: total,
                    items_per_page: parseInt(limit)
                }
            };
        } catch (error) {
            this.logger.error('ExportLogRepository.findAll Error:', error);
            throw error;
        }
    }

    async getStatistics(filters = {}) {
        try {
            const query = this.knex(`${this.tableName} as l`)
                .leftJoin('export_targets as t', 'l.target_id', 't.id')
                .leftJoin('export_profiles as p', 't.profile_id', 'p.id')
                .leftJoin('edge_servers as g', 'l.gateway_id', 'g.id');

            if (filters.date_from) {
                query.where('l.timestamp', '>=', filters.date_from);
            }
            if (filters.date_to) {
                query.where('l.timestamp', '<=', filters.date_to);
            }
            if (filters.target_id) {
                query.where('l.target_id', filters.target_id);
            }
            if (filters.target_type) {
                query.whereRaw('LOWER(t.target_type) = ?', [filters.target_type.toLowerCase()]);
            }
            if (filters.search_term) {
                query.where(function () {
                    this.where('t.name', 'like', `%${filters.search_term}%`)
                        .orWhere('p.name', 'like', `%${filters.search_term}%`)
                        .orWhere('g.server_name', 'like', `%${filters.search_term}%`);
                });
            }

            if (filters.gateway_id) {
                query.where('l.gateway_id', filters.gateway_id);
            }

            const stats = await query.select(
                this.knex.raw('COUNT(*) as total'),
                this.knex.raw("SUM(CASE WHEN UPPER(l.status) = 'SUCCESS' THEN 1 ELSE 0 END) as success"),
                this.knex.raw("SUM(CASE WHEN UPPER(l.status) = 'FAILURE' THEN 1 ELSE 0 END) as failure"),
                this.knex.raw('MAX(l.timestamp) as last_dispatch')
            ).first();

            return {
                total: parseInt(stats?.total || 0),
                success: parseInt(stats?.success || 0),
                failure: parseInt(stats?.failure || 0),
                last_dispatch: stats?.last_dispatch || null
            };
        } catch (error) {
            this.logger.error('ExportLogRepository.getStatistics Error:', error);
            throw error;
        }
    }
}

module.exports = ExportLogRepository;
