const BaseRepository = require('./BaseRepository');

class PermissionRepository extends BaseRepository {
    constructor() {
        super('permissions');
        this.allowedFields = ['id', 'name', 'description', 'category', 'resource', 'actions', 'is_system'];
    }


    // Override to ensure we can fetch by string ID
    async findById(id) {
        const query = `SELECT * FROM ${this.tableName} WHERE id = ?`;
        return this.executeQuerySingle(query, [id]);
    }

    async findAll() {
        const query = `SELECT * FROM ${this.tableName}`;
        return this.executeQuery(query);
    }
}


module.exports = PermissionRepository;
