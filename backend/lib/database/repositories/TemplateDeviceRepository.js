const BaseRepository = require('./BaseRepository');

/**
 * TemplateDeviceRepository class
 * Handles database operations for device templates.
 */
class TemplateDeviceRepository extends BaseRepository {
    constructor() {
        super('template_devices');
    }

    /**
     * 모든 템플릿을 조회합니다.
     */
    async findAll(options = {}) {
        try {
            const page = options.page ? parseInt(options.page) : 1;
            const limit = options.limit ? parseInt(options.limit) : 20;
            const offset = (page - 1) * limit;

            // Base query for counting
            const countQuery = this.query()
                .count('* as total');

            // Apply filters to count query
            if (options.modelId) countQuery.where('model_id', options.modelId);
            if (options.protocolId) countQuery.where('protocol_id', options.protocolId);
            if (options.manufacturerId) {
                countQuery.whereIn('model_id', function () {
                    this.select('id').from('device_models').where('manufacturer_id', options.manufacturerId);
                });
            }

            if (options.usageStatus === 'used') {
                countQuery.whereIn('id', function () {
                    this.select('template_device_id').from('devices').whereNotNull('template_device_id');
                });
            } else if (options.usageStatus === 'unused') {
                countQuery.whereNotIn('id', function () {
                    this.select('template_device_id').from('devices').whereNotNull('template_device_id');
                });
            }

            if (options.isPublic !== undefined) countQuery.where('is_public', options.isPublic ? 1 : 0);
            if (options.search) {
                countQuery.where('name', 'like', `%${options.search}%`);
            }
            // Soft Delete Filter: Exclude deleted items by default
            countQuery.where('is_deleted', 0);

            // Get total count
            const countResult = await countQuery.first();
            const total = parseInt(countResult.total || 0);

            // Main query for data
            const query = this.query()
                .select(
                    'template_devices.*',
                    'dm.name as model_name',
                    'dm.manual_url',
                    'dm.description as model_description',
                    'm.name as manufacturer_name',
                    'dm.manufacturer_id as manufacturer_id',
                    'p.display_name as protocol_name',
                    this.knex.raw('(SELECT COUNT(*) FROM template_data_points WHERE template_device_id = template_devices.id) as point_count'),
                    this.knex.raw('(SELECT COUNT(*) FROM devices WHERE template_device_id = template_devices.id) as device_count')
                )
                .leftJoin('device_models as dm', 'dm.id', 'template_devices.model_id')
                .leftJoin('manufacturers as m', 'm.id', 'dm.manufacturer_id')
                .leftJoin('protocols as p', 'p.id', 'template_devices.protocol_id');

            // Apply filters to main query
            if (options.modelId) query.where('model_id', options.modelId);
            if (options.protocolId) query.where('protocol_id', options.protocolId);
            if (options.manufacturerId) query.where('dm.manufacturer_id', options.manufacturerId);

            if (options.usageStatus === 'used') {
                query.whereIn('template_devices.id', function () {
                    this.select('template_device_id').from('devices').whereNotNull('template_device_id');
                });
            } else if (options.usageStatus === 'unused') {
                query.whereNotIn('template_devices.id', function () {
                    this.select('template_device_id').from('devices').whereNotNull('template_device_id');
                });
            }

            if (options.isPublic !== undefined) query.where('is_public', options.isPublic ? 1 : 0);
            if (options.search) {
                query.where('template_devices.name', 'like', `%${options.search}%`);
            }
            // Soft Delete Filter: Exclude deleted items by default
            query.where('template_devices.is_deleted', 0);

            const sortBy = options.sort_by || 'template_devices.name';
            const sortOrder = (options.sort_order || 'ASC').toUpperCase();

            // Apply sorting and pagination
            query.orderBy(sortBy, sortOrder)
                .limit(limit)
                .offset(offset);

            const items = await query;

            // Parse config if it exists
            items.forEach(item => {
                if (item.config && typeof item.config === 'string') {
                    try {
                        item.config = JSON.parse(item.config);
                    } catch (e) {
                        item.config = {};
                    }
                }
            });

            return {
                items: items,
                pagination: {
                    total_count: total,
                    total_pages: Math.ceil(total / limit),
                    current_page: page,
                    limit: limit
                }
            };
        } catch (error) {
            this.logger.error('TemplateDeviceRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 템플릿을 조회합니다.
     */
    async findById(id, trx = null) {
        try {
            const query = trx ? trx(this.tableName) : this.query();
            const template = await query
                .select('template_devices.*', 'dm.name as model_name', 'dm.manual_url', 'dm.description as model_description', 'm.name as manufacturer_name', 'dm.manufacturer_id as manufacturer_id', 'p.display_name as protocol_name')
                .leftJoin('device_models as dm', 'dm.id', 'template_devices.model_id')
                .leftJoin('manufacturers as m', 'm.id', 'dm.manufacturer_id')
                .leftJoin('protocols as p', 'p.id', 'template_devices.protocol_id')
                .where('template_devices.id', id)
                .andWhere('template_devices.is_deleted', 0) // Soft delete check
                .first();

            if (template) {
                // Fetch associated data points
                const dataPointQuery = trx ? trx('template_data_points') : this.knex('template_data_points');
                const dataPoints = await dataPointQuery
                    .where('template_device_id', id)
                    .orderBy('address', 'asc');

                template.data_points = dataPoints;

                // Parse config
                if (template.config && typeof template.config === 'string') {
                    try {
                        template.config = JSON.parse(template.config);
                    } catch (e) {
                        template.config = {};
                    }
                }
            }

            return template;
        } catch (error) {
            this.logger.error('TemplateDeviceRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 새로운 템플릿을 생성합니다.
     */
    async create(data, trx = null) {
        try {
            const query = trx ? trx(this.tableName) : this.query();
            // is_deleted defaults to 0
            const [id] = await query.insert({
                model_id: data.model_id,
                name: data.name,
                description: data.description || null,
                protocol_id: data.protocol_id,
                config: typeof data.config === 'object' ? JSON.stringify(data.config) : (data.config || '{}'),
                polling_interval: data.polling_interval || 1000,
                timeout: data.timeout || 3000,
                is_public: data.is_public !== false ? 1 : 0,
                created_by: data.created_by || null,
                is_deleted: 0
            });
            return await this.findById(id, trx);
        } catch (error) {
            this.logger.error('TemplateDeviceRepository.create 오류:', error);
            throw error;
        }
    }

    /**
     * 템플릿 정보를 업데이트합니다.
     */
    async update(id, data, trx = null) {
        try {
            const updateData = {
                updated_at: this.knex.raw("datetime('now', 'localtime')")
            };

            const fields = [
                'model_id', 'name', 'description', 'protocol_id',
                'config', 'polling_interval', 'timeout', 'is_public'
            ];

            fields.forEach(field => {
                if (data[field] !== undefined) {
                    if (field === 'config' && typeof data[field] === 'object') {
                        updateData[field] = JSON.stringify(data[field]);
                    } else if (field === 'is_public') {
                        updateData[field] = data[field] ? 1 : 0;
                    } else {
                        updateData[field] = data[field];
                    }
                }
            });

            const query = trx ? trx(this.tableName) : this.query();
            const affected = await query.where('id', id).update(updateData);
            return affected > 0 ? await this.findById(id, trx) : null;
        } catch (error) {
            this.logger.error('TemplateDeviceRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 템플릿을 삭제합니다. (Soft Delete)
     */
    async deleteById(id) {
        try {
            this.logger.log(`🗑️ [TemplateDeviceRepository] Soft-deleting template ${id}...`);
            const affected = await this.query()
                .where('id', id)
                .update({
                    is_deleted: 1,
                    updated_at: this.knex.raw("datetime('now', 'localtime')")
                });

            return affected > 0;
        } catch (error) {
            this.logger.error('TemplateDeviceRepository.deleteById 오류:', error);
            throw error;
        }
    }
}

module.exports = TemplateDeviceRepository;
