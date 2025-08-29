// ============================================================================
// backend/lib/database/repositories/AlarmOccurrenceRepository.js
// BaseRepository 상속받은 AlarmOccurrenceRepository - device_id INTEGER 완전 수정본
// ============================================================================

const BaseRepository = require('./BaseRepository');
const AlarmQueries = require('../queries/AlarmQueries');

/**
 * 알람 발생 Repository 클래스 - device_id INTEGER 타입 완전 처리
 * BaseRepository를 상속받아 공통 기능 활용
 * 
 * 수정사항:
 * - device_id: TEXT → INTEGER 완전 변경
 * - AlarmQueries.js 쿼리 분리 패턴 적용
 * - 실제 alarm_occurrences 스키마에 맞춘 필드 정의
 * - INTEGER 타입 검증 및 변환 로직 추가
 */
class AlarmOccurrenceRepository extends BaseRepository {
    constructor() {
        super('alarm_occurrences');
        
        // 실제 alarm_occurrences 테이블 필드 정의 - cleared_by 필드 추가
        this.fields = {
            id: 'autoIncrement',
            rule_id: 'int',
            tenant_id: 'int',
            occurrence_time: 'timestamp',
            trigger_value: 'text',              
            trigger_condition: 'text',
            alarm_message: 'text',
            severity: 'varchar(20)',
            state: 'varchar(20)',               
            
            // 확인(Acknowledge) 정보
            acknowledged_time: 'timestamp',
            acknowledged_by: 'int',
            acknowledge_comment: 'text',
            
            // 해제(Clear) 정보 - cleared_by 필드 추가!
            cleared_time: 'timestamp',
            cleared_value: 'text',              
            clear_comment: 'text',
            cleared_by: 'int',                  // ⭐ 누락된 필드 추가!
            
            // 알림 정보
            notification_sent: 'int',
            notification_time: 'timestamp',
            notification_count: 'int',
            notification_result: 'text',        
            
            // 메타데이터
            context_data: 'text',               
            source_name: 'varchar(100)',
            location: 'varchar(200)',
            
            // 디바이스/포인트 정보
            device_id: 'int',                   
            point_id: 'int',
            category: 'varchar(50)',
            tags: 'text',
            
            // 감사 정보
            created_at: 'timestamp',
            updated_at: 'timestamp'
        };

        console.log('AlarmOccurrenceRepository 초기화 완료 (cleared_by 필드 포함)');
    }
    // ========================================================================
    // BaseRepository 필수 메서드 구현 (AlarmQueries 활용)
    // ========================================================================

    /**
     * 모든 알람 발생 조회 (페이징, 필터링 지원) - device_id INTEGER 처리
     */
    async findAll(options = {}) {
        try {
            const {
                tenantId = 1,
                state,
                severity,
                ruleId,
                deviceId,
                acknowledged,
                page = 1,
                limit = 50,
                sortBy = 'occurrence_time',
                sortOrder = 'DESC'
            } = options;

            // AlarmQueries 사용
            let { query, params } = AlarmQueries.buildAlarmOccurrenceFilters(
                AlarmQueries.AlarmOccurrence.FIND_ALL,
                { 
                    tenant_id: tenantId, 
                    state, 
                    severity, 
                    rule_id: ruleId,
                    device_id: deviceId,  // INTEGER 처리됨
                    acknowledged 
                }
            );

            // 정렬 추가
            query = AlarmQueries.addSorting(query, sortBy, sortOrder);

            // 페이징 추가
            if (limit) {
                const offset = (page - 1) * limit;
                query = AlarmQueries.addPagination(query, limit, offset);
            }

            const results = await this.executeQuery(query, params);

            // 결과 포맷팅
            const alarmOccurrences = results.map(occurrence => this.formatAlarmOccurrence(occurrence));

            // 전체 개수 조회 (페이징용)
            const countResult = await this.countByConditions({ 
                tenantId, 
                state, 
                severity, 
                ruleId, 
                deviceId: deviceId ? parseInt(deviceId) : undefined  // INTEGER 변환
            });

            console.log(`알람 발생 ${alarmOccurrences.length}개 조회 완료 (총 ${countResult}개)`);

            return {
                items: alarmOccurrences,
                pagination: {
                    page,
                    limit,
                    total: countResult,
                    totalPages: Math.ceil(countResult / limit)
                }
            };

        } catch (error) {
            console.error('AlarmOccurrenceRepository.findAll 실패:', error);
            throw error;
        }
    }

    /**
     * ID로 알람 발생 조회
     */
    async findById(id, tenantId = null) {
        try {
            const cacheKey = `alarmOccurrence_${id}_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            const query = AlarmQueries.AlarmOccurrence.FIND_BY_ID;
            const params = [id, tenantId || 1];

            const result = await this.executeQuerySingle(query, params);
            const alarmOccurrence = result ? this.formatAlarmOccurrence(result) : null;

            if (alarmOccurrence) {
                this.setCache(cacheKey, alarmOccurrence);
            }

            console.log(`알람 발생 ID ${id} 조회: ${alarmOccurrence ? '성공' : '없음'}`);
            return alarmOccurrence;

        } catch (error) {
            console.error(`AlarmOccurrenceRepository.findById(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 발생 생성 - device_id INTEGER 처리
     */
    async create(alarmOccurrenceData) {
        try {
            const data = {
                ...alarmOccurrenceData,
                occurrence_time: alarmOccurrenceData.occurrence_time || new Date().toISOString(),
                state: alarmOccurrenceData.state || 'active'
            };

            // device_id INTEGER 검증 및 변환
            if (data.device_id !== null && data.device_id !== undefined) {
                data.device_id = AlarmQueries.validateDeviceId(data.device_id);
            }

            // point_id INTEGER 검증 및 변환  
            if (data.point_id !== null && data.point_id !== undefined) {
                data.point_id = AlarmQueries.validatePointId(data.point_id);
            }

            // 필수 필드 검증 (AlarmQueries 헬퍼 사용)
            AlarmQueries.validateAlarmOccurrence(data);

            // 쿼리와 파라미터 생성 (device_id INTEGER 처리 포함)
            const query = AlarmQueries.AlarmOccurrence.CREATE;
            const params = AlarmQueries.buildCreateOccurrenceParams(data);

            console.log(`INSERT 쿼리 실행 - device_id: ${data.device_id} (타입: ${typeof data.device_id})`);

            const result = await this.executeNonQuery(query, params);

            if (result && (result.lastInsertRowid || result.insertId)) {
                const newId = result.lastInsertRowid || result.insertId;
                
                // 캐시 무효화
                this.clearCache();
                
                console.log(`새 알람 발생 생성 완료: ID ${newId}`);
                return await this.findById(newId);
            }

            throw new Error('알람 발생 생성 실패 - ID 반환되지 않음');

        } catch (error) {
            console.error('AlarmOccurrenceRepository.create 실패:', error);
            throw error;
        }
    }

    /**
     * 알람 발생 업데이트
     */
    async update(id, updateData, tenantId = null) {
        try {
            // device_id가 포함된 경우 INTEGER 검증
            if (updateData.device_id !== undefined) {
                updateData.device_id = AlarmQueries.validateDeviceId(updateData.device_id);
            }

            const query = AlarmQueries.AlarmOccurrence.UPDATE_STATE;
            const params = [updateData.state, id, tenantId || 1];

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.clearCache();
                
                console.log(`알람 발생 ID ${id} 업데이트 완료`);
                return await this.findById(id, tenantId);
            }

            return null;

        } catch (error) {
            console.error(`AlarmOccurrenceRepository.update(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 발생 삭제
     */
    async deleteById(id, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.DELETE;
            const params = [id];

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.clearCache();
                
                console.log(`알람 발생 ID ${id} 삭제 완료`);
                return true;
            }

            return false;

        } catch (error) {
            console.error(`AlarmOccurrenceRepository.deleteById(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 발생 존재 확인
     */
    async exists(id, tenantId = null) {
        try {
            const query = 'SELECT 1 FROM alarm_occurrences WHERE id = ? AND tenant_id = ? LIMIT 1';
            const params = [id, tenantId || 1];

            const result = await this.executeQuerySingle(query, params);
            return !!result;

        } catch (error) {
            console.error(`AlarmOccurrenceRepository.exists(${id}) 실패:`, error);
            return false;
        }
    }

    /**
     * 조건별 개수 조회
     */
    async countByConditions(conditions = {}) {
        try {
            let { query, params } = AlarmQueries.buildAlarmOccurrenceFilters(
                AlarmQueries.AlarmOccurrence.COUNT_ALL, 
                conditions
            );

            const result = await this.executeQuerySingle(query, params);
            return result ? result.count : 0;

        } catch (error) {
            console.error('countByConditions 실패:', error);
            return 0;
        }
    }

    // ========================================================================
    // 알람 발생 특화 메서드들 (AlarmQueries 활용) - device_id INTEGER 처리
    // ========================================================================

    /**
     * 활성 알람만 조회
     */
    async findActive(tenantId = null) {
        try {
            const cacheKey = `activeAlarms_${tenantId || 'all'}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            const query = AlarmQueries.AlarmOccurrence.FIND_ACTIVE;
            const params = [tenantId || 1];

            const results = await this.executeQuery(query, params);
            const activeAlarms = results.map(occurrence => this.formatAlarmOccurrence(occurrence));

            this.setCache(cacheKey, activeAlarms, 60000); // 1분 캐시
            console.log(`활성 알람 ${activeAlarms.length}개 조회 완료`);
            return activeAlarms;

        } catch (error) {
            console.error('findActive 실패:', error);
            throw error;
        }
    }

    /**
     * 미확인 알람만 조회
     */
    async findUnacknowledged(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.FIND_UNACKNOWLEDGED;
            const params = [tenantId || 1];

            const results = await this.executeQuery(query, params);
            return results.map(occurrence => this.formatAlarmOccurrence(occurrence));

        } catch (error) {
            console.error('findUnacknowledged 실패:', error);
            throw error;
        }
    }

    /**
     * 규칙별 알람 발생 조회
     */
    async findByRule(ruleId, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.FIND_BY_RULE;
            const params = [ruleId, tenantId || 1];

            const results = await this.executeQuery(query, params);
            return results.map(occurrence => this.formatAlarmOccurrence(occurrence));

        } catch (error) {
            console.error(`findByRule(${ruleId}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 디바이스별 알람 발생 조회 - device_id INTEGER 처리
     */
    async findByDevice(deviceId, tenantId = null) {
        try {
            // device_id INTEGER 검증
            const validDeviceId = AlarmQueries.validateDeviceId(deviceId);
            if (validDeviceId === null) {
                throw new Error('유효하지 않은 device_id입니다');
            }

            const query = AlarmQueries.AlarmOccurrence.FIND_BY_DEVICE;
            const params = [tenantId || 1, validDeviceId];

            const results = await this.executeQuery(query, params);
            return results.map(occurrence => this.formatAlarmOccurrence(occurrence));

        } catch (error) {
            console.error(`findByDevice(${deviceId}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 디바이스별 활성 알람 조회 - device_id INTEGER 처리
     */
    async findActiveByDevice(deviceId, tenantId = null) {
        try {
            // device_id INTEGER 검증
            const validDeviceId = AlarmQueries.validateDeviceId(deviceId);
            if (validDeviceId === null) {
                throw new Error('유효하지 않은 device_id입니다');
            }

            const query = AlarmQueries.AlarmOccurrence.FIND_ACTIVE_BY_DEVICE;
            const params = [tenantId || 1, validDeviceId];

            const results = await this.executeQuery(query, params);
            
            console.log(`활성 알람 조회 (Device ${validDeviceId}): ${results.length}건`);
            return results.map(occurrence => this.formatAlarmOccurrence(occurrence));

        } catch (error) {
            console.error(`findActiveByDevice(${deviceId}) 실패:`, error);
            throw error;
        }
    }

    // ========================================================================
    // 알람 상태 변경 메서드들 (AlarmQueries 활용)
    // ========================================================================

    /**
     * 알람 확인 (Acknowledge)
     */
    async acknowledge(id, userId, comment = '', tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.ACKNOWLEDGE;
            const params = [userId, comment, id, tenantId || 1];

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.clearCache();
                
                console.log(`알람 ID ${id} 확인 처리 완료 (사용자: ${userId})`);
                return await this.findById(id, tenantId);
            }

            return null;

        } catch (error) {
            console.error(`acknowledge(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 해제 (Clear)
     */
    async clear(id, clearedBy, clearedValue, comment = '', tenantId = null) {
        try {
            // cleared_by INTEGER 검증
            const validClearedBy = parseInt(clearedBy);
            if (isNaN(validClearedBy) || validClearedBy <= 0) {
                throw new Error('유효하지 않은 cleared_by 사용자 ID입니다');
            }

            // AlarmQueries.AlarmOccurrence.CLEAR 쿼리 사용
            // UPDATE alarm_occurrences SET state = 'cleared', cleared_time = CURRENT_TIMESTAMP, 
            // cleared_value = ?, clear_comment = ?, cleared_by = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?
            const query = AlarmQueries.AlarmOccurrence.CLEAR;
            const params = [
                clearedValue ? JSON.stringify(clearedValue) : null,
                comment,
                validClearedBy,                     // ⭐ cleared_by 파라미터 추가!
                id,
                tenantId || 1
            ];

            console.log(`알람 해제 실행: ID ${id}, cleared_by: ${validClearedBy}`);

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.clearCache();
                
                console.log(`알람 ID ${id} 해제 처리 완료 (사용자: ${validClearedBy})`);
                return await this.findById(id, tenantId);
            }

            return null;

        } catch (error) {
            console.error(`clear(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 통계 조회
     */
    async getStatsByTenant(tenantId) {
        try {
            const cacheKey = `alarmOccurrenceStats_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            const query = AlarmQueries.AlarmOccurrence.STATS_SUMMARY;
            const result = await this.executeQuerySingle(query, [tenantId]);

            const stats = result || {
                total_occurrences: 0,
                active_alarms: 0,
                unacknowledged_alarms: 0,
                acknowledged_alarms: 0,
                cleared_alarms: 0
            };

            // 심각도별 분포 추가
            const severityQuery = AlarmQueries.AlarmOccurrence.STATS_BY_SEVERITY;
            const severityResults = await this.executeQuery(severityQuery, [tenantId]);

            stats.severity_distribution = severityResults.reduce((acc, row) => {
                acc[row.severity] = {
                    total: row.count,
                    active: row.active_count
                };
                return acc;
            }, {});

            // 디바이스별 통계 추가 (device_id INTEGER 처리)
            const deviceStatsQuery = AlarmQueries.AlarmOccurrence.STATS_BY_DEVICE;
            const deviceStats = await this.executeQuery(deviceStatsQuery, [tenantId]);
            stats.device_distribution = deviceStats;

            this.setCache(cacheKey, stats, 30000); // 30초 캐시
            return stats;

        } catch (error) {
            console.error('getStatsByTenant 실패:', error);
            return {
                total_occurrences: 0,
                active_alarms: 0,
                unacknowledged_alarms: 0,
                acknowledged_alarms: 0,
                cleared_alarms: 0,
                severity_distribution: {},
                device_distribution: []
            };
        }
    }

    // ========================================================================
    // 헬퍼 메서드들
    // ========================================================================

    /**
     * 알람 발생 데이터 포맷팅 (JSON 필드 파싱 포함)
     */
    formatAlarmOccurrence(occurrence) {
        if (!occurrence) return null;

        try {
            return {
                ...occurrence,
                // ID 필드들 INTEGER 변환
                device_id: occurrence.device_id ? parseInt(occurrence.device_id) : null,
                point_id: occurrence.point_id ? parseInt(occurrence.point_id) : null,
                acknowledged_by: occurrence.acknowledged_by ? parseInt(occurrence.acknowledged_by) : null,
                cleared_by: occurrence.cleared_by ? parseInt(occurrence.cleared_by) : null,  // ⭐ 추가!
                
                // 상태 플래그
                is_acknowledged: !!occurrence.acknowledged_time,
                is_cleared: !!occurrence.cleared_time,
                is_active: occurrence.state === 'active',
                
                // JSON 필드 파싱
                trigger_value: this.parseJSONField(occurrence.trigger_value),
                cleared_value: this.parseJSONField(occurrence.cleared_value),
                context_data: this.parseJSONField(occurrence.context_data),
                notification_result: this.parseJSONField(occurrence.notification_result)
            };
        } catch (error) {
            console.warn(`데이터 포맷팅 실패 for occurrence ${occurrence.id}:`, error);
            return occurrence;
        }
    }

    /**
     * 특정 사용자가 해제한 알람들 조회
     */
    async findClearedByUser(userId, tenantId = null) {
        try {
            const validUserId = parseInt(userId);
            if (isNaN(validUserId) || validUserId <= 0) {
                throw new Error('유효하지 않은 사용자 ID입니다');
            }

            const query = `
                SELECT ao.*, ar.name as rule_name
                FROM alarm_occurrences ao
                LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
                WHERE ao.cleared_by = ? AND ao.tenant_id = ?
                ORDER BY ao.cleared_time DESC
            `;
            const params = [validUserId, tenantId || 1];

            const results = await this.executeQuery(query, params);
            
            console.log(`사용자 ${validUserId}가 해제한 알람 ${results.length}건 조회`);
            return results.map(occurrence => this.formatAlarmOccurrence(occurrence));

        } catch (error) {
            console.error(`findClearedByUser(${userId}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 특정 사용자가 확인한 알람들 조회
     */
    async findAcknowledgedByUser(userId, tenantId = null) {
        try {
            const validUserId = parseInt(userId);
            if (isNaN(validUserId) || validUserId <= 0) {
                throw new Error('유효하지 않은 사용자 ID입니다');
            }

            const query = `
                SELECT ao.*, ar.name as rule_name
                FROM alarm_occurrences ao
                LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
                WHERE ao.acknowledged_by = ? AND ao.tenant_id = ?
                ORDER BY ao.acknowledged_time DESC
            `;
            const params = [validUserId, tenantId || 1];

            const results = await this.executeQuery(query, params);
            
            console.log(`사용자 ${validUserId}가 확인한 알람 ${results.length}건 조회`);
            return results.map(occurrence => this.formatAlarmOccurrence(occurrence));

        } catch (error) {
            console.error(`findAcknowledgedByUser(${userId}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 사용자별 알람 처리 통계
     */
    async getUserActionStats(userId, tenantId = null) {
        try {
            const validUserId = parseInt(userId);
            if (isNaN(validUserId) || validUserId <= 0) {
                throw new Error('유효하지 않은 사용자 ID입니다');
            }

            const query = `
                SELECT 
                    COUNT(CASE WHEN acknowledged_by = ? THEN 1 END) as acknowledged_count,
                    COUNT(CASE WHEN cleared_by = ? THEN 1 END) as cleared_count,
                    COUNT(CASE WHEN acknowledged_by = ? AND acknowledged_time >= datetime('now', '-7 days') THEN 1 END) as acknowledged_last_week,
                    COUNT(CASE WHEN cleared_by = ? AND cleared_time >= datetime('now', '-7 days') THEN 1 END) as cleared_last_week
                FROM alarm_occurrences 
                WHERE tenant_id = ?
            `;
            const params = [validUserId, validUserId, validUserId, validUserId, tenantId || 1];

            const result = await this.executeQuerySingle(query, params);
            
            return {
                user_id: validUserId,
                total_acknowledged: result.acknowledged_count || 0,
                total_cleared: result.cleared_count || 0,
                acknowledged_last_week: result.acknowledged_last_week || 0,
                cleared_last_week: result.cleared_last_week || 0
            };

        } catch (error) {
            console.error(`getUserActionStats(${userId}) 실패:`, error);
            return {
                user_id: validUserId,
                total_acknowledged: 0,
                total_cleared: 0,
                acknowledged_last_week: 0,
                cleared_last_week: 0
            };
        }
    }
    /**
     * JSON 필드 안전 파싱
     */
    parseJSONField(field) {
        if (!field || field === 'null') return null;
        
        try {
            return JSON.parse(field);
        } catch {
            return field; // JSON이 아니면 원본 반환
        }
    }

    /**
     * 캐시에서 데이터 조회
     */
    getFromCache(key) {
        if (!this.cacheEnabled || !this.cache) return null;
        
        const cached = this.cache.get(key);
        if (cached && Date.now() - cached.timestamp < this.cacheTimeout) {
            return cached.data;
        }
        
        // 만료된 캐시 제거
        if (cached) {
            this.cache.delete(key);
        }
        return null;
    }

    /**
     * 캐시에 데이터 저장
     */
    setCache(key, data, ttl = null) {
        if (!this.cacheEnabled || !this.cache) return;
        
        this.cache.set(key, {
            data: data,
            timestamp: Date.now()
        });
    }

    /**
     * 캐시 초기화
     */
    clearCache() {
        if (this.cache) {
            this.cache.clear();
            console.log('AlarmOccurrenceRepository 캐시 초기화 완료');
        }
    }
}

module.exports = AlarmOccurrenceRepository;