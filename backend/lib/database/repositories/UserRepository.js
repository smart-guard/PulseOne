// =============================================================================
// backend/lib/database/repositories/UserRepository.js
// Knex 기반 BaseRepository 확장 및 부분 업데이트 지원
// =============================================================================

const BaseRepository = require('./BaseRepository');
const UserQueries = require('../queries/UserQueries');

class UserRepository extends BaseRepository {
    constructor() {
        super('users');
    }

    // ==========================================================================
    // 조회 메소드
    // ==========================================================================

    async findAll(tenantId = null) {
        try {
            let query = this.query().select(
                'id', 'tenant_id', 'username', 'email', 'full_name', 'department',
                'role', 'is_active', 'last_login', 'created_at', 'updated_at'
            );

            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }

            return await query.orderBy('username', 'asc');
        } catch (error) {
            this.logger.error('UserRepository.findAll 오류:', error);
            throw error;
        }
    }

    async findById(id, tenantId = null) {
        try {
            let query = this.query().where('id', id);

            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }

            const user = await query.first();
            return user || null;
        } catch (error) {
            this.logger.error('UserRepository.findById 오류:', error);
            throw error;
        }
    }

    async findByUsername(username) {
        try {
            return await this.query().where('username', username).first();
        } catch (error) {
            this.logger.error('UserRepository.findByUsername 오류:', error);
            throw error;
        }
    }

    async findForAuthentication(usernameOrEmail) {
        try {
            return await this.query()
                .where(builder => {
                    builder.where('username', usernameOrEmail)
                        .orWhere('email', usernameOrEmail);
                })
                .andWhere('is_active', 1)
                .first();
        } catch (error) {
            this.logger.error('UserRepository.findForAuthentication 오류:', error);
            throw error;
        }
    }

    // ==========================================================================
    // CRUD 메소드
    // ==========================================================================

    async create(userData) {
        try {
            // JSON 필드 처리 (Service에서 이미 처리했을 수도 있지만 Repository에서도 안전하게 보장)
            const dataToInsert = {
                tenant_id: userData.tenant_id,
                username: userData.username,
                email: userData.email,
                password_hash: userData.password_hash,
                full_name: userData.full_name,
                department: userData.department,
                role: userData.role || 'viewer',
                permissions: typeof userData.permissions === 'object' ? JSON.stringify(userData.permissions) : userData.permissions,
                site_access: typeof userData.site_access === 'object' ? JSON.stringify(userData.site_access) : userData.site_access,
                device_access: typeof userData.device_access === 'object' ? JSON.stringify(userData.device_access) : userData.device_access,
                is_active: userData.is_active !== false ? 1 : 0
            };

            const [id] = await this.query().insert(dataToInsert);
            return id;
        } catch (error) {
            this.logger.error('UserRepository.create 오류:', error);
            throw error;
        }
    }

    /**
     * 전체 업데이트 (PUT)
     */
    async update(id, userData, tenantId = null) {
        try {
            const dataToUpdate = {
                username: userData.username,
                email: userData.email,
                full_name: userData.full_name,
                department: userData.department,
                role: userData.role,
                permissions: typeof userData.permissions === 'object' ? JSON.stringify(userData.permissions) : userData.permissions,
                site_access: typeof userData.site_access === 'object' ? JSON.stringify(userData.site_access) : userData.site_access,
                device_access: typeof userData.device_access === 'object' ? JSON.stringify(userData.device_access) : userData.device_access,
                is_active: userData.is_active !== false ? 1 : 0,
                updated_at: this.knex.fn.now()
            };

            let query = this.query().where('id', id);
            if (tenantId) query = query.where('tenant_id', tenantId);

            const affected = await query.update(dataToUpdate);
            return affected > 0;
        } catch (error) {
            this.logger.error('UserRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 부분 업데이트 (PATCH)
     */
    async updatePartial(id, updateData, tenantId = null) {
        try {
            const allowedFields = [
                'username', 'email', 'password_hash', 'full_name', 'department',
                'role', 'permissions', 'site_access', 'device_access', 'is_active',
                'last_login', 'password_reset_token', 'password_reset_expires'
            ];

            const dataToUpdate = {
                updated_at: this.knex.fn.now()
            };

            allowedFields.forEach(field => {
                if (updateData[field] !== undefined) {
                    if (['permissions', 'site_access', 'device_access'].includes(field) && typeof updateData[field] === 'object') {
                        dataToUpdate[field] = JSON.stringify(updateData[field]);
                    } else if (field === 'is_active') {
                        dataToUpdate[field] = updateData[field] ? 1 : 0;
                    } else {
                        dataToUpdate[field] = updateData[field];
                    }
                }
            });

            let query = this.query().where('id', id);
            if (tenantId) query = query.where('tenant_id', tenantId);

            const affected = await query.update(dataToUpdate);
            return affected > 0;
        } catch (error) {
            this.logger.error('UserRepository.updatePartial 오류:', error);
            throw error;
        }
    }

    async deleteById(id, tenantId = null) {
        try {
            let query = this.query().where('id', id);
            if (tenantId) query = query.where('tenant_id', tenantId);

            const affected = await query.delete();
            return affected > 0;
        } catch (error) {
            this.logger.error('UserRepository.deleteById 오류:', error);
            throw error;
        }
    }

    // ==========================================================================
    // 유효성 및 통계
    // ==========================================================================

    async checkExists(id, tenantId = null) {
        try {
            let query = this.query().where('id', id);
            if (tenantId) query = query.where('tenant_id', tenantId);
            const user = await query.select('id').first();
            return !!user;
        } catch (error) {
            return false;
        }
    }

    async getStats(tenantId) {
        try {
            const query = UserQueries.getUserStatistics();
            const results = await this.executeQuery(query, [tenantId]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            this.logger.error('UserRepository.getStats 오류:', error);
            return null;
        }
    }
}

module.exports = UserRepository;