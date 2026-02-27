const BaseRepository = require('./BaseRepository');

/**
 * ManufacturerRepository class
 * Handles database operations for manufacturers.
 */
class ManufacturerRepository extends BaseRepository {
    constructor() {
        super('manufacturers');
    }

    /**
     * 모든 제조사를 조회합니다.
     */
    async findAll(options = {}) {
        try {
            // 1. 기본 쿼리 빌더 생성 (조건절 재사용을 위해 함수로 분리하거나 별도 처리)
            const buildQuery = (builder) => {
                if (options.isActive !== undefined) {
                    builder.where('is_active', options.isActive ? 1 : 0);
                }
                if (options.search) {
                    builder.where('name', 'like', `%${options.search}%`);
                }
                if (options.onlyDeleted) {
                    builder.where('is_deleted', 1);
                } else if (!options.includeDeleted) {
                    builder.where('is_deleted', 0);
                }
                return builder;
            };

            // 2. 전체 카운트 조회
            const countBuilder = this.query().count('* as count');
            buildQuery(countBuilder);

            const countResult = await countBuilder.first();
            const total = countResult ? (parseInt(countResult.count) || 0) : 0;

            // 3. 데이터 조회
            const query = this.query()
                .select('manufacturers.*')
                .select(this.knex.raw('(SELECT COUNT(*) FROM device_models WHERE device_models.manufacturer_id = manufacturers.id) as model_count'));
            buildQuery(query);

            const sortBy = options.sort_by || 'name';
            const sortOrder = (options.sort_order || 'ASC').toUpperCase();
            query.orderBy(sortBy, sortOrder);

            const limit = parseInt(options.limit) || 1000; // Limit to 1000 if not specified
            const page = Math.max(1, parseInt(options.page) || 1);
            const offset = (page - 1) * limit;

            query.limit(limit).offset(offset);

            const items = await query;

            return {
                items: items.map(item => this.parseManufacturer(item)),
                pagination: {
                    total_count: total,
                    current_page: page,
                    total_pages: Math.ceil(total / limit),
                    has_next: page * limit < total,
                    has_prev: page > 1
                }
            };
        } catch (error) {
            this.logger.error('ManufacturerRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 제조사를 조회합니다.
     */
    async findById(id) {
        try {
            const result = await this.query().where('id', id).first();
            return this.parseManufacturer(result);
        } catch (error) {
            this.logger.error('ManufacturerRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 이름으로 제조사를 조회합니다.
     */
    async findByName(name) {
        try {
            return await this.query().where('name', name).first();
        } catch (error) {
            this.logger.error('ManufacturerRepository.findByName 오류:', error);
            throw error;
        }
    }

    /**
     * 새로운 제조사를 생성합니다.
     */
    async create(data) {
        try {
            const [id] = await this.query().insert({
                name: data.name,
                description: data.description || null,
                country: data.country || null,
                website: data.website || null,
                logo_url: data.logo_url || null,
                is_active: data.is_active !== false ? 1 : 0
            });
            return await this.findById(id);
        } catch (error) {
            this.logger.error('ManufacturerRepository.create 오류:', error);
            throw error;
        }
    }

    /**
     * 제조사 정보를 업데이트합니다.
     */
    async update(id, data) {
        try {
            const updateData = {
                updated_at: this.knex.raw("datetime('now', 'localtime')")
            };

            const fields = ['name', 'description', 'country', 'website', 'logo_url', 'is_active'];
            fields.forEach(field => {
                if (data[field] !== undefined) {
                    updateData[field] = field === 'is_active' ? (data[field] ? 1 : 0) : data[field];
                }
            });

            const affected = await this.query().where('id', id).update(updateData);
            return affected > 0 ? await this.findById(id) : null;
        } catch (error) {
            this.logger.error('ManufacturerRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 제조사를 삭제합니다.
     */
    async deleteById(id) {
        try {
            // 연결된 모델이 있는지 확인
            const modelCountResult = await this.knex('device_models').where('manufacturer_id', id).count('* as count').first();
            const modelCount = modelCountResult ? (parseInt(modelCountResult.count) || 0) : 0;

            if (modelCount > 0) {
                throw new Error(`해당 제조사에 연결된 ${modelCount}개의 모델이 존재하여 삭제할 수 없습니다.`);
            }

            const affected = await this.query().where('id', id).update({
                is_deleted: 1,
                updated_at: this.knex.raw("datetime('now', 'localtime')")
            });
            return affected > 0;
        } catch (error) {
            this.logger.error('ManufacturerRepository.deleteById 오류:', error);
            throw error;
        }
    }

    /**
     * 제조사를 복구합니다.
     */
    async restoreById(id) {
        try {
            const affected = await this.query().where('id', id).update({
                is_deleted: 0,
                updated_at: this.knex.raw("datetime('now', 'localtime')")
            });
            return affected > 0;
        } catch (error) {
            this.logger.error('ManufacturerRepository.restoreById 오류:', error);
            throw error;
        }
    }

    /**
     * 데이터를 파싱하여 명확한 타입을 보장합니다.
     */
    parseManufacturer(item) {
        if (!item) return null;
        return {
            ...item,
            is_active: item.is_active === 1 || item.is_active === true,
            model_count: parseInt(item.model_count) || 0
        };
    }

    /**
     * 제조사 통계 정보를 조회합니다.
     */
    async getStatistics() {
        try {
            // 1. 전체 제조사 수 (삭제되지 않은 것만)
            const manufacturerCountResult = await this.query()
                .where('is_deleted', 0)
                .count('* as count')
                .first();
            const totalManufacturers = manufacturerCountResult ? (parseInt(manufacturerCountResult.count) || 0) : 0;

            // 2. 전체 모델 수 (삭제되지 않은 제조사의 모델만 - 추후 models에도 is_deleted가 있다면 필터 필요)
            const modelCountResult = await this.knex('device_models')
                .join('manufacturers', 'device_models.manufacturer_id', 'manufacturers.id')
                .where('manufacturers.is_deleted', 0)
                .count('device_models.id as count')
                .first();
            const totalModels = modelCountResult ? (parseInt(modelCountResult.count) || 0) : 0;

            // 3. 진출 국가 수 (삭제되지 않은 제조사 기준)
            const countryCountResult = await this.query()
                .where('is_deleted', 0)
                .countDistinct('country as count')
                .whereNotNull('country')
                .whereNot('country', '')
                .first();

            // SQLite용 fallback (countDistinct가 안될 경우)
            let totalCountries = countryCountResult ? (parseInt(countryCountResult.count) || 0) : 0;
            if (totalCountries === 0) {
                const distinctCountries = await this.query()
                    .where('is_deleted', 0)
                    .distinct('country')
                    .whereNotNull('country')
                    .whereNot('country', '');
                totalCountries = distinctCountries.length;
            }

            return {
                total_manufacturers: totalManufacturers,
                total_models: totalModels,
                total_countries: totalCountries
            };
        } catch (error) {
            this.logger.error('ManufacturerRepository.getStatistics 오류:', error);
            throw error;
        }
    }
}

module.exports = ManufacturerRepository;
