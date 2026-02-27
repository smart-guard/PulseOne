const BaseService = require('./BaseService');

class RoleService extends BaseService {
    constructor(roleRepository, permissionRepository) {
        super(roleRepository);
        this.roleRepository = roleRepository;
        this.permissionRepository = permissionRepository;
    }

    async getAllRoles() {
        return this.roleRepository.findAllWithStats();
    }

    // Updated to accept trx
    async getRole(id, trx = null) {
        return this.roleRepository.getRoleWithPermissions(id, trx);
    }

    async getAllPermissions() {
        // PermissionRepo now implements findAll
        return this.permissionRepository.findAll();
    }

    async createRole(roleData, permissions) {
        return this.transaction(async (trx) => {
            // 1. Create Role
            const roleId = roleData.id || `role_${Date.now()}`;

            await trx('roles').insert({
                id: roleId,
                name: roleData.name,
                description: roleData.description,
                is_system: 0
            });

            // 2. Add Permissions
            if (permissions && Array.isArray(permissions)) {
                const batch = permissions.map(permId => ({
                    role_id: roleId,
                    permission_id: permId
                }));
                if (batch.length > 0) {
                    await trx('role_permissions').insert(batch);
                }
            }

            // Pass trx to see uncommitted changes
            return this.getRole(roleId, trx);
        });
    }

    async updateRole(id, roleData, permissions) {
        return this.transaction(async (trx) => {
            await trx('roles').where('id', id).update({
                name: roleData.name,
                description: roleData.description,
                updated_at: trx.raw("datetime('now', 'localtime')")
            });

            if (permissions && Array.isArray(permissions)) {
                await trx('role_permissions').where('role_id', id).del();
                const batch = permissions.map(permId => ({
                    role_id: id,
                    permission_id: permId
                }));
                if (batch.length > 0) {
                    await trx('role_permissions').insert(batch);
                }
            }

            return this.getRole(id, trx);
        });
    }

    async deleteRole(id) {
        return this.transaction(async (trx) => {
            const role = await trx('roles').where('id', id).first();
            if (!role) throw new Error('Role not found');
            if (role.is_system) throw new Error('Cannot delete system role');

            const userCount = await trx('users').where('role', id).where('is_deleted', 0).count('id as count').first();
            if (userCount.count > 0) throw new Error('Cannot delete role assigned to users');

            await trx('roles').where('id', id).del();
            // Cascade delete should handle role_permissions if FK configured, or rely on manual cleanup if logic dictates.
            // Current DB schema has ON DELETE CASCADE? NO, migrate_rbac.sql:
            // CREATE TABLE role_permissions (... FOREIGN KEY (role_id) REFERENCES roles(id) ON DELETE CASCADE ...);
            // So it should cascade.
        });
    }
}

module.exports = RoleService;
