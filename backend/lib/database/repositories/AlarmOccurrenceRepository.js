// ============================================================================
// backend/lib/database/repositories/AlarmOccurrenceRepository.js
// BaseRepository 상속받은 AlarmOccurrenceRepository - 실제 스키마 기반, 쿼리 분리
// ============================================================================

const BaseRepository = require('./BaseRepository');
const AlarmQueries = require('../queries/AlarmQueries');

/**
 * 알람 발생 Repository 클래스 (실제 데이터베이스 스키마와 100% 일치)
 * BaseRepository를 상속받아 공통 기능 활용
 * 
 * 🔧 주요 수정사항:
 * - AlarmQueries.js 쿼리 분리 패턴 적용
 * - 실제 alarm_occurrences 스키마에 맞춘 필드 정의
 * - INSERT/UPDATE 쿼리 실제 컬럼명 반영
 * - acknowledged_time, cleared_time 등 정확한 컬럼명 사용
 */
class AlarmOccurrenceRepository extends BaseRepository {
    constructor() {
        super('alarm_occurrences');
        
        // 🔥 실제 alarm_occurrences 테이블 필드 정의 (스키마와 100% 일치)
        this.fields = {
            id: 'autoIncrement',
            rule_id: 'int',
            tenant_id: 'int',
            occurrence_time: 'timestamp',
            trigger_value: 'text',              // JSON
            trigger_condition: 'text',
            alarm_message: 'text',
            severity: 'varchar(20)',
            state: 'varchar(20)',               // 'active', 'cleared', 'acknowledged'
            
            // 확인(Acknowledge) 정보
            acknowledged_time: 'timestamp',
            acknowledged_by: 'int',
            acknowledge_comment: 'text',
            
            // 해제(Clear) 정보  
            cleared_time: 'timestamp',
            cleared_value: 'text',              // JSON
            clear_comment: 'text',
            
            // 알림 정보
            notification_sent: 'int',
            notification_time: 'timestamp',
            notification_count: 'int',
            notification_result: 'text',        // JSON
            
            // 메타데이터
            context_data: 'text',               // JSON
            source_name: 'varchar(100)',
            location: 'varchar(200)',
            device_id: 'text',
            point_id: 'int',
            
            // 감사 정보
            created_at: 'timestamp',
            updated_at: 'timestamp'
        };

        console.log('✅ AlarmOccurrenceRepository 초기화 완료 (실제 스키마 기반)');
    }

    // ========================================================================
    // 🔥 BaseRepository 필수 메서드 구현 (AlarmQueries 활용)
    // ========================================================================

    /**
     * 모든 알람 발생 조회 (페이징, 필터링 지원)
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

            // 기본 쿼리에서 시작
            let query = `
                SELECT 
                    ao.*,
                    ar.name as rule_name,
                    ar.severity as rule_severity,
                    ar.target_type,
                    ar.target_id
                FROM alarm_occurrences ao
                LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
                WHERE ao.tenant_id = ?
            `;
            const params = [tenantId];

            // 추가 필터들
            if (state) {
                query += ' AND ao.state = ?';
                params.push(state);
            }

            if (severity) {
                query += ' AND ao.severity = ?';
                params.push(severity);
            }

            if (ruleId) {
                query += ' AND ao.rule_id = ?';
                params.push(ruleId);
            }

            if (deviceId) {
                query += ' AND ao.device_id = ?';
                params.push(deviceId);
            }

            // 확인 상태 필터링
            if (acknowledged !== undefined) {
                if (acknowledged) {
                    query += ' AND ao.acknowledged_time IS NOT NULL';
                } else {
                    query += ' AND ao.acknowledged_time IS NULL';
                }
            }

            // 정렬 추가
            query += ` ORDER BY ao.${sortBy} ${sortOrder}`;

            // 페이징 추가
            if (limit) {
                query += ' LIMIT ? OFFSET ?';
                params.push(limit, (page - 1) * limit);
            }

            const results = await this.executeQuery(query, params);

            // 결과 포맷팅
            const alarmOccurrences = results.map(occurrence => this.formatAlarmOccurrence(occurrence));

            // 전체 개수 조회 (페이징용) - 필터 조건 동일하게 적용
            let countQuery = 'SELECT COUNT(*) as total FROM alarm_occurrences WHERE tenant_id = ?';
            let countParams = [tenantId];
            
            if (state) {
                countQuery += ' AND state = ?';
                countParams.push(state);
            }
            if (severity) {
                countQuery += ' AND severity = ?';
                countParams.push(severity);
            }
            if (ruleId) {
                countQuery += ' AND rule_id = ?';
                countParams.push(ruleId);
            }
            if (deviceId) {
                countQuery += ' AND device_id = ?';
                countParams.push(deviceId);
            }
            if (acknowledged !== undefined) {
                if (acknowledged) {
                    countQuery += ' AND acknowledged_time IS NOT NULL';
                } else {
                    countQuery += ' AND acknowledged_time IS NULL';
                }
            }

            const countResult = await this.executeQuerySingle(countQuery, countParams);
            const total = countResult ? countResult.total : 0;

            console.log(`✅ 알람 발생 ${alarmOccurrences.length}개 조회 완료 (총 ${total}개)`);

            return {
                items: alarmOccurrences,
                pagination: {
                    page,
                    limit,
                    total,
                    totalPages: Math.ceil(total / limit)
                }
            };

        } catch (error) {
            console.error('❌ AlarmOccurrenceRepository.findAll 실패:', error);
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

            console.log(`✅ 알람 발생 ID ${id} 조회: ${alarmOccurrence ? '성공' : '없음'}`);
            return alarmOccurrence;

        } catch (error) {
            console.error(`❌ AlarmOccurrenceRepository.findById(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 🔧 알람 발생 생성 - 실제 스키마에 맞춘 INSERT
     */
    async create(alarmOccurrenceData) {
        try {
            const data = {
                ...alarmOccurrenceData,
                occurrence_time: alarmOccurrenceData.occurrence_time || new Date().toISOString(),
                state: alarmOccurrenceData.state || 'active'
            };

            // 🔥 필수 필드 검증 (AlarmQueries 헬퍼 사용)
            AlarmQueries.validateOccurrenceRequiredFields(data);

            // 🔥 쿼리와 파라미터 생성 (13개 컬럼/값 정확히 일치)
            const query = AlarmQueries.AlarmOccurrence.CREATE;
            const params = AlarmQueries.buildCreateOccurrenceParams(data);

            console.log(`🔧 INSERT 쿼리 실행 - 컬럼: 13개, 값: ${params.length}개`);
            console.log('파라미터:', params);

            const result = await this.executeNonQuery(query, params);

            if (result && (result.lastInsertRowid || result.insertId)) {
                const newId = result.lastInsertRowid || result.insertId;
                
                // 캐시 무효화
                this.clearCache();
                
                console.log(`✅ 새 알람 발생 생성 완료: ID ${newId}`);
                return await this.findById(newId);
            }

            throw new Error('알람 발생 생성 실패 - ID 반환되지 않음');

        } catch (error) {
            console.error('❌ AlarmOccurrenceRepository.create 실패:', error);
            throw error;
        }
    }

    /**
     * 알람 발생 업데이트
     */
    async update(id, updateData, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.UPDATE_STATE;
            const params = [updateData.state, id, tenantId || 1];

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.clearCache();
                
                console.log(`✅ 알람 발생 ID ${id} 업데이트 완료`);
                return await this.findById(id, tenantId);
            }

            return null;

        } catch (error) {
            console.error(`❌ AlarmOccurrenceRepository.update(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 발생 삭제
     */
    async deleteById(id, tenantId = null) {
        try {
            const query = 'DELETE FROM alarm_occurrences WHERE id = ? AND tenant_id = ?';
            const params = [id, tenantId || 1];

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.clearCache();
                
                console.log(`✅ 알람 발생 ID ${id} 삭제 완료`);
                return true;
            }

            return false;

        } catch (error) {
            console.error(`❌ AlarmOccurrenceRepository.deleteById(${id}) 실패:`, error);
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
            console.error(`❌ AlarmOccurrenceRepository.exists(${id}) 실패:`, error);
            return false;
        }
    }

    // ========================================================================
    // 🔥 알람 발생 특화 메서드들 (AlarmQueries 활용)
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
            console.log(`✅ 활성 알람 ${activeAlarms.length}개 조회 완료`);
            return activeAlarms;

        } catch (error) {
            console.error('❌ findActive 실패:', error);
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
            console.error('❌ findUnacknowledged 실패:', error);
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
            console.error(`❌ findByRule(${ruleId}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 디바이스별 알람 발생 조회
     */
    async findByDevice(deviceId, tenantId = null) {
        try {
            const query = `
                SELECT ao.*, ar.name as rule_name
                FROM alarm_occurrences ao
                LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
                WHERE ao.device_id = ? AND ao.tenant_id = ?
                ORDER BY ao.occurrence_time DESC
            `;
            const params = [deviceId, tenantId || 1];

            const results = await this.executeQuery(query, params);
            return results.map(occurrence => this.formatAlarmOccurrence(occurrence));

        } catch (error) {
            console.error(`❌ findByDevice(${deviceId}) 실패:`, error);
            throw error;
        }
    }

    // ========================================================================
    // 🔥 알람 상태 변경 메서드들 (AlarmQueries 활용)
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
                
                console.log(`✅ 알람 ID ${id} 확인 처리 완료 (사용자: ${userId})`);
                return await this.findById(id, tenantId);
            }

            return null;

        } catch (error) {
            console.error(`❌ acknowledge(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 해제 (Clear)
     */
    async clear(id, clearedValue, comment = '', tenantId = null) {
        try {
            const query = AlarmQueries.AlarmOccurrence.CLEAR;
            const params = [
                clearedValue ? JSON.stringify(clearedValue) : null,
                comment,
                id,
                tenantId || 1
            ];

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.clearCache();
                
                console.log(`✅ 알람 ID ${id} 해제 처리 완료`);
                return await this.findById(id, tenantId);
            }

            return null;

        } catch (error) {
            console.error(`❌ clear(${id}) 실패:`, error);
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

            this.setCache(cacheKey, stats, 30000); // 30초 캐시
            return stats;

        } catch (error) {
            console.error('❌ getStatsByTenant 실패:', error);
            return {
                total_occurrences: 0,
                active_alarms: 0,
                unacknowledged_alarms: 0,
                acknowledged_alarms: 0,
                cleared_alarms: 0,
                severity_distribution: {}
            };
        }
    }

    // ========================================================================
    // 🔥 헬퍼 메서드들
    // ========================================================================

    /**
     * 알람 발생 데이터 포맷팅 (JSON 필드 파싱 포함)
     */
    formatAlarmOccurrence(occurrence) {
        if (!occurrence) return null;

        try {
            return {
                ...occurrence,
                is_acknowledged: !!occurrence.acknowledged_time,
                is_cleared: !!occurrence.cleared_time,
                is_active: occurrence.state === 'active',
                trigger_value: occurrence.trigger_value ? JSON.parse(occurrence.trigger_value) : null,
                cleared_value: occurrence.cleared_value ? JSON.parse(occurrence.cleared_value) : null,
                context_data: occurrence.context_data ? JSON.parse(occurrence.context_data) : null,
                notification_result: occurrence.notification_result ? JSON.parse(occurrence.notification_result) : null
            };
        } catch (error) {
            console.warn(`JSON 파싱 실패 for occurrence ${occurrence.id}:`, error);
            return occurrence;
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
            console.log('🗑️ AlarmOccurrenceRepository 캐시 초기화 완료');
        }
    }
}
/**
 * POST /api/alarms/occurrences
 * 새로운 알람 발생 생성 (테스트용)
 */

module.exports = AlarmOccurrenceRepository;
