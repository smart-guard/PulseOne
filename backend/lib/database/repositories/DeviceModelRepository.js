const BaseRepository = require('./BaseRepository');

/**
 * DeviceModelRepository class
 * Handles database operations for device models.
 */
class DeviceModelRepository extends BaseRepository {
    constructor() {
        super('device_models');
    }

    /**
     * 모든 디바이스 모델을 조회합니다.
     */
    async findAll(options = {}) {
        try {
            const query = this.query()
                .select('device_models.*', 'm.name as manufacturer_name')
                .select(this.knex.raw('(SELECT id FROM template_devices WHERE model_id = device_models.id LIMIT 1) as template_id'))
                .leftJoin('manufacturers as m', 'm.id', 'device_models.manufacturer_id');

            if (options.manufacturerId) {
                query.where('manufacturer_id', options.manufacturerId);
            }

            if (options.isActive !== undefined) {
                query.where('device_models.is_active', options.isActive ? 1 : 0);
            }

            if (options.deviceType) {
                query.where('device_type', options.deviceType);
            }

            if (options.search) {
                query.where('device_models.name', 'LIKE', `%${options.search}%`);
            }

            const sortBy = options.sort_by || 'device_models.name';
            const sortOrder = (options.sort_order || 'ASC').toUpperCase();
            query.orderBy(sortBy, sortOrder);

            // 페이지네이션 처리
            if (options.page && options.limit) {
                const page = parseInt(options.page);
                const limit = parseInt(options.limit);
                const offset = (page - 1) * limit;

                // 전체 카운트 조회
                const countQuery = this.query()
                    .count('* as total');

                if (options.manufacturerId) countQuery.where('manufacturer_id', options.manufacturerId);
                if (options.isActive !== undefined) countQuery.where('is_active', options.isActive ? 1 : 0);
                if (options.deviceType) countQuery.where('device_type', options.deviceType);
                if (options.search) countQuery.where('name', 'LIKE', `%${options.search}%`);

                const [{ total }] = await countQuery;
                const items = await query.limit(limit).offset(offset);

                return {
                    items,
                    pagination: this.calculatePagination(total, page, limit)
                };
            }

            return await query;
        } catch (error) {
            this.logger.error('DeviceModelRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 디바이스 모델을 조회합니다.
     */
    async findById(id) {
        try {
            return await this.query()
                .select('device_models.*', 'm.name as manufacturer_name')
                .select(this.knex.raw('(SELECT id FROM template_devices WHERE model_id = device_models.id LIMIT 1) as template_id'))
                .leftJoin('manufacturers as m', 'm.id', 'device_models.manufacturer_id')
                .where('device_models.id', id)
                .first();
        } catch (error) {
            this.logger.error('DeviceModelRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 새로운 디바이스 모델을 생성합니다.
     */
    async create(data) {
        try {
            const [id] = await this.query().insert({
                manufacturer_id: data.manufacturer_id,
                name: data.name,
                model_number: data.model_number || null,
                device_type: data.device_type || 'PLC',
                description: data.description || null,
                image_url: data.image_url || null,
                manual_url: data.manual_url || null,
                metadata: typeof data.metadata === 'object' ? JSON.stringify(data.metadata) : (data.metadata || null),
                is_active: data.is_active !== false ? 1 : 0
            });
            return await this.findById(id);
        } catch (error) {
            this.logger.error('DeviceModelRepository.create 오류:', error);
            throw error;
        }
    }

    /**
     * 디바이스 모델 정보를 업데이트합니다.
     */
    async update(id, data) {
        try {
            const updateData = {
                updated_at: this.knex.fn.now()
            };

            const fields = [
                'manufacturer_id', 'name', 'model_number', 'device_type',
                'description', 'image_url', 'manual_url', 'metadata', 'is_active'
            ];

            fields.forEach(field => {
                if (data[field] !== undefined) {
                    if (field === 'metadata' && typeof data[field] === 'object') {
                        updateData[field] = JSON.stringify(data[field]);
                    } else if (field === 'is_active') {
                        updateData[field] = data[field] ? 1 : 0;
                    } else {
                        updateData[field] = data[field];
                    }
                }
            });

            const affected = await this.query().where('id', id).update(updateData);
            return affected > 0 ? await this.findById(id) : null;
        } catch (error) {
            this.logger.error('DeviceModelRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 디바이스 모델을 삭제합니다.
     */
    async deleteById(id) {
        try {
            const affected = await this.query().where('id', id).del();
            return affected > 0;
        } catch (error) {
            this.logger.error('DeviceModelRepository.deleteById 오류:', error);
            throw error;
        }
    }
}

module.exports = DeviceModelRepository;
