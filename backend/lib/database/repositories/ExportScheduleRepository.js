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
     */
    async findAll() {
        try {
            return await this.query()
                .select('export_schedules.*', 'export_profiles.name as profile_name', 'export_targets.name as target_name')
                .leftJoin('export_profiles', 'export_schedules.profile_id', 'export_profiles.id')
                .leftJoin('export_targets', 'export_schedules.target_id', 'export_targets.id')
                .orderBy('export_schedules.id', 'ASC');
        } catch (error) {
            this.logger.error('ExportScheduleRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 스케줄 조회
     */
    async findById(id) {
        try {
            return await this.query()
                .select('export_schedules.*', 'export_profiles.name as profile_name', 'export_targets.name as target_name')
                .leftJoin('export_profiles', 'export_schedules.profile_id', 'export_profiles.id')
                .leftJoin('export_targets', 'export_schedules.target_id', 'export_targets.id')
                .where('export_schedules.id', id)
                .first();
        } catch (error) {
            this.logger.error('ExportScheduleRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 활성화된 스케줄 조회
     */
    async findActive() {
        return await this.query()
            .where({ is_enabled: 1 })
            .orderBy('schedule_name', 'ASC');
    }

    /**
     * 스케줄 생성
     */
    async save(data) {
        try {
            const dataToInsert = {
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
            return await this.findById(id);
        } catch (error) {
            this.logger.error('ExportScheduleRepository.save 오류:', error);
            throw error;
        }
    }

    /**
     * 스케줄 업데이트
     */
    async update(id, data) {
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

            const affected = await this.query().where('id', id).update(dataToUpdate);
            return affected > 0 ? await this.findById(id) : null;
        } catch (error) {
            this.logger.error('ExportScheduleRepository.update 오류:', error);
            throw error;
        }
    }
}

module.exports = ExportScheduleRepository;
