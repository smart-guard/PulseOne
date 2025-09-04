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
            console.log('AlarmOccurrenceRepository.findAll 호출:', filters);
            
            let query = AlarmQueries.AlarmOccurrence.FIND_ALL;
            const params = [];
            const conditions = [];

            // 테넌트 ID 필수
            params.push(filters.tenantId || 1);

            // 필터 조건 추가
            if (filters.state && filters.state !== 'all') {
                conditions.push('ao.state = ?');
                params.push(filters.state);
            }

            if (filters.severity && filters.severity !== 'all') {
                conditions.push('ao.severity = ?');
                params.push(filters.severity);
            }

            if (filters.ruleId) {
                conditions.push('ao.rule_id = ?');
                params.push(parseInt(filters.ruleId));
            }

            // device_id INTEGER 처리
            if (filters.deviceId) {
                const deviceId = parseInt(filters.deviceId);
                if (!isNaN(deviceId)) {
                    conditions.push('ao.device_id = ?');
                    params.push(deviceId);
                }
            }

            if (filters.acknowledged === true || filters.acknowledged === 'true') {
                conditions.push('ao.acknowledged_time IS NOT NULL');
            } else if (filters.acknowledged === false || filters.acknowledged === 'false') {
                conditions.push('ao.acknowledged_time IS NULL');
            }

            if (filters.category && filters.category !== 'all') {
                conditions.push('ao.category = ?');
                params.push(filters.category);
            }

            if (filters.tag) {
                conditions.push('ao.tags LIKE ?');
                params.push(`%${filters.tag}%`);
            }

            if (filters.dateFrom) {
                conditions.push('ao.occurrence_time >= ?');
                params.push(filters.dateFrom);
            }

            if (filters.dateTo) {
                conditions.push('ao.occurrence_time <= ?');
                params.push(filters.dateTo);
            }

            if (filters.search) {
                conditions.push('(ao.alarm_message LIKE ? OR ao.source_name LIKE ? OR ao.location LIKE ?)');
                const searchParam = `%${filters.search}%`;
                params.push(searchParam, searchParam, searchParam);
            }

            // 조건들을 쿼리에 추가
            if (conditions.length > 0) {
                query += ' AND ' + conditions.join(' AND ');
            }

            // 정렬 추가
            const sortBy = filters.sortBy || 'occurrence_time';
            const sortOrder = filters.sortOrder || 'DESC';
            query += ` ORDER BY ao.${sortBy} ${sortOrder}`;

            // 페이징 처리를 위한 변수
            const page = filters.page || 1;
            const limit = filters.limit || 50;
            const offset = (page - 1) * limit;

            // 전체 개수 먼저 조회 (페이징 정보를 위해)
            let countQuery = 'SELECT COUNT(*) as total FROM alarm_occurrences ao WHERE ao.tenant_id = ?';
            const countParams = [filters.tenantId || 1];
            
            if (conditions.length > 0) {
                // 동일한 조건 적용
                const conditionsForCount = [...conditions];
                countQuery += ' AND ' + conditionsForCount.join(' AND ');
                // params에서 첫번째(tenant_id) 제외하고 나머지 추가
                countParams.push(...params.slice(1));
            }
            
            const countResult = await this.executeQuery(countQuery, countParams);
            const total = countResult.length > 0 ? countResult[0].total : 0;

            // 페이징 적용한 실제 데이터 조회
            query += ' LIMIT ? OFFSET ?';
            params.push(limit, offset);

            const occurrences = await this.executeQuery(query, params);

            console.log(`✅ 알람 발생 ${occurrences.length}개 조회 완료 (전체: ${total}개)`);

            // 페이징 정보와 함께 반환
            return {
                items: occurrences.map(occurrence => this.parseAlarmOccurrence(occurrence)),
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
            console.error('AlarmOccurrenceRepository.findAll 실패:', error.message);
            throw error;
        }
    }

    async findById(id, tenantId = null) {
        try {
            console.log(`AlarmOccurrenceRepository.findById 호출: id=${id}, tenantId=${tenantId}`);
            
            const query = AlarmQueries.AlarmOccurrence.FIND_BY_ID;
            const params = [id, tenantId || 1];

            const occurrences = await this.executeQuery(query, params);
            
            if (occurrences.length === 0) {
                console.log(`알람 발생 ID ${id} 찾을 수 없음`);
                return null;
            }
            
            console.log(`✅ 알람 발생 ID ${id} 조회 성공`);
            
            return this.parseAlarmOccurrence(occurrences[0]);
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.findById 실패:', error.message);
            throw error;
        }
    }

    async findActive(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.FIND_ACTIVE;
            const params = [tenantId || 1];
            
            const occurrences = await this.executeQuery(query, params);
            
            return occurrences.map(occurrence => this.parseAlarmOccurrence(occurrence));
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.findActive 실패:', error.message);
            throw error;
        }
    }

    async findUnacknowledged(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.FIND_UNACKNOWLEDGED;
            const params = [tenantId || 1];
            
            const occurrences = await this.executeQuery(query, params);
            
            return occurrences.map(occurrence => this.parseAlarmOccurrence(occurrence));
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.findUnacknowledged 실패:', error.message);
            throw error;
        }
    }

    async findByDevice(deviceId, tenantId = null) {
        try {
            // device_id INTEGER 검증
            const validDeviceId = parseInt(deviceId);
            if (isNaN(validDeviceId)) {
                throw new Error('Invalid device_id');
            }

            const query = AlarmQueries.AlarmOccurrence.FIND_BY_DEVICE;
            const params = [tenantId || 1, validDeviceId];
            
            const occurrences = await this.executeQuery(query, params);
            
            return occurrences.map(occurrence => this.parseAlarmOccurrence(occurrence));
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.findByDevice 실패:', error.message);
            throw error;
        }
    }

    async findActiveByDevice(deviceId, tenantId = null) {
        try {
            // device_id INTEGER 검증
            const validDeviceId = parseInt(deviceId);
            if (isNaN(validDeviceId)) {
                throw new Error('Invalid device_id');
            }

            const query = AlarmQueries.AlarmOccurrence.FIND_ACTIVE_BY_DEVICE;
            const params = [tenantId || 1, validDeviceId];
            
            const occurrences = await this.executeQuery(query, params);
            
            return occurrences.map(occurrence => this.parseAlarmOccurrence(occurrence));
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.findActiveByDevice 실패:', error.message);
            throw error;
        }
    }

    async findByRule(ruleId, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.FIND_BY_RULE;
            const params = [ruleId, tenantId || 1];
            
            const occurrences = await this.executeQuery(query, params);
            
            return occurrences.map(occurrence => this.parseAlarmOccurrence(occurrence));
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.findByRule 실패:', error.message);
            throw error;
        }
    }

    async findByCategory(category, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.FIND_BY_CATEGORY;
            const params = [tenantId || 1, category];
            
            const occurrences = await this.executeQuery(query, params);
            
            return occurrences.map(occurrence => this.parseAlarmOccurrence(occurrence));
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.findByCategory 실패:', error.message);
            throw error;
        }
    }

    async findByTag(tag, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.FIND_BY_TAG;
            const params = [tenantId || 1, `%${tag}%`];
            
            const occurrences = await this.executeQuery(query, params);
            
            return occurrences.map(occurrence => this.parseAlarmOccurrence(occurrence));
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.findByTag 실패:', error.message);
            throw error;
        }
    }

    async findRecentOccurrences(limit = 50, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.RECENT_OCCURRENCES;
            const params = [tenantId || 1, limit];
            
            const occurrences = await this.executeQuery(query, params);
            
            return occurrences.map(occurrence => this.parseAlarmOccurrence(occurrence));
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.findRecentOccurrences 실패:', error.message);
            throw error;
        }
    }

    async findClearedByUser(userId, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.FIND_CLEARED_BY_USER;
            const params = [parseInt(userId), tenantId || 1];
            
            const occurrences = await this.executeQuery(query, params);
            
            return occurrences.map(occurrence => this.parseAlarmOccurrence(occurrence));
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.findClearedByUser 실패:', error.message);
            throw error;
        }
    }

    async findAcknowledgedByUser(userId, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.FIND_ACKNOWLEDGED_BY_USER;
            const params = [parseInt(userId), tenantId || 1];
            
            const occurrences = await this.executeQuery(query, params);
            
            return occurrences.map(occurrence => this.parseAlarmOccurrence(occurrence));
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.findAcknowledgedByUser 실패:', error.message);
            throw error;
        }
    }

    async findTodayAlarms(tenantId = null) {
        try {
            const { startDate, endDate } = AlarmQueries.getTodayDateRange();
            const query = AlarmQueries.AlarmOccurrence.FIND_TODAY_ALARMS;
            const params = [tenantId || 1, startDate, endDate];
            
            const occurrences = await this.executeQuery(query, params);
            
            return occurrences.map(occurrence => this.parseAlarmOccurrence(occurrence));
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.findTodayAlarms 실패:', error.message);
            throw error;
        }
    }

    async findByDateRange(startDate, endDate, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.FIND_BY_DATE_RANGE_WITH_FILTERS;
            const params = [tenantId || 1, startDate, endDate];
            
            const occurrences = await this.executeQuery(query, params);
            
            return occurrences.map(occurrence => this.parseAlarmOccurrence(occurrence));
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.findByDateRange 실패:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 생성, 수정, 삭제 연산
    // ==========================================================================

    async create(occurrenceData, userId = null) {
        try {
            console.log('AlarmOccurrenceRepository.create 호출:', occurrenceData);
            
            // 필수 필드 검증
            AlarmQueries.validateAlarmOccurrence(occurrenceData);
            
            const query = AlarmQueries.AlarmOccurrence.CREATE;
            const params = AlarmQueries.buildCreateOccurrenceParams({
                ...occurrenceData,
                created_at: new Date().toISOString(),
                updated_at: new Date().toISOString()
            });

            const result = await this.executeNonQuery(query, params);
            const insertId = result.lastInsertRowid || result.insertId || result.lastID;

            if (insertId) {
                console.log(`✅ 알람 발생 생성 완료 (ID: ${insertId})`);
                return await this.findById(insertId, occurrenceData.tenant_id);
            } else {
                throw new Error('Alarm occurrence creation failed - no ID returned');
            }
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.create 실패:', error.message);
            throw error;
        }
    }

    async updateState(id, state, tenantId = null) {
        try {
            console.log(`AlarmOccurrenceRepository.updateState 호출: ID ${id}, state=${state}`);

            const query = AlarmQueries.AlarmOccurrence.UPDATE_STATE;
            const params = [state, id, tenantId || 1];
            
            const result = await this.executeNonQuery(query, params);

            if (result.changes && result.changes > 0) {
                console.log(`✅ 알람 발생 ID ${id} 상태 업데이트 완료`);
                return await this.findById(id, tenantId);
            } else {
                throw new Error(`Alarm occurrence with ID ${id} not found or update failed`);
            }
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.updateState 실패:', error.message);
            throw error;
        }
    }

    async acknowledge(id, userId, comment = '', tenantId = null) {
        try {
            console.log(`AlarmOccurrenceRepository.acknowledge 호출: ID ${id}, userId=${userId}`);

            const query = AlarmQueries.AlarmOccurrence.ACKNOWLEDGE;
            const params = [userId, comment, id, tenantId || 1];
            
            const result = await this.executeNonQuery(query, params);

            if (result.changes && result.changes > 0) {
                console.log(`✅ 알람 ID ${id} 확인 처리 완료`);
                return await this.findById(id, tenantId);
            } else {
                throw new Error(`Alarm occurrence with ID ${id} not found`);
            }
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.acknowledge 실패:', error.message);
            throw error;
        }
    }

    async clear(id, userId, clearedValue, comment = '', tenantId = null) {
        try {
            console.log(`AlarmOccurrenceRepository.clear 호출: ID ${id}, userId=${userId}`);

            const query = AlarmQueries.AlarmOccurrence.CLEAR;
            const params = [
                clearedValue ? JSON.stringify(clearedValue) : null,
                comment,
                userId,  // cleared_by
                id,
                tenantId || 1
            ];
            
            const result = await this.executeNonQuery(query, params);

            if (result.changes && result.changes > 0) {
                console.log(`✅ 알람 ID ${id} 해제 처리 완료`);
                return await this.findById(id, tenantId);
            } else {
                throw new Error(`Alarm occurrence with ID ${id} not found`);
            }
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.clear 실패:', error.message);
            throw error;
        }
    }

    async delete(id) {
        try {
            console.log(`AlarmOccurrenceRepository.delete 호출: ID ${id}`);

            const query = AlarmQueries.AlarmOccurrence.DELETE;
            const params = [id];
            
            const result = await this.executeNonQuery(query, params);

            if (result.changes && result.changes > 0) {
                console.log(`✅ 알람 발생 ID ${id} 삭제 완료`);
                return true;
            } else {
                return false;
            }
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.delete 실패:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 통계 및 집계 연산
    // ==========================================================================

    async getStatsSummary(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.STATS_SUMMARY;
            const stats = await this.executeQuery(query, [tenantId || 1]);
            
            return stats.length > 0 ? stats[0] : {
                total_occurrences: 0,
                active_alarms: 0,
                unacknowledged_alarms: 0,
                acknowledged_alarms: 0,
                cleared_alarms: 0
            };
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.getStatsSummary 실패:', error.message);
            throw error;
        }
    }

    async getStatsBySeverity(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.STATS_BY_SEVERITY;
            const stats = await this.executeQuery(query, [tenantId || 1]);
            
            return stats;
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.getStatsBySeverity 실패:', error.message);
            throw error;
        }
    }

    async getStatsByCategory(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.STATS_BY_CATEGORY;
            const stats = await this.executeQuery(query, [tenantId || 1]);
            
            return stats;
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.getStatsByCategory 실패:', error.message);
            throw error;
        }
    }

    async getStatsByDevice(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.STATS_BY_DEVICE;
            const stats = await this.executeQuery(query, [tenantId || 1]);
            
            return stats.map(stat => ({
                ...stat,
                device_id: parseInt(stat.device_id),
                total_alarms: parseInt(stat.total_alarms),
                active_alarms: parseInt(stat.active_alarms)
            }));
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.getStatsByDevice 실패:', error.message);
            throw error;
        }
    }

    async getStatsToday(tenantId = null) {
        try {
            const { startDate, endDate } = AlarmQueries.getTodayDateRange();
            const query = AlarmQueries.AlarmOccurrence.STATS_TODAY;
            const stats = await this.executeQuery(query, [tenantId || 1, startDate, endDate]);
            
            return stats.length > 0 ? stats[0] : {
                today_total: 0,
                today_active: 0,
                today_unacknowledged: 0,
                today_critical: 0,
                today_major: 0,
                today_minor: 0,
                today_warning: 0
            };
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.getStatsToday 실패:', error.message);
            throw error;
        }
    }

    async getUserActionStats(userId, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.USER_ACTION_STATS;
            const params = [
                parseInt(userId), 
                parseInt(userId), 
                parseInt(userId), 
                parseInt(userId), 
                tenantId || 1
            ];
            
            const stats = await this.executeQuery(query, params);
            
            return stats.length > 0 ? stats[0] : {
                acknowledged_count: 0,
                cleared_count: 0,
                acknowledged_last_week: 0,
                cleared_last_week: 0
            };
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.getUserActionStats 실패:', error.message);
            throw error;
        }
    }

    async countAll(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.COUNT_ALL;
            const result = await this.executeQuery(query, [tenantId || 1]);
            
            return result.length > 0 ? result[0].count : 0;
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.countAll 실패:', error.message);
            throw error;
        }
    }

    async countActive(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.COUNT_ACTIVE;
            const result = await this.executeQuery(query, [tenantId || 1]);
            
            return result.length > 0 ? result[0].count : 0;
            
        } catch (error) {
            console.error('AlarmOccurrenceRepository.countActive 실패:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 추가 메서드들 - 기존 기능 유지
    // ==========================================================================

    async exists(id, tenantId = null) {
        try {
            const query = 'SELECT 1 FROM alarm_occurrences WHERE id = ? AND tenant_id = ? LIMIT 1';
            const params = [id, tenantId || 1];

            const result = await this.executeQuery(query, params);
            return result.length > 0;

        } catch (error) {
            console.error(`AlarmOccurrenceRepository.exists(${id}) 실패:`, error.message);
            return false;
        }
    }

    async countByConditions(conditions = {}) {
        try {
            let query = 'SELECT COUNT(*) as count FROM alarm_occurrences WHERE tenant_id = ?';
            const params = [conditions.tenantId || 1];

            if (conditions.state) {
                query += ' AND state = ?';
                params.push(conditions.state);
            }

            if (conditions.severity) {
                query += ' AND severity = ?';
                params.push(conditions.severity);
            }

            if (conditions.ruleId) {
                query += ' AND rule_id = ?';
                params.push(parseInt(conditions.ruleId));
            }

            if (conditions.deviceId) {
                const deviceId = parseInt(conditions.deviceId);
                if (!isNaN(deviceId)) {
                    query += ' AND device_id = ?';
                    params.push(deviceId);
                }
            }

            const result = await this.executeQuery(query, params);
            return result.length > 0 ? result[0].count : 0;

        } catch (error) {
            console.error('AlarmOccurrenceRepository.countByConditions 실패:', error.message);
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