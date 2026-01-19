const BaseRepository = require('./BaseRepository');

/**
 * ExportProfileRepository class
 * Handles database operations for export profiles (data mapping).
 */
class ExportProfileRepository extends BaseRepository {
    constructor() {
        super('export_profiles');
    }

    /**
     * 전체 프로파일 조회
     */
    /**
     * 전체 프로파일 조회
     */
    async findAll() {
        try {
            return await this.query().orderBy('name', 'ASC');
        } catch (error) {
            this.logger.error('ExportProfileRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 프로파일 조회
     */
    async findById(id) {
        try {
            return await this.query().where('id', id).first();
        } catch (error) {
            this.logger.error('ExportProfileRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 이름으로 프로파일 찾기
     */
    async findByName(name) {
        return await this.query()
            .where({ name })
            .first();
    }

    /**
     * 활성화된 프로파일 조회
     */
    async findActive() {
        return await this.query()
            .where({ is_enabled: 1 })
            .orderBy('name', 'ASC');
    }

    /**
     * 프로파일 생성
     */
    async save(data) {
        try {
            const dataToInsert = {
                name: data.name,
                description: data.description || '',
                data_points: typeof data.data_points === 'object' ? JSON.stringify(data.data_points) : (data.data_points || '[]'),
                is_enabled: data.is_enabled !== undefined ? data.is_enabled : 1,
                created_at: this.knex.fn.now(),
                updated_at: this.knex.fn.now()
            };

            const [id] = await this.knex(this.tableName).insert(dataToInsert);
            return await this.findById(id);
        } catch (error) {
            this.logger.error('ExportProfileRepository.save 오류:', error);
            throw error;
        }
    }

    /**
     * 프로파일 업데이트
     */
    async update(id, data, tenantId = null) {
        try {
            const dataToUpdate = {
                updated_at: this.knex.fn.now()
            };

            const allowedFields = ['name', 'description', 'data_points', 'is_enabled'];
            allowedFields.forEach(field => {
                if (data[field] !== undefined) {
                    if (field === 'data_points' && typeof data[field] === 'object') {
                        dataToUpdate[field] = JSON.stringify(data[field]);
                    } else {
                        dataToUpdate[field] = data[field];
                    }
                }
            });

            const query = this.query().where('id', id);

            const affected = await query.update(dataToUpdate);
            return affected > 0 ? await this.findById(id) : null;
        } catch (error) {
            this.logger.error('ExportProfileRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 프로파일 삭제
     */
    async deleteById(id) {
        try {
            const query = this.query().where('id', id);

            const affected = await query.delete();
            return affected > 0;
        } catch (error) {
            this.logger.error('ExportProfileRepository.deleteById 오류:', error);
            throw error;
        }
    }
}

module.exports = ExportProfileRepository;
