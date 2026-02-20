const BaseRepository = require('./BaseRepository');

/**
 * ExportScheduleRepository class
 * Handles database operations for export schedules (periodic tasks).
 */
class ExportScheduleRepository extends BaseRepository {
    constructor() {
        super('export_schedules');
    }

    /**
     * 전체 스케줄 조회
     * @param {number} [tenantId] 테넌트 ID
     */
    async findAll(tenantId = null) {
        try {
            const query = this.query()
                .select('export_schedules.*', 'export_profiles.name as profile_name', 'export_targets.name as target_name')
                .leftJoin('export_profiles', 'export_schedules.profile_id', 'export_profiles.id')
                .leftJoin('export_targets', 'export_schedules.target_id', 'export_targets.id');

            if (tenantId) {
                query.where('export_schedules.tenant_id', tenantId);
            }

            return await query.orderBy('export_schedules.id', 'ASC');
        } catch (error) {
            this.logger.error('ExportScheduleRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 스케줄 조회
     * @param {number} id 스케줄 ID
     * @param {number} [tenantId] 테넌트 ID
     */
    async findById(id, tenantId = null) {
        try {
            const query = this.query()
                .select('export_schedules.*', 'export_profiles.name as profile_name', 'export_targets.name as target_name')
                .leftJoin('export_profiles', 'export_schedules.profile_id', 'export_profiles.id')
                .leftJoin('export_targets', 'export_schedules.target_id', 'export_targets.id')
                .where('export_schedules.id', id);

            if (tenantId) {
                query.where('export_schedules.tenant_id', tenantId);
            }

            return await query.first();
        } catch (error) {
            this.logger.error('ExportScheduleRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 활성화된 스케줄 조회
     */
    async findActive(tenantId = null) {
        const query = this.query().where('export_schedules.is_enabled', 1);
        if (tenantId) {
            query.where('export_schedules.tenant_id', tenantId);
        }
        return await query.orderBy('schedule_name', 'ASC');
    }

    /**
     * 스케줄 생성
     */
    async save(data, tenantId) {
        if (!tenantId && !data.tenant_id) {
            throw new Error('tenant_id is required for ExportSchedule');
        }

        try {
            const dataToInsert = {
                tenant_id: tenantId || data.tenant_id,
                profile_id: data.profile_id,
                target_id: data.target_id,
                schedule_name: data.schedule_name,
                description: data.description,
                cron_expression: data.cron_expression,
                timezone: data.timezone || 'UTC',
                data_range: data.data_range || 'day',
                lookback_periods: data.lookback_periods || 1,
                is_enabled: data.is_enabled !== undefined ? data.is_enabled : 1,
                next_run_at: this.knex.fn.now(),
                created_at: this.knex.fn.now(),
                updated_at: this.knex.fn.now()
            };

            const [id] = await this.knex(this.tableName).insert(dataToInsert);
            return await this.findById(id, tenantId || data.tenant_id);
        } catch (error) {
            this.logger.error('ExportScheduleRepository.save 오류:', error);
            throw error;
        }
    }

    /**
     * 스케줄 업데이트
     */
    async update(id, data, tenantId = null) {
        try {
            const dataToUpdate = {
                updated_at: this.knex.fn.now()
            };

            const allowedFields = [
                'profile_id', 'target_id', 'schedule_name', 'description',
                'cron_expression', 'timezone', 'data_range', 'lookback_periods',
                'is_enabled', 'last_run_at', 'last_status', 'next_run_at'
            ];

            allowedFields.forEach(field => {
                if (data[field] !== undefined) {
                    dataToUpdate[field] = data[field];
                }
            });

            const query = this.query().where('id', id);
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }

            const affected = await query.update(dataToUpdate);
            return affected > 0 ? await this.findById(id, tenantId) : null;
        } catch (error) {
            this.logger.error('ExportScheduleRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 스케줄 삭제
     */
    async deleteById(id, tenantId = null) {
        try {
            const query = this.query().where('id', id);
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }
            const affected = await query.delete();
            return affected > 0;
        } catch (error) {
            this.logger.error('ExportScheduleRepository.deleteById 오류:', error);
            throw error;
        }
    }
}

module.exports = ExportScheduleRepository;
