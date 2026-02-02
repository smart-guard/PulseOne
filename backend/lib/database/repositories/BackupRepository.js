const BaseRepository = require('./BaseRepository');

/**
 * BackupRepository class
 * Handles database operations for system backups.
 */
class BackupRepository extends BaseRepository {
    constructor() {
        super('backups');
    }

    /**
     * 모든 백업 기록을 조회합니다 (삭제되지 않은 것 위주).
     */
    async findAll(tenantId = null, options = {}) {
        try {
            const query = this.query().where('is_deleted', 0);

            if (options.status) {
                query.where('status', options.status);
            }

            if (options.type) {
                query.where('type', options.type);
            }

            // Pagination
            const page = parseInt(options.page) || 1;
            const limit = parseInt(options.limit) || 20;
            const offset = (page - 1) * limit;

            query.orderBy('created_at', 'DESC');

            const items = await query.limit(limit).offset(offset);

            // Total count
            const countQuery = this.query().where('is_deleted', 0);
            if (options.status) countQuery.where('status', options.status);
            if (options.type) countQuery.where('type', options.type);

            const countResult = await countQuery.count('id as total').first();
            const total = countResult ? (countResult.total || 0) : 0;

            return { items, total, page, limit };
        } catch (error) {
            this.logger.error('BackupRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 백업 정보를 조회합니다.
     */
    async findById(id) {
        try {
            return await this.query().where('id', id).where('is_deleted', 0).first();
        } catch (error) {
            this.logger.error('BackupRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 새로운 백업 기록을 생성합니다.
     */
    async create(data) {
        try {
            const [id] = await this.query().insert({
                name: data.name,
                filename: data.filename,
                type: data.type || 'full',
                status: data.status || 'running',
                size: data.size || 0,
                location: data.location || '/app/data/backup',
                created_by: data.created_by || 'system',
                description: data.description || '',
                duration: data.duration || 0,
                is_deleted: 0
            });
            return id;
        } catch (error) {
            this.logger.error('BackupRepository.create 오류:', error);
            throw error;
        }
    }

    /**
     * 백업 정보를 업데이트합니다.
     */
    async update(id, data) {
        try {
            return await this.query().where('id', id).update(data);
        } catch (error) {
            this.logger.error('BackupRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 백업을 논리적으로 삭제합니다.
     */
    async softDelete(id) {
        try {
            return await this.update(id, { is_deleted: 1 });
        } catch (error) {
            this.logger.error('BackupRepository.softDelete 오류:', error);
            throw error;
        }
    }
}

module.exports = BackupRepository;
