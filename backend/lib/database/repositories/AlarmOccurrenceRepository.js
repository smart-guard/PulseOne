// =============================================================================
// backend/lib/database/repositories/AlarmOccurrenceRepository.js
// ProtocolRepository 패턴과 100% 동일하게 구현
// =============================================================================

const BaseRepository = require('./BaseRepository');
const AlarmQueries = require('../queries/AlarmQueries');

class AlarmOccurrenceRepository extends BaseRepository {
    constructor() {
        // ProtocolRepository와 동일한 패턴: 매개변수 없는 생성자
        super('alarm_occurrences');
        console.log('🚨 AlarmOccurrenceRepository initialized with standard pattern');
    }

    // ==========================================================================
    // 기본 CRUD 연산 (AlarmQueries 사용)
    // ==========================================================================

    async findAll(filters = {}) {
        try {
            this.logger?.info('AlarmOccurrenceRepository.findAll called with:', filters);

            const query = this.query('ao')
                .leftJoin('alarm_rules as ar', 'ao.rule_id', 'ar.id')
                .leftJoin('devices as d', 'ao.device_id', 'd.id')
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('sites as s', 'd.site_id', 's.id')
                .leftJoin('data_points as dp', 'ao.point_id', 'dp.id')
                .leftJoin('virtual_points as vp', 'ao.point_id', 'vp.id')
                .leftJoin('users as u_ack', 'ao.acknowledged_by', 'u_ack.id')
                .leftJoin('users as u_clr', 'ao.cleared_by', 'u_clr.id')
                .leftJoin('tenants as t_ack', 'u_ack.tenant_id', 't_ack.id')
                .leftJoin('tenants as t_clr', 'u_clr.tenant_id', 't_clr.id')
                .select(
                    'ao.*',
                    'ar.name as rule_name',
                    'ar.severity as rule_severity',
                    'ar.target_type',
                    'ar.target_id',
                    'd.name as device_name',
                    'd.device_type',
                    'd.manufacturer',
                    'd.model',
                    'p.protocol_type',
                    'p.display_name as protocol_name',
                    'dp.name as data_point_name',
                    'dp.description as data_point_description',
                    'dp.unit',
                    'dp.data_type',
                    's.name as site_name',
                    's.location as site_location',
                    'vp.name as virtual_point_name',
                    'vp.description as virtual_point_description',
                    'u_ack.full_name as acknowledged_by_name',
                    'u_clr.full_name as cleared_by_name',
                    't_ack.company_name as acknowledged_by_company',
                    't_clr.company_name as cleared_by_company'
                );

            // 테넌트 격리
            query.where('ao.tenant_id', filters.tenantId || 1);

            // 필터 조건 추가
            if (filters.state && filters.state !== 'all') {
                let states = [];
                if (Array.isArray(filters.state)) {
                    states = filters.state;
                } else if (typeof filters.state === 'string') {
                    states = filters.state.split(',').map(s => s.trim());
                } else {
                    states = [filters.state];
                }

                // 대소문자 모두 포함하도록 확장
                const expandedStates = [];
                states.forEach(s => {
                    const ls = s.toLowerCase();
                    expandedStates.push(ls);
                    expandedStates.push(ls.toUpperCase());
                });
                query.whereIn('ao.state', expandedStates);
            }

            if (filters.severity && filters.severity !== 'all') {
                const sev = filters.severity.toLowerCase();
                query.whereIn('ao.severity', [sev, sev.toUpperCase()]);
            }

            if (filters.ruleId) {
                query.where('ao.rule_id', parseInt(filters.ruleId));
            }

            if (filters.deviceId) {
                const deviceId = parseInt(filters.deviceId);
                if (!isNaN(deviceId)) {
                    query.where('ao.device_id', deviceId);
                }
            }

            if (filters.acknowledged === true || filters.acknowledged === 'true') {
                query.whereNotNull('ao.acknowledged_time');
            } else if (filters.acknowledged === false || filters.acknowledged === 'false') {
                query.whereNull('ao.acknowledged_time');
            }

            if (filters.category && filters.category !== 'all') {
                query.where('ao.category', filters.category);
            }

            if (filters.tag) {
                query.where('ao.tags', 'like', `%${filters.tag}%`);
            }

            if (filters.dateFrom) {
                query.where('ao.occurrence_time', '>=', filters.dateFrom);
            }

            if (filters.dateTo) {
                query.where('ao.occurrence_time', '<=', filters.dateTo);
            }

            if (filters.search) {
                query.where(builder => {
                    builder.where('ao.alarm_message', 'like', `%${filters.search}%`)
                        .orWhere('ao.source_name', 'like', `%${filters.search}%`)
                        .orWhere('ao.location', 'like', `%${filters.search}%`);
                });
            }

            // 페이징 및 정렬 정보
            const sortBy = filters.sortBy || 'occurrence_time';
            const sortOrder = filters.sortOrder || 'DESC';
            const page = parseInt(filters.page) || 1;
            const limit = parseInt(filters.limit) || 50;
            const offset = (page - 1) * limit;

            // 전체 개수와 데이터 동시 조회
            const [items, countResult] = await Promise.all([
                query.clone().orderBy(`ao.${sortBy}`, sortOrder).limit(limit).offset(offset),
                query.clone().clearSelect().clearOrder().count('* as total').first()
            ]);

            const total = countResult ? countResult.total : 0;

            this.logger?.info(`✅ Found ${items.length} occurrences (Total: ${total})`);

            return {
                items: (items || []).map(occurrence => this.parseAlarmOccurrence(occurrence)),
                pagination: {
                    page: page,
                    limit: limit,
                    total: total,
                    totalPages: Math.ceil(total / limit),
                    hasNext: page < Math.ceil(total / limit),
                    hasPrev: page > 1
                }
            };

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findAll failed:', error.message);
            return {
                items: [],
                pagination: {
                    page: filters.page || 1,
                    limit: filters.limit || 50,
                    total: 0,
                    totalPages: 0,
                    hasNext: false,
                    hasPrev: false
                }
            };
        }
    }

    async findById(id, tenantId = null) {
        try {
            this.logger?.info(`AlarmOccurrenceRepository.findById called: id=${id}, tenantId=${tenantId}`);

            const item = await this.query('ao')
                .leftJoin('alarm_rules as ar', 'ao.rule_id', 'ar.id')
                .leftJoin('devices as d', 'ao.device_id', 'd.id')
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('sites as s', 'd.site_id', 's.id')
                .leftJoin('data_points as dp', 'ao.point_id', 'dp.id')
                .leftJoin('virtual_points as vp', 'ao.point_id', 'vp.id')
                .select(
                    'ao.*',
                    'ar.name as rule_name',
                    'ar.severity as rule_severity',
                    'ar.target_type',
                    'ar.target_id',
                    'd.name as device_name',
                    'd.device_type',
                    'p.protocol_type',
                    'p.display_name as protocol_name',
                    'dp.name as data_point_name',
                    'dp.description as data_point_description',
                    's.name as site_name',
                    's.location as site_location',
                    'vp.name as virtual_point_name'
                )
                .where('ao.id', id)
                .where('ao.tenant_id', tenantId || 1)
                .first();

            if (!item) {
                this.logger?.info(`Occurrence ID ${id} not found`);
                return null;
            }

            return this.parseAlarmOccurrence(item);

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findById failed:', error.message);
            throw error;
        }
    }

    async findActive(tenantId = null) {
        try {
            const items = await this.query('ao')
                .leftJoin('alarm_rules as ar', 'ao.rule_id', 'ar.id')
                .leftJoin('devices as d', 'ao.device_id', 'd.id')
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('data_points as dp', 'ao.point_id', 'dp.id')
                .leftJoin('sites as s', 'd.site_id', 's.id')
                .leftJoin('virtual_points as vp', 'ao.point_id', 'vp.id')
                .select(
                    'ao.*',
                    'ar.name as rule_name',
                    'ar.severity as rule_severity',
                    'd.name as device_name',
                    'p.protocol_type',
                    'dp.name as data_point_name',
                    's.location as site_location',
                    'vp.name as virtual_point_name'
                )
                .where('ao.tenant_id', tenantId || 1)
                .where('ao.state', 'active')
                .orderBy('ao.occurrence_time', 'desc');

            return (items || []).map(item => this.parseAlarmOccurrence(item));

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findActive failed:', error.message);
            throw error;
        }
    }

    /**
     * Dashboard 전용: Raw SQL을 사용한 활성 알람 조회
     */
    async findActivePlainSQL(tenantId = null, limit = 5) {
        try {
            const sql = AlarmQueries.AlarmOccurrence.FIND_ACTIVE;
            const items = await this.executeQuery(sql + ` LIMIT ${limit}`, [tenantId || 1]);
            return (items || []).map(item => this.parseAlarmOccurrence(item));
        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findActivePlainSQL failed:', error.message);
            return [];
        }
    }

    async findUnacknowledged(tenantId = null) {
        try {
            const items = await this.query('ao')
                .leftJoin('alarm_rules as ar', 'ao.rule_id', 'ar.id')
                .leftJoin('devices as d', 'ao.device_id', 'd.id')
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('data_points as dp', 'ao.point_id', 'dp.id')
                .leftJoin('sites as s', 'd.site_id', 's.id')
                .select(
                    'ao.*',
                    'ar.name as rule_name',
                    'd.name as device_name',
                    'p.protocol_type',
                    'dp.name as data_point_name',
                    's.location as site_location'
                )
                .where('ao.tenant_id', tenantId || 1)
                .whereNull('ao.acknowledged_time')
                .orderBy('ao.occurrence_time', 'desc');

            return (items || []).map(item => this.parseAlarmOccurrence(item));

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findUnacknowledged failed:', error.message);
            throw error;
        }
    }

    async findByDevice(deviceId, tenantId = null) {
        try {
            const validDeviceId = parseInt(deviceId);
            if (isNaN(validDeviceId)) {
                throw new Error('Invalid device_id');
            }

            const items = await this.query('ao')
                .leftJoin('alarm_rules as ar', 'ao.rule_id', 'ar.id')
                .leftJoin('devices as d', 'ao.device_id', 'd.id')
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('data_points as dp', 'ao.point_id', 'dp.id')
                .leftJoin('sites as s', 'd.site_id', 's.id')
                .select(
                    'ao.*',
                    'ar.name as rule_name',
                    'd.name as device_name',
                    'p.protocol_type',
                    'dp.name as data_point_name',
                    's.location as site_location'
                )
                .where('ao.tenant_id', tenantId || 1)
                .where('ao.device_id', validDeviceId)
                .orderBy('ao.occurrence_time', 'desc');

            return (items || []).map(item => this.parseAlarmOccurrence(item));

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findByDevice failed:', error.message);
            throw error;
        }
    }

    async findActiveByDevice(deviceId, tenantId = null) {
        try {
            const validDeviceId = parseInt(deviceId);
            if (isNaN(validDeviceId)) {
                throw new Error('Invalid device_id');
            }

            const items = await this.query('ao')
                .leftJoin('alarm_rules as ar', 'ao.rule_id', 'ar.id')
                .select(
                    'ao.*',
                    'ar.name as rule_name',
                    'ar.description as rule_description'
                )
                .where('ao.tenant_id', tenantId || 1)
                .where('ao.device_id', validDeviceId)
                .where('ao.state', 'active')
                .orderBy('ao.occurrence_time', 'desc');

            return (items || []).map(item => this.parseAlarmOccurrence(item));

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findActiveByDevice failed:', error.message);
            throw error;
        }
    }

    async findByRule(ruleId, tenantId = null) {
        try {
            const items = await this.query()
                .where('rule_id', ruleId)
                .where('tenant_id', tenantId || 1)
                .orderBy('occurrence_time', 'desc');

            return (items || []).map(item => this.parseAlarmOccurrence(item));

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findByRule failed:', error.message);
            throw error;
        }
    }

    async findByCategory(category, tenantId = null) {
        try {
            const items = await this.query('ao')
                .leftJoin('alarm_rules as ar', 'ao.rule_id', 'ar.id')
                .leftJoin('devices as d', 'ao.device_id', 'd.id')
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('data_points as dp', 'ao.point_id', 'dp.id')
                .select(
                    'ao.*',
                    'ar.name as rule_name',
                    'ar.severity as rule_severity',
                    'd.name as device_name',
                    'p.protocol_type',
                    'dp.name as data_point_name'
                )
                .where('ao.tenant_id', tenantId || 1)
                .where('ao.category', category)
                .orderBy('ao.occurrence_time', 'desc');

            return (items || []).map(item => this.parseAlarmOccurrence(item));

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findByCategory failed:', error.message);
            throw error;
        }
    }

    async findByTag(tag, tenantId = null) {
        try {
            const items = await this.query('ao')
                .leftJoin('alarm_rules as ar', 'ao.rule_id', 'ar.id')
                .leftJoin('devices as d', 'ao.device_id', 'd.id')
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('data_points as dp', 'ao.point_id', 'dp.id')
                .select(
                    'ao.*',
                    'ar.name as rule_name',
                    'ar.severity as rule_severity',
                    'd.name as device_name',
                    'p.protocol_type',
                    'dp.name as data_point_name'
                )
                .where('ao.tenant_id', tenantId || 1)
                .where('ao.tags', 'like', `%${tag}%`)
                .orderBy('ao.occurrence_time', 'desc');

            return (items || []).map(item => this.parseAlarmOccurrence(item));

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findByTag failed:', error.message);
            throw error;
        }
    }

    async findRecentOccurrences(limit = 50, tenantId = null) {
        try {
            const items = await this.query('ao')
                .leftJoin('alarm_rules as ar', 'ao.rule_id', 'ar.id')
                .leftJoin('devices as d', 'ao.device_id', 'd.id')
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('data_points as dp', 'ao.point_id', 'dp.id')
                .leftJoin('sites as s', 'd.site_id', 's.id')
                .select(
                    'ao.*',
                    'ar.name as rule_name',
                    'd.name as device_name',
                    'p.protocol_type',
                    'dp.name as data_point_name',
                    's.location as site_location'
                )
                .where('ao.tenant_id', tenantId || 1)
                .orderBy('ao.occurrence_time', 'desc')
                .limit(limit);

            return (items || []).map(item => this.parseAlarmOccurrence(item));

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findRecentOccurrences failed:', error.message);
            throw error;
        }
    }

    /**
     * Dashboard 전용 별칭: findRecentAlarms
     */
    async findRecentAlarms(tenantId = null, limit = 5) {
        return await this.findRecentOccurrences(limit, tenantId);
    }

    async findClearedByUser(userId, tenantId = null) {
        try {
            const items = await this.query('ao')
                .leftJoin('alarm_rules as ar', 'ao.rule_id', 'ar.id')
                .leftJoin('devices as d', 'ao.device_id', 'd.id')
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('data_points as dp', 'ao.point_id', 'dp.id')
                .leftJoin('sites as s', 'd.site_id', 's.id')
                .select(
                    'ao.*',
                    'ar.name as rule_name',
                    'd.name as device_name',
                    'p.protocol_type',
                    'dp.name as data_point_name',
                    's.location as site_location'
                )
                .where('ao.cleared_by', parseInt(userId))
                .where('ao.tenant_id', tenantId || 1)
                .orderBy('ao.cleared_time', 'desc');

            return (items || []).map(item => this.parseAlarmOccurrence(item));

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findClearedByUser failed:', error.message);
            throw error;
        }
    }

    async findAcknowledgedByUser(userId, tenantId = null) {
        try {
            const items = await this.query('ao')
                .leftJoin('alarm_rules as ar', 'ao.rule_id', 'ar.id')
                .leftJoin('devices as d', 'ao.device_id', 'd.id')
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('data_points as dp', 'ao.point_id', 'dp.id')
                .leftJoin('sites as s', 'd.site_id', 's.id')
                .select(
                    'ao.*',
                    'ar.name as rule_name',
                    'd.name as device_name',
                    'p.protocol_type',
                    'dp.name as data_point_name',
                    's.location as site_location'
                )
                .where('ao.acknowledged_by', parseInt(userId))
                .where('ao.tenant_id', tenantId || 1)
                .orderBy('ao.acknowledged_time', 'desc');

            return (items || []).map(item => this.parseAlarmOccurrence(item));

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findAcknowledgedByUser failed:', error.message);
            throw error;
        }
    }

    async findTodayAlarms(tenantId = null) {
        try {
            const { startDate, endDate } = AlarmQueries.getTodayDateRange();
            const items = await this.query('ao')
                .leftJoin('alarm_rules as ar', 'ao.rule_id', 'ar.id')
                .leftJoin('devices as d', 'ao.device_id', 'd.id')
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('data_points as dp', 'ao.point_id', 'dp.id')
                .leftJoin('sites as s', 'd.site_id', 's.id')
                .select(
                    'ao.*',
                    'ar.name as rule_name',
                    'd.name as device_name',
                    'p.protocol_type',
                    'dp.name as data_point_name',
                    's.location as site_location'
                )
                .where('ao.tenant_id', tenantId || 1)
                .where('ao.occurrence_time', '>=', startDate)
                .where('ao.occurrence_time', '<', endDate)
                .orderBy('ao.occurrence_time', 'desc');

            return (items || []).map(item => this.parseAlarmOccurrence(item));

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findTodayAlarms failed:', error.message);
            throw error;
        }
    }

    async findByDateRange(startDate, endDate, tenantId = null) {
        try {
            const items = await this.query('ao')
                .leftJoin('alarm_rules as ar', 'ao.rule_id', 'ar.id')
                .leftJoin('devices as d', 'ao.device_id', 'd.id')
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('data_points as dp', 'ao.point_id', 'dp.id')
                .leftJoin('sites as s', 'd.site_id', 's.id')
                .select(
                    'ao.*',
                    'ar.name as rule_name',
                    'ar.severity as rule_severity',
                    'd.name as device_name',
                    'p.protocol_type',
                    'dp.name as data_point_name',
                    's.location as site_location'
                )
                .where('ao.tenant_id', tenantId || 1)
                .where('ao.occurrence_time', '>=', startDate)
                .where('ao.occurrence_time', '<=', endDate)
                .orderBy('ao.occurrence_time', 'desc');

            return (items || []).map(item => this.parseAlarmOccurrence(item));

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.findByDateRange failed:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 생성, 수정, 삭제 연산
    // ==========================================================================

    async create(occurrenceData, userId = null) {
        try {
            this.logger?.info('AlarmOccurrenceRepository.create called:', occurrenceData);

            // 필수 필드 검증 (유지)
            AlarmQueries.validateAlarmOccurrence(occurrenceData);

            const data = {
                ...occurrenceData,
                created_at: new Date().toISOString(),
                updated_at: new Date().toISOString()
            };

            // JSON 필드 처리
            if (data.trigger_value && typeof data.trigger_value !== 'string') data.trigger_value = JSON.stringify(data.trigger_value);
            if (data.cleared_value && typeof data.cleared_value !== 'string') data.cleared_value = JSON.stringify(data.cleared_value);
            if (data.context_data && typeof data.context_data !== 'string') data.context_data = JSON.stringify(data.context_data);
            if (data.notification_result && typeof data.notification_result !== 'string') data.notification_result = JSON.stringify(data.notification_result);
            if (data.tags && typeof data.tags !== 'string') data.tags = JSON.stringify(data.tags);

            const [id] = await this.knex(this.tableName).insert(data);

            if (id) {
                this.logger?.info(`✅ Occurrence created (ID: ${id})`);
                return await this.findById(id, occurrenceData.tenant_id);
            } else {
                throw new Error('Alarm occurrence creation failed - no ID returned');
            }

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.create failed:', error.message);
            throw error;
        }
    }

    async updateState(id, state, tenantId = null) {
        try {
            this.logger?.info(`AlarmOccurrenceRepository.updateState called: ID ${id}, state=${state}`);

            const result = await this.knex(this.tableName)
                .where('id', id)
                .where('tenant_id', tenantId || 1)
                .update({
                    state,
                    updated_at: this.knex.fn.now()
                });

            if (result > 0) {
                this.logger?.info(`✅ Occurrence ID ${id} state updated`);
                return await this.findById(id, tenantId);
            } else {
                throw new Error(`Alarm occurrence with ID ${id} not found or update failed`);
            }

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.updateState failed:', error.message);
            throw error;
        }
    }

    async acknowledge(id, userId, comment = '', tenantId = null) {
        try {
            this.logger?.info(`AlarmOccurrenceRepository.acknowledge called: ID ${id}, userId=${userId}`);

            const result = await this.knex(this.tableName)
                .where('id', id)
                .where('tenant_id', tenantId || 1)
                .update({
                    acknowledged_time: this.knex.fn.now(),
                    acknowledged_by: userId,
                    acknowledge_comment: comment,
                    state: 'acknowledged',
                    updated_at: this.knex.fn.now()
                });

            if (result > 0) {
                this.logger?.info(`✅ Alarm ID ${id} acknowledged`);
                return await this.findById(id, tenantId);
            } else {
                throw new Error(`Alarm occurrence with ID ${id} not found`);
            }

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.acknowledge failed:', error.message);
            throw error;
        }
    }

    async clear(id, userId, clearedValue, comment = '', tenantId = null) {
        try {
            this.logger?.info(`AlarmOccurrenceRepository.clear called: ID ${id}, userId=${userId}`);

            const result = await this.knex(this.tableName)
                .where('id', id)
                .where('tenant_id', tenantId || 1)
                .update({
                    cleared_time: this.knex.fn.now(),
                    cleared_value: clearedValue ? JSON.stringify(clearedValue) : null,
                    clear_comment: comment,
                    cleared_by: userId,
                    state: 'cleared',
                    updated_at: this.knex.fn.now()
                });

            if (result > 0) {
                this.logger?.info(`✅ Alarm ID ${id} cleared`);
                return await this.findById(id, tenantId);
            } else {
                throw new Error(`Alarm occurrence with ID ${id} not found`);
            }

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.clear failed:', error.message);
            throw error;
        }
    }

    async delete(id) {
        try {
            this.logger?.info(`AlarmOccurrenceRepository.delete called: ID ${id}`);

            const result = await this.knex(this.tableName)
                .where('id', id)
                .del();

            if (result > 0) {
                this.logger?.info(`✅ Occurrence ID ${id} deleted`);
                return true;
            } else {
                return false;
            }

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.delete failed:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 통계 및 집계 연산
    // ==========================================================================

    async getStatsSummary(tenantId = null) {
        try {
            const result = await this.knex(this.tableName)
                .where('tenant_id', tenantId || 1)
                .select([
                    this.knex.raw('COUNT(*) as total_occurrences'),
                    this.knex.raw("SUM(CASE WHEN LOWER(state) IN ('active', 'acknowledged') THEN 1 ELSE 0 END) as active_alarms"),
                    this.knex.raw("SUM(CASE WHEN acknowledged_time IS NULL AND LOWER(state) = 'active' THEN 1 ELSE 0 END) as unacknowledged_alarms"),
                    this.knex.raw("SUM(CASE WHEN LOWER(state) = 'acknowledged' THEN 1 ELSE 0 END) as acknowledged_alarms"),
                    this.knex.raw("SUM(CASE WHEN LOWER(state) = 'cleared' THEN 1 ELSE 0 END) as cleared_alarms")
                ])
                .first();

            return result || {
                total_occurrences: 0,
                active_alarms: 0,
                unacknowledged_alarms: 0,
                acknowledged_alarms: 0,
                cleared_alarms: 0
            };

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.getStatsSummary failed:', error.message);
            throw error;
        }
    }

    async getStatsBySeverity(tenantId = null) {
        try {
            const stats = await this.knex(this.tableName)
                .where('tenant_id', tenantId || 1)
                .select('severity')
                .count('* as count')
                .select(this.knex.raw("SUM(CASE WHEN state = 'active' THEN 1 ELSE 0 END) as active_count"))
                .groupBy('severity')
                .orderByRaw(`
                    CASE severity 
                        WHEN 'critical' THEN 1 
                        WHEN 'high' THEN 2 
                        WHEN 'medium' THEN 3 
                        WHEN 'low' THEN 4 
                        ELSE 5 
                    END
                `);

            return stats;

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.getStatsBySeverity failed:', error.message);
            throw error;
        }
    }

    async getStatsByCategory(tenantId = null) {
        try {
            const stats = await this.knex(this.tableName)
                .where('tenant_id', tenantId || 1)
                .whereNotNull('category')
                .select('category')
                .count('* as count')
                .groupBy('category')
                .orderBy('count', 'desc');

            return stats;

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.getStatsByCategory failed:', error.message);
            throw error;
        }
    }

    async getStatsByDevice(tenantId = null) {
        try {
            const stats = await this.knex(this.tableName)
                .where('tenant_id', tenantId || 1)
                .whereNotNull('device_id')
                .select('device_id')
                .count('* as total_alarms')
                .select(this.knex.raw("SUM(CASE WHEN state = 'active' THEN 1 ELSE 0 END) as active_alarms"))
                .groupBy('device_id')
                .orderBy('total_alarms', 'desc');

            return (stats || []).map(stat => ({
                ...stat,
                device_id: parseInt(stat.device_id),
                total_alarms: parseInt(stat.total_alarms),
                active_alarms: parseInt(stat.active_alarms)
            }));

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.getStatsByDevice failed:', error.message);
            throw error;
        }
    }

    async getStatsToday(tenantId = null) {
        try {
            const { startDate, endDate } = AlarmQueries.getTodayDateRange();
            const result = await this.knex(this.tableName)
                .where('tenant_id', tenantId || 1)
                .where('occurrence_time', '>=', startDate)
                .where('occurrence_time', '<', endDate)
                .select([
                    this.knex.raw('COUNT(*) as today_total'),
                    this.knex.raw("SUM(CASE WHEN state = 'active' THEN 1 ELSE 0 END) as today_active"),
                    this.knex.raw('SUM(CASE WHEN acknowledged_time IS NULL THEN 1 ELSE 0 END) as today_unacknowledged'),
                    this.knex.raw("SUM(CASE WHEN severity = 'critical' THEN 1 ELSE 0 END) as today_critical"),
                    this.knex.raw("SUM(CASE WHEN severity IN ('major', 'high') THEN 1 ELSE 0 END) as today_major"),
                    this.knex.raw("SUM(CASE WHEN severity IN ('minor', 'low') THEN 1 ELSE 0 END) as today_minor"),
                    this.knex.raw("SUM(CASE WHEN severity IN ('medium', 'warning') THEN 1 ELSE 0 END) as today_warning")
                ])
                .first();

            return result || {
                today_total: 0,
                today_active: 0,
                today_unacknowledged: 0,
                today_critical: 0,
                today_major: 0,
                today_minor: 0,
                today_warning: 0
            };

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.getStatsToday failed:', error.message);
            throw error;
        }
    }

    async getUserActionStats(userId, tenantId = null) {
        try {
            const uId = parseInt(userId);
            const result = await this.knex(this.tableName)
                .where('tenant_id', tenantId || 1)
                .select([
                    this.knex.raw('COUNT(CASE WHEN acknowledged_by = ? THEN 1 END) as acknowledged_count', [uId]),
                    this.knex.raw('COUNT(CASE WHEN cleared_by = ? THEN 1 END) as cleared_count', [uId]),
                    this.knex.raw("COUNT(CASE WHEN acknowledged_by = ? AND acknowledged_time >= datetime('now', '-7 days') THEN 1 END) as acknowledged_last_week", [uId]),
                    this.knex.raw("COUNT(CASE WHEN cleared_by = ? AND cleared_time >= datetime('now', '-7 days') THEN 1 END) as cleared_last_week", [uId])
                ])
                .first();

            return result || {
                acknowledged_count: 0,
                cleared_count: 0,
                acknowledged_last_week: 0,
                cleared_last_week: 0
            };

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.getUserActionStats failed:', error.message);
            throw error;
        }
    }

    async countAll(tenantId = null) {
        try {
            const result = await this.knex(this.tableName)
                .where('tenant_id', tenantId || 1)
                .count('* as count')
                .first();

            return result ? result.count : 0;

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.countAll failed:', error.message);
            throw error;
        }
    }

    async countActive(tenantId = null) {
        try {
            const result = await this.knex(this.tableName)
                .where('tenant_id', tenantId || 1)
                .where('state', 'active')
                .count('* as count')
                .first();

            return result ? result.count : 0;

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.countActive failed:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 추가 메서드들 - 기존 기능 유지
    // ==========================================================================

    async exists(id, tenantId = null) {
        try {
            const result = await this.knex(this.tableName)
                .where('id', id)
                .where('tenant_id', tenantId || 1)
                .select(1)
                .first();
            return !!result;

        } catch (error) {
            this.logger?.error(`AlarmOccurrenceRepository.exists(${id}) failed:`, error.message);
            return false;
        }
    }

    async countByConditions(conditions = {}) {
        try {
            const query = this.knex(this.tableName)
                .where('tenant_id', conditions.tenantId || 1);

            if (conditions.state) {
                query.where('state', conditions.state);
            }

            if (conditions.severity) {
                query.where('severity', conditions.severity);
            }

            if (conditions.ruleId) {
                query.where('rule_id', parseInt(conditions.ruleId));
            }

            if (conditions.deviceId) {
                const deviceId = parseInt(conditions.deviceId);
                if (!isNaN(deviceId)) {
                    query.where('device_id', deviceId);
                }
            }

            const result = await query.count('* as count').first();
            return result ? result.count : 0;

        } catch (error) {
            this.logger?.error('AlarmOccurrenceRepository.countByConditions failed:', error.message);
            return 0;
        }
    }

    // ==========================================================================
    // 캐시 관련 메서드들
    // ==========================================================================

    getFromCache(key) {
        // BaseRepository에서 캐시 기능이 제공되는 경우 사용
        // 현재는 캐시 미사용으로 null 반환
        return null;
    }

    setCache(key, data, ttl = null) {
        // BaseRepository에서 캐시 기능이 제공되는 경우 사용
        // 현재는 캐시 미사용
    }

    clearCache() {
        // BaseRepository에서 캐시 기능이 제공되는 경우 사용
        // 현재는 캐시 미사용
    }

    // ==========================================================================
    // 유틸리티 메소드들 - ProtocolRepository 패턴과 동일
    // ==========================================================================

    parseAlarmOccurrence(occurrence) {
        if (!occurrence) return null;

        return {
            ...occurrence,
            // INTEGER 필드 변환
            id: parseInt(occurrence.id),
            rule_id: parseInt(occurrence.rule_id),
            tenant_id: parseInt(occurrence.tenant_id),
            device_id: occurrence.device_id ? parseInt(occurrence.device_id) : null,
            point_id: occurrence.point_id ? parseInt(occurrence.point_id) : null,
            acknowledged_by: occurrence.acknowledged_by ? parseInt(occurrence.acknowledged_by) : null,
            cleared_by: occurrence.cleared_by ? parseInt(occurrence.cleared_by) : null,
            notification_sent: parseInt(occurrence.notification_sent) || 0,
            notification_count: parseInt(occurrence.notification_count) || 0,

            // Boolean 필드 변환
            is_acknowledged: Boolean(occurrence.acknowledged_time),
            is_cleared: Boolean(occurrence.cleared_time),
            is_active: occurrence.state === 'active',

            // JSON 필드 파싱
            trigger_value: this.parseJsonField(occurrence.trigger_value),
            cleared_value: this.parseJsonField(occurrence.cleared_value),
            context_data: this.parseJsonField(occurrence.context_data),
            notification_result: this.parseJsonField(occurrence.notification_result),
            tags: this.parseJsonField(occurrence.tags) || []
        };
    }

    parseJsonField(jsonStr) {
        if (!jsonStr) return null;
        try {
            return typeof jsonStr === 'string' ? JSON.parse(jsonStr) : jsonStr;
        } catch (error) {
            console.warn('JSON 파싱 실패:', error.message);
            return null;
        }
    }
}

module.exports = AlarmOccurrenceRepository;