// =============================================================================
// backend/lib/services/UserService.js
// 비즈니스 로직 처리 및 테넌트 격리 보장
// =============================================================================

const BaseService = require('./BaseService');
const UserRepository = require('../database/repositories/UserRepository');
const bcrypt = require('bcryptjs');

class UserService extends BaseService {
    constructor() {
        super(new UserRepository());
    }

    // ==========================================================================
    // 사용자 조회
    // ==========================================================================

    async getAllUsers(tenantId = null, filters = {}) {
        return await this.handleRequest(async () => {
            const users = await this.repository.findAll(tenantId, filters);
            return users.map(user => this.sanitizeUser(user));
        }, 'UserService.getAllUsers');
    }

    async getUserById(id, tenantId = null) {
        return await this.handleRequest(async () => {
            const user = await this.repository.findById(id, tenantId);
            if (!user) throw new Error('사용자를 찾을 수 없습니다.');
            return this.sanitizeUser(user);
        }, 'UserService.getUserById');
    }

    // ==========================================================================
    // CRUD 연산
    // ==========================================================================

    async createUser(userData, tenantId = null) {
        return await this.handleRequest(async () => {
            // 중복 확인
            const existingUser = await this.repository.findByUsername(userData.username);
            if (existingUser) throw new Error('이미 존재하는 사용자명입니다.');

            // 비밀번호 해싱
            if (userData.password) {
                userData.password_hash = await bcrypt.hash(userData.password, 10);
                delete userData.password;
            }

            // 테넌트 보장 (System Admin인 경우 req.body.tenant_id 허용)
            if (tenantId) {
                userData.tenant_id = tenantId;
            } else if (!userData.tenant_id) {
                // System Admin이지만 tenant_id를 지정하지 않은 경우 에러 또는 기본 처리
                // 여기서는 에러보다는 필수로 지정해야 함을 가정
                // throw new Error('테넌트를 지정해야 합니다.');
            }

            const id = await this.repository.create(userData);
            return await this.repository.findById(id, tenantId || userData.tenant_id);
        }, 'CreateUser');
    }

    async updateUser(id, userData, tenantId) {
        return await this.handleRequest(async () => {
            const updated = await this.repository.update(id, userData, tenantId);
            if (!updated) throw new Error('User not found or update failed');
            return await this.repository.findById(id, tenantId);
        }, 'UpdateUser');
    }

    async patchUser(id, updateData, tenantId = null) {
        return await this.handleRequest(async () => {
            // 비밀번호 부분 업데이트 시 해싱 처리
            if (updateData.password) {
                updateData.password_hash = await bcrypt.hash(updateData.password, 10);
                delete updateData.password;
            }

            const success = await this.repository.updatePartial(id, updateData, tenantId);
            if (!success) throw new Error('사용자 부분 업데이트 실패');
            return { id, success: true };
        }, 'UserService.patchUser');
    }

    async deleteUser(id, tenantId) {
        return await this.handleRequest(async () => {
            const deleted = await this.repository.deleteById(id, tenantId);
            if (!deleted) throw new Error('User not found or delete failed');
            return { id, deleted: true };
        }, 'DeleteUser');
    }

    async restoreUser(id, tenantId) {
        return await this.handleRequest(async () => {
            const restored = await this.repository.restoreById(id, tenantId);
            if (!restored) throw new Error('User not found or restore failed');
            return await this.repository.findById(id, tenantId);
        }, 'RestoreUser');
    }

    // ==========================================================================
    // 유틸리티
    // ==========================================================================

    /**
     * 민감 정보 제거 (비밀번호 해시 등)
     */
    sanitizeUser(user) {
        if (!user) return null;
        const sanitized = { ...user };
        delete sanitized.password_hash;
        delete sanitized.password_reset_token;

        // JSON 필드 파싱 처리
        ['permissions', 'site_access', 'device_access'].forEach(field => {
            if (typeof sanitized[field] === 'string') {
                try {
                    sanitized[field] = JSON.parse(sanitized[field]);
                } catch (e) {
                    sanitized[field] = [];
                }
            }
        });

        return sanitized;
    }

    async getStats(tenantId) {
        const stats = await this.repository.getStats(tenantId);
        return this.createResponse(true, stats);
    }
}

module.exports = new UserService();
