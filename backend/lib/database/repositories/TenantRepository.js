// =============================================================================
// backend/lib/database/repositories/TenantRepository.js
// 🔧 DeviceRepository 패턴과 완전 동일 (Knex 기반 리팩토링)
// =============================================================================

const BaseRepository = require('./BaseRepository');

class TenantRepository extends BaseRepository {
    constructor() {
        super('tenants');
    }

    /**
     * 모든 테넌트를 조회합니다 (페이징/검색 지원).
     */
    async findAll(options = {}) {
        try {
            const buildQuery = (builder) => {
                if (options.isActive !== undefined) {
                    builder.where('is_active', options.isActive ? 1 : 0);
                }
                if (options.search) {
                    builder.where(function () {
                        this.where('company_name', 'like', `%${options.search}%`)
                            .orWhere('company_code', 'like', `%${options.search}%`)
                            .orWhere('domain', 'like', `%${options.search}%`);
                    });
                }

                // 삭제된 항목 필터링
                if (options.onlyDeleted === true) {
                    builder.where('is_deleted', 1);
                } else if (options.includeDeleted !== true) {
                    builder.where('is_deleted', 0);
                }

                return builder;
            };

            // 1. 전체 카운트 조회
            const countBuilder = this.query().count('* as count');
            buildQuery(countBuilder);
            const countResult = await countBuilder.first();
            const total = countResult ? (parseInt(countResult.count) || 0) : 0;

            // 2. 데이터 조회
            const query = this.query().select('*');
            buildQuery(query);

            const limit = parseInt(options.limit) || 20;
            const page = Math.max(1, parseInt(options.page) || 1);
            const offset = (page - 1) * limit;

            const items = await query.orderBy('company_name', 'asc').limit(limit).offset(offset);

            return {
                items,
                pagination: {
                    total_count: total,
                    current_page: page,
                    total_pages: Math.ceil(total / limit),
                    limit
                }
            };
        } catch (error) {
            console.error(`TenantRepository::findAll failed: ${error.message}`);
            throw error;
        }
    }

    async findById(id) {
        try {
            return await this.query().where('id', id).first();
        } catch (error) {
            console.error(`TenantRepository::findById failed: ${error.message}`);
            throw error;
        }
    }

    async findByDomain(domain) {
        try {
            return await this.query().where('domain', domain).where('is_deleted', 0).first();
        } catch (error) {
            console.error(`TenantRepository::findByDomain failed: ${error.message}`);
            throw error;
        }
    }

    async findByCompanyCode(companyCode) {
        try {
            return await this.query().where('company_code', companyCode).where('is_deleted', 0).first();
        } catch (error) {
            console.error(`TenantRepository::findByCompanyCode failed: ${error.message}`);
            throw error;
        }
    }

    async create(tenantData) {
        try {
            const [id] = await this.query().insert({
                company_name: tenantData.company_name,
                company_code: tenantData.company_code,
                domain: tenantData.domain || null,
                contact_name: tenantData.contact_name || null,
                contact_email: tenantData.contact_email || null,
                contact_phone: tenantData.contact_phone || null,
                subscription_plan: tenantData.subscription_plan || 'starter',
                subscription_status: tenantData.subscription_status || 'active',
                max_edge_servers: tenantData.max_edge_servers || 3,
                max_data_points: tenantData.max_data_points || 1000,
                max_users: tenantData.max_users || 5,
                is_active: tenantData.is_active !== false ? 1 : 0,
                trial_end_date: tenantData.trial_end_date || null
            });
            return id;
        } catch (error) {
            console.error(`TenantRepository::create failed: ${error.message}`);
            throw error;
        }
    }

    async update(id, tenantData) {
        try {
            const affected = await this.query().where('id', id).update({
                company_name: tenantData.company_name,
                company_code: tenantData.company_code,
                domain: tenantData.domain || null,
                contact_name: tenantData.contact_name || null,
                contact_email: tenantData.contact_email || null,
                contact_phone: tenantData.contact_phone || null,
                subscription_plan: tenantData.subscription_plan || 'starter',
                subscription_status: tenantData.subscription_status || 'active',
                max_edge_servers: tenantData.max_edge_servers || 3,
                max_data_points: tenantData.max_data_points || 1000,
                max_users: tenantData.max_users || 5,
                is_active: tenantData.is_active !== false ? 1 : 0,
                trial_end_date: tenantData.trial_end_date || null,
                updated_at: this.knex.raw("datetime('now', 'localtime')")
            });
            return affected > 0;
        } catch (error) {
            console.error(`TenantRepository::update failed: ${error.message}`);
            throw error;
        }
    }

    async deleteById(id) {
        try {
            const affected = await this.query().where('id', id).update({
                is_deleted: 1,
                updated_at: this.knex.raw("datetime('now', 'localtime')")
            });
            return affected > 0;
        } catch (error) {
            console.error(`TenantRepository::deleteById (soft) failed: ${error.message}`);
            throw error;
        }
    }

    async restoreById(id) {
        try {
            const affected = await this.query().where('id', id).update({
                is_deleted: 0,
                updated_at: this.knex.raw("datetime('now', 'localtime')")
            });
            return affected > 0;
        } catch (error) {
            console.error(`TenantRepository::restoreById failed: ${error.message}`);
            throw error;
        }
    }

    async checkCompanyNameExists(companyName, excludeId = null) {
        try {
            let query = this.query().where('company_name', companyName);
            if (excludeId) query = query.whereNot('id', excludeId);
            const result = await query.count('* as count').first();
            return result && result.count > 0;
        } catch (error) {
            console.error(`TenantRepository::checkCompanyNameExists failed: ${error.message}`);
            throw error;
        }
    }

    async checkCompanyCodeExists(companyCode, excludeId = null) {
        try {
            let query = this.query().where('company_code', companyCode);
            if (excludeId) query = query.whereNot('id', excludeId);
            const result = await query.count('* as count').first();
            return result && result.count > 0;
        } catch (error) {
            console.error(`TenantRepository::checkCompanyCodeExists failed: ${error.message}`);
            throw error;
        }
    }

    async getGlobalStatistics() {
        try {
            const totalResult = await this.query().where('is_deleted', 0).count('* as count').first();
            const activeResult = await this.query().where('is_deleted', 0).where('subscription_status', 'active').count('* as count').first();
            const trialResult = await this.query().where('is_deleted', 0).where('subscription_status', 'trial').count('* as count').first();

            return {
                total_tenants: parseInt(totalResult.count) || 0,
                active_tenants: parseInt(activeResult.count) || 0,
                trial_tenants: parseInt(trialResult.count) || 0
            };
        } catch (error) {
            console.error(`TenantRepository::getGlobalStatistics failed: ${error.message}`);
            throw error;
        }
    }

    async getTenantStatistics(id, trx = null) {
        try {
            const knex = trx || this.knex;
            // Simplified stats
            const tenant = await this.query(trx).where('id', id).first();
            if (!tenant) return null;

            const edgeServers = await knex('edge_servers')
                .where('tenant_id', id)
                .where('is_deleted', 0)
                .count('* as count')
                .first();

            const sites = await knex('sites')
                .where('tenant_id', id)
                .where('is_deleted', 0)
                .count('* as count')
                .first();

            const users = await knex('users')
                .where('tenant_id', id)
                .count('* as count')
                .first();

            // 데이터 포인트 합계 (활성화된 디바이스의 활성화된 데이터 포인트만 합산)
            const dataPoints = await knex('data_points as dp')
                .join('devices as d', 'dp.device_id', 'd.id')
                .where('d.tenant_id', id)
                .where('d.is_deleted', 0)
                .where('d.is_enabled', 1)
                .where('dp.is_enabled', 1)
                .count('* as count')
                .first();

            return {
                ...tenant,
                edge_servers_count: parseInt(edgeServers.count) || 0,
                data_points_count: parseInt(dataPoints.count) || 0,
                sites_count: parseInt(sites.count) || 0,
                users_count: parseInt(users.count) || 0
            };
        } catch (error) {
            console.error(`TenantRepository::getTenantStatistics failed: ${error.message}`);
            throw error;
        }
    }
}

module.exports = TenantRepository;