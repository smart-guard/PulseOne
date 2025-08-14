// =============================================================================
// backend/lib/database/repositories/UserRepository.js
// User Repository - C++ UserRepository 패턴 그대로 적용
// =============================================================================

const BaseRepository = require('./BaseRepository');
const UserQueries = require('../queries/UserQueries');

/**
 * @class UserRepository
 * @extends BaseRepository
 * @description C++ UserRepository 패턴을 Node.js로 포팅
 * 
 * 주요 기능:
 * - 사용자 CRUD 연산
 * - 역할별 조회
 * - 테넌트별 조회
 * - 인증 관련 기능
 * - 권한 관리
 */
class UserRepository extends BaseRepository {
    constructor(dbManager, logger, cacheConfig = null) {
        super('users', dbManager, logger, cacheConfig);
        this.entityName = 'User';
        
        if (this.logger) {
            this.logger.info('👤 UserRepository initialized with BaseEntity pattern');
            this.logger.info(`✅ Cache enabled: ${this.isCacheEnabled() ? 'YES' : 'NO'}`);
        }
    }

    // ==========================================================================
    // 기본 CRUD 연산 (BaseRepository 상속)
    // ==========================================================================

    /**
     * 모든 사용자 조회
     * @returns {Promise<Array>} 사용자 목록
     */
    async findAll() {
        try {
            const query = UserQueries.findAll();
            const results = await this.executeQuery(query);
            this.logger?.debug(`UserRepository::findAll - Found ${results.length} users`);
            return results;
        } catch (error) {
            this.logger?.error(`UserRepository::findAll failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * ID로 사용자 조회
     * @param {number} id 사용자 ID
     * @returns {Promise<Object|null>} 사용자 객체 또는 null
     */
    async findById(id) {
        try {
            const cacheKey = `user:${id}`;
            
            // 캐시 확인
            if (this.isCacheEnabled()) {
                const cached = await this.getFromCache(cacheKey);
                if (cached) {
                    this.logger?.debug(`UserRepository::findById - Cache hit for ID: ${id}`);
                    return cached;
                }
            }

            const query = UserQueries.findById();
            const results = await this.executeQuery(query, [id]);
            
            const user = results.length > 0 ? results[0] : null;
            
            // 캐시에 저장
            if (user && this.isCacheEnabled()) {
                await this.setCache(cacheKey, user);
            }
            
            this.logger?.debug(`UserRepository::findById - ${user ? 'Found' : 'Not found'} user with ID: ${id}`);
            return user;
        } catch (error) {
            this.logger?.error(`UserRepository::findById failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 인증 관련 조회
    // ==========================================================================

    /**
     * 사용자명으로 조회
     * @param {string} username 사용자명
     * @returns {Promise<Object|null>} 사용자 객체 또는 null
     */
    async findByUsername(username) {
        try {
            const cacheKey = `user:username:${username}`;
            
            // 캐시 확인
            if (this.isCacheEnabled()) {
                const cached = await this.getFromCache(cacheKey);
                if (cached) {
                    this.logger?.debug(`UserRepository::findByUsername - Cache hit for username: ${username}`);
                    return cached;
                }
            }

            const query = UserQueries.findByUsername();
            const results = await this.executeQuery(query, [username]);
            
            const user = results.length > 0 ? results[0] : null;
            
            // 캐시에 저장
            if (user && this.isCacheEnabled()) {
                await this.setCache(cacheKey, user);
            }
            
            this.logger?.debug(`UserRepository::findByUsername - ${user ? 'Found' : 'Not found'} user: ${username}`);
            return user;
        } catch (error) {
            this.logger?.error(`UserRepository::findByUsername failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 이메일로 조회
     * @param {string} email 이메일
     * @returns {Promise<Object|null>} 사용자 객체 또는 null
     */
    async findByEmail(email) {
        try {
            const query = UserQueries.findByEmail();
            const results = await this.executeQuery(query, [email]);
            
            const user = results.length > 0 ? results[0] : null;
            this.logger?.debug(`UserRepository::findByEmail - ${user ? 'Found' : 'Not found'} user: ${email}`);
            return user;
        } catch (error) {
            this.logger?.error(`UserRepository::findByEmail failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 테넌트/역할별 조회
    // ==========================================================================

    /**
     * 테넌트별 사용자 조회
     * @param {number} tenantId 테넌트 ID
     * @returns {Promise<Array>} 사용자 목록
     */
    async findByTenant(tenantId) {
        try {
            const query = UserQueries.findByTenant();
            const results = await this.executeQuery(query, [tenantId]);
            this.logger?.debug(`UserRepository::findByTenant - Found ${results.length} users for tenant ${tenantId}`);
            return results;
        } catch (error) {
            this.logger?.error(`UserRepository::findByTenant failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 역할별 사용자 조회
     * @param {string} role 역할 (system_admin, company_admin, site_admin, engineer, operator, viewer)
     * @returns {Promise<Array>} 사용자 목록
     */
    async findByRole(role) {
        try {
            const query = UserQueries.findByRole();
            const results = await this.executeQuery(query, [role]);
            this.logger?.debug(`UserRepository::findByRole - Found ${results.length} users with role: ${role}`);
            return results;
        } catch (error) {
            this.logger?.error(`UserRepository::findByRole failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 활성 사용자 조회
     * @param {number} tenantId 테넌트 ID (선택사항)
     * @returns {Promise<Array>} 활성 사용자 목록
     */
    async findActiveUsers(tenantId = null) {
        try {
            const query = tenantId 
                ? UserQueries.findActiveUsersByTenant()
                : UserQueries.findActiveUsers();
            
            const params = tenantId ? [tenantId] : [];
            const results = await this.executeQuery(query, params);
            
            this.logger?.debug(`UserRepository::findActiveUsers - Found ${results.length} active users`);
            return results;
        } catch (error) {
            this.logger?.error(`UserRepository::findActiveUsers failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // CRUD 연산 (생성, 수정, 삭제)
    // ==========================================================================

    /**
     * 새 사용자 생성
     * @param {Object} userData 사용자 데이터
     * @returns {Promise<number>} 생성된 사용자 ID
     */
    async create(userData) {
        try {
            const query = UserQueries.create();
            const params = [
                userData.tenant_id || null,
                userData.username,
                userData.email,
                userData.password_hash,
                userData.full_name || null,
                userData.department || null,
                userData.role || 'viewer',
                userData.permissions ? JSON.stringify(userData.permissions) : null,
                userData.site_access ? JSON.stringify(userData.site_access) : null,
                userData.device_access ? JSON.stringify(userData.device_access) : null,
                userData.is_active !== undefined ? userData.is_active : 1
            ];

            const result = await this.executeNonQuery(query, params);
            const userId = result.lastID;
            
            // 캐시 무효화
            if (this.isCacheEnabled()) {
                await this.invalidateCache(`user:${userId}`);
                await this.invalidateCache(`user:username:${userData.username}`);
            }
            
            this.logger?.info(`UserRepository::create - Created user ${userData.username} with ID: ${userId}`);
            return userId;
        } catch (error) {
            this.logger?.error(`UserRepository::create failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 사용자 정보 수정
     * @param {number} id 사용자 ID
     * @param {Object} updateData 수정할 데이터
     * @returns {Promise<boolean>} 성공 여부
     */
    async update(id, updateData) {
        try {
            const query = UserQueries.update();
            const params = [
                updateData.username,
                updateData.email,
                updateData.full_name,
                updateData.department,
                updateData.role,
                updateData.permissions ? JSON.stringify(updateData.permissions) : null,
                updateData.site_access ? JSON.stringify(updateData.site_access) : null,
                updateData.device_access ? JSON.stringify(updateData.device_access) : null,
                updateData.is_active,
                id
            ];

            const result = await this.executeNonQuery(query, params);
            const success = result.changes > 0;
            
            // 캐시 무효화
            if (success && this.isCacheEnabled()) {
                await this.invalidateCache(`user:${id}`);
                await this.invalidateCache(`user:username:*`);
            }
            
            this.logger?.info(`UserRepository::update - ${success ? 'Updated' : 'Failed to update'} user ID: ${id}`);
            return success;
        } catch (error) {
            this.logger?.error(`UserRepository::update failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 사용자 삭제
     * @param {number} id 사용자 ID
     * @returns {Promise<boolean>} 성공 여부
     */
    async delete(id) {
        try {
            const query = UserQueries.delete();
            const result = await this.executeNonQuery(query, [id]);
            const success = result.changes > 0;
            
            // 캐시 무효화
            if (success && this.isCacheEnabled()) {
                await this.invalidateCache(`user:${id}`);
                await this.invalidateCache(`user:username:*`);
            }
            
            this.logger?.info(`UserRepository::delete - ${success ? 'Deleted' : 'Failed to delete'} user ID: ${id}`);
            return success;
        } catch (error) {
            this.logger?.error(`UserRepository::delete failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 인증 및 세션 관리
    // ==========================================================================

    /**
     * 비밀번호 업데이트
     * @param {number} userId 사용자 ID
     * @param {string} passwordHash 해시된 비밀번호
     * @returns {Promise<boolean>} 성공 여부
     */
    async updatePassword(userId, passwordHash) {
        try {
            const query = UserQueries.updatePassword();
            const result = await this.executeNonQuery(query, [passwordHash, userId]);
            const success = result.changes > 0;
            
            // 캐시 무효화
            if (success && this.isCacheEnabled()) {
                await this.invalidateCache(`user:${userId}`);
            }
            
            this.logger?.info(`UserRepository::updatePassword - ${success ? 'Updated' : 'Failed to update'} password for user ${userId}`);
            return success;
        } catch (error) {
            this.logger?.error(`UserRepository::updatePassword failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 마지막 로그인 시간 업데이트
     * @param {number} userId 사용자 ID
     * @returns {Promise<boolean>} 성공 여부
     */
    async updateLastLogin(userId) {
        try {
            const query = UserQueries.updateLastLogin();
            const result = await this.executeNonQuery(query, [userId]);
            const success = result.changes > 0;
            
            // 캐시 무효화
            if (success && this.isCacheEnabled()) {
                await this.invalidateCache(`user:${userId}`);
            }
            
            this.logger?.debug(`UserRepository::updateLastLogin - Updated last login for user ${userId}`);
            return success;
        } catch (error) {
            this.logger?.error(`UserRepository::updateLastLogin failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 유효성 검증
    // ==========================================================================

    /**
     * 사용자명 중복 확인
     * @param {string} username 사용자명
     * @param {number} excludeId 제외할 사용자 ID (수정 시 사용)
     * @returns {Promise<boolean>} 중복 여부
     */
    async isUsernameTaken(username, excludeId = null) {
        try {
            const query = excludeId 
                ? UserQueries.isUsernameTakenExcluding()
                : UserQueries.isUsernameTaken();
            
            const params = excludeId 
                ? [username, excludeId]
                : [username];
            
            const results = await this.executeQuery(query, params);
            const isTaken = results[0]?.count > 0;
            
            this.logger?.debug(`UserRepository::isUsernameTaken - Username ${username} ${isTaken ? 'taken' : 'available'}`);
            return isTaken;
        } catch (error) {
            this.logger?.error(`UserRepository::isUsernameTaken failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 이메일 중복 확인
     * @param {string} email 이메일
     * @param {number} excludeId 제외할 사용자 ID
     * @returns {Promise<boolean>} 중복 여부
     */
    async isEmailTaken(email, excludeId = null) {
        try {
            const query = excludeId 
                ? UserQueries.isEmailTakenExcluding()
                : UserQueries.isEmailTaken();
            
            const params = excludeId 
                ? [email, excludeId]
                : [email];
            
            const results = await this.executeQuery(query, params);
            const isTaken = results[0]?.count > 0;
            
            this.logger?.debug(`UserRepository::isEmailTaken - Email ${email} ${isTaken ? 'taken' : 'available'}`);
            return isTaken;
        } catch (error) {
            this.logger?.error(`UserRepository::isEmailTaken failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 통계 및 집계
    // ==========================================================================

    /**
     * 테넌트별 사용자 수 조회
     * @param {number} tenantId 테넌트 ID
     * @returns {Promise<number>} 사용자 수
     */
    async countByTenant(tenantId) {
        try {
            const query = UserQueries.countByTenant();
            const results = await this.executeQuery(query, [tenantId]);
            const count = results[0]?.count || 0;
            this.logger?.debug(`UserRepository::countByTenant - Found ${count} users for tenant ${tenantId}`);
            return count;
        } catch (error) {
            this.logger?.error(`UserRepository::countByTenant failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 역할별 사용자 통계
     * @param {number} tenantId 테넌트 ID (선택사항)
     * @returns {Promise<Array>} 역할별 사용자 수
     */
    async getStatsByRole(tenantId = null) {
        try {
            const query = tenantId 
                ? UserQueries.getStatsByRoleAndTenant()
                : UserQueries.getStatsByRole();
            
            const params = tenantId ? [tenantId] : [];
            const results = await this.executeQuery(query, params);
            
            this.logger?.debug(`UserRepository::getStatsByRole - Found stats for ${results.length} roles`);
            return results;
        } catch (error) {
            this.logger?.error(`UserRepository::getStatsByRole failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * 전체 사용자 수 조회
     * @returns {Promise<number>} 사용자 수
     */
    async getTotalCount() {
        try {
            const query = UserQueries.getTotalCount();
            const results = await this.executeQuery(query);
            const count = results[0]?.count || 0;
            this.logger?.debug(`UserRepository::getTotalCount - Total users: ${count}`);
            return count;
        } catch (error) {
            this.logger?.error(`UserRepository::getTotalCount failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 유틸리티 메서드
    // ==========================================================================

    /**
     * 사용자가 존재하는지 확인
     * @param {number} id 사용자 ID
     * @returns {Promise<boolean>} 존재 여부
     */
    async exists(id) {
        try {
            const user = await this.findById(id);
            return user !== null;
        } catch (error) {
            this.logger?.error(`UserRepository::exists failed: ${error.message}`);
            return false;
        }
    }

    /**
     * 사용자 활성화/비활성화
     * @param {number} userId 사용자 ID
     * @param {boolean} isActive 활성화 여부
     * @returns {Promise<boolean>} 성공 여부
     */
    async setActive(userId, isActive) {
        try {
            const query = UserQueries.setActive();
            const result = await this.executeNonQuery(query, [isActive ? 1 : 0, userId]);
            const success = result.changes > 0;
            
            // 캐시 무효화
            if (success && this.isCacheEnabled()) {
                await this.invalidateCache(`user:${userId}`);
            }
            
            this.logger?.info(`UserRepository::setActive - ${success ? 'Updated' : 'Failed to update'} active status for user ${userId} to ${isActive}`);
            return success;
        } catch (error) {
            this.logger?.error(`UserRepository::setActive failed: ${error.message}`);
            throw error;
        }
    }
}

module.exports = UserRepository;