// =============================================================================
// backend/lib/database/repositories/RepositoryFactory.js
// Repository 팩토리 - C++ RepositoryFactory 패턴 완전 적용
// =============================================================================

const BaseRepository = require('./BaseRepository');
const SiteRepository = require('./SiteRepository');
const TenantRepository = require('./TenantRepository');
const DeviceRepository = require('./DeviceRepository');
const VirtualPointRepository = require('./VirtualPointRepository');
const AlarmOccurrenceRepository = require('./AlarmOccurrenceRepository');
const AlarmRuleRepository = require('./AlarmRuleRepository');
const UserRepository = require('./UserRepository');
// TODO: 아직 구현 안됨
// const DataPointRepository = require('./DataPointRepository');

/**
 * @class RepositoryFactory
 * @description Repository 인스턴스들을 생성하고 관리하는 팩토리 클래스
 * 
 * C++ RepositoryFactory 패턴을 Node.js로 완전 포팅:
 * - 싱글턴 패턴으로 Repository 인스턴스 관리
 * - 의존성 주입 지원 (dbManager, logger, cacheConfig)
 * - 캐시 설정 공유
 * - 모든 구현된 Repository 지원
 */
class RepositoryFactory {
    constructor() {
        this.repositories = new Map();
        this.dbManager = null;
        this.logger = null;
        this.cacheConfig = {
            enabled: false,
            ttl: 300,
            prefix: 'repo:'
        };
        this.initialized = false;
    }

    /**
     * 팩토리 초기화
     * @param {Object} dbManager 데이터베이스 매니저
     * @param {Object} logger 로거 인스턴스
     * @param {Object} cacheConfig 캐시 설정
     */
    initialize(dbManager, logger = null, cacheConfig = {}) {
        this.dbManager = dbManager;
        this.logger = logger;
        this.cacheConfig = { ...this.cacheConfig, ...cacheConfig };
        this.initialized = true;
        
        this.logger?.info('🏭 RepositoryFactory initialized');
        this.logger?.info(`Cache: ${this.cacheConfig.enabled ? 'ENABLED' : 'DISABLED'}`);
        this.logger?.info(`📦 Available Repositories: Site, Tenant, Device, VirtualPoint, AlarmOccurrence, AlarmRule, User`);
    }

    /**
     * Repository 인스턴스 생성 또는 반환
     * @param {string} repositoryType Repository 타입
     * @returns {BaseRepository} Repository 인스턴스
     */
    getRepository(repositoryType) {
        if (!this.initialized) {
            throw new Error('RepositoryFactory must be initialized before use');
        }

        // 이미 생성된 인스턴스가 있으면 반환
        if (this.repositories.has(repositoryType)) {
            return this.repositories.get(repositoryType);
        }

        // 새 인스턴스 생성
        let repository;
        
        switch (repositoryType.toLowerCase()) {
            case 'site':
            case 'sites':
                repository = new SiteRepository(
                    this.dbManager, 
                    this.logger, 
                    this.cacheConfig
                );
                break;
                
            case 'tenant':
            case 'tenants':
                repository = new TenantRepository(
                    this.dbManager, 
                    this.logger, 
                    this.cacheConfig
                );
                break;
                
            case 'device':
            case 'devices':
                repository = new DeviceRepository(
                    this.dbManager, 
                    this.logger, 
                    this.cacheConfig
                );
                break;
                
            case 'virtualpoint':
            case 'virtual_point':
            case 'virtualpoints':
            case 'virtual_points':
                repository = new VirtualPointRepository(
                    this.dbManager, 
                    this.logger, 
                    this.cacheConfig
                );
                break;
                
            case 'alarmoccurrence':
            case 'alarm_occurrence':
            case 'alarmoccurrences':
            case 'alarm_occurrences':
                repository = new AlarmOccurrenceRepository(
                    this.dbManager, 
                    this.logger, 
                    this.cacheConfig
                );
                break;
                
            case 'alarmrule':
            case 'alarm_rule':
            case 'alarmrules':
            case 'alarm_rules':
                repository = new AlarmRuleRepository(
                    this.dbManager, 
                    this.logger, 
                    this.cacheConfig
                );
                break;
                
            case 'user':
            case 'users':
                repository = new UserRepository(
                    this.dbManager, 
                    this.logger, 
                    this.cacheConfig
                );
                break;

            // TODO: DataPointRepository 구현 후 추가
            // case 'datapoint':
            // case 'data_point':
            // case 'datapoints':
            // case 'data_points':
            //     repository = new DataPointRepository(
            //         this.dbManager, 
            //         this.logger, 
            //         this.cacheConfig
            //     );
            //     break;
                
            default:
                throw new Error(`Unknown repository type: ${repositoryType}`);
        }

        // 캐시에 저장
        this.repositories.set(repositoryType, repository);
        
        this.logger?.debug(`✅ Created ${repositoryType}Repository instance`);
        return repository;
    }

    // ==========================================================================
    // 🎯 구체적 Repository 인스턴스 반환 메서드들 (모두 활성화!)
    // ==========================================================================

    /**
     * SiteRepository 인스턴스 반환
     * @returns {SiteRepository}
     */
    getSiteRepository() {
        return this.getRepository('site');
    }

    /**
     * TenantRepository 인스턴스 반환
     * @returns {TenantRepository}
     */
    getTenantRepository() {
        return this.getRepository('tenant');
    }

    /**
     * DeviceRepository 인스턴스 반환
     * @returns {DeviceRepository}
     */
    getDeviceRepository() {
        return this.getRepository('device');
    }

    /**
     * VirtualPointRepository 인스턴스 반환
     * @returns {VirtualPointRepository}
     */
    getVirtualPointRepository() {
        return this.getRepository('virtualpoint');
    }

    /**
     * AlarmOccurrenceRepository 인스턴스 반환
     * @returns {AlarmOccurrenceRepository}
     */
    getAlarmOccurrenceRepository() {
        return this.getRepository('alarmoccurrence');
    }

    /**
     * AlarmRuleRepository 인스턴스 반환
     * @returns {AlarmRuleRepository}
     */
    getAlarmRuleRepository() {
        return this.getRepository('alarmrule');
    }

    /**
     * UserRepository 인스턴스 반환
     * @returns {UserRepository}
     */
    getUserRepository() {
        return this.getRepository('user');
    }

    // TODO: DataPointRepository 구현 후 활성화
    // /**
    //  * DataPointRepository 인스턴스 반환
    //  * @returns {DataPointRepository}
    //  */
    // getDataPointRepository() {
    //     return this.getRepository('datapoint');
    // }

    // ==========================================================================
    // 🛠️ 관리 및 유틸리티 메서드들
    // ==========================================================================

    /**
     * 모든 Repository의 캐시 초기화
     * @returns {Promise<boolean>}
     */
    async clearAllCaches() {
        try {
            const clearPromises = Array.from(this.repositories.values())
                .map(repo => repo.clearCache());
            
            await Promise.all(clearPromises);
            
            this.logger?.info('🧹 All repository caches cleared');
            return true;
        } catch (error) {
            this.logger?.error(`❌ Failed to clear repository caches: ${error.message}`);
            return false;
        }
    }

    /**
     * 모든 Repository의 통계 정보 수집
     * @returns {Object} 통계 정보
     */
    getAllStats() {
        const stats = {};
        
        for (const [type, repo] of this.repositories) {
            stats[type] = repo.getStats();
        }
        
        return {
            factory: {
                initialized: this.initialized,
                repositoryCount: this.repositories.size,
                cacheEnabled: this.cacheConfig.enabled,
                availableRepositories: [
                    'SiteRepository',
                    'TenantRepository', 
                    'DeviceRepository',
                    'VirtualPointRepository',
                    'AlarmOccurrenceRepository',
                    'AlarmRuleRepository',
                    'UserRepository'
                    // 'DataPointRepository' // TODO: 구현 후 추가
                ]
            },
            repositories: stats
        };
    }

    /**
     * 모든 Repository의 헬스 체크
     * @returns {Promise<Object>} 헬스 체크 결과
     */
    async healthCheckAll() {
        const results = {};
        
        for (const [type, repo] of this.repositories) {
            try {
                results[type] = await repo.healthCheck();
            } catch (error) {
                results[type] = {
                    status: 'error',
                    error: error.message,
                    timestamp: new Date().toISOString()
                };
            }
        }
        
        return {
            factory: {
                status: 'healthy',
                initialized: this.initialized,
                repositoryCount: this.repositories.size,
                timestamp: new Date().toISOString()
            },
            repositories: results
        };
    }

    /**
     * Repository 인스턴스 제거 (메모리 정리)
     * @param {string} repositoryType Repository 타입
     * @returns {boolean} 성공 여부
     */
    removeRepository(repositoryType) {
        const removed = this.repositories.delete(repositoryType);
        
        if (removed) {
            this.logger?.debug(`🗑️ Removed ${repositoryType}Repository instance`);
        }
        
        return removed;
    }

    /**
     * 특정 타입의 Repository들 일괄 제거
     * @param {Array<string>} repositoryTypes Repository 타입들
     * @returns {number} 제거된 Repository 수
     */
    removeRepositories(repositoryTypes) {
        let removedCount = 0;
        
        for (const type of repositoryTypes) {
            if (this.removeRepository(type)) {
                removedCount++;
            }
        }
        
        this.logger?.info(`🗑️ Removed ${removedCount} repository instances`);
        return removedCount;
    }

    /**
     * 팩토리 정리 (모든 Repository 인스턴스 제거)
     */
    cleanup() {
        const repositoryCount = this.repositories.size;
        this.repositories.clear();
        this.initialized = false;
        
        this.logger?.info(`🧹 RepositoryFactory cleanup completed (removed ${repositoryCount} repositories)`);
    }

    /**
     * 팩토리 재시작 (모든 Repository 인스턴스 재생성)
     * @returns {boolean} 성공 여부
     */
    restart() {
        try {
            this.cleanup();
            // 기존 설정으로 재초기화
            if (this.dbManager) {
                this.initialize(this.dbManager, this.logger, this.cacheConfig);
                this.logger?.info('🔄 RepositoryFactory restarted successfully');
                return true;
            } else {
                this.logger?.error('❌ Cannot restart: missing dbManager');
                return false;
            }
        } catch (error) {
            this.logger?.error(`❌ RepositoryFactory restart failed: ${error.message}`);
            return false;
        }
    }

    /**
     * 캐시 설정 업데이트 (모든 Repository에 적용)
     * @param {Object} newCacheConfig 새 캐시 설정
     * @returns {boolean} 성공 여부
     */
    updateCacheConfig(newCacheConfig) {
        try {
            this.cacheConfig = { ...this.cacheConfig, ...newCacheConfig };
            
            // 기존 Repository들에 새 설정 적용
            for (const [type, repo] of this.repositories) {
                repo.cacheConfig = this.cacheConfig;
                this.logger?.debug(`📝 Updated cache config for ${type}Repository`);
            }
            
            this.logger?.info('⚙️ Cache configuration updated for all repositories');
            return true;
        } catch (error) {
            this.logger?.error(`❌ Failed to update cache config: ${error.message}`);
            return false;
        }
    }
}

// =============================================================================
// 🎯 싱글턴 인스턴스 관리
// =============================================================================

let factoryInstance = null;

/**
 * RepositoryFactory 싱글턴 인스턴스 반환
 * @returns {RepositoryFactory}
 */
function getRepositoryFactory() {
    if (!factoryInstance) {
        factoryInstance = new RepositoryFactory();
    }
    return factoryInstance;
}

/**
 * 새로운 RepositoryFactory 인스턴스 생성 (테스트용)
 * @returns {RepositoryFactory}
 */
function createRepositoryFactory() {
    return new RepositoryFactory();
}

// =============================================================================
// 🎯 Export
// =============================================================================

module.exports = {
    RepositoryFactory,
    getRepositoryFactory,
    createRepositoryFactory,
    
    // Repository 클래스들 직접 export (개별 사용 가능)
    BaseRepository,
    SiteRepository,
    TenantRepository,
    DeviceRepository,
    VirtualPointRepository,
    AlarmOccurrenceRepository,
    AlarmRuleRepository,
    UserRepository
    // DataPointRepository  // TODO: 구현 후 추가
};