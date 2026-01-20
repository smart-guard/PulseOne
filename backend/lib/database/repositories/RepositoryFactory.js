// =============================================================================
// backend/lib/database/repositories/RepositoryFactory.js
// 🔧 기존 DatabaseFactory 완전 호환 버전 - initialize() 메서드 제거
// =============================================================================

const BaseRepository = require('./BaseRepository');
const SiteRepository = require('./SiteRepository');
const TenantRepository = require('./TenantRepository');
const DeviceRepository = require('./DeviceRepository');
const VirtualPointRepository = require('./VirtualPointRepository');
const AlarmOccurrenceRepository = require('./AlarmOccurrenceRepository');
const AlarmRuleRepository = require('./AlarmRuleRepository');
const UserRepository = require('./UserRepository');
const ProtocolRepository = require('./ProtocolRepository');
const AlarmTemplateRepository = require('./AlarmTemplateRepository');
const ManufacturerRepository = require('./ManufacturerRepository');
const DeviceModelRepository = require('./DeviceModelRepository');
const TemplateDeviceRepository = require('./TemplateDeviceRepository');
const TemplateDeviceSettingsRepository = require('./TemplateDeviceSettingsRepository');
const TemplateDataPointRepository = require('./TemplateDataPointRepository');
const AuditLogRepository = require('./AuditLogRepository');
const EdgeServerRepository = require('./EdgeServerRepository');
const DeviceGroupRepository = require('./DeviceGroupRepository');
const ExportProfileRepository = require('./ExportProfileRepository');
const ExportTargetRepository = require('./ExportTargetRepository');
const ExportGatewayRepository = require('./ExportGatewayRepository');
const PayloadTemplateRepository = require('./PayloadTemplateRepository');
const ExportTargetMappingRepository = require('./ExportTargetMappingRepository');
const ExportScheduleRepository = require('./ExportScheduleRepository');

// 기존 DatabaseFactory 사용
const DatabaseFactory = require('../DatabaseFactory');

/**
 * @class RepositoryFactory
 * @description Repository 인스턴스들을 생성하고 관리하는 팩토리 클래스 (싱글턴)
 * 
 * 기존 DatabaseFactory와 완전 호환:
 * - DatabaseFactory는 생성자에서 자동 초기화됨
 * - initialize() 메서드 불필요
 * - 기존 Repository 패턴 100% 준수
 */
class RepositoryFactory {
    constructor() {
        if (RepositoryFactory.instance) {
            return RepositoryFactory.instance;
        }

        this.repositories = new Map();
        this.dbManager = null;
        this.logger = null;
        this.cacheConfig = {
            enabled: false,
            ttl: 300,
            prefix: 'repo:'
        };
        this.initialized = false;

        RepositoryFactory.instance = this;
    }

    /**
     * 싱글턴 인스턴스 반환 (C++ 패턴과 동일)
     * @returns {RepositoryFactory} 팩토리 인스턴스
     */
    static getInstance() {
        if (!RepositoryFactory.instance) {
            RepositoryFactory.instance = new RepositoryFactory();
        }
        return RepositoryFactory.instance;
    }

    /**
     * 팩토리 초기화 (기존 DatabaseFactory 사용)
     * @param {Object} config 초기화 설정 (선택사항)
     */
    async initialize(config = {}) {
        if (this.initialized) {
            console.log('🏭 RepositoryFactory already initialized');
            return true;
        }

        try {
            console.log('🔧 RepositoryFactory initializing...');

            // 기존 DatabaseFactory 사용 (생성자에서 자동 초기화됨)
            this.dbManager = DatabaseFactory.getInstance(config.database);

            // 로거 설정 (간단한 콘솔 로거)
            this.logger = {
                info: console.log,
                warn: console.warn,
                error: console.error,
                debug: console.debug
            };

            // 캐시 설정
            if (config.cache) {
                this.cacheConfig = { ...this.cacheConfig, ...config.cache };
            }

            this.initialized = true;

            this.logger.info('🏭 RepositoryFactory initialized');
            this.logger.info(`Cache: ${this.cacheConfig.enabled ? 'ENABLED' : 'DISABLED'}`);
            this.logger.info('📦 Available Repositories: Site, Tenant, Device, VirtualPoint, AlarmOccurrence, AlarmRule, User, Protocol, Manufacturer, DeviceModel, TemplateDevice, AuditLog, EdgeServer, DeviceGroup');

            return true;
        } catch (error) {
            console.error('❌ RepositoryFactory initialization failed:', error.message);
            this.initialized = false;
            return false;
        }
    }

    /**
     * 팩토리 종료 및 연결 정리
     */
    async shutdown() {
        if (!this.initialized) return;

        try {
            if (this.dbManager) {
                await this.dbManager.closeAllConnections();
            }
            this.initialized = false;
            console.log('🏭 RepositoryFactory shutdown complete');
        } catch (error) {
            console.error('⚠️ RepositoryFactory shutdown error:', error.message);
        }
    }

    /**
     * 초기화 상태 확인
     * @returns {boolean} 초기화 여부
     */
    isInitialized() {
        return this.initialized;
    }

    // =========================================================================
    // Repository 생성 메서드들 (C++ 패턴과 동일한 네이밍)
    // =========================================================================

    /**
     * SiteRepository 반환
     * @returns {SiteRepository} Site Repository 인스턴스
     */
    getSiteRepository() {
        return this.getRepository('SiteRepository');
    }

    /**
     * TenantRepository 반환
     * @returns {TenantRepository} Tenant Repository 인스턴스
     */
    getTenantRepository() {
        return this.getRepository('TenantRepository');
    }

    /**
     * DeviceRepository 반환
     * @returns {DeviceRepository} Device Repository 인스턴스
     */
    getDeviceRepository() {
        return this.getRepository('DeviceRepository');
    }

    /**
     * VirtualPointRepository 반환
     * @returns {VirtualPointRepository} VirtualPoint Repository 인스턴스
     */
    getVirtualPointRepository() {
        return this.getRepository('VirtualPointRepository');
    }

    /**
     * AlarmRuleRepository 반환
     * @returns {AlarmRuleRepository} AlarmRule Repository 인스턴스
     */
    getAlarmRuleRepository() {
        return this.getRepository('AlarmRuleRepository');
    }

    /**
     * AlarmOccurrenceRepository 반환
     * @returns {AlarmOccurrenceRepository} AlarmOccurrence Repository 인스턴스
     */
    getAlarmOccurrenceRepository() {
        return this.getRepository('AlarmOccurrenceRepository');
    }

    /**
     * UserRepository 반환
     * @returns {UserRepository} User Repository 인스턴스
     */
    getUserRepository() {
        return this.getRepository('UserRepository');
    }

    /**
     * ProtocolRepository 반환
     * @returns {ProtocolRepository} Protocol Repository 인스턴스
     */
    getProtocolRepository() {
        return this.getRepository('ProtocolRepository');
    }

    /**
     * AlarmTemplateRepository 반환
     * @returns {AlarmTemplateRepository} AlarmTemplate Repository 인스턴스
     */
    getAlarmTemplateRepository() {
        return this.getRepository('AlarmTemplateRepository');
    }

    /**
     * ManufacturerRepository 반환
     */
    getManufacturerRepository() {
        return this.getRepository('ManufacturerRepository');
    }

    /**
     * DeviceModelRepository 반환
     */
    getDeviceModelRepository() {
        return this.getRepository('DeviceModelRepository');
    }

    /**
     * TemplateDeviceRepository 반환
     */
    getTemplateDeviceRepository() {
        return this.getRepository('TemplateDeviceRepository');
    }

    /**
     * TemplateDeviceSettingsRepository 반환
     */
    getTemplateDeviceSettingsRepository() {
        return this.getRepository('TemplateDeviceSettingsRepository');
    }

    /**
     * TemplateDataPointRepository 반환
     */
    getTemplateDataPointRepository() {
        return this.getRepository('TemplateDataPointRepository');
    }

    /**
     * AuditLogRepository 반환
     */
    getAuditLogRepository() {
        return this.getRepository('AuditLogRepository');
    }

    /**
     * EdgeServerRepository 반환
     */
    getEdgeServerRepository() {
        return this.getRepository('EdgeServerRepository');
    }

    /**
     * DeviceGroupRepository 반환
     */
    getDeviceGroupRepository() {
        return this.getRepository('DeviceGroupRepository');
    }

    /**
     * ExportProfileRepository 반환
     */
    getExportProfileRepository() {
        return this.getRepository('ExportProfileRepository');
    }

    /**
     * ExportTargetRepository 반환
     */
    getExportTargetRepository() {
        return this.getRepository('ExportTargetRepository');
    }

    /**
     * ExportGatewayRepository 반환
     */
    getExportGatewayRepository() {
        return this.getRepository('ExportGatewayRepository');
    }

    /**
     * PayloadTemplateRepository 반환
     */
    getPayloadTemplateRepository() {
        return this.getRepository('PayloadTemplateRepository');
    }

    /**
     * ExportTargetMappingRepository 반환
     */
    getExportTargetMappingRepository() {
        return this.getRepository('ExportTargetMappingRepository');
    }

    /**
     * ExportScheduleRepository 반환
     */
    getExportScheduleRepository() {
        return this.getRepository('ExportScheduleRepository');
    }

    // =========================================================================
    // 내부 구현 메서드들
    // =========================================================================

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

        try {
            switch (repositoryType) {
                case 'SiteRepository':
                    repository = new SiteRepository();
                    break;

                case 'TenantRepository':
                    repository = new TenantRepository();
                    break;

                case 'DeviceRepository':
                    repository = new DeviceRepository();
                    break;

                case 'VirtualPointRepository':
                    repository = new VirtualPointRepository();
                    break;

                case 'AlarmOccurrenceRepository':
                    repository = new AlarmOccurrenceRepository();
                    break;

                case 'AlarmRuleRepository':
                    repository = new AlarmRuleRepository();
                    break;

                case 'UserRepository':
                    repository = new UserRepository();
                    break;

                case 'ProtocolRepository':
                    repository = new ProtocolRepository();
                    break;

                case 'AlarmTemplateRepository':
                    repository = new AlarmTemplateRepository();
                    break;

                case 'ManufacturerRepository':
                    repository = new ManufacturerRepository();
                    break;

                case 'DeviceModelRepository':
                    repository = new DeviceModelRepository();
                    break;

                case 'TemplateDeviceRepository':
                    repository = new TemplateDeviceRepository();
                    break;

                case 'TemplateDeviceSettingsRepository':
                    repository = new TemplateDeviceSettingsRepository();
                    break;

                case 'TemplateDataPointRepository':
                    repository = new TemplateDataPointRepository();
                    break;

                case 'AuditLogRepository':
                    repository = new AuditLogRepository();
                    break;

                case 'EdgeServerRepository':
                    repository = new EdgeServerRepository();
                    break;

                case 'DeviceGroupRepository':
                    repository = new DeviceGroupRepository();
                    break;

                case 'ExportProfileRepository':
                    repository = new ExportProfileRepository();
                    break;

                case 'ExportTargetRepository':
                    repository = new ExportTargetRepository();
                    break;

                case 'ExportGatewayRepository':
                    repository = new ExportGatewayRepository();
                    break;

                case 'PayloadTemplateRepository':
                    repository = new PayloadTemplateRepository();
                    break;

                case 'ExportTargetMappingRepository':
                    repository = new ExportTargetMappingRepository();
                    break;

                case 'ExportScheduleRepository':
                    repository = new ExportScheduleRepository();
                    break;

                default:
                    throw new Error(`Unknown repository type: ${repositoryType}`);
            }

            // 캐시에 저장
            this.repositories.set(repositoryType, repository);

            this.logger?.info(`✅ ${repositoryType} created and cached`);
            return repository;

        } catch (error) {
            this.logger?.error(`❌ Failed to create ${repositoryType}: ${error.message}`);
            throw error;
        }
    }

    /**
     * 모든 Repository 캐시 클리어
     * @returns {Promise<boolean>} 성공 여부
     */
    async clearAllCaches() {
        try {
            const clearPromises = [];

            for (const [type, repo] of this.repositories) {
                if (repo.clearCache && typeof repo.clearCache === 'function') {
                    clearPromises.push(repo.clearCache());
                }
            }

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
            if (repo.getStats && typeof repo.getStats === 'function') {
                stats[type] = repo.getStats();
            } else if (repo.getCacheStats && typeof repo.getCacheStats === 'function') {
                stats[type] = repo.getCacheStats();
            } else {
                stats[type] = {
                    status: 'unknown',
                    message: 'Statistics not available'
                };
            }
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
                    'UserRepository',
                    'ProtocolRepository'
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
                if (repo.healthCheck && typeof repo.healthCheck === 'function') {
                    results[type] = await repo.healthCheck();
                } else {
                    // 기본 헬스체크 (Repository 존재 여부만 확인)
                    results[type] = {
                        status: 'healthy',
                        message: 'Repository instance exists',
                        tableName: repo.tableName || 'unknown',
                        timestamp: new Date().toISOString()
                    };
                }
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
     * 팩토리 정리 및 연결 해제
     * @returns {Promise<void>}
     */
    async cleanup() {
        try {
            // 모든 Repository 정리
            for (const [type, repo] of this.repositories) {
                if (repo.cleanup && typeof repo.cleanup === 'function') {
                    await repo.cleanup();
                }
            }

            this.repositories.clear();

            // 데이터베이스 연결 해제
            if (this.dbManager && this.dbManager.closeAllConnections) {
                await this.dbManager.closeAllConnections();
            }

            this.initialized = false;
            this.logger?.info('🧹 RepositoryFactory cleanup completed');
        } catch (error) {
            this.logger?.error(`❌ RepositoryFactory cleanup failed: ${error.message}`);
        }
    }

    /**
     * DatabaseFactory 인스턴스 반환 (고급 사용자용)
     * @returns {DatabaseFactory} DatabaseFactory 인스턴스
     */
    getDatabaseFactory() {
        return this.dbManager;
    }

    /**
     * 연결 상태 확인
     * @returns {Object} 연결 상태 정보
     */
    getConnectionStatus() {
        if (this.dbManager && this.dbManager.getConnectionStatus) {
            return this.dbManager.getConnectionStatus();
        }

        return {
            status: this.initialized ? 'initialized' : 'not_initialized',
            timestamp: new Date().toISOString()
        };
    }
}

// 싱글턴 인스턴스 초기화
RepositoryFactory.instance = null;

module.exports = RepositoryFactory;