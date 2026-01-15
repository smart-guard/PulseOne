const BaseRepository = require('./BaseRepository');

class RoleRepository extends BaseRepository {
    constructor() {
        super('roles');
        this.allowedFields = ['id', 'name', 'description', 'is_system'];
    }

    // Updated to use Knex builder when trx is present to avoid raw result parsing mismatch
    async findById(id, trx = null) {
        if (trx) {
            return trx(this.tableName).where('id', id).first();
        }
        const query = `SELECT * FROM ${this.tableName} WHERE id = ?`;
        return this.executeQuerySingle(query, [id]);
    }

    // Get role with its permissions
    async getRoleWithPermissions(roleId, trx = null) {
        const role = await this.findById(roleId, trx);
        if (!role) return null;

        const permissions = await this.getPermissionsForRole(roleId, trx);
        return { ...role, permissions: permissions.map(p => p.id) };
    }

    // Get all permissions assigned to a role
    async getPermissionsForRole(roleId, trx = null) {
        if (trx) {
            return trx('permissions')
                .join('role_permissions', 'permissions.id', 'role_permissions.permission_id')
                .where('role_permissions.role_id', roleId)
                .select('permissions.*');
        }
        const query = `
      SELECT p.* 
      FROM permissions p
      JOIN role_permissions rp ON p.id = rp.permission_id
      WHERE rp.role_id = ?
    `;
        return this.executeQuery(query, [roleId]);
    }

    // Add permission to role (Legacy/Direct helper)
    async addPermission(roleId, permissionId) {
        // Typically use executeNonQuery or knex insert
        const query = `INSERT OR IGNORE INTO role_permissions (role_id, permission_id) VALUES (?, ?)`;
        return this.executeNonQuery(query, [roleId, permissionId]);
    }

    // Remove permission from role
    async removePermission(roleId, permissionId) {
        const query = `DELETE FROM role_permissions WHERE role_id = ? AND permission_id = ?`;
        return this.executeNonQuery(query, [roleId, permissionId]);
    }

    // Clear all permissions for a role
    async clearPermissions(roleId) {
        const query = `DELETE FROM role_permissions WHERE role_id = ?`;
        return this.executeNonQuery(query, [roleId]);
    }

    // Get all roles with user count
    async findAllWithStats() {
        const query = `
      SELECT r.*, 
        (SELECT COUNT(*) FROM role_permissions rp WHERE rp.role_id = r.id) as permissionCount,
        (SELECT COUNT(*) FROM users u WHERE u.role = r.id AND u.is_deleted = 0) as userCount
      FROM roles r
      ORDER BY r.is_system DESC, r.name ASC
    `;
        return this.executeQuery(query);
    }
}

module.exports = RoleRepository;
