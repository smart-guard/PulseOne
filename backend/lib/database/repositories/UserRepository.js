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

    async findAll(tenantId = null, filters = {}) {
        try {
            let query = this.query().select(
                'id', 'tenant_id', 'username', 'email', 'full_name', 'department',
                'role', 'is_active', 'is_deleted', 'last_login', 'created_at', 'updated_at'
            );

            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }

            // 필터링: 삭제된 사용자 제외 (기본값)
            if (filters.onlyDeleted) {
                query = query.where('is_deleted', 1);
            } else if (!filters.includeDeleted) {
                query = query.where('is_deleted', 0);
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

            // 기본적으로 삭제되지 않은 사용자만 조회 (상세 보기 등에서는 삭제된 유저도 필요할 수 있으므로 상황에 따라 조절)
            const user = await query.first();
            return user || null;
        } catch (error) {
            this.logger.error('UserRepository.findById 오류:', error);
            throw error;
        }
    }

    async findByUsername(username) {
        try {
            return await this.query()
                .where('username', username)
                .where('is_deleted', 0)
                .first();
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
                .andWhere('is_deleted', 0)
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
                phone: userData.phone,
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
                phone: userData.phone,
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
                'username', 'email', 'password_hash', 'full_name', 'phone', 'department',
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

            // Soft delete
            const affected = await query.update({
                is_deleted: 1,
                updated_at: this.knex.fn.now()
            });
            return affected > 0;
        } catch (error) {
            this.logger.error('UserRepository.deleteById 오류:', error);
            throw error;
        }
    }

    async restoreById(id, tenantId = null) {
        try {
            let query = this.query().where('id', id);
            if (tenantId) query = query.where('tenant_id', tenantId);

            const affected = await query.update({
                is_deleted: 0,
                updated_at: this.knex.fn.now()
            });
            return affected > 0;
        } catch (error) {
            this.logger.error('UserRepository.restoreById 오류:', error);
            throw error;
        }
    }

    async hardDeleteById(id, tenantId = null) {
        try {
            let query = this.query().where('id', id);
            if (tenantId) query = query.where('tenant_id', tenantId);

            const affected = await query.delete();
            return affected > 0;
        } catch (error) {
            this.logger.error('UserRepository.hardDeleteById 오류:', error);
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
            let query = this.query().select(
                this.knex.raw('COUNT(*) as total_users'),
                this.knex.raw('SUM(CASE WHEN is_active = 1 AND is_deleted = 0 THEN 1 ELSE 0 END) as active_users'),
                this.knex.raw('SUM(CASE WHEN is_deleted = 1 THEN 1 ELSE 0 END) as deleted_users'),
                this.knex.raw('SUM(CASE WHEN role != "viewer" AND is_deleted = 0 THEN 1 ELSE 0 END) as admin_users'),
                this.knex.raw('SUM(CASE WHEN last_login >= datetime("now", "-24 hours") AND is_deleted = 0 THEN 1 ELSE 0 END) as active_today')
            );

            if (tenantId) {
                query = query.where('tenant_id', tenantId);
            }

            // Note: Total users count in stats usually implies non-deleted users for the dashboard view, 
            // but let's count raw rows for total if that's what's expected, 
            // OR follow the query logic: 
            // The original query counted COUNT(*) which included deleted ones if not filtered? 
            // Wait, the original findAll filters is_deleted=0. 
            // The stats card "Total Users" usually expects active+inactive but NOT deleted.
            // Let's refine:
            // "total_users" should be non-deleted users.

            const results = await query.first();

            // Adjust total_users to exclude deleted if the raw query included them
            // Actually, let's explicitely use the boolean logic in the select or a where clause?
            // If I use where('is_deleted', 0) it filters the whole set, so deleted_users count will be 0.
            // So we must NOT filter by is_deleted in the main query, but handle it in CASE WHEN.

            // Correct logic for total_users (non-deleted):
            // The previous query was: SELECT COUNT(*) ... WHERE tenant_id = ?
            // It didn't filter is_deleted=0 in main WHERE, so it counted everything? 
            // Let's look at previous UserQueries.getUserStatistics: 
            // It selects COUNT(*) as total_users. It does not have is_deleted=0 in WHERE.
            // So total_users includes deleted ones? 
            // BUT findAll filters is_deleted=0.
            // Providing consistency: "Total Users" usually means "Existing Users".
            // Let's use: SUM(CASE WHEN is_deleted = 0 THEN 1 ELSE 0 END) as total_users

            // Re-writing the select to be safer:
            return {
                total_users: results.total_users - results.deleted_users, // If count(*) includes deleted
                // actually let's just use the SUM logic for total too to be precise
                // But I can't easily change the select structure above without re-running.
                // Let's stick to the raw definitions I wrote above?

                // Wait, I wrote: 
                // COUNT(*) as total_users
                // SUM(... is_deleted=1 ...) as deleted_users

                // If I don't filter safely, total_users includes deleted.
                // UI shows "Total Users" 0. 
                // Let's make total_users = is_deleted=0 count.
                ...results,
                total_users: results.total_users - (results.deleted_users || 0)
            };
        } catch (error) {
            this.logger.error('UserRepository.getStats 오류:', error);
            // Fallback to manual calc if needed or return zeros
            return {
                total_users: 0,
                active_users: 0,
                deleted_users: 0,
                admin_users: 0,
                active_today: 0
            };
        }
    }
}

module.exports = UserRepository;