// =============================================================================
// backend/lib/database/repositories/TenantRepository.js
// 🔧 DeviceRepository 패턴과 완전 동일 (쿼리 분리 적용)
// =============================================================================

const BaseRepository = require('./BaseRepository');
const TenantQueries = require('../queries/TenantQueries');

class TenantRepository extends BaseRepository {
    constructor() {
        // DeviceRepository와 동일한 패턴: 매개변수 없는 생성자
        super('tenants');
        console.log('🏢 TenantRepository initialized with standard pattern');
    }

    // ==========================================================================
    // 기본 CRUD 연산 (TenantQueries 사용)
    // ==========================================================================

    async findAll() {
        try {
            const query = TenantQueries.findAll();
            const results = await this.executeQuery(query);
            return results;
        } catch (error) {
            console.error(`TenantRepository::findAll failed: ${error.message}`);
            throw error;
        }
    }

    async findById(id) {
        try {
            const query = TenantQueries.findById();
            const results = await this.executeQuery(query, [id]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`TenantRepository::findById failed: ${error.message}`);
            throw error;
        }
    }

    async findByDomain(domain) {
        try {
            const query = TenantQueries.findByDomain();
            const results = await this.executeQuery(query, [domain]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`TenantRepository::findByDomain failed: ${error.message}`);
            throw error;
        }
    }

    async findByCompanyCode(companyCode) {
        try {
            const query = TenantQueries.findByCompanyCode();
            const results = await this.executeQuery(query, [companyCode]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`TenantRepository::findByCompanyCode failed: ${error.message}`);
            throw error;
        }
    }

    async findActiveTenants() {
        try {
            const query = TenantQueries.findActiveTenants();
            const results = await this.executeQuery(query);
            return results;
        } catch (error) {
            console.error(`TenantRepository::findActiveTenants failed: ${error.message}`);
            throw error;
        }
    }

    async findBySubscriptionPlan(plan) {
        try {
            const query = TenantQueries.findBySubscriptionPlan();
            const results = await this.executeQuery(query, [plan]);
            return results;
        } catch (error) {
            console.error(`TenantRepository::findBySubscriptionPlan failed: ${error.message}`);
            throw error;
        }
    }

    async findExpiringTrials(days = 7) {
        try {
            const query = TenantQueries.findExpiringTrials();
            const results = await this.executeQuery(query, [days]);
            return results;
        } catch (error) {
            console.error(`TenantRepository::findExpiringTrials failed: ${error.message}`);
            throw error;
        }
    }

    async create(tenantData) {
        try {
            const query = TenantQueries.create();
            const params = [
                tenantData.company_name,
                tenantData.company_code,
                tenantData.domain || null,
                tenantData.contact_name || null,
                tenantData.contact_email || null,
                tenantData.contact_phone || null,
                tenantData.subscription_plan || 'starter',
                tenantData.subscription_status || 'active',
                tenantData.max_edge_servers || 3,
                tenantData.max_data_points || 1000,
                tenantData.max_users || 5,
                tenantData.is_active !== false ? 1 : 0,
                tenantData.trial_end_date || null
            ];
            
            const result = await this.executeNonQuery(query, params);
            return result.insertId || result.lastInsertRowid;
        } catch (error) {
            console.error(`TenantRepository::create failed: ${error.message}`);
            throw error;
        }
    }

    async update(id, tenantData) {
        try {
            const query = TenantQueries.update();
            const params = [
                tenantData.company_name,
                tenantData.company_code,
                tenantData.domain || null,
                tenantData.contact_name || null,
                tenantData.contact_email || null,
                tenantData.contact_phone || null,
                tenantData.subscription_plan || 'starter',
                tenantData.subscription_status || 'active',
                tenantData.max_edge_servers || 3,
                tenantData.max_data_points || 1000,
                tenantData.max_users || 5,
                tenantData.is_active !== false ? 1 : 0,
                tenantData.trial_end_date || null,
                id
            ];
            
            const result = await this.executeNonQuery(query, params);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`TenantRepository::update failed: ${error.message}`);
            throw error;
        }
    }

    async updateSubscription(id, subscriptionData) {
        try {
            const query = TenantQueries.updateSubscription();
            const params = [
                subscriptionData.subscription_plan,
                subscriptionData.subscription_status,
                subscriptionData.max_edge_servers || 3,
                subscriptionData.max_data_points || 1000,
                subscriptionData.max_users || 5,
                subscriptionData.trial_end_date || null,
                id
            ];
            
            const result = await this.executeNonQuery(query, params);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`TenantRepository::updateSubscription failed: ${error.message}`);
            throw error;
        }
    }

    async deleteById(id) {
        try {
            const query = TenantQueries.delete();
            const result = await this.executeNonQuery(query, [id]);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`TenantRepository::deleteById failed: ${error.message}`);
            throw error;
        }
    }

    async exists(id) {
        try {
            const query = TenantQueries.exists();
            const result = await this.executeQuerySingle(query, [id]);
            return result && result.count > 0;
        } catch (error) {
            console.error(`TenantRepository::exists failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 특화 메서드들 (TenantQueries 사용)
    // ==========================================================================

    async checkCompanyNameExists(companyName, excludeId = null) {
        try {
            let query = TenantQueries.checkCompanyNameExists();
            let params = [companyName];
            
            if (excludeId) {
                query += ' AND id != ?';
                params.push(excludeId);
            }
            
            const result = await this.executeQuerySingle(query, params);
            return result && result.count > 0;
        } catch (error) {
            console.error(`TenantRepository::checkCompanyNameExists failed: ${error.message}`);
            throw error;
        }
    }

    async checkCompanyCodeExists(companyCode, excludeId = null) {
        try {
            let query = TenantQueries.checkCompanyCodeExists();
            let params = [companyCode];
            
            if (excludeId) {
                query += ' AND id != ?';
                params.push(excludeId);
            }
            
            const result = await this.executeQuerySingle(query, params);
            return result && result.count > 0;
        } catch (error) {
            console.error(`TenantRepository::checkCompanyCodeExists failed: ${error.message}`);
            throw error;
        }
    }

    async checkDomainExists(domain, excludeId = null) {
        try {
            let query = TenantQueries.checkDomainExists();
            let params = [domain];
            
            if (excludeId) {
                query += ' AND id != ?';
                params.push(excludeId);
            }
            
            const result = await this.executeQuerySingle(query, params);
            return result && result.count > 0;
        } catch (error) {
            console.error(`TenantRepository::checkDomainExists failed: ${error.message}`);
            throw error;
        }
    }

    async getTenantStatistics(id) {
        try {
            const query = TenantQueries.getTenantStatistics();
            const results = await this.executeQuery(query, [id]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`TenantRepository::getTenantStatistics failed: ${error.message}`);
            throw error;
        }
    }

    async searchByCompanyName(searchTerm) {
        try {
            const query = TenantQueries.searchByCompanyName();
            const results = await this.executeQuery(query, [`%${searchTerm}%`]);
            return results;
        } catch (error) {
            console.error(`TenantRepository::searchByCompanyName failed: ${error.message}`);
            throw error;
        }
    }

    async getUsageLimits(id) {
        try {
            const query = TenantQueries.getUsageLimits();
            const results = await this.executeQuery(query, [id]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`TenantRepository::getUsageLimits failed: ${error.message}`);
            throw error;
        }
    }
}

module.exports = TenantRepository;