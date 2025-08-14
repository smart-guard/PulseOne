// =============================================================================
// backend/lib/database/repositories/TenantRepository.js
// Tenant Repository - C++ TenantRepository 패턴 그대로 적용
// =============================================================================

const BaseRepository = require('./BaseRepository');
const TenantQueries = require('../queries/TenantQueries');

/**
 * @class TenantRepository
 * @extends BaseRepository
 * @description C++ TenantRepository 패턴을 Node.js로 포팅
 * 
 * 주요 기능:
 * - 테넌트 CRUD 연산
 * - 구독 상태 관리
 * - 도메인/이름 중복 확인
 * - 사용량 제한 관리
 * - 만료 테넌트 조회
 */
class TenantRepository extends BaseRepository {
    constructor(dbManager, logger, cacheConfig = null) {
        super('tenants', dbManager, logger, cacheConfig);
        this.entityName = 'Tenant';
        
        if (this.logger) {
            this.logger.info('🏢 TenantRepository initialized with BaseEntity pattern');
            this.logger.info(`✅ Cache enabled: ${this.isCacheEnabled() ? 'YES' : 'NO'}`);
        }
    }

    // ==========================================================================
    // 기본 CRUD 연산 (BaseRepository 상속)
    // ==========================================================================

    /**
     * 모든 테넌트 조회
     * @returns {Promise<Array>} 테넌트 목록
     */
    async findAll() {
        try {
            const query = TenantQueries.findAll();
            const results = await this.executeQuery(query);
            this.logger?.debug(`TenantRepository::findAll - Found ${results.length} tenants`);
            return results;
        } catch (error) {
            this.logger?.error(`TenantRepository::findAll failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * ID로 테넌트 조회
     * @param {number} id 테넌트 ID
     * @returns {Promise<Object|null>} 테넌트 객체 또는 null
     */
    async findById(id) {
        try {
            const cacheKey = `tenant:${id}`;
            
            // 캐시 확인
            if (this.isCacheEnabled()) {
                const cached = await this.getFromCache(cacheKey);
                if (cached) {
                    this.logger?.debug(`TenantRepository::findById - Cache hit for ID: ${id}`);
                    return cached;
                }
            }

            const query = TenantQueries.findById();
            const results = await this.executeQuery(query, [id]);
            
            const tenant = results.length > 0 ? results[0] : null;
            
            // 캐시에 저장
            if (tenant && this.isCacheEnabled()) {
                await this.setCache(cacheKey, tenant);
            }
            
            this.logger?.debug(`TenantRepository::findById - ${tenant ? 'Found' : 'Not found'} tenant with ID: ${id}`);
            return tenant;
        } catch (error) {
            this.logger?.error(`TenantRepository::findById failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 도메인/이름 기반 검색
    // ==========================================================================

    /**
     * 도메인으로 테넌트 조회
     * @param {string} domain 도메인
     * @returns {Promise<Object|null>} 테넌트 객체 또는 null
     */
    async findByDomain(domain) {
        try {
            const cacheKey = `tenant:domain:${domain}`;
            
            // 캐시 확인
            if (this.isCacheEnabled()) {
                const cached = await this.getFromCache(cacheKey);
                if (cached) {
                    this.logger?.debug(`TenantRepository::findByDomain - Cache hit for domain: ${domain}`);
                    return cached;
                }
            }

            const query = TenantQueries.findByDomain();
            const results = await this.executeQuery(query, [domain]);
            
            const tenant = results.length > 0 ? results[0] : null;
            
            // 캐시에 저장
            if (tenant && this.isCacheEnabled()) {
                await this.setCache(cacheKey, tenant);
            }
            
            this.logger?.debug(`TenantRepository::findByDomain - ${tenant ? 'Found' : 'Not found'} tenant with domain: ${domain}`);
            return tenant;
        } catch (error) {
            this.logger?.error(`TenantRepository::findByDomain failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 회사명으로 테넌트 조회
     * @param {string} companyName 회사명
     * @returns {Promise<Object|null>} 테넌트 객체 또는 null
     */
    async findByCompanyName(companyName) {
        try {
            const query = TenantQueries.findByCompanyName();
            const results = await this.executeQuery(query, [companyName]);
            
            const tenant = results.length > 0 ? results[0] : null;
            this.logger?.debug(`TenantRepository::findByCompanyName - ${tenant ? 'Found' : 'Not found'} tenant: ${companyName}`);
            return tenant;
        } catch (error) {
            this.logger?.error(`TenantRepository::findByCompanyName failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 회사 코드로 테넌트 조회
     * @param {string} companyCode 회사 코드
     * @returns {Promise<Object|null>} 테넌트 객체 또는 null
     */
    async findByCompanyCode(companyCode) {
        try {
            const query = TenantQueries.findByCompanyCode();
            const results = await this.executeQuery(query, [companyCode]);
            
            const tenant = results.length > 0 ? results[0] : null;
            this.logger?.debug(`TenantRepository::findByCompanyCode - ${tenant ? 'Found' : 'Not found'} tenant: ${companyCode}`);
            return tenant;
        } catch (error) {
            this.logger?.error(`TenantRepository::findByCompanyCode failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 상태별 조회
    // ==========================================================================

    /**
     * 구독 상태별 테넌트 조회
     * @param {string} status 구독 상태 (active, inactive, suspended, trial)
     * @returns {Promise<Array>} 테넌트 목록
     */
    async findBySubscriptionStatus(status) {
        try {
            const query = TenantQueries.findBySubscriptionStatus();
            const results = await this.executeQuery(query, [status]);
            this.logger?.debug(`TenantRepository::findBySubscriptionStatus - Found ${results.length} tenants with status: ${status}`);
            return results;
        } catch (error) {
            this.logger?.error(`TenantRepository::findBySubscriptionStatus failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 활성 테넌트 조회
     * @returns {Promise<Array>} 활성 테넌트 목록
     */
    async findActiveTenants() {
        try {
            const query = TenantQueries.findActiveTenants();
            const results = await this.executeQuery(query);
            this.logger?.debug(`TenantRepository::findActiveTenants - Found ${results.length} active tenants`);
            return results;
        } catch (error) {
            this.logger?.error(`TenantRepository::findActiveTenants failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 만료된 테넌트 조회
     * @returns {Promise<Array>} 만료된 테넌트 목록
     */
    async findExpiredTenants() {
        try {
            const query = TenantQueries.findExpiredTenants();
            const results = await this.executeQuery(query);
            this.logger?.debug(`TenantRepository::findExpiredTenants - Found ${results.length} expired tenants`);
            return results;
        } catch (error) {
            this.logger?.error(`TenantRepository::findExpiredTenants failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 트라이얼 테넌트 조회
     * @returns {Promise<Array>} 트라이얼 테넌트 목록
     */
    async findTrialTenants() {
        try {
            const query = TenantQueries.findTrialTenants();
            const results = await this.executeQuery(query);
            this.logger?.debug(`TenantRepository::findTrialTenants - Found ${results.length} trial tenants`);
            return results;
        } catch (error) {
            this.logger?.error(`TenantRepository::findTrialTenants failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // CRUD 연산 (생성, 수정, 삭제)
    // ==========================================================================

    /**
     * 새 테넌트 생성
     * @param {Object} tenantData 테넌트 데이터
     * @returns {Promise<number>} 생성된 테넌트 ID
     */
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
                tenantData.subscription_status || 'trial',
                tenantData.max_edge_servers || 3,
                tenantData.max_data_points || 1000,
                tenantData.max_users || 5,
                tenantData.is_active !== undefined ? tenantData.is_active : 1,
                tenantData.trial_end_date || null
            ];

            const result = await this.executeNonQuery(query, params);
            const tenantId = result.lastID;
            
            // 캐시 무효화
            if (this.isCacheEnabled()) {
                await this.invalidateCache(`tenant:${tenantId}`);
                await this.invalidateCache(`tenant:domain:${tenantData.domain}`);
            }
            
            this.logger?.info(`TenantRepository::create - Created tenant ${tenantData.company_name} with ID: ${tenantId}`);
            return tenantId;
        } catch (error) {
            this.logger?.error(`TenantRepository::create failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 테넌트 정보 수정
     * @param {number} id 테넌트 ID
     * @param {Object} updateData 수정할 데이터
     * @returns {Promise<boolean>} 성공 여부
     */
    async update(id, updateData) {
        try {
            const query = TenantQueries.update();
            const params = [
                updateData.company_name,
                updateData.company_code,
                updateData.domain,
                updateData.contact_name,
                updateData.contact_email,
                updateData.contact_phone,
                updateData.subscription_plan,
                updateData.subscription_status,
                updateData.max_edge_servers,
                updateData.max_data_points,
                updateData.max_users,
                updateData.is_active,
                updateData.trial_end_date,
                id
            ];

            const result = await this.executeNonQuery(query, params);
            const success = result.changes > 0;
            
            // 캐시 무효화
            if (success && this.isCacheEnabled()) {
                await this.invalidateCache(`tenant:${id}`);
                await this.invalidateCache(`tenant:domain:*`);
            }
            
            this.logger?.info(`TenantRepository::update - ${success ? 'Updated' : 'Failed to update'} tenant ID: ${id}`);
            return success;
        } catch (error) {
            this.logger?.error(`TenantRepository::update failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 테넌트 삭제
     * @param {number} id 테넌트 ID
     * @returns {Promise<boolean>} 성공 여부
     */
    async delete(id) {
        try {
            const query = TenantQueries.delete();
            const result = await this.executeNonQuery(query, [id]);
            const success = result.changes > 0;
            
            // 캐시 무효화
            if (success && this.isCacheEnabled()) {
                await this.invalidateCache(`tenant:${id}`);
                await this.invalidateCache(`tenant:domain:*`);
            }
            
            this.logger?.info(`TenantRepository::delete - ${success ? 'Deleted' : 'Failed to delete'} tenant ID: ${id}`);
            return success;
        } catch (error) {
            this.logger?.error(`TenantRepository::delete failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 구독 관리
    // ==========================================================================

    /**
     * 구독 상태 업데이트
     * @param {number} tenantId 테넌트 ID
     * @param {string} status 새 구독 상태
     * @returns {Promise<boolean>} 성공 여부
     */
    async updateSubscriptionStatus(tenantId, status) {
        try {
            const query = TenantQueries.updateSubscriptionStatus();
            const result = await this.executeNonQuery(query, [status, tenantId]);
            const success = result.changes > 0;
            
            // 캐시 무효화
            if (success && this.isCacheEnabled()) {
                await this.invalidateCache(`tenant:${tenantId}`);
            }
            
            this.logger?.info(`TenantRepository::updateSubscriptionStatus - ${success ? 'Updated' : 'Failed to update'} status for tenant ${tenantId} to ${status}`);
            return success;
        } catch (error) {
            this.logger?.error(`TenantRepository::updateSubscriptionStatus failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 구독 연장
     * @param {number} tenantId 테넌트 ID
     * @param {number} additionalDays 연장할 일수
     * @returns {Promise<boolean>} 성공 여부
     */
    async extendSubscription(tenantId, additionalDays) {
        try {
            const query = TenantQueries.extendSubscription();
            const result = await this.executeNonQuery(query, [additionalDays, tenantId]);
            const success = result.changes > 0;
            
            // 캐시 무효화
            if (success && this.isCacheEnabled()) {
                await this.invalidateCache(`tenant:${tenantId}`);
            }
            
            this.logger?.info(`TenantRepository::extendSubscription - ${success ? 'Extended' : 'Failed to extend'} subscription for tenant ${tenantId} by ${additionalDays} days`);
            return success;
        } catch (error) {
            this.logger?.error(`TenantRepository::extendSubscription failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 사용량 제한 업데이트
     * @param {number} tenantId 테넌트 ID
     * @param {Object} limits 제한값들
     * @returns {Promise<boolean>} 성공 여부
     */
    async updateLimits(tenantId, limits) {
        try {
            const query = TenantQueries.updateLimits();
            const params = [
                limits.max_edge_servers,
                limits.max_data_points,
                limits.max_users,
                tenantId
            ];

            const result = await this.executeNonQuery(query, params);
            const success = result.changes > 0;
            
            // 캐시 무효화
            if (success && this.isCacheEnabled()) {
                await this.invalidateCache(`tenant:${tenantId}`);
            }
            
            this.logger?.info(`TenantRepository::updateLimits - ${success ? 'Updated' : 'Failed to update'} limits for tenant ${tenantId}`);
            return success;
        } catch (error) {
            this.logger?.error(`TenantRepository::updateLimits failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 유효성 검증
    // ==========================================================================

    /**
     * 도메인 중복 확인
     * @param {string} domain 도메인
     * @param {number} excludeId 제외할 테넌트 ID (수정 시 사용)
     * @returns {Promise<boolean>} 중복 여부
     */
    async isDomainTaken(domain, excludeId = null) {
        try {
            const query = excludeId 
                ? TenantQueries.isDomainTakenExcluding()
                : TenantQueries.isDomainTaken();
            
            const params = excludeId 
                ? [domain, excludeId]
                : [domain];
            
            const results = await this.executeQuery(query, params);
            const isTaken = results[0]?.count > 0;
            
            this.logger?.debug(`TenantRepository::isDomainTaken - Domain ${domain} ${isTaken ? 'taken' : 'available'}`);
            return isTaken;
        } catch (error) {
            this.logger?.error(`TenantRepository::isDomainTaken failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 회사명 중복 확인
     * @param {string} companyName 회사명
     * @param {number} excludeId 제외할 테넌트 ID
     * @returns {Promise<boolean>} 중복 여부
     */
    async isCompanyNameTaken(companyName, excludeId = null) {
        try {
            const query = excludeId 
                ? TenantQueries.isCompanyNameTakenExcluding()
                : TenantQueries.isCompanyNameTaken();
            
            const params = excludeId 
                ? [companyName, excludeId]
                : [companyName];
            
            const results = await this.executeQuery(query, params);
            const isTaken = results[0]?.count > 0;
            
            this.logger?.debug(`TenantRepository::isCompanyNameTaken - Company name ${companyName} ${isTaken ? 'taken' : 'available'}`);
            return isTaken;
        } catch (error) {
            this.logger?.error(`TenantRepository::isCompanyNameTaken failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 통계 및 집계
    // ==========================================================================

    /**
     * 구독 상태별 통계
     * @returns {Promise<Array>} 상태별 테넌트 수
     */
    async getSubscriptionStatusStats() {
        try {
            const query = TenantQueries.getSubscriptionStatusStats();
            const results = await this.executeQuery(query);
            this.logger?.debug(`TenantRepository::getSubscriptionStatusStats - Found stats for ${results.length} statuses`);
            return results;
        } catch (error) {
            this.logger?.error(`TenantRepository::getSubscriptionStatusStats failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 구독 플랜별 통계
     * @returns {Promise<Array>} 플랜별 테넌트 수
     */
    async getSubscriptionPlanStats() {
        try {
            const query = TenantQueries.getSubscriptionPlanStats();
            const results = await this.executeQuery(query);
            this.logger?.debug(`TenantRepository::getSubscriptionPlanStats - Found stats for ${results.length} plans`);
            return results;
        } catch (error) {
            this.logger?.error(`TenantRepository::getSubscriptionPlanStats failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 전체 테넌트 수 조회
     * @returns {Promise<number>} 테넌트 수
     */
    async getTotalCount() {
        try {
            const query = TenantQueries.getTotalCount();
            const results = await this.executeQuery(query);
            const count = results[0]?.count || 0;
            this.logger?.debug(`TenantRepository::getTotalCount - Total tenants: ${count}`);
            return count;
        } catch (error) {
            this.logger?.error(`TenantRepository::getTotalCount failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 유틸리티 메서드
    // ==========================================================================

    /**
     * 테넌트가 존재하는지 확인
     * @param {number} id 테넌트 ID
     * @returns {Promise<boolean>} 존재 여부
     */
    async exists(id) {
        try {
            const tenant = await this.findById(id);
            return tenant !== null;
        } catch (error) {
            this.logger?.error(`TenantRepository::exists failed: ${error.message}`);
            return false;
        }
    }

    /**
     * 테넌트 활성화/비활성화
     * @param {number} tenantId 테넌트 ID
     * @param {boolean} isActive 활성화 여부
     * @returns {Promise<boolean>} 성공 여부
     */
    async setActive(tenantId, isActive) {
        try {
            const query = TenantQueries.setActive();
            const result = await this.executeNonQuery(query, [isActive ? 1 : 0, tenantId]);
            const success = result.changes > 0;
            
            // 캐시 무효화
            if (success && this.isCacheEnabled()) {
                await this.invalidateCache(`tenant:${tenantId}`);
            }
            
            this.logger?.info(`TenantRepository::setActive - ${success ? 'Updated' : 'Failed to update'} active status for tenant ${tenantId} to ${isActive}`);
            return success;
        } catch (error) {
            this.logger?.error(`TenantRepository::setActive failed: ${error.message}`);
            throw error;
        }
    }
}

module.exports = TenantRepository;