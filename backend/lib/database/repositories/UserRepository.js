// =============================================================================
// backend/lib/database/repositories/UserRepository.js
// 🔧 DeviceRepository 패턴과 완전 동일 (쿼리 분리 적용)
// =============================================================================

const BaseRepository = require('./BaseRepository');
const UserQueries = require('../queries/UserQueries');

class UserRepository extends BaseRepository {
    constructor() {
        // DeviceRepository와 동일한 패턴: 매개변수 없는 생성자
        super('users');
        console.log('👤 UserRepository initialized with standard pattern');
    }

    // ==========================================================================
    // 기본 CRUD 연산 (UserQueries 사용)
    // ==========================================================================

    async findAll() {
        try {
            const query = UserQueries.findAll();
            const results = await this.executeQuery(query);
            return results;
        } catch (error) {
            console.error(`UserRepository::findAll failed: ${error.message}`);
            throw error;
        }
    }

    async findById(id) {
        try {
            const query = UserQueries.findById();
            const results = await this.executeQuery(query, [id]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`UserRepository::findById failed: ${error.message}`);
            throw error;
        }
    }

    async findByUsername(username) {
        try {
            const query = UserQueries.findByUsername();
            const results = await this.executeQuery(query, [username]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`UserRepository::findByUsername failed: ${error.message}`);
            throw error;
        }
    }

    async findByEmail(email) {
        try {
            const query = UserQueries.findByEmail();
            const results = await this.executeQuery(query, [email]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`UserRepository::findByEmail failed: ${error.message}`);
            throw error;
        }
    }

    async findForAuthentication(usernameOrEmail) {
        try {
            const query = UserQueries.findForAuthentication();
            const results = await this.executeQuery(query, [usernameOrEmail, usernameOrEmail]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`UserRepository::findForAuthentication failed: ${error.message}`);
            throw error;
        }
    }

    async findByTenant(tenantId) {
        try {
            const query = UserQueries.findByTenant();
            const results = await this.executeQuery(query, [tenantId]);
            return results;
        } catch (error) {
            console.error(`UserRepository::findByTenant failed: ${error.message}`);
            throw error;
        }
    }

    async findByRole(role) {
        try {
            const query = UserQueries.findByRole();
            const results = await this.executeQuery(query, [role]);
            return results;
        } catch (error) {
            console.error(`UserRepository::findByRole failed: ${error.message}`);
            throw error;
        }
    }

    async findActiveUsers() {
        try {
            const query = UserQueries.findActiveUsers();
            const results = await this.executeQuery(query);
            return results;
        } catch (error) {
            console.error(`UserRepository::findActiveUsers failed: ${error.message}`);
            throw error;
        }
    }

    async findActiveUsersByTenant(tenantId) {
        try {
            const query = UserQueries.findActiveUsersByTenant();
            const results = await this.executeQuery(query, [tenantId]);
            return results;
        } catch (error) {
            console.error(`UserRepository::findActiveUsersByTenant failed: ${error.message}`);
            throw error;
        }
    }

    async findInactiveUsers(days = 30) {
        try {
            const query = UserQueries.findInactiveUsers();
            const results = await this.executeQuery(query, [days]);
            return results;
        } catch (error) {
            console.error(`UserRepository::findInactiveUsers failed: ${error.message}`);
            throw error;
        }
    }

    async findWithSessions(id) {
        try {
            const query = UserQueries.findWithSessions();
            const results = await this.executeQuery(query, [id]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`UserRepository::findWithSessions failed: ${error.message}`);
            throw error;
        }
    }

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
                userData.is_active !== false ? 1 : 0
            ];
            
            const result = await this.executeNonQuery(query, params);
            return result.insertId || result.lastInsertRowid;
        } catch (error) {
            console.error(`UserRepository::create failed: ${error.message}`);
            throw error;
        }
    }

    async update(id, userData) {
        try {
            const query = UserQueries.update();
            const params = [
                userData.username,
                userData.email,
                userData.full_name || null,
                userData.department || null,
                userData.role || 'viewer',
                userData.permissions ? JSON.stringify(userData.permissions) : null,
                userData.site_access ? JSON.stringify(userData.site_access) : null,
                userData.device_access ? JSON.stringify(userData.device_access) : null,
                userData.is_active !== false ? 1 : 0,
                id
            ];
            
            const result = await this.executeNonQuery(query, params);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`UserRepository::update failed: ${error.message}`);
            throw error;
        }
    }

    async updatePassword(id, passwordHash) {
        try {
            const query = UserQueries.updatePassword();
            const result = await this.executeNonQuery(query, [passwordHash, id]);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`UserRepository::updatePassword failed: ${error.message}`);
            throw error;
        }
    }

    async updateLastLogin(id) {
        try {
            const query = UserQueries.updateLastLogin();
            const result = await this.executeNonQuery(query, [id]);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`UserRepository::updateLastLogin failed: ${error.message}`);
            throw error;
        }
    }

    async updatePermissions(id, permissions) {
        try {
            const query = UserQueries.updatePermissions();
            const permissionsJson = permissions ? JSON.stringify(permissions) : null;
            const result = await this.executeNonQuery(query, [permissionsJson, id]);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`UserRepository::updatePermissions failed: ${error.message}`);
            throw error;
        }
    }

    async updateSiteAccess(id, siteAccess) {
        try {
            const query = UserQueries.updateSiteAccess();
            const siteAccessJson = siteAccess ? JSON.stringify(siteAccess) : null;
            const result = await this.executeNonQuery(query, [siteAccessJson, id]);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`UserRepository::updateSiteAccess failed: ${error.message}`);
            throw error;
        }
    }

    async updateDeviceAccess(id, deviceAccess) {
        try {
            const query = UserQueries.updateDeviceAccess();
            const deviceAccessJson = deviceAccess ? JSON.stringify(deviceAccess) : null;
            const result = await this.executeNonQuery(query, [deviceAccessJson, id]);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`UserRepository::updateDeviceAccess failed: ${error.message}`);
            throw error;
        }
    }

    async setPasswordResetToken(id, token, expiresAt) {
        try {
            const query = UserQueries.setPasswordResetToken();
            const result = await this.executeNonQuery(query, [token, expiresAt, id]);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`UserRepository::setPasswordResetToken failed: ${error.message}`);
            throw error;
        }
    }

    async clearPasswordResetToken(id) {
        try {
            const query = UserQueries.clearPasswordResetToken();
            const result = await this.executeNonQuery(query, [id]);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`UserRepository::clearPasswordResetToken failed: ${error.message}`);
            throw error;
        }
    }

    async deleteById(id) {
        try {
            const query = UserQueries.delete();
            const result = await this.executeNonQuery(query, [id]);
            return result.affectedRows > 0 || result.changes > 0;
        } catch (error) {
            console.error(`UserRepository::deleteById failed: ${error.message}`);
            throw error;
        }
    }

    async exists(id) {
        try {
            const query = UserQueries.exists();
            const result = await this.executeQuerySingle(query, [id]);
            return result && result.count > 0;
        } catch (error) {
            console.error(`UserRepository::exists failed: ${error.message}`);
            throw error;
        }
    }

    // ==========================================================================
    // 특화 메서드들 (UserQueries 사용)
    // ==========================================================================

    async checkUsernameExists(username, excludeId = null) {
        try {
            let query = UserQueries.checkUsernameExists();
            let params = [username];
            
            if (excludeId) {
                query += ' AND id != ?';
                params.push(excludeId);
            }
            
            const result = await this.executeQuerySingle(query, params);
            return result && result.count > 0;
        } catch (error) {
            console.error(`UserRepository::checkUsernameExists failed: ${error.message}`);
            throw error;
        }
    }

    async checkEmailExists(email, excludeId = null) {
        try {
            let query = UserQueries.checkEmailExists();
            let params = [email];
            
            if (excludeId) {
                query += ' AND id != ?';
                params.push(excludeId);
            }
            
            const result = await this.executeQuerySingle(query, params);
            return result && result.count > 0;
        } catch (error) {
            console.error(`UserRepository::checkEmailExists failed: ${error.message}`);
            throw error;
        }
    }

    async getUserStatistics(tenantId) {
        try {
            const query = UserQueries.getUserStatistics();
            const results = await this.executeQuery(query, [tenantId]);
            return results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error(`UserRepository::getUserStatistics failed: ${error.message}`);
            throw error;
        }
    }

    async searchByName(searchTerm, tenantId = null) {
        try {
            let query = UserQueries.searchByName();
            let params = [`%${searchTerm}%`];
            
            if (tenantId) {
                query += ' AND tenant_id = ?';
                params.push(tenantId);
            }
            
            const results = await this.executeQuery(query, params);
            return results;
        } catch (error) {
            console.error(`UserRepository::searchByName failed: ${error.message}`);
            throw error;
        }
    }

    async getUsersByPermission(permission, tenantId = null) {
        try {
            let query = UserQueries.getUsersByPermission();
            let params = [`%${permission}%`];
            
            if (tenantId) {
                query += ' AND tenant_id = ?';
                params.push(tenantId);
            }
            
            const results = await this.executeQuery(query, params);
            return results;
        } catch (error) {
            console.error(`UserRepository::getUsersByPermission failed: ${error.message}`);
            throw error;
        }
    }
}

module.exports = UserRepository;